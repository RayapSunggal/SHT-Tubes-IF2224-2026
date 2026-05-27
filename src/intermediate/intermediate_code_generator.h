#ifndef INTERMEDIATE_CODE_GENERATOR_H
#define INTERMEDIATE_CODE_GENERATOR_H

#include <stdbool.h>
#include <stddef.h>

#include "../ast/ast_node.h"
#include "intermediate_code.h"

#define IC_RESERVED_RUNTIME_CELLS 3

bool intermediateCodeGenerate(const AstNode *root, InstructionList *out, char *error, size_t errorSize);

int intermediateCodeRuntimeAddressForTabIndex(int tabIndex);
int intermediateCodeGlobalMemorySize(void);

#endif
