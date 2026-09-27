#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/statements/statements.h"

AstNodeId parser_break_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_BREAK_STMT;

    node -> as.break_stmt.label = STRING_ID_NONE;

    Token break_tok = parser_advance(p); // advance past `break`

    if (!p -> in_loop_context) {
        diagnostic_add_token(
            p -> file, 
            DIAG_ERROR,
            &break_tok,
            DIAG_LOC_WHOLE_TOK,
            "'break' found outside of a loop",
            "'break' can only appear inside of loops"
        );
    }

    /*
     *
     * label logic here if we get there
     *
     */

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &break_tok,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past `;`

    return id;
}
