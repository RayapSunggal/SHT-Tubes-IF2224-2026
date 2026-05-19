#include "ast_decorator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "symbol_table.h"

static char *g_message = NULL;
static size_t g_messageSize = 0;
static bool g_hasError = false;

static void semError(const char *format, ...) {
    if (g_hasError) {
        return;
    }
    g_hasError = true;

    if (g_message == NULL || g_messageSize == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(g_message, g_messageSize, format, args);
    va_end(args);
}

static BaseType baseTypeFromName(const char *name) {
    if (name == NULL) {
        return TYPE_NONE;
    }
    if (strcasecmp(name, "integer") == 0) return TYPE_INTEGER;
    if (strcasecmp(name, "real") == 0)    return TYPE_REAL;
    if (strcasecmp(name, "boolean") == 0) return TYPE_BOOLEAN;
    if (strcasecmp(name, "char") == 0)    return TYPE_CHAR;
    if (strcasecmp(name, "string") == 0)  return TYPE_STRING;
    return TYPE_NONE;
}

static bool typesCompatible(BaseType a, BaseType b) {
    if (a == TYPE_NONE || b == TYPE_NONE) {
        return false;
    }
    if (a == b) {
        return true;
    }
    if ((a == TYPE_INTEGER || a == TYPE_REAL) &&
        (b == TYPE_INTEGER || b == TYPE_REAL)) {
        return true;
    }
    return false;
}

static bool assignmentCompatible(BaseType t1, BaseType t2) {
    if (t1 == TYPE_NONE || t2 == TYPE_NONE) {
        return false;
    }
    if (t1 == t2) {
        return true;
    }
    if (t1 == TYPE_REAL && t2 == TYPE_INTEGER) {
        return true;
    }
    return false;
}

static void visit(AstNode *node);
static BaseType visitExpression(AstNode *node);
static void visitStatement(AstNode *node);
static void visitDeclPart(AstNode *node);
static void visitBlock(AstNode *node);

static BaseType resolveTypeNode(AstNode *typeNode, int *refOut) {
    if (refOut != NULL) {
        *refOut = -1;
    }
    if (typeNode == NULL) {
        return TYPE_NONE;
    }

    if (typeNode->type == AST_TYPE_IDENT) {
        BaseType bt = baseTypeFromName(typeNode->sval);
        if (bt != TYPE_NONE) {
            return bt;
        }
        int idx = symLookup(typeNode->sval);
        if (idx != -1 && tab[idx].obj == OBJ_TYPE) {
            typeNode->tabIdx = idx;
            if (refOut != NULL) {
                *refOut = tab[idx].ref;
            }
            return tab[idx].type;
        }
        semError("Unknown type '%s'.", typeNode->sval ? typeNode->sval : "?");
        return TYPE_NONE;
    }

    if (typeNode->type == AST_RANGE) {
        BaseType lo = TYPE_INTEGER;
        if (typeNode->childCount >= 1) {
            AstNode *lowN = typeNode->children[0];
            if (lowN->type == AST_CHAR_LIT) lo = TYPE_CHAR;
            else if (lowN->type == AST_REAL_LIT) lo = TYPE_REAL;
        }
        if (lo == TYPE_REAL) {
            semError("Subrange tidak boleh memiliki type Real.");
            return TYPE_NONE;
        }
        if (typeNode->childCount >= 2) {
            AstNode *a = typeNode->children[0];
            AstNode *b = typeNode->children[1];
            if (a->type == AST_INT_LIT && b->type == AST_INT_LIT &&
                a->ival > b->ival) {
                semError("Lower bound subrange (%lld) lebih besar dari upper bound (%lld).",
                         a->ival, b->ival);
                return TYPE_NONE;
            }
        }
        return lo;
    }

    if (typeNode->type == AST_ENUMERATED) {
        return TYPE_INTEGER;
    }

    if (typeNode->type == AST_ARRAY_TYPE) {
        BaseType indexType = TYPE_INTEGER;
        int low = 0, high = 0;
        BaseType elemType = TYPE_INTEGER;
        int elemRef = -1;
        AstNode *rangeN = NULL;
        AstNode *elemN = NULL;

        for (size_t i = 0; i < typeNode->childCount; i++) {
            AstNode *c = typeNode->children[i];
            if (c->type == AST_RANGE && rangeN == NULL) {
                rangeN = c;
            } else if ((c->type == AST_TYPE_IDENT || c->type == AST_ARRAY_TYPE ||
                        c->type == AST_RECORD_TYPE) && elemN == NULL) {
                elemN = c;
            }
        }

        if (rangeN != NULL && rangeN->childCount >= 2) {
            AstNode *a = rangeN->children[0];
            AstNode *b = rangeN->children[1];
            if (a->type == AST_INT_LIT) { low = (int)a->ival; indexType = TYPE_INTEGER; }
            else if (a->type == AST_CHAR_LIT) {
                low = a->sval ? (unsigned char)a->sval[0] : 0;
                indexType = TYPE_CHAR;
            }
            if (b->type == AST_INT_LIT)  high = (int)b->ival;
            else if (b->type == AST_CHAR_LIT) high = b->sval ? (unsigned char)b->sval[0] : 0;
            if (a->type == AST_REAL_LIT || b->type == AST_REAL_LIT) {
                semError("Index type array tidak boleh Real.");
                return TYPE_NONE;
            }
            if (low > high) {
                semError("Batas bawah index array (%d) melebihi batas atas (%d).", low, high);
                return TYPE_NONE;
            }
        }

        if (elemN != NULL) {
            elemType = resolveTypeNode(elemN, &elemRef);
            if (elemType == TYPE_NONE) {
                return TYPE_NONE;
            }
        }

        int elsz = sizeOfBaseType(elemType);
        if (elsz == 0) {
            elsz = 1;
        }
        int aref = symEnterArray(indexType, elemType, elemRef, low, high, elsz);
        if (refOut != NULL) {
            *refOut = aref;
        }
        return TYPE_ARRAY;
    }

    if (typeNode->type == AST_RECORD_TYPE) {
        int blockIdx = symEnterRecordBlock();
        int adr = 0;
        for (size_t i = 0; i < typeNode->childCount; i++) {
            AstNode *fp = typeNode->children[i];
            if (fp->type != AST_FIELD_PART || fp->childCount < 2) {
                continue;
            }
            AstNode *idList = fp->children[0];
            AstNode *fieldType = fp->children[1];
            int fieldRef = -1;
            BaseType ft = resolveTypeNode(fieldType, &fieldRef);
            if (ft == TYPE_NONE) {
                symExitRecordBlock();
                return TYPE_NONE;
            }
            for (size_t j = 0; j < idList->childCount; j++) {
                symEnterField(idList->children[j]->sval, ft, fieldRef, adr);
                adr += sizeOfBaseType(ft);
            }
        }
        symExitRecordBlock();
        if (refOut != NULL) {
            *refOut = blockIdx;
        }
        return TYPE_RECORD;
    }

    return TYPE_NONE;
}

static BaseType visitVarRef(AstNode *node) {
    int idx = symLookup(node->sval);
    if (idx == -1) {
        semError("Identifier '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;
    node->typeIdx = tab[idx].type;
    return tab[idx].type;
}

static BaseType visitArrayAccess(AstNode *node) {
    if (node->childCount < 1) {
        return TYPE_NONE;
    }
    AstNode *base = node->children[0];
    if (base->type != AST_VAR) {
        return TYPE_NONE;
    }
    int idx = symLookup(base->sval);
    if (idx == -1) {
        semError("Identifier '%s' belum dideklarasikan.",
                 base->sval ? base->sval : "?");
        return TYPE_NONE;
    }
    base->tabIdx = idx;
    base->lexLevel = tab[idx].lev;
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;

    if (tab[idx].type != TYPE_ARRAY) {
        semError("Identifier '%s' bukan array sehingga tidak dapat diindeks.",
                 base->sval ? base->sval : "?");
        return TYPE_NONE;
    }

    int aref = tab[idx].ref;
    for (size_t i = 1; i < node->childCount; i++) {
        BaseType it = visitExpression(node->children[i]);
        if (it == TYPE_NONE) {
            return TYPE_NONE;
        }
        if (aref >= 0 && it != atab[aref].xtyp &&
            !((it == TYPE_INTEGER) && (atab[aref].xtyp == TYPE_INTEGER))) {
            if (it != atab[aref].xtyp) {
                semError("Tipe index array '%s' tidak sesuai.",
                         base->sval ? base->sval : "?");
                return TYPE_NONE;
            }
        }
    }

    BaseType elem = (aref >= 0) ? atab[aref].etyp : TYPE_INTEGER;
    node->typeIdx = elem;
    return elem;
}

static BaseType visitFieldAccess(AstNode *node) {
    if (node->childCount < 2) {
        return TYPE_NONE;
    }
    AstNode *base = node->children[0];
    AstNode *field = node->children[1];

    if (base->type != AST_VAR) {
        semError("Akses field hanya didukung pada variabel record.");
        return TYPE_NONE;
    }
    int idx = symLookup(base->sval);
    if (idx == -1) {
        semError("Identifier '%s' belum dideklarasikan.",
                 base->sval ? base->sval : "?");
        return TYPE_NONE;
    }
    base->tabIdx = idx;
    base->lexLevel = tab[idx].lev;

    if (tab[idx].type != TYPE_RECORD || tab[idx].ref < 0) {
        semError("Identifier '%s' bukan record.", base->sval ? base->sval : "?");
        return TYPE_NONE;
    }

    int fi = btab[tab[idx].ref].last;
    while (fi != -1) {
        if (tab[fi].identifier != NULL && field->sval != NULL &&
            strcmp(tab[fi].identifier, field->sval) == 0) {
            field->tabIdx = fi;
            node->tabIdx = fi;
            node->typeIdx = tab[fi].type;
            return tab[fi].type;
        }
        fi = tab[fi].link;
    }

    semError("Field '%s' tidak ditemukan pada record '%s'.",
             field->sval ? field->sval : "?", base->sval ? base->sval : "?");
    return TYPE_NONE;
}

static BaseType visitCall(AstNode *node) {
    int idx = symLookup(node->sval);
    if (idx == -1) {
        semError("Procedure/function '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;

    if (tab[idx].obj != OBJ_PROCEDURE && tab[idx].obj != OBJ_FUNCTION) {
        semError("Identifier '%s' bukan procedure atau function.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *pl = node->children[i];
        if (pl->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < pl->childCount; j++) {
                if (visitExpression(pl->children[j]) == TYPE_NONE) {
                    return TYPE_NONE;
                }
            }
        }
    }

    node->typeIdx = tab[idx].type;
    return tab[idx].type;
}

static bool isArithmeticOp(const char *op) {
    return op != NULL && (strcmp(op, "plus") == 0 || strcmp(op, "minus") == 0 ||
                          strcmp(op, "times") == 0 || strcmp(op, "rdiv") == 0 ||
                          strcmp(op, "idiv") == 0 || strcmp(op, "imod") == 0);
}

static bool isRelationalOp(const char *op) {
    return op != NULL && (strcmp(op, "eql") == 0 || strcmp(op, "neq") == 0 ||
                          strcmp(op, "lss") == 0 || strcmp(op, "leq") == 0 ||
                          strcmp(op, "gtr") == 0 || strcmp(op, "geq") == 0);
}

static bool isBooleanOp(const char *op) {
    return op != NULL && (strcmp(op, "andsy") == 0 || strcmp(op, "orsy") == 0);
}

static BaseType visitBinOp(AstNode *node) {
    if (node->childCount < 2) {
        return TYPE_NONE;
    }
    BaseType lt = visitExpression(node->children[0]);
    BaseType rt = visitExpression(node->children[1]);
    if (lt == TYPE_NONE || rt == TYPE_NONE) {
        return TYPE_NONE;
    }

    const char *op = node->sval;

    if (isBooleanOp(op)) {
        if (lt != TYPE_BOOLEAN || rt != TYPE_BOOLEAN) {
            semError("Operator '%s' membutuhkan operand boolean.", op);
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (isRelationalOp(op)) {
        if (!typesCompatible(lt, rt)) {
            semError("Operand operator relasional '%s' bertipe tidak kompatibel.", op);
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (isArithmeticOp(op)) {
        if (!((lt == TYPE_INTEGER || lt == TYPE_REAL) &&
              (rt == TYPE_INTEGER || rt == TYPE_REAL))) {
            semError("Operator aritmatika '%s' membutuhkan operand numerik.", op);
            return TYPE_NONE;
        }
        if ((strcmp(op, "idiv") == 0 || strcmp(op, "imod") == 0) &&
            (lt != TYPE_INTEGER || rt != TYPE_INTEGER)) {
            semError("Operator '%s' membutuhkan operand integer.", op);
            return TYPE_NONE;
        }
        BaseType res;
        if (strcmp(op, "rdiv") == 0) {
            res = TYPE_REAL;
        } else if (lt == TYPE_REAL || rt == TYPE_REAL) {
            res = TYPE_REAL;
        } else {
            res = TYPE_INTEGER;
        }
        node->typeIdx = res;
        return res;
    }

    node->typeIdx = lt;
    return lt;
}

static BaseType visitUnOp(AstNode *node) {
    if (node->childCount < 1) {
        return TYPE_NONE;
    }
    BaseType t = visitExpression(node->children[0]);
    if (t == TYPE_NONE) {
        return TYPE_NONE;
    }

    if (node->sval != NULL && strcmp(node->sval, "not") == 0) {
        if (t != TYPE_BOOLEAN) {
            semError("Operator 'not' membutuhkan operand boolean.");
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (t != TYPE_INTEGER && t != TYPE_REAL) {
        semError("Operator unary '%s' membutuhkan operand numerik.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->typeIdx = t;
    return t;
}

static BaseType visitExpression(AstNode *node) {
    if (node == NULL) {
        return TYPE_NONE;
    }

    switch (node->type) {
        case AST_INT_LIT:
            node->typeIdx = TYPE_INTEGER;
            return TYPE_INTEGER;
        case AST_REAL_LIT:
            node->typeIdx = TYPE_REAL;
            return TYPE_REAL;
        case AST_CHAR_LIT:
            node->typeIdx = TYPE_CHAR;
            return TYPE_CHAR;
        case AST_STRING_LIT:
            node->typeIdx = TYPE_STRING;
            return TYPE_STRING;
        case AST_BOOL_LIT:
            node->typeIdx = TYPE_BOOLEAN;
            return TYPE_BOOLEAN;
        case AST_VAR:
            return visitVarRef(node);
        case AST_ARRAY_ACCESS:
            return visitArrayAccess(node);
        case AST_FIELD_ACCESS:
            return visitFieldAccess(node);
        case AST_FUNC_CALL:
            return visitCall(node);
        case AST_BINOP:
            return visitBinOp(node);
        case AST_UNOP:
            return visitUnOp(node);
        default:
            return TYPE_NONE;
    }
}

static BaseType lvalueType(AstNode *target) {
    if (target == NULL) {
        return TYPE_NONE;
    }
    if (target->type == AST_VAR) {
        return visitVarRef(target);
    }
    if (target->type == AST_ARRAY_ACCESS) {
        return visitArrayAccess(target);
    }
    if (target->type == AST_FIELD_ACCESS) {
        return visitFieldAccess(target);
    }
    return TYPE_NONE;
}

static void visitAssign(AstNode *node) {
    if (node->childCount < 2) {
        return;
    }
    AstNode *target = node->children[0];
    AstNode *expr = node->children[1];

    BaseType lt = lvalueType(target);
    if (lt == TYPE_NONE) {
        return;
    }
    BaseType rt = visitExpression(expr);
    if (rt == TYPE_NONE) {
        return;
    }

    if (!assignmentCompatible(lt, rt)) {
        semError("Type mismatch: tidak dapat assign %s ke %s.",
                 baseTypeToString(rt), baseTypeToString(lt));
        return;
    }
    node->typeIdx = TYPE_VOID;
    node->tabIdx = target->tabIdx;
    node->lexLevel = target->lexLevel;
}

static void visitIf(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType ct = visitExpression(node->children[0]);
    if (ct == TYPE_NONE) {
        return;
    }
    if (ct != TYPE_BOOLEAN) {
        semError("Kondisi if-statement harus bertipe boolean.");
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        visitStatement(node->children[i]);
    }
}

static void visitWhile(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType ct = visitExpression(node->children[0]);
    if (ct == TYPE_NONE) {
        return;
    }
    if (ct != TYPE_BOOLEAN) {
        semError("Kondisi while-statement harus bertipe boolean.");
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        visitStatement(node->children[i]);
    }
}

static void visitFor(AstNode *node) {
    int idx = symLookup(node->sval);
    if (idx == -1) {
        semError("Variabel counter '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;
    if (tab[idx].type != TYPE_INTEGER && tab[idx].type != TYPE_CHAR) {
        semError("Variabel counter '%s' harus bertipe integer/char.",
                 node->sval ? node->sval : "?");
        return;
    }

    size_t exprSeen = 0;
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            visitStatement(c);
        } else if (exprSeen < 2) {
            BaseType bt = visitExpression(c);
            if (bt != TYPE_NONE && bt != tab[idx].type &&
                !(bt == TYPE_INTEGER && tab[idx].type == TYPE_INTEGER)) {
                if (bt != tab[idx].type) {
                    semError("Batas for-loop tidak kompatibel dengan counter '%s'.",
                             node->sval ? node->sval : "?");
                    return;
                }
            }
            exprSeen++;
        }
    }
}

static void visitRepeat(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_STMT_LIST) {
            visitStatement(c);
        } else {
            BaseType ct = visitExpression(c);
            if (ct != TYPE_NONE && ct != TYPE_BOOLEAN) {
                semError("Kondisi repeat-until harus bertipe boolean.");
                return;
            }
        }
    }
}

static void visitCase(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType sel = visitExpression(node->children[0]);
    if (sel == TYPE_NONE) {
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        AstNode *cb = node->children[i];
        if (cb->type != AST_CASE_BLOCK) {
            continue;
        }
        for (size_t j = 0; j < cb->childCount; j++) {
            AstNode *c = cb->children[j];
            if (c->type == AST_INT_LIT || c->type == AST_CHAR_LIT ||
                c->type == AST_BOOL_LIT || c->type == AST_VAR) {
                (void)visitExpression(c);
            } else {
                visitStatement(c);
            }
        }
    }
}

static void visitStatement(AstNode *node) {
    if (node == NULL) {
        return;
    }
    switch (node->type) {
        case AST_COMPOUND:
        case AST_STMT_LIST:
            for (size_t i = 0; i < node->childCount; i++) {
                visitStatement(node->children[i]);
            }
            break;
        case AST_ASSIGN:
            visitAssign(node);
            break;
        case AST_IF:
            visitIf(node);
            break;
        case AST_WHILE:
            visitWhile(node);
            break;
        case AST_FOR:
            visitFor(node);
            break;
        case AST_REPEAT:
            visitRepeat(node);
            break;
        case AST_CASE:
            visitCase(node);
            break;
        case AST_PROC_CALL:
            (void)visitCall(node);
            break;
        case AST_EMPTY_STMT:
            break;
        default:
            break;
    }
}

static void visitConstDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    AstNode *valNode = node->children[0];
    BaseType bt = TYPE_NONE;
    switch (valNode->type) {
        case AST_INT_LIT:    bt = TYPE_INTEGER; break;
        case AST_REAL_LIT:   bt = TYPE_REAL;    break;
        case AST_CHAR_LIT:   bt = TYPE_CHAR;    break;
        case AST_STRING_LIT: bt = TYPE_STRING;  break;
        case AST_BOOL_LIT:   bt = TYPE_BOOLEAN; break;
        case AST_VAR: {
            int idx = symLookup(valNode->sval);
            if (idx != -1 && tab[idx].obj == OBJ_CONSTANT) {
                bt = tab[idx].type;
            }
            break;
        }
        default: break;
    }
    if (bt == TYPE_NONE) {
        semError("Tipe konstanta '%s' tidak diketahui.",
                 node->sval ? node->sval : "?");
        return;
    }
    int r = symEnter(node->sval, OBJ_CONSTANT, bt, -1, 1, 0);
    if (r == -1) {
        semError("Redeklarasi konstanta '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = r;
    node->typeIdx = bt;
    node->lexLevel = tab[r].lev;
}

static void visitTypeDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    int ref = -1;
    BaseType bt = resolveTypeNode(node->children[0], &ref);
    if (bt == TYPE_NONE) {
        if (!g_hasError) {
            semError("Tipe pada deklarasi type '%s' tidak diketahui.",
                     node->sval ? node->sval : "?");
        }
        return;
    }
    int r = symEnter(node->sval, OBJ_TYPE, bt, ref, 1, 0);
    if (r == -1) {
        semError("Redeklarasi type '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = r;
    node->typeIdx = bt;
    node->lexLevel = tab[r].lev;
}

static void visitVarDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    int ref = -1;
    BaseType bt = resolveTypeNode(node->children[0], &ref);
    if (bt == TYPE_NONE) {
        if (!g_hasError) {
            semError("Tipe variabel '%s' tidak diketahui.",
                     node->sval ? node->sval : "?");
        }
        return;
    }
    int r = symEnter(node->sval, OBJ_VARIABLE, bt, ref, 1,
                     btab[display[currentLevel]].vsze);
    if (r == -1) {
        semError("Redeklarasi identifier '%s' pada scope yang sama.",
                 node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = r;
    node->typeIdx = bt;
    node->lexLevel = tab[r].lev;
}

static void visitParamGroup(AstNode *node, int funcTabIdx) {
    if (node->childCount < 2) {
        return;
    }
    AstNode *idList = node->children[0];
    AstNode *typeN = node->children[1];
    int ref = -1;
    BaseType bt = resolveTypeNode(typeN, &ref);
    if (bt == TYPE_NONE) {
        semError("Tipe parameter tidak diketahui.");
        return;
    }
    for (size_t i = 0; i < idList->childCount; i++) {
        int r = symEnter(idList->children[i]->sval, OBJ_VARIABLE, bt, ref, 0,
                         btab[display[currentLevel]].psze);
        if (r == -1) {
            semError("Redeklarasi parameter '%s'.",
                     idList->children[i]->sval);
            return;
        }
        idList->children[i]->tabIdx = r;
        idList->children[i]->typeIdx = bt;
        idList->children[i]->lexLevel = tab[r].lev;
        if (funcTabIdx >= 0) {
            tab[funcTabIdx].nrm += 1;
        }
    }
}

static void visitProcDecl(AstNode *node) {
    int procIdx = symEnter(node->sval, OBJ_PROCEDURE, TYPE_VOID, -1, 0, 0);
    if (procIdx == -1) {
        semError("Redeklarasi procedure '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = procIdx;
    node->lexLevel = tab[procIdx].lev;

    symEnterScope();
    tab[procIdx].ref = display[currentLevel];
    tab[procIdx].nrm = 0;

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < c->childCount; j++) {
                if (c->children[j]->type == AST_PARAM_GROUP) {
                    visitParamGroup(c->children[j], procIdx);
                }
            }
        } else if (c->type == AST_BLOCK) {
            visitBlock(c);
        }
    }

    symExitScope();
}

static void visitFuncDecl(AstNode *node) {
    BaseType retType = TYPE_NONE;
    AstNode *retNode = NULL;
    if (node->childCount >= 1 && node->children[0]->type == AST_TYPE_IDENT) {
        retNode = node->children[0];
        retType = baseTypeFromName(retNode->sval);
        if (retType == TYPE_NONE) {
            int ti = symLookup(retNode->sval);
            if (ti != -1 && tab[ti].obj == OBJ_TYPE) {
                retType = tab[ti].type;
            }
        }
    }

    int funcIdx = symEnter(node->sval, OBJ_FUNCTION, retType, -1, 0, 0);
    if (funcIdx == -1) {
        semError("Redeklarasi function '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = funcIdx;
    node->typeIdx = retType;
    node->lexLevel = tab[funcIdx].lev;
    if (retNode != NULL) {
        retNode->typeIdx = retType;
    }

    symEnterScope();
    tab[funcIdx].ref = display[currentLevel];
    tab[funcIdx].nrm = 0;

    symEnter(node->sval, OBJ_VARIABLE, retType, -1, 1, 0);

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < c->childCount; j++) {
                if (c->children[j]->type == AST_PARAM_GROUP) {
                    visitParamGroup(c->children[j], funcIdx);
                }
            }
        } else if (c->type == AST_BLOCK) {
            visitBlock(c);
        }
    }

    symExitScope();
}

static void visitDeclPart(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        switch (c->type) {
            case AST_DECL_PART:
                visitDeclPart(c);
                break;
            case AST_CONST_DECL:
                visitConstDecl(c);
                break;
            case AST_TYPE_DECL:
                visitTypeDecl(c);
                break;
            case AST_VAR_DECL:
                visitVarDecl(c);
                break;
            case AST_PROC_DECL:
                visitProcDecl(c);
                break;
            case AST_FUNC_DECL:
                visitFuncDecl(c);
                break;
            default:
                break;
        }
        if (g_hasError) {
            return;
        }
    }
}

static void visitBlock(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_DECL_PART) {
            visitDeclPart(c);
        }
        if (g_hasError) {
            return;
        }
    }
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            visitStatement(c);
        }
        if (g_hasError) {
            return;
        }
    }
}

static void visit(AstNode *node) {
    if (node == NULL) {
        semError("AST kosong.");
        return;
    }
    if (node->type != AST_PROGRAM) {
        semError("Root AST harus berupa Program.");
        return;
    }

    int pIdx = symEnter(node->sval, OBJ_PROGRAM, TYPE_VOID, -1, 1, 0);
    if (pIdx == -1) {
        semError("Redeklarasi identifier program '%s'.",
                 node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = pIdx;
    node->lexLevel = 0;

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_DECL_PART) {
            visitDeclPart(c);
        }
        if (g_hasError) {
            return;
        }
    }
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            visitStatement(c);
        }
        if (g_hasError) {
            return;
        }
    }
}

bool decorateAst(AstNode *root, char *message, size_t messageSize) {
    g_message = message;
    g_messageSize = messageSize;
    g_hasError = false;
    if (message != NULL && messageSize > 0) {
        message[0] = '\0';
    }

    visit(root);

    return !g_hasError;
}

static const char *decoratedTypeName(AstNodeType type) {
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

static const char *shortTypeName(int typeCode) {
    switch ((BaseType)typeCode) {
        case TYPE_INTEGER: return "integer";
        case TYPE_REAL:    return "real";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_CHAR:    return "char";
        case TYPE_STRING:  return "string";
        case TYPE_ARRAY:   return "array";
        case TYPE_RECORD:  return "record";
        case TYPE_VOID:    return "void";
        default:           return "none";
    }
}

static void printDecoratedNode(const AstNode *node, FILE *stream) {
    fprintf(stream, "%s", decoratedTypeName(node->type));

    if (node->sval != NULL) {
        fprintf(stream, "('%s')", node->sval);
    } else if (node->type == AST_INT_LIT) {
        fprintf(stream, "(%lld)", node->ival);
    } else if (node->type == AST_REAL_LIT) {
        fprintf(stream, "(%g)", node->rval);
    }

    if (node->typeIdx >= 0 || node->tabIdx >= 0 || node->lexLevel >= 0) {
        int first = 1;
        fprintf(stream, "  -> ");
        if (node->typeIdx >= 0) {
            fprintf(stream, "type:%s", shortTypeName(node->typeIdx));
            first = 0;
        }
        if (node->tabIdx >= 0) {
            if (!first) fprintf(stream, ", ");
            fprintf(stream, "tab_index:%d", node->tabIdx);
            first = 0;
        }
        if (node->lexLevel >= 0) {
            if (!first) fprintf(stream, ", ");
            fprintf(stream, "lev:%d", node->lexLevel);
        }
    }

    fprintf(stream, "\n");
}

static void printDecoratedAstRec(const AstNode *node, FILE *stream,
                                 int depth, int isLast, const char *prefix) {
    char nextPrefix[1024];

    if (node == NULL) {
        return;
    }

    if (depth == 0) {
        printDecoratedNode(node, stream);
        prefix = "";
    } else {
        fprintf(stream, "%s%s", prefix, isLast ? "`-- " : "|-- ");
        printDecoratedNode(node, stream);
    }

    if ((size_t)snprintf(nextPrefix, sizeof(nextPrefix), "%s%s",
                         prefix, isLast ? "    " : "|   ") >= sizeof(nextPrefix)) {
        return;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        printDecoratedAstRec(node->children[i], stream, depth + 1,
                             (i + 1 == node->childCount), nextPrefix);
    }
}

void printDecoratedAst(const AstNode *node, FILE *stream) {
    if (node == NULL || stream == NULL) {
        return;
    }
    printDecoratedAstRec(node, stream, 0, 1, "");
}
