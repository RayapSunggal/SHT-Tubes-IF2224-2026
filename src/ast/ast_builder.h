#ifndef AST_BUILDER_H
#define AST_BUILDER_H

#include "../parse_tree/parse_tree.h"
#include "ast_node.h"

/*
 * Pake Syntax-Directed Translation (L-Attributed Grammar).
 * Return root AST, atau NULL klo gagal.
 */
AstNode *buildAst(const ParseTreeNode *parseRoot);

#endif
