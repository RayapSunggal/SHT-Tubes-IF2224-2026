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
    STATE_COMMENT_BRACE,
    STATE_COMMENT_PAREN,
    STATE_DONE,
    STATE_UNKNOWN
} LexerState;

typedef struct {
    bool ready;
    bool eof;
    char current;

    int pendingPeriods;
    LexerState state;
} Lexer;

void initLexer(Lexer *lx, const char *filename);
void closeLexer(Lexer *lx);
Token getToken(Lexer *lx);

#endif
