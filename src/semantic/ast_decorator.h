#ifndef AST_DECORATOR_H
#define AST_DECORATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../ast/ast_node.h"

bool decorateAst(AstNode *root, char *message, size_t messageSize);

void printDecoratedAst(const AstNode *node, FILE *stream);

#endif
