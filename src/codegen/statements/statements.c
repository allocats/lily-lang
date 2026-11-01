#include "codegen/statements/statements.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "codegen/defers/defers.h"
#include "codegen/expressions/expressions.h"
#include "codegen/types.h"
#include "codegen/types/types.h"
#include "ids.h"
#include "types/entries/entries.h"
#include "types/entries/kinds.h"
#include "utils/macros.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

// ====== Returns ======
static bool codegen_return_statement(CodegenCtx *ctx, const AstNode *node);

// ====== Variable Declaration =======
static bool codegen_variable_declaration(CodegenCtx *ctx, const AstNode *node);

/* 
 *
 * ==================================
 *
 *          Block entrypoint
 *
 * ==================================
 * 
 */

BlockResult codegen_block(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_BLOCK);

    defer_context_enter(ctx);

    for (u32 i = 0; i < node->as.block.statements.count; i += 1) {
        const AstNodeId statement_id  = node->as.block.statements.ids[i];
        const AstNode *statement_node = &ctx->file->ast.nodes[statement_id];

        switch (statement_node->kind) {
        case AST_BLOCK: {
            BlockResult result = codegen_block(ctx, statement_node);

            if (result == BLOCK_ERROR || result == BLOCK_TERMINATED) {
                return result;
            }

            break;
        }

        case AST_VARIABLE_DECL: {
            if (!codegen_variable_declaration(ctx, statement_node)) {
                return BLOCK_ERROR;
            }

            break;
        }

        case AST_UNARY_OP:
        case AST_BINARY_OP:
        case AST_FUNCTION_CALL: {
            if (!codegen_expression(ctx, statement_node)) {
                return BLOCK_ERROR;
            }

            break;
        }

        case AST_DEFER_STMT: {
            defer_context_append(ctx, statement_node->as.defer_stmt.expr);

            break;
        }

        case AST_RETURN_STMT: {
            if (codegen_return_statement(ctx, statement_node)) {
                defer_context_exit(ctx);
                return BLOCK_TERMINATED;
            }

            return BLOCK_ERROR;
        }

        default:
            UNREACHABLE("Impossible statement %s", AST_KIND_STRINGS[statement_node->kind]);
        }
    }

    if (!defer_context_emit_current_stack(ctx)) {
        return BLOCK_ERROR;
    }

    defer_context_exit(ctx);

    return BLOCK_NOT_TERMINATED;
}

/* 
 *
 * ==================================
 *
 *          Return statement
 *
 * ==================================
 * 
 */

static bool codegen_return_statement(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_RETURN_STMT);

    // ================ Emit deferred statments ===================
    if (!defer_context_emit_all_stacks(ctx)) {
        return false;
    }

    // ================ emit llvm.va_end ==================

    if (ctx->llvm_ap_address != null) {
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

    // ================ Actual return ===================

    if (node->as.return_stmt.expr == AST_NODE_ID_NONE) {
        LLVMBuildRetVoid(ctx->llvm_builder);
        return true;
    }

    const AstNode *expression = &ctx->file->ast.nodes[node->as.return_stmt.expr];

    LLVMValueRef value = codegen_expression(ctx, expression);

    LLVMBuildRet(ctx->llvm_builder, value);

    return true;
}

/* 
 *
 * ==================================
 *
 *        Variable Declaration
 *
 * ==================================
 * 
 */

static bool codegen_variable_declaration(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_VARIABLE_DECL);

    LLVMTypeRef type = type_to_llvm_type(node->resolved_type_id);

    // Move builder to the declarations block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, ctx->declarations_block);

    LLVMValueRef address = LLVMBuildAlloca(ctx->llvm_builder, type, "");

    ctx->symbol_map[node->resolved_symbol_id] = address;

    // Reposition builder back to the function block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, ctx->current_block);

    if (node->as.variable_decl.init_expr != AST_NODE_ID_NONE) {
        const AstNodeId expression_id  = node->as.variable_decl.init_expr;
        const AstNode *expression_node = &ctx->file->ast.nodes[expression_id];

        LLVMValueRef value = codegen_expression(ctx, expression_node);

        if (value == null) {
            return false;
        }

        LLVMBuildStore(ctx->llvm_builder, value, address);
    } else {
        if (type_is_kind(&ctx->ws->type_table, node->resolved_type_id, TYPE_POINTER)) {
            LLVMValueRef null_value = LLVMConstPointerNull(type);

            if (null_value == null) {
                return false;
            }

            LLVMBuildStore(ctx->llvm_builder, null_value, address);
        }
    }

    return true;
}
