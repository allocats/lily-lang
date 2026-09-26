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

// helper: L(n) = left associativity at level n, R(n) = right associativity at level n
#define L(n) { (n), (n) - 1 }
#define R(n) { (n), (n) }

typedef struct {
    u8 lbp;
    u8 rbp;
} OpInfo;

static thread_local OpInfo op_info_table[] = {
    // Assignment
    [TOK_EQ]            = R(10),
    [TOK_PLUS_EQ]       = R(10),
    [TOK_MINUS_EQ]      = R(10),
    [TOK_STAR_EQ]       = R(10),
    [TOK_SLASH_EQ]      = R(10),
    [TOK_PERCENT_EQ]    = R(10),
    [TOK_SHL_EQ]        = R(10),
    [TOK_SHR_EQ]        = R(10),
    [TOK_AMP_EQ]        = R(10),
    [TOK_PIPE_EQ]       = R(10),
    [TOK_CARET_EQ]      = R(10),

    // Ranges
    [TOK_DOT_DOT]       = L(20),

    // Logical OR 
    [TOK_PIPE_PIPE]     = L(30),

    // Logical AND
    [TOK_AMP_AMP]       = L(40),

    // Bitwise OR 
    [TOK_PIPE]          = L(50),

    // Bitwise XOR 
    [TOK_CARET]         = L(60),

    // Bitwise AND
    [TOK_AMP]           = L(70),

    // Equality
    [TOK_BANG_EQ]       = L(80),
    [TOK_EQ_EQ]         = L(80),

    // Relational 
    [TOK_LT]            = L(90),
    [TOK_LT_EQ]         = L(90),
    [TOK_GT]            = L(90),
    [TOK_GT_EQ]         = L(90),

    // Shift
    [TOK_SHL]           = L(100),
    [TOK_SHR]           = L(100),

    // Additive 
    [TOK_PLUS]          = L(110),
    [TOK_MINUS]         = L(110),

    // Multiplicative 
    [TOK_STAR]          = L(120),
    [TOK_SLASH]         = L(120),
    [TOK_PERCENT]       = L(120),

    // Postfix
    [TOK_L_PAREN]       = L(130),
    [TOK_L_BRACKET]     = L(130),
    [TOK_ARROW]         = L(130),
    [TOK_DOT]           = L(130),
};

static constexpr u32 op_table_length = (sizeof(op_info_table) / sizeof(op_info_table[0]));

static AstNodeId nud(Parser* p, Token token);
static AstNodeId led(Parser* p, Token token, AstNodeId left);

static void parse_call_args(Parser* p, AstNodeId id);

static u8 lbp_of(TokenKind kind);

/*
 *
 * entry point expression parsing function for pratt parser
 *
 */

AstNodeId parse_expression(Parser* p, u32 prec) {
    u32 starting_token_index = p -> cursor;

    Token token = parser_advance(p);

    AstNodeId left = nud(p, token);

    while (lbp_of(parser_peek(p).kind) > prec) {
        Token op = parser_advance(p);

        left = led(p, op, left);
    }

    u32 end_token_index = p -> cursor - 1;

    AstNode* node = parser_get_node(p, left);

    node -> tokens.start = starting_token_index;
    node -> tokens.end = end_token_index;

    return left;
}

static AstNodeId nud(Parser* p, Token token) {
    switch (token.kind) {
        case TOK_INTEGER_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;

            node -> as.literal.kind = LITERAL_INTEGER;
            node -> as.literal.as.integer = get_integer_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_FLOAT_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;

            node -> as.literal.kind = LITERAL_FLOATING;
            node -> as.literal.as.floating = get_floating_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_CHAR_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;

            node -> as.literal.kind = LITERAL_CHAR;
            node -> as.literal.as.character = get_character_literal_from_token(p -> file, token);

            return id;
        }

        case TOK_STRING_LIT: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;

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

            node -> as.literal.kind = LITERAL_BOOL;
            node -> as.literal.as.boolean = token.kind == TOK_KW_TRUE;

            return id;
        }

        case TOK_KW_NULL: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_LITERAL;
            node -> tokens.start -= 1;

            node -> as.literal.kind = LITERAL_NULL;

            return id;
        }

        case TOK_IDENTIFIER: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_IDENTIFIER;
            node -> tokens.start -= 1;

            node -> as.identifier.name = string_intern_token(&p -> workspace -> string_interner, p -> file, token);

            return id;
        }

        case TOK_STAR:
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_AMP: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_UNARY_OP;
            node -> tokens.start -= 1;

            node -> as.unary_op.op = token.kind;
            node -> as.unary_op.operand = parse_expression(p, 119);

            return id;
        }

        case TOK_L_PAREN: {
            AstNodeId id = parse_expression(p, 0);

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

static AstNodeId led(Parser* p, Token token, AstNodeId left) {
    switch (token.kind) {
        case TOK_L_PAREN: {
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

        case TOK_L_BRACKET: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_INDEX;
            node -> tokens.start -= 2;

            node -> as.index.identifier = left;

            AstNodeId index_expr_id = parse_expression(p, 0);

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

        case TOK_DOT: {
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

            return id;
        }

        default: {
            AstNodeId id  = parser_create_node(p);
            AstNode* node = parser_get_node(p, id);

            node -> kind = AST_BINARY_OP; 

            AstNodeId right = parse_expression(p, op_info_table[token.kind].rbp);

            node = parser_get_node(p, id);

            node -> as.binary_op.op = token.kind;
            node -> as.binary_op.lhs = left;
            node -> as.binary_op.rhs = right;

            return id;
        }
    }
}

static void parse_call_args(Parser* p, AstNodeId id) {
    AstNode* call = parser_get_node(p, id);

    if (parser_check(p, TOK_R_PAREN)) {
        return;
    }

    do {
        AstNodeId arg_id = parse_expression(p, 0);

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
 * lbp helper
 *
 */

static inline u8 lbp_of(TokenKind kind) {
    if (kind > op_table_length) {
        return 0;
    }

    return op_info_table[kind].lbp;
}
