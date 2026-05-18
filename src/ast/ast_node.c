#include "ast_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cloneStr(const char *src) {
    char *copy;
    size_t len;

    if (src == NULL) return NULL;
    len = strlen(src);
    copy = (char *)malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

AstNode *astCreateNode(AstNodeType type) {
    AstNode *node = (AstNode *)calloc(1, sizeof(AstNode));
    if (node == NULL) return NULL;
    node->type = type;
    node->typeIdx = -1;
    node->tabIdx = -1;
    node->lexLevel = -1;
    return node;
}

AstNode *astCreateSval(AstNodeType type, const char *sval) {
    AstNode *node = astCreateNode(type);
    if (node == NULL) return NULL;
    node->sval = cloneStr(sval);
    return node;
}

AstNode *astCreateIval(AstNodeType type, long long ival) {
    AstNode *node = astCreateNode(type);
    if (node == NULL) return NULL;
    node->ival = ival;
    return node;
}

AstNode *astCreateRval(AstNodeType type, double rval) {
    AstNode *node = astCreateNode(type);
    if (node == NULL) return NULL;
    node->rval = rval;
    return node;
}

int astAddChild(AstNode *parent, AstNode *child) {
    AstNode **newChildren;
    size_t newCapacity;

    if (parent == NULL || child == NULL) return 0;

    if (parent->childCount == parent->childCapacity) {
        newCapacity = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        newChildren = (AstNode **)realloc(parent->children, newCapacity * sizeof(AstNode *));
        if (newChildren == NULL) return 0;
        parent->children = newChildren;
        parent->childCapacity = newCapacity;
    }

    parent->children[parent->childCount++] = child;
    return 1;
}

void astFree(AstNode *node) {
    size_t i;

    if (node == NULL) return;
    for (i = 0; i < node->childCount; i++) {
        astFree(node->children[i]);
    }
    free(node->children);
    free(node->sval);
    free(node);
}

static const char *astTypeName(AstNodeType type) {
    switch (type) {
        case AST_PROGRAM:       return "Program";
        case AST_BLOCK:         return "Block";
        case AST_CONST_DECL:    return "ConstDecl";
        case AST_TYPE_DECL:     return "TypeDecl";
        case AST_VAR_DECL:      return "VarDecl";
        case AST_PARAM_GROUP:   return "ParamGroup";
        case AST_PROC_DECL:     return "ProcDecl";
        case AST_FUNC_DECL:     return "FuncDecl";
        case AST_TYPE_IDENT:    return "TypeIdent";
        case AST_ARRAY_TYPE:    return "ArrayType";
        case AST_RECORD_TYPE:   return "RecordType";
        case AST_RANGE:         return "Range";
        case AST_ENUMERATED:    return "Enumerated";
        case AST_FIELD_PART:    return "FieldPart";
        case AST_COMPOUND:      return "Compound";
        case AST_ASSIGN:        return "Assign";
        case AST_IF:            return "If";
        case AST_WHILE:         return "While";
        case AST_FOR:           return "For";
        case AST_REPEAT:        return "Repeat";
        case AST_CASE:          return "Case";
        case AST_CASE_BLOCK:    return "CaseBlock";
        case AST_PROC_CALL:     return "ProcCall";
        case AST_EMPTY_STMT:    return "EmptyStmt";
        case AST_BINOP:         return "BinOp";
        case AST_UNOP:          return "UnOp";
        case AST_VAR:           return "Var";
        case AST_ARRAY_ACCESS:  return "ArrayAccess";
        case AST_FIELD_ACCESS:  return "FieldAccess";
        case AST_FUNC_CALL:     return "FuncCall";
        case AST_INT_LIT:       return "IntLit";
        case AST_REAL_LIT:      return "RealLit";
        case AST_CHAR_LIT:      return "CharLit";
        case AST_STRING_LIT:    return "StringLit";
        case AST_BOOL_LIT:      return "BoolLit";
        case AST_IDENT_LIST:    return "IdentList";
        case AST_PARAM_LIST:    return "ParamList";
        case AST_DECL_PART:     return "DeclPart";
        case AST_STMT_LIST:     return "StmtList";
        default:                return "Unknown";
    }
}

static void printNodeLine(const AstNode *node) {
    printf("%s", astTypeName(node->type));

    if (node->sval != NULL) {
        printf("('%s')", node->sval);
    } else if (node->type == AST_INT_LIT) {
        printf("(%lld)", node->ival);
    } else if (node->type == AST_REAL_LIT) {
        printf("(%g)", node->rval);
    }

    /* Anotasi semantik */
    if (node->typeIdx >= 0 || node->tabIdx >= 0 || node->lexLevel >= 0) {
        printf(" [");
        int first = 1;
        if (node->typeIdx >= 0) {
            printf("type:%d", node->typeIdx);
            first = 0;
        }
        if (node->tabIdx >= 0) {
            if (!first) printf(", ");
            printf("tab:%d", node->tabIdx);
            first = 0;
        }
        if (node->lexLevel >= 0) {
            if (!first) printf(", ");
            printf("lev:%d", node->lexLevel);
        }
        printf("]");
    }

    printf("\n");
}

void astPrint(const AstNode *node, int depth, int isLast, const char *prefix) {
    size_t i;
    char nextPrefix[512];
    const char *branch;
    const char *cont;

    if (node == NULL) return;

    if (depth == 0) {
        printNodeLine(node);
        prefix = "";
    } else {
        branch = isLast ? "`-- " : "|-- ";
        printf("%s%s", prefix, branch);
        printNodeLine(node);
    }

    cont = isLast ? "    " : "|   ";
    if ((size_t)(snprintf(nextPrefix, sizeof(nextPrefix), "%s%s", prefix, cont)) >= sizeof(nextPrefix)) {
        return;
    }

    for (i = 0; i < node->childCount; i++) {
        int last = (i + 1 == node->childCount);
        astPrint(node->children[i], depth + 1, last, nextPrefix);
    }
}
