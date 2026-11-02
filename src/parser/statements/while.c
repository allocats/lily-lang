#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/statements/statements.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

#include <pthread.h>

extern thread_local ThreadContext thread_context;

AstNodeId parse_while_loop(Parser *p) {
    AstNodeId id  = parser_create_node(p);
    AstNode *node = parser_get_node(p, id);

    node->kind = AST_WHILE_LOOP;

    node->as.while_loop.body      = AST_NODE_ID_NONE;
    node->as.while_loop.condition = AST_NODE_ID_NONE;

    parser_advance(p); // advance past `while`

    AstNodeId condition_id = parse_expression(p);

    if (parser_is_node(p, condition_id, AST_ERROR)) {
        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    node                          = parser_get_node(p, id);
    node->as.while_loop.condition = condition_id;

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

        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    AstNodeId block_id = parse_block(p);

    node = parser_get_node(p, id);

    node->tokens.end         = p->cursor - 1;
    node->as.while_loop.body = block_id;

    return id;
}
