#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "parser/declarations/declarations.h"
#include "parser/directives/directives.h"
#include "parser/expressions/expressions.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/statements/statements.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

#include <pthread.h>

extern thread_local ThreadContext thread_context;

AstNodeId parse_statement(Parser *p) {
    Token token = parser_peek(p);

    switch (token.kind) {
    case TOK_HASHTAG: {
        return parse_directive(p);
    }

    case TOK_IDENTIFIER: {
        if (parser_peek_ahead_by(p, 1).kind == TOK_COLON) {
            return parse_variable_declaration(p);
        } else {
            AstNodeId id = parse_expression(p);

            if (parser_is_node(p, id, AST_ERROR)) {
                return id;
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

                return parser_error(p, id, RECOVERY_STATEMENT);
            }

            AstNode *node = parser_get_node(p, id);

            node->tokens.end = p->cursor;

            parser_advance(p); // advance past ';'

            return id;
        }
    }

    case TOK_L_BRACE: {
        return parse_block(p);
    }

    case TOK_KW_IF: {
        return parse_if_statement(p);
    }

    case TOK_KW_FOR: {
        bool previous_loop_context = p->in_loop_context;

        p->in_loop_context = true;

        AstNodeId id = parse_for_loop(p);

        p->in_loop_context = previous_loop_context;

        return id;
    }

    case TOK_KW_WHILE: {
        bool previous_loop_context = p->in_loop_context;

        p->in_loop_context = true;

        AstNodeId id = parse_while_loop(p);

        p->in_loop_context = previous_loop_context;

        return id;
    }

    case TOK_KW_BREAK: {
        return parse_break_statement(p);
    }

    case TOK_KW_CONTINUE: {
        return parse_continue_statement(p);
    }

    case TOK_KW_DEFER: {
        return parse_defer_statement(p);
    }

    case TOK_KW_RETURN: {
        return parse_return_statement(p);
    }

    default: {
        AstNodeId id = parse_expression(p);

        if (parser_is_node(p, id, AST_ERROR)) {
            return id;
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

            return parser_error(p, id, RECOVERY_STATEMENT);
        }

        AstNode *node = parser_get_node(p, id);

        node->tokens.end = p->cursor;

        parser_advance(p); // advance past ';'

        return id;
    }
    }
}
