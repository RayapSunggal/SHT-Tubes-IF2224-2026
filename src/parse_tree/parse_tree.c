#include "parse_tree.h"

#include <stdio.h>
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

static void setReadError(char *message, size_t messageSize, const char *text) {
    if (message != NULL && messageSize > 0) {
        snprintf(message, messageSize, "%s", text);
    }
}

static void stripLineEnding(char *line) {
    size_t len;

    if (line == NULL) {
        return;
    }

    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static int parsePrintedTreeLine(char *line, int *depth, char **label) {
    int pos = 0;

    if (line == NULL || depth == NULL || label == NULL) {
        return 0;
    }

    if (line[0] == '\0') {
        return 0;
    }

    if (line[0] != '|' && line[0] != '`' && line[0] != ' ') {
        *depth = 0;
        *label = line;
        return 1;
    }

    while (line[pos] != '\0') {
        if (strncmp(line + pos, TREE_BRANCH, 4) == 0 ||
            strncmp(line + pos, TREE_LAST, 4) == 0) {
            *depth = pos / 4 + 1;
            *label = line + pos + 4;
            return pos % 4 == 0 && **label != '\0';
        }

        if (strncmp(line + pos, TREE_CONT, 4) == 0 ||
            strncmp(line + pos, TREE_SPACE, 4) == 0) {
            pos += 4;
            continue;
        }

        return 0;
    }

    return 0;
}

ParseTreeNode *parseTreeReadFromFile(const char *path, char *message, size_t messageSize) {
    FILE *stream;
    char line[4096];
    ParseTreeNode *root = NULL;
    ParseTreeNode *stack[256];
    int lineNumber = 0;

    if (message != NULL && messageSize > 0) {
        message[0] = '\0';
    }

    if (path == NULL) {
        setReadError(message, messageSize, "Path parse tree tidak valid.");
        return NULL;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        setReadError(message, messageSize, "Gagal membuka file parse tree.");
        return NULL;
    }

    for (int i = 0; i < 256; i++) {
        stack[i] = NULL;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        int depth;
        char *label;
        ParseTreeNode *node;

        lineNumber++;
        stripLineEnding(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!parsePrintedTreeLine(line, &depth, &label) || depth < 0 || depth >= 256) {
            snprintf(message, messageSize, "Format parse tree tidak valid pada baris %d.", lineNumber);
            parseTreeFree(root);
            fclose(stream);
            return NULL;
        }

        node = parseTreeCreateNode(label);
        if (node == NULL) {
            setReadError(message, messageSize, "Out of memory saat membaca parse tree.");
            parseTreeFree(root);
            fclose(stream);
            return NULL;
        }

        if (depth == 0) {
            if (root != NULL) {
                setReadError(message, messageSize, "Parse tree memiliki lebih dari satu root.");
                parseTreeFree(node);
                parseTreeFree(root);
                fclose(stream);
                return NULL;
            }
            root = node;
            stack[0] = node;
            continue;
        }

        if (root == NULL || stack[depth - 1] == NULL) {
            snprintf(message, messageSize, "Parent parse tree tidak ditemukan pada baris %d.", lineNumber);
            parseTreeFree(node);
            parseTreeFree(root);
            fclose(stream);
            return NULL;
        }

        if (!parseTreeAddChild(stack[depth - 1], node)) {
            setReadError(message, messageSize, "Out of memory saat menyusun parse tree.");
            parseTreeFree(node);
            parseTreeFree(root);
            fclose(stream);
            return NULL;
        }

        stack[depth] = node;
        for (int i = depth + 1; i < 256; i++) {
            stack[i] = NULL;
        }
    }

    fclose(stream);

    if (root == NULL) {
        setReadError(message, messageSize, "File parse tree kosong.");
        return NULL;
    }

    return root;
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
