#include "codegen/expressions/expressions.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "codegen/declarations/declarations.h"
#include "codegen/types.h"
#include "codegen/types/types.h"
#include "ids.h"
#include "symbols/symbol/kinds.h"
#include "utils/macros.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdio.h>

// =========== Identifiers =============
static LLVMValueRef codegen_identifier(CodegenCtx *ctx, const AstNode *node);

// =========== Identifiers =============
static LLVMValueRef codegen_function_call(CodegenCtx *ctx, const AstNode *node);

// =========== Literals =============
static LLVMValueRef codegen_literal(CodegenCtx *ctx, const AstNode *node);
static LLVMValueRef codegen_string_literal(CodegenCtx *ctx, const StringId id);

/* 
 *
 * ===========================================
 *
 *          Expression entrypoint
 *
 * ===========================================
 * 
 */

LLVMValueRef codegen_expression(CodegenCtx *ctx, const AstNode *node) {
    switch (node->kind) {
    case AST_LITERAL:
        return codegen_literal(ctx, node);

    case AST_IDENTIFIER:
        return codegen_identifier(ctx, node);

    case AST_FUNCTION_CALL:
        return codegen_function_call(ctx, node);

    default:
        return null;
    }
}

/* 
 *
 * ===========================================
 *
 *                  Identifiers
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_identifier(CodegenCtx *ctx, const AstNode *node) {
    const SymbolId symbol_id = node->resolved_symbol_id;
    assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[symbol_id];

    switch (symbol->kind) {
    case SYMBOL_VARIABLE: {
        LLVMValueRef address = ctx->symbol_map[symbol_id];
        assert(address != null);

        LLVMTypeRef type = type_to_llvm_type(symbol->as.variable_symbol.type_id);

        return LLVMBuildLoad2(ctx->llvm_builder, type, address, "");
    }

    case SYMBOL_PARAMETER: {
        if (symbol->as.parameter_symbol.type_id == ctx->ws->builtin_types.type_va_list) {
            return ctx->llvm_ap_address;
        }

        LLVMValueRef address = ctx->symbol_map[symbol_id];
        assert(address != null);

        LLVMTypeRef type = type_to_llvm_type(symbol->as.parameter_symbol.type_id);

        return LLVMBuildLoad2(ctx->llvm_builder, type, address, "");
    }

    case SYMBOL_FUNCTION: {
        if (ctx->symbol_map[symbol_id] != null) {
            return ctx->symbol_map[symbol_id];
        }

        return codegen_function_signature(ctx, symbol_id);
    }

    case SYMBOL_VARIANT:
        break;

    default:
        UNREACHABLE("Impossible SymbolKind %s", SYMBOL_KIND_STRINGS[symbol->kind]);
    }

    return null;
}

/* 
 *
 * ================================================
 *
 *                  Function Calls
 *
 * ================================================
 * 
 */

static LLVMValueRef codegen_function_call(CodegenCtx *ctx, const AstNode *node) {
    const AstNode *identifier = &ctx->file->ast.nodes[node->as.func_call.identifier];

    LLVMValueRef function = codegen_expression(ctx, identifier);
    assert(function != null);

    LLVMTypeRef function_type = LLVMGlobalGetValueType(function);

    u32 argument_count = node->as.func_call.arguments.count;

    LLVMValueRef *arguments = lily_alloc(argument_count * sizeof(LLVMValueRef));

    for (u32 i = 0; i < argument_count; i += 1) {
        const AstNodeId argument_id  = node->as.func_call.arguments.ids[i];
        const AstNode *argument_node = &ctx->file->ast.nodes[argument_id];

        LLVMValueRef argument = codegen_expression(ctx, argument_node);
        assert(argument != null);

        arguments[i] = argument;
    }

    LLVMValueRef call = LLVMBuildCall2(ctx->llvm_builder, function_type, function, arguments, argument_count, "");

    free(arguments);

    return call;
}

/* 
 *
 * ===========================================
 *
 *                  Literals
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_literal(CodegenCtx *ctx, const AstNode *node) {
    switch (node->as.literal.kind) {
    case LITERAL_STRING:
        return codegen_string_literal(ctx, node->as.literal.as.string);

    case LITERAL_INTEGER:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.integer, 0);

    case LITERAL_BOOL:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.boolean, 0);

    case LITERAL_CHAR:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.character, 0);

    case LITERAL_FLOATING:
        return LLVMConstReal(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.floating);

    case LITERAL_NULL:
        return LLVMConstPointerNull(type_to_llvm_type(node->resolved_type_id));
    }
}

static LLVMValueRef codegen_string_literal(CodegenCtx *ctx, const StringId id) {
    if (ctx->string_map[id] != null) {
        return ctx->string_map[id];
    }

    char name[64] = {0};
    snprintf(name, sizeof(name), "str.%u", id);

    str8 str = ctx->ws->global_string_interner.entries[id].str;

    char *copy   = lily_calloc(str.len + 1, 1);
    char *cursor = copy;

    for (u32 i = 0; i < str.len; i += 1) {
        if (str.ptr[i] == '\\') {
            i += 1;

            char c = str.ptr[i];

            // at end or early break, no point in parsing rest of the string
            if (c == '0') {
                *cursor++ = '\0';
                break;
            }

            switch (c) {
            case 'a': {
                *cursor++ = '\a';
            } break;
            case 'b': {
                *cursor++ = '\b';
            } break;
            case 'f': {
                *cursor++ = '\f';
            } break;
            case 'n': {
                *cursor++ = '\n';
            } break;
            case 'r': {
                *cursor++ = '\r';
            } break;
            case 't': {
                *cursor++ = '\t';
            } break;
            case 'v': {
                *cursor++ = '\v';
            } break;
            case '\\': {
                *cursor++ = '\\';
            } break;
            case '"': {
                *cursor++ = '\"';
            } break;

            default: {
                *cursor++ = '\\';
                *cursor++ = c;
            } break;
            }
        } else {
            *cursor++ = str.ptr[i];
        }
    }

    LLVMValueRef string = LLVMBuildGlobalString(ctx->llvm_builder, copy, name);

    ctx->string_map[id] = string;

    free(copy);

    return string;
}
