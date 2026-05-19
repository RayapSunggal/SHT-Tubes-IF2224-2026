#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include "../parse_tree/parse_tree.h"

bool analyzeSemanticTree(const ParseTreeNode *root, char *message, size_t messageSize);

#endif // SEMANTIC_H
