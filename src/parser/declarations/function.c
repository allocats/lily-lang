#include "ast/ast.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/statements/statements.h"
#include "parser/type_expressions/type_expressions.h"
#include "string_interner/interner.h"
#include "tokens/kinds.h"

AstNodeId parse_function_declaration(Parser* p, AstNodeId id, StringId name) {
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_FUNCTION_DECL;

    node -> as.func_decl.name = name;
    node -> as.func_decl.return_type_expr = AST_NODE_ID_NONE;

    parser_init_id_list(p, &node -> as.func_decl.parameters, 4);

    AstFlag flags = AST_FLAG_TOP_DECL;

    while (!parser_check(p, TOK_KW_FN)) {
        Token token = parser_advance(p);

        AstFlag flag = AST_FLAG_NONE;

        switch (token.kind) {
            case TOK_KW_NOINLINE: {
                if (flags & AST_FLAG_INLINE) {
                    diagnostic_add_token(
                        p -> file,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add noinline modifier to function with inline modifier"
                    );

                    break;
                }

                flag = AST_FLAG_NOINLINE;
            } break;

            case TOK_KW_INLINE: {
                if (flags & AST_FLAG_NOINLINE) {
                    diagnostic_add_token(
                        p -> file,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add inline modifier to function with noinline modifier"
                    );

                    break;
                }

                flag = AST_FLAG_INLINE;
            } break;

            case TOK_KW_INTRINSIC: {
                if (flags & AST_FLAG_FOREIGN) {
                    diagnostic_add_token(
                        p -> file,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add intrinsic modifier to function with foreign modifier"
                    );

                    break;
                }

                flag = AST_FLAG_INTRINSIC;
            } break;

            case TOK_KW_FOREIGN: {
                if (flags & AST_FLAG_INTRINSIC) {
                    diagnostic_add_token(
                        p -> file,
                        DIAG_ERROR,
                        &token,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid function modifier",
                        "cannot add foreign modifier to function with intrinsic modifier"
                    );

                    break;
                }

                flag = AST_FLAG_FOREIGN;
            } break;

            default: {
                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "unexpected token",
                    "expected (noinline | inline | foreign | intrinsic | fn)"
                );

                flag = AST_FLAG_NONE;
            } break;
        }

        if (flag != AST_FLAG_NONE && flags & flag) {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "duplicated function attributes",
                "remove this modifier"
            );
        }

        flags |= flag;
    }

    node -> flags |= flags;

    parser_advance(p); // advance past `fn`

    if (!parser_check(p, TOK_L_PAREN)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_WHOLE_TOK,
            "",
            "" 
        );

        return parser_error(p, id, RECOVERY_DECLARATION);
    }

    parser_advance(p); // advance past `(`

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        AstNodeId parameter_id  = parser_create_node(p);
        AstNode* parameter_node = parser_get_node(p, parameter_id);

        parameter_node -> kind = AST_PARAMETER_DECL;

        Token parameter_name_tok = parser_advance(p); // advance past `identifier`

        if (parameter_name_tok.kind != TOK_IDENTIFIER) {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &parameter_name_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier for the parameter name"
            );

            return parser_error(p, id, RECOVERY_DECLARATION);
        }

        StringId parameter_name_id = string_intern_token(
            &p -> workspace -> string_interner,
            p -> file,
            parameter_name_tok
        );

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &parameter_name_tok,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error(p, id, RECOVERY_DECLARATION);
        }

        parser_advance(p); // advance past ':'

        AstNodeId parameter_type_expr_id = parse_parameter_type_expression(p);

        if (parser_is_node(p, parameter_type_expr_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_DECLARATION);
        }

        AstNode* parameter_type_expr_node = parser_get_node(p, parameter_type_expr_id);

        parameter_node = parser_get_node(p, parameter_id);

        parameter_node -> flags |= parameter_type_expr_node -> flags;
        parameter_node -> as.param_decl.name = parameter_name_id;
        parameter_node -> as.param_decl.type_expr = parameter_type_expr_id;

        if (parser_is_node(p, parameter_type_expr_id, AST_TYPE_VARIADIC)) {
            node = parser_get_node(p, id);

            node -> flags |= AST_FLAG_VARIADIC;

            if (parser_check(p, TOK_COMMA)) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "variadic parameter must be the last parameter",
                    "remove the parameters after '...'"
                );

                return parser_error(p, id, RECOVERY_DECLARATION);
            }

            if (!parser_check(p, TOK_R_PAREN)) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here"
                );

                return parser_error(p, id, RECOVERY_DECLARATION);
            }

            ast_append_id_list(
                &p -> file -> ast,
                &node -> as.func_decl.parameters,
                parameter_id
            );

            parser_advance(p);
            break;
        }

        ast_append_id_list(
            &p -> file -> ast,
            &node -> as.func_decl.parameters,
            parameter_id
        );

        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
            continue;
        }

        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ',' or ')'",
            "add a ',' or ')' here"
        );

        return parser_error(p, id, RECOVERY_DECLARATION);
    }

    if (parser_check(p, TOK_ARROW)) {
        parser_advance(p); // advance past '->'
        
        AstNodeId return_type_expression_id = parse_type_expression(p);

        if (parser_is_node(p, return_type_expression_id, AST_ERROR)) {
            return parser_error(p, id, RECOVERY_DECLARATION);
        }

        node = parser_get_node(p, id);

        node -> as.func_decl.return_type_expr = return_type_expression_id;
    }

    node = parser_get_node(p, id);

    if (node -> flags & AST_FLAG_INTRINSIC || node -> flags & AST_FLAG_FOREIGN) {
        if (!parser_check(p, TOK_SEMI)) {
            Token previous = parser_peek_behind_by(p, 1);

            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_WHOLE_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error(p, id, RECOVERY_DECLARATION);
        }

        node -> tokens.end = p -> cursor;

        parser_advance(p); // advance past ';'

        p -> file -> ast.total_declaration_count += 1;
        p -> file -> ast.top_level_declaration_count += 1;

        return id;
    }

    if (!parser_check(p, TOK_L_BRACE)) {
        Token token = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &token,
            DIAG_LOC_END_OF_TOK,
            "expected '{' for function body",
            "add a '{' here" 
        );

        return parser_error(p, id, RECOVERY_DECLARATION);
    }

    AstNodeId block_id = parse_block(p);

    if (parser_is_node(p, block_id, AST_ERROR)) {
        return parser_error(p, id, RECOVERY_DECLARATION);
    }

    node = parser_get_node(p, id);

    node -> as.func_decl.body = block_id;
    node -> tokens.end = p -> cursor;

    p -> file -> ast.total_declaration_count += node -> as.func_decl.parameters.count;

    return id;
}
