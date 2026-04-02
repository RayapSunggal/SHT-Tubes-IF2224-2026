#include "lexer.h"

#include <ctype.h>
#include <string.h>

#include "../mesinkarakter/mesinkarakter.h"

typedef struct {
    const char *text;
    TokenType type;
} Keyword;

static const Keyword KEYWORDS[]={
    {"program", TOKEN_PROGRAMSY},
    {"var", TOKEN_VARSY},
    {"const", TOKEN_CONSTSY},
    {"type", TOKEN_TYPESY},
    {"function", TOKEN_FUNCTIONSY},
    {"procedure", TOKEN_PROCEDURESY},
    {"array", TOKEN_ARRAYSY},
    {"record", TOKEN_RECORDSY},
    {"begin", TOKEN_BEGINSY},
    {"end", TOKEN_ENDSY},
    {"if", TOKEN_IFSY},
    {"case", TOKEN_CASESY},
    {"repeat", TOKEN_REPEATSY},
    {"while", TOKEN_WHILESY},
    {"for", TOKEN_FORSY},
    {"else", TOKEN_ELSESY},
    {"until", TOKEN_UNTILSY},
    {"of", TOKEN_OFSY},
    {"do", TOKEN_DOSY},
    {"to", TOKEN_TOSY},
    {"downto", TOKEN_DOWNTOSY},
    {"then", TOKEN_THENSY},
    {"div", TOKEN_IDIV},
    {"mod", TOKEN_IMOD},
    {"and", TOKEN_ANDSY},
    {"or", TOKEN_ORSY},
    {"not", TOKEN_NOTSY}
};

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

static bool isQuoteBoundaryChar(char c) {
    if (c=='\0' || isspace((unsigned char)c)) {
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

static void toLowerStr(char *s) {
    int i=0;

    while (s[i]!='\0') {
        s[i]=(char)tolower((unsigned char)s[i]);
        i++;
    }
}

static char lowerChar(char c) {
    return (char)tolower((unsigned char)c);
}

static TokenType resolveIdentifierType(const char *lexeme) {
    char lower[MAX_LEXEME];
    size_t i;

    strncpy(lower, lexeme, MAX_LEXEME - 1);
    lower[MAX_LEXEME - 1]='\0';
    toLowerStr(lower);

    for (i=0; i < (sizeof(KEYWORDS) / sizeof(KEYWORDS[0])); i++) {
        if (strcmp(lower, KEYWORDS[i].text)==0) {
            return KEYWORDS[i].type;
        }
    }

    return TOKEN_IDENT;
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
    lx->pendingPeriods=0;
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
    lx->pendingPeriods=0;
    lx->state=STATE_DONE;
}

static void lexerAdvance(Lexer *lx) {
    if (lx->eof) {
        return;
    }

    ADV();
    syncWithMachine(lx);
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

static void finishKeywordOrIdent(
    Lexer *lx,
    Token *token,
    char *lexeme,
    int idx,
    TokenType keywordType
) {
    if (lx->eof || !isIdentifierPart(lx->current)) {
        lexeme[idx]='\0';
        setToken(token, keywordType, lexeme);
        lx->state=STATE_DONE;
    }
    else {
        lx->state=STATE_IDENT;
    }
}

Token getToken(Lexer *lx) {
    Token token;
    char lexeme[MAX_LEXEME];
    int idx=0;

    if (!lx->ready) {
        setToken(&token, TOKEN_EOF, "EOF");
        return token;
    }
    if (lx->pendingPeriods > 0) {
        lx->pendingPeriods--;
        setToken(&token, TOKEN_PERIOD, ".");
        return token;
    }

    lx->state=STATE_START;

    while (lx->state!=STATE_DONE && lx->state!=STATE_UNKNOWN) {
        switch (lx->state) {
            case STATE_START:
                while (!lx->eof && isspace((unsigned char)lx->current)) {
                    lexerAdvance(lx); //skip baca spasi
                }

                if (lx->eof) { //sudah habis baca
                    setToken(&token, TOKEN_EOF, "EOF");
                    lx->state=STATE_DONE;
                    break;
                }

                idx=0;
                lexeme[0]='\0';

                if (isIdentifierStart(lx->current)) { //ciri-ciri identifier
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=initialKeywordState(lexeme[0]);
                    break;
                }

                if (isDigitChar(lx->current)) { //ciri-ciri kalau angka
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

                if (lx->current=='(') { //ciri ciri komen atau kurung saja
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
                        lx->state=STATE_DONE;
                        break;
                    case '-':
                        appendChar(lexeme, &idx, '-');
                        lexerAdvance(lx);

                        if (!lx->eof && isDigitChar(lx->current)) {
                            appendChar(lexeme, &idx, lx->current);
                            lexerAdvance(lx);
                            lx->state=STATE_NUMBER;
                        }
                        else {
                            setToken(&token, TOKEN_MINUS, "-");
                            lx->state=STATE_DONE;
                        }
                        break;
                    case '*':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_TIMES, "*");
                        lx->state=STATE_DONE;
                        break;
                    case '/':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RDIV, "/");
                        lx->state=STATE_DONE;
                        break;
                    case ')':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RPARENT, ")");
                        lx->state=STATE_DONE;
                        break;
                    case '[':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_LBRACK, "[");
                        lx->state=STATE_DONE;
                        break;
                    case ']':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_RBRACK, "]");
                        lx->state=STATE_DONE;
                        break;
                    case ',':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_COMMA, ",");
                        lx->state=STATE_DONE;
                        break;
                    case ';':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_SEMICOLON, ";");
                        lx->state=STATE_DONE;
                        break;
                    default:
                        lexeme[0]=lx->current;
                        lexeme[1]='\0';
                        lexerAdvance(lx);
                        setUnknownToken(&token, lexeme);
                        lx->state=STATE_UNKNOWN;
                        break;
                }
                break;

            case STATE_IDENT:
                while (!lx->eof && isIdentifierPart(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                lexeme[idx]='\0';
                setToken(&token, resolveIdentifierType(lexeme), lexeme);
                lx->state=STATE_DONE;
                break;

            case STATE_NUMBER:
                while (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (!lx->eof && lx->current=='.') {
                    lexerAdvance(lx);

                    if (!lx->eof && isDigitChar(lx->current)) {
                        appendChar(lexeme, &idx, '.');
                        lx->state=STATE_REAL;
                        break;
                    }

                    lx->pendingPeriods=1;
                }
                else if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    lx->state=STATE_INVALID_EXPONENT;
                    break;
                }

                lexeme[idx]='\0';
                setToken(&token, TOKEN_INTCON, lexeme);
                lx->state=STATE_DONE;
                break;

            case STATE_REAL:
                while (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (!lx->eof && lx->current=='.') {
                    lexerAdvance(lx);
                    lx->pendingPeriods=1;
                }
                else if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    lx->state=STATE_INVALID_EXPONENT;
                    break;
                }

                lexeme[idx]='\0';
                setToken(&token, TOKEN_REALCON, lexeme);
                lx->state=STATE_DONE;
                break;

            case STATE_INVALID_EXPONENT:
                if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_INVALID_EXPONENT_SIGN;
                }
                else {
                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    lx->state=STATE_UNKNOWN;
                }
                break;

            case STATE_INVALID_EXPONENT_SIGN:
                if (!lx->eof && (lx->current=='+' || lx->current=='-')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }
                lx->state=STATE_INVALID_EXPONENT_BODY;
                break;

            case STATE_INVALID_EXPONENT_BODY:
                while (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                while (!lx->eof && isIdentifierPart(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                lexeme[idx]='\0';
                setUnknownToken(&token, lexeme);
                lx->state=STATE_UNKNOWN;
                break;

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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_CONSTSY);
                break;

            case STATE_CA:
                keywordStepOrFallback(lx, lexeme, &idx, 's', STATE_CAS);
                break;

            case STATE_CAS:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_CASE);
                break;

            case STATE_CASE:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_CASESY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_TYPESY);
                break;

            case STATE_TO:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_TOSY);
                break;

            case STATE_TH:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_THE);
                break;

            case STATE_THE:
                keywordStepOrFallback(lx, lexeme, &idx, 'n', STATE_THEN);
                break;

            case STATE_THEN:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_THENSY);
                break;

            case STATE_V:
                keywordStepOrFallback(lx, lexeme, &idx, 'a', STATE_VA);
                break;

            case STATE_VA:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_VAR);
                break;

            case STATE_VAR:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_VARSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_FUNCTIONSY);
                break;

            case STATE_FO:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_FOR);
                break;

            case STATE_FOR:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_FORSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_PROCEDURESY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_PROGRAMSY);
                break;
            case STATE_A:
                keywordStepOrFallback(lx, lexeme, &idx, 'r', STATE_AR);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ARRAYSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_RECORDSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_REPEATSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_BEGINSY);
                break;

            case STATE_I:
                keywordStepOrFallback(lx, lexeme, &idx, 'f', STATE_IF);
                break;

            case STATE_IF:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_IFSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_WHILESY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ENDSY);
                break;

            case STATE_EL:
                keywordStepOrFallback(lx, lexeme, &idx, 's', STATE_ELS);
                break;

            case STATE_ELS:
                keywordStepOrFallback(lx, lexeme, &idx, 'e', STATE_ELSE);
                break;

            case STATE_ELSE:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_ELSESY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_UNTILSY);
                break;

            case STATE_O:
                keywordStepOrFallback(lx, lexeme, &idx, 'f', STATE_OF);
                break;

            case STATE_OF:
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_OFSY);
                break;

            case STATE_D:
                keywordStepOrFallback(lx, lexeme, &idx, 'o', STATE_DO);
                break;

            case STATE_DO:
                if (currentIs(lx, 'w')) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_DOW;
                }
                else {
                    finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_DOSY);
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
                finishKeywordOrIdent(lx, &token, lexeme, idx, TOKEN_DOWNTOSY);
                break;

            case STATE_QUOTE:
                while (!lx->eof) {
                    if (lx->current=='\n' || lx->current=='\r') {
                        setUnknownToken(&token, "Invalid string");
                        lx->state=STATE_UNKNOWN;
                        break;
                    }

                    if (lx->current=='\'') {
                        lexerAdvance(lx);

                        if (!lx->eof && lx->current=='\'') {
                            appendChar(lexeme, &idx, '\'');
                            lexerAdvance(lx);

                            if (idx==1 && (lx->eof || isQuoteBoundaryChar(lx->current))) {
                                lexeme[idx]='\0';
                                setToken(&token, TOKEN_CHARCON, lexeme);
                                lx->state=STATE_DONE;
                                break;
                            }

                            continue;
                        }

                        lexeme[idx]='\0';
                        if (idx==1) {
                            setToken(&token, TOKEN_CHARCON, lexeme);
                        }
                        else {
                            setToken(&token, TOKEN_STRING, lexeme);
                        }
                        lx->state=STATE_DONE;
                        break;
                    }

                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (lx->state!=STATE_DONE && lx->eof) {
                    setUnknownToken(&token, "Invalid string");
                    lx->state=STATE_UNKNOWN;
                }
                break;

            case STATE_COLON:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_BECOMES, ":=");
                }
                else {
                    setToken(&token, TOKEN_COLON, ":");
                }
                lx->state=STATE_DONE;
                break;

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
                lx->state=STATE_DONE;
                break;

            case STATE_GT:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_GEQ, ">=");
                }
                else {
                    setToken(&token, TOKEN_GTR, ">");
                }
                lx->state=STATE_DONE;
                break;

            case STATE_EQUAL:
                if (!lx->eof && lx->current=='=') {
                    lexerAdvance(lx);
                    setToken(&token, TOKEN_EQL, "==");
                    lx->state=STATE_DONE;
                }
                else {
                    setUnknownToken(&token, "Invalid '='");
                    lx->state=STATE_UNKNOWN;
                }
                break;

            case STATE_PERIOD:
                if (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, '.');

                    while (!lx->eof && isDigitChar(lx->current)) {
                        appendChar(lexeme, &idx, lx->current);
                        lexerAdvance(lx);
                    }

                    if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                        lx->state=STATE_INVALID_EXPONENT;
                        break;
                    }

                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    lx->state=STATE_UNKNOWN;
                }
                else {
                    setToken(&token, TOKEN_PERIOD, ".");
                    lx->state=STATE_DONE;
                }
                break;

            case STATE_BRACKET:
                if (!lx->eof && lx->current=='*') {
                    lexerAdvance(lx);
                    lx->state=STATE_COMMENT_PAREN;
                }
                else {
                    setToken(&token, TOKEN_LPARENT, "(");
                    lx->state=STATE_DONE;
                }
                break;

            case STATE_COMMENT_BRACE:
                while (!lx->eof && lx->current!='}') {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (lx->eof) {
                    setUnknownToken(&token, "Invalid comment");
                    lx->state=STATE_UNKNOWN;
                    break;
                }

                lexerAdvance(lx);
                lexeme[idx]='\0';
                setToken(&token, TOKEN_COMMENT, lexeme);
                lx->state=STATE_DONE;
                break;

            case STATE_COMMENT_PAREN:
                if (lx->eof) {
                    setUnknownToken(&token, "Invalid comment");
                    lx->state=STATE_UNKNOWN;
                    break;
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
                    lx->state=STATE_UNKNOWN;
                }
                else if (lx->current==')') {
                    lexerAdvance(lx);
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_COMMENT, lexeme);
                    lx->state=STATE_DONE;
                }
                else if (lx->current!='\0') {
                    appendChar(lexeme, &idx, '*');
                    lx->state=STATE_COMMENT_PAREN;
                }
                else {
                    setUnknownToken(&token, "Invalid comment");
                    lx->state=STATE_UNKNOWN;
                }
                break;

            case STATE_DONE:
                break;

            case STATE_UNKNOWN:
                break;

            default:
                setUnknownToken(&token, "Unknown");
                lx->state=STATE_UNKNOWN;
                break;
        }
    }

    return token;
}



