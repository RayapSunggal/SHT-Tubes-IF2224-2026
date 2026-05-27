#ifndef RUNTIME_STACK_H
#define RUNTIME_STACK_H

#include <stdbool.h>
#include <stddef.h>

#include "runtime_value.h"

typedef struct {
    RuntimeValue *items;
    size_t count;
    size_t capacity;
    size_t maxSize;
} RuntimeStack;

void runtimeStackInit(RuntimeStack *stack, size_t maxSize);
void runtimeStackDestroy(RuntimeStack *stack);
bool runtimeStackPush(RuntimeStack *stack, RuntimeValue value, char *error, size_t errorSize);
bool runtimeStackPop(RuntimeStack *stack, RuntimeValue *out, char *error, size_t errorSize);
bool runtimeStackPeek(const RuntimeStack *stack, RuntimeValue *out, char *error, size_t errorSize);
bool runtimeStackIsEmpty(const RuntimeStack *stack);
size_t runtimeStackSize(const RuntimeStack *stack);
void runtimeStackClear(RuntimeStack *stack);

#endif
