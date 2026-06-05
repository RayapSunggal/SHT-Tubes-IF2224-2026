#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../runtime/runtime_value.h"

#define IC_FRAME_ADDRESS_BASE (-1073741824LL)
#define IC_FRAME_ADDRESS_LEVEL_STRIDE 1000000LL
#define IC_MAX_CALL_PARAM_SLOTS 256
#define IC_CALL_NAME_MAX 64

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
    /* OPR 1..14 follow the Milestone 4 guidebook convention. */
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
    OPR_WRTLN = 14,

    /*
     * Internal extensions, intentionally placed after the guidebook range:
     * RDIV forces Pascal real division, TO_REAL preserves decorated-AST
     * integer-to-real assignment semantics, TO_CHAR restores char-typed loop
     * counters after arithmetic increments, and the error operations surface
     * array/subrange runtime validation with precise messages.
     */
    OPR_RDIV = 15,
    OPR_TO_REAL = 16,
    OPR_INDEX_ERROR = 17,
    OPR_RANGE_ERROR = 18,
    OPR_TO_CHAR = 19
} OprCode;

typedef struct {
    Opcode opcode;
    int level;
    long long operand;
    RuntimeValue literal;
} Instruction;

typedef struct {
    bool valid;
    size_t target;
    int lexicalLevel;
    int frameSlotCount;
    int parameterSlotCount;
    int returnOffset;
    int returnSlotCount;
    bool isFunction;
    char name[IC_CALL_NAME_MAX];
    int parameterOffsets[IC_MAX_CALL_PARAM_SLOTS];
} RuntimeCallInfo;

typedef struct {
    Instruction *items;
    size_t count;
    size_t capacity;
    RuntimeCallInfo *callInfos;
    size_t callInfoCount;
    size_t callInfoCapacity;
} InstructionList;

void instructionListInit(InstructionList *list);
void instructionListFree(InstructionList *list);
bool instructionListEmit(InstructionList *list, Instruction instruction);
bool instructionListPatchOperand(InstructionList *list, size_t index, long long operand);
bool instructionListAddCallInfo(InstructionList *list, RuntimeCallInfo info);
const RuntimeCallInfo *instructionListFindCallInfo(const InstructionList *list, size_t target);

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
