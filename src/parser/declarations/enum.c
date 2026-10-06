#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/type_expressions/type_expressions.h"
#include "string_interner/local/interner.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"
#include <threads.h>

extern thread_local ThreadContext thread_context;

AstNodeId parse_enum_declaration(Parser* p, AstNodeId id, StringId name) {
    parser_advance(p); // advance past `enum`

    AstNode* node  = parser_get_node(p, id);

    node -> kind = AST_ENUM_DECL;

    node -> as.enum_decl.name = name;
    node -> as.enum_decl.type_expr = AST_NODE_ID_NONE;

    parser_init_id_list(p, &node -> as.enum_decl.variants, 4);

    if (parser_check(p, TOK_COLON)) {
        parser_advance(p); // advance past `:`

        AstNodeId type_expression_id = parse_type_expression(p);

        if (parser_is_node(p, type_expression_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_NONE);
        }

        node = parser_get_node(p, id);
        node -> as.enum_decl.type_expr = type_expression_id;
    }

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

        return parser_error(p, id, RECOVERY_NONE);
    }

    parser_advance(p); // advance past `{`

    while (!parser_check(p, TOK_R_BRACE)) {
        u32 starting_token_index = p -> cursor;

        AstNodeId variant_value_expression_id = AST_NODE_ID_NONE;

        Token variant_name_tok = parser_advance(p);

        if (variant_name_tok.kind != TOK_IDENTIFIER) {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &variant_name_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected an identifier for enum variant name",
                "add a valid identifier here"
            );

            return parser_error(p, id, RECOVERY_NONE);
        }

        if (parser_check(p, TOK_EQ)) {
            parser_advance(p); // advance past `=`

            variant_value_expression_id = parse_expression(p);
        }

        if (!parser_check(p, TOK_SEMI)) {
            Token previous = parser_peek_behind_by(p, 1);

            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error(p, id, RECOVERY_NONE);
        }

        StringId variant_name_id = local_string_intern_token(thread_context.string_interner, p -> file, variant_name_tok);

        AstNodeId variant_id  = parser_create_node(p);
        AstNode* variant_node = parser_get_node(p, variant_id);

        variant_node -> kind = AST_VARIANT_DECL;

        variant_node -> tokens.start = starting_token_index;
        variant_node -> tokens.end = p -> cursor;

        variant_node -> as.variant_decl.name = variant_name_id;
        variant_node -> as.variant_decl.value_expr = variant_value_expression_id;

        node = parser_get_node(p, id);

        parser_append_id_list(p, &node -> as.enum_decl.variants, variant_id);
        
        /*
         *
         * advance past `;` and restart the loop
         *
         */
        parser_advance(p);
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past `{`

    p -> file -> ast.total_declaration_count += node -> as.enum_decl.variants.count;

    return id;
}
