#include "ast/kinds.h"
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

extern thread_local ThreadContext thread_context;

AstNodeId parse_defer_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_DEFER_STMT;
    node -> as.defer_stmt.expr = AST_NODE_ID_NONE;

    parser_advance(p); // advance past `defer`

    AstNodeId expression_id = parse_expression(p);

    if (!parser_check(p, TOK_SEMI)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            thread_context.diagnostic_engine,
            p -> file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    node = parser_get_node(p, id);

    node -> tokens.end = p -> cursor;
    node -> as.defer_stmt.expr = expression_id;

    parser_advance(p);

    return id;
}
