#include "codegen/types/types.h"
#include "ast/types.h"
#include "symbols/symbol/types.h"
#include "threadpool/types.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"
#include "types/entries/types.h"
#include "utils/macros.h"
#include "utils/wrappers.h"

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

    if (builtins->type_bool == id) {
        return LLVMInt1TypeInContext(ctx->llvm_ctx);
    }

    if (builtins->type_i8 == id || builtins->type_u8 == id || builtins->type_char == id) {
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

static LLVMTypeRef struct_type_to_llvm_type(TypeId id) {
    CodegenCtx *ctx = thread_context.codegen_ctx;

    const Type *type     = &ctx->ws->type_table.entries[id];
    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[type->symbol_id];

    const bool is_packed = symbol->flags & AST_FLAG_PACKED;

    u32 field_count = type->as.struct_type.field_count;

    LLVMTypeRef *llvm_field_types = lily_alloc(sizeof(LLVMTypeRef) * field_count);

    for (u32 i = 0; i < field_count; i += 1) {
        const TypeId field_type_id = type->as.struct_type.fields[i];

        LLVMTypeRef llvm_field_type = type_to_llvm_type(field_type_id);
        assert(llvm_field_type != null);

        llvm_field_types[i] = llvm_field_type;
    }

    // get the name
    str8 name_str = ctx->ws->global_string_interner.entries[symbol->name_id].str;

    // null terminate the string temporarily
    char putback                   = *(name_str.ptr + name_str.len);
    *(name_str.ptr + name_str.len) = 0;

    LLVMTypeRef llvm_type = LLVMStructCreateNamed(ctx->llvm_ctx, name_str.ptr);

    // putback the char
    *(name_str.ptr + name_str.len) = putback;

    LLVMStructSetBody(llvm_type, llvm_field_types, field_count, is_packed);

    free(llvm_field_types);

    return llvm_type;
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

    case TYPE_STRUCT:
        return ctx->type_map[id] = struct_type_to_llvm_type(id);

    default:
        UNREACHABLE("Invalid type");
    }
}

inline bool type_is_string(CodegenCtx *ctx, TypeId id) {
    return id == ctx->ws->builtin_types.type_string;
}
