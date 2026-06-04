#ifndef STACK_MACHINE_EXECUTOR_H
#define STACK_MACHINE_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "../intermediate/intermediate_code.h"
#include "runtime_memory.h"
#include "runtime_stack.h"

#define STACK_MACHINE_DEFAULT_MAX_STACK 4096
#define STACK_MACHINE_DEFAULT_MAX_CALL_DEPTH 1024
#define STACK_MACHINE_FRAME_CANARY 0x4D344341u

typedef struct {
    RuntimeStack stack;
    RuntimeMemory memory;
    size_t callDepth;
    size_t maxCallDepth;
    size_t bp;
    size_t frameSlots;
    long long returnOffset;
    char *output;
    size_t outputLength;
    size_t outputCapacity;
    char error[512];
    size_t pc;
    bool stopped;
} StackMachineExecutor;

void stackMachineExecutorInit(StackMachineExecutor *executor);
void stackMachineExecutorDestroy(StackMachineExecutor *executor);
bool stackMachineExecute(StackMachineExecutor *executor, const InstructionList *instructions);
const char *stackMachineOutput(const StackMachineExecutor *executor);
const char *stackMachineError(const StackMachineExecutor *executor);

#endif
