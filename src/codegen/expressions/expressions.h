#ifndef LILY_CODEGEN_EXPRESSIONS_H
#define LILY_CODEGEN_EXPRESSIONS_H

#include "codegen/types.h"

LLVMValueRef codegen_expression(CodegenCtx *ctx, const AstNode *node);

#endif // !LILY_CODEGEN_EXPRESSIONS_H
