#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/type_expressions/type_expressions.h"
#include "parser/recovery/types.h"
#include "tokens/kinds.h"

#include <threads.h>

/*
 *
 * Type modifiers! pointers, and arrays, create a stack essentially
 *
 */

typedef enum {
    TYPE_MOD_POINTER,
    TYPE_MOD_ARRAY
} TypeModifierKind;

typedef struct {
    TypeModifierKind kind;
    AstNodeId size_expr;
} TypeModifier;

static constexpr u32 type_modifier_max = 64;

AstNodeId parse_type_expression(Parser* p) {
    u32 starting_token_index = p -> cursor;
    
    AstFlag flags = AST_FLAG_NONE;

    if (parser_check(p, TOK_KW_CONST)) {
        flags |= AST_FLAG_CONSTANT;

        parser_advance(p);
    }

    TypeModifier modifiers[type_modifier_max];
    u32 modifier_count = 0;

    while (p -> cursor < p -> token_count) {
        Token token = parser_peek(p);
        
        if (token.kind == TOK_STAR) {
            if (modifier_count >= type_modifier_max) {
                Token token = parser_peek(p);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_START_OF_TOK,
                    "too many type modifiers",
                    "type expression has too many pointer/array modifiers"
                );

                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            modifiers[modifier_count++] = (TypeModifier) {
                .kind = TYPE_MOD_POINTER,
                .size_expr = AST_NODE_ID_NONE
            };

            parser_advance(p);

            continue;
        }

        if (token.kind == TOK_L_BRACKET) {
            parser_advance(p);

            if (parser_check(p, TOK_R_BRACKET)) {
                modifiers[modifier_count++] = (TypeModifier) {
                    .kind = TYPE_MOD_ARRAY,
                    .size_expr = AST_NODE_ID_NONE
                };
                
                parser_advance(p);

                continue;
            }

            AstNodeId size_expr = parse_expression(p);

            if (!parser_check(p, TOK_R_BRACKET)) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ']'",
                    "add a ']' here"
                );

                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            parser_advance(p);

            if (modifier_count >= type_modifier_max) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "too many type modifiers",
                    "type expression has too many pointer/array modifiers"
                );

                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            modifiers[modifier_count++] = (TypeModifier) {
                .kind = TYPE_MOD_ARRAY,
                .size_expr = size_expr
            };

            continue;
        }

        break;
    }

    AstNodeId base_id = AST_NODE_ID_NONE;

    // function type
    if (parser_check(p, TOK_KW_FN)) {
        u32 fn_start = p -> cursor;

        parser_advance(p);

        if (!parser_check(p, TOK_L_PAREN)) {
            Token token = parser_peek_behind_by(p, 1);

            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &token,
                DIAG_LOC_END_OF_TOK,
                "expected '(' after 'fn'",
                "function type parameters start with '('"
            );

            AstNodeId id = parser_create_node(p);
            return parser_error(p, id, RECOVERY_EXPRESSION);
        }

        parser_advance(p);

        AstNodeId fn_id  = parser_create_node(p);
        AstNode* fn_node = parser_get_node(p, fn_id);

        fn_node -> kind = AST_TYPE_FUNCTION;
        fn_node -> tokens.start = fn_start;

        fn_node -> as.function_type.return_type = AST_NODE_ID_NONE;
        
        parser_init_id_list(p, &fn_node -> as.function_type.parameters, 4);

        while (!parser_check(p, TOK_R_PAREN)) {
            if (p -> cursor >= p -> token_count) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add ')' to close the function type"
                );

                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            AstNodeId param_type_id = parse_parameter_type_expression(p);

            if (parser_is_node(p, param_type_id, AST_ERROR)) {
                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            parser_append_id_list(p, &fn_node -> as.function_type.parameters, param_type_id);

            if (parser_check(p, TOK_COMMA)) {
                parser_advance(p);
                continue;
            }

            if (!parser_check(p, TOK_R_PAREN)) {
                Token token = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ',' or ')'",
                    "separate function parameter types with commas"
                );

                AstNodeId id = parser_create_node(p);
                return parser_error(p, id, RECOVERY_EXPRESSION);
            }
        }

        parser_advance(p);

        // return type, mandatory

        if (!parser_check(p, TOK_ARROW)) {
            Token token = parser_peek_behind_by(p, 1);

            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &token,
                DIAG_LOC_END_OF_TOK,
                "expected ' -> '",
                "function types must specify a return type"
            );

            AstNodeId id = parser_create_node(p);
            return parser_error(p, id, RECOVERY_EXPRESSION);
        }

        parser_advance(p);

        AstNodeId return_type_id = parse_type_expression(p);

        if (parser_is_node(p, return_type_id, AST_ERROR)) {
            AstNodeId id = parser_create_node(p);
            return parser_error(p, id, RECOVERY_EXPRESSION);
        }

        fn_node = parser_get_node(p, fn_id);

        fn_node -> as.function_type.return_type = return_type_id;
        fn_node -> tokens.end = p -> cursor - 1;

        base_id = fn_id;
    } else { 

        // regular type

        AstNodeId primary_id = parse_expression_for_type_expression(p);

        if (parser_is_node(p, primary_id, AST_ERROR)) {
            AstNodeId id = parser_create_node(p);
            return parser_error(p, id, RECOVERY_EXPRESSION);
        }

        AstNode* primary_node = parser_get_node(p, primary_id);

        if (
            primary_node -> kind != AST_IDENTIFIER      &&
            primary_node -> kind != AST_FUNCTION_CALL   &&
            primary_node -> kind != AST_MEMBER_ACCESS
        ) {
            diagnostic_add_token_span(
                p -> file,
                DIAG_ERROR,
                primary_node -> tokens,
                "expected type name, member access, function type, or function call",
                "type must be an identifier, member access, function type, or function call"
            );

            AstNodeId id = parser_create_node(p);
            return parser_error(p, id, RECOVERY_EXPRESSION);
        }


        AstNodeId type_id  = parser_create_node(p);
        AstNode* type_node = parser_get_node(p, type_id);

        type_node -> kind = AST_TYPE_BASE;

        type_node -> as.base_type.expr = primary_id;

        base_id = type_id;
    }

    // create the final type with modifiers applied!
    AstNodeId id = base_id;

    assert(modifier_count < U8_MAX);

    while (modifier_count > 0) {
        TypeModifier modifier = modifiers[--modifier_count];

        if (modifier.kind == TYPE_MOD_POINTER) {
            AstNodeId ptr_id  = parser_create_node(p);
            AstNode* ptr_node = parser_get_node(p, ptr_id);

            ptr_node -> kind = AST_TYPE_POINTER;

            ptr_node -> as.pointer_type.base_type = id;

            id = ptr_id;
        } else if (modifier.kind == TYPE_MOD_ARRAY) {
            AstNodeId array_id  = parser_create_node(p);
            AstNode* array_node = parser_get_node(p, array_id);

            array_node -> kind = AST_TYPE_ARRAY;

            array_node -> as.array_type.element = id;
            array_node -> as.array_type.size_expr = modifier.size_expr;

            id = array_id;
        }
    }

    AstNode* node = parser_get_node(p, id);

    node -> flags = flags;

    node -> tokens.start = starting_token_index;
    node -> tokens.end = p -> cursor - 1;

    return id;
}

AstNodeId parse_parameter_type_expression(Parser* p) {
    if (parser_check(p, TOK_ELLIPSIS)) {
        parser_advance(p);

        AstNodeId id  = parser_create_node(p);
        AstNode* node = parser_get_node(p, id);

        node -> kind = AST_TYPE_VARIADIC;
        node -> flags |= AST_FLAG_VARIADIC;
        node -> as.variadic_type.element_type = AST_NODE_ID_NONE;

        if (!parser_check(p, TOK_R_PAREN)) {
            AstNodeId element_id = parse_type_expression(p);

            node = parser_get_node(p, id);
            node -> as.variadic_type.element_type = element_id;
        }

        return id;
    }

    return parse_type_expression(p);
}
