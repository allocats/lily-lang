#include "parser/declarations/declarations.h"
#include "diagnostics/diagnostics.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "string_interner/local/interner.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

extern thread_local ThreadContext thread_context;

AstNodeId parse_top_level_declaration(Parser *p, AstNodeId id) {
    Token identifier = parser_peek(p);

    StringId name_id = local_string_intern_token(thread_context.string_interner, p->file, identifier);

    parser_advance(p); // advance past 'identifier'
    parser_advance(p); // advance past '::'

    Token token = parser_peek(p);

    switch (token.kind) {
    case TOK_KW_INTRINSIC:
    case TOK_KW_FOREIGN:
    case TOK_KW_NOINLINE:
    case TOK_KW_INLINE:
    case TOK_KW_FN:
        return parse_function_declaration(p, id, name_id);

    case TOK_KW_STRUCT:
        return parse_struct_declaration(p, id, name_id);

    case TOK_KW_UNION:
        return parse_union_declaration(p, id, name_id);

    case TOK_KW_ENUM:
        return parse_enum_declaration(p, id, name_id);

    case TOK_HASHTAG:
        return parse_directive_declaration(p, id, name_id);

    default: {
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

        node->as.variable_decl.name = name_id;

        node->flags |= AST_FLAG_CONSTANT;

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

        // diagnostic_add_token(
        //     thread_context.diagnostic_engine,
        //     p->file,
        //     DIAG_ERROR,
        //     &token,
        //     DIAG_LOC_WHOLE_TOK,
        //     "unexpected token",
        //     "expected (fn | enum | struct | union | inline | noinline | foreign | intrinsic | identifier)"
        // );
        //
        // return parser_error(p, id, RECOVERY_DECLARATION);
    } break;
    }
}
