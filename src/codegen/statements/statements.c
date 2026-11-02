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
#include "utils/wrappers.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

// ====== Returns ======
static bool codegen_return_statement(CodegenCtx *ctx, const AstNode *node);

// ====== Variable Declaration =======
static bool codegen_variable_declaration(CodegenCtx *ctx, const AstNode *node);

// ====== If Statements =======
static bool codegen_if_statement(CodegenCtx *ctx, const AstNode *node);

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

        case AST_IF_STMT: {
            if (!codegen_if_statement(ctx, statement_node)) {
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

/* 
 *
 * =============================
 *
 *        If Statements
 *
 * =============================
 * 
 */

static bool codegen_if_statement(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_IF_STMT);

    bool result = true;

    // ==== initialization ====
    const AstNodeId else_id  = node->as.if_stmt.else_block;
    const AstNode *else_node = &ctx->file->ast.nodes[else_id];

    u32 branch_count = node->as.if_stmt.branches.count;

    LLVMBasicBlockRef *condition_blocks = lily_alloc(branch_count * sizeof(LLVMBasicBlockRef));
    LLVMBasicBlockRef *body_blocks      = lily_alloc(branch_count * sizeof(LLVMBasicBlockRef));

    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "if.merge");

    LLVMBasicBlockRef else_block = else_id != AST_NODE_ID_NONE
                                       ? LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "if.else")
                                       : null;

    for (u32 i = 0; i < branch_count; i += 1) {
        condition_blocks[i] = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "if.cond");
        body_blocks[i]      = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "if.body");
    }

    // ==== entry from current block ====
    LLVMBuildBr(ctx->llvm_builder, condition_blocks[0]);

    // ==== each branch ====
    for (u32 i = 0; i < branch_count; i += 1) {
        const AstNodeId branch_id  = node->as.if_stmt.branches.ids[i];
        const AstNode *branch_node = &ctx->file->ast.nodes[branch_id];

        assert(branch_node->kind == AST_BRANCH);

        const AstNodeId condition_id  = branch_node->as.branch.condition;
        const AstNode *condition_node = &ctx->file->ast.nodes[condition_id];

        const AstNodeId body_id  = branch_node->as.branch.body;
        const AstNode *body_node = &ctx->file->ast.nodes[body_id];

        // get branch's blocks
        LLVMBasicBlockRef condition_block = condition_blocks[i];
        LLVMBasicBlockRef body_block      = body_blocks[i];

        LLVMPositionBuilderAtEnd(ctx->llvm_builder, condition_block);
        ctx->current_block = condition_block;

        // ==== branch condition ====
        LLVMValueRef condition = codegen_expression(ctx, condition_node);

        if (!condition) {
            result = false;
            goto exit_codegen_if_statement;
        }

        // ==== next branch if false, else goto body ====
        LLVMBasicBlockRef next_branch_block = merge_block;

        if (i + 1 < branch_count) {
            next_branch_block = condition_blocks[i + 1];
        } else if (else_block != null) {
            next_branch_block = else_block;
        }

        LLVMBuildCondBr(ctx->llvm_builder, condition, body_block, next_branch_block);

        // ==== branch body ====
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, body_block);
        ctx->current_block = body_block;

        BlockResult body_result = codegen_block(ctx, body_node);

        if (body_result == BLOCK_ERROR) {
            result = false;
            goto exit_codegen_if_statement;
        }

        if (body_result == BLOCK_NOT_TERMINATED) {
            LLVMBuildBr(ctx->llvm_builder, merge_block);
        }
    }

    // ==== else block ====
    if (else_block != null) {
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, else_block);
        ctx->current_block = else_block;

        BlockResult body_result = codegen_block(ctx, else_node);

        if (body_result == BLOCK_ERROR) {
            result = false;
            goto exit_codegen_if_statement;
        }

        if (body_result == BLOCK_NOT_TERMINATED) {
            LLVMBuildBr(ctx->llvm_builder, merge_block);
        }
    }

exit_codegen_if_statement:
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_block);
    ctx->current_block = merge_block;

    free(condition_blocks);
    free(body_blocks);

    return result;
}
