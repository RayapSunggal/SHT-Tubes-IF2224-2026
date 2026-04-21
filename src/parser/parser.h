#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "../parse_tree/parse_tree.h"

#define PARSER_MAX_MESSAGE 512

typedef struct {
    bool success;
    char message[PARSER_MAX_MESSAGE];
    ParseTreeNode *tree;
} SyntaxResult;

bool analyzeSyntaxFile(const char *inputPath, SyntaxResult *result);
void freeSyntaxResult(SyntaxResult *result);

#endif

