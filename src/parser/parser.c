#include "parser/parser.h"
#include "ast/ast.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/types.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/directives/directives.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "parser/types.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"
#include "tokens/types.h"

#include <assert.h>
#include <pthread.h>

extern Driver driver;
extern thread_local ThreadContext thread_context;

/*
 *
 * Parser entry point
 *
 */

void parse_file(File *file) {
    if (file->stage == FILE_PARSED)
        return;

    /*
     * Should be impossible to be here not at this state
     */
    assert(file->stage == FILE_LEXED);

    Parser p = {
        .workspace       = driver.workspaces[file->workspace_id],
        .file            = file,
        .cursor          = 0,
        .token_count     = file->tokens.count,
        .in_loop_context = false,
    };

    while (p.cursor < p.token_count) {
        Token token = parser_peek(&p);

        switch (token.kind) {
        case TOK_EOF: {
            if (file->stage != FILE_ERROR) {
                file->stage = FILE_PARSED;
            }

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
            AstNodeId id = AST_NODE_ID_NONE;

            Token operator = parser_peek_ahead_by(&p, 1);

            switch (operator.kind) {
            case TOK_COLON: {
                id = parse_variable_declaration(&p);
            } break;

            case TOK_COLON_COLON: {
                id = parser_create_node(&p);

                parse_top_level_declaration(&p, id);
            } break;

            default: {
                diagnostic_add_token(
                    thread_context.diagnostic_engine,
                    file,
                    DIAG_ERROR,
                    &operator,
                    DIAG_LOC_WHOLE_TOK,
                    "unexpected token",
                    "expected (':' | '::') after identifier"
                );

                id = parser_create_node(&p);

                parser_error(&p, id, RECOVERY_DECLARATION);
            } break;
            }

            /*
                 *
                 * recover should the top level parsing fail and restart the loop
                 * more of a backup recovery, as most nodes will recover correctly 
                 * on their own.
                 *
                 */
            if (parser_is_node(&p, id, AST_ERROR)) {
                parser_recover(&p, RECOVERY_DECLARATION);

                continue;
            }

            /*
                 *
                 * mark the node as a top level declaration node
                 *
                 */
            parser_set_node_flags(&p, id, AST_FLAG_TOP_DECL);

            /*
                 *
                 * increment the number of top level declarations, this 
                 * is used to allocate once for the file's scope, the
                 * scope that holds all the top level symbols
                 *
                 */
            p.file->ast.top_level_declaration_count += 1;
            p.file->ast.total_declaration_count += 1;
        } break;

        default: {
            diagnostic_add_token(
                thread_context.diagnostic_engine,
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

inline AstNodeId parser_create_node(Parser *p) {
    AstNodeId id  = ast_alloc_node(&p->file->ast);
    AstNode *node = parser_get_node(p, id);

    node->id           = id;
    node->flags        = AST_FLAG_NONE;
    node->tokens.start = p->cursor;

    node->resolved_symbol_id = GLOBAL_SYMBOL_ID_NONE;
    node->resolved_type_id   = TYPE_ID_NONE;

    p->last_allocated_node_id = id;

    return id;
}

inline AstNode *parser_get_node(Parser *p, AstNodeId id) {
    return ast_get_node(&p->file->ast, id);
}

inline bool parser_is_node(Parser *p, AstNodeId id, AstKind kind) {
    return ast_get_node(&p->file->ast, id)->kind == kind;
}

inline void parser_set_node_flags(Parser *p, AstNodeId id, AstFlag flags) {
    AstNode *node = parser_get_node(p, id);
    node->flags |= flags;
}

inline void parser_init_id_list(Parser *p, AstNodeIdList *list, u32 count) {
    ast_id_list_init(&p->file->ast, list, count);
}

inline void parser_append_id_list(Parser *p, AstNodeIdList *list, AstNodeId id) {
    ast_append_id_list(&p->file->ast, list, id);
}

/*
 *
 * Parser movement functions through source tokens
 *
 */

inline Token parser_peek(Parser *p) {
    assert(p->cursor < p->token_count);
    return p->file->tokens.items[p->cursor];
}

inline Token parser_peek_ahead_by(Parser *p, u32 offset) {
    assert(p->cursor + offset < p->token_count);
    return p->file->tokens.items[p->cursor + offset];
}

inline Token parser_peek_behind_by(Parser *p, u32 offset) {
    assert(p->cursor < p->token_count);
    assert(p->cursor - offset > 0);
    return p->file->tokens.items[p->cursor - offset];
}

inline Token parser_advance(Parser *p) {
    assert(p->cursor < p->token_count);
    return p->file->tokens.items[p->cursor++];
}

inline bool parser_check(Parser *p, TokenKind kind) {
    return parser_peek(p).kind == kind;
}

inline bool parser_check_ahead_by(Parser *p, TokenKind kind, u32 offset) {
    return (parser_peek_ahead_by(p, offset).kind == kind);
}
