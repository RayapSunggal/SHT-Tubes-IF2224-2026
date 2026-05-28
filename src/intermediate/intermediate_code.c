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
}

void instructionListFree(InstructionList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        instructionFree(&list->items[i]);
    }
    free(list->items);
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
    if (strcmp(operatorName, "rdiv") == 0)  { *code = OPR_DIV; return true; }
    if (strcmp(operatorName, "imod") == 0)  { *code = OPR_MOD; return true; }
    if (strcmp(operatorName, "eql") == 0)   { *code = OPR_EQL; return true; }
    if (strcmp(operatorName, "neq") == 0)   { *code = OPR_NEQ; return true; }
    if (strcmp(operatorName, "lss") == 0)   { *code = OPR_LSS; return true; }
    if (strcmp(operatorName, "geq") == 0)   { *code = OPR_GEQ; return true; }
    if (strcmp(operatorName, "gtr") == 0)   { *code = OPR_GTR; return true; }
    if (strcmp(operatorName, "leq") == 0)   { *code = OPR_LEQ; return true; }

    return false;
}

void instructionPrint(const Instruction *instruction, size_t line, FILE *stream) {
    char operand[256];

    if (instruction == NULL || stream == NULL) {
        return;
    }

    if (instruction->opcode == OPCODE_LIT) {
        runtimeValueToString(instruction->literal, operand, sizeof(operand));
        fprintf(stream, "%zu %s %d %s\n", line, opcodeMnemonic(instruction->opcode),
                instruction->level, operand);
        return;
    }

    fprintf(stream, "%zu %s %d %lld\n", line, opcodeMnemonic(instruction->opcode),
            instruction->level, instruction->operand);
}

void instructionListPrint(const InstructionList *list, FILE *stream) {
    if (list == NULL || stream == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        instructionPrint(&list->items[i], i, stream);
    }
}
