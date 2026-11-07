#ifndef LILY_CODEGEN_DECLARATIONS_H
#define LILY_CODEGEN_DECLARATIONS_H

#include "codegen/types.h"

LLVMValueRef codegen_function_signature(CodegenCtx *ctx, SymbolId symbol_id);

bool codegen_function(CodegenCtx *ctx, const AstNode *node);
bool codegen_global_variable(CodegenCtx *ctx, const AstNode *node);

#endif // !LILY_CODEGEN_DECLARATIONS_H
