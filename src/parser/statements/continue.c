#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/statements/statements.h"
#include "threadpool/types.h"

#include <pthread.h>

extern thread_local ThreadContext thread_context;

AstNodeId parser_continue_statement(Parser *p) {
    AstNodeId id  = parser_create_node(p);
    AstNode *node = parser_get_node(p, id);

    node->kind = AST_CONTINUE_STMT;

    node->as.continue_stmt.label = LOCAL_STRING_ID_NONE;

    Token continue_tok = parser_advance(p); // advance past `continue`

    if (!p->in_loop_context) {
        diagnostic_add_token(
            thread_context.diagnostic_engine,
            p->file,
            DIAG_ERROR,
            &continue_tok,
            DIAG_LOC_WHOLE_TOK,
            "'continue' found outside of a loop",
            "'continue' can only appear inside of loops"
        );
    }

    /*
     *
     * label logic here if we get there
     *
     */

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            thread_context.diagnostic_engine,
            p->file,
            DIAG_ERROR,
            &continue_tok,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    node->tokens.end = p->cursor;

    parser_advance(p); // advance past `;`

    return id;
}
