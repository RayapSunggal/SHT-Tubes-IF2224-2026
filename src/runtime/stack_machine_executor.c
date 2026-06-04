#include "stack_machine_executor.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../semantic/symbol_table.h"

#define ARION_INT_MIN (-2147483647LL - 1LL)
#define ARION_INT_MAX 2147483647LL
#define FRAME_META_COUNT 5

static void clearError(StackMachineExecutor *executor) {
    if (executor != NULL) {
        executor->error[0] = '\0';
    }
}

static void setRuntimeError(StackMachineExecutor *executor, const Instruction *instruction, const char *format, ...) {
    va_list args;
    char detail[256];

    if (executor == NULL) {
        return;
    }

    va_start(args, format);
    (void)vsnprintf(detail, sizeof(detail), format, args);
    va_end(args);

    if (instruction != NULL) {
        (void)snprintf(executor->error, sizeof(executor->error),
                       "Runtime Error at PC=%zu during %s %d %lld: %s",
                       executor->pc,
                       opcodeMnemonic(instruction->opcode),
                       instruction->level,
                       instruction->opcode == OPCODE_LIT ? 0 : instruction->operand,
                       detail);
    } else {
        (void)snprintf(executor->error, sizeof(executor->error), "Runtime Error: %s", detail);
    }
}

static bool appendOutput(StackMachineExecutor *executor, const char *text) {
    size_t textLength;
    size_t needed;
    size_t newCapacity;
    char *newOutput;

    if (executor == NULL || text == NULL) {
        return false;
    }

    textLength = strlen(text);
    needed = executor->outputLength + textLength + 1;

    if (needed > executor->outputCapacity) {
        newCapacity = executor->outputCapacity == 0 ? 64 : executor->outputCapacity;
        while (newCapacity < needed) {
            newCapacity *= 2;
        }

        newOutput = (char *)realloc(executor->output, newCapacity);
        if (newOutput == NULL) {
            setRuntimeError(executor, NULL, "Output allocation failed");
            return false;
        }

        executor->output = newOutput;
        executor->outputCapacity = newCapacity;
    }

    memcpy(executor->output + executor->outputLength, text, textLength + 1);
    executor->outputLength += textLength;
    return true;
}

static bool appendValueOutput(StackMachineExecutor *executor, RuntimeValue value, bool newline) {
    char buffer[256];

    runtimeValueToString(value, buffer, sizeof(buffer));
    if (!appendOutput(executor, buffer)) {
        return false;
    }
    if (newline && !appendOutput(executor, "\n")) {
        return false;
    }
    return true;
}

static bool stackPop(StackMachineExecutor *executor, const Instruction *instruction, RuntimeValue *out) {
    char error[256];

    error[0] = '\0';
    if (!runtimeStackPop(&executor->stack, out, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s", error[0] != '\0' ? error : "Stack Underflow");
        return false;
    }
    return true;
}

static bool stackPush(StackMachineExecutor *executor, const Instruction *instruction, RuntimeValue value) {
    char error[256];

    if (value.type == RUNTIME_VALUE_INTEGER) {
        if (value.integerValue > ARION_INT_MAX) {
            setRuntimeError(executor, instruction, "Numerical Overflow");
            return false;
        }
        if (value.integerValue < ARION_INT_MIN) {
            setRuntimeError(executor, instruction, "Numerical Underflow");
            return false;
        }
    }

    error[0] = '\0';
    if (!runtimeStackPush(&executor->stack, value, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s", error[0] != '\0' ? error : "Stack Overflow");
        return false;
    }
    return true;
}

static bool memoryGet(StackMachineExecutor *executor, const Instruction *instruction, long long address, RuntimeValue *out) {
    char error[256];

    error[0] = '\0';
    if (!runtimeMemoryGet(&executor->memory, address, out, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s at address %lld",
                        error[0] != '\0' ? error : "Memory Access Out of Bounds",
                        address);
        return false;
    }
    return true;
}

static bool memorySet(StackMachineExecutor *executor, const Instruction *instruction, long long address, RuntimeValue value) {
    char error[256];

    error[0] = '\0';
    if (!runtimeMemorySet(&executor->memory, address, value, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s at address %lld",
                        error[0] != '\0' ? error : "Memory Access Out of Bounds",
                        address);
        return false;
    }
    return true;
}

static bool validateTarget(StackMachineExecutor *executor, const InstructionList *instructions,
                           const Instruction *instruction, long long target) {
    if (target < 0 || (size_t)target >= instructions->count) {
        setRuntimeError(executor, instruction, "Invalid Jump Target %lld", target);
        return false;
    }
    return true;
}

static bool stackGetAt(StackMachineExecutor *executor, const Instruction *instruction, size_t index, RuntimeValue *out) {
    char error[256];

    error[0] = '\0';
    if (!runtimeStackGetAt(&executor->stack, index, out, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s", error[0] != '\0' ? error : "Stack Frame Access Out of Bounds");
        return false;
    }
    return true;
}

static bool stackSetAt(StackMachineExecutor *executor, const Instruction *instruction, size_t index, RuntimeValue value) {
    char error[256];

    error[0] = '\0';
    if (!runtimeStackSetAt(&executor->stack, index, value, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s", error[0] != '\0' ? error : "Stack Frame Access Out of Bounds");
        return false;
    }
    return true;
}

static bool stackTruncate(StackMachineExecutor *executor, const Instruction *instruction, size_t newSize) {
    char error[256];

    error[0] = '\0';
    if (!runtimeStackTruncate(&executor->stack, newSize, error, sizeof(error))) {
        setRuntimeError(executor, instruction, "%s", error[0] != '\0' ? error : "Stack Corruption detected");
        return false;
    }
    return true;
}

static bool stackReadInteger(StackMachineExecutor *executor, const Instruction *instruction,
                             size_t index, long long *value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    if (!stackGetAt(executor, instruction, index, &runtimeValue)) {
        return false;
    }

    if (!runtimeValueIsNumeric(runtimeValue)) {
        runtimeValueFree(&runtimeValue);
        setRuntimeError(executor, instruction, "Stack Corruption detected");
        return false;
    }

    *value = runtimeValueAsInteger(runtimeValue);
    runtimeValueFree(&runtimeValue);
    return true;
}

static int tabEntrySize(int tabIndex) {
    int size = sizeOfType(tab[tabIndex].type, tab[tabIndex].ref);
    return size > 0 ? size : 1;
}

static int parameterSlotCount(int blockIndex) {
    int slots = 0;
    int count = symTabCount();

    for (int i = 0; i < count; i++) {
        if (symBlockForTabIndex(i) == blockIndex && tab[i].obj == OBJ_VARIABLE && tab[i].nrm == 0) {
            slots += tabEntrySize(i);
        }
    }

    return slots;
}

static int functionReturnOffset(int functionIndex) {
    int count = symTabCount();

    if (functionIndex < 0 || functionIndex >= count || tab[functionIndex].identifier == NULL ||
        tab[functionIndex].obj != OBJ_FUNCTION) {
        return -1;
    }

    for (int i = functionIndex + 1; i < count; i++) {
        if (tab[i].obj == OBJ_VARIABLE &&
            tab[i].lev == tab[functionIndex].lev + 1 &&
            tab[i].identifier != NULL &&
            strcasecmp(tab[i].identifier, tab[functionIndex].identifier) == 0) {
            return symFrameOffsetForTabIndex(i);
        }
    }

    return -1;
}

static bool pushInteger(StackMachineExecutor *executor, const Instruction *instruction, long long value) {
    RuntimeValue runtimeValue = runtimeValueInteger(value);
    bool ok = stackPush(executor, instruction, runtimeValue);
    runtimeValueFree(&runtimeValue);
    return ok;
}

static bool buildFrameSlots(StackMachineExecutor *executor, const Instruction *instruction,
                            int blockIndex, int slotCount, int paramSlots, RuntimeValue **outSlots) {
    RuntimeValue *slots;
    size_t stackSize = runtimeStackSize(&executor->stack);
    size_t paramStart;
    int paramCursor = 0;
    int count = symTabCount();

    if (paramSlots < 0 || slotCount < paramSlots || stackSize < (size_t)paramSlots) {
        setRuntimeError(executor, instruction, "Stack Underflow");
        return false;
    }

    slots = (RuntimeValue *)calloc(slotCount == 0 ? 1 : (size_t)slotCount, sizeof(RuntimeValue));
    if (slots == NULL) {
        setRuntimeError(executor, instruction, "Call stack allocation failed");
        return false;
    }

    for (int i = 0; i < slotCount; i++) {
        slots[i] = runtimeValueNone();
    }

    paramStart = stackSize - (size_t)paramSlots;
    for (int i = 0; i < count; i++) {
        if (symBlockForTabIndex(i) == blockIndex && tab[i].obj == OBJ_VARIABLE && tab[i].nrm == 0) {
            int offset = symFrameOffsetForTabIndex(i);
            int size = tabEntrySize(i);
            for (int j = 0; j < size; j++) {
                RuntimeValue value = runtimeValueNone();
                if (paramCursor >= paramSlots ||
                    offset < 0 || offset + j >= slotCount ||
                    !stackGetAt(executor, instruction, paramStart + (size_t)paramCursor, &value)) {
                    for (int k = 0; k < slotCount; k++) {
                        runtimeValueFree(&slots[k]);
                    }
                    free(slots);
                    return false;
                }
                runtimeValueFree(&slots[offset + j]);
                slots[offset + j] = value;
                paramCursor++;
            }
        }
    }

    *outSlots = slots;
    return true;
}

static bool pushCallFrame(StackMachineExecutor *executor, const Instruction *instruction, size_t returnPc) {
    int calleeIndex = instruction->level;
    int blockIndex = -1;
    int slotCount = 0;
    int paramSlots = 0;
    int returnOffset = -1;
    RuntimeValue *slots = NULL;
    size_t frameBp;
    size_t previousBp = executor->bp;
    size_t previousSlots = executor->frameSlots;
    long long previousReturnOffset = executor->returnOffset;

    if (executor->callDepth >= executor->maxCallDepth) {
        setRuntimeError(executor, instruction, "Stack Overflow");
        return false;
    }

    if (calleeIndex > 0 && calleeIndex < symTabCount() &&
        (tab[calleeIndex].obj == OBJ_PROCEDURE || tab[calleeIndex].obj == OBJ_FUNCTION)) {
        blockIndex = tab[calleeIndex].ref;
        slotCount = symFrameSlotCountForBlock(blockIndex);
        paramSlots = parameterSlotCount(blockIndex);
        returnOffset = functionReturnOffset(calleeIndex);
    }

    if (!buildFrameSlots(executor, instruction, blockIndex, slotCount, paramSlots, &slots)) {
        return false;
    }

    frameBp = runtimeStackSize(&executor->stack) - (size_t)paramSlots;
    if (!stackTruncate(executor, instruction, frameBp)) {
        for (int i = 0; i < slotCount; i++) {
            runtimeValueFree(&slots[i]);
        }
        free(slots);
        return false;
    }

    for (int i = 0; i < slotCount; i++) {
        if (!stackPush(executor, instruction, slots[i])) {
            for (int j = 0; j < slotCount; j++) {
                runtimeValueFree(&slots[j]);
            }
            free(slots);
            return false;
        }
    }

    for (int i = 0; i < slotCount; i++) {
        runtimeValueFree(&slots[i]);
    }
    free(slots);

    if (!pushInteger(executor, instruction, (long long)previousBp) ||
        !pushInteger(executor, instruction, (long long)previousSlots) ||
        !pushInteger(executor, instruction, previousReturnOffset) ||
        !pushInteger(executor, instruction, (long long)returnPc) ||
        !pushInteger(executor, instruction, STACK_MACHINE_FRAME_CANARY)) {
        return false;
    }

    executor->bp = frameBp;
    executor->frameSlots = (size_t)slotCount;
    executor->returnOffset = returnOffset;
    executor->callDepth++;
    return true;
}

static bool popCallFrame(StackMachineExecutor *executor, const Instruction *instruction, size_t *returnPc) {
    size_t metaStart;
    long long previousBp;
    long long previousSlots;
    long long previousReturnOffset;
    long long returnTarget;
    long long canary;
    RuntimeValue returnValue = runtimeValueNone();
    bool hasReturnValue = false;

    if (executor->callDepth == 0) {
        return false;
    }

    metaStart = executor->bp + executor->frameSlots;
    if (runtimeStackSize(&executor->stack) != metaStart + FRAME_META_COUNT) {
        setRuntimeError(executor, instruction, "Stack Corruption detected");
        return false;
    }

    if (!stackReadInteger(executor, instruction, metaStart, &previousBp) ||
        !stackReadInteger(executor, instruction, metaStart + 1, &previousSlots) ||
        !stackReadInteger(executor, instruction, metaStart + 2, &previousReturnOffset) ||
        !stackReadInteger(executor, instruction, metaStart + 3, &returnTarget) ||
        !stackReadInteger(executor, instruction, metaStart + 4, &canary)) {
        return false;
    }

    if (canary != STACK_MACHINE_FRAME_CANARY) {
        setRuntimeError(executor, instruction, "Stack Corruption detected");
        return false;
    }

    if (executor->returnOffset >= 0) {
        if ((size_t)executor->returnOffset >= executor->frameSlots ||
            !stackGetAt(executor, instruction, executor->bp + (size_t)executor->returnOffset, &returnValue)) {
            return false;
        }
        hasReturnValue = true;
    }

    if (!stackTruncate(executor, instruction, executor->bp)) {
        runtimeValueFree(&returnValue);
        return false;
    }

    executor->bp = previousBp < 0 ? 0 : (size_t)previousBp;
    executor->frameSlots = previousSlots < 0 ? 0 : (size_t)previousSlots;
    executor->returnOffset = previousReturnOffset;
    executor->callDepth--;

    if (hasReturnValue) {
        if (!stackPush(executor, instruction, returnValue)) {
            runtimeValueFree(&returnValue);
            return false;
        }
        runtimeValueFree(&returnValue);
    }

    *returnPc = (size_t)returnTarget;
    return true;
}

static bool popNumericPair(StackMachineExecutor *executor, const Instruction *instruction,
                           RuntimeValue *left, RuntimeValue *right) {
    if (!stackPop(executor, instruction, right)) {
        return false;
    }
    if (!stackPop(executor, instruction, left)) {
        runtimeValueFree(right);
        return false;
    }
    if (!runtimeValueIsNumeric(*left) || !runtimeValueIsNumeric(*right)) {
        runtimeValueFree(left);
        runtimeValueFree(right);
        setRuntimeError(executor, instruction, "%s requires numeric operands", oprCodeName((int)instruction->operand));
        return false;
    }
    return true;
}

static bool popAddress(StackMachineExecutor *executor, const Instruction *instruction, long long *address) {
    RuntimeValue value = runtimeValueNone();

    if (!stackPop(executor, instruction, &value)) {
        return false;
    }

    if (!runtimeValueIsNumeric(value)) {
        runtimeValueFree(&value);
        setRuntimeError(executor, instruction, "address operand must be numeric");
        return false;
    }

    *address = runtimeValueAsInteger(value);
    runtimeValueFree(&value);
    return true;
}

static bool decodeFrameAddress(long long address, size_t *offset) {
    if (address >= IC_FRAME_ADDRESS_BASE && address < 0) {
        *offset = (size_t)(address - IC_FRAME_ADDRESS_BASE);
        return true;
    }
    return false;
}

static bool frameGet(StackMachineExecutor *executor, const Instruction *instruction, size_t offset, RuntimeValue *out) {
    if (executor->callDepth == 0 || offset >= executor->frameSlots) {
        setRuntimeError(executor, instruction, "Stack Frame Access Out of Bounds");
        return false;
    }
    return stackGetAt(executor, instruction, executor->bp + offset, out);
}

static bool frameSet(StackMachineExecutor *executor, const Instruction *instruction, size_t offset, RuntimeValue value) {
    if (executor->callDepth == 0 || offset >= executor->frameSlots) {
        setRuntimeError(executor, instruction, "Stack Frame Access Out of Bounds");
        return false;
    }
    return stackSetAt(executor, instruction, executor->bp + offset, value);
}

static bool checkedIntResult(StackMachineExecutor *executor, const Instruction *instruction, long long value) {
    RuntimeValue result;
    bool ok;

    if (value > ARION_INT_MAX) {
        setRuntimeError(executor, instruction, "Numerical Overflow");
        return false;
    }
    if (value < ARION_INT_MIN) {
        setRuntimeError(executor, instruction, "Numerical Underflow");
        return false;
    }

    result = runtimeValueInteger(value);
    ok = stackPush(executor, instruction, result);
    runtimeValueFree(&result);
    return ok;
}

static bool pushRealResult(StackMachineExecutor *executor, const Instruction *instruction, double value) {
    RuntimeValue result = runtimeValueReal(value);
    bool ok = stackPush(executor, instruction, result);
    runtimeValueFree(&result);
    return ok;
}

static bool compareValues(RuntimeValue left, RuntimeValue right, OprCode code, bool *result) {
    if (runtimeValueIsNumeric(left) && runtimeValueIsNumeric(right)) {
        double l = runtimeValueAsDouble(left);
        double r = runtimeValueAsDouble(right);

        switch (code) {
            case OPR_EQL: *result = l == r; return true;
            case OPR_NEQ: *result = l != r; return true;
            case OPR_LSS: *result = l < r; return true;
            case OPR_GEQ: *result = l >= r; return true;
            case OPR_GTR: *result = l > r; return true;
            case OPR_LEQ: *result = l <= r; return true;
            default: return false;
        }
    }

    if (left.type == RUNTIME_VALUE_STRING && right.type == RUNTIME_VALUE_STRING) {
        int cmp = strcmp(left.stringValue != NULL ? left.stringValue : "",
                         right.stringValue != NULL ? right.stringValue : "");

        switch (code) {
            case OPR_EQL: *result = cmp == 0; return true;
            case OPR_NEQ: *result = cmp != 0; return true;
            case OPR_LSS: *result = cmp < 0; return true;
            case OPR_GEQ: *result = cmp >= 0; return true;
            case OPR_GTR: *result = cmp > 0; return true;
            case OPR_LEQ: *result = cmp <= 0; return true;
            default: return false;
        }
    }

    return false;
}

static bool executeNeg(StackMachineExecutor *executor, const Instruction *instruction) {
    RuntimeValue operand = runtimeValueNone();
    bool ok;

    if (!stackPop(executor, instruction, &operand)) {
        return false;
    }
    if (!runtimeValueIsNumeric(operand)) {
        runtimeValueFree(&operand);
        setRuntimeError(executor, instruction, "NEG requires numeric operand");
        return false;
    }

    if (operand.type == RUNTIME_VALUE_REAL) {
        ok = pushRealResult(executor, instruction, -runtimeValueAsDouble(operand));
    } else {
        long long value = runtimeValueAsInteger(operand);
        if (value == ARION_INT_MIN) {
            runtimeValueFree(&operand);
            setRuntimeError(executor, instruction, "Numerical Overflow");
            return false;
        }
        ok = checkedIntResult(executor, instruction, -value);
    }

    runtimeValueFree(&operand);
    return ok;
}

static bool executeArithmetic(StackMachineExecutor *executor, const Instruction *instruction, OprCode code) {
    RuntimeValue left = runtimeValueNone();
    RuntimeValue right = runtimeValueNone();
    bool ok = false;

    if (!popNumericPair(executor, instruction, &left, &right)) {
        return false;
    }

    if (code == OPR_RDIV || left.type == RUNTIME_VALUE_REAL || right.type == RUNTIME_VALUE_REAL) {
        double l = runtimeValueAsDouble(left);
        double r = runtimeValueAsDouble(right);
        switch (code) {
            case OPR_ADD: ok = pushRealResult(executor, instruction, l + r); break;
            case OPR_SUB: ok = pushRealResult(executor, instruction, l - r); break;
            case OPR_MUL: ok = pushRealResult(executor, instruction, l * r); break;
            case OPR_DIV:
            case OPR_RDIV:
                if (runtimeValueIsZero(right)) {
                    setRuntimeError(executor, instruction, "Division by Zero in %s", oprCodeName(code));
                } else {
                    ok = pushRealResult(executor, instruction, l / r);
                }
                break;
            case OPR_MOD:
                setRuntimeError(executor, instruction, "MOD requires integer operands");
                break;
            default:
                setRuntimeError(executor, instruction, "Invalid arithmetic OPR Code %d", (int)code);
                break;
        }
    } else {
        long long l = runtimeValueAsInteger(left);
        long long r = runtimeValueAsInteger(right);
        switch (code) {
            case OPR_ADD: ok = checkedIntResult(executor, instruction, l + r); break;
            case OPR_SUB: ok = checkedIntResult(executor, instruction, l - r); break;
            case OPR_MUL: ok = checkedIntResult(executor, instruction, l * r); break;
            case OPR_DIV:
                if (r == 0) {
                    setRuntimeError(executor, instruction, "Division by Zero in DIV");
                } else if (l == ARION_INT_MIN && r == -1) {
                    setRuntimeError(executor, instruction, "Numerical Overflow");
                } else {
                    ok = checkedIntResult(executor, instruction, l / r);
                }
                break;
            case OPR_MOD:
                if (r == 0) {
                    setRuntimeError(executor, instruction, "Division by Zero in MOD");
                } else {
                    ok = checkedIntResult(executor, instruction, l % r);
                }
                break;
            default:
                setRuntimeError(executor, instruction, "Invalid arithmetic OPR Code %d", (int)code);
                break;
        }
    }

    runtimeValueFree(&left);
    runtimeValueFree(&right);
    return ok;
}

static bool executeComparison(StackMachineExecutor *executor, const Instruction *instruction, OprCode code) {
    RuntimeValue left = runtimeValueNone();
    RuntimeValue right = runtimeValueNone();
    RuntimeValue resultValue;
    bool result = false;
    bool ok;

    if (!stackPop(executor, instruction, &right)) {
        return false;
    }
    if (!stackPop(executor, instruction, &left)) {
        runtimeValueFree(&right);
        return false;
    }

    if (!compareValues(left, right, code, &result)) {
        runtimeValueFree(&left);
        runtimeValueFree(&right);
        setRuntimeError(executor, instruction, "%s operands are incompatible", oprCodeName(code));
        return false;
    }

    resultValue = runtimeValueBoolean(result);
    ok = stackPush(executor, instruction, resultValue);
    runtimeValueFree(&resultValue);
    runtimeValueFree(&left);
    runtimeValueFree(&right);
    return ok;
}

static bool executeWrite(StackMachineExecutor *executor, const Instruction *instruction, bool newline) {
    RuntimeValue value = runtimeValueNone();
    bool ok;

    if (!stackPop(executor, instruction, &value)) {
        return false;
    }

    ok = appendValueOutput(executor, value, newline);
    runtimeValueFree(&value);
    return ok;
}

static bool executeToReal(StackMachineExecutor *executor, const Instruction *instruction) {
    RuntimeValue value = runtimeValueNone();
    bool ok;

    if (!stackPop(executor, instruction, &value)) {
        return false;
    }
    if (!runtimeValueIsNumeric(value)) {
        runtimeValueFree(&value);
        setRuntimeError(executor, instruction, "TO_REAL requires numeric operand");
        return false;
    }

    ok = pushRealResult(executor, instruction, runtimeValueAsDouble(value));
    runtimeValueFree(&value);
    return ok;
}

static bool executeRangeError(StackMachineExecutor *executor,
                              const Instruction *instruction,
                              const char *name,
                              const char *valueName) {
    RuntimeValue value = runtimeValueNone();
    RuntimeValue low = runtimeValueNone();
    RuntimeValue high = runtimeValueNone();
    long long valueInt;
    long long lowInt;
    long long highInt;

    if (!stackPop(executor, instruction, &high)) {
        return false;
    }
    if (!stackPop(executor, instruction, &low)) {
        runtimeValueFree(&high);
        return false;
    }
    if (!stackPop(executor, instruction, &value)) {
        runtimeValueFree(&low);
        runtimeValueFree(&high);
        return false;
    }

    if (!runtimeValueIsNumeric(value) ||
        !runtimeValueIsNumeric(low) ||
        !runtimeValueIsNumeric(high)) {
        runtimeValueFree(&value);
        runtimeValueFree(&low);
        runtimeValueFree(&high);
        setRuntimeError(executor, instruction, "%s operands must be numeric", name);
        return false;
    }

    valueInt = runtimeValueAsInteger(value);
    lowInt = runtimeValueAsInteger(low);
    highInt = runtimeValueAsInteger(high);
    runtimeValueFree(&value);
    runtimeValueFree(&low);
    runtimeValueFree(&high);

    setRuntimeError(executor, instruction, "%s: %s %lld outside range %lld..%lld",
                    name, valueName, valueInt, lowInt, highInt);
    return false;
}

static bool executeOpr(StackMachineExecutor *executor, const Instruction *instruction) {
    OprCode code = (OprCode)instruction->operand;

    switch (code) {
        case OPR_NEG:
            return executeNeg(executor, instruction);
        case OPR_ADD:
        case OPR_SUB:
        case OPR_MUL:
        case OPR_DIV:
        case OPR_RDIV:
        case OPR_MOD:
            return executeArithmetic(executor, instruction, code);
        /* Internal OPR extensions above the guidebook range 1..14. */
        case OPR_TO_REAL:
            return executeToReal(executor, instruction);
        case OPR_INDEX_ERROR:
            return executeRangeError(executor, instruction, "IndexOutOfBoundsException", "index");
        case OPR_RANGE_ERROR:
            return executeRangeError(executor, instruction, "RangeCheckException", "value");
        case OPR_EQL:
        case OPR_NEQ:
        case OPR_LSS:
        case OPR_GEQ:
        case OPR_GTR:
        case OPR_LEQ:
            return executeComparison(executor, instruction, code);
        case OPR_WRT:
            return executeWrite(executor, instruction, false);
        case OPR_WRTLN:
            return executeWrite(executor, instruction, true);
        default:
            setRuntimeError(executor, instruction, "Invalid OPR Code %lld", instruction->operand);
            return false;
    }
}

void stackMachineExecutorInit(StackMachineExecutor *executor) {
    if (executor == NULL) {
        return;
    }

    runtimeStackInit(&executor->stack, STACK_MACHINE_DEFAULT_MAX_STACK);
    runtimeMemoryInit(&executor->memory);
    executor->callDepth = 0;
    executor->maxCallDepth = STACK_MACHINE_DEFAULT_MAX_CALL_DEPTH;
    executor->bp = 0;
    executor->frameSlots = 0;
    executor->returnOffset = -1;
    executor->output = NULL;
    executor->outputLength = 0;
    executor->outputCapacity = 0;
    executor->pc = 0;
    executor->stopped = false;
    clearError(executor);
}

void stackMachineExecutorDestroy(StackMachineExecutor *executor) {
    if (executor == NULL) {
        return;
    }

    runtimeStackDestroy(&executor->stack);
    runtimeMemoryDestroy(&executor->memory);
    free(executor->output);
    stackMachineExecutorInit(executor);
}

bool stackMachineExecute(StackMachineExecutor *executor, const InstructionList *instructions) {
    if (executor == NULL || instructions == NULL) {
        return false;
    }

    clearError(executor);
    executor->pc = 0;
    executor->stopped = false;
    executor->callDepth = 0;
    executor->bp = 0;
    executor->frameSlots = 0;
    executor->returnOffset = -1;
    executor->outputLength = 0;
    if (executor->output != NULL) {
        executor->output[0] = '\0';
    }
    runtimeStackClear(&executor->stack);
    runtimeMemoryDestroy(&executor->memory);

    while (executor->pc < instructions->count && !executor->stopped) {
        const Instruction *instruction = &instructions->items[executor->pc];

        switch (instruction->opcode) {
            case OPCODE_INT:
                if (instruction->operand < 0 ||
                    !runtimeMemoryAllocate(&executor->memory, (size_t)instruction->operand,
                                           executor->error, sizeof(executor->error))) {
                    if (executor->error[0] == '\0') {
                        setRuntimeError(executor, instruction, "INT memory allocation failed");
                    }
                    return false;
                }
                executor->pc++;
                break;
            case OPCODE_LIT:
                if (!stackPush(executor, instruction, instruction->literal)) {
                    return false;
                }
                executor->pc++;
                break;
            case OPCODE_LOD: {
                RuntimeValue value = runtimeValueNone();
                long long address = instruction->operand;
                size_t frameOffset;
                if (instruction->level == 1 && !popAddress(executor, instruction, &address)) {
                    return false;
                }
                if (instruction->level == 2) {
                    if (instruction->operand < 0 ||
                        !frameGet(executor, instruction, (size_t)instruction->operand, &value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else if (instruction->level == 1 && decodeFrameAddress(address, &frameOffset)) {
                    if (!frameGet(executor, instruction, frameOffset, &value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else if (instruction->level == 0 || instruction->level == 1) {
                    if (!memoryGet(executor, instruction, address, &value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else {
                    setRuntimeError(executor, instruction, "Invalid LOD addressing mode %d", instruction->level);
                    return false;
                }
                if (!stackPush(executor, instruction, value)) {
                    runtimeValueFree(&value);
                    return false;
                }
                runtimeValueFree(&value);
                executor->pc++;
                break;
            }
            case OPCODE_STO: {
                RuntimeValue value = runtimeValueNone();
                long long address = instruction->operand;
                size_t frameOffset;
                if (instruction->level == 1 && !popAddress(executor, instruction, &address)) {
                    return false;
                }
                if (!stackPop(executor, instruction, &value)) {
                    return false;
                }
                if (instruction->level == 2) {
                    if (instruction->operand < 0 ||
                        !frameSet(executor, instruction, (size_t)instruction->operand, value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else if (instruction->level == 1 && decodeFrameAddress(address, &frameOffset)) {
                    if (!frameSet(executor, instruction, frameOffset, value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else if (instruction->level == 0 || instruction->level == 1) {
                    if (!memorySet(executor, instruction, address, value)) {
                        runtimeValueFree(&value);
                        return false;
                    }
                } else {
                    setRuntimeError(executor, instruction, "Invalid STO addressing mode %d", instruction->level);
                    runtimeValueFree(&value);
                    return false;
                }
                runtimeValueFree(&value);
                executor->pc++;
                break;
            }
            case OPCODE_CAL:
                if (!validateTarget(executor, instructions, instruction, instruction->operand) ||
                    !pushCallFrame(executor, instruction, executor->pc + 1)) {
                    return false;
                }
                executor->pc = (size_t)instruction->operand;
                break;
            case OPCODE_JMP:
                if (!validateTarget(executor, instructions, instruction, instruction->operand)) {
                    return false;
                }
                executor->pc = (size_t)instruction->operand;
                break;
            case OPCODE_JPC: {
                RuntimeValue condition = runtimeValueNone();
                bool jump;
                if (!stackPop(executor, instruction, &condition)) {
                    return false;
                }
                jump = runtimeValueIsZero(condition);
                runtimeValueFree(&condition);
                if (jump) {
                    if (!validateTarget(executor, instructions, instruction, instruction->operand)) {
                        return false;
                    }
                    executor->pc = (size_t)instruction->operand;
                } else {
                    executor->pc++;
                }
                break;
            }
            case OPCODE_OPR:
                if (!executeOpr(executor, instruction)) {
                    return false;
                }
                executor->pc++;
                break;
            case OPCODE_RET: {
                size_t returnPc;
                if (popCallFrame(executor, instruction, &returnPc)) {
                    if (returnPc >= instructions->count) {
                        setRuntimeError(executor, instruction, "Invalid Return Target %zu", returnPc);
                        return false;
                    }
                    executor->pc = returnPc;
                } else if (executor->error[0] != '\0') {
                    return false;
                } else {
                    executor->stopped = true;
                }
                break;
            }
            default:
                setRuntimeError(executor, instruction, "Invalid opcode %d", (int)instruction->opcode);
                return false;
        }
    }

    if (!executor->stopped && executor->pc >= instructions->count && executor->callDepth > 0) {
        setRuntimeError(executor, NULL, "Stack Corruption detected");
        return false;
    }

    return true;
}

const char *stackMachineOutput(const StackMachineExecutor *executor) {
    if (executor == NULL || executor->output == NULL) {
        return "";
    }
    return executor->output;
}

const char *stackMachineError(const StackMachineExecutor *executor) {
    if (executor == NULL) {
        return "Runtime Error: executor is not initialized";
    }
    return executor->error;
}
