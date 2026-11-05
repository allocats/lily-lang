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

// ====== If Statements =======
static BlockResult codegen_if_statement(CodegenCtx *ctx, const AstNode *node);

// ====== While Loop =======
static bool codegen_while_loop(CodegenCtx *ctx, const AstNode *node);

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
            BlockResult result = codegen_if_statement(ctx, statement_node);

            if (result == BLOCK_ERROR || result == BLOCK_TERMINATED) {
                return result;
            }

            break;
        }

        case AST_WHILE_LOOP: {
            if (!codegen_while_loop(ctx, statement_node)) {
                return BLOCK_ERROR;
            }

            break;
        }

        case AST_BREAK_STMT: {
            LLVMBuildBr(ctx->llvm_builder, ctx->control_flow_merge);
            defer_context_exit(ctx);

            return BLOCK_TERMINATED;
        }

        case AST_CONTINUE_STMT: {
            LLVMBuildBr(ctx->llvm_builder, ctx->control_flow_condition);
            defer_context_exit(ctx);

            return BLOCK_TERMINATED;
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

static BlockResult codegen_if_statement(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_IF_STMT);

    // ==== initialization ====
    const AstNodeId else_id  = node->as.if_stmt.else_block;
    const AstNode *else_node = else_id != AST_NODE_ID_NONE ? &ctx->file->ast.nodes[else_id] : null;

    u32 branch_count = node->as.if_stmt.branches.count;

    LLVMBasicBlockRef merge_block = null;

    // ==== each branch ====
    for (u32 i = 0; i < branch_count; i += 1) {
        const AstNodeId branch_id  = node->as.if_stmt.branches.ids[i];
        const AstNode *branch_node = &ctx->file->ast.nodes[branch_id];

        const AstNodeId condition_id  = branch_node->as.branch.condition;
        const AstNode *condition_node = &ctx->file->ast.nodes[condition_id];

        const AstNodeId body_id  = branch_node->as.branch.body;
        const AstNode *body_node = &ctx->file->ast.nodes[body_id];

        // Create branch's body
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "if.body");

        // ==== branch condition ====
        LLVMValueRef condition = codegen_expression(ctx, condition_node);

        if (!condition) {
            return BLOCK_ERROR;
        }

        // ==== next branch if false, else goto body ====
        LLVMBasicBlockRef next_branch_block = null;

        if (i + 1 < branch_count) {
            next_branch_block = LLVMCreateBasicBlockInContext(ctx->llvm_ctx, "if.cond");
        } else if (else_node != null) {
            next_branch_block = LLVMCreateBasicBlockInContext(ctx->llvm_ctx, "if.else");
        } else {
            merge_block = !merge_block
                              ? LLVMCreateBasicBlockInContext(ctx->llvm_ctx, "if.merge")
                              : merge_block;

            next_branch_block = merge_block;
        }

        LLVMBuildCondBr(ctx->llvm_builder, condition, body_block, next_branch_block);

        // ==== branch body ====
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, body_block);
        ctx->current_block = body_block;

        BlockResult body_result = codegen_block(ctx, body_node);

        if (body_result == BLOCK_ERROR) {
            return BLOCK_ERROR;
        }

        if (body_result == BLOCK_NOT_TERMINATED) {
            merge_block = !merge_block
                              ? LLVMCreateBasicBlockInContext(ctx->llvm_ctx, "if.merge")
                              : merge_block;

            LLVMBuildBr(ctx->llvm_builder, merge_block);
        }

        if (next_branch_block != merge_block) {
            LLVMAppendExistingBasicBlock(ctx->current_function, next_branch_block);

            LLVMPositionBuilderAtEnd(ctx->llvm_builder, next_branch_block);
            ctx->current_block = next_branch_block;
        }
    }

    // ==== else block ====
    if (else_node != null) {
        BlockResult body_result = codegen_block(ctx, else_node);

        if (body_result == BLOCK_ERROR) {
            return BLOCK_ERROR;
        }

        if (body_result == BLOCK_NOT_TERMINATED) {
            merge_block = !merge_block
                              ? LLVMCreateBasicBlockInContext(ctx->llvm_ctx, "if.merge")
                              : merge_block;

            LLVMBuildBr(ctx->llvm_builder, merge_block);
        }
    }

    if (!merge_block) {
        return BLOCK_TERMINATED;
    }

    LLVMAppendExistingBasicBlock(ctx->current_function, merge_block);
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_block);
    ctx->current_block = merge_block;

    return BLOCK_NOT_TERMINATED;
}

/* 
 *
 * =============================
 *
 *        While Loops
 *
 * =============================
 * 
 */

static bool codegen_while_loop(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_WHILE_LOOP);

    // ==== blocks ====
    LLVMBasicBlockRef cond_block  = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "while.cond");
    LLVMBasicBlockRef body_block  = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "while.body");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "while.merge");

    // ==== cfg blocks ====
    LLVMBasicBlockRef previous_control_flow_condition_block = ctx->control_flow_condition;
    LLVMBasicBlockRef previous_control_flow_merge_block     = ctx->control_flow_merge;

    ctx->control_flow_condition = cond_block;
    ctx->control_flow_merge     = merge_block;

    LLVMBuildBr(ctx->llvm_builder, cond_block);

    // ==== condition ====
    const AstNodeId condition_id  = node->as.while_loop.condition;
    const AstNode *condition_node = &ctx->file->ast.nodes[condition_id];

    LLVMPositionBuilderAtEnd(ctx->llvm_builder, cond_block);
    ctx->current_block = cond_block;

    LLVMValueRef condition = codegen_expression(ctx, condition_node);

    if (!condition) {
        return false;
    }

    LLVMBuildCondBr(ctx->llvm_builder, condition, body_block, merge_block);

    // ==== body ====
    const AstNodeId body_id  = node->as.while_loop.body;
    const AstNode *body_node = &ctx->file->ast.nodes[body_id];

    LLVMPositionBuilderAtEnd(ctx->llvm_builder, body_block);
    ctx->current_block = body_block;

    BlockResult body_result = codegen_block(ctx, body_node);

    if (body_result == BLOCK_ERROR) {
        return false;
    }

    if (body_result == BLOCK_NOT_TERMINATED) {
        LLVMBuildBr(ctx->llvm_builder, cond_block);
    }

    // ==== merge ====
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_block);
    ctx->current_block = merge_block;

    ctx->control_flow_condition = previous_control_flow_condition_block;
    ctx->control_flow_merge     = previous_control_flow_merge_block;

    return true;
}
