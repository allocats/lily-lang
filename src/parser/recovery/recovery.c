#include "ast/kinds.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "tokens/kinds.h"

/*
 *
 * bitwise trickery to check if we are at a sync token
 *
 */

static constexpr u64 stmt_sync_tokens = (u64)  0
                                      | ((u64) 1 << TOK_EOF)
                                      | ((u64) 1 << TOK_KW_IF)
                                      | ((u64) 1 << TOK_KW_FOR)
                                      | ((u64) 1 << TOK_KW_WHILE)
                                      | ((u64) 1 << TOK_KW_DEFER)
                                      | ((u64) 1 << TOK_KW_RETURN)
                                      | ((u64) 1 << TOK_R_BRACE)
                                      | ((u64) 1 << TOK_SEMI)
                                      | ((u64) 1 << TOK_HASHTAG);

static constexpr u64 expr_sync_tokens = (u64)  0
                                      | ((u64) 1 << TOK_EOF)
                                      | ((u64) 1 << TOK_L_PAREN)
                                      | ((u64) 1 << TOK_L_BRACE)
                                      | ((u64) 1 << TOK_L_BRACKET)
                                      | ((u64) 1 << TOK_R_PAREN)
                                      | ((u64) 1 << TOK_R_BRACE)
                                      | ((u64) 1 << TOK_R_BRACKET)
                                      | ((u64) 1 << TOK_COMMA)
                                      | ((u64) 1 << TOK_SEMI);

/*
 *
 *  helpers
 *
 */

static bool is_token_sync_token(RecoveryKind recovery_kind, TokenKind kind) {
    switch (recovery_kind) {
        case RECOVERY_DECLARATION:
            return kind == TOK_EOF || kind == TOK_HASHTAG;

        case RECOVERY_EXPRESSION:
            return expr_sync_tokens & (1 << kind);

        case RECOVERY_STATEMENT:
            return stmt_sync_tokens & (1 << kind);

        case RECOVERY_NONE:
            return true;
    }

    return false;
}

void parser_recover(Parser* p, RecoveryKind kind) {
    while (p -> cursor < p -> token_count) {
        Token token = parser_peek(p);

        if (is_token_sync_token(kind, token.kind)) {
            return;
        }

        parser_advance(p);
    }
}

/*
 *
 *  main error function 
 *
 *  sets the offending node to AST_ERROR, and marks the end of the node
 *
 */

AstNodeId parser_error(Parser* p, AstNodeId id, RecoveryKind kind) {
    parser_recover(p, kind);

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;

    return id;
}
