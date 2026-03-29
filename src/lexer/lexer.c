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
    lx->hasPeek=false;
    lx->peekEof=true;
    lx->peekChar='\0';
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
    lx->hasPeek=false;
    lx->peekEof=true;
    lx->peekChar='\0';
    lx->pendingPeriods=0;
    lx->state=STATE_DONE;
}

static void lexerAdvance(Lexer *lx) {
    if (lx->eof) {
        return;
    }

    if (lx->hasPeek) {
        lx->hasPeek=false;
        lx->eof=lx->peekEof;
        lx->current=lx->peekEof ? '\0' : lx->peekChar;
    }
    else {
        ADV();
        syncWithMachine(lx);
    }
}

static bool lexerPeek(Lexer *lx, char *out) {
    if (lx->eof) {
        return false;
    }

    if (!lx->hasPeek) {
        ADV();
        lx->peekEof=EOP;
        lx->peekChar=EOP ? '\0' : CC;
        lx->hasPeek=true;
    }

    if (lx->peekEof) {
        return false;
    }

    *out=lx->peekChar;
    return true;
}

static void consumeInvalidExponent(Lexer *lx, char *lexeme, int *idx) {
    appendChar(lexeme, idx, lx->current);
    lexerAdvance(lx);

    if (!lx->eof && (lx->current=='+' || lx->current=='-')) {
        appendChar(lexeme, idx, lx->current);
        lexerAdvance(lx);
    }

    while (!lx->eof && isDigitChar(lx->current)) {
        appendChar(lexeme, idx, lx->current);
        lexerAdvance(lx);
    }

    while (!lx->eof && isIdentifierPart(lx->current)) {
        appendChar(lexeme, idx, lx->current);
        lexerAdvance(lx);
    }
}

Token getToken(Lexer *lx) {
    Token token;
    char lexeme[MAX_LEXEME];
    int idx=0;
    char next='\0';

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
                    lexerAdvance(lx);
                }

                if (lx->eof) {
                    setToken(&token, TOKEN_EOF, "EOF");
                    lx->state=STATE_DONE;
                    break;
                }

                idx=0;
                lexeme[0]='\0';

                if (isIdentifierStart(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                    lx->state=STATE_IDENT;
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

                if (lx->current=='(' && lexerPeek(lx, &next) && next=='*') {
                    lexerAdvance(lx);
                    lexerAdvance(lx);
                    lx->state=STATE_COMMENT_PAREN;
                    break;
                }

                switch (lx->current) {
                    case '+':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_PLUS, "+");
                        lx->state=STATE_DONE;
                        break;
                    case '-':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_MINUS, "-");
                        lx->state=STATE_DONE;
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
                    case '(':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_LPARENT, "(");
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
                    case '.':
                        lexerAdvance(lx);
                        setToken(&token, TOKEN_PERIOD, ".");
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
                    if (lexerPeek(lx, &next) && isDigitChar(next)) {
                        appendChar(lexeme, &idx, '.');
                        lexerAdvance(lx);
                        lx->state=STATE_REAL;
                    }
                    else if (lexerPeek(lx, &next) && next=='.') {
                        lexerAdvance(lx);
                        lexerAdvance(lx);
                        lx->pendingPeriods=2;
                        lexeme[idx]='\0';
                        setToken(&token, TOKEN_INTCON, lexeme);
                        lx->state=STATE_DONE;
                    }
                    else {
                        lexeme[idx]='\0';
                        setToken(&token, TOKEN_INTCON, lexeme);
                        lx->state=STATE_DONE;
                    }
                }
                else if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    consumeInvalidExponent(lx, lexeme, &idx);
                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    lx->state=STATE_UNKNOWN;
                }
                else {
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_INTCON, lexeme);
                    lx->state=STATE_DONE;
                }
                break;

            case STATE_REAL:
                while (!lx->eof && isDigitChar(lx->current)) {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (!lx->eof && (lx->current=='e' || lx->current=='E')) {
                    consumeInvalidExponent(lx, lexeme, &idx);
                    lexeme[idx]='\0';
                    setUnknownToken(&token, lexeme);
                    lx->state=STATE_UNKNOWN;
                }
                else {
                    lexeme[idx]='\0';
                    setToken(&token, TOKEN_REALCON, lexeme);
                    lx->state=STATE_DONE;
                }
                break;

            case STATE_QUOTE:
                while (!lx->eof) {
                    if (lx->current=='\n' || lx->current=='\r') {
                        setUnknownToken(&token, "Unterminated string");
                        lx->state=STATE_UNKNOWN;
                        break;
                    }

                    if (lx->current=='\'') {
                        if (lexerPeek(lx, &next) && next=='\'') {
                            appendChar(lexeme, &idx, '\'');
                            lexerAdvance(lx);
                            lexerAdvance(lx);

                            if (idx==1 && (lx->eof || isQuoteBoundaryChar(lx->current))) {
                                lexeme[idx]='\0';
                                setToken(&token, TOKEN_CHARCON, lexeme);
                                lx->state=STATE_DONE;
                                break;
                            }

                            continue;
                        }

                        lexerAdvance(lx);
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
                    setUnknownToken(&token, "Unterminated string");
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

            case STATE_COMMENT_BRACE:
                while (!lx->eof && lx->current!='}') {
                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (lx->eof) {
                    setUnknownToken(&token, "Unterminated comment");
                    lx->state=STATE_UNKNOWN;
                    break;
                }

                lexerAdvance(lx);
                lexeme[idx]='\0';
                setToken(&token, TOKEN_COMMENT, lexeme);
                lx->state=STATE_DONE;
                break;

            case STATE_COMMENT_PAREN:
                while (!lx->eof) {
                    if (lx->current=='*' && lexerPeek(lx, &next) && next==')') {
                        lexerAdvance(lx);
                        lexerAdvance(lx);
                        lexeme[idx]='\0';
                        setToken(&token, TOKEN_COMMENT, lexeme);
                        lx->state=STATE_DONE;
                        break;
                    }

                    appendChar(lexeme, &idx, lx->current);
                    lexerAdvance(lx);
                }

                if (lx->state!=STATE_DONE) {
                    setUnknownToken(&token, "Unterminated comment");
                    lx->state=STATE_UNKNOWN;
                }
                break;

            case STATE_DONE:
                break;

            case STATE_UNKNOWN:
                break;

            default:
                setUnknownToken(&token, "Unknown lexer state");
                lx->state=STATE_UNKNOWN;
                break;
        }
    }

    return token;
}

