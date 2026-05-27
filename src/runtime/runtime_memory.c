#include "runtime_memory.h"

#include <stdio.h>
#include <stdlib.h>

static void setError(char *error, size_t errorSize, const char *message) {
    if (error != NULL && errorSize > 0) {
        (void)snprintf(error, errorSize, "%s", message);
    }
}

static bool validAddress(const RuntimeMemory *memory, long long address, char *error, size_t errorSize) {
    if (memory == NULL || memory->cells == NULL) {
        setError(error, errorSize, "Runtime Error: Memory is not allocated");
        return false;
    }
    if (address < 0 || (size_t)address >= memory->size) {
        setError(error, errorSize, "Runtime Error: Memory Access Out of Bounds");
        return false;
    }
    return true;
}

void runtimeMemoryInit(RuntimeMemory *memory) {
    if (memory == NULL) {
        return;
    }

    memory->cells = NULL;
    memory->size = 0;
}

void runtimeMemoryDestroy(RuntimeMemory *memory) {
    if (memory == NULL) {
        return;
    }

    for (size_t i = 0; i < memory->size; i++) {
        runtimeValueFree(&memory->cells[i]);
    }
    free(memory->cells);
    runtimeMemoryInit(memory);
}

bool runtimeMemoryAllocate(RuntimeMemory *memory, size_t size, char *error, size_t errorSize) {
    RuntimeValue *cells;

    if (memory == NULL) {
        setError(error, errorSize, "Runtime Error: Memory is not initialized");
        return false;
    }

    runtimeMemoryDestroy(memory);

    cells = (RuntimeValue *)calloc(size == 0 ? 1 : size, sizeof(RuntimeValue));
    if (cells == NULL) {
        setError(error, errorSize, "Runtime Error: Memory allocation failed");
        return false;
    }

    memory->cells = cells;
    memory->size = size;
    for (size_t i = 0; i < size; i++) {
        memory->cells[i] = runtimeValueNone();
    }

    return true;
}

bool runtimeMemoryGet(const RuntimeMemory *memory, long long address, RuntimeValue *out, char *error, size_t errorSize) {
    if (out == NULL) {
        setError(error, errorSize, "Runtime Error: Memory output is not initialized");
        return false;
    }
    if (!validAddress(memory, address, error, errorSize)) {
        return false;
    }

    *out = runtimeValueCopy(memory->cells[address]);
    return true;
}

bool runtimeMemorySet(RuntimeMemory *memory, long long address, RuntimeValue value, char *error, size_t errorSize) {
    if (!validAddress(memory, address, error, errorSize)) {
        return false;
    }

    runtimeValueFree(&memory->cells[address]);
    memory->cells[address] = runtimeValueCopy(value);
    return true;
}

size_t runtimeMemorySize(const RuntimeMemory *memory) {
    return memory == NULL ? 0 : memory->size;
}
