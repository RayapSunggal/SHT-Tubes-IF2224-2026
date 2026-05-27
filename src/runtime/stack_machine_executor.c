#include "stack_machine_executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clearError(StackMachineExecutor *executor) {
    if (executor != NULL) {
        executor->error[0] = '\0';
    }
}

static void setError(StackMachineExecutor *executor, const char *message) {
    if (executor != NULL) {
        (void)snprintf(executor->error, sizeof(executor->error), "%s", message);
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
            setError(executor, "Runtime Error: Output allocation failed");
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

static bool popValue(StackMachineExecutor *executor, RuntimeValue *out) {
    return runtimeStackPop(&executor->stack, out, executor->error, sizeof(executor->error));
}

static bool pushValue(StackMachineExecutor *executor, RuntimeValue value) {
    return runtimeStackPush(&executor->stack, value, executor->error, sizeof(executor->error));
}

static bool popNumericPair(StackMachineExecutor *executor, RuntimeValue *left, RuntimeValue *right) {
    if (!popValue(executor, right)) {
        return false;
    }
    if (!popValue(executor, left)) {
        runtimeValueFree(right);
        return false;
    }
    if (!runtimeValueIsNumeric(*left) || !runtimeValueIsNumeric(*right)) {
        runtimeValueFree(left);
        runtimeValueFree(right);
        setError(executor, "Runtime Error: OPR requires numeric operands");
        return false;
    }
    return true;
}

static bool pushNumericResult(StackMachineExecutor *executor, RuntimeValue left, RuntimeValue right, double result) {
    RuntimeValue value;
    bool ok;

    if (left.type == RUNTIME_VALUE_REAL || right.type == RUNTIME_VALUE_REAL) {
        value = runtimeValueReal(result);
    } else {
        value = runtimeValueInteger((long long)result);
    }

    ok = pushValue(executor, value);
    runtimeValueFree(&value);
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
            default: return false;
        }
    }

    return false;
}

static bool executeNeg(StackMachineExecutor *executor) {
    RuntimeValue operand;
    RuntimeValue result;
    bool ok;

    if (!popValue(executor, &operand)) {
        return false;
    }
    if (!runtimeValueIsNumeric(operand)) {
        runtimeValueFree(&operand);
        setError(executor, "Runtime Error: NEG requires numeric operand");
        return false;
    }

    if (operand.type == RUNTIME_VALUE_REAL) {
        result = runtimeValueReal(-runtimeValueAsDouble(operand));
    } else {
        result = runtimeValueInteger(-runtimeValueAsInteger(operand));
    }

    ok = pushValue(executor, result);
    runtimeValueFree(&operand);
    runtimeValueFree(&result);
    return ok;
}

static bool executeArithmetic(StackMachineExecutor *executor, OprCode code) {
    RuntimeValue left;
    RuntimeValue right;
    bool ok;

    if (!popNumericPair(executor, &left, &right)) {
        return false;
    }

    switch (code) {
        case OPR_ADD:
            ok = pushNumericResult(executor, left, right,
                                   runtimeValueAsDouble(left) + runtimeValueAsDouble(right));
            break;
        case OPR_SUB:
            ok = pushNumericResult(executor, left, right,
                                   runtimeValueAsDouble(left) - runtimeValueAsDouble(right));
            break;
        case OPR_MUL:
            ok = pushNumericResult(executor, left, right,
                                   runtimeValueAsDouble(left) * runtimeValueAsDouble(right));
            break;
        case OPR_DIV:
            if (runtimeValueIsZero(right)) {
                setError(executor, "Runtime Error: Division by Zero");
                ok = false;
            } else {
                if (left.type == RUNTIME_VALUE_REAL || right.type == RUNTIME_VALUE_REAL) {
                    ok = pushNumericResult(executor, left, right,
                                           runtimeValueAsDouble(left) / runtimeValueAsDouble(right));
                } else {
                    RuntimeValue result = runtimeValueInteger(runtimeValueAsInteger(left) /
                                                              runtimeValueAsInteger(right));
                    ok = pushValue(executor, result);
                    runtimeValueFree(&result);
                }
            }
            break;
        case OPR_MOD:
            if (runtimeValueIsZero(right)) {
                setError(executor, "Runtime Error: Division by Zero");
                ok = false;
            } else if (left.type == RUNTIME_VALUE_REAL || right.type == RUNTIME_VALUE_REAL) {
                setError(executor, "Runtime Error: MOD requires integer operands");
                ok = false;
            } else {
                RuntimeValue result = runtimeValueInteger(runtimeValueAsInteger(left) %
                                                          runtimeValueAsInteger(right));
                ok = pushValue(executor, result);
                runtimeValueFree(&result);
            }
            break;
        default:
            setError(executor, "Runtime Error: Unsupported arithmetic OPR");
            ok = false;
            break;
    }

    runtimeValueFree(&left);
    runtimeValueFree(&right);
    return ok;
}

static bool executeComparison(StackMachineExecutor *executor, OprCode code) {
    RuntimeValue left;
    RuntimeValue right;
    RuntimeValue resultValue;
    bool result = false;
    bool ok;

    if (!popValue(executor, &right)) {
        return false;
    }
    if (!popValue(executor, &left)) {
        runtimeValueFree(&right);
        return false;
    }

    if (!compareValues(left, right, code, &result)) {
        runtimeValueFree(&left);
        runtimeValueFree(&right);
        setError(executor, "Runtime Error: OPR comparison operands are incompatible");
        return false;
    }

    resultValue = runtimeValueBoolean(result);
    ok = pushValue(executor, resultValue);
    runtimeValueFree(&resultValue);
    runtimeValueFree(&left);
    runtimeValueFree(&right);
    return ok;
}

static bool executeWrite(StackMachineExecutor *executor, bool newline) {
    RuntimeValue value;
    bool ok;

    if (!popValue(executor, &value)) {
        return false;
    }

    ok = appendValueOutput(executor, value, newline);
    runtimeValueFree(&value);
    return ok;
}

static bool executeOpr(StackMachineExecutor *executor, OprCode code) {
    switch (code) {
        case OPR_NEG:
            return executeNeg(executor);
        case OPR_ADD:
        case OPR_SUB:
        case OPR_MUL:
        case OPR_DIV:
        case OPR_MOD:
            return executeArithmetic(executor, code);
        case OPR_EQL:
        case OPR_NEQ:
        case OPR_LSS:
        case OPR_GEQ:
        case OPR_GTR:
        case OPR_LEQ:
            return executeComparison(executor, code);
        case OPR_WRT:
            return executeWrite(executor, false);
        case OPR_WRTLN:
            return executeWrite(executor, true);
        default:
            setError(executor, "Runtime Error: Unknown OPR code");
            return false;
    }
}

void stackMachineExecutorInit(StackMachineExecutor *executor) {
    if (executor == NULL) {
        return;
    }

    runtimeStackInit(&executor->stack, STACK_MACHINE_DEFAULT_MAX_STACK);
    runtimeMemoryInit(&executor->memory);
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
    executor->outputLength = 0;
    if (executor->output != NULL) {
        executor->output[0] = '\0';
    }
    runtimeStackClear(&executor->stack);

    while (executor->pc < instructions->count && !executor->stopped) {
        const Instruction *instruction = &instructions->items[executor->pc];
        bool advance = true;

        switch (instruction->opcode) {
            case OPCODE_INT:
                if (instruction->operand < 0 ||
                    !runtimeMemoryAllocate(&executor->memory,
                                           (size_t)instruction->operand,
                                           executor->error,
                                           sizeof(executor->error))) {
                    return false;
                }
                break;
            case OPCODE_LIT:
                if (!pushValue(executor, instruction->literal)) {
                    return false;
                }
                break;
            case OPCODE_LOD: {
                RuntimeValue value = runtimeValueNone();
                if (!runtimeMemoryGet(&executor->memory, instruction->operand,
                                      &value, executor->error, sizeof(executor->error))) {
                    return false;
                }
                if (!pushValue(executor, value)) {
                    runtimeValueFree(&value);
                    return false;
                }
                runtimeValueFree(&value);
                break;
            }
            case OPCODE_STO: {
                RuntimeValue value = runtimeValueNone();
                if (!popValue(executor, &value)) {
                    return false;
                }
                if (!runtimeMemorySet(&executor->memory, instruction->operand,
                                      value, executor->error, sizeof(executor->error))) {
                    runtimeValueFree(&value);
                    return false;
                }
                runtimeValueFree(&value);
                break;
            }
            case OPCODE_OPR:
                if (!executeOpr(executor, (OprCode)instruction->operand)) {
                    return false;
                }
                break;
            case OPCODE_RET:
                executor->stopped = true;
                advance = false;
                break;
            default:
                setError(executor, "Runtime Error: Unknown opcode");
                return false;
        }

        if (advance) {
            executor->pc++;
        }
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
