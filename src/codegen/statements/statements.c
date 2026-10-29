#include "codegen/statements/statements.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "codegen/expressions/expressions.h"
#include "codegen/types.h"
#include "ids.h"
#include "utils/macros.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

// ====== Returns ======
static bool codegen_return_statement(CodegenCtx *ctx, const AstNode *node);

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

    for (u32 i = 0; i < node->as.block.statements.count; i += 1) {
        const AstNodeId statement_id  = node->as.block.statements.ids[i];
        const AstNode *statement_node = &ctx->file->ast.nodes[statement_id];

        switch (statement_node->kind) {
        case AST_BINARY_OP:
        case AST_FUNCTION_CALL: {
            if (!codegen_expression(ctx, statement_node)) {
                return BLOCK_ERROR;
            }

            break;
        }

        case AST_RETURN_STMT: {
            if (codegen_return_statement(ctx, statement_node)) {
                return BLOCK_TERMINATED;
            }

            return BLOCK_ERROR;
        }

        default:
            UNREACHABLE("Impossible statement %s", AST_KIND_STRINGS[statement_node->kind]);
        }
    }

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

    if (node->as.return_stmt.expr == AST_NODE_ID_NONE) {
        LLVMBuildRetVoid(ctx->llvm_builder);
        return true;
    }

    const AstNode *expression = &ctx->file->ast.nodes[node->as.return_stmt.expr];

    LLVMValueRef value = codegen_expression(ctx, expression);

    LLVMBuildRet(ctx->llvm_builder, value);

    return true;
}
