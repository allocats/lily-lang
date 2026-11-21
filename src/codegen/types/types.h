#ifndef LILY_CODEGEN_TYPES_TYPES_H
#define LILY_CODEGEN_TYPES_TYPES_H

#include "codegen/types.h"
#include "ids.h"

#include <llvm-c/Types.h>

LLVMTypeRef type_to_llvm_type(TypeId id);

bool type_is_string(CodegenCtx *ctx, TypeId id);

#endif // !LILY_CODEGEN_TYPES_TYPES_H
