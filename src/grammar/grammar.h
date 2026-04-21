#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdbool.h>

#include "../node/node.h"

bool isExactLabel(Node *node, const char *label);
bool isIdentLabel(Node *node);
bool isIntconLabel(Node *node);
bool isRealconLabel(Node *node);
bool isCharconLabel(Node *node);
bool isStringLabel(Node *node);
bool isProcedureFunctionCallLabel(Node *node);
bool isSemicolonStatementPair(Node *semicolonNode, Node *statementNode);
bool isCommaExpressionPair(Node *commaNode, Node *exprNode);
bool isCommaIdentPair(Node *commaNode, Node *identNode);
bool isSemicolonParameterGroupPair(Node *semicolonNode, Node *parameterGroupNode);
bool isSemicolonFieldPartPair(Node *semicolonNode, Node *fieldPartNode);
bool isSemicolonCaseBlockPair(Node *semicolonNode, Node *caseBlockNode);
bool isConstDeclEntry(Node *identNode, Node *eqlNode, Node *constantNode, Node *semicolonNode);
bool isTypeDeclEntry(Node *identNode, Node *eqlNode, Node *typeNode, Node *semicolonNode);
bool isVarDeclEntry(Node *identifierListNode, Node *colonNode, Node *typeNode, Node *semicolonNode);

bool is_program_complete(Node *node);
bool is_program_header_complete(Node *node);
bool is_declaration_part_complete(Node *node);
bool is_const_declaration_complete(Node *node);
bool is_constant_complete(Node *node);
bool is_type_declaration_complete(Node *node);
bool is_var_declaration_complete(Node *node);
bool is_identifier_list_complete(Node *node);
bool is_type_complete(Node *node);
bool is_array_type_complete(Node *node);
bool is_range_complete(Node *node);
bool is_enumerated_complete(Node *node);
bool is_record_type_complete(Node *node);
bool is_field_list_complete(Node *node);
bool is_field_part_complete(Node *node);
bool is_subprogram_declaration_complete(Node *node);
bool is_procedure_declaration_complete(Node *node);
bool is_function_declaration_complete(Node *node);
bool is_block_complete(Node *node);
bool is_formal_parameter_list_complete(Node *node);
bool is_parameter_group_complete(Node *node);
bool is_compound_statement_complete(Node *node);
bool is_statement_list_complete(Node *node);
bool is_statement_complete(Node *node);
bool is_assignment_statement_complete(Node *node);
bool is_if_statement_complete(Node *node);
bool is_case_statement_complete(Node *node);
bool is_case_block_complete(Node *node);
bool is_while_statement_complete(Node *node);
bool is_repeat_statement_complete(Node *node);
bool is_for_statement_complete(Node *node);
bool is_procedure_function_call_complete(Node *node);
bool is_parameter_list_complete(Node *node);
bool is_expression_complete(Node *node);
bool is_simple_expression_complete(Node *node);
bool is_term_complete(Node *node);
bool is_factor_complete(Node *node);
bool is_relational_operator_complete(Node *node);
bool is_additive_operator_complete(Node *node);
bool is_multiplicative_operator_complete(Node *node);

#endif
