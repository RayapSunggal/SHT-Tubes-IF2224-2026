#ifndef STACK_MACHINE_EXECUTOR_H
#define STACK_MACHINE_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "../intermediate/intermediate_code.h"
#include "runtime_memory.h"
#include "runtime_stack.h"

#define STACK_MACHINE_DEFAULT_MAX_STACK 1024

typedef struct {
    RuntimeStack stack;
    RuntimeMemory memory;
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
