#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "string_interner/interner.h"
#include "tokens/kinds.h"
#include "tokens/tokens.h"

/*
 *
 * all the parsing levels
 *
 */
static AstNodeId parse_assignment(Parser* p);
static AstNodeId parse_range(Parser* p);
static AstNodeId parse_logical_or(Parser* p);
static AstNodeId parse_logical_and(Parser* p);
static AstNodeId parse_bitwise_or(Parser* p);
static AstNodeId parse_bitwise_xor(Parser* p);
static AstNodeId parse_bitwise_and(Parser* p);
static AstNodeId parse_equality(Parser* p);
static AstNodeId parse_relational(Parser* p);
static AstNodeId parse_shift(Parser* p);
static AstNodeId parse_additive(Parser* p);
static AstNodeId parse_multiplicative(Parser* p);
static AstNodeId parse_unary(Parser* p);
static AstNodeId parse_postfix(Parser* p);
static AstNodeId parse_primary(Parser* p);

/*
 *
 * abstracted parsing functions
 *
 */

static AstNodeId parse_call(Parser* p, AstNodeId left);
static AstNodeId parse_index(Parser* p, AstNodeId left);
static AstNodeId parse_member_access(Parser* p, AstNodeId left);
static void      parse_call_args(Parser* p, AstNodeId id);

/*
 *
 * helpers
 *
 */

static AstNodeId make_binary_node(Parser* p, u32 starting_token_index, TokenKind op, AstNodeId lhs, AstNodeId rhs);
static bool      is_assignment_op(TokenKind kind);
static bool      is_unary_op(TokenKind kind);

/*
 *
 * expression parser entry point
 *
 */

inline AstNodeId parse_expression(Parser* p) {
    return parse_assignment(p);
}

/*
 *
 * entry point for type expressions! we need to skip the assignment level
 * no parser flag by doing it this way
 *
 */
inline AstNodeId parse_expression_for_type_expression(Parser* p) {
    return parse_range(p);
}

/*
 *
 * Assignment: Is right associative so it recurses into itself for the RHS
 *
 */
static AstNodeId parse_assignment(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_range(p);

    Token operator_tok = parser_peek(p);

    if (is_assignment_op(operator_tok.kind)) {
        parser_advance(p);

        AstNodeId rhs = parse_assignment(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Ranges
 *
 */
static AstNodeId parse_range(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_logical_or(p);

    while (parser_check(p, TOK_DOT_DOT)) {
        Token range_operator_tok = parser_advance(p);

        AstNodeId rhs = parse_logical_or(p);

        lhs = make_binary_node(p, starting_token_index, range_operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Logical OR the `||` token
 *
 */
static AstNodeId parse_logical_or(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_logical_and(p);

    while (parser_check(p, TOK_PIPE_PIPE)) {
        Token operator_tok = parser_advance(p);

        AstNodeId rhs = parse_logical_and(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Logical AND the `&&` token
 *
 */
static AstNodeId parse_logical_and(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_bitwise_or(p);

    while (parser_check(p, TOK_AMP_AMP)) {
        Token operator_tok = parser_advance(p);

        AstNodeId rhs = parse_bitwise_or(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Bitwise OR the `|` token
 *
 */
static AstNodeId parse_bitwise_or(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_bitwise_xor(p);

    while (parser_check(p, TOK_PIPE)) {
        Token operator_tok = parser_advance(p);

        AstNodeId rhs = parse_bitwise_xor(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Bitwise XOR the `^` token
 *
 */
static AstNodeId parse_bitwise_xor(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_bitwise_and(p);

    while (parser_check(p, TOK_CARET)) {
        Token operator_tok = parser_advance(p);

        AstNodeId rhs = parse_bitwise_and(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Bitwise AND the `&` token
 *
 */
static AstNodeId parse_bitwise_and(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_equality(p);

    while (parser_check(p, TOK_AMP)) {
        Token operator_tok = parser_advance(p);

        AstNodeId rhs = parse_equality(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);
    }

    return lhs;
}

/*
 *
 * Equality the `!=` and `==` tokens
 *
 */
static AstNodeId parse_equality(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_relational(p);

    Token operator_tok = parser_peek(p);

    while (operator_tok.kind == TOK_BANG_EQ || operator_tok.kind == TOK_EQ_EQ) {
        parser_advance(p);

        AstNodeId rhs = parse_relational(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);

        operator_tok = parser_peek(p);
    }

    return lhs;
}

/*
 *
 * Relational the `>=`, `>`, `<`, `<=` tokens
 *
 */
static AstNodeId parse_relational(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_shift(p);

    Token operator_tok = parser_peek(p);

    while (
        operator_tok.kind == TOK_LT_EQ || operator_tok.kind == TOK_LT ||
        operator_tok.kind == TOK_GT_EQ || operator_tok.kind ==  TOK_GT
    ) {
        parser_advance(p);

        AstNodeId rhs = parse_shift(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);

        operator_tok = parser_peek(p);
    }

    return lhs;
}

/*
 *
 * Bitshifting the `<<` and `>>` operators
 *
 */
static AstNodeId parse_shift(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_additive(p);

    Token operator_tok = parser_peek(p);

    while (operator_tok.kind == TOK_SHL || operator_tok.kind == TOK_SHR) {
        parser_advance(p);

        AstNodeId rhs = parse_additive(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);

        operator_tok = parser_peek(p);
    }

    return lhs;
}

/*
 *
 * Additive the `-` and `+` tokens
 *
 */
static AstNodeId parse_additive(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_multiplicative(p);

    Token operator_tok = parser_peek(p);

    while (operator_tok.kind == TOK_PLUS || operator_tok.kind == TOK_MINUS) {
        parser_advance(p);

        AstNodeId rhs = parse_multiplicative(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);

        operator_tok = parser_peek(p);
    }

    return lhs;
}

/*
 *
 * Multiplicative the `*`, `/` and the `%` tokkens
 *
 */
static AstNodeId parse_multiplicative(Parser* p) {
    u32 starting_token_index = p -> cursor;

    AstNodeId lhs = parse_unary(p);

    Token operator_tok = parser_peek(p);

    while (operator_tok.kind == TOK_STAR || operator_tok.kind == TOK_SLASH || operator_tok.kind == TOK_PERCENT) {
        parser_advance(p);

        AstNodeId rhs = parse_unary(p);

        lhs = make_binary_node(p, starting_token_index, operator_tok.kind, lhs, rhs);

        operator_tok = parser_peek(p);
    }

    return lhs;
}

/*
 *
 * Unary operator, right associative
 *
 */
static AstNodeId parse_unary(Parser* p) {
    Token operator_tok = parser_peek(p);

    if (is_unary_op(operator_tok.kind)) {
        u32 starting_token_index = p -> cursor;

        parser_advance(p); // advance past `operator`

        AstNodeId operand_id = parse_unary(p);

        AstNodeId id  = parser_create_node(p);
        AstNode* node = parser_get_node(p, id);

        node -> kind = AST_UNARY_OP;

        node -> as.unary_op.op = operator_tok.kind;
        node -> as.unary_op.operand = operand_id;

        node -> tokens.start = starting_token_index;
        node -> tokens.end = p -> cursor - 1;

        return id;
    }

    return parse_postfix(p);
}

static AstNodeId parse_postfix(Parser* p) {
    AstNodeId lhs = parse_primary(p);

    while (true) {
        Token operator_tok = parser_peek(p);

        if (operator_tok.kind == TOK_L_PAREN) {
            lhs = parse_call(p, lhs);
        } else if (operator_tok.kind == TOK_L_BRACKET) {
            lhs = parse_index(p, lhs);
        } else if (operator_tok.kind == TOK_DOT) {
            lhs = parse_member_access(p, lhs);
        } else {
            break;
        }
    }

    return lhs;
}

static AstNodeId parse_primary(Parser* p) {
    Token token = parser_advance(p);

    switch (token.kind) {
        case TOK_INTEGER_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_INTEGER;
            node -> as.literal.as.integer = get_integer_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_FLOAT_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_FLOATING;
            node -> as.literal.as.floating = get_floating_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_CHAR_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_CHAR;
            node -> as.literal.as.character = get_character_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_STRING_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_STRING;
            node -> as.literal.as.string = string_intern_token(&p -> workspace -> string_interner, p -> file, token);

            return id;
        }

        case TOK_KW_TRUE:
        case TOK_KW_FALSE: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_BOOL;
            node -> as.literal.as.boolean = token.kind == TOK_KW_TRUE;

            return id;
        }

        case TOK_KW_NULL: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.literal.kind = LITERAL_NULL;

            return id;
        }

        case TOK_IDENTIFIER: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_IDENTIFIER;
            node -> tokens.start -= 1;
            node -> tokens.end = node -> tokens.start;

            node -> as.identifier.name = string_intern_token(&p -> workspace -> string_interner, p -> file, token);

            return id;
        }

        case TOK_L_PAREN: {
            AstNodeId id = parse_expression(p);

            if (!parser_check(p, TOK_R_PAREN)) {
                Token previous = parser_peek_behind_by(p, 1);

                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &previous,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here"
                );

                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            parser_advance(p);

            return id;
        }

        case TOK_HASHTAG: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_EXECUTE_DIRECTIVE;
            node -> tokens.start -= 1;

            Token directive_tok = parser_advance(p);

            if (!string_lookup_token(&p -> workspace -> string_interner, p -> file, directive_tok)) {
                diagnostic_add_token(
                    p -> file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid directive in expression",
                    "only the `#execute` directive is allowed in expressions"
                );

                return parser_error(p, id, RECOVERY_EXPRESSION);
            }

            AstNodeId expr_id = parse_unary(p);

            node = parser_get_node(p, id);

            node -> as.exec_directive.expr = expr_id;

            node -> tokens.end = p -> cursor - 1;

            return id;
        }

        default: {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "invalid start to expression",
                null
            );

            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> tokens.start = p -> cursor - 1;

            return parser_error(p, id, RECOVERY_EXPRESSION);
        }
    }
}

/*
 *
 * abstracted parsing functions
 *
 */

static AstNodeId parse_call(Parser* p, AstNodeId left) {
    parser_advance(p);

    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_FUNCTION_CALL;
    node -> tokens.start -= 2;

    node -> as.func_call.identifier = left;

    parser_init_id_list(p, &node -> as.func_call.arguments, 4);

    parse_call_args(p, id);

    if (!parser_check(p, TOK_R_PAREN)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ')'",
            "add a ')' here"
        );

        return parser_error(p, id, RECOVERY_EXPRESSION);
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p);

    return id;
}

static AstNodeId parse_index(Parser* p, AstNodeId left) {
    parser_advance(p); // advance past `[`

    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_INDEX;
    node -> tokens.start -= 2;

    node -> as.index.identifier = left;

    AstNodeId index_expr_id = parse_expression(p);

    node = parser_get_node(p, id);
    node -> as.index.index_expr = index_expr_id;

    if (!parser_check(p, TOK_R_BRACKET)) {
        Token previous = parser_peek_behind_by(p, 1);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ']'",
            "add a ']' here"
        );

        return parser_error(p, id, RECOVERY_EXPRESSION);
    }

    node -> tokens.end = p -> cursor;

    parser_advance(p);

    return id;
}

static AstNodeId parse_member_access(Parser* p, AstNodeId left) {
    parser_advance(p); // advance past `.`

    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_MEMBER_ACCESS;
    node -> tokens.start -= 2;

    if (!parser_check(p, TOK_IDENTIFIER)) {
        Token bad_token = parser_peek(p);

        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &bad_token,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier",
            "member access requires an identifier"
        );

        return parser_error(p, id, RECOVERY_EXPRESSION);
    }

    Token member_token = parser_advance(p);

    AstNodeId member_id  = parser_create_node(p);
    AstNode* member_node = parser_get_node(p, member_id);

    member_node -> kind = AST_IDENTIFIER;
    member_node -> as.identifier.name = string_intern_token(
        &p -> workspace -> string_interner,
        p -> file,
        member_token
    );

    node = parser_get_node(p, id);

    node -> as.member_access.object = left;
    node -> as.member_access.member = member_id;

    node -> tokens.end = p -> cursor - 1;

    return id;
}

static void parse_call_args(Parser* p, AstNodeId id) {
    AstNode* call = parser_get_node(p, id);

    if (parser_check(p, TOK_R_PAREN)) {
        return;
    }

    do {
        AstNodeId arg_id = parse_expression(p);

        call = parser_get_node(p, id);

        parser_append_id_list(p, &call -> as.func_call.arguments, arg_id);

        if (!parser_check(p, TOK_COMMA)) {
            break;
        }

        parser_advance(p);
    } while (p -> cursor < p -> token_count);
}

/*
 *
 * helpers
 *
 */

static AstNodeId make_binary_node(Parser* p, u32 starting_token_index, TokenKind op, AstNodeId lhs, AstNodeId rhs) {
    AstNodeId id  = parser_create_node(p);
    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_BINARY_OP;

    node -> as.binary_op.op = op;
    node -> as.binary_op.lhs = lhs;
    node -> as.binary_op.rhs = rhs;

    node -> tokens.start = starting_token_index;
    node -> tokens.end = p -> cursor - 1;

    return id;
}

static bool is_assignment_op(TokenKind kind) {
    switch (kind) {
        case TOK_EQ:
        case TOK_PLUS_EQ:
        case TOK_MINUS_EQ:
        case TOK_STAR_EQ:
        case TOK_SLASH_EQ:
        case TOK_PERCENT_EQ:
        case TOK_SHL_EQ:
        case TOK_SHR_EQ:
        case TOK_AMP_EQ:
        case TOK_PIPE_EQ:
        case TOK_CARET_EQ:
            return true;

        default:
            return false;
    }
}

static bool is_unary_op(TokenKind kind) {
    switch (kind) {
        case TOK_AMP:
        case TOK_STAR:
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
            return true;

        default:
            return false;
    }
}
