#ifndef LILY_CODEGEN_TYPES_TYPES_H
#define LILY_CODEGEN_TYPES_TYPES_H

#include "ids.h"

#include <llvm-c/Types.h>

LLVMTypeRef type_to_llvm_type(TypeId id);

#endif // !LILY_CODEGEN_TYPES_TYPES_H
