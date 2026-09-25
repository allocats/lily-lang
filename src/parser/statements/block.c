#include "ast/kinds.h"
#include "ast/types.h"
#include "parser/parser.h"
#include "parser/statements/statements.h"

AstNodeId parse_block(Parser* p) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_BLOCK;

    return id;
}
