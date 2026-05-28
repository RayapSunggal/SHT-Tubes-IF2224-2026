#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../runtime/runtime_value.h"

#define IC_FRAME_ADDRESS_BASE (-1073741824LL)

typedef enum {
    OPCODE_INT,
    OPCODE_LIT,
    OPCODE_LOD,
    OPCODE_STO,
    OPCODE_CAL,
    OPCODE_JMP,
    OPCODE_JPC,
    OPCODE_OPR,
    OPCODE_RET
} Opcode;

typedef enum {
    OPR_NEG = 1,
    OPR_ADD = 2,
    OPR_SUB = 3,
    OPR_MUL = 4,
    OPR_DIV = 5,
    OPR_MOD = 6,
    OPR_EQL = 7,
    OPR_NEQ = 8,
    OPR_LSS = 9,
    OPR_GEQ = 10,
    OPR_GTR = 11,
    OPR_LEQ = 12,
    OPR_WRT = 13,
    OPR_WRTLN = 14
} OprCode;

typedef struct {
    Opcode opcode;
    int level;
    long long operand;
    RuntimeValue literal;
} Instruction;

typedef struct {
    Instruction *items;
    size_t count;
    size_t capacity;
} InstructionList;

void instructionListInit(InstructionList *list);
void instructionListFree(InstructionList *list);
bool instructionListEmit(InstructionList *list, Instruction instruction);
bool instructionListPatchOperand(InstructionList *list, size_t index, long long operand);

Instruction instructionCreate(Opcode opcode, int level, long long operand);
Instruction instructionCreateLiteral(RuntimeValue value);
Instruction instructionCopy(Instruction instruction);
void instructionFree(Instruction *instruction);

const char *opcodeMnemonic(Opcode opcode);
const char *oprCodeName(int code);
bool oprCodeFromOperator(const char *operatorName, OprCode *code);

void instructionPrint(const Instruction *instruction, size_t line, FILE *stream);
void instructionListPrint(const InstructionList *list, FILE *stream);

#endif
