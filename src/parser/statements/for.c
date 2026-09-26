#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/statements/statements.h"
#include "tokens/kinds.h"

AstNodeId parser_for_loop(Parser* p) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_FOR_LOOP;

    node -> as.for_loop.label = STRING_ID_NONE;
    node -> as.for_loop.body  = AST_NODE_ID_NONE;

    parser_advance(p); // advance past `for`

    // TODO: tmrw think about the differnet for loops and ponder changing expression parsing to include the := thing

    if (!parser_check(p, TOK_L_BRACE)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
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

    node -> tokens.end = p -> cursor - 1;
    node -> as.for_loop.body = block_id;

    return id;
}
