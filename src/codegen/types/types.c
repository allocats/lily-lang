#include "codegen/types.h"
#include "threadpool/types.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"
#include "types/entries/types.h"
#include "utils/macros.h"

#include <assert.h>

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

extern thread_local ThreadContext thread_context;

static LLVMTypeRef base_type_to_llvm_type(const TypeId id) {
    CodegenCtx *ctx = thread_context.codegen_ctx;

    const BuiltinTypeIds *builtins = &ctx->ws->builtin_types;

    if (builtins->type_void == id) {
        return LLVMVoidTypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_i8 == id || builtins->type_u8 == id || builtins->type_bool == id || builtins->type_char == id) {
        return LLVMInt8TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_i16 == id || builtins->type_u16 == id) {
        return LLVMInt16TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_i32 == id || builtins->type_u32 == id) {
        return LLVMInt32TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_i64 == id || builtins->type_u64 == id) {
        return LLVMInt64TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_isize == id || builtins->type_usize == id) {
        return LLVMInt64TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_f32 == id) {
        return LLVMFloatTypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_f64 == id) {
        return LLVMDoubleTypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_va_list == id) {
        return LLVMPointerTypeInContext(ctx->llvm_ctx, 0);
    }

    UNREACHABLE("Invalid base type");
}

LLVMTypeRef type_to_llvm_type(TypeId id) {
    CodegenCtx *ctx = thread_context.codegen_ctx;

    if (ctx->type_map[id] != null) {
        return ctx->type_map[id];
    }

    const TypeTable *table = &ctx->ws->type_table;
    const Type *type       = &table->entries[id];

    switch (type->kind) {
    case TYPE_BASE:
        return ctx->type_map[id] = base_type_to_llvm_type(id);

    case TYPE_POINTER:
        return ctx->type_map[id] = LLVMPointerType(type_to_llvm_type(type->as.pointer_type.pointee_id), 0);

    default:
        UNREACHABLE("Invalid type");
    }
}
