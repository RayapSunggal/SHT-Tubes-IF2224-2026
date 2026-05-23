#include "parse_tree.h"

#include <stdlib.h>
#include <string.h>

#define TREE_BRANCH "|-- "
#define TREE_LAST "`-- "
#define TREE_CONT "|   "
#define TREE_SPACE "    "

static char *cloneString(const char *src) {
    char *copy;
    size_t len;

    if (src==NULL) {
        return NULL;
    }

    len=strlen(src);
    copy=(char *)malloc(len + 1);
    if (copy==NULL) {
        return NULL;
    }

    memcpy(copy, src, len + 1);
    return copy;
}

static char *makeNextPrefix(const char *prefix, bool isLastChild) {
    const char *addition;
    char *newPrefix;
    size_t prefixLen;
    size_t additionLen;

    addition=isLastChild ? TREE_SPACE : TREE_CONT;
    prefixLen=strlen(prefix);
    additionLen=strlen(addition);

    newPrefix=(char *)malloc(prefixLen + additionLen + 1);
    if (newPrefix==NULL) {
        return NULL;
    }

    memcpy(newPrefix, prefix, prefixLen);
    memcpy(newPrefix + prefixLen, addition, additionLen + 1);
    return newPrefix;
}

static void parseTreePrintChildren(const ParseTreeNode *node, FILE *stream, const char *prefix) {
    size_t i;

    for (i=0; i < node->childCount; i++) {
        const ParseTreeNode *child=node->children[i];
        bool isLast=(i + 1)==node->childCount;
        char *nextPrefix;

        fprintf(stream, "%s%s%s\n", prefix, isLast ? TREE_LAST : TREE_BRANCH, child->label);
        nextPrefix=makeNextPrefix(prefix, isLast);
        if (nextPrefix==NULL) {
            return;
        }

        parseTreePrintChildren(child, stream, nextPrefix);
        free(nextPrefix);
    }
}

ParseTreeNode *parseTreeCreateNode(const char *label) {
    ParseTreeNode *node;

    if (label==NULL) {
        return NULL;
    }

    node=(ParseTreeNode *)malloc(sizeof(ParseTreeNode));
    if (node==NULL) {
        return NULL;
    }

    node->label=cloneString(label);
    if (node->label==NULL) {
        free(node);
        return NULL;
    }

    node->children=NULL;
    node->childCount=0;
    node->childCapacity=0;
    return node;
}

bool parseTreeAddChild(ParseTreeNode *parent, ParseTreeNode *child) {
    ParseTreeNode **newChildren;
    size_t newCapacity;

    if (parent==NULL || child==NULL) {
        return false;
    }

    if (parent->childCount==parent->childCapacity) {
        newCapacity=parent->childCapacity==0 ? 4 : parent->childCapacity * 2;
        newChildren=(ParseTreeNode **)realloc(parent->children, newCapacity * sizeof(ParseTreeNode *));
        if (newChildren==NULL) {
            return false;
        }

        parent->children=newChildren;
        parent->childCapacity=newCapacity;
    }

    parent->children[parent->childCount]=child;
    parent->childCount++;
    return true;
}

void parseTreePrint(const ParseTreeNode *root, FILE *stream) {
    if (root==NULL || stream==NULL) {
        return;
    }

    fprintf(stream, "%s\n", root->label);
    parseTreePrintChildren(root, stream, "");
}

void parseTreeFree(ParseTreeNode *root) {
    size_t i;

    if (root==NULL) {
        return;
    }

    for (i=0; i < root->childCount; i++) {
        parseTreeFree(root->children[i]);
    }

    free(root->children);
    free(root->label);
    free(root);
}
