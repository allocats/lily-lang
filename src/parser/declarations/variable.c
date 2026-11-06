#include "ast/kinds.h"
#include "ast/types.h"
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

extern thread_local ThreadContext thread_context;

AstNodeId parse_variable_declaration(Parser *p) {
    AstNodeId id  = parser_create_node(p);
    AstNode *node = parser_get_node(p, id);

    node->kind = AST_VARIABLE_DECL;

    /*
     *
     * initiate the node's values, no unitialised bs please
     *
     */
    node->as.variable_decl.name      = LOCAL_STRING_ID_NONE;
    node->as.variable_decl.type_expr = AST_NODE_ID_NONE;
    node->as.variable_decl.init_expr = AST_NODE_ID_NONE;

    Token name_token = parser_peek(p);

    node->as.variable_decl.name = local_string_intern_token(thread_context.string_interner, p->file, name_token);

    /*
     * in theory should not have to check this because it's only possible to be 
     * inside this function if these two conditions were met
     */

    parser_advance(p); // advance past `identifier`

    if (parser_check(p, TOK_COLON_COLON)) {
        node->flags |= AST_FLAG_CONSTANT;

        parser_advance(p); // advance past `::`

        AstNodeId initializer_id = parse_expression(p);

        if (parser_is_node(p, initializer_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_NONE);
        }

        node = parser_get_node(p, id);

        node->as.variable_decl.init_expr = initializer_id;

        if (!parser_check(p, TOK_SEMI)) {
            Token previous = parser_peek_behind_by(p, 1);

            diagnostic_add_token(
                thread_context.diagnostic_engine,
                p->file,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error(p, id, RECOVERY_NONE);
        }

        node->tokens.end = p->cursor;

        parser_advance(p); // advance past ';'

        p->file->ast.total_declaration_count += 1;

        return id;
    } else {
        parser_advance(p); // advance past `:`
    }

    /*
     *
     * Type, only parse type expr if it exists
     *
     */
    if (!parser_check(p, TOK_EQ)) {
        AstNodeId type_expression_id  = parse_type_expression(p);
        AstNode *type_expression_node = parser_get_node(p, type_expression_id);

        if (type_expression_node->kind == AST_ERROR) {
            return parser_error(p, id, RECOVERY_NONE);
        }

        node = parser_get_node(p, id);

        node->as.variable_decl.type_expr = type_expression_id;
        node->flags |= type_expression_node->flags;
    }

    if (parser_check(p, TOK_EQ)) {
        parser_advance(p);

        AstNodeId initializer_id = parse_expression(p);

        if (parser_is_node(p, initializer_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_NONE);
        }

        node = parser_get_node(p, id);

        node->as.variable_decl.init_expr = initializer_id;
    }

    if (!parser_check(p, TOK_SEMI)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            thread_context.diagnostic_engine,
            p->file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    node->tokens.end = p->cursor;

    parser_advance(p); // advance past ';'

    p->file->ast.total_declaration_count += 1;

    return id;
}
