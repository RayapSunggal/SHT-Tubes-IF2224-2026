#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../grammar/grammar.h"
#include "../lexer/lexer.h"
#include "../token/token.h"

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
    size_t pos;
    bool fatalError;
    size_t errorCount;
    size_t errorLength;
    char error[PARSER_MAX_MESSAGE];
} Parser;

static void describeToken(Token *token, char *buf, size_t bufSize);
static Token *parserCurrentToken(Parser *ps);
static void parserRecordSyntaxErrorToken(Parser *ps, Token *token, const char *fmt, ...);
static void parserRecordSyntaxErrorCurrent(Parser *ps, const char *fmt, ...);
static bool isStatementStart(TokenType type);

static bool parserSetError(Parser *ps, const char *fmt, ...) {
    va_list args;

    ps->fatalError=true;
    va_start(args, fmt);
    (void)vsnprintf(ps->error, sizeof(ps->error), fmt, args);
    va_end(args);
    ps->errorLength=strlen(ps->error);
    return false;
}

static void parserAppendErrorText(Parser *ps, const char *text) {
    size_t available;
    size_t textLen;

    if (ps->fatalError || text==NULL || ps->errorLength >= sizeof(ps->error) - 1) {
        return;
    }

    available=sizeof(ps->error) - 1 - ps->errorLength;
    textLen=strlen(text);
    if (textLen > available) {
        textLen=available;
    }

    memcpy(ps->error + ps->errorLength, text, textLen);
    ps->errorLength += textLen;
    ps->error[ps->errorLength]='\0';
}

static void parserRecordSyntaxErrorV(Parser *ps, Token *token, const char *fmt, va_list args) {
    char detail[256];
    char found[96];
    char line[512];

    if (ps->fatalError) {
        return;
    }

    (void)vsnprintf(detail, sizeof(detail), fmt, args);

    if (ps->errorCount > 0) {
        parserAppendErrorText(ps, "\n");
    }

    if (token==NULL) {
        (void)snprintf(line, sizeof(line), "Line ?: %s", detail);
    }
    else {
        describeToken(token, found, sizeof(found));
        (void)snprintf(
            line,
            sizeof(line),
            "Line %zu near %s: %s",
            token->line,
            found,
            detail
        );
    }

    parserAppendErrorText(ps, line);
    ps->errorCount++;
}

static void parserRecordSyntaxErrorToken(Parser *ps, Token *token, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    parserRecordSyntaxErrorV(ps, token, fmt, args);
    va_end(args);
}

static void parserRecordSyntaxErrorCurrent(Parser *ps, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    parserRecordSyntaxErrorV(ps, parserCurrentToken(ps), fmt, args);
    va_end(args);
}

static bool tokenHasPrintableLexeme(TokenType type) {
    switch (type) {
        case TOKEN_INTCON:
        case TOKEN_REALCON:
        case TOKEN_CHARCON:
        case TOKEN_STRING:
        case TOKEN_IDENT:
        case TOKEN_UNKNOWN:
            return true;
        default:
            return false;
    }
}

static void describeToken(Token *token, char *buf, size_t bufSize) {
    if (tokenHasPrintableLexeme(token->type)) {
        (void)snprintf(
            buf,
            bufSize,
            "%s (%s)",
            tokenTypeToString(token->type),
            token->lexeme
        );
        return;
    }

    (void)snprintf(buf, bufSize, "%s", tokenTypeToString(token->type));
}

static Token *parserCurrentToken(Parser *ps) {
    if (ps->count==0) {
        return NULL;
    }

    if (ps->pos >= ps->count) {
        return &ps->tokens[ps->count - 1];
    }

    return &ps->tokens[ps->pos];
}

static TokenType parserCurrentType(Parser *ps) {
    Token *token=parserCurrentToken(ps);

    if (token==NULL) {
        return TOKEN_EOF;
    }

    return token->type;
}

static TokenType parserPeekType(Parser *ps, size_t lookahead) {
    size_t idx=ps->pos + lookahead;

    if (ps->count==0) {
        return TOKEN_EOF;
    }

    if (idx >= ps->count) {
        return ps->tokens[ps->count - 1].type;
    }

    return ps->tokens[idx].type;
}

static void parserAdvance(Parser *ps) {
    if (ps->pos < ps->count) {
        ps->pos++;
    }
}

static bool isConstantStart(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_IDENT:
        case TOKEN_INTCON:
        case TOKEN_REALCON:
        case TOKEN_CHARCON:
        case TOKEN_STRING:
            return true;
        default:
            return false;
    }
}

static bool isExpressionStart(TokenType type) {
    return isConstantStart(type) ||
           type==TOKEN_LPARENT ||
           type==TOKEN_NOTSY;
}

static bool isTypeStart(TokenType type) {
    return type==TOKEN_ARRAYSY ||
           type==TOKEN_RECORDSY ||
           type==TOKEN_LPARENT ||
           type==TOKEN_LBRACK ||
           type==TOKEN_IDENT ||
           isExpressionStart(type);
}

static bool isDeclarationStart(TokenType type) {
    switch (type) {
        case TOKEN_CONSTSY:
        case TOKEN_TYPESY:
        case TOKEN_VARSY:
        case TOKEN_PROCEDURESY:
        case TOKEN_FUNCTIONSY:
            return true;
        default:
            return false;
    }
}

static bool isStatementListTerminator(TokenType type) {
    return type==TOKEN_ENDSY ||
           type==TOKEN_UNTILSY ||
           type==TOKEN_ELSESY ||
           type==TOKEN_EOF;
}

static void parserSkipUntilStatementBoundary(Parser *ps) {
    while (parserCurrentType(ps)!=TOKEN_EOF &&
           !isStatementListTerminator(parserCurrentType(ps)) &&
           parserCurrentType(ps)!=TOKEN_SEMICOLON &&
           !isStatementStart(parserCurrentType(ps))) {
        parserAdvance(ps);
    }
}

static void parserSkipUntilExpressionBoundary(Parser *ps) {
    while (parserCurrentType(ps)!=TOKEN_EOF &&
           parserCurrentType(ps)!=TOKEN_SEMICOLON &&
           parserCurrentType(ps)!=TOKEN_COMMA &&
           parserCurrentType(ps)!=TOKEN_RPARENT &&
           parserCurrentType(ps)!=TOKEN_RBRACK &&
           parserCurrentType(ps)!=TOKEN_COLON &&
           parserCurrentType(ps)!=TOKEN_THENSY &&
           parserCurrentType(ps)!=TOKEN_DOSY &&
           parserCurrentType(ps)!=TOKEN_TOSY &&
           parserCurrentType(ps)!=TOKEN_DOWNTOSY &&
           parserCurrentType(ps)!=TOKEN_OFSY &&
           !isStatementListTerminator(parserCurrentType(ps)) &&
           parserCurrentType(ps)!=TOKEN_PERIOD) {
        parserAdvance(ps);
    }
}

static void parserSkipUntilTypeBoundary(Parser *ps) {
    while (parserCurrentType(ps)!=TOKEN_EOF &&
           parserCurrentType(ps)!=TOKEN_SEMICOLON &&
           parserCurrentType(ps)!=TOKEN_COMMA &&
           parserCurrentType(ps)!=TOKEN_RPARENT &&
           parserCurrentType(ps)!=TOKEN_RBRACK &&
           parserCurrentType(ps)!=TOKEN_OFSY &&
           parserCurrentType(ps)!=TOKEN_ENDSY &&
           !isDeclarationStart(parserCurrentType(ps)) &&
           parserCurrentType(ps)!=TOKEN_BEGINSY) {
        parserAdvance(ps);
    }
}

static bool parserShouldInsertMissingToken(Parser *ps, TokenType expected) {
    TokenType current=parserCurrentType(ps);

    switch (expected) {
        case TOKEN_SEMICOLON:
            return current==TOKEN_EOF ||
                   isStatementListTerminator(current) ||
                   isStatementStart(current) ||
                   current==TOKEN_IDENT ||
                   isDeclarationStart(current);
        case TOKEN_COLON:
            return current==TOKEN_EOF ||
                   isTypeStart(current) ||
                   isStatementStart(current);
        case TOKEN_EQL:
            return current==TOKEN_EOF ||
                   isTypeStart(current) ||
                   isConstantStart(current);
        case TOKEN_BECOMES:
            return current==TOKEN_EOF ||
                   isExpressionStart(current);
        case TOKEN_THENSY:
        case TOKEN_DOSY:
            return current==TOKEN_EOF ||
                   isStatementStart(current);
        case TOKEN_RPARENT:
            return current==TOKEN_EOF ||
                   parserCurrentType(ps)==TOKEN_SEMICOLON ||
                   parserCurrentType(ps)==TOKEN_COMMA ||
                   parserCurrentType(ps)==TOKEN_COLON ||
                   parserCurrentType(ps)==TOKEN_THENSY ||
                   parserCurrentType(ps)==TOKEN_DOSY ||
                   parserCurrentType(ps)==TOKEN_TOSY ||
                   parserCurrentType(ps)==TOKEN_DOWNTOSY ||
                   isStatementListTerminator(current);
        case TOKEN_RBRACK:
            return current==TOKEN_EOF ||
                   parserCurrentType(ps)==TOKEN_OFSY ||
                   parserCurrentType(ps)==TOKEN_SEMICOLON ||
                   parserCurrentType(ps)==TOKEN_COMMA;
        case TOKEN_ENDSY:
            return current==TOKEN_EOF ||
                   current==TOKEN_PERIOD ||
                   current==TOKEN_SEMICOLON ||
                   isStatementListTerminator(current);
        case TOKEN_PERIOD:
            return current==TOKEN_EOF;
        default:
            return current==TOKEN_EOF;
    }
}

static ParseTreeNode *parserCreateNode(Parser *ps, const char *label) {
    ParseTreeNode *node=parseTreeCreateNode(label);

    if (node==NULL) {
        parserSetError(ps, "Out of memory while building parse tree.");
    }

    return node;
}

static bool parserAttachChild(Parser *ps, ParseTreeNode *parent, ParseTreeNode *child) {
    if (!parseTreeAddChild(parent, child)) {
        parseTreeFree(child);
        return parserSetError(ps, "Out of memory while building parse tree.");
    }

    return true;
}

static ParseTreeNode *parserCreateTokenNode(Parser *ps, Token *token) {
    char label[2 * MAX_LEXEME + 32];

    if (token==NULL) {
        return parserCreateNode(ps, "unknown");
    }

    if (tokenHasPrintableLexeme(token->type)) {
        (void)snprintf(
            label,
            sizeof(label),
            "%s(%s)",
            tokenTypeToString(token->type),
            token->lexeme
        );
    }
    else {
        (void)snprintf(label, sizeof(label), "%s", tokenTypeToString(token->type));
    }

    return parserCreateNode(ps, label);
}

static bool parserExpectToken(Parser *ps, TokenType expected, ParseTreeNode *parent) {
    char found[128];
    Token *token;

    if (parserCurrentType(ps)==expected) {
        if (parent!=NULL) {
            ParseTreeNode *leaf;

            token=parserCurrentToken(ps);
            leaf=parserCreateTokenNode(ps, token);
            if (leaf==NULL) {
                return false;
            }

            if (!parserAttachChild(ps, parent, leaf)) {
                return false;
            }
        }

        parserAdvance(ps);
        return true;
    }

    token=parserCurrentToken(ps);
    if (token!=NULL) {
        describeToken(token, found, sizeof(found));
    }
    else {
        (void)snprintf(found, sizeof(found), "eof");
    }

    parserRecordSyntaxErrorToken(
        ps,
        token,
        "Expected %s, found %s.",
        tokenTypeToString(expected),
        found
    );

    if (parserShouldInsertMissingToken(ps, expected)) {
        (void)parent;
        return true;
    }

    if (parserCurrentType(ps)!=TOKEN_EOF) {
        parserAdvance(ps);
    }

    if (parserCurrentType(ps)==expected) {
        if (parent!=NULL) {
            ParseTreeNode *leaf=parserCreateTokenNode(ps, parserCurrentToken(ps));
            if (leaf==NULL) {
                return false;
            }

            if (!parserAttachChild(ps, parent, leaf)) {
                return false;
            }
        }

        parserAdvance(ps);
    }

    return true;
}

static bool parserAcceptToken(Parser *ps, TokenType accepted, ParseTreeNode *parent) {
    if (parserCurrentType(ps)!=accepted) {
        return false;
    }

    return parserExpectToken(ps, accepted, parent);
}

static bool parserAppendToken(Parser *ps, Token *token) {
    Token *newTokens;
    size_t newCapacity;

    if (ps->count==ps->capacity) {
        newCapacity=ps->capacity==0 ? 32 : ps->capacity * 2;
        newTokens=(Token *)realloc(ps->tokens, newCapacity * sizeof(Token));
        if (newTokens==NULL) {
            return parserSetError(ps, "Out of memory while reading tokens.");
        }

        ps->tokens=newTokens;
        ps->capacity=newCapacity;
    }

    ps->tokens[ps->count]=*token;
    ps->count++;
    return true;
}


static bool loadTokensFromLexer(Parser *ps, const char *inputPath) {
    Lexer lx;

    initLexer(&lx, inputPath);
    if (!lx.ready) {
        return parserSetError(ps, "Failed to open source file: %s", inputPath);
    }

    while (true) {
        Token tk=getToken(&lx);

        if (tk.type==TOKEN_COMMENT) {
            continue;
        }

        if (tk.type==TOKEN_UNKNOWN) {
            parserRecordSyntaxErrorToken(ps, &tk, "Invalid token.");
            continue;
        }

        if (!parserAppendToken(ps, &tk)) {
            closeLexer(&lx);
            return false;
        }

        if (tk.type==TOKEN_EOF) {
            break;
        }
    }

    closeLexer(&lx);
    return true;
}

static void parserDestroy(Parser *ps) {
    free(ps->tokens);
    ps->tokens=NULL;
    ps->count=0;
    ps->capacity=0;
    ps->pos=0;
    ps->fatalError=false;
    ps->errorCount=0;
    ps->errorLength=0;
    ps->error[0]='\0';
}

static bool isRelationalOperator(TokenType type) {
    switch (type) {
        case TOKEN_EQL:
        case TOKEN_NEQ:
        case TOKEN_GTR:
        case TOKEN_GEQ:
        case TOKEN_LSS:
        case TOKEN_LEQ:
            return true;
        default:
            return false;
    }
}

static bool isAdditiveOperator(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_ORSY:
            return true;
        default:
            return false;
    }
}

static bool isMultiplicativeOperator(TokenType type) {
    switch (type) {
        case TOKEN_TIMES:
        case TOKEN_RDIV:
        case TOKEN_IDIV:
        case TOKEN_IMOD:
        case TOKEN_ANDSY:
            return true;
        default:
            return false;
    }
}

static bool isStatementStart(TokenType type) {
    switch (type) {
        case TOKEN_IDENT:
        case TOKEN_IFSY:
        case TOKEN_CASESY:
        case TOKEN_WHILESY:
        case TOKEN_REPEATSY:
        case TOKEN_FORSY:
        case TOKEN_BEGINSY:
            return true;
        default:
            return false;
    }
}

static ParseTreeNode *parseProgram(Parser *ps);
static ParseTreeNode *parseProgramHeader(Parser *ps);
static ParseTreeNode *parseDeclarationPart(Parser *ps);
static ParseTreeNode *parseConstDeclaration(Parser *ps);
static ParseTreeNode *parseConstant(Parser *ps);
static ParseTreeNode *parseTypeDeclaration(Parser *ps);
static ParseTreeNode *parseVarDeclaration(Parser *ps);
static ParseTreeNode *parseIdentifierList(Parser *ps);
static ParseTreeNode *parseType(Parser *ps);
static ParseTreeNode *parseArrayType(Parser *ps);
static ParseTreeNode *parseRange(Parser *ps);
static ParseTreeNode *parseEnumerated(Parser *ps);
static ParseTreeNode *parseRecordType(Parser *ps);
static ParseTreeNode *parseFieldList(Parser *ps);
static ParseTreeNode *parseFieldPart(Parser *ps);
static ParseTreeNode *parseSubprogramDeclaration(Parser *ps);
static ParseTreeNode *parseProcedureDeclaration(Parser *ps);
static ParseTreeNode *parseFunctionDeclaration(Parser *ps);
static ParseTreeNode *parseBlock(Parser *ps);
static ParseTreeNode *parseFormalParameterList(Parser *ps);
static ParseTreeNode *parseParameterGroup(Parser *ps);
static ParseTreeNode *parseCompoundStatement(Parser *ps);
static ParseTreeNode *parseStatementList(Parser *ps);
static ParseTreeNode *parseStatement(Parser *ps);
static ParseTreeNode *parseAssignmentStatement(Parser *ps);
static ParseTreeNode *parseIfStatement(Parser *ps);
static ParseTreeNode *parseCaseStatement(Parser *ps);
static ParseTreeNode *parseCaseBlock(Parser *ps);
static ParseTreeNode *parseWhileStatement(Parser *ps);
static ParseTreeNode *parseRepeatStatement(Parser *ps);
static ParseTreeNode *parseForStatement(Parser *ps);
static ParseTreeNode *parseVariable(Parser *ps);
static ParseTreeNode *parseComponentVariable(Parser *ps);
static ParseTreeNode *parseIndexList(Parser *ps);
static ParseTreeNode *parseProcedureFunctionCall(Parser *ps);
static ParseTreeNode *parseParameterList(Parser *ps);
static ParseTreeNode *parseExpression(Parser *ps);
static ParseTreeNode *parseSimpleExpression(Parser *ps);
static ParseTreeNode *parseTerm(Parser *ps);
static ParseTreeNode *parseFactor(Parser *ps);

static Node *grammarCreateNode(Parser *ps, const char *label);
static bool grammarAttachChild(Parser *ps, Node *parent, Node *child);
static void grammarFreeNode(Node *node);
static Node *convertParseTreeToGrammarNode(Parser *ps, ParseTreeNode *src);
static bool validateWithGrammar(Parser *ps, ParseTreeNode *root);

static bool hasBecomesAfterVariable(Parser *ps) {
    size_t i = ps->pos;

    if (i >= ps->count || ps->tokens[i].type != TOKEN_IDENT) {
        return false;
    }
    i++;

    while (i < ps->count) {
        TokenType t = ps->tokens[i].type;

        if (t == TOKEN_LBRACK) {
            int depth = 1;
            i++;
            while (i < ps->count && depth > 0) {
                if (ps->tokens[i].type == TOKEN_LBRACK) depth++;
                else if (ps->tokens[i].type == TOKEN_RBRACK) depth--;
                i++;
            }
        }
        else if (t == TOKEN_PERIOD) {
            if (i + 1 < ps->count && ps->tokens[i + 1].type == TOKEN_PERIOD) {
                break;
            }
            i++;
            if (i < ps->count && ps->tokens[i].type == TOKEN_IDENT) {
                i++;
            }
        }
        else {
            break;
        }
    }

    return i < ps->count && ps->tokens[i].type == TOKEN_BECOMES;
}

static bool isTypeBoundary(TokenType type) {
    switch (type) {
        case TOKEN_SEMICOLON:
        case TOKEN_COMMA:
        case TOKEN_RBRACK:
        case TOKEN_RPARENT:
        case TOKEN_OFSY:
            return true;
        default:
            return false;
    }
}

static bool hasRangeOperatorAheadInType(Parser *ps) {
    size_t i;
    int parenDepth=0;
    int brackDepth=0;

    for (i=ps->pos; i < ps->count; i++) {
        TokenType type=ps->tokens[i].type;

        if (type==TOKEN_LPARENT) {
            parenDepth++;
            continue;
        }

        if (type==TOKEN_RPARENT) {
            if (parenDepth > 0) {
                parenDepth--;
            }
            else if (brackDepth==0 && isTypeBoundary(type)) {
                return false;
            }
            continue;
        }

        if (type==TOKEN_LBRACK) {
            brackDepth++;
            continue;
        }

        if (type==TOKEN_RBRACK) {
            if (brackDepth > 0) {
                brackDepth--;
            }
            else if (parenDepth==0 && isTypeBoundary(type)) {
                return false;
            }
            continue;
        }

        if (parenDepth==0 && brackDepth==0 && isTypeBoundary(type)) {
            return false;
        }

        if (parenDepth==0 &&
            brackDepth==0 &&
            type==TOKEN_PERIOD &&
            i + 1 < ps->count &&
            ps->tokens[i + 1].type==TOKEN_PERIOD) {
            return true;
        }

        if (type==TOKEN_EOF) {
            return false;
        }
    }

    return false;
}

static ParseTreeNode *parseProgramHeader(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<program-header>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_PROGRAMSY, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node) ||
        !parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseConstant(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<constant>");
    TokenType type=parserCurrentType(ps);

    if (node==NULL) {
        return NULL;
    }

    if (type==TOKEN_CHARCON || type==TOKEN_STRING) {
        if (!parserExpectToken(ps, type, node)) {
            parseTreeFree(node);
            return NULL;
        }

        return node;
    }

    if (type==TOKEN_PLUS || type==TOKEN_MINUS) {
        if (!parserExpectToken(ps, type, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    type=parserCurrentType(ps);
    if (type==TOKEN_IDENT || type==TOKEN_INTCON || type==TOKEN_REALCON) {
        if (!parserExpectToken(ps, type, node)) {
            parseTreeFree(node);
            return NULL;
        }

        return node;
    }

    parserRecordSyntaxErrorCurrent(ps, "Expected constant.");
    if (!isConstantStart(parserCurrentType(ps))) {
        parserSkipUntilExpressionBoundary(ps);
    }
    return node;
}

static ParseTreeNode *parseConstDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<const-declaration>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_CONSTSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_IDENT) {
        parserRecordSyntaxErrorCurrent(ps, "Expected identifier after constsy.");
        parserSkipUntilTypeBoundary(ps);
        return node;
    }

    while (parserCurrentType(ps)==TOKEN_IDENT) {
        ParseTreeNode *constantNode;

        if (!parserExpectToken(ps, TOKEN_IDENT, node) ||
            !parserExpectToken(ps, TOKEN_EQL, node)) {
            parseTreeFree(node);
            return NULL;
        }

        constantNode=parseConstant(ps);
        if (constantNode==NULL || !parserAttachChild(ps, node, constantNode)) {
            parseTreeFree(node);
            return NULL;
        }

        if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseIdentifierList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<identifier-list>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserAcceptToken(ps, TOKEN_COMMA, node)) {
        if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseRange(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<range>");
    ParseTreeNode *leftConst;
    ParseTreeNode *rightConst;

    if (node==NULL) {
        return NULL;
    }

    leftConst=parseConstant(ps);
    if (leftConst==NULL || !parserAttachChild(ps, node, leftConst)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_PERIOD, node) ||
        !parserExpectToken(ps, TOKEN_PERIOD, node)) {
        parseTreeFree(node);
        return NULL;
    }

    rightConst=parseConstant(ps);
    if (rightConst==NULL || !parserAttachChild(ps, node, rightConst)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseEnumerated(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<enumerated>");

    if (node==NULL) {
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_LPARENT) {
        parserRecordSyntaxErrorCurrent(ps, "Expected '(' to begin enumerated type.");
        parserSkipUntilTypeBoundary(ps);
        return node;
    }

    if (!parserExpectToken(ps, TOKEN_LPARENT, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserAcceptToken(ps, TOKEN_COMMA, node)) {
        if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_RPARENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseFieldPart(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<field-part>");
    ParseTreeNode *identifierListNode;
    ParseTreeNode *typeNode;

    if (node==NULL) {
        return NULL;
    }

    identifierListNode=parseIdentifierList(ps);
    if (identifierListNode==NULL || !parserAttachChild(ps, node, identifierListNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_COLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    typeNode=parseType(ps);
    if (typeNode==NULL || !parserAttachChild(ps, node, typeNode)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseFieldList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<field-list>");
    ParseTreeNode *fieldPart;

    if (node==NULL) {
        return NULL;
    }

    fieldPart=parseFieldPart(ps);
    if (fieldPart==NULL || !parserAttachChild(ps, node, fieldPart)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserCurrentType(ps)!=TOKEN_ENDSY && parserCurrentType(ps)!=TOKEN_EOF) {
        if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
            if (parserPeekType(ps, 1)==TOKEN_ENDSY) {
                if (!parserExpectToken(ps, TOKEN_SEMICOLON, NULL)) {
                    parseTreeFree(node);
                    return NULL;
                }
                break;
            }

            if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
                parseTreeFree(node);
                return NULL;
            }
        }
        else if (parserCurrentType(ps)==TOKEN_IDENT) {
            parserRecordSyntaxErrorCurrent(ps, "Expected semicolon before next field declaration.");
        }
        else {
            parserRecordSyntaxErrorCurrent(ps, "Unexpected token in record field list.");
            parserSkipUntilTypeBoundary(ps);
            if (parserCurrentType(ps)==TOKEN_ENDSY || parserCurrentType(ps)==TOKEN_EOF) {
                break;
            }
            if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
                continue;
            }
        }

        if (parserCurrentType(ps)!=TOKEN_IDENT) {
            continue;
        }

        fieldPart=parseFieldPart(ps);
        if (fieldPart==NULL || !parserAttachChild(ps, node, fieldPart)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseRecordType(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<record-type>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_RECORDSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_ENDSY) {
        ParseTreeNode *fieldListNode=parseFieldList(ps);
        if (fieldListNode==NULL || !parserAttachChild(ps, node, fieldListNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_ENDSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseArrayType(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<array-type>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_ARRAYSY, node) ||
        !parserExpectToken(ps, TOKEN_LBRACK, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_IDENT &&
        parserPeekType(ps, 1)!=TOKEN_PERIOD) {
        if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }
    else {
        ParseTreeNode *rangeNode=parseRange(ps);
        if (rangeNode==NULL || !parserAttachChild(ps, node, rangeNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_RBRACK, node) ||
        !parserExpectToken(ps, TOKEN_OFSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    {
        ParseTreeNode *typeNode=parseType(ps);
        if (typeNode==NULL || !parserAttachChild(ps, node, typeNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseType(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<type>");
    ParseTreeNode *child;
    TokenType current=parserCurrentType(ps);

    if (node==NULL) {
        return NULL;
    }

    if (current==TOKEN_ARRAYSY) {
        child=parseArrayType(ps);
    }
    else if (current==TOKEN_RECORDSY) {
        child=parseRecordType(ps);
    }
    else if (current==TOKEN_LPARENT || current==TOKEN_LBRACK) {
        child=parseEnumerated(ps);
    }
    else if (hasRangeOperatorAheadInType(ps)) {
        child=parseRange(ps);
    }
    else if (current==TOKEN_IDENT) {
        child=NULL;
        if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
        return node;
    }
    else {
        parserRecordSyntaxErrorCurrent(ps, "Expected type.");
        parserSkipUntilTypeBoundary(ps);
        return node;
    }

    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseTypeDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<type-declaration>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_TYPESY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_IDENT) {
        parserRecordSyntaxErrorCurrent(ps, "Expected identifier after typesy.");
        parserSkipUntilTypeBoundary(ps);
        return node;
    }

    while (parserCurrentType(ps)==TOKEN_IDENT) {
        ParseTreeNode *typeNode;

        if (!parserExpectToken(ps, TOKEN_IDENT, node) ||
            !parserExpectToken(ps, TOKEN_EQL, node)) {
            parseTreeFree(node);
            return NULL;
        }

        typeNode=parseType(ps);
        if (typeNode==NULL || !parserAttachChild(ps, node, typeNode)) {
            parseTreeFree(node);
            return NULL;
        }

        if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseVarDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<var-declaration>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_VARSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_IDENT) {
        parserRecordSyntaxErrorCurrent(ps, "Expected identifier after varsy.");
        parserSkipUntilTypeBoundary(ps);
        return node;
    }

    while (parserCurrentType(ps)==TOKEN_IDENT) {
        ParseTreeNode *identifierListNode;
        ParseTreeNode *typeNode;

        identifierListNode=parseIdentifierList(ps);
        if (identifierListNode==NULL || !parserAttachChild(ps, node, identifierListNode)) {
            parseTreeFree(node);
            return NULL;
        }

        if (!parserExpectToken(ps, TOKEN_COLON, node)) {
            parseTreeFree(node);
            return NULL;
        }

        typeNode=parseType(ps);
        if (typeNode==NULL || !parserAttachChild(ps, node, typeNode)) {
            parseTreeFree(node);
            return NULL;
        }

        if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseParameterGroup(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<parameter-group>");
    ParseTreeNode *identifierListNode;

    if (node==NULL) {
        return NULL;
    }

    identifierListNode=parseIdentifierList(ps);
    if (identifierListNode==NULL || !parserAttachChild(ps, node, identifierListNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_COLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_ARRAYSY) {
        ParseTreeNode *arrayTypeNode=parseArrayType(ps);
        if (arrayTypeNode==NULL || !parserAttachChild(ps, node, arrayTypeNode)) {
            parseTreeFree(node);
            return NULL;
        }

        return node;
    }

    if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseFormalParameterList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<formal-parameter-list>");
    ParseTreeNode *parameterGroup;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_LPARENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    parameterGroup=parseParameterGroup(ps);
    if (parameterGroup==NULL || !parserAttachChild(ps, node, parameterGroup)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserCurrentType(ps)!=TOKEN_RPARENT && parserCurrentType(ps)!=TOKEN_EOF) {
        if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
            if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
                parseTreeFree(node);
                return NULL;
            }
        }
        else if (parserCurrentType(ps)==TOKEN_IDENT) {
            parserRecordSyntaxErrorCurrent(ps, "Expected semicolon before next parameter group.");
        }
        else {
            parserRecordSyntaxErrorCurrent(ps, "Unexpected token in parameter list.");
            parserSkipUntilTypeBoundary(ps);
            if (parserCurrentType(ps)==TOKEN_RPARENT || parserCurrentType(ps)==TOKEN_EOF) {
                break;
            }
            if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
                continue;
            }
        }

        if (parserCurrentType(ps)!=TOKEN_IDENT) {
            continue;
        }

        parameterGroup=parseParameterGroup(ps);
        if (parameterGroup==NULL || !parserAttachChild(ps, node, parameterGroup)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_RPARENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseBlock(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "block");
    ParseTreeNode *declarationPartNode;
    ParseTreeNode *compoundStatementNode;

    if (node==NULL) {
        return NULL;
    }

    declarationPartNode=parseDeclarationPart(ps);
    if (declarationPartNode==NULL || !parserAttachChild(ps, node, declarationPartNode)) {
        parseTreeFree(node);
        return NULL;
    }

    compoundStatementNode=parseCompoundStatement(ps);
    if (compoundStatementNode==NULL || !parserAttachChild(ps, node, compoundStatementNode)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseProcedureDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<procedure-declaration>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_PROCEDURESY, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_LPARENT) {
        ParseTreeNode *parameterListNode=parseFormalParameterList(ps);
        if (parameterListNode==NULL || !parserAttachChild(ps, node, parameterListNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    {
        ParseTreeNode *blockNode=parseBlock(ps);
        if (blockNode==NULL || !parserAttachChild(ps, node, blockNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseFunctionDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<function-declaration>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_FUNCTIONSY, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_LPARENT) {
        ParseTreeNode *parameterListNode=parseFormalParameterList(ps);
        if (parameterListNode==NULL || !parserAttachChild(ps, node, parameterListNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_COLON, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node) ||
        !parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    {
        ParseTreeNode *blockNode=parseBlock(ps);
        if (blockNode==NULL || !parserAttachChild(ps, node, blockNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseSubprogramDeclaration(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<subprogram-declaration>");
    ParseTreeNode *child;

    if (node==NULL) {
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_PROCEDURESY) {
        child=parseProcedureDeclaration(ps);
    }
    else if (parserCurrentType(ps)==TOKEN_FUNCTIONSY) {
        child=parseFunctionDeclaration(ps);
    }
    else {
        parserRecordSyntaxErrorCurrent(ps, "Expected subprogram declaration.");
        parserSkipUntilStatementBoundary(ps);
        return node;
    }

    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseDeclarationPart(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<declaration-part>");

    if (node==NULL) {
        return NULL;
    }

    while (parserCurrentType(ps)==TOKEN_CONSTSY) {
        ParseTreeNode *constNode=parseConstDeclaration(ps);
        if (constNode==NULL || !parserAttachChild(ps, node, constNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    while (parserCurrentType(ps)==TOKEN_TYPESY) {
        ParseTreeNode *typeNode=parseTypeDeclaration(ps);
        if (typeNode==NULL || !parserAttachChild(ps, node, typeNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    while (parserCurrentType(ps)==TOKEN_VARSY) {
        ParseTreeNode *varNode=parseVarDeclaration(ps);
        if (varNode==NULL || !parserAttachChild(ps, node, varNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    while (parserCurrentType(ps)==TOKEN_PROCEDURESY ||
           parserCurrentType(ps)==TOKEN_FUNCTIONSY) {
        ParseTreeNode *subprogramNode=parseSubprogramDeclaration(ps);
        if (subprogramNode==NULL || !parserAttachChild(ps, node, subprogramNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseIndexList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<index-list>");
    TokenType current;

    if (node==NULL) {
        return NULL;
    }

    current=parserCurrentType(ps);
    if (current!=TOKEN_INTCON && current!=TOKEN_CHARCON && current!=TOKEN_IDENT) {
        parserRecordSyntaxErrorCurrent(ps, "Expected index (intcon, charcon, or ident).");
        return node;
    }

    if (!parserExpectToken(ps, current, node)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserAcceptToken(ps, TOKEN_COMMA, node)) {
        ParseTreeNode *subList=parseIndexList(ps);
        if (subList==NULL || !parserAttachChild(ps, node, subList)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseComponentVariable(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<component-variable>");

    if (node==NULL) {
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_LBRACK) {
        ParseTreeNode *indexListNode;

        if (!parserExpectToken(ps, TOKEN_LBRACK, node)) {
            parseTreeFree(node);
            return NULL;
        }

        indexListNode=parseIndexList(ps);
        if (indexListNode==NULL || !parserAttachChild(ps, node, indexListNode)) {
            parseTreeFree(node);
            return NULL;
        }

        if (!parserExpectToken(ps, TOKEN_RBRACK, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }
    else if (parserCurrentType(ps)==TOKEN_PERIOD) {
        if (!parserExpectToken(ps, TOKEN_PERIOD, node) ||
            !parserExpectToken(ps, TOKEN_IDENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }
    else {
        parserRecordSyntaxErrorCurrent(ps, "Expected '[' or '.' for component-variable.");
        return node;
    }

    return node;
}

static ParseTreeNode *parseVariable(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<variable>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserCurrentType(ps)==TOKEN_LBRACK || parserCurrentType(ps)==TOKEN_PERIOD) {
        if (parserCurrentType(ps)==TOKEN_PERIOD && parserPeekType(ps, 1)==TOKEN_PERIOD) {
            break;
        }
        ParseTreeNode *compVar=parseComponentVariable(ps);
        if (compVar==NULL || !parserAttachChild(ps, node, compVar)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseProcedureFunctionCall(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<procedure/function-call>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_IDENT, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserAcceptToken(ps, TOKEN_LPARENT, node)) {
        if (parserCurrentType(ps)!=TOKEN_RPARENT) {
            ParseTreeNode *parameterListNode=parseParameterList(ps);
            if (parameterListNode==NULL || !parserAttachChild(ps, node, parameterListNode)) {
                parseTreeFree(node);
                return NULL;
            }
        }

        if (!parserExpectToken(ps, TOKEN_RPARENT, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseFactor(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<factor>");
    TokenType current=parserCurrentType(ps);

    if (node==NULL) {
        return NULL;
    }

    if (current==TOKEN_IDENT) {
        TokenType next=parserPeekType(ps, 1);

        if (next==TOKEN_LPARENT) {
            ParseTreeNode *callNode=parseProcedureFunctionCall(ps);
            if (callNode==NULL || !parserAttachChild(ps, node, callNode)) {
                parseTreeFree(node);
                return NULL;
            }
            return node;
        }

        if (next==TOKEN_LBRACK ||
            (next==TOKEN_PERIOD && parserPeekType(ps, 2)!=TOKEN_PERIOD)) {
            ParseTreeNode *varNode=parseVariable(ps);
            if (varNode==NULL || !parserAttachChild(ps, node, varNode)) {
                parseTreeFree(node);
                return NULL;
            }
            return node;
        }

        {
            ParseTreeNode *varNode=parseVariable(ps);
            if (varNode==NULL || !parserAttachChild(ps, node, varNode)) {
                parseTreeFree(node);
                return NULL;
            }
            return node;
        }
    }

    switch (current) {
        case TOKEN_INTCON:
        case TOKEN_REALCON:
        case TOKEN_CHARCON:
        case TOKEN_STRING:
            if (!parserExpectToken(ps, current, node)) {
                parseTreeFree(node);
                return NULL;
            }
            return node;
        case TOKEN_LPARENT: {
            ParseTreeNode *exprNode;

            if (!parserExpectToken(ps, TOKEN_LPARENT, node)) {
                parseTreeFree(node);
                return NULL;
            }

            exprNode=parseExpression(ps);
            if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
                parseTreeFree(node);
                return NULL;
            }

            if (!parserExpectToken(ps, TOKEN_RPARENT, node)) {
                parseTreeFree(node);
                return NULL;
            }

            return node;
        }
        case TOKEN_NOTSY: {
            ParseTreeNode *factorNode;

            if (!parserExpectToken(ps, TOKEN_NOTSY, node)) {
                parseTreeFree(node);
                return NULL;
            }

            factorNode=parseFactor(ps);
            if (factorNode==NULL || !parserAttachChild(ps, node, factorNode)) {
                parseTreeFree(node);
                return NULL;
            }

            return node;
        }
        default:
            parserRecordSyntaxErrorCurrent(ps, "Expected factor.");
            if (!isExpressionStart(parserCurrentType(ps))) {
                parserSkipUntilExpressionBoundary(ps);
            }
            return node;
    }
}

static ParseTreeNode *parseTerm(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<term>");
    ParseTreeNode *factorNode;

    if (node==NULL) {
        return NULL;
    }

    factorNode=parseFactor(ps);
    if (factorNode==NULL || !parserAttachChild(ps, node, factorNode)) {
        parseTreeFree(node);
        return NULL;
    }

    while (isMultiplicativeOperator(parserCurrentType(ps))) {
        ParseTreeNode *opNode=parserCreateNode(ps, "<multiplicative-operator>");
        TokenType op=parserCurrentType(ps);

        if (opNode==NULL || !parserExpectToken(ps, op, opNode)) {
            parseTreeFree(opNode);
            parseTreeFree(node);
            return NULL;
        }

        if (!parserAttachChild(ps, node, opNode)) {
            parseTreeFree(node);
            return NULL;
        }

        factorNode=parseFactor(ps);
        if (factorNode==NULL || !parserAttachChild(ps, node, factorNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseSimpleExpression(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<simple-expression>");
    ParseTreeNode *termNode;

    if (node==NULL) {
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_PLUS || parserCurrentType(ps)==TOKEN_MINUS) {
        TokenType unary=parserCurrentType(ps);

        if (!parserExpectToken(ps, unary, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    termNode=parseTerm(ps);
    if (termNode==NULL || !parserAttachChild(ps, node, termNode)) {
        parseTreeFree(node);
        return NULL;
    }

    while (isAdditiveOperator(parserCurrentType(ps))) {
        ParseTreeNode *opNode=parserCreateNode(ps, "<additive-operator>");
        TokenType op=parserCurrentType(ps);

        if (opNode==NULL || !parserExpectToken(ps, op, opNode)) {
            parseTreeFree(opNode);
            parseTreeFree(node);
            return NULL;
        }

        if (!parserAttachChild(ps, node, opNode)) {
            parseTreeFree(node);
            return NULL;
        }

        termNode=parseTerm(ps);
        if (termNode==NULL || !parserAttachChild(ps, node, termNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseExpression(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<expression>");
    ParseTreeNode *simpleExpr;

    if (node==NULL) {
        return NULL;
    }

    simpleExpr=parseSimpleExpression(ps);
    if (simpleExpr==NULL || !parserAttachChild(ps, node, simpleExpr)) {
        parseTreeFree(node);
        return NULL;
    }

    if (isRelationalOperator(parserCurrentType(ps))) {
        ParseTreeNode *opNode=parserCreateNode(ps, "<relational-operator>");
        TokenType op=parserCurrentType(ps);

        if (opNode==NULL || !parserExpectToken(ps, op, opNode)) {
            parseTreeFree(opNode);
            parseTreeFree(node);
            return NULL;
        }

        if (!parserAttachChild(ps, node, opNode)) {
            parseTreeFree(node);
            return NULL;
        }

        simpleExpr=parseSimpleExpression(ps);
        if (simpleExpr==NULL || !parserAttachChild(ps, node, simpleExpr)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseParameterList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<parameter-list>");
    ParseTreeNode *exprNode;

    if (node==NULL) {
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserCurrentType(ps)!=TOKEN_RPARENT && parserCurrentType(ps)!=TOKEN_EOF) {
        if (parserCurrentType(ps)==TOKEN_COMMA) {
            if (!parserExpectToken(ps, TOKEN_COMMA, node)) {
                parseTreeFree(node);
                return NULL;
            }
        }
        else if (isExpressionStart(parserCurrentType(ps))) {
            parserRecordSyntaxErrorCurrent(ps, "Expected comma before next parameter.");
        }
        else {
            break;
        }

        exprNode=parseExpression(ps);
        if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseAssignmentStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<assignment-statement>");
    ParseTreeNode *varNode;
    ParseTreeNode *exprNode;

    if (node==NULL) {
        return NULL;
    }

    varNode=parseVariable(ps);
    if (varNode==NULL || !parserAttachChild(ps, node, varNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_BECOMES, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseIfStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<if-statement>");
    ParseTreeNode *exprNode;
    ParseTreeNode *stmtNode;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_IFSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_THENSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    stmtNode=parseStatement(ps);
    if (stmtNode==NULL || !parserAttachChild(ps, node, stmtNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserAcceptToken(ps, TOKEN_ELSESY, node)) {
        stmtNode=parseStatement(ps);
        if (stmtNode==NULL || !parserAttachChild(ps, node, stmtNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseCaseBlock(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<case-block>");
    ParseTreeNode *constantNode;
    ParseTreeNode *statementNode;

    if (node==NULL) {
        return NULL;
    }

    constantNode=parseConstant(ps);
    if (constantNode==NULL || !parserAttachChild(ps, node, constantNode)) {
        parseTreeFree(node);
        return NULL;
    }

    while (parserAcceptToken(ps, TOKEN_COMMA, node)) {
        constantNode=parseConstant(ps);
        if (constantNode==NULL || !parserAttachChild(ps, node, constantNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_COLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    statementNode=parseStatement(ps);
    if (statementNode==NULL || !parserAttachChild(ps, node, statementNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
        if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
            parseTreeFree(node);
            return NULL;
        }

        if (isConstantStart(parserCurrentType(ps))) {
            ParseTreeNode *nextBlock=parseCaseBlock(ps);
            if (nextBlock==NULL || !parserAttachChild(ps, node, nextBlock)) {
                parseTreeFree(node);
                return NULL;
            }
        }
        else if (parserCurrentType(ps)!=TOKEN_ENDSY && parserCurrentType(ps)!=TOKEN_EOF) {
            parserRecordSyntaxErrorCurrent(ps, "Expected case label or end after semicolon.");
            parserSkipUntilStatementBoundary(ps);
        }
    }

    return node;
}

static ParseTreeNode *parseCaseStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<case-statement>");
    ParseTreeNode *exprNode;
    ParseTreeNode *caseBlockNode;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_CASESY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_OFSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    caseBlockNode=parseCaseBlock(ps);
    if (caseBlockNode==NULL || !parserAttachChild(ps, node, caseBlockNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_ENDSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseWhileStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<while-statement>");
    ParseTreeNode *exprNode;
    ParseTreeNode *compoundNode;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_WHILESY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_DOSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_BEGINSY) {
        parserRecordSyntaxErrorCurrent(ps, "Expected 'begin' (compound-statement) after 'do' in while-statement.");
    }

    compoundNode=parseCompoundStatement(ps);
    if (compoundNode==NULL || !parserAttachChild(ps, node, compoundNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseRepeatStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<repeat-statement>");
    ParseTreeNode *stmtListNode;
    ParseTreeNode *exprNode;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_REPEATSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    stmtListNode=parseStatementList(ps);
    if (stmtListNode==NULL || !parserAttachChild(ps, node, stmtListNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_UNTILSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseForStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<for-statement>");
    ParseTreeNode *exprNode;
    ParseTreeNode *compoundNode;

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_FORSY, node) ||
        !parserExpectToken(ps, TOKEN_IDENT, node) ||
        !parserExpectToken(ps, TOKEN_BECOMES, node)) {
        parseTreeFree(node);
        return NULL;
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)==TOKEN_TOSY) {
        if (!parserExpectToken(ps, TOKEN_TOSY, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }
    else if (parserCurrentType(ps)==TOKEN_DOWNTOSY) {
        if (!parserExpectToken(ps, TOKEN_DOWNTOSY, node)) {
            parseTreeFree(node);
            return NULL;
        }
    }
    else {
        parserRecordSyntaxErrorCurrent(ps, "Expected tosy or downtosy in for-statement.");
    }

    exprNode=parseExpression(ps);
    if (exprNode==NULL || !parserAttachChild(ps, node, exprNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_DOSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    /* Spesifikasi M3: for-statement wajib compound-statement */
    if (parserCurrentType(ps)!=TOKEN_BEGINSY) {
        parserRecordSyntaxErrorCurrent(ps, "Expected 'begin' (compound-statement) after 'do' in for-statement.");
    }

    compoundNode=parseCompoundStatement(ps);
    if (compoundNode==NULL || !parserAttachChild(ps, node, compoundNode)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<statement>");
    ParseTreeNode *child;
    TokenType current=parserCurrentType(ps);

    if (node==NULL) {
        return NULL;
    }

    switch (current) {
        case TOKEN_IDENT:
            if (hasBecomesAfterVariable(ps)) {
                child=parseAssignmentStatement(ps);
            }
            else {
                child=parseProcedureFunctionCall(ps);
            }
            break;
        case TOKEN_IFSY:
            child=parseIfStatement(ps);
            break;
        case TOKEN_CASESY:
            child=parseCaseStatement(ps);
            break;
        case TOKEN_WHILESY:
            child=parseWhileStatement(ps);
            break;
        case TOKEN_REPEATSY:
            child=parseRepeatStatement(ps);
            break;
        case TOKEN_FORSY:
            child=parseForStatement(ps);
            break;
        case TOKEN_BEGINSY:
            child=parseCompoundStatement(ps);
            break;
        default:
            parserRecordSyntaxErrorCurrent(ps, "Expected statement.");
            parserSkipUntilStatementBoundary(ps);
            return node;
    }

    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseStatementList(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<statement-list>");
    ParseTreeNode *stmtNode;

    if (node==NULL) {
        return NULL;
    }

    if (isStatementListTerminator(parserCurrentType(ps))) {
        return node;
    }

    if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
        if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
            parseTreeFree(node);
            return NULL;
        }
        if (isStatementListTerminator(parserCurrentType(ps))) {
            return node;
        }
    }

    if (!isStatementStart(parserCurrentType(ps))) {
        parserRecordSyntaxErrorCurrent(ps, "Expected statement-list.");
        parserSkipUntilStatementBoundary(ps);
        return node;
    }

    stmtNode=parseStatement(ps);
    if (stmtNode==NULL || !parserAttachChild(ps, node, stmtNode)) {
        parseTreeFree(node);
        return NULL;
    }

    while (!isStatementListTerminator(parserCurrentType(ps))) {
        if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
            if (!parserExpectToken(ps, TOKEN_SEMICOLON, node)) {
                parseTreeFree(node);
                return NULL;
            }

            if (isStatementListTerminator(parserCurrentType(ps))) {
                break;
            }

            if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
                continue;
            }

            if (!isStatementStart(parserCurrentType(ps))) {
                parserRecordSyntaxErrorCurrent(ps, "Expected statement after semicolon.");
                parserSkipUntilStatementBoundary(ps);
                continue;
            }
        }
        else if (isStatementStart(parserCurrentType(ps))) {
            parserRecordSyntaxErrorCurrent(ps, "Expected semicolon before next statement.");
        }
        else {
            parserRecordSyntaxErrorCurrent(ps, "Unexpected token in statement-list.");
            parserSkipUntilStatementBoundary(ps);
            if (isStatementListTerminator(parserCurrentType(ps))) {
                break;
            }
            if (parserCurrentType(ps)==TOKEN_SEMICOLON) {
                continue;
            }
        }

        if (!isStatementStart(parserCurrentType(ps))) {
            continue;
        }

        stmtNode=parseStatement(ps);
        if (stmtNode==NULL || !parserAttachChild(ps, node, stmtNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    return node;
}

static ParseTreeNode *parseCompoundStatement(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<compound-statement>");

    if (node==NULL) {
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_BEGINSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    if (parserCurrentType(ps)!=TOKEN_ENDSY) {
        ParseTreeNode *statementListNode=parseStatementList(ps);
        if (statementListNode==NULL || !parserAttachChild(ps, node, statementListNode)) {
            parseTreeFree(node);
            return NULL;
        }
    }

    if (!parserExpectToken(ps, TOKEN_ENDSY, node)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static ParseTreeNode *parseProgram(Parser *ps) {
    ParseTreeNode *node=parserCreateNode(ps, "<program>");
    ParseTreeNode *child;

    if (node==NULL) {
        return NULL;
    }

    child=parseProgramHeader(ps);
    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    child=parseDeclarationPart(ps);
    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    child=parseCompoundStatement(ps);
    if (child==NULL || !parserAttachChild(ps, node, child)) {
        parseTreeFree(node);
        return NULL;
    }

    if (!parserExpectToken(ps, TOKEN_PERIOD, node) ||
        !parserExpectToken(ps, TOKEN_EOF, NULL)) {
        parseTreeFree(node);
        return NULL;
    }

    return node;
}

static char *grammarCloneString(Parser *ps, const char *src) {
    char *copy;
    size_t len;

    if (src==NULL) {
        return NULL;
    }

    len=strlen(src);
    copy=(char *)malloc(len + 1);
    if (copy==NULL) {
        parserSetError(ps, "Out of memory while building grammar validation tree.");
        return NULL;
    }

    memcpy(copy, src, len + 1);
    return copy;
}

static Node *grammarCreateNode(Parser *ps, const char *label) {
    Node *node;

    node=(Node *)malloc(sizeof(Node));
    if (node==NULL) {
        parserSetError(ps, "Out of memory while building grammar validation tree.");
        return NULL;
    }

    node->name=grammarCloneString(ps, label);
    if (node->name==NULL) {
        free(node);
        return NULL;
    }

    node->child=NULL;
    node->childCount=0;
    node->childCapacity=0;
    return node;
}

static bool grammarAttachChild(Parser *ps, Node *parent, Node *child) {
    Node **newChildren;
    int newCapacity;

    if (parent==NULL || child==NULL) {
        return false;
    }

    if (parent->childCount==parent->childCapacity) {
        newCapacity=parent->childCapacity==0 ? 4 : parent->childCapacity * 2;
        newChildren=(Node **)realloc(parent->child, (size_t)newCapacity * sizeof(Node *));
        if (newChildren==NULL) {
            grammarFreeNode(child);
            return parserSetError(ps, "Out of memory while building grammar validation tree.");
        }

        parent->child=newChildren;
        parent->childCapacity=newCapacity;
    }

    parent->child[parent->childCount]=child;
    parent->childCount++;
    return true;
}

static void grammarFreeNode(Node *node) {
    int i;

    if (node==NULL) {
        return;
    }

    for (i=0; i < node->childCount; i++) {
        grammarFreeNode(node->child[i]);
    }

    free(node->child);
    free(node->name);
    free(node);
}

static bool parseTreeLabelEquals(ParseTreeNode *node, const char *label) {
    return node!=NULL && label!=NULL && strcmp(node->label, label)==0;
}

static bool parseTreeIsStatementWrapper(ParseTreeNode *node) {
    return parseTreeLabelEquals(node, "statement") || parseTreeLabelEquals(node, "<statement>");
}

static Node *convertParseTreeNodeDefault(Parser *ps, ParseTreeNode *src);
static Node *convertExpressionNode(Parser *ps, ParseTreeNode *src);
static Node *convertAsGrammarStatement(Parser *ps, ParseTreeNode *src);
static Node *convertStatementListNode(Parser *ps, ParseTreeNode *src);
static Node *convertIfStatementNode(Parser *ps, ParseTreeNode *src);
static Node *convertWhileStatementNode(Parser *ps, ParseTreeNode *src);
static Node *convertForStatementNode(Parser *ps, ParseTreeNode *src);
static Node *convertCaseBlockNode(Parser *ps, ParseTreeNode *src);
static Node *convertCaseStatementNode(Parser *ps, ParseTreeNode *src);
static Node *convertProcedureDeclarationNode(Parser *ps, ParseTreeNode *src);

static Node *convertParseTreeToGrammarNode(Parser *ps, ParseTreeNode *src) {
    if (src==NULL) {
        return NULL;
    }

    if (parseTreeLabelEquals(src, "<expression>")) {
        return convertExpressionNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<simple-expression>")) {
        return convertParseTreeNodeDefault(ps, src);
    }

    if (parseTreeLabelEquals(src, "<term>")) {
        return convertParseTreeNodeDefault(ps, src);
    }

    if (parseTreeLabelEquals(src, "<statement-list>")) {
        return convertStatementListNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<if-statement>")) {
        return convertIfStatementNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<while-statement>")) {
        return convertWhileStatementNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<for-statement>")) {
        return convertForStatementNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<case-block>")) {
        return convertCaseBlockNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<repeat-statement>")) {
        return convertParseTreeNodeDefault(ps, src);
    }

    if (parseTreeLabelEquals(src, "<case-statement>")) {
        return convertCaseStatementNode(ps, src);
    }

    if (parseTreeLabelEquals(src, "<procedure-declaration>")) {
        return convertProcedureDeclarationNode(ps, src);
    }

    return convertParseTreeNodeDefault(ps, src);
}

static Node *convertParseTreeNodeDefault(Parser *ps, ParseTreeNode *src) {
    Node *dst;
    size_t i;

    dst=grammarCreateNode(ps, src->label);
    if (dst==NULL) {
        return NULL;
    }

    for (i=0; i < src->childCount; i++) {
        Node *child=convertParseTreeToGrammarNode(ps, src->children[i]);

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertExpressionNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, src->label);

    if (dst==NULL) {
        return NULL;
    }

    if (src->childCount==3 &&
        !parseTreeLabelEquals(src->children[1], "<relational-operator>")) {
        Node *left=convertParseTreeToGrammarNode(ps, src->children[0]);
        Node *opWrapper=grammarCreateNode(ps, "<relational-operator>");
        Node *opLeaf;
        Node *right;

        if (left==NULL || opWrapper==NULL || !grammarAttachChild(ps, dst, left)) {
            grammarFreeNode(opWrapper);
            grammarFreeNode(dst);
            return NULL;
        }

        opLeaf=convertParseTreeToGrammarNode(ps, src->children[1]);
        if (opLeaf==NULL || !grammarAttachChild(ps, opWrapper, opLeaf) || !grammarAttachChild(ps, dst, opWrapper)) {
            grammarFreeNode(dst);
            return NULL;
        }

        right=convertParseTreeToGrammarNode(ps, src->children[2]);
        if (right==NULL || !grammarAttachChild(ps, dst, right)) {
            grammarFreeNode(dst);
            return NULL;
        }

        return dst;
    }

    grammarFreeNode(dst);
    return convertParseTreeNodeDefault(ps, src);
}

static Node *convertAsGrammarStatement(Parser *ps, ParseTreeNode *src) {
    Node *wrapper;
    Node *content;

    if (src==NULL) {
        return NULL;
    }

    if (parseTreeIsStatementWrapper(src)) {
        return convertParseTreeToGrammarNode(ps, src);
    }

    wrapper=grammarCreateNode(ps, "statement");
    if (wrapper==NULL) {
        return NULL;
    }

    content=convertParseTreeToGrammarNode(ps, src);
    if (content==NULL || !grammarAttachChild(ps, wrapper, content)) {
        grammarFreeNode(content);
        grammarFreeNode(wrapper);
        return NULL;
    }

    return wrapper;
}

static Node *convertStatementListNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<statement-list>");
    size_t i;

    if (dst==NULL) {
        return NULL;
    }

    for (i=0; i < src->childCount; i++) {
        Node *child;

        if (parseTreeLabelEquals(src->children[i], "semicolon")) {
            child=convertParseTreeToGrammarNode(ps, src->children[i]);
        }
        else {
            child=convertAsGrammarStatement(ps, src->children[i]);
        }

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(child);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertIfStatementNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<if-statement>");
    size_t i;

    if (dst==NULL) {
        return NULL;
    }

    for (i=0; i < src->childCount; i++) {
        Node *child;
        bool statementPos=(i==3 || i==5);

        if (statementPos) {
            child=convertAsGrammarStatement(ps, src->children[i]);
        }
        else {
            child=convertParseTreeToGrammarNode(ps, src->children[i]);
        }

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(child);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertWhileStatementNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<while-statement>");
    size_t i;

    if (dst==NULL) {
        return NULL;
    }

    /* Struktur M3: whilesy expr dosy compound-statement semicolon */
    for (i=0; i < src->childCount; i++) {
        Node *child=convertParseTreeToGrammarNode(ps, src->children[i]);

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(child);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertForStatementNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<for-statement>");
    size_t i;

    if (dst==NULL) {
        return NULL;
    }

    /* Struktur M3: forsy ident becomes expr (tosy|downtosy) expr dosy compound-statement semicolon */
    for (i=0; i < src->childCount; i++) {
        Node *child=convertParseTreeToGrammarNode(ps, src->children[i]);

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(child);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertCaseBlockNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<case-block>");
    size_t i;
    size_t colonIdx=src->childCount;

    if (dst==NULL) {
        return NULL;
    }

    for (i=0; i < src->childCount; i++) {
        if (parseTreeLabelEquals(src->children[i], "colon")) {
            colonIdx=i;
            break;
        }
    }

    for (i=0; i < src->childCount; i++) {
        Node *child;
        bool statementPos=(colonIdx < src->childCount && i==colonIdx + 1);

        if (statementPos) {
            child=convertAsGrammarStatement(ps, src->children[i]);
        }
        else {
            child=convertParseTreeToGrammarNode(ps, src->children[i]);
        }

        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(child);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertCaseBlockChain(Parser *ps, ParseTreeNode *src, size_t idx, size_t endIdx, bool *ok) {
    Node *head;

    *ok=false;

    if (idx > endIdx || !parseTreeLabelEquals(src->children[idx], "<case-block>")) {
        return NULL;
    }

    head=convertParseTreeToGrammarNode(ps, src->children[idx]);
    if (head==NULL) {
        return NULL;
    }

    if (idx==endIdx) {
        *ok=true;
        return head;
    }

    if (idx + 1 > endIdx || !parseTreeLabelEquals(src->children[idx + 1], "semicolon")) {
        grammarFreeNode(head);
        return NULL;
    }

    {
        Node *semicolonNode=convertParseTreeToGrammarNode(ps, src->children[idx + 1]);
        if (semicolonNode==NULL || !grammarAttachChild(ps, head, semicolonNode)) {
            grammarFreeNode(semicolonNode);
            grammarFreeNode(head);
            return NULL;
        }
    }

    if (idx + 1==endIdx) {
        *ok=true;
        return head;
    }

    {
        Node *tail=convertCaseBlockChain(ps, src, idx + 2, endIdx, ok);
        if (!(*ok) || tail==NULL || !grammarAttachChild(ps, head, tail)) {
            grammarFreeNode(tail);
            grammarFreeNode(head);
            return NULL;
        }
    }

    *ok=true;
    return head;
}

static Node *convertCaseStatementNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<case-statement>");
    Node *headCaseBlock;
    bool chainOk;
    size_t caseEndIdx;

    if (dst==NULL) {
        return NULL;
    }

    if (src->childCount < 5 ||
        !parseTreeLabelEquals(src->children[0], "casesy") ||
        !parseTreeLabelEquals(src->children[2], "ofsy") ||
        !parseTreeLabelEquals(src->children[src->childCount - 1], "endsy")) {
        grammarFreeNode(dst);
        return convertParseTreeNodeDefault(ps, src);
    }

    {
        Node *caseToken=convertParseTreeToGrammarNode(ps, src->children[0]);
        Node *exprNode=convertParseTreeToGrammarNode(ps, src->children[1]);
        Node *ofToken=convertParseTreeToGrammarNode(ps, src->children[2]);

        if (caseToken==NULL || exprNode==NULL || ofToken==NULL ||
            !grammarAttachChild(ps, dst, caseToken) ||
            !grammarAttachChild(ps, dst, exprNode) ||
            !grammarAttachChild(ps, dst, ofToken)) {
            grammarFreeNode(caseToken);
            grammarFreeNode(exprNode);
            grammarFreeNode(ofToken);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    caseEndIdx=src->childCount - 2;
    headCaseBlock=convertCaseBlockChain(ps, src, 3, caseEndIdx, &chainOk);
    if (!chainOk || headCaseBlock==NULL || !grammarAttachChild(ps, dst, headCaseBlock)) {
        grammarFreeNode(headCaseBlock);
        grammarFreeNode(dst);
        return NULL;
    }

    {
        Node *endToken=convertParseTreeToGrammarNode(ps, src->children[src->childCount - 1]);
        if (endToken==NULL || !grammarAttachChild(ps, dst, endToken)) {
            grammarFreeNode(endToken);
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static Node *convertProcedureDeclarationNode(Parser *ps, ParseTreeNode *src) {
    Node *dst=grammarCreateNode(ps, "<procedure-declaration>");
    size_t i;
    size_t limit=src->childCount;

    if (dst==NULL) {
        return NULL;
    }

    if (src->childCount >= 5 && parseTreeLabelEquals(src->children[src->childCount - 1], "semicolon")) {
        limit=src->childCount - 1;
    }

    for (i=0; i < limit; i++) {
        Node *child=convertParseTreeToGrammarNode(ps, src->children[i]);
        if (child==NULL || !grammarAttachChild(ps, dst, child)) {
            grammarFreeNode(dst);
            return NULL;
        }
    }

    return dst;
}

static bool validateWithGrammar(Parser *ps, ParseTreeNode *root) {
    Node *grammarRoot;
    bool complete;

    grammarRoot=convertParseTreeToGrammarNode(ps, root);
    if (grammarRoot==NULL) {
        return false;
    }

    complete=is_program_complete(grammarRoot);
    grammarFreeNode(grammarRoot);

    if (!complete) {
        return parserSetError(ps, "Grammar validation failed: parse tree does not match the grammar rules.");
    }

    return true;
}

bool analyzeSyntaxFile(const char *inputPath, SyntaxResult *result) {
    Parser parser;
    ParseTreeNode *root;

    if (result==NULL || inputPath==NULL) {
        return false;
    }

    result->success=false;
    result->tree=NULL;
    result->message[0]='\0';

    parser.tokens=NULL;
    parser.count=0;
    parser.capacity=0;
    parser.pos=0;
    parser.fatalError=false;
    parser.errorCount=0;
    parser.errorLength=0;
    parser.error[0]='\0';

    if (!loadTokensFromLexer(&parser, inputPath)) {
        (void)snprintf(
            result->message,
            sizeof(result->message),
            "%s",
            parser.error[0]!='\0' ? parser.error : "Invalid syntax."
        );
        parserDestroy(&parser);
        return false;
    }

    root=parseProgram(&parser);
    if (parser.fatalError || root==NULL) {
        (void)snprintf(
            result->message,
            sizeof(result->message),
            "%s",
            parser.error[0]!='\0' ? parser.error : "Invalid syntax."
        );
        parserDestroy(&parser);
        return false;
    }

    if (parser.errorCount > 0) {
        const char *prefix="Ditemukan error syntax:\n";
        size_t prefixLen=strlen(prefix);

        parseTreeFree(root);
        memcpy(result->message, prefix, prefixLen);
        result->message[prefixLen]='\0';
        if (prefixLen < sizeof(result->message) - 1) {
            size_t available=sizeof(result->message) - 1 - prefixLen;
            (void)strncat(result->message, parser.error, available);
        }
        parserDestroy(&parser);
        return false;
    }

    if (!validateWithGrammar(&parser, root)) {
        parseTreeFree(root);
        (void)snprintf(
            result->message,
            sizeof(result->message),
            "%s",
            parser.error[0]!='\0' ? parser.error : "Invalid syntax."
        );
        parserDestroy(&parser);
        return false;
    }

    result->success=true;
    result->tree=root;
    (void)snprintf(result->message, sizeof(result->message), "Syntax analysis successful.");

    parserDestroy(&parser);
    return true;
}

void freeSyntaxResult(SyntaxResult *result) {
    if (result==NULL) {
        return;
    }

    parseTreeFree(result->tree);
    result->tree=NULL;
}
