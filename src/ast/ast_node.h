#ifndef AST_NODE_H
#define AST_NODE_H

#include <stddef.h>

typedef enum {
    /* Program structure */
    AST_PROGRAM,            /* program Name; decls; compound */
    AST_BLOCK,              /* declaration-part + compound-statement */

    /* Declarations */
    AST_CONST_DECL,         /* const Name == value */
    AST_TYPE_DECL,          /* type Name = <type> */
    AST_VAR_DECL,           /* var name : type */
    AST_PARAM_GROUP,        /* parameter group: name : type */
    AST_PROC_DECL,          /* procedure Name(params); block */
    AST_FUNC_DECL,          /* function Name(params): rettype; block */

    /* Type nodes */
    AST_TYPE_IDENT,         /* named type: Integer, Boolean, dsb */
    AST_ARRAY_TYPE,         /* array [range] of type */
    AST_RECORD_TYPE,        /* record field-list end */
    AST_RANGE,              /* low .. high */
    AST_ENUMERATED,         /* (id1, id2, ...) */
    AST_FIELD_PART,         /* field-list entry: ids : type */

    /* Statements */
    AST_COMPOUND,           /* begin statement-list end */
    AST_ASSIGN,             /* var := expr */
    AST_IF,                 /* if expr then stmt [else stmt] */
    AST_WHILE,              /* while expr do compound */
    AST_FOR,                /* for id := expr (to|downto) expr do compound */
    AST_REPEAT,             /* repeat stmts until expr */
    AST_CASE,               /* case expr of case-blocks end */
    AST_CASE_BLOCK,         /* const-list : statement */
    AST_PROC_CALL,          /* procedure/function call */
    AST_EMPTY_STMT,         /* empty statement */

    /* Expressions */
    AST_BINOP,              /* left op right */
    AST_UNOP,               /* op operand  (unary minus, not) */
    AST_VAR,                /* variable reference */
    AST_ARRAY_ACCESS,       /* arr[index] */
    AST_FIELD_ACCESS,       /* record.field */
    AST_FUNC_CALL,          /* function call in expression */
    AST_INT_LIT,            /* integer literal */
    AST_REAL_LIT,           /* real literal */
    AST_CHAR_LIT,           /* char literal */
    AST_STRING_LIT,         /* string literal */
    AST_BOOL_LIT,           /* true / false */

    /* Helpers */
    AST_IDENT_LIST,         /* a, b, c */
    AST_PARAM_LIST,         /* actual parameter list */
    AST_DECL_PART,          /* declaration part wrapper */
    AST_STMT_LIST           /* statement list wrapper */
} AstNodeType;

typedef struct AstNode {
    AstNodeType type;

    /* nama identifier */
    char *sval;             /* string/ident/operator value */
    long long ival;         /* integer literal */
    double rval;            /* real literal */

    /* Children */
    struct AstNode **children;
    size_t childCount;
    size_t childCapacity;

    /* Anotasi semantik (diisi oleh visitor saat dekorasi) */
    int typeIdx;            /* indeks tipe di tab (-1 = belum diisi) */
    int tabIdx;             /* indeks entri di tab (-1 = belum diisi) */
    int lexLevel;           /* lexical level saat node ini berada */
} AstNode;

AstNode *astCreateNode(AstNodeType type);
AstNode *astCreateSval(AstNodeType type, const char *sval);
AstNode *astCreateIval(AstNodeType type, long long ival);
AstNode *astCreateRval(AstNodeType type, double rval);
int astAddChild(AstNode *parent, AstNode *child);
void astFree(AstNode *node);
void astPrint(const AstNode *node, int depth, int isLast, const char *prefix);

#endif
