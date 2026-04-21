#include "lexer.h"

#include <ctype.h>
#include <string.h>

#include "../mesinkarakter/mesinkarakter.h"

static void setToken(Token *t, TokenType type, const char *lexeme) {
    size_t n;

    t->type=type;
    n=strlen(lexeme);
    if (n >= MAX_LEXEME) {
        n=MAX_LEXEME - 1;
    }

    memcpy(t->lexeme, lexeme, n);
    t->lexeme[n]='\0';
}

static void setUnknownToken(Token *t, const char *message) {
    setToken(t, TOKEN_UNKNOWN, message);
}

static void appendChar(char *buf, int *index, char c) {
    if (*index < MAX_LEXEME - 1) {
        buf[*index]=c;
        (*index)++;
    }
}

static bool isLetter(char c) {
    return isalpha((unsigned char)c)!=0;
}

static bool isDigitChar(char c) {
    return isdigit((unsigned char)c)!=0;
}

static bool isIdentifierStart(char c) {
    return isLetter(c);
}


static bool isIdentifierPart(char c) {
    return isLetter(c) || isDigitChar(c);
}

static bool isDefaultSeparatorChar(char c) {
    return c=='\0' || isspace((unsigned char)c);
}

static bool isQuoteBoundaryChar(char c) {
    if (isDefaultSeparatorChar(c)) {
        return true;
    }

    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
        case '[':
        case ']':
        case ',':
        case ';':
        case '.':
        case ':':
        case '<':
        case '>':
        case '=':
        case '{':
        case '}':
            return true;
        default:
            return false;
    }
}


static char lowerChar(char c) {
    return (char)tolower((unsigned char)c);
}

static void syncWithMachine(Lexer *lx) {
    lx->eof=EOP;
    if (EOP) {
        lx->current='\0';
    }
    else {
        lx->current=CC;
    }
}

void initLexer(Lexer *lx, const char *filename) {
    lx->ready=STARTFILE(filename);
    lx->state=STATE_START;

    if (lx->ready) {
        syncWithMachine(lx);
    }
    else {
        lx->eof=true;
        lx->current='\0';
    }
}

void closeLexer(Lexer *lx) {
    if (lx->ready) {
        CLOSE();
    }

    lx->ready=false;
    lx->eof=true;
    lx->current='\0';
    lx->state=STATE_START;
}

static void lexerAdvance(Lexer *lx) {
    if (lx->eof) {
        return;
    }

    ADV();
    syncWithMachine(lx);
}

static void consumeUntilDefaultSeparator(Lexer *lx, char *lexeme, int *idx) {
    while (!lx->eof && !isDefaultSeparatorChar(lx->current)) {
        appendChar(lexeme, idx, lx->current);
        lexerAdvance(lx);
    }
}

static bool currentIs(Lexer *lx, char expectedLower) {
    return !lx->eof && lowerChar(lx->current)==expectedLower;
}

static LexerState initialKeywordState(char firstChar) {
    switch (lowerChar(firstChar)) {
        case 'c':
            return STATE_C;
        case 't':
            return STATE_T;
        case 'v':
            return STATE_V;
        case 'f':
            return STATE_F;
        case 'p':
            return STATE_P;
        case 'a':
            return STATE_A;
        case 'r':
            return STATE_R;
        case 'b':
            return STATE_B;
        case 'i':
            return STATE_I;
        case 'n':
            return STATE_N;
        case 'm':
            return STATE_M;
        case 'w':
            return STATE_W;
        case 'e':
            return STATE_E;
        case 'u':
            return STATE_U;
        case 'o':
            return STATE_O;
        case 'd':
            return STATE_D;
        default:
            return STATE_IDENT;
    }
}

static void keywordStepOrFallback(
    Lexer *lx,
    char *lexeme,
    int *idx,
    char expectedLower,
    LexerState nextState
) {
    if (currentIs(lx, expectedLower)) {
        appendChar(lexeme, idx, lx->current);
        lexerAdvance(lx);
        lx->state=nextState;
    }
    else {
        lx->state=STATE_IDENT;
    }
}

static bool finishKeywordOrIdent(
    Lexer *lx,
    Token *token,
    char *lexeme,
    int idx,
    TokenType keywordType
) {
    if (lx->eof || !isIdentifierPart(lx->current)) {
        lexeme[idx]='\0';
        setToken(token, keywordType, lexeme);
        return true;
    }

    lx->state=STATE_IDENT;
    return false;
}

#define RETURN_TOKEN() do { lx->state=STATE_START; return token; } while (0)
#define RETURN_TOKEN_KEEP_STATE() do { return token; } while (0)

Token getToken(Lexer *lx) {
    Token token;
    char lexeme[MAX_LEXEME];
    int idx=0;

    if (!lx->ready) {
        setToken(&token, TOKEN_EOF, "EOF");
        RETURN_TOKEN();
    }

    lexeme[0]='\0';

    for (;;) {
        switch (lx->state) {
            case STATE_PENDING_PERIOD:
                if (!lx->eof && lx->current=='.') {
                    lx->state=STATE_SECOND_PERIOD;
                }
                else {
                    lx->state=STATE_START;
                }
                setToken(&token, TOKEN_PERIOD, ".");
                RETURN_TOKEN();

            case STATE_SECOND_PERIOD:
                if (!lx->eof && lx->current=='.') {
                    lexerAdvance(lx);
                }
                lx->state=STATE_START;
                setToken(&token, TOKEN_PERIOD, ".");
                RETURN_TOKEN();

            case STATE_START:
                idx=0;
                lexeme[0]='\0';

                if (lx->eof) {
                    setToken(&token, TOKEN_EOF, "EOF");
                    RETURN_TOKEN();
                }

                if (isspace((unsigned char)lx->current)) {
                    lexerAdvance(lx); //skip baca spasi
                    break;
                }

                if (isIdentifierStart(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=initialKeywordState(lexeme[0]);
                    break;
                }

                if (isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_NUMBER;
                    break;
                }

                if (lx->current=='\'') {
                    lexerAdvance(lx);
                    lx->state=STATE_QUOTE;
                    break;
                }

                if (lx->current==':') {
                    lexerAdvance(lx);
                    lx->state=STATE_COLON;
                    break;
                }

                if (lx->current=='<') {
                    lexerAdvance(lx);
                    lx->state=STATE_LT;
                    break;
                }

                if (lx->current=='>') {
                    lexerAdvance(lx);
                    lx->state=STATE_GT;
                    break;
                }

                if (lx->current=='=') {
                    lexerAdvance(lx);
                    lx->state=STATE_EQUAL;
                    break;
                }

                if (lx->current=='{') {
                    lexerAdvance(lx);
                    lx->state=STATE_COMMENT_BRACE;
                    break;
                }

                if (lx->current=='(') {
                    lexerAdvance(lx);
                    lx->state=STATE_BRACKET;
                    break;
                }

                if (lx->current=='.') {
                    lexerAdvance(lx);
                    lx->state=STATE_PERIOD;
                    break;
                }

                switch (lx->current) {
                    case '+':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_PLUS, "+");
                        RETURN_TOKEN();
                    case '-':
                        appendChar(lexeme, &idx, '-');
                        lexerAdvance(lx);
                        lx->state=STATE_MINUS;
                        break;
                    case '*':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_TIMES, "*");
                        RETURN_TOKEN();
                    case '/':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RDIV, "/");
                        RETURN_TOKEN();
                    case ')':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RPARENT, ")");
                        RETURN_TOKEN();
                    case '[':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_LBRACK, "[");
                        RETURN_TOKEN();
                    case ']':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RBRACK, "]");
                        RETURN_TOKEN();
                    case ',':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_COMMA, ",");
                        RETURN_TOKEN();
                    case ';':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_SEMICOLON, ";");
                        RETURN_TOKEN();
                    default:
                        lexeme[0]=lx->current;
                        lexeme[1]='\0';
                        lexerAdvance(lx);
                        setUnknownToken(&token, lexeme);
                        RETURN_TOKEN();
                }
                break;

            case STATE_MINUS:
                if (!lx->eof && isDigitChar(lx->current)) {
                    lx->state=STATE_NUMBER;
                }
                else {
                    setToken(&token, TOKEN_MINUS, "-");
                    RETURN_TOKEN();
                }
                break;

            case STATE_IDENT:
                if (!lx->eof && isIdentifierPart(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    break;
                }

                lexeme[idx]='\0';
                setToken(&token, TOKEN_IDENT, lexeme);
                RETURN_TOKEN();

            case STATE_NUMBER:
                if (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    break;
                }

                if (!lx->eof && lx->current=='.') {
                    lexerAdvance(lx);
                    lx->state=STATE_NUMBER_DOT;
                    break;
                }

                if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    lx->state=STATE_INVALID_EXPONENT;
                    break;
                }

                lexeme[idx]='\0';
                setToken(&token, TOKEN_INTCON, lexeme);
                RETURN_TOKEN();

            case STATE_NUMBER_DOT:
                if (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, '.');
                    lx->state=STATE_REAL;
                    break;
                }

                if (!lx->eof &&
                    !isDefaultSeparatorChar(lx->current) &&
                    lx->current!='.') {
                    appendChar(lexeme, &idx, '.');
                    consumeUntilDefaultSeparator(lx, lexeme, &idx);
                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    RETURN_TOKEN();
                }

                lx->state=STATE_PENDING_PERIOD;
                lexeme[idx]='\0';
                setToken(&token, TOKEN_INTCON, lexeme);
                RETURN_TOKEN_KEEP_STATE();

            case STATE_REAL:
                if (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    break;
                }

                if (!lx->eof && lx->current=='.') {
                    lexerAdvance(lx);
                    lx->state=STATE_PENDING_PERIOD;
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_REALCON, lexeme);
                    RETURN_TOKEN_KEEP_STATE();
                }

                if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    lx->state=STATE_INVALID_EXPONENT;
                    break;
                }

                lexeme[idx]='\0';
                setToken(&token, TOKEN_REALCON, lexeme);
                RETURN_TOKEN();

            case STATE_INVALID_EXPONENT:
                if (lx->eof) {
                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    RETURN_TOKEN();
                }

                appendChar(lexeme, &idx, lx->current);
                lexerAdvance(lx);
                lx->state=STATE_INVALID_EXPONENT_SIGN;
                break;

            case STATE_INVALID_EXPONENT_SIGN:
                if (!lx->eof && (lx->current=='+' || lx->current=='-')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }
                lx->state=STATE_INVALID_EXPONENT_BODY;
                break;

            case STATE_INVALID_EXPONENT_BODY:
                consumeUntilDefaultSeparator(lx, lexeme, &idx);
                lexeme[idx]='\0';
                setUnknownToken(&token, lexeme);
                RETURN_TOKEN();

            case STATE_C:
                if (currentIs(lx, 'o')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_CO;
                }
                else if (currentIs(lx, 'a')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_CA;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_CO:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_CON);
                break;

            case STATE_CON:
                keywordStepOrFallback(lx, lexeme, &idx, 's', STATE_CONS);
                break;

            case STATE_CONS:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_CONST);
                break;

            case STATE_CONST:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_CONSTSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_CA:
                keywordStepOrFallback(lx, lexeme, &idx, 's', STATE_CAS);
                break;

            case STATE_CAS:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_CASE);
                break;

            case STATE_CASE:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_CASESY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_T:
                if (currentIs(lx, 'y')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_TY;
                }
                else if (currentIs(lx, 'o')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_TO;
                }
                else if (currentIs(lx, 'h')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_TH;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_TY:
                keywordStepOrFallback(lx, lexeme, &idx, 'p', STATE_TYP);
                break;

            case STATE_TYP:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_TYPE);
                break;

            case STATE_TYPE:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_TYPESY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_TO:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_TOSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_TH:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_THE);
                break;

            case STATE_THE:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_THEN);
                break;

            case STATE_THEN:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_THENSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_V:
                keywordStepOrFallback(lx, lexeme, &idx, 'a', STATE_VA);
                break;

            case STATE_VA:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_VAR);
                break;

            case STATE_VAR:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_VARSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_F:
                if (currentIs(lx, 'u')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_FU;
                }
                else if (currentIs(lx, 'o')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_FO;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_FU:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_FUN);
                break;

            case STATE_FUN:
                keywordStepOrFallback(lx, lexeme, &idx, 'c', STATE_FUNC);
                break;

            case STATE_FUNC:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_FUNCT);
                break;

            case STATE_FUNCT:
                keywordStepOrFallback(lx, lexeme, &idx, 'i', STATE_FUNCTI);
                break;

            case STATE_FUNCTI:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_FUNCTIO);
                break;

            case STATE_FUNCTIO:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_FUNCTION);
                break;

            case STATE_FUNCTION:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_FUNCTIONSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_FO:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_FOR);
                break;

            case STATE_FOR:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_FORSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_P:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_PR);
                break;

            case STATE_PR:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_PRO);
                break;

            case STATE_PRO:
                if (currentIs(lx, 'c')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_PROC;
                }
                else if (currentIs(lx, 'g')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_PROG;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_PROC:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_PROCE);
                break;

            case STATE_PROCE:
                keywordStepOrFallback(lx, lexeme, &idx, 'd', STATE_PROCED);
                break;

            case STATE_PROCED:
                keywordStepOrFallback(lx, lexeme, &idx, 'u', STATE_PROCEDU);
                break;

            case STATE_PROCEDU:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_PROCEDUR);
                break;

            case STATE_PROCEDUR:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_PROCEDURE);
                break;

            case STATE_PROCEDURE:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_PROCEDURESY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_PROG:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_PROGR);
                break;

            case STATE_PROGR:
                keywordStepOrFallback(lx, lexeme, &idx, 'a', STATE_PROGRA);
                break;

            case STATE_PROGRA:
                keywordStepOrFallback(lx, lexeme, &idx, 'm', STATE_PROGRAM);
                break;

            case STATE_PROGRAM:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_PROGRAMSY)) {
                    RETURN_TOKEN();
                }
                break;
            case STATE_A:
                if (currentIs(lx, 'n')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_AN;
                }
                else if (currentIs(lx, 'r')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_AR;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_AN:
                keywordStepOrFallback(lx, lexeme, &idx, 'd', STATE_AND);
                break;

            case STATE_AND:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ANDSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_AR:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_ARR);
                break;

            case STATE_ARR:
                keywordStepOrFallback(lx, lexeme, &idx, 'a', STATE_ARRA);
                break;

            case STATE_ARRA:
                keywordStepOrFallback(lx, lexeme, &idx, 'y', STATE_ARRAY);
                break;

            case STATE_ARRAY:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ARRAYSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_R:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_RE);
                break;

            case STATE_RE:
                if (currentIs(lx, 'c')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_REC;
                }
                else if (currentIs(lx, 'p')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_REP;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_REC:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_RECO);
                break;

            case STATE_RECO:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_RECOR);
                break;

            case STATE_RECOR:
                keywordStepOrFallback(lx, lexeme, &idx, 'd', STATE_RECORD);
                break;

            case STATE_RECORD:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_RECORDSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_REP:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_REPE);
                break;

            case STATE_REPE:
                keywordStepOrFallback(lx, lexeme, &idx, 'a', STATE_REPEA);
                break;

            case STATE_REPEA:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_REPEAT);
                break;

            case STATE_REPEAT:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_REPEATSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_B:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_BE);
                break;

            case STATE_BE:
                keywordStepOrFallback(lx, lexeme, &idx, 'g', STATE_BEG);
                break;

            case STATE_BEG:
                keywordStepOrFallback(lx, lexeme, &idx, 'i', STATE_BEGI);
                break;

            case STATE_BEGI:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_BEGIN);
                break;

            case STATE_BEGIN:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_BEGINSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_I:
                keywordStepOrFallback(lx, lexeme, &idx, 'f', STATE_IF);
                break;

            case STATE_IF:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_IFSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_N:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_NO);
                break;

            case STATE_NO:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_NOT);
                break;

            case STATE_NOT:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_NOTSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_M:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_MO);
                break;

            case STATE_MO:
                keywordStepOrFallback(lx, lexeme, &idx, 'd', STATE_MOD);
                break;

            case STATE_MOD:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_IMOD)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_W:
                keywordStepOrFallback(lx, lexeme, &idx, 'h', STATE_WH);
                break;

            case STATE_WH:
                keywordStepOrFallback(lx, lexeme, &idx, 'i', STATE_WHI);
                break;

            case STATE_WHI:
                keywordStepOrFallback(lx, lexeme, &idx, 'l', STATE_WHIL);
                break;

            case STATE_WHIL:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_WHILE);
                break;

            case STATE_WHILE:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_WHILESY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_E:
                if (currentIs(lx, 'n')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_EN;
                }
                else if (currentIs(lx, 'l')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_EL;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_EN:
                keywordStepOrFallback(lx, lexeme, &idx, 'd', STATE_END);
                break;

            case STATE_END:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ENDSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_EL:
                keywordStepOrFallback(lx, lexeme, &idx, 's', STATE_ELS);
                break;

            case STATE_ELS:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_ELSE);
                break;

            case STATE_ELSE:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ELSESY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_U:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_UN);
                break;

            case STATE_UN:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_UNT);
                break;

            case STATE_UNT:
                keywordStepOrFallback(lx, lexeme, &idx, 'i', STATE_UNTI);
                break;

            case STATE_UNTI:
                keywordStepOrFallback(lx, lexeme, &idx, 'l', STATE_UNTIL);
                break;

            case STATE_UNTIL:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_UNTILSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_O:
                if (currentIs(lx, 'f')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_OF;
                }
                else if (currentIs(lx, 'r')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_OR;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_OF:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_OFSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_OR:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ORSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_D:
                if (currentIs(lx, 'o')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_DO;
                }
                else if (currentIs(lx, 'i')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_DI;
                }
                else {
                    lx->state=STATE_IDENT;
                }
                break;

            case STATE_DI:
                keywordStepOrFallback(lx, lexeme, &idx, 'v', STATE_DIV);
                break;

            case STATE_DIV:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_IDIV)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_DO:
                if (currentIs(lx, 'w')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_DOW;
                }
                else if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_DOSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_DOW:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_DOWN);
                break;

            case STATE_DOWN:
                keywordStepOrFallback(lx, lexeme, &idx, 't', STATE_DOWNT);
                break;

            case STATE_DOWNT:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_DOWNTO);
                break;

            case STATE_DOWNTO:
                if (finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_DOWNTOSY)) {
                    RETURN_TOKEN();
                }
                break;

            case STATE_QUOTE:
                if (lx->eof || lx->current=='\n' || lx->current=='\r') {
                    setUnknownToken(&token, "Invalid string");
                    RETURN_TOKEN();
                }

                if (lx->current=='\'') {
                    lexerAdvance(lx);
                    lx->state=STATE_QUOTE_AFTER_QUOTE;
                    break;
                }

                appendChar(lexeme, &idx, lx->current);
                lexerAdvance(lx);
                break;

            case STATE_QUOTE_AFTER_QUOTE:
                if (!lx->eof && lx->current=='\'') {
                    appendChar(lexeme, &idx, '\'');
                    lexerAdvance(lx);
                    lx->state=STATE_QUOTE_ESCAPED;
                    break;
                }

                lexeme[idx]='\0';
                if (idx==1) {
                    setToken(&token, TOKEN_CHARCON, lexeme);
                }
                else {
                    setToken(&token, TOKEN_STRING, lexeme);
                }
                RETURN_TOKEN();

            case STATE_QUOTE_ESCAPED:
                if (lx->eof || isQuoteBoundaryChar(lx->current)) {
                    lexeme[idx]='\0';
                    if (idx==1) {
                        setToken(&token, TOKEN_CHARCON, lexeme);
                    }
                    else {
                        setToken(&token, TOKEN_STRING, lexeme);
                    }
                    RETURN_TOKEN();
                }
                lx->state=STATE_QUOTE;
                break;

            case STATE_COLON:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_BECOMES, ":=");
                }
                else {
                    setToken(&token, TOKEN_COLON, ":");
                }
                RETURN_TOKEN();

            case STATE_LT:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_LEQ, "<=");
                }
                else if (!lx->eof && lx->current=='>') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_NEQ, "<>");
                }
                else {
                    setToken(&token, TOKEN_LSS, "<");
                }
                RETURN_TOKEN();

            case STATE_GT:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_GEQ, ">=");
                }
                else {
                    setToken(&token, TOKEN_GTR, ">");
                }
                RETURN_TOKEN();

            case STATE_EQUAL:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_EQL, "==");
                }
                else {
                    setUnknownToken(&token, "Invalid '='");
                }
                RETURN_TOKEN();
            case STATE_PERIOD:
                setToken(&token, TOKEN_PERIOD, ".");
                RETURN_TOKEN();

            case STATE_LEADING_DOT_NUMBER:
                if (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    break;
                }

                if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    lx->state=STATE_INVALID_EXPONENT;
                    break;
                }

                if (!lx->eof && !isDefaultSeparatorChar(lx->current)) {
                    consumeUntilDefaultSeparator(lx, lexeme, &idx);
                }

                lexeme[idx]='\0';
                setUnknownToken(&token, lexeme);
                RETURN_TOKEN();

            case STATE_BRACKET:
                if (!lx->eof && lx->current=='*') {
                    lexerAdvance(lx);
                    lx->state=STATE_COMMENT_PAREN;
                }
                else {
                    setToken(&token, TOKEN_LPARENT, "(");
                    RETURN_TOKEN();
                }
                break;

            case STATE_COMMENT_BRACE:
                if (lx->eof) {
                    setUnknownToken(&token, "Invalid comment");
                    RETURN_TOKEN();
                }

                if (lx->current=='}') {
                    lexerAdvance(lx);
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_COMMENT, lexeme);
                    RETURN_TOKEN();
                }

                appendChar(lexeme, &idx, lx->current);
                lexerAdvance(lx);
                break;

            case STATE_COMMENT_PAREN:
                if (lx->eof) {
                    setUnknownToken(&token, "Invalid comment");
                    RETURN_TOKEN();
                }

                if (lx->current=='*') {
                    lexerAdvance(lx);
                    lx->state=STATE_COMMENT_PAREN_NEAREND;
                }
                else {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }
                break;

            case STATE_COMMENT_PAREN_NEAREND:
                if (lx->eof) {
                    setUnknownToken(&token, "Invalid comment");
                    RETURN_TOKEN();
                }

                if (lx->current==')') {
                    lexerAdvance(lx);
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_COMMENT, lexeme);
                    RETURN_TOKEN();
                }

                if (lx->current=='\0') {
                    setUnknownToken(&token, "Invalid comment");
                    RETURN_TOKEN();
                }

                appendChar(lexeme, &idx, '*');
                lx->state=STATE_COMMENT_PAREN;
                break;

            case STATE_UNKNOWN:
                setUnknownToken(&token, "Unknown");
                RETURN_TOKEN();

            default:
                setUnknownToken(&token, "Unknown");
                RETURN_TOKEN();
        }
    }
}

#undef RETURN_TOKEN
#undef RETURN_TOKEN_KEEP_STATE

