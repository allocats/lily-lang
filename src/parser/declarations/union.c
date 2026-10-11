#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/type_expressions/type_expressions.h"
#include "string_interner/local/interner.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

extern thread_local ThreadContext thread_context;

AstNodeId parse_union_declaration(Parser* p, AstNodeId id, StringId name) {
    parser_advance(p); // advance past `union`

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_UNION_DECL;
    node -> as.union_decl.name = name;
    node -> as.union_decl.alignment_directive = AST_NODE_ID_NONE;

    parser_init_id_list(p, &node -> as.union_decl.fields, 8);

    if (parser_check(p, TOK_HASHTAG)) {
        AstNodeId attribute_id  = parse_directive(p);
        AstNode* attribute_node = parser_get_node(p, attribute_id); 

        node = parser_get_node(p, id);

        switch (attribute_node -> kind) {
            case AST_PACKED_DIRECTIVE: {
                node -> flags |= AST_FLAG_PACKED;
            } break;

            case AST_ALIGN_DIRECTIVE: {
                node -> as.union_decl.alignment_directive = attribute_id;
            } break;

            default: {
                diagnostic_add_token_span(
                    thread_context.diagnostic_engine,
                    p -> file,
                    DIAG_ERROR,
                    attribute_node -> tokens,
                    "invalid union attribute",
                    "only '#packed' and '#align N' is allowed as union attributes"
                );
            } break;
        }
    }

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

        return parser_error(p, id, RECOVERY_NONE);
    }

    parser_advance(p); // advance past `{`

    while (!parser_check(p, TOK_R_BRACE)) {
        u32 field_starting_token_index = p -> cursor;

        Token field_name_tok = parser_advance(p);

        if (field_name_tok.kind != TOK_IDENTIFIER) {
            diagnostic_add_token(
                thread_context.diagnostic_engine,
                p -> file,
                DIAG_ERROR,
                &field_name_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier for field name",
                "add a valid identifier here" 
            );

            return parser_error(p, id, RECOVERY_NONE);
        }

        StringId field_name_id = local_string_intern_token(thread_context.string_interner, p -> file, field_name_tok);

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                thread_context.diagnostic_engine,
                p -> file,
                DIAG_ERROR,
                &field_name_tok,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error(p, id, RECOVERY_NONE);
        }

        parser_advance(p);

        AstNodeId field_type_expression_id = parse_type_expression(p); 

        if (parser_is_node(p, field_type_expression_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_NONE);
        }

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

            return parser_error(p, id, RECOVERY_NONE);
        }

        AstNodeId field_decl_id  = parser_create_node(p);
        AstNode* field_decl_node = parser_get_node(p, field_decl_id);

        field_decl_node -> kind = AST_FIELD_DECL;

        field_decl_node -> as.field_decl.name = field_name_id;
        field_decl_node -> as.field_decl.type_expr = field_type_expression_id; 

        field_decl_node -> tokens.start = field_starting_token_index;
        field_decl_node -> tokens.end   = p -> cursor;

        parser_advance(p);

        node = parser_get_node(p, id);

        parser_append_id_list(p, &node -> as.union_decl.fields, field_decl_id);
    }

    node = parser_get_node(p, id);

    node -> tokens.end = p -> cursor;

    parser_advance(p);

    p -> file -> ast.total_declaration_count += node -> as.union_decl.fields.count;

    return id;
}
