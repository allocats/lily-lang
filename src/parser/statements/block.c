#include "ast/kinds.h"
#include "ast/types.h"
#include "ids.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/statements/statements.h"
#include "tokens/kinds.h"

#include <assert.h>

AstNodeId parse_block(Parser* p) {
    parser_advance(p); // advance past `{`

    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_BLOCK;

    parser_init_id_list(p, &node -> as.block.statements, 16);

    while (true) {
        if (parser_check(p, TOK_R_BRACE)) {
            break;
        }

        AstNodeId statement = parse_statement(p);

        assert(statement != AST_NODE_ID_NONE);

        node = parser_get_node(p, id);

        if (parser_is_node(p, statement, AST_ERROR)) {
            parser_recover(p, RECOVERY_STATEMENT); 

            if (parser_check(p, TOK_SEMI)) {
                parser_advance(p);
            }
        } else {
            parser_append_id_list(p, &node -> as.block.statements, statement);
        }
    }

    node = parser_get_node(p, id);

    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past `}`

    return id;
}
