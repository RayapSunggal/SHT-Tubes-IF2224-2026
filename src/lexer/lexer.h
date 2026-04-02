#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#include "../token/token.h"

typedef enum {
    STATE_START,
    STATE_IDENT,
    STATE_NUMBER,
    STATE_REAL,
    STATE_QUOTE,
    STATE_COLON,
    STATE_LT,
    STATE_GT,
    STATE_EQUAL,
    STATE_BRACKET,
    STATE_PERIOD,
    STATE_PENDING_PERIOD,
    STATE_COMMENT_BRACE,
    STATE_COMMENT_PAREN,
    STATE_COMMENT_PAREN_NEAREND,
    STATE_NUMBER_DOT,
    STATE_LEADING_DOT_NUMBER,
    STATE_QUOTE_AFTER_QUOTE,
    STATE_QUOTE_ESCAPED,

    STATE_C,
    STATE_CO,
    STATE_CON,
    STATE_CONS,
    STATE_CONST,
    STATE_CA,
    STATE_CAS,
    STATE_CASE,

    STATE_T,
    STATE_TY,
    STATE_TYP,
    STATE_TYPE,
    STATE_TO,
    STATE_TH,
    STATE_THE,
    STATE_THEN,

    STATE_V,
    STATE_VA,
    STATE_VAR,

    STATE_F,
    STATE_FU,
    STATE_FUN,
    STATE_FUNC,
    STATE_FUNCT,
    STATE_FUNCTI,
    STATE_FUNCTIO,
    STATE_FUNCTION,
    STATE_FO,
    STATE_FOR,

    STATE_P,
    STATE_PR,
    STATE_PRO,
    STATE_PROC,
    STATE_PROCE,
    STATE_PROCED,
    STATE_PROCEDU,
    STATE_PROCEDUR,
    STATE_PROCEDURE,
    STATE_PROG,
    STATE_PROGR,
    STATE_PROGRA,
    STATE_PROGRAM,

    STATE_A,
    STATE_AN,
    STATE_AND,
    STATE_AR,
    STATE_ARR,
    STATE_ARRA,
    STATE_ARRAY,

    STATE_R,
    STATE_RE,
    STATE_REC,
    STATE_RECO,
    STATE_RECOR,
    STATE_RECORD,
    STATE_REP,
    STATE_REPE,
    STATE_REPEA,
    STATE_REPEAT,

    STATE_B,
    STATE_BE,
    STATE_BEG,
    STATE_BEGI,
    STATE_BEGIN,

    STATE_I,
    STATE_IF,

    STATE_N,
    STATE_NO,
    STATE_NOT,

    STATE_M,
    STATE_MO,
    STATE_MOD,

    STATE_W,
    STATE_WH,
    STATE_WHI,
    STATE_WHIL,
    STATE_WHILE,

    STATE_E,
    STATE_EN,
    STATE_END,
    STATE_EL,
    STATE_ELS,
    STATE_ELSE,

    STATE_U,
    STATE_UN,
    STATE_UNT,
    STATE_UNTI,
    STATE_UNTIL,

    STATE_O,
    STATE_OF,
    STATE_OR,

    STATE_D,
    STATE_DO,
    STATE_DI,
    STATE_DIV,
    STATE_DOW,
    STATE_DOWN,
    STATE_DOWNT,
    STATE_DOWNTO,

    STATE_INVALID_EXPONENT,
    STATE_INVALID_EXPONENT_SIGN,
    STATE_INVALID_EXPONENT_BODY,

    STATE_UNKNOWN
} LexerState;

typedef struct {
    bool ready;
    bool eof;
    char current;
    LexerState state;
} Lexer;

void initLexer(Lexer *lx, const char *filename);
void closeLexer(Lexer *lx);
Token getToken(Lexer *lx);

#endif







