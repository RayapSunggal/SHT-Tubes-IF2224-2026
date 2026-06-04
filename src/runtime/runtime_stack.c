#include "runtime_stack.h"

#include <stdio.h>
#include <stdlib.h>

static void setError(char *error, size_t errorSize, const char *message) {
    if (error != NULL && errorSize > 0) {
        (void)snprintf(error, errorSize, "%s", message);
    }
}

void runtimeStackInit(RuntimeStack *stack, size_t maxSize) {
    if (stack == NULL) {
        return;
    }

    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
    stack->maxSize = maxSize;
}

void runtimeStackDestroy(RuntimeStack *stack) {
    if (stack == NULL) {
        return;
    }

    runtimeStackClear(stack);
    free(stack->items);
    runtimeStackInit(stack, 0);
}

bool runtimeStackPush(RuntimeStack *stack, RuntimeValue value, char *error, size_t errorSize) {
    RuntimeValue *newItems;
    size_t newCapacity;

    if (stack == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (stack->maxSize > 0 && stack->count >= stack->maxSize) {
        setError(error, errorSize, "Runtime Error: Stack Overflow");
        return false;
    }

    if (stack->count == stack->capacity) {
        newCapacity = stack->capacity == 0 ? 16 : stack->capacity * 2;
        if (stack->maxSize > 0 && newCapacity > stack->maxSize) {
            newCapacity = stack->maxSize;
        }
        newItems = (RuntimeValue *)realloc(stack->items, newCapacity * sizeof(RuntimeValue));
        if (newItems == NULL) {
            setError(error, errorSize, "Runtime Error: Stack allocation failed");
            return false;
        }
        stack->items = newItems;
        stack->capacity = newCapacity;
    }

    stack->items[stack->count++] = runtimeValueCopy(value);
    return true;
}

bool runtimeStackPop(RuntimeStack *stack, RuntimeValue *out, char *error, size_t errorSize) {
    if (stack == NULL || out == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (stack->count == 0) {
        setError(error, errorSize, "Runtime Error: Stack Underflow");
        return false;
    }

    *out = stack->items[--stack->count];
    stack->items[stack->count] = runtimeValueNone();
    return true;
}

bool runtimeStackPeek(const RuntimeStack *stack, RuntimeValue *out, char *error, size_t errorSize) {
    if (stack == NULL || out == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (stack->count == 0) {
        setError(error, errorSize, "Runtime Error: Stack Underflow");
        return false;
    }

    *out = runtimeValueCopy(stack->items[stack->count - 1]);
    return true;
}

bool runtimeStackGetAt(const RuntimeStack *stack, size_t index, RuntimeValue *out, char *error, size_t errorSize) {
    if (stack == NULL || out == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (index >= stack->count) {
        setError(error, errorSize, "Runtime Error: Stack Frame Access Out of Bounds");
        return false;
    }

    *out = runtimeValueCopy(stack->items[index]);
    return true;
}

bool runtimeStackSetAt(RuntimeStack *stack, size_t index, RuntimeValue value, char *error, size_t errorSize) {
    if (stack == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (index >= stack->count) {
        setError(error, errorSize, "Runtime Error: Stack Frame Access Out of Bounds");
        return false;
    }

    runtimeValueFree(&stack->items[index]);
    stack->items[index] = runtimeValueCopy(value);
    return true;
}

bool runtimeStackIsEmpty(const RuntimeStack *stack) {
    return stack == NULL || stack->count == 0;
}

size_t runtimeStackSize(const RuntimeStack *stack) {
    return stack == NULL ? 0 : stack->count;
}

void runtimeStackClear(RuntimeStack *stack) {
    if (stack == NULL) {
        return;
    }

    for (size_t i = 0; i < stack->count; i++) {
        runtimeValueFree(&stack->items[i]);
    }
    stack->count = 0;
}

bool runtimeStackTruncate(RuntimeStack *stack, size_t newSize, char *error, size_t errorSize) {
    if (stack == NULL) {
        setError(error, errorSize, "Runtime Error: Stack is not initialized");
        return false;
    }

    if (newSize > stack->count) {
        setError(error, errorSize, "Runtime Error: Stack Corruption detected");
        return false;
    }

    for (size_t i = newSize; i < stack->count; i++) {
        runtimeValueFree(&stack->items[i]);
    }
    stack->count = newSize;
    return true;
}
