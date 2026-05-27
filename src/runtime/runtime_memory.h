#ifndef RUNTIME_MEMORY_H
#define RUNTIME_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

#include "runtime_value.h"

typedef struct {
    RuntimeValue *cells;
    size_t size;
} RuntimeMemory;

void runtimeMemoryInit(RuntimeMemory *memory);
void runtimeMemoryDestroy(RuntimeMemory *memory);
bool runtimeMemoryAllocate(RuntimeMemory *memory, size_t size, char *error, size_t errorSize);
bool runtimeMemoryGet(const RuntimeMemory *memory, long long address, RuntimeValue *out, char *error, size_t errorSize);
bool runtimeMemorySet(RuntimeMemory *memory, long long address, RuntimeValue value, char *error, size_t errorSize);
size_t runtimeMemorySize(const RuntimeMemory *memory);

#endif
