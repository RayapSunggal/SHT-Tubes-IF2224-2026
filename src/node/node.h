#ifndef NODE_H
#define NODE_H

typedef struct Node {
    char *name;
    struct Node **child;
    int childCount;
    int childCapacity;
} Node;

#endif
