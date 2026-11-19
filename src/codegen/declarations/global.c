#include "ast/kinds.h"
#include "codegen/declarations/declarations.h"
#include "codegen/expressions/expressions.h"
#include "codegen/types/types.h"
#include "ids.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

bool codegen_global_variable(CodegenCtx *ctx, const AstNode *node) {
    assert(node->kind == AST_VARIABLE_DECL);

    LLVMTypeRef type = type_to_llvm_type(node->resolved_type_id);

    LLVMValueRef value = null;

    if (node->as.variable_decl.init_expr != AST_NODE_ID_NONE) {
        const AstNodeId value_id  = node->as.variable_decl.init_expr;
        const AstNode *value_node = &ctx->file->ast.nodes[value_id];

        value = codegen_expression(ctx, value_node);

        if (!value) {
            return false;
        }
    }

    LLVMValueRef variable = LLVMAddGlobal(ctx->llvm_module, type, "");

    LLVMSetInitializer(variable, value);

    if (node->flags & AST_FLAG_FOREIGN) {
        LLVMSetLinkage(variable, LLVMExternalLinkage);
    }

    ctx->symbol_map[node->resolved_symbol_id] = variable;

    return true;
}
