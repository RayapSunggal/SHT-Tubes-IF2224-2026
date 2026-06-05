#include "intermediate_code.h"

#include <stdlib.h>
#include <string.h>

void instructionListInit(InstructionList *list) {
    if (list == NULL) {
        return;
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->callInfos = NULL;
    list->callInfoCount = 0;
    list->callInfoCapacity = 0;
}

void instructionListFree(InstructionList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        instructionFree(&list->items[i]);
    }
    free(list->items);
    free(list->callInfos);
    instructionListInit(list);
}

bool instructionListEmit(InstructionList *list, Instruction instruction) {
    Instruction *newItems;
    size_t newCapacity;

    if (list == NULL) {
        return false;
    }

    if (list->count == list->capacity) {
        newCapacity = list->capacity == 0 ? 16 : list->capacity * 2;
        newItems = (Instruction *)realloc(list->items, newCapacity * sizeof(Instruction));
        if (newItems == NULL) {
            return false;
        }
        list->items = newItems;
        list->capacity = newCapacity;
    }

    list->items[list->count++] = instructionCopy(instruction);
    return true;
}

bool instructionListAddCallInfo(InstructionList *list, RuntimeCallInfo info) {
    RuntimeCallInfo *newInfos;
    size_t newCapacity;

    if (list == NULL || !info.valid) {
        return false;
    }

    for (size_t i = 0; i < list->callInfoCount; i++) {
        if (list->callInfos[i].target == info.target) {
            list->callInfos[i] = info;
            return true;
        }
    }

    if (list->callInfoCount == list->callInfoCapacity) {
        newCapacity = list->callInfoCapacity == 0 ? 8 : list->callInfoCapacity * 2;
        newInfos = (RuntimeCallInfo *)realloc(list->callInfos,
                                              newCapacity * sizeof(RuntimeCallInfo));
        if (newInfos == NULL) {
            return false;
        }
        list->callInfos = newInfos;
        list->callInfoCapacity = newCapacity;
    }

    list->callInfos[list->callInfoCount++] = info;
    return true;
}

const RuntimeCallInfo *instructionListFindCallInfo(const InstructionList *list, size_t target) {
    if (list == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < list->callInfoCount; i++) {
        if (list->callInfos[i].valid && list->callInfos[i].target == target) {
            return &list->callInfos[i];
        }
    }

    return NULL;
}

bool instructionListPatchOperand(InstructionList *list, size_t index, long long operand) {
    if (list == NULL || index >= list->count) {
        return false;
    }
    list->items[index].operand = operand;
    return true;
}

Instruction instructionCreate(Opcode opcode, int level, long long operand) {
    Instruction instruction;

    instruction.opcode = opcode;
    instruction.level = level;
    instruction.operand = operand;
    instruction.literal = runtimeValueNone();
    return instruction;
}

Instruction instructionCreateLiteral(RuntimeValue value) {
    Instruction instruction;

    instruction.opcode = OPCODE_LIT;
    instruction.level = 0;
    instruction.operand = 0;
    instruction.literal = runtimeValueCopy(value);
    return instruction;
}

Instruction instructionCopy(Instruction instruction) {
    Instruction copy = instruction;
    copy.literal = runtimeValueCopy(instruction.literal);
    return copy;
}

void instructionFree(Instruction *instruction) {
    if (instruction == NULL) {
        return;
    }
    runtimeValueFree(&instruction->literal);
    instruction->opcode = OPCODE_RET;
    instruction->level = 0;
    instruction->operand = 0;
}

const char *opcodeMnemonic(Opcode opcode) {
    switch (opcode) {
        case OPCODE_INT: return "INT";
        case OPCODE_LIT: return "LIT";
        case OPCODE_LOD: return "LOD";
        case OPCODE_RLOD: return "LOD";
        case OPCODE_STO: return "STO";
        case OPCODE_CAL: return "CAL";
        case OPCODE_JMP: return "JMP";
        case OPCODE_JPC: return "JPC";
        case OPCODE_OPR: return "OPR";
        case OPCODE_RET: return "RET";
        default:         return "???";
    }
}

const char *oprCodeName(int code) {
    switch ((OprCode)code) {
        case OPR_NEG:   return "NEG";
        case OPR_ADD:   return "ADD";
        case OPR_SUB:   return "SUB";
        case OPR_MUL:   return "MUL";
        case OPR_DIV:   return "DIV";
        case OPR_MOD:   return "MOD";
        case OPR_EQL:   return "EQL";
        case OPR_NEQ:   return "NEQ";
        case OPR_LSS:   return "LSS";
        case OPR_GEQ:   return "GEQ";
        case OPR_GTR:   return "GTR";
        case OPR_LEQ:   return "LEQ";
        case OPR_WRT:   return "WRT";
        case OPR_WRTLN: return "WRTLN";
        case OPR_RDIV:  return "RDIV";
        case OPR_TO_REAL: return "TO_REAL";
        case OPR_INDEX_ERROR: return "INDEX_ERROR";
        case OPR_RANGE_ERROR: return "RANGE_ERROR";
        case OPR_TO_CHAR: return "TO_CHAR";
        default:        return "UNKNOWN";
    }
}

bool oprCodeFromOperator(const char *operatorName, OprCode *code) {
    if (operatorName == NULL || code == NULL) {
        return false;
    }

    if (strcmp(operatorName, "plus") == 0)  { *code = OPR_ADD; return true; }
    if (strcmp(operatorName, "minus") == 0) { *code = OPR_SUB; return true; }
    if (strcmp(operatorName, "times") == 0) { *code = OPR_MUL; return true; }
    if (strcmp(operatorName, "idiv") == 0)  { *code = OPR_DIV; return true; }
    if (strcmp(operatorName, "rdiv") == 0)  { *code = OPR_RDIV; return true; }
    if (strcmp(operatorName, "imod") == 0)  { *code = OPR_MOD; return true; }
    if (strcmp(operatorName, "eql") == 0)   { *code = OPR_EQL; return true; }
    if (strcmp(operatorName, "neq") == 0)   { *code = OPR_NEQ; return true; }
    if (strcmp(operatorName, "lss") == 0)   { *code = OPR_LSS; return true; }
    if (strcmp(operatorName, "geq") == 0)   { *code = OPR_GEQ; return true; }
    if (strcmp(operatorName, "gtr") == 0)   { *code = OPR_GTR; return true; }
    if (strcmp(operatorName, "leq") == 0)   { *code = OPR_LEQ; return true; }

    return false;
}

static void appendEscapedChar(char *buffer, size_t bufferSize, size_t *pos, char ch) {
    const char *escape = NULL;

    if (buffer == NULL || bufferSize == 0 || pos == NULL || *pos + 1 >= bufferSize) {
        return;
    }

    switch (ch) {
        case '\\': escape = "\\\\"; break;
        case '"':  escape = "\\\""; break;
        case '\n': escape = "\\n"; break;
        case '\t': escape = "\\t"; break;
        case '\r': escape = "\\r"; break;
        default: break;
    }

    if (escape != NULL) {
        for (size_t i = 0; escape[i] != '\0' && *pos + 1 < bufferSize; i++) {
            buffer[(*pos)++] = escape[i];
        }
        return;
    }

    buffer[(*pos)++] = ch;
}

static void formatStringLiteral(const char *text, char *buffer, size_t bufferSize) {
    size_t pos = 0;

    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    buffer[pos++] = '"';
    if (text == NULL) {
        text = "";
    }

    for (size_t i = 0; text[i] != '\0' && pos + 1 < bufferSize; i++) {
        appendEscapedChar(buffer, bufferSize, &pos, text[i]);
    }

    if (pos + 1 < bufferSize) {
        buffer[pos++] = '"';
    }
    buffer[pos] = '\0';
}

static void formatLiteralOperand(RuntimeValue value, char *buffer, size_t bufferSize) {
    if (value.type == RUNTIME_VALUE_STRING) {
        formatStringLiteral(value.stringValue, buffer, bufferSize);
        return;
    }

    runtimeValueToString(value, buffer, bufferSize);
}

void instructionPrint(const Instruction *instruction, size_t line, FILE *stream) {
    char operand[256];

    if (instruction == NULL || stream == NULL) {
        return;
    }

    if (instruction->opcode == OPCODE_LIT) {
        formatLiteralOperand(instruction->literal, operand, sizeof(operand));
        fprintf(stream, "%zu %s %d %s\n", line, opcodeMnemonic(instruction->opcode),
                instruction->level, operand);
        return;
    }

    if (instruction->opcode == OPCODE_OPR && instruction->operand > OPR_WRTLN) {
        fprintf(stream, "%zu %s %d %lld ; %s\n", line, opcodeMnemonic(instruction->opcode),
                instruction->level, instruction->operand, oprCodeName((int)instruction->operand));
        return;
    }

    if (instruction->opcode == OPCODE_RLOD) {
        fprintf(stream, "%zu LOD %d %lld ; RAW_LOAD allow_uninitialized=true\n",
                line, instruction->level, instruction->operand);
        return;
    }

    fprintf(stream, "%zu %s %d %lld\n", line, opcodeMnemonic(instruction->opcode),
            instruction->level, instruction->operand);
}

static void instructionPrintWithMetadata(const InstructionList *list,
                                         const Instruction *instruction,
                                         size_t line,
                                         FILE *stream) {
    if (instruction == NULL || stream == NULL) {
        return;
    }

    if (instruction->opcode == OPCODE_CAL) {
        const RuntimeCallInfo *info = instructionListFindCallInfo(list, (size_t)instruction->operand);
        fprintf(stream, "%zu %s %d %lld", line, opcodeMnemonic(instruction->opcode),
                instruction->level, instruction->operand);
        if (info != NULL) {
            fprintf(stream,
                    " ; CALL name=%s target=%zu lex=%d frameSlots=%d paramSlots=%d returnOffset=%d returnSlots=%d structuredReturn=%s paramOffsets=[",
                    info->name[0] != '\0' ? info->name : "?",
                    info->target,
                    info->lexicalLevel,
                    info->frameSlotCount,
                    info->parameterSlotCount,
                    info->returnOffset,
                    info->returnSlotCount,
                    info->structuredReturn ? "true" : "false");
            for (int i = 0; i < info->parameterSlotCount; i++) {
                fprintf(stream, "%s%d", i == 0 ? "" : ",", info->parameterOffsets[i]);
            }
            fprintf(stream, "]");
        }
        fprintf(stream, "\n");
        return;
    }

    instructionPrint(instruction, line, stream);
}

void instructionListPrint(const InstructionList *list, FILE *stream) {
    if (list == NULL || stream == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        instructionPrintWithMetadata(list, &list->items[i], i, stream);
    }
}
