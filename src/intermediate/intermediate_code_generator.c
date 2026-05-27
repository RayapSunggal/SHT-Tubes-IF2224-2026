#include "intermediate_code_generator.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "../semantic/symbol_table.h"

typedef struct {
    InstructionList *instructions;
    char *error;
    size_t errorSize;
    bool hasError;
} GeneratorContext;

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

int intermediateCodeRuntimeAddressForTabIndex(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= symTabCount()) {
        return -1;
    }
    if (tab[tabIndex].obj != OBJ_VARIABLE) {
        return -1;
    }
    return IC_RESERVED_RUNTIME_CELLS + tab[tabIndex].adr;
}

int intermediateCodeGlobalMemorySize(void) {
    int size = IC_RESERVED_RUNTIME_CELLS;
    int tabCount = symTabCount();

    for (int i = 0; i < tabCount; i++) {
        int endAddress;
        int entrySize;

        if (tab[i].identifier == NULL || tab[i].obj != OBJ_VARIABLE || tab[i].lev != 0) {
            continue;
        }

        entrySize = sizeOfType(tab[i].type, tab[i].ref);
        if (entrySize <= 0) {
            entrySize = 1;
        }

        endAddress = IC_RESERVED_RUNTIME_CELLS + tab[i].adr + entrySize;
        if (endAddress > size) {
            size = endAddress;
        }
    }

    return size;
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

static bool generateExpression(GeneratorContext *ctx, const AstNode *node);
static bool generateStatement(GeneratorContext *ctx, const AstNode *node);

static bool emitIdentifierLoad(GeneratorContext *ctx, const AstNode *node) {
    int idx;
    int address;

    if (node == NULL) {
        setError(ctx, "Intermediate Code Error: identifier kosong.");
        return false;
    }

    idx = node->tabIdx >= 0 ? node->tabIdx : symLookup(node->sval);
    if (idx < 0 || idx >= symTabCount()) {
        setError(ctx, "Intermediate Code Error: identifier '%s' tidak ditemukan di symbol table.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (tab[idx].obj == OBJ_CONSTANT) {
        return emitLiteral(ctx, runtimeValueInteger(tab[idx].adr));
    }

    if (tab[idx].obj != OBJ_VARIABLE) {
        setError(ctx, "Intermediate Code Error: identifier '%s' bukan nilai yang bisa dimuat.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    address = intermediateCodeRuntimeAddressForTabIndex(idx);
    if (address < 0) {
        setError(ctx, "Intermediate Code Error: alamat variabel '%s' tidak valid.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    return emitSimple(ctx, OPCODE_LOD, 0, address);
}

static bool generateBinaryExpression(GeneratorContext *ctx, const AstNode *node) {
    OprCode code;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: binary expression tidak lengkap.");
        return false;
    }

    if (!oprCodeFromOperator(node->sval, &code)) {
        setError(ctx, "Intermediate Code Error: operator '%s' belum didukung pada scope stack-machine dasar.",
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

    if (node->sval == NULL || strcmp(node->sval, "-") != 0) {
        setError(ctx, "Intermediate Code Error: unary operator '%s' belum didukung pada scope ini.",
                 node->sval != NULL ? node->sval : "?");
        return false;
    }

    if (!generateExpression(ctx, node->children[0])) {
        return false;
    }

    return emitSimple(ctx, OPCODE_OPR, 0, OPR_NEG);
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
        case AST_BINOP:
            return generateBinaryExpression(ctx, node);
        case AST_UNOP:
            return generateUnaryExpression(ctx, node);
        case AST_ARRAY_ACCESS:
        case AST_FIELD_ACCESS:
            setError(ctx, "Intermediate Code Error: akses array/record belum termasuk scope implementasi linear dasar.");
            return false;
        case AST_FUNC_CALL:
            setError(ctx, "Intermediate Code Error: function call belum termasuk scope implementasi linear dasar.");
            return false;
        default:
            setError(ctx, "Intermediate Code Error: node ekspresi belum didukung.");
            return false;
    }
}

static bool generateAssignment(GeneratorContext *ctx, const AstNode *node) {
    const AstNode *target;
    const AstNode *expr;
    int idx;
    int address;

    if (node->childCount < 2) {
        setError(ctx, "Intermediate Code Error: assignment tidak lengkap.");
        return false;
    }

    target = node->children[0];
    expr = node->children[1];

    if (target->type != AST_VAR) {
        setError(ctx, "Intermediate Code Error: assignment array/record belum termasuk scope implementasi ini.");
        return false;
    }

    idx = target->tabIdx >= 0 ? target->tabIdx : symLookup(target->sval);
    if (idx < 0 || idx >= symTabCount() || tab[idx].obj != OBJ_VARIABLE) {
        setError(ctx, "Intermediate Code Error: target assignment '%s' bukan variabel.",
                 target->sval != NULL ? target->sval : "?");
        return false;
    }

    address = intermediateCodeRuntimeAddressForTabIndex(idx);
    if (address < 0) {
        setError(ctx, "Intermediate Code Error: alamat target assignment '%s' tidak valid.",
                 target->sval != NULL ? target->sval : "?");
        return false;
    }

    if (!generateExpression(ctx, expr)) {
        return false;
    }

    return emitSimple(ctx, OPCODE_STO, 0, address);
}

static bool generateWriteCall(GeneratorContext *ctx, const AstNode *node) {
    const AstNode *params = firstParamList(node);
    bool isWriteln = node->sval != NULL && strcasecmp(node->sval, "writeln") == 0;

    if (params == NULL || params->childCount == 0) {
        setError(ctx, "Intermediate Code Error: write/writeln tanpa argumen belum didukung pada scope ini.");
        return false;
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
    if (node->sval != NULL &&
        (strcasecmp(node->sval, "write") == 0 || strcasecmp(node->sval, "writeln") == 0)) {
        return generateWriteCall(ctx, node);
    }

    setError(ctx, "Intermediate Code Error: procedure call '%s' belum termasuk scope implementasi linear dasar.",
             node->sval != NULL ? node->sval : "?");
    return false;
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
        case AST_ASSIGN:
            return generateAssignment(ctx, node);
        case AST_PROC_CALL:
            return generateProcedureCall(ctx, node);
        case AST_EMPTY_STMT:
            return true;
        case AST_IF:
        case AST_WHILE:
        case AST_FOR:
        case AST_REPEAT:
        case AST_CASE:
            setError(ctx, "Intermediate Code Error: control-flow belum dikerjakan pada scope stack-machine dasar.");
            return false;
        default:
            return true;
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

bool intermediateCodeGenerate(const AstNode *root,
                              InstructionList *out,
                              char *error,
                              size_t errorSize) {
    GeneratorContext ctx;
    const AstNode *compound;

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

    ctx.instructions = out;
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

    if (!emitSimple(&ctx, OPCODE_INT, 0, intermediateCodeGlobalMemorySize())) {
        instructionListFree(out);
        return false;
    }
    if (!generateStatement(&ctx, compound)) {
        instructionListFree(out);
        return false;
    }
    if (!emitSimple(&ctx, OPCODE_RET, 0, 0)) {
        instructionListFree(out);
        return false;
    }

    if (ctx.hasError) {
        instructionListFree(out);
        return false;
    }

    return true;
}
