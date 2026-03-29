#include "token.h"

const char *tokenTypeToString(TokenType type) {
    switch (type) {
        case TOKEN_INTCON: return "intcon";
        case TOKEN_REALCON: return "realcon";
        case TOKEN_CHARCON: return "charcon";
        case TOKEN_STRING: return "string";
        case TOKEN_IDENT: return "ident";
        case TOKEN_COMMENT: return "comment";

        case TOKEN_PLUS: return "plus";
        case TOKEN_MINUS: return "minus";
        case TOKEN_TIMES: return "times";
        case TOKEN_IDIV: return "idiv";
        case TOKEN_RDIV: return "rdiv";
        case TOKEN_IMOD: return "imod";

        case TOKEN_ANDSY: return "andsy";
        case TOKEN_ORSY: return "orsy";
        case TOKEN_NOTSY: return "notsy";

        case TOKEN_EQL: return "eql";
        case TOKEN_NEQ: return "neq";
        case TOKEN_GTR: return "gtr";
        case TOKEN_GEQ: return "geq";
        case TOKEN_LSS: return "lss";
        case TOKEN_LEQ: return "leq";

        case TOKEN_LPARENT: return "lparent";
        case TOKEN_RPARENT: return "rparent";
        case TOKEN_LBRACK: return "lbrack";
        case TOKEN_RBRACK: return "rbrack";
        case TOKEN_COMMA: return "comma";
        case TOKEN_SEMICOLON: return "semicolon";
        case TOKEN_PERIOD: return "period";
        case TOKEN_COLON: return "colon";
        case TOKEN_BECOMES: return "becomes";

        case TOKEN_CONSTSY: return "constsy";
        case TOKEN_TYPESY: return "typesy";
        case TOKEN_VARSY: return "varsy";
        case TOKEN_FUNCTIONSY: return "functionsy";
        case TOKEN_PROCEDURESY: return "proceduresy";
        case TOKEN_ARRAYSY: return "arraysy";
        case TOKEN_RECORDSY: return "recordsy";
        case TOKEN_PROGRAMSY: return "programsy";
        case TOKEN_BEGINSY: return "beginsy";
        case TOKEN_IFSY: return "ifsy";
        case TOKEN_CASESY: return "casesy";
        case TOKEN_REPEATSY: return "repeatsy";
        case TOKEN_WHILESY: return "whilesy";
        case TOKEN_FORSY: return "forsy";
        case TOKEN_ENDSY: return "endsy";
        case TOKEN_ELSESY: return "elsesy";
        case TOKEN_UNTILSY: return "untilsy";
        case TOKEN_OFSY: return "ofsy";
        case TOKEN_DOSY: return "dosy";
        case TOKEN_TOSY: return "tosy";
        case TOKEN_DOWNTOSY: return "downtosy";
        case TOKEN_THENSY: return "thensy";

        case TOKEN_EOF: return "eof";
        case TOKEN_UNKNOWN: return "unknown";
        default: return "unknown";
    }
}