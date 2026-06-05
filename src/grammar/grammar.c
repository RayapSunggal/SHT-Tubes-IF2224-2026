#include "grammar.h"
#include <string.h>

bool isExactLabel(Node *node, const char *label) {
    return node!=NULL && strcmp(node->name, label)==0;
}

bool isIdentLabel(Node *node) {
    return node!=NULL && strncmp(node->name, "ident", 5)==0;
}

bool isIntconLabel(Node *node) {
    return node!=NULL && strncmp(node->name, "intcon", 6)==0;
}

bool isRealconLabel(Node *node) {
    return node!=NULL && strncmp(node->name, "realcon", 7)==0;
}

bool isCharconLabel(Node *node) {
    return node!=NULL && strncmp(node->name, "charcon", 7)==0;
}

bool isStringLabel(Node *node) {
    return node!=NULL && strncmp(node->name, "string", 6)==0;
}

bool isProcedureFunctionCallLabel(Node *node) {
    if (node==NULL) {
        return false;
    }

    return strcmp(node->name, "<procedure/function-call>")==0 ||
           strcmp(node->name, "<procedure-call>")==0 ||
           strcmp(node->name, "<function-call>")==0;
}

bool isSemicolonStatementPair(Node *semicolonNode, Node *statementNode) {
    return isExactLabel(semicolonNode, "semicolon") && is_statement_complete(statementNode);
}

bool isCommaExpressionPair(Node *commaNode, Node *exprNode) {
    return isExactLabel(commaNode, "comma") && is_expression_complete(exprNode);
}

bool isCommaIdentPair(Node *commaNode, Node *identNode) {
    return isExactLabel(commaNode, "comma") && isIdentLabel(identNode);
}

bool isSemicolonParameterGroupPair(Node *semicolonNode, Node *parameterGroupNode) {
    return isExactLabel(semicolonNode, "semicolon") && is_parameter_group_complete(parameterGroupNode);
}

bool isSemicolonFieldPartPair(Node *semicolonNode, Node *fieldPartNode) {
    return isExactLabel(semicolonNode, "semicolon") && is_field_part_complete(fieldPartNode);
}

bool isSemicolonCaseBlockPair(Node *semicolonNode, Node *caseBlockNode) {
    return isExactLabel(semicolonNode, "semicolon") && is_case_block_complete(caseBlockNode);
}

bool isConstDeclEntry(Node *identNode, Node *eqlNode, Node *constantNode, Node *semicolonNode) {
    return isIdentLabel(identNode) &&
           isExactLabel(eqlNode, "eql") &&
           is_constant_complete(constantNode) &&
           isExactLabel(semicolonNode, "semicolon");
}

bool isTypeDeclEntry(Node *identNode, Node *eqlNode, Node *typeNode, Node *semicolonNode) {
    return isIdentLabel(identNode) &&
           isExactLabel(eqlNode, "eql") &&
           is_type_complete(typeNode) &&
           isExactLabel(semicolonNode, "semicolon");
}

bool isVarDeclEntry(Node *identifierListNode, Node *colonNode, Node *typeNode, Node *semicolonNode) {
    return is_identifier_list_complete(identifierListNode) &&
           isExactLabel(colonNode, "colon") &&
           is_type_complete(typeNode) &&
           isExactLabel(semicolonNode, "semicolon");
}

// 1
bool is_program_complete(Node *node) {
    if (!isExactLabel(node, "<program>")) return false;
    if (node->childCount!=4) return false;
    if (!is_program_header_complete(node->child[0])) return false;
    if (!is_declaration_part_complete(node->child[1])) return false;
    if (!is_compound_statement_complete(node->child[2])) return false;
    return isExactLabel(node->child[3], "period");
}

// 2
bool is_program_header_complete(Node *node) {
    if (!isExactLabel(node, "<program-header>")) return false;
    if (node->childCount!=3) return false;
    if (!isExactLabel(node->child[0], "programsy")) return false;
    if (!isIdentLabel(node->child[1])) return false;
    return isExactLabel(node->child[2], "semicolon");
}

// 3
bool is_declaration_part_complete(Node *node) {
    int i = 0;

    if (!isExactLabel(node, "<declaration-part>")) return false;

    while (i < node->childCount && is_const_declaration_complete(node->child[i])) i++;
    while (i < node->childCount && is_type_declaration_complete(node->child[i])) i++;
    while (i < node->childCount && is_var_declaration_complete(node->child[i])) i++;
    while (i < node->childCount && is_subprogram_declaration_complete(node->child[i])) i++;

    return i==node->childCount;
}

// 4
bool is_const_declaration_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<const-declaration>")) return false;
    if (node->childCount < 5) return false;
    if (!isExactLabel(node->child[0], "constsy")) return false;

    while (i + 3 < node->childCount) {
        if (!isConstDeclEntry(node->child[i], node->child[i+1], node->child[i+2], node->child[i+3]))
            return false;
        i += 4;
    }

    return i==node->childCount;
}

// 5
bool is_constant_complete(Node *node) {
    if (!isExactLabel(node, "<constant>")) return false;

    if (node->childCount==1) {
        return isCharconLabel(node->child[0]) ||
               isStringLabel(node->child[0]) ||
               isIdentLabel(node->child[0]) ||
               isIntconLabel(node->child[0]) ||
               isRealconLabel(node->child[0]);
    }

    if (node->childCount==2) {
        if (!isExactLabel(node->child[0], "plus") && !isExactLabel(node->child[0], "minus"))
            return false;
        return isIdentLabel(node->child[1]) ||
               isIntconLabel(node->child[1]) ||
               isRealconLabel(node->child[1]);
    }

    return false;
}

// 6
bool is_type_declaration_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<type-declaration>")) return false;
    if (node->childCount < 5) return false;
    if (!isExactLabel(node->child[0], "typesy")) return false;

    while (i + 3 < node->childCount) {
        if (!isTypeDeclEntry(node->child[i], node->child[i+1], node->child[i+2], node->child[i+3]))
            return false;
        i += 4;
    }

    return i==node->childCount;
}

// 7
bool is_var_declaration_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<var-declaration>")) return false;
    if (node->childCount < 5) return false;
    if (!isExactLabel(node->child[0], "varsy")) return false;

    while (i + 3 < node->childCount) {
        if (!isVarDeclEntry(node->child[i], node->child[i+1], node->child[i+2], node->child[i+3]))
            return false;
        i += 4;
    }

    return i==node->childCount;
}

// 8
bool is_identifier_list_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<identifier-list>")) return false;
    if (node->childCount < 1) return false;
    if (!isIdentLabel(node->child[0])) return false;

    while (i + 1 < node->childCount) {
        if (!isCommaIdentPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    return i==node->childCount;
}

// 9
bool is_type_complete(Node *node) {
    if (!isExactLabel(node, "<type>")) return false;
    if (node->childCount!=1) return false;
    return isIdentLabel(node->child[0]) ||
           is_array_type_complete(node->child[0]) ||
           is_range_complete(node->child[0]) ||
           is_enumerated_complete(node->child[0]) ||
           is_record_type_complete(node->child[0]);
}

// 10
bool is_array_type_complete(Node *node) {
    if (!isExactLabel(node, "<array-type>")) return false;
    if (node->childCount!=6) return false;
    if (!isExactLabel(node->child[0], "arraysy")) return false;
    if (!isExactLabel(node->child[1], "lbrack")) return false;
    if (!is_range_complete(node->child[2]) && !isIdentLabel(node->child[2])) return false;
    if (!isExactLabel(node->child[3], "rbrack")) return false;
    if (!isExactLabel(node->child[4], "ofsy")) return false;
    return is_type_complete(node->child[5]);
}

// 11
bool is_range_complete(Node *node) {
    if (!isExactLabel(node, "<range>")) return false;
    if (node->childCount!=4) return false;
    if (!is_constant_complete(node->child[0])) return false;
    if (!isExactLabel(node->child[1], "period")) return false;
    if (!isExactLabel(node->child[2], "period")) return false;
    return is_constant_complete(node->child[3]);
}

// 12
bool is_enumerated_complete(Node *node) {
    int i = 2;

    if (!isExactLabel(node, "<enumerated>")) return false;
    if (node->childCount < 3) return false;
    if (!isExactLabel(node->child[0], "lparent")) return false;
    if (!isIdentLabel(node->child[1])) return false;

    while (i + 1 < node->childCount - 1) {
        if (!isCommaIdentPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    if (i!=node->childCount - 1) return false;
    return isExactLabel(node->child[node->childCount - 1], "rparent");
}

// 13
bool is_record_type_complete(Node *node) {
    if (!isExactLabel(node, "<record-type>")) return false;
    if (node->childCount==2) {
        return isExactLabel(node->child[0], "recordsy") &&
               isExactLabel(node->child[1], "endsy");
    }
    if (node->childCount==3) {
        if (!isExactLabel(node->child[0], "recordsy")) return false;
        if (!is_field_list_complete(node->child[1])) return false;
        return isExactLabel(node->child[2], "endsy");
    }
    return false;
}

// 14
bool is_field_list_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<field-list>")) return false;
    if (node->childCount < 1) return false;
    if (!is_field_part_complete(node->child[0])) return false;

    while (i + 1 < node->childCount) {
        if (!isSemicolonFieldPartPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    return i==node->childCount;
}

// 15
bool is_field_part_complete(Node *node) {
    if (!isExactLabel(node, "<field-part>")) return false;
    if (node->childCount!=3) return false;
    if (!is_identifier_list_complete(node->child[0])) return false;
    if (!isExactLabel(node->child[1], "colon")) return false;
    return is_type_complete(node->child[2]);
}

// 16
bool is_subprogram_declaration_complete(Node *node) {
    if (!isExactLabel(node, "<subprogram-declaration>")) return false;
    if (node->childCount!=1) return false;
    return is_procedure_declaration_complete(node->child[0]) ||
           is_function_declaration_complete(node->child[0]);
}

// 17
bool is_procedure_declaration_complete(Node *node) {
    if (!isExactLabel(node, "<procedure-declaration>")) return false;

    if (node->childCount==5) {
        if (!isExactLabel(node->child[0], "proceduresy")) return false;
        if (!isIdentLabel(node->child[1])) return false;
        if (!is_formal_parameter_list_complete(node->child[2])) return false;
        if (!isExactLabel(node->child[3], "semicolon")) return false;
        return is_block_complete(node->child[4]);
    }

    if (node->childCount==4) {
        if (!isExactLabel(node->child[0], "proceduresy")) return false;
        if (!isIdentLabel(node->child[1])) return false;
        if (!isExactLabel(node->child[2], "semicolon")) return false;
        return is_block_complete(node->child[3]);
    }

    return false;
}

// 18
bool is_function_declaration_complete(Node *node) {
    if (!isExactLabel(node, "<function-declaration>")) return false;

    if (node->childCount==8) {
        if (!isExactLabel(node->child[0], "functionsy")) return false;
        if (!isIdentLabel(node->child[1])) return false;
        if (!is_formal_parameter_list_complete(node->child[2])) return false;
        if (!isExactLabel(node->child[3], "colon")) return false;
        if (!isIdentLabel(node->child[4])) return false;
        if (!isExactLabel(node->child[5], "semicolon")) return false;
        if (!is_block_complete(node->child[6])) return false;
        return isExactLabel(node->child[7], "semicolon");
    }

    if (node->childCount==7) {
        if (!isExactLabel(node->child[0], "functionsy")) return false;
        if (!isIdentLabel(node->child[1])) return false;
        if (!isExactLabel(node->child[2], "colon")) return false;
        if (!isIdentLabel(node->child[3])) return false;
        if (!isExactLabel(node->child[4], "semicolon")) return false;
        if (!is_block_complete(node->child[5])) return false;
        return isExactLabel(node->child[6], "semicolon");
    }

    return false;
}

// 19
bool is_block_complete(Node *node) {
    if (!isExactLabel(node, "block") && !isExactLabel(node, "<block>")) return false;
    if (node->childCount!=2) return false;
    if (!is_declaration_part_complete(node->child[0])) return false;
    return is_compound_statement_complete(node->child[1]);
}

// 20
bool is_formal_parameter_list_complete(Node *node) {
    int i = 2;

    if (!isExactLabel(node, "<formal-parameter-list>")) return false;
    if (node->childCount < 3) return false;
    if (!isExactLabel(node->child[0], "lparent")) return false;
    if (!is_parameter_group_complete(node->child[1])) return false;

    while (i + 1 < node->childCount - 1) {
        if (!isSemicolonParameterGroupPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    if (i!=node->childCount - 1) return false;
    return isExactLabel(node->child[node->childCount - 1], "rparent");
}

// 21
bool is_parameter_group_complete(Node *node) {
    if (!isExactLabel(node, "<parameter-group>")) return false;
    if (node->childCount!=3) return false;
    if (!is_identifier_list_complete(node->child[0])) return false;
    if (!isExactLabel(node->child[1], "colon")) return false;
    return isIdentLabel(node->child[2]) || is_array_type_complete(node->child[2]);
}

// 22
bool is_compound_statement_complete(Node *node) {
    if (!isExactLabel(node, "<compound-statement>")) return false;
    if (node->childCount==2) {
        return isExactLabel(node->child[0], "beginsy") &&
               isExactLabel(node->child[1], "endsy");
    }
    if (node->childCount==3) {
        if (!isExactLabel(node->child[0], "beginsy")) return false;
        if (!is_statement_list_complete(node->child[1])) return false;
        return isExactLabel(node->child[2], "endsy");
    }
    return false;
}

// 23
bool is_statement_list_complete(Node *node) {
    int i = 0;
    bool expectStatement = true;
    bool sawContent = false;

    if (!isExactLabel(node, "<statement-list>")) return false;
    if (node->childCount == 0) return true;

    while (i < node->childCount) {
        if (isExactLabel(node->child[i], "semicolon")) {
            expectStatement = true;
            sawContent = true;
            i++;
            continue;
        }

        if (!expectStatement || !is_statement_complete(node->child[i])) return false;
        expectStatement = false;
        sawContent = true;
        i++;
    }

    return sawContent;
}

// 24
bool is_statement_complete(Node *node) {
    if (!isExactLabel(node, "statement") && !isExactLabel(node, "<statement>")) return false;
    if (node->childCount==0) return true;
    if (node->childCount!=1) return false;
    return is_assignment_statement_complete(node->child[0]) ||
           is_compound_statement_complete(node->child[0]) ||
           is_if_statement_complete(node->child[0]) ||
           is_case_statement_complete(node->child[0]) ||
           is_while_statement_complete(node->child[0]) ||
           is_repeat_statement_complete(node->child[0]) ||
           is_for_statement_complete(node->child[0]) ||
           is_procedure_function_call_complete(node->child[0]);
}

// 25
bool is_assignment_statement_complete(Node *node) {
    if (!isExactLabel(node, "<assignment-statement>")) return false;
    if (node->childCount!=3) return false;
    if (!is_variable_complete(node->child[0])) return false;
    if (!isExactLabel(node->child[1], "becomes")) return false;
    return is_expression_complete(node->child[2]);
}

// 26
bool is_if_statement_complete(Node *node) {
    if (!isExactLabel(node, "<if-statement>")) return false;

    if (node->childCount==4) {
        if (!isExactLabel(node->child[0], "ifsy")) return false;
        if (!is_expression_complete(node->child[1])) return false;
        if (!isExactLabel(node->child[2], "thensy")) return false;
        return is_statement_complete(node->child[3]);
    }

    if (node->childCount==6) {
        if (!isExactLabel(node->child[0], "ifsy")) return false;
        if (!is_expression_complete(node->child[1])) return false;
        if (!isExactLabel(node->child[2], "thensy")) return false;
        if (!is_statement_complete(node->child[3])) return false;
        if (!isExactLabel(node->child[4], "elsy") && !isExactLabel(node->child[4], "elsesy"))
            return false;
        return is_statement_complete(node->child[5]);
    }

    return false;
}

// 27
bool is_case_statement_complete(Node *node) {
    if (!isExactLabel(node, "<case-statement>")) return false;
    if (node->childCount!=5) return false;
    if (!isExactLabel(node->child[0], "casesy")) return false;
    if (!is_expression_complete(node->child[1])) return false;
    if (!isExactLabel(node->child[2], "ofsy")) return false;
    if (!is_case_block_complete(node->child[3])) return false;
    return isExactLabel(node->child[4], "endsy");
}

// 28
bool is_case_block_complete(Node *node) {
    int i = 0;

    if (!isExactLabel(node, "<case-block>")) return false;
    if (node->childCount < 3) return false;
    if (!is_constant_complete(node->child[i])) return false;

    i++;
    while (i + 1 < node->childCount && isExactLabel(node->child[i], "comma")) {
        if (!is_constant_complete(node->child[i+1])) return false;
        i += 2;
    }

    if (i >= node->childCount || !isExactLabel(node->child[i], "colon")) return false;
    i++;
    if (i >= node->childCount || !is_statement_complete(node->child[i])) return false;
    i++;

    while (i < node->childCount) {
        if (i==node->childCount - 1) {
            return isExactLabel(node->child[i], "semicolon") ||
                   is_case_block_complete(node->child[i]);
        }
        if (!isSemicolonCaseBlockPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    return true;
}

// 29
bool is_while_statement_complete(Node *node) {
    if (!isExactLabel(node, "<while-statement>")) return false;
    if (node->childCount!=4) return false;
    if (!isExactLabel(node->child[0], "whilesy")) return false;
    if (!is_expression_complete(node->child[1])) return false;
    if (!isExactLabel(node->child[2], "dosy")) return false;
    return is_statement_complete(node->child[3]);
}

// 30
bool is_repeat_statement_complete(Node *node) {
    if (!isExactLabel(node, "<repeat-statement>")) return false;
    if (node->childCount!=4) return false;
    if (!isExactLabel(node->child[0], "repeatsy")) return false;
    if (!is_statement_list_complete(node->child[1])) return false;
    if (!isExactLabel(node->child[2], "untilsy")) return false;
    return is_expression_complete(node->child[3]);
}

// 31
bool is_for_statement_complete(Node *node) {
    if (!isExactLabel(node, "<for-statement>")) return false;
    if (node->childCount!=8) return false;
    if (!isExactLabel(node->child[0], "forsy")) return false;
    if (!isIdentLabel(node->child[1])) return false;
    if (!isExactLabel(node->child[2], "becomes")) return false;
    if (!is_expression_complete(node->child[3])) return false;
    if (!isExactLabel(node->child[4], "tosy") && !isExactLabel(node->child[4], "downtosy"))
        return false;
    if (!is_expression_complete(node->child[5])) return false;
    if (!isExactLabel(node->child[6], "dosy")) return false;
    return is_statement_complete(node->child[7]);
}

bool is_index_list_complete(Node *node) {
    if (!isExactLabel(node, "<index-list>")) return false;
    if (node->childCount < 1) return false;
    if (!isIntconLabel(node->child[0]) &&
        !isCharconLabel(node->child[0]) &&
        !isIdentLabel(node->child[0])) return false;

    int i = 1;
    while (i + 1 < node->childCount) {
        if (!isExactLabel(node->child[i], "comma")) return false;
        if (!is_index_list_complete(node->child[i + 1])) return false;
        i += 2;
    }

    return true;
}

bool is_component_variable_complete(Node *node) {
    if (!isExactLabel(node, "<component-variable>")) return false;

    if (node->childCount==3) {
        if (!isExactLabel(node->child[0], "lbrack")) return false;
        if (!is_index_list_complete(node->child[1])) return false;
        return isExactLabel(node->child[2], "rbrack");
    }

    if (node->childCount==2) {
        if (!isExactLabel(node->child[0], "period")) return false;
        return isIdentLabel(node->child[1]);
    }

    return false;
}

bool is_variable_complete(Node *node) {
    int i;

    if (!isExactLabel(node, "<variable>")) return false;
    if (node->childCount < 1) return false;
    if (!isIdentLabel(node->child[0])) return false;

    for (i = 1; i < node->childCount; i++) {
        if (!is_component_variable_complete(node->child[i])) return false;
    }

    return true;
}

// 32
bool is_procedure_function_call_complete(Node *node) {
    int effectiveChildCount;

    if (!isProcedureFunctionCallLabel(node)) return false;

    effectiveChildCount = node->childCount;
    if (effectiveChildCount >= 2 &&
        isExactLabel(node->child[effectiveChildCount - 1], "semicolon")) {
        effectiveChildCount--;
    }

    if (effectiveChildCount==3) {
        if (!isIdentLabel(node->child[0])) return false;
        if (!isExactLabel(node->child[1], "lparent")) return false;
        return isExactLabel(node->child[2], "rparent");
    }

    if (effectiveChildCount==4) {
        if (!isIdentLabel(node->child[0])) return false;
        if (!isExactLabel(node->child[1], "lparent")) return false;
        if (!is_parameter_list_complete(node->child[2])) return false;
        return isExactLabel(node->child[3], "rparent");
    }

    return false;
}

// 33
bool is_parameter_list_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<parameter-list>")) return false;
    if (node->childCount < 1) return false;
    if (!is_expression_complete(node->child[0])) return false;

    while (i + 1 < node->childCount) {
        if (!isCommaExpressionPair(node->child[i], node->child[i+1])) return false;
        i += 2;
    }

    return i==node->childCount;
}

// 34
bool is_expression_complete(Node *node) {
    if (!isExactLabel(node, "<expression>")) return false;

    if (node->childCount==1) {
        return is_simple_expression_complete(node->child[0]);
    }

    if (node->childCount==3) {
        if (!is_simple_expression_complete(node->child[0])) return false;
        if (!is_relational_operator_complete(node->child[1])) return false;
        return is_simple_expression_complete(node->child[2]);
    }

    return false;
}

// 35
bool is_simple_expression_complete(Node *node) {
    int i = 0;

    if (!isExactLabel(node, "<simple-expression>")) return false;
    if (node->childCount < 1) return false;

    if (isExactLabel(node->child[0], "plus") || isExactLabel(node->child[0], "minus")) {
        i = 1;
    }

    if (i >= node->childCount || !is_term_complete(node->child[i])) return false;
    i++;

    while (i + 1 < node->childCount) {
        if (!is_additive_operator_complete(node->child[i])) return false;
        if (!is_term_complete(node->child[i+1])) return false;
        i += 2;
    }

    return i==node->childCount;
}

// 36
bool is_term_complete(Node *node) {
    int i = 1;

    if (!isExactLabel(node, "<term>")) return false;
    if (node->childCount < 1) return false;
    if (!is_factor_complete(node->child[0])) return false;

    while (i + 1 < node->childCount) {
        if (!is_multiplicative_operator_complete(node->child[i])) return false;
        if (!is_factor_complete(node->child[i+1])) return false;
        i += 2;
    }

    return i==node->childCount;
}

// 37
bool is_factor_complete(Node *node) {
    if (!isExactLabel(node, "<factor>")) return false;

    if (node->childCount==1) {
        return isIntconLabel(node->child[0]) ||
               isRealconLabel(node->child[0]) ||
               isCharconLabel(node->child[0]) ||
               isStringLabel(node->child[0]) ||
               is_procedure_function_call_complete(node->child[0]) ||
               is_variable_complete(node->child[0]);
    }

    if (node->childCount==2) {
        if (!isExactLabel(node->child[0], "notsy")) return false;
        return is_factor_complete(node->child[1]);
    }

    if (node->childCount==3) {
        if (!isExactLabel(node->child[0], "lparent")) return false;
        if (!is_expression_complete(node->child[1])) return false;
        return isExactLabel(node->child[2], "rparent");
    }

    return false;
}

// 38
bool is_relational_operator_complete(Node *node) {
    if (!isExactLabel(node, "<relational-operator>")) return false;
    if (node->childCount!=1) return false;
    return isExactLabel(node->child[0], "eql") ||
           isExactLabel(node->child[0], "neq") ||
           isExactLabel(node->child[0], "gtr") ||
           isExactLabel(node->child[0], "geq") ||
           isExactLabel(node->child[0], "lss") ||
           isExactLabel(node->child[0], "leq");
}

// 39
bool is_additive_operator_complete(Node *node) {
    if (!isExactLabel(node, "<additive-operator>")) return false;
    if (node->childCount!=1) return false;
    return isExactLabel(node->child[0], "plus") ||
           isExactLabel(node->child[0], "minus") ||
           isExactLabel(node->child[0], "orsy");
}

// 40
bool is_multiplicative_operator_complete(Node *node) {
    if (!isExactLabel(node, "<multiplicative-operator>")) return false;
    if (node->childCount!=1) return false;
    return isExactLabel(node->child[0], "times") ||
           isExactLabel(node->child[0], "rdiv") ||
           isExactLabel(node->child[0], "idiv") ||
           isExactLabel(node->child[0], "imod") ||
           isExactLabel(node->child[0], "andsy");
}
