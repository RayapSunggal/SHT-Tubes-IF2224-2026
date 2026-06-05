#include "ast_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

static int labelEq(const ParseTreeNode *n, const char *lbl) {
    return n != NULL && strcmp(n->label, lbl) == 0;
}

static int labelPrefix(const ParseTreeNode *n, const char *prefix) {
    return n != NULL && strncmp(n->label, prefix, strlen(prefix)) == 0;
}

static const char *extractLexeme(const char *label, char *buf, size_t bufSize) {
    const char *lp = strchr(label, '(');
    const char *rp = strrchr(label, ')');
    size_t len;

    if (lp == NULL || rp == NULL || rp <= lp + 1) return NULL;
    lp++;
    len = (size_t)(rp - lp);
    if (len >= bufSize) len = bufSize - 1;
    memcpy(buf, lp, len);
    buf[len] = '\0';
    return buf;
}

static AstNode *cloneAstNode(const AstNode *node) {
    AstNode *copy;
    size_t i;

    if (node == NULL) return NULL;

    if (node->sval != NULL) {
        copy = astCreateSval(node->type, node->sval);
    } else {
        copy = astCreateNode(node->type);
    }

    if (copy == NULL) return NULL;

    copy->ival = node->ival;
    copy->rval = node->rval;

    for (i = 0; i < node->childCount; i++) {
        AstNode *childCopy = cloneAstNode(node->children[i]);
        if (childCopy == NULL || !astAddChild(copy, childCopy)) {
            astFree(childCopy);
            astFree(copy);
            return NULL;
        }
    }

    return copy;
}

static AstNode *buildProgram(const ParseTreeNode *n);
static AstNode *buildProgramHeader(const ParseTreeNode *n);
static AstNode *buildDeclarationPart(const ParseTreeNode *n);
static AstNode *buildConstDeclaration(const ParseTreeNode *n);
static AstNode *buildTypeDeclaration(const ParseTreeNode *n);
static AstNode *buildVarDeclaration(const ParseTreeNode *n);
static AstNode *buildIdentifierList(const ParseTreeNode *n);
static AstNode *buildType(const ParseTreeNode *n);
static AstNode *buildArrayType(const ParseTreeNode *n);
static AstNode *buildRange(const ParseTreeNode *n);
static AstNode *buildEnumerated(const ParseTreeNode *n);
static AstNode *buildRecordType(const ParseTreeNode *n);
static AstNode *buildFieldPart(const ParseTreeNode *n);
static AstNode *buildSubprogramDeclaration(const ParseTreeNode *n);
static AstNode *buildProcedureDeclaration(const ParseTreeNode *n);
static AstNode *buildFunctionDeclaration(const ParseTreeNode *n);
static AstNode *buildBlock(const ParseTreeNode *n);
static AstNode *buildFormalParameterList(const ParseTreeNode *n);
static AstNode *buildParameterGroup(const ParseTreeNode *n);
static AstNode *buildCompoundStatement(const ParseTreeNode *n);
static AstNode *buildStatementList(const ParseTreeNode *n);
static AstNode *buildStatement(const ParseTreeNode *n);
static AstNode *buildAssignmentStatement(const ParseTreeNode *n);
static AstNode *buildIfStatement(const ParseTreeNode *n);
static AstNode *buildWhileStatement(const ParseTreeNode *n);
static AstNode *buildForStatement(const ParseTreeNode *n);
static AstNode *buildRepeatStatement(const ParseTreeNode *n);
static AstNode *buildCaseStatement(const ParseTreeNode *n);
static AstNode *buildCaseBlock(const ParseTreeNode *n);
static AstNode *buildProcFuncCall(const ParseTreeNode *n, AstNodeType callType);
static AstNode *buildParameterList(const ParseTreeNode *n);
static AstNode *buildExpression(const ParseTreeNode *n);
static AstNode *buildSimpleExpression(const ParseTreeNode *n);
static AstNode *buildTerm(const ParseTreeNode *n);
static AstNode *buildFactor(const ParseTreeNode *n);
static AstNode *buildVariable(const ParseTreeNode *n);
static AstNode *buildConstant(const ParseTreeNode *n);
static AstNode *dispatchNode(const ParseTreeNode *n);


static AstNode *dispatchNode(const ParseTreeNode *n) {
    if (n == NULL) return NULL;

    if (labelEq(n, "<program>"))                return buildProgram(n);
    if (labelEq(n, "<program-header>"))         return buildProgramHeader(n);
    if (labelEq(n, "<declaration-part>"))       return buildDeclarationPart(n);
    if (labelEq(n, "<const-declaration>"))      return buildConstDeclaration(n);
    if (labelEq(n, "<type-declaration>"))       return buildTypeDeclaration(n);
    if (labelEq(n, "<var-declaration>"))        return buildVarDeclaration(n);
    if (labelEq(n, "<identifier-list>"))        return buildIdentifierList(n);
    if (labelEq(n, "<type>"))                   return buildType(n);
    if (labelEq(n, "<array-type>"))             return buildArrayType(n);
    if (labelEq(n, "<range>"))                  return buildRange(n);
    if (labelEq(n, "<enumerated>"))             return buildEnumerated(n);
    if (labelEq(n, "<record-type>"))            return buildRecordType(n);
    if (labelEq(n, "<field-part>"))             return buildFieldPart(n);
    if (labelEq(n, "<subprogram-declaration>")) return buildSubprogramDeclaration(n);
    if (labelEq(n, "<procedure-declaration>"))  return buildProcedureDeclaration(n);
    if (labelEq(n, "<function-declaration>"))   return buildFunctionDeclaration(n);
    if (labelEq(n, "block"))                    return buildBlock(n);
    if (labelEq(n, "<formal-parameter-list>"))  return buildFormalParameterList(n);
    if (labelEq(n, "<parameter-group>"))        return buildParameterGroup(n);
    if (labelEq(n, "<compound-statement>"))     return buildCompoundStatement(n);
    if (labelEq(n, "<statement-list>"))         return buildStatementList(n);
    if (labelEq(n, "<statement>"))              return buildStatement(n);
    if (labelEq(n, "<assignment-statement>"))   return buildAssignmentStatement(n);
    if (labelEq(n, "<if-statement>"))           return buildIfStatement(n);
    if (labelEq(n, "<while-statement>"))        return buildWhileStatement(n);
    if (labelEq(n, "<for-statement>"))          return buildForStatement(n);
    if (labelEq(n, "<repeat-statement>"))       return buildRepeatStatement(n);
    if (labelEq(n, "<case-statement>"))         return buildCaseStatement(n);
    if (labelEq(n, "<case-block>"))             return buildCaseBlock(n);
    if (labelEq(n, "<procedure/function-call>")) return buildProcFuncCall(n, AST_PROC_CALL);
    if (labelEq(n, "<parameter-list>"))         return buildParameterList(n);
    if (labelEq(n, "<expression>"))             return buildExpression(n);
    if (labelEq(n, "<simple-expression>"))      return buildSimpleExpression(n);
    if (labelEq(n, "<term>"))                   return buildTerm(n);
    if (labelEq(n, "<factor>"))                 return buildFactor(n);
    if (labelEq(n, "<variable>"))               return buildVariable(n);
    if (labelEq(n, "<constant>"))               return buildConstant(n);

    if (labelEq(n, "<field-list>")) {
        AstNode *list = astCreateNode(AST_DECL_PART);
        size_t i;
        if (list == NULL) return NULL;
        for (i = 0; i < n->childCount; i++) {
            if (labelEq(n->children[i], "<field-part>")) {
                AstNode *fp = buildFieldPart(n->children[i]);
                if (fp == NULL || !astAddChild(list, fp)) { astFree(list); return NULL; }
            }
        }
        return list;
    }

    return NULL;
}


static AstNode *buildProgram(const ParseTreeNode *n) {
    AstNode *prog;
    AstNode *header;
    AstNode *decls;
    AstNode *compound;
    size_t i;

    if (n->childCount < 3) return NULL;

    header = NULL; decls = NULL; compound = NULL;
    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<program-header>") && header == NULL)
            header = buildProgramHeader(c);
        else if (labelEq(c, "<declaration-part>") && decls == NULL)
            decls = buildDeclarationPart(c);
        else if (labelEq(c, "<compound-statement>") && compound == NULL)
            compound = buildCompoundStatement(c);
    }

    if (header == NULL || decls == NULL || compound == NULL) {
        astFree(header); astFree(decls); astFree(compound);
        return NULL;
    }

    prog = astCreateSval(AST_PROGRAM, header->sval);
    if (prog == NULL) { astFree(header); astFree(decls); astFree(compound); return NULL; }

    astFree(header);

    if (!astAddChild(prog, decls) || !astAddChild(prog, compound)) {
        astFree(prog);
        return NULL;
    }

    return prog;
}

static AstNode *buildProgramHeader(const ParseTreeNode *n) {
    char buf[256];
    size_t i;

    for (i = 0; i < n->childCount; i++) {
        if (labelPrefix(n->children[i], "IDENT(") || labelPrefix(n->children[i], "ident(")) {
            extractLexeme(n->children[i]->label, buf, sizeof(buf));
            return astCreateSval(AST_PROGRAM, buf);
        }
    }
    return astCreateSval(AST_PROGRAM, "?");
}


static AstNode *buildDeclarationPart(const ParseTreeNode *n) {
    AstNode *part = astCreateNode(AST_DECL_PART);
    size_t i;

    if (part == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        AstNode *child = dispatchNode(n->children[i]);
        if (child != NULL && !astAddChild(part, child)) {
            astFree(part);
            return NULL;
        }
    }

    return part;
}

static AstNode *buildConstDeclaration(const ParseTreeNode *n) {
    AstNode *constPart = astCreateNode(AST_DECL_PART);
    size_t i = 1;

    if (constPart == NULL) return NULL;

    while (i + 2 < n->childCount) {
        char buf[256];
        AstNode *cd;
        AstNode *val;

        if (!labelPrefix(n->children[i], "IDENT(") && !labelPrefix(n->children[i], "ident(")) {
            i++;
            continue;
        }

        extractLexeme(n->children[i]->label, buf, sizeof(buf));
        cd = astCreateSval(AST_CONST_DECL, buf);
        if (cd == NULL) { astFree(constPart); return NULL; }

        if (i + 2 < n->childCount && labelEq(n->children[i+2], "<constant>")) {
            val = buildConstant(n->children[i+2]);
            if (val == NULL || !astAddChild(cd, val)) { astFree(cd); astFree(constPart); return NULL; }
        }

        if (!astAddChild(constPart, cd)) { astFree(constPart); return NULL; }
        i += 4;
    }
    return constPart;
}

static AstNode *buildTypeDeclaration(const ParseTreeNode *n) {
    AstNode *typePart = astCreateNode(AST_DECL_PART);
    size_t i = 1;

    if (typePart == NULL) return NULL;

    while (i + 2 < n->childCount) {
        char buf[256];
        AstNode *td;
        AstNode *typeNode;

        if (!labelPrefix(n->children[i], "IDENT(") && !labelPrefix(n->children[i], "ident(")) {
            i++;
            continue;
        }

        extractLexeme(n->children[i]->label, buf, sizeof(buf));
        td = astCreateSval(AST_TYPE_DECL, buf);
        if (td == NULL) { astFree(typePart); return NULL; }

        if (i + 2 < n->childCount && labelEq(n->children[i+2], "<type>")) {
            typeNode = buildType(n->children[i+2]);
            if (typeNode == NULL || !astAddChild(td, typeNode)) { astFree(td); astFree(typePart); return NULL; }
        }

        if (!astAddChild(typePart, td)) { astFree(typePart); return NULL; }
        i += 4;
    }

    return typePart;
}

static AstNode *buildVarDeclaration(const ParseTreeNode *n) {
    AstNode *varPart = astCreateNode(AST_DECL_PART);
    size_t i = 1;

    if (varPart == NULL) return NULL;

    while (i < n->childCount) {
        AstNode *idList;
        AstNode *typeNode;
        AstNode *vd;
        size_t j;

        if (!labelEq(n->children[i], "<identifier-list>")) { i++; continue; }

        idList = buildIdentifierList(n->children[i]);
        if (idList == NULL) { astFree(varPart); return NULL; }

        typeNode = NULL;
        if (i + 2 < n->childCount && labelEq(n->children[i+2], "<type>")) {
            typeNode = buildType(n->children[i+2]);
        }

        for (j = 0; j < idList->childCount; j++) {
            const char *name = idList->children[j]->sval;
            vd = astCreateSval(AST_VAR_DECL, name);
            if (vd == NULL) { astFree(idList); astFree(typeNode); astFree(varPart); return NULL; }

            if (typeNode != NULL) {
                AstNode *typeCopy = cloneAstNode(typeNode);
                if (typeCopy == NULL) { astFree(vd); astFree(idList); astFree(typeNode); astFree(varPart); return NULL; }
                if (!astAddChild(vd, typeCopy)) { astFree(typeCopy); astFree(vd); astFree(idList); astFree(typeNode); astFree(varPart); return NULL; }
            }

            if (!astAddChild(varPart, vd)) { astFree(idList); astFree(varPart); return NULL; }
        }

        astFree(typeNode);
        astFree(idList);
        i += 4;
    }

    return varPart;
}

static AstNode *buildIdentifierList(const ParseTreeNode *n) {
    AstNode *list = astCreateNode(AST_IDENT_LIST);
    size_t i;
    char buf[256];

    if (list == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) {
            AstNode *id;
            extractLexeme(c->label, buf, sizeof(buf));
            id = astCreateSval(AST_VAR, buf);
            if (id == NULL || !astAddChild(list, id)) { astFree(list); return NULL; }
        }
    }

    return list;
}


static AstNode *buildType(const ParseTreeNode *n) {
    char buf[256];

    if (n->childCount == 1) {
        const ParseTreeNode *c = n->children[0];
        if (labelEq(c, "<array-type>"))   return buildArrayType(c);
        if (labelEq(c, "<record-type>"))  return buildRecordType(c);
        if (labelEq(c, "<enumerated>"))   return buildEnumerated(c);
        if (labelEq(c, "<range>"))        return buildRange(c);
    }

    if (n->childCount == 1 &&
        (labelPrefix(n->children[0], "IDENT(") || labelPrefix(n->children[0], "ident("))) {
        extractLexeme(n->children[0]->label, buf, sizeof(buf));
        return astCreateSval(AST_TYPE_IDENT, buf);
    }

    if (n->childCount == 0) {
        if (strncmp(n->label, "IDENT(", 6) == 0 || strncmp(n->label, "ident(", 6) == 0) {
            extractLexeme(n->label, buf, sizeof(buf));
            return astCreateSval(AST_TYPE_IDENT, buf);
        }
    }

    {
        size_t i;
        for (i = 0; i < n->childCount; i++) {
            if (labelPrefix(n->children[i], "IDENT(") || labelPrefix(n->children[i], "ident(")) {
                extractLexeme(n->children[i]->label, buf, sizeof(buf));
                return astCreateSval(AST_TYPE_IDENT, buf);
            }
        }
    }

    return astCreateSval(AST_TYPE_IDENT, "unknown");
}

static AstNode *buildArrayType(const ParseTreeNode *n) {
    AstNode *arr = astCreateNode(AST_ARRAY_TYPE);
    size_t i;

    if (arr == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<range>")) {
            AstNode *range = buildRange(c);
            if (range == NULL || !astAddChild(arr, range)) { astFree(arr); return NULL; }
        } else if (labelEq(c, "<type>")) {
            AstNode *elemType = buildType(c);
            if (elemType == NULL || !astAddChild(arr, elemType)) { astFree(arr); return NULL; }
        } else if (labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) {
            char buf[256];
            AstNode *idx;
            extractLexeme(c->label, buf, sizeof(buf));
            idx = astCreateSval(AST_TYPE_IDENT, buf);
            if (idx == NULL || !astAddChild(arr, idx)) { astFree(arr); return NULL; }
        }
    }

    return arr;
}

static AstNode *buildRange(const ParseTreeNode *n) {
    AstNode *range = astCreateNode(AST_RANGE);
    AstNode *low = NULL;
    AstNode *high = NULL;
    size_t i;

    if (range == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<constant>")) {
            AstNode *c = buildConstant(n->children[i]);
            if (c == NULL) { astFree(range); return NULL; }
            if (low == NULL) low = c;
            else high = c;
        }
    }

    if (low == NULL || high == NULL ||
        !astAddChild(range, low) || !astAddChild(range, high)) {
        astFree(range);
        return NULL;
    }

    return range;
}

static AstNode *buildEnumerated(const ParseTreeNode *n) {
    AstNode *en = astCreateNode(AST_ENUMERATED);
    size_t i;
    char buf[256];

    if (en == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) {
            AstNode *id;
            extractLexeme(c->label, buf, sizeof(buf));
            id = astCreateSval(AST_VAR, buf);
            if (id == NULL || !astAddChild(en, id)) { astFree(en); return NULL; }
        }
    }

    return en;
}

static AstNode *buildRecordType(const ParseTreeNode *n) {
    AstNode *rec = astCreateNode(AST_RECORD_TYPE);
    size_t i;

    if (rec == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<field-list>")) {
            size_t j;
            const ParseTreeNode *fl = n->children[i];
            for (j = 0; j < fl->childCount; j++) {
                if (labelEq(fl->children[j], "<field-part>")) {
                    AstNode *fp = buildFieldPart(fl->children[j]);
                    if (fp == NULL || !astAddChild(rec, fp)) { astFree(rec); return NULL; }
                }
            }
        }
    }

    return rec;
}

static AstNode *buildFieldPart(const ParseTreeNode *n) {
    AstNode *fp = astCreateNode(AST_FIELD_PART);
    AstNode *idList = NULL;
    AstNode *typeNode = NULL;
    size_t i;

    if (fp == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<identifier-list>") && idList == NULL)
            idList = buildIdentifierList(n->children[i]);
        else if (labelEq(n->children[i], "<type>") && typeNode == NULL)
            typeNode = buildType(n->children[i]);
    }

    if (idList == NULL || typeNode == NULL ||
        !astAddChild(fp, idList) || !astAddChild(fp, typeNode)) {
        astFree(fp);
        return NULL;
    }

    return fp;
}


static AstNode *buildSubprogramDeclaration(const ParseTreeNode *n) {
    if (n->childCount == 1) return dispatchNode(n->children[0]);
    return NULL;
}

static AstNode *buildProcedureDeclaration(const ParseTreeNode *n) {
    AstNode *proc;
    char buf[256];
    size_t i;

    buf[0] = '\0';
    for (i = 0; i < n->childCount; i++) {
        if (labelPrefix(n->children[i], "IDENT(") || labelPrefix(n->children[i], "ident(")) {
            extractLexeme(n->children[i]->label, buf, sizeof(buf));
            break;
        }
    }

    proc = astCreateSval(AST_PROC_DECL, buf);
    if (proc == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<formal-parameter-list>")) {
            AstNode *params = buildFormalParameterList(c);
            if (params == NULL || !astAddChild(proc, params)) { astFree(proc); return NULL; }
        } else if (labelEq(c, "block")) {
            AstNode *block = buildBlock(c);
            if (block == NULL || !astAddChild(proc, block)) { astFree(proc); return NULL; }
        }
    }

    return proc;
}

static AstNode *buildFunctionDeclaration(const ParseTreeNode *n) {
    AstNode *func;
    char nameBuf[256];
    char retBuf[256];
    size_t i;
    int identCount = 0;

    nameBuf[0] = '\0';
    retBuf[0] = '\0';

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) {
            identCount++;
            if (identCount == 1) extractLexeme(c->label, nameBuf, sizeof(nameBuf));
            else extractLexeme(c->label, retBuf, sizeof(retBuf));
        }
    }

    func = astCreateSval(AST_FUNC_DECL, nameBuf);
    if (func == NULL) return NULL;

    {
        AstNode *ret = astCreateSval(AST_TYPE_IDENT, retBuf);
        if (ret == NULL || !astAddChild(func, ret)) { astFree(func); return NULL; }
    }

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<formal-parameter-list>")) {
            AstNode *params = buildFormalParameterList(c);
            if (params == NULL || !astAddChild(func, params)) { astFree(func); return NULL; }
        } else if (labelEq(c, "block")) {
            AstNode *block = buildBlock(c);
            if (block == NULL || !astAddChild(func, block)) { astFree(func); return NULL; }
        }
    }

    return func;
}

static AstNode *buildBlock(const ParseTreeNode *n) {
    AstNode *block = astCreateNode(AST_BLOCK);
    size_t i;

    if (block == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        AstNode *child = dispatchNode(n->children[i]);
        if (child != NULL && !astAddChild(block, child)) { astFree(block); return NULL; }
    }

    return block;
}

static AstNode *buildFormalParameterList(const ParseTreeNode *n) {
    AstNode *params = astCreateNode(AST_PARAM_LIST);
    size_t i;

    if (params == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<parameter-group>")) {
            AstNode *pg = buildParameterGroup(n->children[i]);
            if (pg == NULL || !astAddChild(params, pg)) { astFree(params); return NULL; }
        }
    }

    return params;
}

static AstNode *buildParameterGroup(const ParseTreeNode *n) {
    AstNode *pg = astCreateNode(AST_PARAM_GROUP);
    AstNode *idList = NULL;
    AstNode *typeNode = NULL;
    size_t i;

    if (pg == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<identifier-list>") && idList == NULL)
            idList = buildIdentifierList(c);
        else if (labelEq(c, "<array-type>") && typeNode == NULL)
            typeNode = buildArrayType(c);
        else if ((labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) && typeNode == NULL) {
            char buf[256];
            extractLexeme(c->label, buf, sizeof(buf));
            typeNode = astCreateSval(AST_TYPE_IDENT, buf);
        }
    }

    if (idList == NULL || typeNode == NULL ||
        !astAddChild(pg, idList) || !astAddChild(pg, typeNode)) {
        astFree(pg);
        return NULL;
    }

    return pg;
}


static AstNode *buildCompoundStatement(const ParseTreeNode *n) {
    AstNode *comp = astCreateNode(AST_COMPOUND);
    size_t i;

    if (comp == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<statement-list>")) {
            AstNode *stmts = buildStatementList(n->children[i]);
            if (stmts == NULL || !astAddChild(comp, stmts)) { astFree(comp); return NULL; }
        }
    }

    return comp;
}

static AstNode *buildStatementList(const ParseTreeNode *n) {
    AstNode *list = astCreateNode(AST_STMT_LIST);
    size_t i;

    if (list == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];

        if (labelPrefix(c, "SEMICOLON") || labelPrefix(c, "semicolon")) continue;

        if (labelEq(c, "<statement>")) {
            AstNode *stmt = buildStatement(c);
            if (stmt == NULL || !astAddChild(list, stmt)) { astFree(list); return NULL; }
        }
    }

    return list;
}

static AstNode *buildStatement(const ParseTreeNode *n) {
    if (n->childCount == 0) {
        return astCreateNode(AST_EMPTY_STMT);
    }

    return dispatchNode(n->children[0]);
}

static AstNode *buildAssignmentStatement(const ParseTreeNode *n) {
    AstNode *assign = astCreateNode(AST_ASSIGN);
    AstNode *varNode = NULL;
    AstNode *exprNode = NULL;
    size_t i;

    if (assign == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<variable>") && varNode == NULL)
            varNode = buildVariable(c);
        else if (labelEq(c, "<expression>") && exprNode == NULL)
            exprNode = buildExpression(c);
    }

    if (varNode == NULL || exprNode == NULL ||
        !astAddChild(assign, varNode) || !astAddChild(assign, exprNode)) {
        astFree(assign);
        return NULL;
    }

    return assign;
}

static AstNode *buildIfStatement(const ParseTreeNode *n) {
    AstNode *ifNode = astCreateNode(AST_IF);
    size_t i;
    int foundThen = 0;
    int foundElse = 0;

    if (ifNode == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<expression>")) {
            AstNode *cond = buildExpression(c);
            if (cond == NULL || !astAddChild(ifNode, cond)) { astFree(ifNode); return NULL; }
        } else if (labelPrefix(c, "THENSY") || labelPrefix(c, "thensy")) {
            foundThen = 1;
        } else if (labelPrefix(c, "ELSESY") || labelPrefix(c, "elsesy")) {
            foundElse = 1;
        } else if (labelEq(c, "<statement>") || labelEq(c, "<compound-statement>")) {
            AstNode *stmt = dispatchNode(c);
            if (stmt == NULL || !astAddChild(ifNode, stmt)) { astFree(ifNode); return NULL; }
        }
    }

    (void)foundThen;
    (void)foundElse;
    return ifNode;
}

static AstNode *buildWhileStatement(const ParseTreeNode *n) {
    AstNode *wh = astCreateNode(AST_WHILE);
    size_t i;

    if (wh == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<expression>")) {
            AstNode *cond = buildExpression(c);
            if (cond == NULL || !astAddChild(wh, cond)) { astFree(wh); return NULL; }
        } else if (labelEq(c, "<statement>")) {
            AstNode *body = buildStatement(c);
            if (body == NULL || !astAddChild(wh, body)) { astFree(wh); return NULL; }
        } else if (labelEq(c, "<compound-statement>")) {
            AstNode *body = buildCompoundStatement(c);
            if (body == NULL || !astAddChild(wh, body)) { astFree(wh); return NULL; }
        }
    }

    return wh;
}

static AstNode *buildForStatement(const ParseTreeNode *n) {
    AstNode *forNode;
    char buf[256];
    int exprCount = 0;
    int toDir = 1; /* 1 = to, -1 = downto */
    size_t i;

    buf[0] = '\0';
    for (i = 0; i < n->childCount; i++) {
        if (labelPrefix(n->children[i], "IDENT(") || labelPrefix(n->children[i], "ident(")) {
            extractLexeme(n->children[i]->label, buf, sizeof(buf));
            break;
        }
    }

    forNode = astCreateSval(AST_FOR, buf);
    if (forNode == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelPrefix(c, "DOWNTOSY") || labelPrefix(c, "downtosy")) {
            toDir = -1;
        } else if (labelEq(c, "<expression>")) {
            AstNode *expr = buildExpression(c);
            if (expr == NULL || !astAddChild(forNode, expr)) { astFree(forNode); return NULL; }
            exprCount++;
        } else if (labelEq(c, "<statement>")) {
            AstNode *body = buildStatement(c);
            if (body == NULL || !astAddChild(forNode, body)) { astFree(forNode); return NULL; }
        } else if (labelEq(c, "<compound-statement>")) {
            AstNode *body = buildCompoundStatement(c);
            if (body == NULL || !astAddChild(forNode, body)) { astFree(forNode); return NULL; }
        }
    }

    forNode->ival = toDir; /* 1=to, -1=downto */
    (void)exprCount;
    return forNode;
}

static AstNode *buildRepeatStatement(const ParseTreeNode *n) {
    AstNode *rep = astCreateNode(AST_REPEAT);
    size_t i;

    if (rep == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<statement-list>")) {
            AstNode *stmts = buildStatementList(c);
            if (stmts == NULL || !astAddChild(rep, stmts)) { astFree(rep); return NULL; }
        } else if (labelEq(c, "<expression>")) {
            AstNode *cond = buildExpression(c);
            if (cond == NULL || !astAddChild(rep, cond)) { astFree(rep); return NULL; }
        }
    }

    return rep;
}

static AstNode *buildCaseStatement(const ParseTreeNode *n) {
    AstNode *cas = astCreateNode(AST_CASE);
    size_t i;

    if (cas == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelEq(c, "<expression>")) {
            AstNode *expr = buildExpression(c);
            if (expr == NULL || !astAddChild(cas, expr)) { astFree(cas); return NULL; }
        } else if (labelEq(c, "<case-block>")) {
            const ParseTreeNode *chain = c;
            while (chain != NULL && labelEq(chain, "<case-block>")) {
                AstNode *cb = buildCaseBlock(chain);
                const ParseTreeNode *next = NULL;
                size_t j;
                if (cb == NULL || !astAddChild(cas, cb)) { astFree(cas); return NULL; }
                for (j = 0; j < chain->childCount; j++) {
                    if (labelEq(chain->children[j], "<case-block>")) {
                        next = chain->children[j];
                        break;
                    }
                }
                chain = next;
            }
        }
    }

    return cas;
}

static AstNode *buildCaseBlock(const ParseTreeNode *n) {
    AstNode *cb = astCreateNode(AST_CASE_BLOCK);
    int pastColon = 0;
    size_t i;

    if (cb == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if (labelPrefix(c, "COLON") || labelPrefix(c, "colon")) {
            pastColon = 1;
        } else if (!pastColon && labelEq(c, "<constant>")) {
            AstNode *con = buildConstant(c);
            if (con == NULL || !astAddChild(cb, con)) { astFree(cb); return NULL; }
        } else if (pastColon && labelEq(c, "<statement>")) {
            AstNode *stmt = buildStatement(c);
            if (stmt == NULL || !astAddChild(cb, stmt)) { astFree(cb); return NULL; }
        }
    }

    return cb;
}

/* ===== Procedure/Function call ===== */

static AstNode *buildProcFuncCall(const ParseTreeNode *n, AstNodeType callType) {
    AstNode *call;
    char buf[256];
    size_t i;

    buf[0] = '\0';
    for (i = 0; i < n->childCount; i++) {
        if (labelPrefix(n->children[i], "IDENT(") || labelPrefix(n->children[i], "ident(")) {
            extractLexeme(n->children[i]->label, buf, sizeof(buf));
            break;
        }
    }

    call = astCreateSval(callType, buf);
    if (call == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<parameter-list>")) {
            AstNode *params = buildParameterList(n->children[i]);
            if (params == NULL || !astAddChild(call, params)) { astFree(call); return NULL; }
        }
    }

    return call;
}

static AstNode *buildParameterList(const ParseTreeNode *n) {
    AstNode *params = astCreateNode(AST_PARAM_LIST);
    size_t i;

    if (params == NULL) return NULL;

    for (i = 0; i < n->childCount; i++) {
        if (labelEq(n->children[i], "<expression>")) {
            AstNode *expr = buildExpression(n->children[i]);
            if (expr == NULL || !astAddChild(params, expr)) { astFree(params); return NULL; }
        }
    }

    return params;
}

/* ===== Expressions ===== */

static AstNode *buildExpression(const ParseTreeNode *n) {
    if (n->childCount == 1) {
        return buildSimpleExpression(n->children[0]);
    }

    if (n->childCount == 3) {
        AstNode *left = buildSimpleExpression(n->children[0]);
        AstNode *right = buildSimpleExpression(n->children[2]);
        AstNode *binop;
        const ParseTreeNode *opNode = n->children[1];
        char opStr[32] = "?";

        if (opNode->childCount > 0) {
            strncpy(opStr, opNode->children[0]->label, sizeof(opStr) - 1);
        }

        binop = astCreateSval(AST_BINOP, opStr);
        if (binop == NULL || left == NULL || right == NULL ||
            !astAddChild(binop, left) || !astAddChild(binop, right)) {
            astFree(left); astFree(right); astFree(binop);
            return NULL;
        }

        return binop;
    }

    if (n->childCount > 0) return dispatchNode(n->children[0]);
    return astCreateNode(AST_EMPTY_STMT);
}

static AstNode *buildSimpleExpression(const ParseTreeNode *n) {
    size_t i = 0;
    AstNode *result = NULL;
    char unary[8] = "";

    if (n->childCount == 0) return astCreateNode(AST_EMPTY_STMT);

    if (labelPrefix(n->children[0], "PLUS") || labelPrefix(n->children[0], "plus")) {
        strncpy(unary, "+", sizeof(unary) - 1);
        i = 1;
    } else if (labelPrefix(n->children[0], "MINUS") || labelPrefix(n->children[0], "minus")) {
        strncpy(unary, "-", sizeof(unary) - 1);
        i = 1;
    }

    if (i >= n->childCount) return astCreateNode(AST_EMPTY_STMT);

    result = buildTerm(n->children[i]);
    i++;

    if (unary[0] == '-' && result != NULL) {
        AstNode *unop = astCreateSval(AST_UNOP, "-");
        if (unop == NULL || !astAddChild(unop, result)) { astFree(result); astFree(unop); return NULL; }
        result = unop;
    }

    while (i + 1 < n->childCount) {
        AstNode *right = buildTerm(n->children[i+1]);
        AstNode *binop;
        const ParseTreeNode *opNode = n->children[i];
        char opStr[32] = "?";

        if (opNode->childCount > 0) {
            strncpy(opStr, opNode->children[0]->label, sizeof(opStr) - 1);
        }

        binop = astCreateSval(AST_BINOP, opStr);
        if (binop == NULL || right == NULL ||
            !astAddChild(binop, result) || !astAddChild(binop, right)) {
            astFree(result); astFree(right); astFree(binop);
            return NULL;
        }

        result = binop;
        i += 2;
    }

    return result;
}

static AstNode *buildTerm(const ParseTreeNode *n) {
    size_t i = 0;
    AstNode *result;

    if (n->childCount == 0) return astCreateNode(AST_EMPTY_STMT);

    result = buildFactor(n->children[0]);
    i = 1;

    while (i + 1 < n->childCount) {
        AstNode *right = buildFactor(n->children[i+1]);
        AstNode *binop;
        const ParseTreeNode *opNode = n->children[i];
        char opStr[32] = "?";

        if (opNode->childCount > 0) {
            strncpy(opStr, opNode->children[0]->label, sizeof(opStr) - 1);
        }

        binop = astCreateSval(AST_BINOP, opStr);
        if (binop == NULL || right == NULL ||
            !astAddChild(binop, result) || !astAddChild(binop, right)) {
            astFree(result); astFree(right); astFree(binop);
            return NULL;
        }

        result = binop;
        i += 2;
    }

    return result;
}

static AstNode *buildFactor(const ParseTreeNode *n) {
    /* <factor> -> INTCON | REALCON | CHARCON | STRING | <variable> |
                  LPARENT <expression> RPARENT | NOTSY <factor> |
                  <procedure/function-call> */
    char buf[512];

    if (n->childCount == 0) return astCreateNode(AST_EMPTY_STMT);

    {
        const ParseTreeNode *c = n->children[0];

        if (labelPrefix(c, "INTCON(") || labelPrefix(c, "intcon(")) {
            long long v = 0;
            extractLexeme(c->label, buf, sizeof(buf));
            v = atoll(buf);
            return astCreateIval(AST_INT_LIT, v);
        }

        if (labelPrefix(c, "REALCON(") || labelPrefix(c, "realcon(")) {
            double v = 0.0;
            extractLexeme(c->label, buf, sizeof(buf));
            v = atof(buf);
            return astCreateRval(AST_REAL_LIT, v);
        }

        if (labelPrefix(c, "CHARCON(") || labelPrefix(c, "charcon(")) {
            extractLexeme(c->label, buf, sizeof(buf));
            return astCreateSval(AST_CHAR_LIT, buf);
        }

        if (labelPrefix(c, "STRING(") || labelPrefix(c, "string(")) {
            extractLexeme(c->label, buf, sizeof(buf));
            return astCreateSval(AST_STRING_LIT, buf);
        }

        if (labelPrefix(c, "NOTSY") || labelPrefix(c, "notsy")) {
            AstNode *unop = astCreateSval(AST_UNOP, "not");
            AstNode *operand = (n->childCount > 1) ? buildFactor(n->children[1]) : NULL;
            if (unop == NULL || operand == NULL || !astAddChild(unop, operand)) {
                astFree(unop); astFree(operand);
                return NULL;
            }
            return unop;
        }

        if (labelEq(c, "<variable>"))               return buildVariable(c);
        if (labelEq(c, "<procedure/function-call>")) return buildProcFuncCall(c, AST_FUNC_CALL);
        if (labelEq(c, "<expression>"))              return buildExpression(c);

        /* LPARENT <expression> RPARENT */
        if (labelPrefix(c, "LPARENT") || labelPrefix(c, "lparent")) {
            if (n->childCount >= 2) return buildExpression(n->children[1]);
        }
    }

    return astCreateNode(AST_EMPTY_STMT);
}

static int addIndexListChildren(AstNode *target, const ParseTreeNode *list) {
    char buf[256];
    size_t i;

    if (target == NULL || list == NULL) return 0;

    for (i = 0; i < list->childCount; i++) {
        const ParseTreeNode *idx = list->children[i];
        AstNode *idxNode = NULL;

        if (labelEq(idx, "<index-list>")) {
            if (!addIndexListChildren(target, idx)) return 0;
            continue;
        }

        if (labelPrefix(idx, "INTCON(") || labelPrefix(idx, "intcon(")) {
            extractLexeme(idx->label, buf, sizeof(buf));
            idxNode = astCreateIval(AST_INT_LIT, atoll(buf));
        } else if (labelPrefix(idx, "IDENT(") || labelPrefix(idx, "ident(")) {
            extractLexeme(idx->label, buf, sizeof(buf));
            idxNode = astCreateSval(AST_VAR, buf);
        } else if (labelPrefix(idx, "CHARCON(") || labelPrefix(idx, "charcon(")) {
            extractLexeme(idx->label, buf, sizeof(buf));
            idxNode = astCreateSval(AST_CHAR_LIT, buf);
        }

        if (idxNode != NULL && !astAddChild(target, idxNode)) {
            astFree(idxNode);
            return 0;
        }
    }

    return 1;
}

static AstNode *buildVariable(const ParseTreeNode *n) {
    AstNode *var = NULL;
    char buf[256];
    size_t i;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];
        if ((labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) && var == NULL) {
            extractLexeme(c->label, buf, sizeof(buf));
            var = astCreateSval(AST_VAR, buf);
            if (var == NULL) return NULL;
        } else if (labelEq(c, "<component-variable>")) {
            const ParseTreeNode *cv = c;
            if (cv->childCount > 0) {
                if (labelPrefix(cv->children[0], "LBRACK") || labelPrefix(cv->children[0], "lbrack")) {
                    AstNode *acc = astCreateNode(AST_ARRAY_ACCESS);
                    size_t j;
                    if (acc == NULL || !astAddChild(acc, var)) { astFree(acc); return NULL; }
                    var = acc;
                    for (j = 0; j < cv->childCount; j++) {
                        if (labelEq(cv->children[j], "<index-list>")) {
                            if (!addIndexListChildren(var, cv->children[j])) return NULL;
                        }
                    }
                }
                else if (labelPrefix(cv->children[0], "PERIOD") || labelPrefix(cv->children[0], "period")) {
                    if (cv->childCount >= 2) {
                        AstNode *fa = astCreateNode(AST_FIELD_ACCESS);
                        char fieldBuf[256];
                        AstNode *field;
                        extractLexeme(cv->children[1]->label, fieldBuf, sizeof(fieldBuf));
                        field = astCreateSval(AST_VAR, fieldBuf);
                        if (fa == NULL || field == NULL ||
                            !astAddChild(fa, var) || !astAddChild(fa, field)) {
                            astFree(fa); astFree(field);
                            return NULL;
                        }
                        var = fa;
                    }
                }
            }
        }
    }

    return var != NULL ? var : astCreateNode(AST_EMPTY_STMT);
}

static AstNode *buildConstant(const ParseTreeNode *n) {
    char buf[256];
    int negative = 0;
    size_t i;

    for (i = 0; i < n->childCount; i++) {
        const ParseTreeNode *c = n->children[i];

        if (labelPrefix(c, "MINUS") || labelPrefix(c, "minus")) {
            negative = 1;
            continue;
        }
        if (labelPrefix(c, "PLUS") || labelPrefix(c, "plus")) {
            continue;
        }

        if (labelPrefix(c, "INTCON(") || labelPrefix(c, "intcon(")) {
            long long v;
            extractLexeme(c->label, buf, sizeof(buf));
            v = atoll(buf);
            return astCreateIval(AST_INT_LIT, negative ? -v : v);
        }
        if (labelPrefix(c, "REALCON(") || labelPrefix(c, "realcon(")) {
            double v;
            extractLexeme(c->label, buf, sizeof(buf));
            v = atof(buf);
            return astCreateRval(AST_REAL_LIT, negative ? -v : v);
        }
        if (labelPrefix(c, "CHARCON(") || labelPrefix(c, "charcon(")) {
            extractLexeme(c->label, buf, sizeof(buf));
            return astCreateSval(AST_CHAR_LIT, buf);
        }
        if (labelPrefix(c, "STRING(") || labelPrefix(c, "string(")) {
            extractLexeme(c->label, buf, sizeof(buf));
            return astCreateSval(AST_STRING_LIT, buf);
        }
        if (labelPrefix(c, "IDENT(") || labelPrefix(c, "ident(")) {
            extractLexeme(c->label, buf, sizeof(buf));
            /* true/false → AST_BOOL_LIT */
            if (strcasecmp(buf, "true") == 0 || strcasecmp(buf, "false") == 0) {
                AstNode *b = astCreateSval(AST_BOOL_LIT, buf);
                return b;
            }
            return astCreateSval(AST_VAR, buf);
        }
    }

    return astCreateNode(AST_EMPTY_STMT);
}

/* ===== Entry point ===== */

AstNode *buildAst(const ParseTreeNode *parseRoot) {
    if (parseRoot == NULL) return NULL;
    return dispatchNode(parseRoot);
}
