#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/statements/statements.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

#include <threads.h>

extern thread_local ThreadContext thread_context;

static AstNodeId parse_branch_body(Parser *p) {
    if (!parser_check(p, TOK_L_BRACE)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            thread_context.diagnostic_engine,
            p->file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected '{'",
            "add a '{' here"
        );

        return AST_NODE_ID_NONE;
    }

    AstNodeId block_id = parse_block(p);

    if (parser_is_node(p, block_id, AST_ERROR)) {
        return AST_NODE_ID_NONE;
    }

    return block_id;
}

static AstNodeId parse_branch(Parser *p) {
    AstNodeId condition_id = parse_expression(p);

    if (parser_is_node(p, condition_id, AST_ERROR)) {
        return AST_NODE_ID_NONE;
    }

    AstNodeId body_id = parse_branch_body(p);

    if (body_id == AST_NODE_ID_NONE) {
        return AST_NODE_ID_NONE;
    }

    AstNodeId branch_id  = parser_create_node(p);
    AstNode *branch_node = parser_get_node(p, branch_id);

    branch_node->kind                = AST_BRANCH;
    branch_node->as.branch.condition = condition_id;
    branch_node->as.branch.body      = body_id;

    return branch_id;
}

AstNodeId parse_if_statement(Parser *p) {
    AstNodeId id  = parser_create_node(p);
    AstNode *node = parser_get_node(p, id);

    node->kind                  = AST_IF_STMT;
    node->as.if_stmt.else_block = AST_NODE_ID_NONE;

    parser_init_id_list(p, &node->as.if_stmt.branches, 2);

    parser_advance(p); // advance past `if`

    // ==== if ====
    AstNodeId branch_id = parse_branch(p);

    if (branch_id == AST_NODE_ID_NONE) {
        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    node = parser_get_node(p, id);
    parser_append_id_list(p, &node->as.if_stmt.branches, branch_id);

    // ==== else if / else ====
    while (parser_check(p, TOK_KW_ELSE)) {
        parser_advance(p); // advance past `else`

        if (parser_check(p, TOK_KW_IF)) {
            parser_advance(p); // advance past `if`

            branch_id = parse_branch(p);

            if (branch_id == AST_NODE_ID_NONE) {
                return parser_error(p, id, RECOVERY_STATEMENT);
            }

            node = parser_get_node(p, id);
            parser_append_id_list(p, &node->as.if_stmt.branches, branch_id);

            continue;
        }

        // plain `else`, must be the last branch
        AstNodeId else_block_id = parse_branch_body(p);

        if (else_block_id == AST_NODE_ID_NONE) {
            return parser_error(p, id, RECOVERY_STATEMENT);
        }

        node                        = parser_get_node(p, id);
        node->as.if_stmt.else_block = else_block_id;

        break;
    }

    node             = parser_get_node(p, id);
    node->tokens.end = p->cursor;

    return id;
}
