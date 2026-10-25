#ifndef LILY_CODEGEN_TYPES_H
#define LILY_CODEGEN_TYPES_H

#include <llvm-c/Types.h>

typedef struct {
    /*
     *
     * This is created once per thread, and destroyed once per thread
     *
     */
    LLVMContextRef llvm_ctx;

    /*
     *
     * Current module (file)
     *
     */
    LLVMModuleRef llvm_module;

    /*
     *
     * Bob the builder
     *
     */
    LLVMBuilderRef llvm_builder;

    /*
     *
     * These three fields are created once, and destroyed at the same time.
     * Indexed using the corresponding IDs.
     *
     * i.e. For string id 46
     * > string = ctx.string_map[46];
     *
     */
    LLVMValueRef *symbol_map;
    LLVMValueRef *string_map;
    LLVMTypeRef *type_map;
} CodegenCtx;

#endif // !LILY_CODEGEN_TYPES_H
