#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/statements/statements.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

#include <threads.h>

extern thread_local ThreadContext thread_context;

AstNodeId parser_for_loop(Parser* p) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_FOR_LOOP;

    node -> as.for_loop.label = LOCAL_STRING_ID_NONE;
    node -> as.for_loop.init  = AST_NODE_ID_NONE;
    node -> as.for_loop.cond  = AST_NODE_ID_NONE;
    node -> as.for_loop.step  = AST_NODE_ID_NONE;
    node -> as.for_loop.body  = AST_NODE_ID_NONE;

    parser_advance(p); // advance past `for`

    /* TODO (26/09): tmrw think about the differnet for loops and ponder changing 
     * expression parsing to include the := thing
     *
     * TODO: Okay it's the next day (27th), going to leave out range based for loops
     * for now, I need to still evaluate how we are going to do it, especially with the 
     * whole reference and type thing
    */ 

    AstNodeId initializer_id = parse_variable_declaration(p);

    if (parser_is_node(p, initializer_id, AST_ERROR)) {
        return parser_error(p, id, RECOVERY_STATEMENT);
    }

    AstNodeId condition_id = parse_expression(p);

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

    parser_advance(p); // advance past `;`

    AstNodeId step_id = parse_expression(p);
    
    if (!parser_check(p, TOK_L_BRACE)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            thread_context.diagnostic_engine,
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

    node -> as.for_loop.init = initializer_id;
    node -> as.for_loop.cond = condition_id;
    node -> as.for_loop.step = step_id;

    node -> as.for_loop.body = block_id;

    return id;
}
