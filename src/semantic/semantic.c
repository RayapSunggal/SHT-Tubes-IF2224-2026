#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "symbol_table.h"

#define MAX_IDENTIFIER_NAME 128
#define MAX_IDENTIFIER_LIST 64

static bool labelEq(const ParseTreeNode *node, const char *label) {
    return node != NULL && node->label != NULL && strcmp(node->label, label) == 0;
}

static bool labelPrefix(const ParseTreeNode *node, const char *prefix) {
    return node != NULL && node->label != NULL && strncmp(node->label, prefix, strlen(prefix)) == 0;
}

static bool extractLexeme(const char *label, char *out, size_t outSize) {
    if (label == NULL || out == NULL || outSize == 0) {
        return false;
    }

    const char *open = strchr(label, '(');
    const char *close = strrchr(label, ')');
    if (open == NULL || close == NULL || close <= open) {
        return false;
    }

    size_t len = (size_t)(close - open - 1);
    if (len >= outSize) {
        len = outSize - 1;
    }

    memcpy(out, open + 1, len);
    out[len] = '\0';
    return true;
}

static bool getIdentifierName(const ParseTreeNode *node, char *out, size_t outSize) {
    if (node == NULL || out == NULL || outSize == 0) {
        return false;
    }

    if (labelPrefix(node, "IDENT(") || labelPrefix(node, "ident(")) {
        return extractLexeme(node->label, out, outSize);
    }

    return false;
}

static BaseType baseTypeFromName(const char *name) {
    if (name == NULL) {
        return TYPE_NONE;
    }

    if (strcmp(name, "integer") == 0 || strcmp(name, "INTEGER") == 0) {
        return TYPE_INTEGER;
    }
    if (strcmp(name, "real") == 0 || strcmp(name, "REAL") == 0) {
        return TYPE_REAL;
    }
    if (strcmp(name, "boolean") == 0 || strcmp(name, "BOOLEAN") == 0) {
        return TYPE_BOOLEAN;
    }
    if (strcmp(name, "char") == 0 || strcmp(name, "CHAR") == 0) {
        return TYPE_CHAR;
    }
    if (strcmp(name, "string") == 0 || strcmp(name, "STRING") == 0) {
        return TYPE_STRING;
    }
    return TYPE_NONE;
}

static int parseIntegerConstant(const ParseTreeNode *node, bool *ok) {
    if (node == NULL || ok == NULL) {
        if (ok) *ok = false;
        return 0;
    }

    if (labelPrefix(node, "INTCON(") || labelPrefix(node, "intcon(")) {
        char buffer[MAX_IDENTIFIER_NAME];
        if (extractLexeme(node->label, buffer, sizeof(buffer))) {
            *ok = true;
            return atoi(buffer);
        }
    }

    *ok = false;
    return 0;
}

static int parseConstantValue(const ParseTreeNode *node, int *out) {
    if (node == NULL || out == NULL) return 0;
    if (labelEq(node, "<constant>") && node->childCount == 1) {
        node = node->children[0];
    }
    bool ok = false;
    int v = parseIntegerConstant(node, &ok);
    if (ok) { *out = v; return 1; }
    if (labelPrefix(node, "CHARCON(") || labelPrefix(node, "charcon(")) {
        char buf[MAX_IDENTIFIER_NAME];
        if (extractLexeme(node->label, buf, sizeof(buf)) && buf[0] != '\0') {
            *out = (unsigned char)buf[0];
            return 1;
        }
    }
    return 0;
}

static int parseRangeBounds(const ParseTreeNode *node, int *low, int *high) {
    if (!labelEq(node, "<range>") || node->childCount != 4) {
        return 0;
    }

    if (!parseConstantValue(node->children[0], low)) return 0;
    if (!parseConstantValue(node->children[3], high)) return 0;
    return 1;
}

static bool parseIdentifierList(const ParseTreeNode *node, char names[MAX_IDENTIFIER_LIST][MAX_IDENTIFIER_NAME], int *count) {
    if (node == NULL || !labelEq(node, "<identifier-list>") || count == NULL) {
        return false;
    }

    *count = 0;
    for (int i = 0; i < (int)node->childCount; i++) {
        if (labelPrefix(node->children[i], "IDENT(") || labelPrefix(node->children[i], "ident(")) {
            if (*count >= MAX_IDENTIFIER_LIST) {
                return false;
            }
            extractLexeme(node->children[i]->label, names[*count], MAX_IDENTIFIER_NAME);
            (*count)++;
        }
    }

    return *count > 0;
}

static BaseType parseTypeNode(const ParseTreeNode *node, int *arrayRef) {
    if (node == NULL || !labelEq(node, "<type>") || node->childCount == 0) {
        return TYPE_NONE;
    }

    const ParseTreeNode *child = node->children[0];
    if (child == NULL) {
        return TYPE_NONE;
    }

    if (labelPrefix(child, "IDENT(") || labelPrefix(child, "ident(")) {
        char name[MAX_IDENTIFIER_NAME];
        if (!extractLexeme(child->label, name, sizeof(name))) {
            return TYPE_NONE;
        }
        BaseType bt = baseTypeFromName(name);
        if (bt != TYPE_NONE) {
            return bt;
        }
        int idx = symLookup(name);
        if (idx != -1 && tab[idx].obj == OBJ_TYPE) {
            if (arrayRef != NULL) {
                *arrayRef = tab[idx].ref;
            }
            return tab[idx].type;
        }
        return TYPE_NONE;
    }

    if (labelEq(child, "<array-type>")) {
        int low = 0, high = 0;
        BaseType indexType = TYPE_INTEGER;

        const ParseTreeNode *rangeNode = NULL;
        const ParseTreeNode *elemTypeNode = NULL;
        for (size_t ci = 0; ci < child->childCount; ci++) {
            if (labelEq(child->children[ci], "<range>")) rangeNode = child->children[ci];
            if (labelEq(child->children[ci], "<type>"))  elemTypeNode = child->children[ci];
        }

        if (rangeNode != NULL) {
            if (!parseRangeBounds(rangeNode, &low, &high)) return TYPE_ARRAY;
            int lo0 = low, hi0 = high;
            if (lo0 >= 'A' && lo0 <= 'z') indexType = TYPE_CHAR;
            else indexType = TYPE_INTEGER;
            (void)lo0; (void)hi0;
        }

        int nestedArrayRef = -1;
        BaseType elementType = TYPE_INTEGER;
        if (elemTypeNode != NULL) {
            elementType = parseTypeNode(elemTypeNode, &nestedArrayRef);
        }

        int size = sizeOfBaseType(elementType);
        if (size == 0) size = 1;

        if (arrayRef != NULL) {
            *arrayRef = symEnterArray(indexType, elementType, nestedArrayRef, low, high, size);
        }
        return TYPE_ARRAY;
    }

    if (labelEq(child, "<record-type>")) {
        int recBlockIdx = symEnterRecordBlock();
        const ParseTreeNode *fieldList = NULL;
        for (size_t ci = 0; ci < child->childCount; ci++) {
            if (labelEq(child->children[ci], "<field-list>")) {
                fieldList = child->children[ci];
                break;
            }
        }
        if (fieldList != NULL) {
            int adr = 0;
            for (size_t ci = 0; ci < fieldList->childCount; ci++) {
                const ParseTreeNode *fp = fieldList->children[ci];
                if (!labelEq(fp, "<field-part>")) continue;
                char names[MAX_IDENTIFIER_LIST][MAX_IDENTIFIER_NAME];
                int nameCount = 0;
                int fieldArrayRef = -1;
                BaseType fieldType = TYPE_NONE;
                for (size_t fi = 0; fi < fp->childCount; fi++) {
                    if (labelEq(fp->children[fi], "<identifier-list>"))
                        parseIdentifierList(fp->children[fi], names, &nameCount);
                    if (labelEq(fp->children[fi], "<type>"))
                        fieldType = parseTypeNode(fp->children[fi], &fieldArrayRef);
                }
                if (fieldType == TYPE_NONE) continue;
                for (int k = 0; k < nameCount; k++) {
                    symEnterField(names[k], fieldType, fieldArrayRef, adr);
                    adr += sizeOfBaseType(fieldType);
                }
            }
        }
        symExitRecordBlock();
        if (arrayRef != NULL) *arrayRef = recBlockIdx;
        return TYPE_RECORD;
    }

    return TYPE_NONE;
}

static void setSemanticError(char *message, size_t messageSize, const char *format, ...) {
    if (message == NULL || messageSize == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(message, messageSize, format, args);
    va_end(args);
}

static BaseType inferExpressionType(const ParseTreeNode *node, char *message, size_t messageSize);

static BaseType inferFactorType(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL) {
        return TYPE_NONE;
    }

    if (labelEq(node, "<factor>")) {
        if (node->childCount == 1) {
            return inferFactorType(node->children[0], message, messageSize);
        }
        if (node->childCount == 3) {
            return inferExpressionType(node->children[1], message, messageSize);
        }
        return TYPE_NONE;
    }

    if (labelPrefix(node, "INTCON(") || labelPrefix(node, "intcon(")) {
        return TYPE_INTEGER;
    }
    if (labelPrefix(node, "REALCON(") || labelPrefix(node, "realcon(")) {
        return TYPE_REAL;
    }
    if (labelPrefix(node, "CHARCON(") || labelPrefix(node, "charcon(")) {
        return TYPE_CHAR;
    }
    if (labelPrefix(node, "STRING(") || labelPrefix(node, "string(")) {
        return TYPE_STRING;
    }

    if (labelEq(node, "<variable>")) {
        const ParseTreeNode *idNode = node->childCount > 0 ? node->children[0] : NULL;
        char name[MAX_IDENTIFIER_NAME];
        if (getIdentifierName(idNode, name, sizeof(name))) {
            int idx = symLookup(name);
            if (idx == -1) {
                setSemanticError(message, messageSize, "Identifier '%s' not declared.", name);
                return TYPE_NONE;
            }
            BaseType t = tab[idx].type;
            if (node->childCount >= 2 && labelEq(node->children[1], "<component-variable>")) {
                const ParseTreeNode *comp = node->children[1];
                if (comp->childCount > 0 && labelEq(comp->children[0], "lbrack")) {
                    if (t == TYPE_ARRAY && tab[idx].ref >= 0) {
                        return atab[tab[idx].ref].etyp;
                    }
                    return TYPE_INTEGER;
                }
                return TYPE_INTEGER;
            }
            return t;
        }
        return TYPE_NONE;
    }

    if (labelEq(node, "<procedure/function-call>")) {
        const ParseTreeNode *idNode = node->childCount > 0 ? node->children[0] : NULL;
        char name[MAX_IDENTIFIER_NAME];
        if (getIdentifierName(idNode, name, sizeof(name))) {
            int idx = symLookup(name);
            if (idx == -1) {
                setSemanticError(message, messageSize, "Procedure/function '%s' not declared.", name);
                return TYPE_NONE;
            }
            if (tab[idx].obj == OBJ_FUNCTION) {
                return tab[idx].type;
            }
            setSemanticError(message, messageSize, "Identifier '%s' is not a function.", name);
            return TYPE_NONE;
        }
        return TYPE_NONE;
    }

    if (labelEq(node, "<expression>") || labelEq(node, "<simple-expression>") || labelEq(node, "<term>")) {
        return inferExpressionType(node, message, messageSize);
    }

    return TYPE_NONE;
}

static bool isAndOperator(const ParseTreeNode *opNode) {
    if (opNode == NULL) return false;
    if (labelEq(opNode, "andsy")) return true;
    if (labelEq(opNode, "<multiplicative-operator>") && opNode->childCount == 1)
        return labelEq(opNode->children[0], "andsy");
    return false;
}

static bool isOrOperator(const ParseTreeNode *opNode) {
    if (opNode == NULL) return false;
    if (labelEq(opNode, "orsy")) return true;
    if (labelEq(opNode, "<additive-operator>") && opNode->childCount == 1)
        return labelEq(opNode->children[0], "orsy");
    return false;
}

static BaseType inferTermType(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (!labelEq(node, "<term>") || node->childCount < 1) {
        return TYPE_NONE;
    }

    BaseType result = inferFactorType(node->children[0], message, messageSize);
    if (result == TYPE_NONE) {
        return TYPE_NONE;
    }

    for (size_t i = 1; i + 1 < node->childCount; i += 2) {
        bool isAnd = isAndOperator(node->children[i]);
        BaseType rhs = inferFactorType(node->children[i + 1], message, messageSize);
        if (rhs == TYPE_NONE) {
            return TYPE_NONE;
        }
        if (isAnd) {
            if (result != TYPE_BOOLEAN || rhs != TYPE_BOOLEAN) {
                setSemanticError(message, messageSize, "Operator 'and' requires boolean operands.");
                return TYPE_NONE;
            }
            result = TYPE_BOOLEAN;
        } else if (result != rhs) {
            if (!((result == TYPE_INTEGER || result == TYPE_REAL) &&
                  (rhs == TYPE_INTEGER || rhs == TYPE_REAL))) {
                setSemanticError(message, messageSize, "Type mismatch in term.");
                return TYPE_NONE;
            }
            result = TYPE_REAL;
        }
    }

    return result;
}

static BaseType inferSimpleExpressionType(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (!labelEq(node, "<simple-expression>") || node->childCount < 1) {
        return TYPE_NONE;
    }

    size_t offset = 0;
    if (labelEq(node->children[0], "plus") || labelEq(node->children[0], "minus")) {
        offset = 1;
    }

    if (offset >= node->childCount) {
        return TYPE_NONE;
    }

    BaseType result = inferTermType(node->children[offset], message, messageSize);
    if (result == TYPE_NONE) {
        return TYPE_NONE;
    }

    for (size_t i = offset + 1; i + 1 < node->childCount; i += 2) {
        bool isOr = isOrOperator(node->children[i]);
        BaseType rhs = inferTermType(node->children[i + 1], message, messageSize);
        if (rhs == TYPE_NONE) {
            return TYPE_NONE;
        }
        if (isOr) {
            if (result != TYPE_BOOLEAN || rhs != TYPE_BOOLEAN) {
                setSemanticError(message, messageSize, "Operator 'or' requires boolean operands.");
                return TYPE_NONE;
            }
            result = TYPE_BOOLEAN;
        } else if (result != rhs) {
            if (!((result == TYPE_INTEGER || result == TYPE_REAL) &&
                  (rhs == TYPE_INTEGER || rhs == TYPE_REAL))) {
                setSemanticError(message, messageSize, "Type mismatch in simple expression.");
                return TYPE_NONE;
            }
            result = TYPE_REAL;
        }
    }

    return result;
}

static BaseType inferExpressionType(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<expression>")) {
        return TYPE_NONE;
    }

    if (node->childCount == 1) {
        return inferSimpleExpressionType(node->children[0], message, messageSize);
    }
    if (node->childCount == 3) {
        BaseType left = inferSimpleExpressionType(node->children[0], message, messageSize);
        BaseType right = inferSimpleExpressionType(node->children[2], message, messageSize);
        if (left == TYPE_NONE || right == TYPE_NONE) {
            return TYPE_NONE;
        }
        if (left != right) {
            setSemanticError(message, messageSize, "Type mismatch in relational expression.");
            return TYPE_NONE;
        }
        return TYPE_BOOLEAN;
    }

    return TYPE_NONE;
}

static bool processStatement(const ParseTreeNode *node, char *message, size_t messageSize);
static bool processCompoundStatement(const ParseTreeNode *node, char *message, size_t messageSize);
static bool processStatementList(const ParseTreeNode *node, char *message, size_t messageSize);

static bool processIfStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<if-statement>")) return true;

    const ParseTreeNode *condNode = NULL;
    const ParseTreeNode *thenNode = NULL;
    const ParseTreeNode *elseNode = NULL;

    for (size_t i = 0; i < node->childCount; i++) {
        if (labelEq(node->children[i], "<expression>")) { condNode = node->children[i]; continue; }
        if (labelEq(node->children[i], "thensy")) {
            if (i + 1 < node->childCount) thenNode = node->children[i + 1];
        }
        if (labelEq(node->children[i], "elsesy")) {
            if (i + 1 < node->childCount) elseNode = node->children[i + 1];
        }
    }

    if (condNode != NULL) {
        BaseType condType = inferExpressionType(condNode, message, messageSize);
        if (condType == TYPE_NONE) return false;
        if (condType != TYPE_BOOLEAN) {
            setSemanticError(message, messageSize, "Condition in if-statement must be boolean.");
            return false;
        }
    }
    if (thenNode != NULL && !processStatement(thenNode, message, messageSize)) return false;
    if (elseNode != NULL && !processStatement(elseNode, message, messageSize)) return false;
    return true;
}

static bool processWhileStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<while-statement>")) return true;

    const ParseTreeNode *condNode = NULL;
    const ParseTreeNode *bodyNode = NULL;

    for (size_t i = 0; i < node->childCount; i++) {
        if (labelEq(node->children[i], "<expression>")) condNode = node->children[i];
        if (labelEq(node->children[i], "<compound-statement>")) bodyNode = node->children[i];
    }

    if (condNode != NULL) {
        BaseType condType = inferExpressionType(condNode, message, messageSize);
        if (condType == TYPE_NONE) return false;
        if (condType != TYPE_BOOLEAN) {
            setSemanticError(message, messageSize, "Condition in while-statement must be boolean.");
            return false;
        }
    }
    if (bodyNode != NULL && !processCompoundStatement(bodyNode, message, messageSize)) return false;
    return true;
}

static bool processForStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<for-statement>")) return true;

    const ParseTreeNode *counterNode = NULL;
    const ParseTreeNode *bodyNode = NULL;
    int exprCount = 0;

    for (size_t i = 0; i < node->childCount; i++) {
        if ((labelPrefix(node->children[i], "IDENT(") || labelPrefix(node->children[i], "ident(")) && counterNode == NULL) {
            counterNode = node->children[i];
        }
        if (labelEq(node->children[i], "<expression>")) {
            if (exprCount == 0) {
                inferExpressionType(node->children[i], message, messageSize);
                if (message[0] != '\0') return false;
            }
            exprCount++;
        }
        if (labelEq(node->children[i], "<compound-statement>")) bodyNode = node->children[i];
    }

    if (counterNode != NULL) {
        char name[MAX_IDENTIFIER_NAME];
        if (getIdentifierName(counterNode, name, sizeof(name))) {
            int idx = symLookup(name);
            if (idx == -1) {
                setSemanticError(message, messageSize, "Counter variable '%s' not declared.", name);
                return false;
            }
            if (tab[idx].type != TYPE_INTEGER) {
                setSemanticError(message, messageSize, "Counter variable '%s' must be integer.", name);
                return false;
            }
        }
    }
    if (bodyNode != NULL && !processCompoundStatement(bodyNode, message, messageSize)) return false;
    return true;
}

static bool processRepeatStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<repeat-statement>")) return true;

    for (size_t i = 0; i < node->childCount; i++) {
        if (labelEq(node->children[i], "<statement-list>")) {
            if (!processStatementList(node->children[i], message, messageSize)) return false;
        }
        if (labelEq(node->children[i], "<expression>")) {
            BaseType condType = inferExpressionType(node->children[i], message, messageSize);
            if (condType == TYPE_NONE) return false;
            if (condType != TYPE_BOOLEAN) {
                setSemanticError(message, messageSize, "Condition in repeat-until must be boolean.");
                return false;
            }
        }
    }
    return true;
}

static bool processStatementList(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<statement-list>")) {
        return true;
    }

    for (int i = 0; i < (int)node->childCount; i++) {
        if (labelEq(node->children[i], "<statement>")) {
            if (!processStatement(node->children[i], message, messageSize)) {
                return false;
            }
        }
    }

    return true;
}

static bool processCompoundStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<compound-statement>")) {
        return true;
    }

    if (node->childCount == 3 && labelEq(node->children[1], "<statement-list>")) {
        return processStatementList(node->children[1], message, messageSize);
    }

    return true;
}

static bool processDeclarationPart(const ParseTreeNode *node, char *message, size_t messageSize);

static bool processBlock(const ParseTreeNode *node, char *message, size_t messageSize);

static bool processAssignmentStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<assignment-statement>") || node->childCount != 3) {
        return true;
    }

    const ParseTreeNode *variableNode = node->children[0];
    const ParseTreeNode *expressionNode = node->children[2];

    if (!labelEq(variableNode, "<variable>")) {
        return true;
    }

    const ParseTreeNode *idNode = variableNode->childCount > 0 ? variableNode->children[0] : NULL;
    char name[MAX_IDENTIFIER_NAME];
    if (!getIdentifierName(idNode, name, sizeof(name))) {
        return true;
    }

    int idx = symLookup(name);
    if (idx == -1) {
        setSemanticError(message, messageSize, "Identifier '%s' not declared.", name);
        return false;
    }

    BaseType leftType = tab[idx].type;
    if (variableNode->childCount >= 2 && labelEq(variableNode->children[1], "<component-variable>")) {
        const ParseTreeNode *comp = variableNode->children[1];
        if (comp->childCount > 0 && labelEq(comp->children[0], "lbrack")) {
            if (leftType == TYPE_ARRAY && tab[idx].ref >= 0) {
                leftType = atab[tab[idx].ref].etyp;
            } else {
                leftType = TYPE_INTEGER;
            }
        } else {
            leftType = TYPE_INTEGER;
        }
    }

    BaseType rightType = inferExpressionType(expressionNode, message, messageSize);
    if (rightType == TYPE_NONE) {
        return false;
    }

    if (leftType == rightType) return true;
    if (leftType == TYPE_REAL && rightType == TYPE_INTEGER) return true;

    setSemanticError(message, messageSize, "Type mismatch assigning to '%s'.", name);
    return false;
}

static bool processProcedureFunctionCall(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<procedure/function-call>") || node->childCount < 1) {
        return true;
    }

    const ParseTreeNode *idNode = node->children[0];
    char name[MAX_IDENTIFIER_NAME];
    if (!getIdentifierName(idNode, name, sizeof(name))) {
        return true;
    }

    int idx = symLookup(name);
    if (idx == -1) {
        setSemanticError(message, messageSize, "Procedure/function '%s' not declared.", name);
        return false;
    }

    if (tab[idx].obj != OBJ_PROCEDURE && tab[idx].obj != OBJ_FUNCTION) {
        if (strcmp(name, "writeln") == 0 || strcmp(name, "readln") == 0 || strcmp(name, "write") == 0 || strcmp(name, "read") == 0) {
            return true;
        }
        setSemanticError(message, messageSize, "Identifier '%s' is not a callable procedure or function.", name);
        return false;
    }

    for (int i = 0; i < (int)node->childCount; i++) {
        if (labelEq(node->children[i], "<parameter-list>")) {
            const ParseTreeNode *paramList = node->children[i];
            for (int j = 0; j < (int)paramList->childCount; j++) {
                if (labelEq(paramList->children[j], "<expression>")) {
                    BaseType type = inferExpressionType(paramList->children[j], message, messageSize);
                    if (type == TYPE_NONE) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

static bool processStatement(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<statement>")) {
        return true;
    }

    if (node->childCount != 1) {
        return true;
    }

    const ParseTreeNode *child = node->children[0];
    if (labelEq(child, "<assignment-statement>")) {
        return processAssignmentStatement(child, message, messageSize);
    }
    if (labelEq(child, "<procedure/function-call>")) {
        return processProcedureFunctionCall(child, message, messageSize);
    }
    if (labelEq(child, "<compound-statement>")) {
        return processCompoundStatement(child, message, messageSize);
    }
    if (labelEq(child, "<if-statement>")) {
        return processIfStatement(child, message, messageSize);
    }
    if (labelEq(child, "<while-statement>")) {
        return processWhileStatement(child, message, messageSize);
    }
    if (labelEq(child, "<for-statement>")) {
        return processForStatement(child, message, messageSize);
    }
    if (labelEq(child, "<repeat-statement>")) {
        return processRepeatStatement(child, message, messageSize);
    }

    return true;
}

static bool processBlock(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "block") || node->childCount != 2) {
        return true;
    }

    if (!processDeclarationPart(node->children[0], message, messageSize)) {
        return false;
    }

    return processCompoundStatement(node->children[1], message, messageSize);
}

static BaseType inferConstantType(const ParseTreeNode *node) {
    if (node == NULL) return TYPE_NONE;
    if (labelEq(node, "<constant>") && node->childCount >= 1) {
        node = node->children[0];
    }
    if (labelPrefix(node, "INTCON(") || labelPrefix(node, "intcon(")) return TYPE_INTEGER;
    if (labelPrefix(node, "REALCON(") || labelPrefix(node, "realcon(")) return TYPE_REAL;
    if (labelPrefix(node, "CHARCON(") || labelPrefix(node, "charcon(")) return TYPE_CHAR;
    if (labelPrefix(node, "STRING(") || labelPrefix(node, "string(")) return TYPE_STRING;
    if (labelPrefix(node, "IDENT(") || labelPrefix(node, "ident(")) {
        char name[MAX_IDENTIFIER_NAME];
        if (extractLexeme(node->label, name, sizeof(name))) {
            int idx = symLookup(name);
            if (idx != -1 && tab[idx].obj == OBJ_CONSTANT) return tab[idx].type;
        }
    }
    return TYPE_NONE;
}

static bool processConstDeclaration(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<const-declaration>")) return true;

    for (int i = 1; i + 3 <= (int)node->childCount; i += 4) {
        const ParseTreeNode *nameNode  = node->children[i];
        const ParseTreeNode *constNode = node->children[i + 2];

        char name[MAX_IDENTIFIER_NAME];
        if (!(labelPrefix(nameNode, "IDENT(") || labelPrefix(nameNode, "ident("))) continue;
        if (!extractLexeme(nameNode->label, name, sizeof(name))) continue;

        BaseType bt = inferConstantType(constNode);
        if (bt == TYPE_NONE) {
            setSemanticError(message, messageSize, "Unknown type for constant '%s'.", name);
            return false;
        }

        int result = symEnter(name, OBJ_CONSTANT, bt, -1, 0, 0);
        if (result == -1) {
            setSemanticError(message, messageSize, "Redeclaration of constant '%s'.", name);
            return false;
        }
    }
    return true;
}

static bool processTypeDeclaration(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<type-declaration>")) return true;

    for (int i = 1; i + 3 <= (int)node->childCount; i += 4) {
        const ParseTreeNode *nameNode = node->children[i];
        const ParseTreeNode *typeNode = node->children[i + 2];

        char name[MAX_IDENTIFIER_NAME];
        if (!(labelPrefix(nameNode, "IDENT(") || labelPrefix(nameNode, "ident("))) continue;
        if (!extractLexeme(nameNode->label, name, sizeof(name))) continue;

        int arrayRef = -1;
        BaseType bt = parseTypeNode(typeNode, &arrayRef);
        if (bt == TYPE_NONE) {
            setSemanticError(message, messageSize, "Unknown type in type declaration of '%s'.", name);
            return false;
        }

        int result = symEnter(name, OBJ_TYPE, bt, arrayRef, 0, 0);
        if (result == -1) {
            setSemanticError(message, messageSize, "Redeclaration of type '%s'.", name);
            return false;
        }
    }
    return true;
}

static bool processVarDeclaration(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<var-declaration>")) {
        return true;
    }

    for (int i = 1; i + 3 <= (int)node->childCount; i += 4) {
        const ParseTreeNode *identifierList = node->children[i];
        const ParseTreeNode *typeNode = node->children[i + 2];

        char names[MAX_IDENTIFIER_LIST][MAX_IDENTIFIER_NAME];
        int nameCount = 0;
        if (!parseIdentifierList(identifierList, names, &nameCount)) {
            continue;
        }

        int arrayRef = -1;
        BaseType baseType = parseTypeNode(typeNode, &arrayRef);
        if (baseType == TYPE_NONE) {
            setSemanticError(message, messageSize, "Unknown type in variable declaration.");
            return false;
        }

        int startAdr = btab[display[currentLevel]].vsze;
        for (int j = 0; j < nameCount; j++) {
            int result = symEnter(names[j], OBJ_VARIABLE, baseType, arrayRef, 1, startAdr);
            if (result == -1) {
                setSemanticError(message, messageSize, "Redeclaration of identifier '%s' in the same scope.", names[j]);
                return false;
            }
            startAdr += sizeOfBaseType(baseType);
        }
    }

    return true;
}

static bool processParameterGroup(const ParseTreeNode *node, char *message, size_t messageSize, int *inserted) {
    if (node == NULL || !labelEq(node, "<parameter-group>") || inserted == NULL) {
        return false;
    }

    char names[MAX_IDENTIFIER_LIST][MAX_IDENTIFIER_NAME];
    int nameCount = 0;
    if (!parseIdentifierList(node->children[0], names, &nameCount)) {
        return false;
    }

    int arrayRef = -1;
    BaseType baseType = TYPE_NONE;
    const ParseTreeNode *typeNode = node->children[2];
    if (labelEq(typeNode, "<type>")) {
        baseType = parseTypeNode(typeNode, &arrayRef);
    } else if (labelPrefix(typeNode, "IDENT(") || labelPrefix(typeNode, "ident(")) {
        char typeName[MAX_IDENTIFIER_NAME];
        extractLexeme(typeNode->label, typeName, sizeof(typeName));
        baseType = baseTypeFromName(typeName);
    }

    if (baseType == TYPE_NONE) {
        setSemanticError(message, messageSize, "Unknown type in parameter group.");
        return false;
    }

    int startAdr = btab[display[currentLevel]].psze;
    for (int i = 0; i < nameCount; i++) {
        int result = symEnter(names[i], OBJ_VARIABLE, baseType, arrayRef, 0, startAdr);
        if (result == -1) {
            setSemanticError(message, messageSize, "Redeclaration of parameter '%s'.", names[i]);
            return false;
        }
        startAdr += sizeOfBaseType(baseType);
        (*inserted)++;
    }

    return true;
}

static bool processFormalParameterList(const ParseTreeNode *node, char *message, size_t messageSize, int *paramCount) {
    if (node == NULL || !labelEq(node, "<formal-parameter-list>") || paramCount == NULL) {
        return false;
    }

    *paramCount = 0;
    for (int i = 0; i < (int)node->childCount; i++) {
        if (labelEq(node->children[i], "<parameter-group>")) {
            if (!processParameterGroup(node->children[i], message, messageSize, paramCount)) {
                return false;
            }
        }
    }

    return true;
}

static bool processSubprogramDeclaration(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<subprogram-declaration>") || node->childCount != 1) {
        return true;
    }

    const ParseTreeNode *subprogram = node->children[0];
    if (subprogram == NULL) {
        return true;
    }

    if (labelEq(subprogram, "<procedure-declaration>")) {
        if (subprogram->childCount < 3) {
            return true;
        }

        const ParseTreeNode *identNode = subprogram->children[1];
        char name[MAX_IDENTIFIER_NAME];
        if (!getIdentifierName(identNode, name, sizeof(name))) {
            return true;
        }

        int paramCount = 0;
        int procedureIndex = symEnter(name, OBJ_PROCEDURE, TYPE_VOID, -1, 0, 0);
        if (procedureIndex == -1) {
            setSemanticError(message, messageSize, "Redeclaration of procedure '%s'.", name);
            return false;
        }

        symEnterScope();
        tab[procedureIndex].ref = display[currentLevel];

        size_t childIndex = 2;
        if (childIndex < subprogram->childCount && labelEq(subprogram->children[childIndex], "<formal-parameter-list>")) {
            if (!processFormalParameterList(subprogram->children[childIndex], message, messageSize, &paramCount)) {
                return false;
            }
            tab[procedureIndex].nrm = paramCount;
            childIndex++;
        }

        while (childIndex < subprogram->childCount && !labelEq(subprogram->children[childIndex], "block")) {
            childIndex++;
        }

        if (childIndex < subprogram->childCount) {
            if (!processBlock(subprogram->children[childIndex], message, messageSize)) {
                symExitScope();
                return false;
            }
        }

        symExitScope();
        return true;
    }

    if (labelEq(subprogram, "<function-declaration>")) {
        if (subprogram->childCount < 5) return true;

        const ParseTreeNode *identNode = subprogram->children[1];
        char name[MAX_IDENTIFIER_NAME];
        if (!getIdentifierName(identNode, name, sizeof(name))) return true;

        BaseType returnType = TYPE_NONE;
        size_t retIdx = 0;
        for (size_t ci = 2; ci + 1 < subprogram->childCount; ci++) {
            if (labelEq(subprogram->children[ci], "colon")) {
                const ParseTreeNode *retNode = subprogram->children[ci + 1];
                if (labelPrefix(retNode, "IDENT(") || labelPrefix(retNode, "ident(")) {
                    char typeName[MAX_IDENTIFIER_NAME];
                    extractLexeme(retNode->label, typeName, sizeof(typeName));
                    returnType = baseTypeFromName(typeName);
                    if (returnType == TYPE_NONE) {
                        int tidx = symLookup(typeName);
                        if (tidx != -1 && tab[tidx].obj == OBJ_TYPE) returnType = tab[tidx].type;
                    }
                }
                retIdx = ci;
                break;
            }
        }

        int paramCount = 0;
        int funcIndex = symEnter(name, OBJ_FUNCTION, returnType, -1, 0, 0);
        if (funcIndex == -1) {
            setSemanticError(message, messageSize, "Redeclaration of function '%s'.", name);
            return false;
        }

        symEnterScope();
        tab[funcIndex].ref = display[currentLevel];

        symEnter(name, OBJ_VARIABLE, returnType, -1, 0, 0);

        for (size_t ci = 2; ci < subprogram->childCount; ci++) {
            if (labelEq(subprogram->children[ci], "<formal-parameter-list>")) {
                if (!processFormalParameterList(subprogram->children[ci], message, messageSize, &paramCount)) {
                    symExitScope();
                    return false;
                }
                tab[funcIndex].nrm = paramCount;
                break;
            }
        }

        for (size_t ci = retIdx; ci < subprogram->childCount; ci++) {
            if (labelEq(subprogram->children[ci], "block")) {
                if (!processBlock(subprogram->children[ci], message, messageSize)) {
                    symExitScope();
                    return false;
                }
                break;
            }
        }

        symExitScope();
        return true;
    }

    return true;
}

static bool processDeclarationPart(const ParseTreeNode *node, char *message, size_t messageSize) {
    if (node == NULL || !labelEq(node, "<declaration-part>")) {
        return true;
    }

    for (int i = 0; i < (int)node->childCount; i++) {
        const ParseTreeNode *child = node->children[i];
        if (labelEq(child, "<const-declaration>")) {
            if (!processConstDeclaration(child, message, messageSize)) {
                return false;
            }
        } else if (labelEq(child, "<type-declaration>")) {
            if (!processTypeDeclaration(child, message, messageSize)) {
                return false;
            }
        } else if (labelEq(child, "<var-declaration>")) {
            if (!processVarDeclaration(child, message, messageSize)) {
                return false;
            }
        } else if (labelEq(child, "<subprogram-declaration>")) {
            if (!processSubprogramDeclaration(child, message, messageSize)) {
                return false;
            }
        }
    }

    return true;
}

bool analyzeSemanticTree(const ParseTreeNode *root, char *message, size_t messageSize) {
    if (root == NULL || message == NULL || messageSize == 0) {
        return false;
    }

    setSemanticError(message, messageSize, "");
    symInit();

    if (!labelEq(root, "<program>")) {
        setSemanticError(message, messageSize, "Semantic analysis requires a <program> parse tree root.");
        return false;
    }

    if (root->childCount < 4) {
        setSemanticError(message, messageSize, "Malformed program parse tree.");
        return false;
    }

    const ParseTreeNode *header = root->children[0];
    const ParseTreeNode *declarations = root->children[1];
    const ParseTreeNode *compound = root->children[2];

    if (!labelEq(header, "<program-header>")) {
        setSemanticError(message, messageSize, "Expected <program-header> in program root.");
        return false;
    }

    if (header->childCount >= 2) {
        char programName[MAX_IDENTIFIER_NAME];
        if (getIdentifierName(header->children[1], programName, sizeof(programName))) {
            if (symEnter(programName, OBJ_PROGRAM, TYPE_VOID, -1, 0, 0) == -1) {
                setSemanticError(message, messageSize, "Redeclaration of program identifier '%s'.", programName);
                return false;
            }
        }
    }

    if (!processDeclarationPart(declarations, message, messageSize)) {
        return false;
    }

    if (!processCompoundStatement(compound, message, messageSize)) {
        return false;
    }

    return true;
}
