#include "parser/parser.h"
#include "parser/types.h"

#include <assert.h>

/*
 *
 * Parser entry point
 *
 */

void parse_file(File* file) {
    assert(file -> stage == FILE_LEXED);

    Parser p = {
        .file = file,
        .cursor = 0,
        .token_count = file -> tokens.count,
        .is_parsing_type_expr = false,
        .is_foreign_allowed = true,
    };
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
    assert(p -> cursor < p -> token_count);
    assert(p -> cursor - offset > 0);
    return p -> file -> tokens.items[p -> cursor - offset];
}

inline Token parser_peek_behind_by(Parser* p, u32 offset) {
    assert(p -> cursor + offset < p -> token_count);
    return p -> file -> tokens.items[p -> cursor + offset];
}

inline Token parser_advance(Parser* p) {
    assert(p -> cursor < p -> token_count);
    return p -> file -> tokens.items[p -> cursor++];
}

inline bool parser_check(Parser* p, TokenKind kind) {
    return parser_peek(p).kind == kind;
}
