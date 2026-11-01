#include "ast/types.h"
#include "codegen/declarations/declarations.h"
#include "codegen/declarations/intrinsics/intrinsics.h"
#include "codegen/statements/statements.h"
#include "codegen/types.h"
#include "codegen/types/types.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "symbols/symbol/kinds.h"
#include "symbols/symbol/types.h"
#include "threadpool/types.h"
#include "utils/macros.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

extern thread_local ThreadContext thread_context;

// ========= Intrinsic =========
static LLVMValueRef codegen_intrinsic_function(CodegenCtx *ctx, const SymbolId id, const Symbol *symbol);

/* 
 *
 * ===========================================
 *
 *                 Functions 
 *
 * ===========================================
 * 
 */

LLVMValueRef codegen_function_signature(CodegenCtx *ctx, SymbolId symbol_id) {
    if (ctx->symbol_map[symbol_id] != null) {
        return ctx->symbol_map[symbol_id];
    }

    Symbol *symbol = &ctx->ws->global_symbol_table.symbols[symbol_id];
    assert(symbol->kind == SYMBOL_FUNCTION);

    if (symbol->flags & AST_FLAG_INTRINSIC) {
        return codegen_intrinsic_function(ctx, symbol_id, symbol);
    }

    str8 function_name = ctx->ws->global_string_interner.entries[symbol->name_id].str;
    str8 mangled_name  = {0};

    bool name_needs_free = false;

    if (symbol_id == ctx->ws->entry_point_id || symbol->flags & AST_FLAG_FOREIGN) {
        mangled_name = function_name;
    } else {
        const u32 mangled_name_len = function_name.len + 16;

        mangled_name.ptr = lily_alloc(mangled_name_len);
        mangled_name.len = snprintf(mangled_name.ptr, mangled_name_len, "%.*s_%u", STR8_FMT(function_name), symbol_id);

        name_needs_free = true;
    }

    TypeId return_type_id = symbol->as.function_symbol.return_type_id;
    assert(return_type_id != TYPE_ID_NONE);

    LLVMTypeRef return_type = type_to_llvm_type(return_type_id);

    u32 parameter_count = symbol->as.function_symbol.parameter_count;

    bool is_variadic = false;

    if (symbol->flags & AST_FLAG_VARIADIC) {
        parameter_count -= 1;

        is_variadic = true;
    }

    LLVMTypeRef *parameter_types = lily_alloc(sizeof(LLVMTypeRef) * parameter_count);

    for (u32 i = 0; i < parameter_count; i += 1) {
        SymbolId parameter_id = symbol->as.function_symbol.parameters[i];
        assert(parameter_id != GLOBAL_SYMBOL_ID_NONE);

        Symbol *parameter_symbol = &ctx->ws->global_symbol_table.symbols[parameter_id];
        assert(parameter_symbol->kind == SYMBOL_PARAMETER);

        TypeId parameter_type_id = parameter_symbol->as.parameter_symbol.type_id;
        parameter_types[i]       = type_to_llvm_type(parameter_type_id);
    }

    LLVMTypeRef function_type = LLVMFunctionType(return_type, parameter_types, parameter_count, is_variadic);
    LLVMValueRef function     = LLVMGetOrInsertFunction(ctx->llvm_module, mangled_name.ptr, mangled_name.len, function_type);

    if (symbol->flags & AST_FLAG_INLINE) {
        u32 attribute_index = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
        assert(attribute_index != 0);

        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(ctx->llvm_ctx, attribute_index, 0);
        LLVMAddAttributeAtIndex(function, LLVMAttributeFunctionIndex, attribute);

    } else if (symbol->flags & AST_FLAG_NOINLINE) {
        u32 attribute_index = LLVMGetEnumAttributeKindForName("noinline", 8);
        assert(attribute_index != 0);

        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(ctx->llvm_ctx, attribute_index, 0);
        LLVMAddAttributeAtIndex(function, LLVMAttributeFunctionIndex, attribute);
    }

    ctx->symbol_map[symbol_id] = function;

    free(parameter_types);

    if (name_needs_free) {
        free(mangled_name.ptr);
    }

    return function;
}

bool codegen_function(CodegenCtx *ctx, const AstNode *node) {
    SymbolId symbol_id = node->resolved_symbol_id;
    assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

    LLVMValueRef function = codegen_function_signature(ctx, symbol_id);

    if (function == null) {
        return false;
    }

    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[symbol_id];

    if (symbol->flags & AST_FLAG_FOREIGN || symbol->flags & AST_FLAG_INTRINSIC) {
        return true;
    }

    bool is_variadic = symbol->flags & AST_FLAG_VARIADIC;

    u32 parameter_count = symbol->as.function_symbol.parameter_count;

    if (is_variadic) {
        parameter_count -= 1;
    }

    /*
     *  ================================
     *
     *      Declarations (Alloca)
     *
     *  ================================
     */

    LLVMBasicBlockRef declaration_block = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, function, "declarations");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, declaration_block);

    ctx->declarations_block = declaration_block;

    for (u32 i = 0; i < parameter_count; i += 1) {
        const SymbolId parameter_id    = symbol->as.function_symbol.parameters[i];
        const Symbol *parameter_symbol = &ctx->ws->global_symbol_table.symbols[parameter_id];

        LLVMTypeRef parameter_type     = type_to_llvm_type(parameter_symbol->as.parameter_symbol.type_id);
        LLVMValueRef parameter_address = LLVMBuildAlloca(ctx->llvm_builder, parameter_type, "");

        ctx->symbol_map[parameter_id] = parameter_address;
    }

    /*
     *  ===============================
     *
     *      Entry (Store params)    
     *
     *  ===============================
     */

    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, function, "entry");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, entry_block);

    ctx->current_block = entry_block;

    for (u32 i = 0; i < parameter_count; i += 1) {
        const SymbolId parameter_id = symbol->as.function_symbol.parameters[i];

        LLVMValueRef parameter_address = ctx->symbol_map[parameter_id];
        LLVMValueRef parameter_value   = LLVMGetParam(function, i);

        LLVMBuildStore(ctx->llvm_builder, parameter_value, parameter_address);
    }

    if (is_variadic) {
        // Set the builder to the stack allocations block
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, ctx->declarations_block);

        LLVMTypeRef types[4] = {
            LLVMInt32TypeInContext(ctx->llvm_ctx),
            LLVMInt32TypeInContext(ctx->llvm_ctx),
            LLVMPointerTypeInContext(ctx->llvm_ctx, 0),
            LLVMPointerTypeInContext(ctx->llvm_ctx, 0),
        };

        LLVMTypeRef ap_type     = LLVMStructTypeInContext(ctx->llvm_ctx, types, 4, false);
        LLVMValueRef ap_address = LLVMBuildAlloca(ctx->llvm_builder, ap_type, "ap");
        LLVMSetAlignment(ap_address, 16);

        ctx->llvm_ap_address = ap_address;

        // Reset the builder back to the current block
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, ctx->current_block);

        const u32 intrinsic_id = LLVMLookupIntrinsicID("llvm.va_start", sizeof("llvm.va_start") - 1);
        assert(intrinsic_id != 0);

        LLVMTypeRef va_start_overload_types[] = {LLVMPointerTypeInContext(ctx->llvm_ctx, 0)};
        LLVMValueRef va_start_fn              = LLVMGetIntrinsicDeclaration(
            ctx->llvm_module,
            intrinsic_id,
            va_start_overload_types,
            1
        );

        LLVMTypeRef param_types[] = {LLVMPointerTypeInContext(ctx->llvm_ctx, 0)};
        LLVMTypeRef va_start_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->llvm_ctx), param_types, 1, 0);

        LLVMValueRef args[] = {ap_address};

        LLVMBuildCall2(ctx->llvm_builder, va_start_type, va_start_fn, args, 1, "");

        // LLVMBuildVAArg(LLVMBuilderRef, LLVMValueRef List, LLVMTypeRef Ty, const char *Name)
    }

    /*
     *  ======================
     *
     *      Function Body
     *
     *  ======================
     */

    const AstNode *block = &ctx->file->ast.nodes[node->as.func_decl.body];

    const BlockResult result = codegen_block(ctx, block);

    if (result == BLOCK_ERROR) {
        return false;
    }

    if (result == BLOCK_NOT_TERMINATED) {

        // ============= emit llvm.va_end ==============
        if (is_variadic) {
            const u32 intrinsic_id = LLVMLookupIntrinsicID("llvm.va_end", sizeof("llvm.va_end") - 1);
            assert(intrinsic_id != 0);

            LLVMTypeRef va_end_overload_types[] = {LLVMPointerTypeInContext(ctx->llvm_ctx, 0)};
            LLVMValueRef va_end_fn              = LLVMGetIntrinsicDeclaration(
                ctx->llvm_module,
                intrinsic_id,
                va_end_overload_types,
                1
            );

            LLVMTypeRef param_types[] = {LLVMPointerTypeInContext(ctx->llvm_ctx, 0)};
            LLVMTypeRef va_end_type   = LLVMFunctionType(LLVMVoidTypeInContext(ctx->llvm_ctx), param_types, 1, 0);

            LLVMValueRef args[] = {ctx->llvm_ap_address};

            LLVMBuildCall2(ctx->llvm_builder, va_end_type, va_end_fn, args, 1, "");
        }

        // ============= implicit 'return' ==============
        LLVMBuildRetVoid(ctx->llvm_builder);
    }

    /*
     *  ===================================
     *
     *      Terminate the declarations
     *
     *  ===================================
     */

    LLVMPositionBuilderAtEnd(ctx->llvm_builder, declaration_block);
    LLVMBuildBr(ctx->llvm_builder, entry_block);

    return true;
}

/* 
 *
 * ===========================================
 *
 *                 Intriniscs
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_intrinsic_function(CodegenCtx *ctx, const SymbolId id, const Symbol *symbol) {
    str8 fn_name = ctx->ws->global_string_interner.entries[symbol->name_id].str;

    u32 len = fn_name.len + sizeof("llvm.") - 1;

    str8 full_name = {
        .ptr = lily_alloc(len + 1),
        .len = len
    };

    snprintf(full_name.ptr, full_name.len + 1, "llvm.%.*s", STR8_FMT(fn_name));

    const u32 intrinsic_id = LLVMLookupIntrinsicID(full_name.ptr, full_name.len);

    if (intrinsic_id == 0) {
        diagnostic_add_generic(
            thread_context.diagnostic_engine,
            DIAG_ERROR,
            "LLVM Unknown intrinsic: '%.*s'",
            STR8_FMT(full_name)
        );

        return null;
    }

    LLVMValueRef intrinsic = null;

    if (LLVMIntrinsicIsOverloaded(intrinsic_id)) {
        const IntrinsicInformation *info = intrinsic_get_information(symbol->name_id);

        if (info == null) {
            diagnostic_add_generic(
                thread_context.diagnostic_engine,
                DIAG_ERROR,
                "Intrinsic '%.*s' is overloaded but has no overload info",
                STR8_FMT(fn_name)
            );

            return null;
        }

        const u32 param_count = symbol->as.function_symbol.parameter_count;

        LLVMTypeRef overload_types[8] = {0};
        u32 overload_count            = 0;

        if (info->return_is_overloaded) {
            overload_types[overload_count++] = type_to_llvm_type(symbol->as.function_symbol.return_type_id);
        }

        for (u32 i = 0; i < param_count; i += 1) {
            if (info->parameter_overload_mask & (1u << i)) {
                const SymbolId parameter_id    = symbol->as.function_symbol.parameters[i];
                const Symbol *parameter_symbol = &ctx->ws->global_symbol_table.symbols[parameter_id];

                overload_types[overload_count++] = type_to_llvm_type(parameter_symbol->as.parameter_symbol.type_id);
            }
        }

        intrinsic = LLVMGetIntrinsicDeclaration(ctx->llvm_module, intrinsic_id, overload_types, overload_count);
    } else {
        intrinsic = LLVMGetIntrinsicDeclaration(ctx->llvm_module, intrinsic_id, null, 0);
    }

    ctx->symbol_map[id] = intrinsic;

    free(full_name.ptr);

    return intrinsic;
}
