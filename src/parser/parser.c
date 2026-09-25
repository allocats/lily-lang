#include "ast/ast.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/types.h"
#include "parser/declarations/declarations.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/types.h"
#include "tokens/kinds.h"
#include "tokens/types.h"

#include <assert.h>

extern Driver driver;

/*
 *
 * Parser entry point
 *
 */

void parse_file(File* file) {
    if (file -> stage == FILE_PARSED) return;

    /*
     * Should be impossible to be here not at this state
     */
    assert(file -> stage == FILE_LEXED);

    Parser p = {
        .workspace = driver.workspaces[file -> workspace_id],
        .file = file,
        .cursor = 0,
        .token_count = file -> tokens.count,
        .is_parsing_type_expr = false,
        .is_foreign_allowed = true,
    };

    while (p.cursor < p.token_count) {
        // just incase it is not set back because of some error
        // or something
        p.is_foreign_allowed = true;

        Token token = parser_peek(&p);

        switch (token.kind) {
            case TOK_EOF: {
                file -> stage = FILE_PARSED;
                return;
            }

            case TOK_HASHTAG: {
                AstNodeId id = parse_directive(&p);

                parser_set_node_flags(&p, id, AST_FLAG_TOP_DECL);

                if (parser_is_node(&p, id, AST_ERROR)) {
                    parser_recover(&p, RECOVERY_DECLARATION);
                }
            } break;
            
            case TOK_IDENTIFIER: {
                AstNodeId node_id = parser_create_node(&p);

                Token operator = parser_peek_ahead_by(&p, 1);

                switch (operator.kind) {
                    case TOK_COLON: {
                        // parse_variable_declaration()
                    } break;

                    case TOK_COLON_COLON: {
                        parse_top_level_declaration(&p, node_id);
                    } break;

                    default: {
                        diagnostic_add_token(
                            file,
                            DIAG_ERROR,
                            &token,
                            DIAG_LOC_WHOLE_TOK,
                            "unexpected token",
                            "expected (':' | '::') after identifier"
                        );

                        parser_error(&p, node_id, RECOVERY_DECLARATION);
                    } break;
                }
            } break;

            default: {
                diagnostic_add_token(
                    file,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "unexpected token",
                    "expected (#directive | identifier)"
                );

                AstNodeId node_id = parser_create_node(&p);
                parser_error(&p, node_id, RECOVERY_DECLARATION);
            } break;
        }
    }
}

/*
 *
 * AST wrappers for the parser for convenience
 *
 */

inline AstNodeId parser_create_node(Parser* p) {
    AstNodeId id = ast_alloc_node(&p -> file -> ast);
    AstNode* node = parser_get_node(p, id);

    node -> tokens.start = p -> cursor;

    p -> last_allocated_node_id = id;

    return id;
}

inline AstNode* parser_get_node(Parser* p, AstNodeId id) {
    return ast_get_node(&p -> file -> ast, id);
}

inline bool parser_is_node(Parser* p, AstNodeId id, AstKind kind) {
    return ast_get_node(&p -> file -> ast, id) -> kind == kind;
}

inline void parser_set_node_flags(Parser* p, AstNodeId id, AstFlag flags) {
    AstNode* node = parser_get_node(p, id);
    node -> flags |= flags;
}

/*
 *
 * Parser movement functions through source tokens
 *
 */

inline Token parser_peek(Parser* p) {
    assert(p -> cursor < p -> token_count);
    return p -> file -> tokens.items[p -> cursor];
}

inline Token parser_peek_ahead_by(Parser* p, u32 offset) {
    assert(p -> cursor + offset < p -> token_count);
    return p -> file -> tokens.items[p -> cursor + offset];
}

inline Token parser_peek_behind_by(Parser* p, u32 offset) {
    assert(p -> cursor < p -> token_count);
    assert(p -> cursor - offset > 0);
    return p -> file -> tokens.items[p -> cursor - offset];
}

inline Token parser_advance(Parser* p) {
    assert(p -> cursor < p -> token_count);
    return p -> file -> tokens.items[p -> cursor++];
}

inline bool parser_check(Parser* p, TokenKind kind) {
    return parser_peek(p).kind == kind;
}
