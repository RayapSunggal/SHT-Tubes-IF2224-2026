#include "intermediate_code_generator.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../semantic/symbol_table.h"

/*
 * Milestone 4 backend entrypoint.
 *
 * The parser first builds an AST, semantic analysis decorates that AST with
 * typeIdx/tabIdx/lexLevel and populates TAB/BTAB/ATAB, and only then this
 * module lowers it to stack-machine IC. Generation below intentionally reads
 * those decorated attributes and symbol-table addresses instead of raw tokens.
 */
typedef struct {
    int tabIndex;
    size_t line;
    bool defined;
} CodeLabel;

typedef struct {
    int tabIndex;
    size_t line;
} PendingCall;

typedef struct {
    InstructionList *instructions;
    const AstNode *root;
    char *error;
    size_t errorSize;
    bool hasError;
    CodeLabel labels[MAX_TAB];
    size_t labelCount;
    PendingCall pendingCalls[MAX_TAB];
    size_t pendingCallCount;
} GeneratorContext;

typedef struct {
    int level;
    long long operand;
} DirectAddress;

enum {
    IC_TEMP_CASE_SELECTOR = 0,
    IC_TEMP_CHECK_VALUE = 1,
    IC_TEMP_FOR_NEXT = 2,
    IC_COMPILER_TEMP_COUNT = 3
};

static void setError(GeneratorContext *ctx, const char *format, ...) {
    va_list args;

    if (ctx == NULL || ctx->hasError) {
        return;
    }

    ctx->hasError = true;
    if (ctx->error == NULL || ctx->errorSize == 0) {
        return;
    }

    va_start(args, format);
    (void)vsnprintf(ctx->error, ctx->errorSize, format, args);
    va_end(args);
}

static bool emitInstruction(GeneratorContext *ctx, Instruction instruction) {
    bool ok;

    if (ctx->hasError) {
        instructionFree(&instruction);
        return false;
    }

    ok = instructionListEmit(ctx->instructions, instruction);
    instructionFree(&instruction);
    if (!ok) {
        setError(ctx, "Intermediate Code Error: gagal menambahkan instruksi.");
    }
    return ok;
}

static bool emitSimple(GeneratorContext *ctx, Opcode opcode, int level, long long operand) {
    Instruction instruction = instructionCreate(opcode, level, operand);
    return emitInstruction(ctx, instruction);
}

static bool emitLiteral(GeneratorContext *ctx, RuntimeValue value) {
    Instruction instruction = instructionCreateLiteral(value);
    bool ok = emitInstruction(ctx, instruction);
    runtimeValueFree(&value);
    return ok;
}

static bool emitPatchable(GeneratorContext *ctx, Opcode opcode, size_t *line) {
    if (line != NULL) {
        *line = ctx->instructions->count;
    }
    return emitSimple(ctx, opcode, 0, 0);
}

static bool patchOperand(GeneratorContext *ctx, size_t line, long long operand) {
    if (!instructionListPatchOperand(ctx->instructions, line, operand)) {
        setError(ctx, "Intermediate Code Error: gagal patch instruksi di line %zu.", line);
        return false;
    }
    return true;
}

static int functionReturnTabIndexForCallInfo(int functionIndex, int blockIndex) {
    if (functionIndex < 0 || functionIndex >= symTabCount() ||
        tab[functionIndex].obj != OBJ_FUNCTION ||
        blockIndex < 0 || blockIndex >= symBtabCount()) {
        return -1;
    }

    for (int i = 0; i < symTabCount(); i++) {
        if (symBlockForTabIndex(i) == blockIndex &&
            tab[i].obj == OBJ_VARIABLE &&
            tab[i].identifier != NULL &&
            tab[functionIndex].identifier != NULL &&
            strcasecmp(tab[i].identifier, tab[functionIndex].identifier) == 0) {
            return i;
        }
    }

    return -1;
}

static int valueSizeForTabIndex(int tabIndex) {
    int size;

    if (tabIndex < 0 || tabIndex >= symTabCount() || tab[tabIndex].obj != OBJ_VARIABLE) {
        return 0;
    }

    size = sizeOfType(tab[tabIndex].type, tab[tabIndex].ref);
    return size > 0 ? size : 1;
}

static bool addRuntimeCallInfo(GeneratorContext *ctx, int tabIndex, size_t target) {
    RuntimeCallInfo info;
    int blockIndex;
    int paramCursor = 0;

    if (tabIndex < 0 || tabIndex >= symTabCount() ||
        (tab[tabIndex].obj != OBJ_PROCEDURE && tab[tabIndex].obj != OBJ_FUNCTION)) {
        setError(ctx, "Intermediate Code Error: metadata call target tidak valid.");
        return false;
    }

    blockIndex = tab[tabIndex].ref;
    if (blockIndex < 0 || blockIndex >= symBtabCount()) {
        setError(ctx, "Intermediate Code Error: block metadata call '%s' tidak valid.",
                 tab[tabIndex].identifier != NULL ? tab[tabIndex].identifier : "?");
        return false;
    }

    memset(&info, 0, sizeof(info));
    info.valid = true;
    info.target = target;
    info.lexicalLevel = tab[tabIndex].lev + 1;
    info.frameSlotCount = symFrameSlotCountForBlock(blockIndex);
    info.isFunction = tab[tabIndex].obj == OBJ_FUNCTION;
    if (info.isFunction) {
        int returnIndex = functionReturnTabIndexForCallInfo(tabIndex, blockIndex);
        info.returnOffset = returnIndex >= 0 ? symFrameOffsetForTabIndex(returnIndex) : -1;
        info.returnSlotCount = returnIndex >= 0 ?
            sizeOfType(tab[returnIndex].type, tab[returnIndex].ref) : 0;
        info.structuredReturn = returnIndex >= 0 &&
            (tab[returnIndex].type == TYPE_ARRAY || tab[returnIndex].type == TYPE_RECORD);
        if (!info.structuredReturn && info.returnSlotCount <= 0) {
            info.returnSlotCount = 1;
        }
    } else {
        info.returnOffset = -1;
    }
    if (tab[tabIndex].identifier != NULL) {
        (void)snprintf(info.name, sizeof(info.name), "%s", tab[tabIndex].identifier);
    }

    for (int i = 0; i < symTabCount(); i++) {
        if (symBlockForTabIndex(i) == blockIndex &&
            tab[i].obj == OBJ_VARIABLE &&
            tab[i].nrm == 0) {
            int offset = symFrameOffsetForTabIndex(i);
            int size = valueSizeForTabIndex(i);
            for (int j = 0; j < size; j++) {
                if (paramCursor >= IC_MAX_CALL_PARAM_SLOTS) {
                    setError(ctx, "Intermediate Code Error: parameter '%s' terlalu besar.",
                             info.name[0] != '\0' ? info.name : "?");
                    return false;
                }
                info.parameterOffsets[paramCursor++] = offset + j;
            }
        }
    }

    info.parameterSlotCount = paramCursor;
    if (info.frameSlotCount < info.parameterSlotCount ||
        info.lexicalLevel <= 0 ||
        info.lexicalLevel >= IC_FRAME_ADDRESS_LEVEL_STRIDE) {
        setError(ctx, "Intermediate Code Error: metadata frame '%s' tidak valid.",
                 info.name[0] != '\0' ? info.name : "?");
        return false;
    }

    if (!instructionListAddCallInfo(ctx->instructions, info)) {
        setError(ctx, "Intermediate Code Error: gagal menyimpan metadata call '%s'.",
                 info.name[0] != '\0' ? info.name : "?");
        return false;
    }

    return true;
}

static int globalDataSize(void) {
    int size = 0;
    int count = symTabCount();

    for (int i = 0; i < count; i++) {
        int endAddress;

        if (tab[i].identifier == NULL || tab[i].obj != OBJ_VARIABLE || tab[i].lev != 0) {
            continue;
        }

        endAddress = tab[i].adr + valueSizeForTabIndex(i);
        if (endAddress > size) {
            size = endAddress;
        }
    }

    return size;
}

static int compilerTempAddress(int tempIndex) {
    return IC_RESERVED_RUNTIME_CELLS + globalDataSize() + tempIndex;
}

static long long frameAddressForLexicalLevel(int lexLevel, int offset) {
    return IC_FRAME_ADDRESS_BASE -
           ((long long)lexLevel * IC_FRAME_ADDRESS_LEVEL_STRIDE) +
           (long long)offset;
}

int intermediateCodeRuntimeAddressForTabIndex(int tabIndex) {
    int address;
    int count = symTabCount();

    if (tabIndex < 0 || tabIndex >= count || tab[tabIndex].obj != OBJ_VARIABLE) {
        return -1;
    }

    if (tab[tabIndex].lev == 0) {
        return IC_RESERVED_RUNTIME_CELLS + tab[tabIndex].adr;
    }

    address = symFrameOffsetForTabIndex(tabIndex);
    if (address < 0) {
        return -1;
    }
    if (address >= IC_FRAME_ADDRESS_LEVEL_STRIDE) {
        return -1;
    }
    return (int)frameAddressForLexicalLevel(tab[tabIndex].lev, address);
}

int intermediateCodeGlobalMemorySize(void) {
    return IC_RESERVED_RUNTIME_CELLS + globalDataSize() + IC_COMPILER_TEMP_COUNT;
}

static const AstNode *firstParamList(const AstNode *node) {
    if (node == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        if (node->children[i]->type == AST_PARAM_LIST) {
            return node->children[i];
        }
    }

    return NULL;
}

static int nodeSymbolIndex(const AstNode *node) {
    if (node == NULL) {
        return -1;
    }
    if (node->tabIdx >= 0 && node->tabIdx < symTabCount()) {
        return node->tabIdx;
    }
    return symLookup(node->sval);
}

static bool sameName(const char *left, const char *right) {
    return left != NULL && right != NULL && strcasecmp(left, right) == 0;
}

static const AstNode *findSubprogramNodeRec(const AstNode *node, int tabIndex, const char *name) {
    const AstNode *found;

    if (node == NULL) {
        return NULL;
    }

    if ((node->type == AST_PROC_DECL || node->type == AST_FUNC_DECL) &&
        ((node->tabIdx >= 0 && node->tabIdx == tabIndex) || sameName(node->sval, name))) {
        return node;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        found = findSubprogramNodeRec(node->children[i], tabIndex, name);
        if (found != NULL) {
            return found;
        }
    }

    return NULL;
}

static const AstNode *findSubprogramNode(GeneratorContext *ctx, int tabIndex) {
    const char *name = NULL;

    if (tabIndex >= 0 && tabIndex < symTabCount()) {
        name = tab[tabIndex].identifier;
    }

    return findSubprogramNodeRec(ctx->root, tabIndex, name);
}

static const AstNode *findConstDeclNodeRec(const AstNode *node, int tabIndex, const char *name) {
    const AstNode *found;

    if (node == NULL) {
        return NULL;
    }

    if (node->type == AST_CONST_DECL) {
        if ((node->tabIdx >= 0 && node->tabIdx == tabIndex) ||
            (node->tabIdx < 0 && sameName(node->sval, name))) {
            return node;
        }
    }

    for (size_t i = 0; i < node->childCount; i++) {
        found = findConstDeclNodeRec(node->children[i], tabIndex, name);
        if (found != NULL) {
            return found;
        }
    }

    return NULL;
}

static const AstNode *findConstValueNode(GeneratorContext *ctx, int tabIndex) {
    const char *name = NULL;
    const AstNode *decl;

    if (tabIndex >= 0 && tabIndex < symTabCount()) {
        name = tab[tabIndex].identifier;
    }

    decl = findConstDeclNodeRec(ctx->root, tabIndex, name);
    if (decl == NULL || decl->childCount < 1) {
        return NULL;
    }
    return decl->children[0];
}

static bool defineLabel(GeneratorContext *ctx, int tabIndex, size_t line) {
    if (tabIndex < 0) {
        setError(ctx, "Intermediate Code Error: label subprogram tidak valid.");
        return false;
    }

    for (size_t i = 0; i < ctx->labelCount; i++) {
        if (ctx->labels[i].tabIndex == tabIndex) {
            ctx->labels[i].line = line;
            ctx->labels[i].defined = true;
            return true;
        }
    }

    if (ctx->labelCount >= MAX_TAB) {
        setError(ctx, "Intermediate Code Error: label subprogram terlalu banyak.");
        return false;
    }

    ctx->labels[ctx->labelCount].tabIndex = tabIndex;
    ctx->labels[ctx->labelCount].line = line;
    ctx->labels[ctx->labelCount].defined = true;
    ctx->labelCount++;
    return true;
}

static bool findLabel(GeneratorContext *ctx, int tabIndex, size_t *line) {
    for (size_t i = 0; i < ctx->labelCount; i++) {
        if (ctx->labels[i].tabIndex == tabIndex && ctx->labels[i].defined) {
            if (line != NULL) {
                *line = ctx->labels[i].line;
            }
            return true;
        }
    }
    return false;
}

static bool emitCall(GeneratorContext *ctx, int tabIndex) {
    size_t line;
    size_t target;

    if (findLabel(ctx, tabIndex, &target)) {
        return emitSimple(ctx, OPCODE_CAL, 0, (long long)target);
    }

    line = ctx->instructions->count;
    if (!emitSimple(ctx, OPCODE_CAL, 0, 0)) {
        return false;
    }

    if (ctx->pendingCallCount >= MAX_TAB) {
        setError(ctx, "Intermediate Code Error: pending call terlalu banyak.");
        return false;
    }

    ctx->pendingCalls[ctx->pendingCallCount].tabIndex = tabIndex;
    ctx->pendingCalls[ctx->pendingCallCount].line = line;
    ctx->pendingCallCount++;
    return true;
}

static bool patchPendingCalls(GeneratorContext *ctx) {
    for (size_t i = 0; i < ctx->pendingCallCount; i++) {
        size_t target;
        int tabIndex = ctx->pendingCalls[i].tabIndex;

        if (!findLabel(ctx, tabIndex, &target)) {
            setError(ctx, "Intermediate Code Error: label subprogram '%s' tidak ditemukan.",
                     tabIndex >= 0 && tabIndex < symTabCount() && tab[tabIndex].identifier != NULL ?
                     tab[tabIndex].identifier : "?");
            return false;
        }

        if (!patchOperand(ctx, ctx->pendingCalls[i].line, (long long)target)) {
            return false;
        }
    }
    return true;
}

static bool generateExpression(GeneratorContext *ctx, const AstNode *node);
static bool generateStatement(GeneratorContext *ctx, const AstNode *node);
static bool generateAddress(GeneratorContext *ctx, const AstNode *node);
static bool generateFunctionCall(GeneratorContext *ctx, const AstNode *node);

static bool emitConstantLoad(GeneratorContext *ctx, int tabIndex) {
    const AstNode *valueNode = findConstValueNode(ctx, tabIndex);

    switch (tab[tabIndex].type) {
        case TYPE_INTEGER:
            return emitLiteral(ctx, runtimeValueInteger(tab[tabIndex].adr));
        case TYPE_REAL:
            if (valueNode != NULL) {
                return generateExpression(ctx, valueNode);
            }
            break;
        case TYPE_BOOLEAN:
            return emitLiteral(ctx, runtimeValueBoolean(tab[tabIndex].adr != 0));
        case TYPE_CHAR:
            return emitLiteral(ctx, runtimeValueChar((char)tab[tabIndex].adr));
        case TYPE_STRING:
            if (valueNode != NULL) {
                return generateExpression(ctx, valueNode);
            }
            break;
        default:
            break;
    }

    setError(ctx, "Intermediate Code Error: konstanta '%s' belum dapat dimuat.",
             tab[tabIndex].identifier != NULL ? tab[tabIndex].identifier : "?");
    return false;
}

static bool emitIntegerLiteral(GeneratorContext *ctx, long long value) {
    return emitLiteral(ctx, runtimeValueInteger(value));
}

static bool directVariableReferenceForIndex(GeneratorContext *ctx,
                                            int idx,
                                            const char *name,
                                            DirectAddress *ref);

static bool directVariableReference(GeneratorContext *ctx, const AstNode *node, DirectAddress *ref) {
    int idx;
    if (node == NULL) {
        setError(ctx, "Intermediate Code Error: target kosong.");
        return false;
    }

    idx = nodeSymbolIndex(node);
    return directVariableReferenceForIndex(ctx, idx, node->sval, ref);
}

static bool directVariableReferenceForIndex(GeneratorContext *ctx,
                                            int idx,
                                            const char *name,
                                            DirectAddress *ref) {
    long long address;

    if (ref == NULL) {
        setError(ctx, "Intermediate Code Error: target kosong.");
        return false;
    }

    if (idx < 0 || idx >= symTabCount() || tab[idx].obj != OBJ_VARIABLE) {
        setError(ctx, "Intermediate Code Error: target '%s' bukan variabel.",
                 name != NULL ? name : "?");
        return false;
    }

    if (tab[idx].lev == 0) {
        ref->level = 0;
        ref->operand = IC_RESERVED_RUNTIME_CELLS + tab[idx].adr;
        return true;
    }

    address = symFrameOffsetForTabIndex(idx);
    if (address < 0) {
        setError(ctx, "Intermediate Code Error: offset frame '%s' tidak valid.",
                 name != NULL ? name : "?");
        return false;
    }

    if (address >= IC_FRAME_ADDRESS_LEVEL_STRIDE) {
        setError(ctx, "Intermediate Code Error: offset frame '%s' terlalu besar.",
                 name != NULL ? name : "?");
        return false;
    }

    ref->level = tab[idx].lev + 1;
    ref->operand = address;
    return true;
}

static bool nodeTypeRef(GeneratorContext *ctx, const AstNode *node, BaseType *type, int *ref) {
    int idx;
    BaseType currentType;
    int currentRef;

    if (node == NULL || type == NULL || ref == NULL) {
        setError(ctx, "Intermediate Code Error: tipe node tidak valid.");
        return false;
    }

    if (node->type == AST_VAR) {
        idx = nodeSymbolIndex(node);
        if (idx < 0 || idx >= symTabCount()) {
            setError(ctx, "Intermediate Code Error: identifier '%s' tidak ditemukan di symbol table.",
                     node->sval != NULL ? node->sval : "?");
            return false;
        }
        *type = tab[idx].type;
        *ref = tab[idx].ref;
        return true;
    }

    if (node->type == AST_FIELD_ACCESS) {
        idx = node->tabIdx;
        if (idx < 0 && node->childCount >= 2) {
            idx = node->children[1]->tabIdx;
        }
        if (idx < 0 || idx >= symTabCount()) {
            setError(ctx, "Intermediate Code Error: field record tidak ditemukan.");
            return false;
        }
        *type = tab[idx].type;
        *ref = tab[idx].ref;
        return true;
    }

    if (node->type == AST_ARRAY_ACCESS) {
        if (node->childCount < 2 || !nodeTypeRef(ctx, node->children[0], &currentType, &currentRef)) {
            return false;
        }
        for (size_t i = 1; i < node->childCount; i++) {
            if (currentType != TYPE_ARRAY || currentRef < 0 || currentRef >= symAtabCount()) {
                setError(ctx, "Intermediate Code Error: dimensi array tidak valid.");
                return false;
            }
            currentType = atab[currentRef].etyp;
            currentRef = atab[currentRef].eref;
        }
        *type = currentType;
        *ref = currentRef;
        return true;
    }

    if (node->type == AST_FUNC_CALL) {
        int blockIndex;
        int returnIndex;
        idx = nodeSymbolIndex(node);
        if (idx < 0 || idx >= symTabCount() || tab[idx].obj != OBJ_FUNCTION) {
            setError(ctx, "Intermediate Code Error: function '%s' tidak ditemukan di symbol table.",
                     node->sval != NULL ? node->sval : "?");
            return false;
        }
        blockIndex = tab[idx].ref;
        returnIndex = functionReturnTabIndexForCallInfo(idx, blockIndex);
        if (returnIndex < 0 || returnIndex >= symTabCount()) {
            setError(ctx, "Intermediate Code Error: return function '%s' tidak valid.",
                     node->sval != NULL ? node->sval : "?");
            return false;
        }
        *type = tab[returnIndex].type;
        *ref = tab[returnIndex].ref;
        return true;
    }

    *type = (BaseType)node->typeIdx;
    *ref = -1;
    return true;
}

static bool generateBoundsCheck(GeneratorContext *ctx, const AstNode *indexNode, int low, int high) {
    int tempAddress = compilerTempAddress(IC_TEMP_CHECK_VALUE);
    size_t lowFail;
    size_t highFail;
    size_t endJump;
    size_t failLine;

    if (!generateExpression(ctx, indexNode) ||
        !emitSimple(ctx, OPCODE_STO, 0, tempAddress) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, low) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_GEQ) ||
        !emitPatchable(ctx, OPCODE_JPC, &lowFail) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, high) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_LEQ) ||
        !emitPatchable(ctx, OPCODE_JPC, &highFail) ||
        !emitPatchable(ctx, OPCODE_JMP, &endJump)) {
        return false;
    }

    failLine = ctx->instructions->count;
    if (!patchOperand(ctx, lowFail, (long long)failLine) ||
        !patchOperand(ctx, highFail, (long long)failLine) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, low) ||
        !emitIntegerLiteral(ctx, high) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_INDEX_ERROR)) {
        return false;
    }

    return patchOperand(ctx, endJump, (long long)ctx->instructions->count);
}

static bool generateRuntimeRangeCheck(GeneratorContext *ctx, int low, int high) {
    int tempAddress = compilerTempAddress(IC_TEMP_CHECK_VALUE);
    size_t lowFail;
    size_t highFail;
    size_t endJump;
    size_t failLine;

    if (!emitSimple(ctx, OPCODE_STO, 0, tempAddress) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, low) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_GEQ) ||
        !emitPatchable(ctx, OPCODE_JPC, &lowFail) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, high) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_LEQ) ||
        !emitPatchable(ctx, OPCODE_JPC, &highFail) ||
        !emitPatchable(ctx, OPCODE_JMP, &endJump)) {
        return false;
    }

    failLine = ctx->instructions->count;
    if (!patchOperand(ctx, lowFail, (long long)failLine) ||
        !patchOperand(ctx, highFail, (long long)failLine) ||
        !emitSimple(ctx, OPCODE_LOD, 0, tempAddress) ||
        !emitIntegerLiteral(ctx, low) ||
        !emitIntegerLiteral(ctx, high) ||
        !emitSimple(ctx, OPCODE_OPR, 0, OPR_RANGE_ERROR) ||
        !patchOperand(ctx, endJump, (long long)ctx->instructions->count)) {
        return false;
    }

    return emitSimple(ctx, OPCODE_LOD, 0, tempAddress);
}

static bool generateAddress(GeneratorContext *ctx, const AstNode *node) {
    int idx;
    int address;
    BaseType currentType;
    int currentRef;

    if (node == NULL) {
        setError(ctx, "Intermediate Code Error: address node kosong.");
        return false;
    }

    if (node->type == AST_VAR) {
        idx = nodeSymbolIndex(node);
        if (idx < 0 || idx >= symTabCount() || tab[idx].obj != OBJ_VARIABLE) {
            setError(ctx, "Intermediate Code Error: identifier '%s' bukan variabel beralamat.",
                     node->sval != NULL ? node->sval : "?");
            return false;
        }
        address = intermediateCodeRuntimeAddressForTabIndex(idx);
        if (address == -1) {
            setError(ctx, "Intermediate Code Error: alamat variabel '%s' tidak valid.",
                     node->sval != NULL ? node->sval : "?");
            return false;
        }
        return emitIntegerLiteral(ctx, address);
    }

    if (node->type == AST_FIELD_ACCESS) {
        int fieldIdx;
        if (node->childCount < 2 || !generateAddress(ctx, node->children[0])) {
            return false;
        }
        fieldIdx = node->tabIdx;
        if (fieldIdx < 0) {
            fieldIdx = node->children[1]->tabIdx;
        }
        if (fieldIdx < 0 || fieldIdx >= symTabCount()) {
            setError(ctx, "Intermediate Code Error: field record tidak valid.");
            return false;
        }
        return emitIntegerLiteral(ctx, tab[fieldIdx].adr) &&
               emitSimple(ctx, OPCODE_OPR, 0, OPR_ADD);
    }

    if (node->type == AST_ARRAY_ACCESS) {
        if (node->childCount < 2 || !nodeTypeRef(ctx, node->children[0], &currentType, &currentRef) ||
            !generateAddress(ctx, node->children[0])) {
            return false;
        }

        for (size_t i = 1; i < node->childCount; i++) {
            int arrayRef;
            if (currentType != TYPE_ARRAY || currentRef < 0 || currentRef >= symAtabCount()) {
                setError(ctx, "Intermediate Code Error: dimensi array tidak valid.");
                return false;
            }
            arrayRef = currentRef;
            if (!generateBoundsCheck(ctx, node->children[i], atab[arrayRef].low, atab[arrayRef].high) ||
                !emitSimple(ctx, OPCODE_LOD, 0, compilerTempAddress(IC_TEMP_CHECK_VALUE)) ||
                !emitIntegerLiteral(ctx, atab[arrayRef].low) ||
                !emitSimple(ctx, OPCODE_OPR, 0, OPR_SUB) ||
                !emitIntegerLiteral(ctx, atab[arrayRef].elsz) ||
                !emitSimple(ctx, OPCODE_OPR, 0, OPR_MUL) ||
                !emitSimple(ctx, OPCODE_OPR, 0, OPR_ADD)) {
                return false;
            }
            currentType = atab[arrayRef].etyp;
            currentRef = atab[arrayRef].eref;
        }

        return true;
    }

    setError(ctx, "Intermediate Code Error: node tidak bisa dijadikan alamat (type=%d).", (int)node->type);
    return false;
}

static bool emitIdentifierLoad(GeneratorContext *ctx, const AstNode *node) {
    int idx;
    DirectAddress ref;

    if (node == NULL) {
        setError(ctx, "Intermediate Code Error: identifier kosong.");
        return false;
    }

    idx = nodeSymbolIndex(node);
    if (idx < 0 || idx >= symTabCount()) {
        setError(ctx, "Intermediate Code Error: identifier '%s' tidak ditemukan di symbol table.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (tab[idx].obj == OBJ_CONSTANT) {
        return emitConstantLoad(ctx, idx);
    }

    if (tab[idx].obj != OBJ_VARIABLE) {
        setError(ctx, "Intermediate Code Error: identifier '%s' bukan nilai yang bisa dimuat.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!directVariableReferenceForIndex(ctx, idx, node->sval, &ref)) {
        return false;
    }

    return emitSimple(ctx, OPCODE_LOD, ref.level, ref.operand);
}

static bool generateBinaryExpression(GeneratorContext *ctx, const AstNode *node) {
    OprCode code;
    size_t falseJump;
    size_t endJump;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: binary expression tidak lengkap.");
        return false;
    }

    if (node->sval != NULL && strcasecmp(node->sval, "orsy") == 0) {
        return generateExpression(ctx, node->children[0]) &&
               emitPatchable(ctx, OPCODE_JPC, &falseJump) &&
               emitLiteral(ctx, runtimeValueBoolean(true)) &&
               emitPatchable(ctx, OPCODE_JMP, &endJump) &&
               patchOperand(ctx, falseJump, (long long)ctx->instructions->count) &&
               generateExpression(ctx, node->children[1]) &&
               patchOperand(ctx, endJump, (long long)ctx->instructions->count);
    }

    if (node->sval != NULL && strcasecmp(node->sval, "andsy") == 0) {
        return generateExpression(ctx, node->children[0]) &&
               emitPatchable(ctx, OPCODE_JPC, &falseJump) &&
               generateExpression(ctx, node->children[1]) &&
               emitPatchable(ctx, OPCODE_JMP, &endJump) &&
               patchOperand(ctx, falseJump, (long long)ctx->instructions->count) &&
               emitLiteral(ctx, runtimeValueBoolean(false)) &&
               patchOperand(ctx, endJump, (long long)ctx->instructions->count);
    }

    if (!oprCodeFromOperator(node->sval, &code)) {
        setError(ctx, "Intermediate Code Error: operator '%s' belum didukung.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!generateExpression(ctx, node->children[0])) {
        return false;
    }
    if (!generateExpression(ctx, node->children[1])) {
        return false;
    }

    return emitSimple(ctx, OPCODE_OPR, 0, code);
}

static bool generateUnaryExpression(GeneratorContext *ctx, const AstNode *node) {
    if (node->childCount < 1) {
        setError(ctx, "Intermediate Code Error: unary expression tidak lengkap.");
        return false;
    }

    if (node->sval != NULL && strcasecmp(node->sval, "not") == 0) {
        return generateExpression(ctx, node->children[0]) &&
               emitLiteral(ctx, runtimeValueBoolean(false)) &&
               emitSimple(ctx, OPCODE_OPR, 0, OPR_EQL);
    }

    if (node->sval == NULL || strcmp(node->sval, "-") != 0) {
        setError(ctx, "Intermediate Code Error: unary operator '%s' belum didukung.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!generateExpression(ctx, node->children[0])) {
        return false;
    }

    return emitSimple(ctx, OPCODE_OPR, 0, OPR_NEG);
}

static int rangeTabIndexForTarget(const AstNode *node) {
    if (node == NULL) {
        return -1;
    }

    if (node->type == AST_VAR) {
        return nodeSymbolIndex(node);
    }

    if (node->type == AST_FIELD_ACCESS) {
        if (node->tabIdx >= 0) {
            return node->tabIdx;
        }
        if (node->childCount >= 2) {
            return node->children[1]->tabIdx;
        }
    }

    return -1;
}

static bool arrayContainerRefForAccess(GeneratorContext *ctx, const AstNode *node, int *arrayRefOut) {
    BaseType currentType;
    int currentRef;
    int arrayRef = -1;

    if (node == NULL || node->type != AST_ARRAY_ACCESS ||
        node->childCount < 2 || arrayRefOut == NULL) {
        return false;
    }

    if (!nodeTypeRef(ctx, node->children[0], &currentType, &currentRef)) {
        return false;
    }

    for (size_t i = 1; i < node->childCount; i++) {
        if (currentType != TYPE_ARRAY || currentRef < 0 || currentRef >= symAtabCount()) {
            return false;
        }
        arrayRef = currentRef;
        currentType = atab[arrayRef].etyp;
        currentRef = atab[arrayRef].eref;
    }

    *arrayRefOut = arrayRef;
    return arrayRef >= 0;
}

static bool rangeForTarget(GeneratorContext *ctx,
                           const AstNode *target,
                           bool *hasRange,
                           int *low,
                           int *high) {
    int tabIndex;
    int arrayRef;

    if (hasRange == NULL || low == NULL || high == NULL) {
        return false;
    }

    *hasRange = false;
    *low = 0;
    *high = 0;

    if (target != NULL && target->type == AST_ARRAY_ACCESS) {
        if (!arrayContainerRefForAccess(ctx, target, &arrayRef)) {
            return true;
        }
        if (arrayRef >= 0 && arrayRef < symAtabCount() && atab[arrayRef].elemHasRange) {
            *hasRange = true;
            *low = atab[arrayRef].elemRangeLow;
            *high = atab[arrayRef].elemRangeHigh;
        }
        return true;
    }

    tabIndex = rangeTabIndexForTarget(target);
    if (tabIndex >= 0 && tabIndex < symTabCount() && tab[tabIndex].hasRange) {
        *hasRange = true;
        *low = tab[tabIndex].rangeLow;
        *high = tab[tabIndex].rangeHigh;
    }

    return true;
}

static bool emitValueGuardsForType(GeneratorContext *ctx,
                                   BaseType targetType,
                                   BaseType sourceType,
                                   bool hasRange,
                                   int rangeLow,
                                   int rangeHigh) {
    if (hasRange) {
        if (!generateRuntimeRangeCheck(ctx, rangeLow, rangeHigh)) {
            return false;
        }
    }

    if (targetType == TYPE_REAL && sourceType == TYPE_INTEGER) {
        return emitSimple(ctx, OPCODE_OPR, 0, OPR_TO_REAL);
    }

    return true;
}

static bool emitValueGuardsForTarget(GeneratorContext *ctx,
                                     const AstNode *target,
                                     const AstNode *source) {
    BaseType targetType;
    BaseType sourceType;
    int targetRef;
    bool hasRange;
    int rangeLow;
    int rangeHigh;

    if (!nodeTypeRef(ctx, target, &targetType, &targetRef)) {
        return false;
    }
    if (!rangeForTarget(ctx, target, &hasRange, &rangeLow, &rangeHigh)) {
        return false;
    }

    sourceType = source != NULL ? (BaseType)source->typeIdx : TYPE_NONE;
    return emitValueGuardsForType(ctx, targetType, sourceType, hasRange, rangeLow, rangeHigh);
}

static bool emitValueGuardsForTabIndex(GeneratorContext *ctx, int tabIndex, BaseType sourceType) {
    if (tabIndex < 0 || tabIndex >= symTabCount()) {
        setError(ctx, "Intermediate Code Error: parameter formal tidak valid.");
        return false;
    }

    return emitValueGuardsForType(ctx,
                                  tab[tabIndex].type,
                                  sourceType,
                                  tab[tabIndex].hasRange,
                                  tab[tabIndex].rangeLow,
                                  tab[tabIndex].rangeHigh);
}

static bool emitAddressedSlotLoadWithOpcode(GeneratorContext *ctx,
                                            const AstNode *node,
                                            int offset,
                                            Opcode opcode) {
    if (offset < 0) {
        setError(ctx, "Intermediate Code Error: offset structured value tidak valid.");
        return false;
    }

    if (!generateAddress(ctx, node)) {
        return false;
    }

    if (offset > 0 &&
        (!emitIntegerLiteral(ctx, offset) ||
         !emitSimple(ctx, OPCODE_OPR, 0, OPR_ADD))) {
        return false;
    }

    return emitSimple(ctx, opcode, 1, 0);
}

static bool emitAddressedSlotRawLoad(GeneratorContext *ctx, const AstNode *node, int offset) {
    return emitAddressedSlotLoadWithOpcode(ctx, node, offset, OPCODE_RLOD);
}

static bool emitStructuredArgument(GeneratorContext *ctx,
                                   const AstNode *actual,
                                   int formalTabIndex) {
    int size;

    if (formalTabIndex < 0 || formalTabIndex >= symTabCount()) {
        setError(ctx, "Intermediate Code Error: parameter formal tidak valid.");
        return false;
    }

    size = sizeOfType(tab[formalTabIndex].type, tab[formalTabIndex].ref);
    if (size <= 0) {
        size = 1;
    }

    if (actual != NULL && actual->type == AST_FUNC_CALL) {
        return generateFunctionCall(ctx, actual);
    }

    for (int offset = 0; offset < size; offset++) {
        if (!emitAddressedSlotRawLoad(ctx, actual, offset)) {
            return false;
        }
    }

    return true;
}

static bool emitStructuredFunctionResultAssignment(GeneratorContext *ctx,
                                                   const AstNode *target,
                                                   const AstNode *source,
                                                   int size) {
    if (size <= 0) {
        return generateFunctionCall(ctx, source);
    }

    if (!generateFunctionCall(ctx, source)) {
        return false;
    }

    for (int offset = size - 1; offset >= 0; offset--) {
        if (!generateAddress(ctx, target)) {
            return false;
        }

        if (offset > 0 &&
            (!emitIntegerLiteral(ctx, offset) ||
             !emitSimple(ctx, OPCODE_OPR, 0, OPR_ADD))) {
            return false;
        }

        if (!emitSimple(ctx, OPCODE_STO, 1, 0)) {
            return false;
        }
    }

    return true;
}

static bool emitStructuredAssignment(GeneratorContext *ctx,
                                     const AstNode *target,
                                     const AstNode *source,
                                     BaseType targetType,
                                     int targetRef) {
    int size = sizeOfType(targetType, targetRef);

    if (size <= 0) {
        if (source != NULL && source->type == AST_FUNC_CALL) {
            return generateFunctionCall(ctx, source);
        }
        return true;
    }

    if (source != NULL && source->type == AST_FUNC_CALL) {
        return emitStructuredFunctionResultAssignment(ctx, target, source, size);
    }

    for (int offset = 0; offset < size; offset++) {
        if (!emitAddressedSlotRawLoad(ctx, source, offset) ||
            !generateAddress(ctx, target)) {
            return false;
        }

        if (offset > 0 &&
            (!emitIntegerLiteral(ctx, offset) ||
             !emitSimple(ctx, OPCODE_OPR, 0, OPR_ADD))) {
            return false;
        }

        if (!emitSimple(ctx, OPCODE_STO, 1, 0)) {
            return false;
        }
    }

    return true;
}

static bool emitCallArguments(GeneratorContext *ctx, const AstNode *callNode, const AstNode *declNode) {
    const AstNode *actuals = firstParamList(callNode);
    const AstNode *formals = firstParamList(declNode);
    size_t actualCount = actuals != NULL ? actuals->childCount : 0;
    size_t formalIndex = 0;

    if (formals == NULL) {
        if (actualCount != 0) {
            setError(ctx, "Intermediate Code Error: jumlah parameter call '%s' tidak cocok.",
                     callNode->sval != NULL ? callNode->sval : "?");
            return false;
        }
        return true;
    }

    for (size_t i = 0; i < formals->childCount; i++) {
        const AstNode *group = formals->children[i];
        const AstNode *idents = NULL;
        if (group->type != AST_PARAM_GROUP || group->childCount < 1) {
            continue;
        }
        idents = group->children[0];
        if (idents->type != AST_IDENT_LIST) {
            continue;
        }
        for (size_t j = 0; j < idents->childCount; j++) {
            int formalTabIndex;
            if (formalIndex >= actualCount) {
                setError(ctx, "Intermediate Code Error: parameter call '%s' kurang.",
                         callNode->sval != NULL ? callNode->sval : "?");
                return false;
            }

            formalTabIndex = idents->children[j]->tabIdx;
            if (formalTabIndex < 0 || formalTabIndex >= symTabCount()) {
                setError(ctx, "Intermediate Code Error: parameter formal tidak valid.");
                return false;
            }

            if (tab[formalTabIndex].type == TYPE_ARRAY ||
                tab[formalTabIndex].type == TYPE_RECORD) {
                if (!emitStructuredArgument(ctx, actuals->children[formalIndex], formalTabIndex)) {
                    return false;
                }
            } else {
                if (!generateExpression(ctx, actuals->children[formalIndex])) {
                    return false;
                }
                if (!emitValueGuardsForTabIndex(ctx,
                                                formalTabIndex,
                                                (BaseType)actuals->children[formalIndex]->typeIdx)) {
                    return false;
                }
            }
            formalIndex++;
        }
    }

    if (formalIndex != actualCount) {
        setError(ctx, "Intermediate Code Error: parameter call '%s' terlalu banyak.",
                 callNode->sval != NULL ? callNode->sval : "?");
        return false;
    }

    return true;
}

static bool generateFunctionCall(GeneratorContext *ctx, const AstNode *node) {
    int functionIndex = nodeSymbolIndex(node);
    const AstNode *declNode;

    if (functionIndex < 0 || functionIndex >= symTabCount() || tab[functionIndex].obj != OBJ_FUNCTION) {
        setError(ctx, "Intermediate Code Error: function '%s' tidak valid.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    declNode = findSubprogramNode(ctx, functionIndex);
    if (declNode == NULL) {
        setError(ctx, "Intermediate Code Error: deklarasi function '%s' tidak ditemukan.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!emitCallArguments(ctx, node, declNode)) {
        return false;
    }

    return emitCall(ctx, functionIndex);
}

static bool generateExpression(GeneratorContext *ctx, const AstNode *node) {
    if (node == NULL) {
        setError(ctx, "Intermediate Code Error: ekspresi kosong.");
        return false;
    }

    switch (node->type) {
        case AST_INT_LIT:
            return emitLiteral(ctx, runtimeValueInteger(node->ival));
        case AST_REAL_LIT:
            return emitLiteral(ctx, runtimeValueReal(node->rval));
        case AST_CHAR_LIT:
            return emitLiteral(ctx, runtimeValueChar(node->sval != NULL ? node->sval[0] : '\0'));
        case AST_STRING_LIT:
            return emitLiteral(ctx, runtimeValueString(node->sval != NULL ? node->sval : ""));
        case AST_BOOL_LIT:
            return emitLiteral(ctx, runtimeValueBoolean(node->sval != NULL &&
                                                       strcasecmp(node->sval, "true") == 0));
        case AST_VAR:
            return emitIdentifierLoad(ctx, node);
        case AST_ARRAY_ACCESS:
            return generateAddress(ctx, node) &&
                   emitSimple(ctx, OPCODE_LOD, 1, 0);
        case AST_BINOP:
            return generateBinaryExpression(ctx, node);
        case AST_UNOP:
            return generateUnaryExpression(ctx, node);
        case AST_FUNC_CALL:
            return generateFunctionCall(ctx, node);
        case AST_FIELD_ACCESS:
            return generateAddress(ctx, node) &&
                   emitSimple(ctx, OPCODE_LOD, 1, 0);
        default:
            setError(ctx, "Intermediate Code Error: node ekspresi tidak didukung (type=%d).",
                     (int)node->type);
            return false;
    }
}

static bool generateAssignment(GeneratorContext *ctx, const AstNode *node) {
    DirectAddress ref;
    const AstNode *target;
    const AstNode *source;
    BaseType targetType;
    BaseType sourceType;
    int targetRef;
    int sourceRef;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: assignment tidak lengkap.");
        return false;
    }

    target = node->children[0];
    source = node->children[1];

    if (!nodeTypeRef(ctx, target, &targetType, &targetRef) ||
        !nodeTypeRef(ctx, source, &sourceType, &sourceRef)) {
        return false;
    }

    if (targetType == TYPE_ARRAY || targetType == TYPE_RECORD ||
        sourceType == TYPE_ARRAY || sourceType == TYPE_RECORD) {
        if (targetType != sourceType ||
            sizeOfType(targetType, targetRef) != sizeOfType(sourceType, sourceRef)) {
            setError(ctx, "Intermediate Code Error: structured assignment tidak kompatibel.");
            return false;
        }
        return emitStructuredAssignment(ctx, target, source, targetType, targetRef);
    }

    if (!generateExpression(ctx, source) ||
        !emitValueGuardsForTarget(ctx, target, source)) {
        return false;
    }

    if (target->type == AST_VAR) {
        if (!directVariableReference(ctx, target, &ref)) {
            return false;
        }
        return emitSimple(ctx, OPCODE_STO, ref.level, ref.operand);
    }

    return generateAddress(ctx, target) &&
           emitSimple(ctx, OPCODE_STO, 1, 0);
}

static bool generateWriteCall(GeneratorContext *ctx, const AstNode *node) {
    const AstNode *params = firstParamList(node);
    bool isWriteln = node->sval != NULL && strcasecmp(node->sval, "writeln") == 0;

    if (params == NULL || params->childCount == 0) {
        if (isWriteln) {
            return emitLiteral(ctx, runtimeValueString("")) &&
                   emitSimple(ctx, OPCODE_OPR, 0, OPR_WRTLN);
        }
        return true;
    }

    for (size_t i = 0; i < params->childCount; i++) {
        OprCode op = OPR_WRT;

        if (!generateExpression(ctx, params->children[i])) {
            return false;
        }

        if (isWriteln && i + 1 == params->childCount) {
            op = OPR_WRTLN;
        }

        if (!emitSimple(ctx, OPCODE_OPR, 0, op)) {
            return false;
        }
    }

    return true;
}

static bool generateProcedureCall(GeneratorContext *ctx, const AstNode *node) {
    int procIndex;
    const AstNode *declNode;

    if (node->sval != NULL &&
        (strcasecmp(node->sval, "read") == 0 || strcasecmp(node->sval, "readln") == 0)) {
        setError(ctx, "read/readln is not supported in Milestone 4 runtime");
        return false;
    }

    if (node->sval != NULL &&
        (strcasecmp(node->sval, "write") == 0 || strcasecmp(node->sval, "writeln") == 0)) {
        return generateWriteCall(ctx, node);
    }

    procIndex = nodeSymbolIndex(node);
    if (procIndex < 0 || procIndex >= symTabCount() || tab[procIndex].obj != OBJ_PROCEDURE) {
        setError(ctx, "Intermediate Code Error: procedure call '%s' tidak valid.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    declNode = findSubprogramNode(ctx, procIndex);
    if (declNode == NULL) {
        setError(ctx, "Intermediate Code Error: deklarasi procedure '%s' tidak ditemukan.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!emitCallArguments(ctx, node, declNode)) {
        return false;
    }

    return emitCall(ctx, procIndex);
}

static bool generateIf(GeneratorContext *ctx, const AstNode *node) {
    size_t falseJump;
    size_t endJump;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: if statement tidak lengkap.");
        return false;
    }

    if (!generateExpression(ctx, node->children[0])) {
        return false;
    }
    if (!emitPatchable(ctx, OPCODE_JPC, &falseJump)) {
        return false;
    }
    if (!generateStatement(ctx, node->children[1])) {
        return false;
    }

    if (node->childCount >= 3) {
        if (!emitPatchable(ctx, OPCODE_JMP, &endJump)) {
            return false;
        }
        if (!patchOperand(ctx, falseJump, (long long)ctx->instructions->count)) {
            return false;
        }
        if (!generateStatement(ctx, node->children[2])) {
            return false;
        }
        return patchOperand(ctx, endJump, (long long)ctx->instructions->count);
    }

    return patchOperand(ctx, falseJump, (long long)ctx->instructions->count);
}

static bool generateWhile(GeneratorContext *ctx, const AstNode *node) {
    size_t loopStart;
    size_t exitJump;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: while statement tidak lengkap.");
        return false;
    }

    loopStart = ctx->instructions->count;
    if (!generateExpression(ctx, node->children[0])) {
        return false;
    }
    if (!emitPatchable(ctx, OPCODE_JPC, &exitJump)) {
        return false;
    }
    if (!generateStatement(ctx, node->children[1])) {
        return false;
    }
    if (!emitSimple(ctx, OPCODE_JMP, 0, (long long)loopStart)) {
        return false;
    }
    return patchOperand(ctx, exitJump, (long long)ctx->instructions->count);
}

static bool generateRepeat(GeneratorContext *ctx, const AstNode *node) {
    size_t loopStart;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: repeat statement tidak lengkap.");
        return false;
    }

    loopStart = ctx->instructions->count;
    if (!generateStatement(ctx, node->children[0])) {
        return false;
    }
    if (!generateExpression(ctx, node->children[1])) {
        return false;
    }
    return emitSimple(ctx, OPCODE_JPC, 0, (long long)loopStart);
}

static bool generateFor(GeneratorContext *ctx, const AstNode *node) {
    int idx;
    DirectAddress ref;
    size_t loopStart;
    size_t exitJump;
    size_t nextExitJump;
    int nextAddress = compilerTempAddress(IC_TEMP_FOR_NEXT);
    OprCode compareOp = node->ival < 0 ? OPR_GEQ : OPR_LEQ;
    OprCode stepOp = node->ival < 0 ? OPR_SUB : OPR_ADD;

    if (node->childCount < 3) {
        setError(ctx, "Intermediate Code Error: for statement tidak lengkap.");
        return false;
    }

    idx = nodeSymbolIndex(node);
    if (idx < 0 || idx >= symTabCount() || tab[idx].obj != OBJ_VARIABLE) {
        setError(ctx, "Intermediate Code Error: counter for '%s' tidak valid.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!directVariableReference(ctx, node, &ref)) {
        return false;
    }

    if (!generateExpression(ctx, node->children[0]) ||
        !emitValueGuardsForTabIndex(ctx, idx, (BaseType)node->children[0]->typeIdx) ||
        !emitSimple(ctx, OPCODE_STO, ref.level, ref.operand)) {
        return false;
    }

    loopStart = ctx->instructions->count;
    if (!emitSimple(ctx, OPCODE_LOD, ref.level, ref.operand) ||
        !generateExpression(ctx, node->children[1]) ||
        !emitSimple(ctx, OPCODE_OPR, 0, compareOp) ||
        !emitPatchable(ctx, OPCODE_JPC, &exitJump) ||
        !generateStatement(ctx, node->children[2]) ||
        !emitSimple(ctx, OPCODE_LOD, ref.level, ref.operand) ||
        !emitLiteral(ctx, runtimeValueInteger(1)) ||
        !emitSimple(ctx, OPCODE_OPR, 0, stepOp) ||
        !emitSimple(ctx, OPCODE_STO, 0, nextAddress) ||
        !emitSimple(ctx, OPCODE_LOD, 0, nextAddress) ||
        !generateExpression(ctx, node->children[1]) ||
        !emitSimple(ctx, OPCODE_OPR, 0, compareOp) ||
        !emitPatchable(ctx, OPCODE_JPC, &nextExitJump) ||
        !emitSimple(ctx, OPCODE_LOD, 0, nextAddress) ||
        (tab[idx].type == TYPE_CHAR && !emitSimple(ctx, OPCODE_OPR, 0, OPR_TO_CHAR)) ||
        !emitValueGuardsForTabIndex(ctx, idx, tab[idx].type) ||
        !emitSimple(ctx, OPCODE_STO, ref.level, ref.operand) ||
        !emitSimple(ctx, OPCODE_JMP, 0, (long long)loopStart)) {
        return false;
    }

    return patchOperand(ctx, exitJump, (long long)ctx->instructions->count) &&
           patchOperand(ctx, nextExitJump, (long long)ctx->instructions->count);
}

static bool isCaseLabelNode(const AstNode *node) {
    if (node == NULL) {
        return false;
    }
    return node->type == AST_INT_LIT ||
           node->type == AST_REAL_LIT ||
           node->type == AST_CHAR_LIT ||
           node->type == AST_STRING_LIT ||
           node->type == AST_BOOL_LIT ||
           node->type == AST_VAR;
}

static const AstNode *caseBlockStatement(const AstNode *block) {
    if (block == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < block->childCount; i++) {
        if (!isCaseLabelNode(block->children[i])) {
            return block->children[i];
        }
    }
    return NULL;
}

static bool generateCase(GeneratorContext *ctx, const AstNode *node) {
    size_t endJumps[MAX_TAB];
    size_t endJumpCount = 0;

    if (node->childCount < 1) {
        setError(ctx, "Intermediate Code Error: case statement tidak lengkap.");
        return false;
    }

    if (!generateExpression(ctx, node->children[0]) ||
        !emitSimple(ctx, OPCODE_STO, 0, compilerTempAddress(IC_TEMP_CASE_SELECTOR))) {
        return false;
    }

    for (size_t i = 1; i < node->childCount; i++) {
        const AstNode *block = node->children[i];
        const AstNode *statement;

        if (block->type != AST_CASE_BLOCK) {
            continue;
        }

        statement = caseBlockStatement(block);
        if (statement == NULL) {
            continue;
        }

        for (size_t j = 0; j < block->childCount; j++) {
            size_t nextCase;
            size_t endJump;

            if (!isCaseLabelNode(block->children[j])) {
                continue;
            }

            if (!emitSimple(ctx, OPCODE_LOD, 0, compilerTempAddress(IC_TEMP_CASE_SELECTOR)) ||
                !generateExpression(ctx, block->children[j]) ||
                !emitSimple(ctx, OPCODE_OPR, 0, OPR_EQL) ||
                !emitPatchable(ctx, OPCODE_JPC, &nextCase) ||
                !generateStatement(ctx, statement) ||
                !emitPatchable(ctx, OPCODE_JMP, &endJump) ||
                !patchOperand(ctx, nextCase, (long long)ctx->instructions->count)) {
                return false;
            }

            if (endJumpCount >= MAX_TAB) {
                setError(ctx, "Intermediate Code Error: case label terlalu banyak.");
                return false;
            }
            endJumps[endJumpCount++] = endJump;
        }
    }

    for (size_t i = 0; i < endJumpCount; i++) {
        if (!patchOperand(ctx, endJumps[i], (long long)ctx->instructions->count)) {
            return false;
        }
    }

    return true;
}

static bool generateStatement(GeneratorContext *ctx, const AstNode *node) {
    if (node == NULL) {
        return true;
    }

    switch (node->type) {
        case AST_COMPOUND:
        case AST_STMT_LIST:
            for (size_t i = 0; i < node->childCount; i++) {
                if (!generateStatement(ctx, node->children[i])) {
                    return false;
                }
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < node->childCount; i++) {
                if (node->children[i]->type == AST_COMPOUND) {
                    return generateStatement(ctx, node->children[i]);
                }
            }
            return true;
        case AST_ASSIGN:
            return generateAssignment(ctx, node);
        case AST_PROC_CALL:
            return generateProcedureCall(ctx, node);
        case AST_IF:
            return generateIf(ctx, node);
        case AST_WHILE:
            return generateWhile(ctx, node);
        case AST_REPEAT:
            return generateRepeat(ctx, node);
        case AST_FOR:
            return generateFor(ctx, node);
        case AST_EMPTY_STMT:
        case AST_VAR_DECL:
        case AST_CONST_DECL:
        case AST_TYPE_DECL:
        case AST_PROC_DECL:
        case AST_FUNC_DECL:
            return true;
        case AST_CASE:
            return generateCase(ctx, node);
        default:
            setError(ctx, "Intermediate Code Error: statement tidak didukung (type=%d).",
                     (int)node->type);
            return false;
    }
}

static const AstNode *programCompound(const AstNode *root) {
    if (root == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < root->childCount; i++) {
        if (root->children[i]->type == AST_COMPOUND) {
            return root->children[i];
        }
    }
    return NULL;
}

static const AstNode *blockCompound(const AstNode *block) {
    if (block == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < block->childCount; i++) {
        if (block->children[i]->type == AST_COMPOUND) {
            return block->children[i];
        }
    }
    return NULL;
}

static bool generateSubprograms(GeneratorContext *ctx, const AstNode *node);

static bool generateSubprogram(GeneratorContext *ctx, const AstNode *node) {
    int tabIndex = nodeSymbolIndex(node);
    const AstNode *body = NULL;
    size_t bodyJump;

    for (size_t i = 0; i < node->childCount; i++) {
        if (node->children[i]->type == AST_BLOCK) {
            body = node->children[i];
            break;
        }
    }

    if (body == NULL) {
        setError(ctx, "Intermediate Code Error: subprogram '%s' tidak memiliki body.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!defineLabel(ctx, tabIndex, ctx->instructions->count) ||
        !addRuntimeCallInfo(ctx, tabIndex, ctx->instructions->count)) {
        return false;
    }

    if (!emitPatchable(ctx, OPCODE_JMP, &bodyJump) ||
        !generateSubprograms(ctx, body) ||
        !patchOperand(ctx, bodyJump, (long long)ctx->instructions->count)) {
        return false;
    }

    if (!generateStatement(ctx, blockCompound(body))) {
        return false;
    }

    return emitSimple(ctx, OPCODE_RET, 0, 0);
}

static bool generateSubprograms(GeneratorContext *ctx, const AstNode *node) {
    if (node == NULL) {
        return true;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        const AstNode *child = node->children[i];

        if (child->type == AST_PROC_DECL || child->type == AST_FUNC_DECL) {
            if (!generateSubprogram(ctx, child)) {
                return false;
            }
        } else {
            if (!generateSubprograms(ctx, child)) {
                return false;
            }
        }
    }

    return true;
}

bool intermediateCodeGenerate(const AstNode *root,
                              InstructionList *out,
                              char *error,
                              size_t errorSize) {
    GeneratorContext ctx;
    const AstNode *compound;
    size_t mainJump;

    if (out == NULL) {
        if (error != NULL && errorSize > 0) {
            (void)snprintf(error, errorSize, "Intermediate Code Error: output instruction list kosong.");
        }
        return false;
    }

    instructionListInit(out);
    if (error != NULL && errorSize > 0) {
        error[0] = '\0';
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.instructions = out;
    ctx.root = root;
    ctx.error = error;
    ctx.errorSize = errorSize;
    ctx.hasError = false;

    if (root == NULL || root->type != AST_PROGRAM) {
        setError(&ctx, "Intermediate Code Error: root AST harus berupa program.");
        return false;
    }

    compound = programCompound(root);
    if (compound == NULL) {
        setError(&ctx, "Intermediate Code Error: program tidak memiliki compound statement.");
        instructionListFree(out);
        return false;
    }

    if (!emitSimple(&ctx, OPCODE_INT, 0, intermediateCodeGlobalMemorySize()) ||
        !emitPatchable(&ctx, OPCODE_JMP, &mainJump) ||
        !generateSubprograms(&ctx, root) ||
        !patchOperand(&ctx, mainJump, (long long)ctx.instructions->count) ||
        !generateStatement(&ctx, compound) ||
        !emitSimple(&ctx, OPCODE_RET, 0, 0) ||
        !patchPendingCalls(&ctx)) {
        instructionListFree(out);
        return false;
    }

    if (ctx.hasError) {
        instructionListFree(out);
        return false;
    }

    return true;
}
