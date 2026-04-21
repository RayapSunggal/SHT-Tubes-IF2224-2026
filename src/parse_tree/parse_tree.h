#ifndef PARSE_TREE_H
#define PARSE_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct ParseTreeNode {
    char *label;
    struct ParseTreeNode **children;
    size_t childCount;
    size_t childCapacity;
} ParseTreeNode;

ParseTreeNode *parseTreeCreateNode(const char *label);
bool parseTreeAddChild(ParseTreeNode *parent, ParseTreeNode *child);
void parseTreePrint(const ParseTreeNode *root, FILE *stream);
void parseTreeFree(ParseTreeNode *root);

#endif