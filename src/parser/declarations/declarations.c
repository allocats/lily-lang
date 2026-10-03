#include "diagnostics/diagnostics.h"
#include "parser/declarations/declarations.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "string_interner/local/interner.h"
#include "threadpool/types.h"
#include "tokens/kinds.h"

extern ThreadContext thread_context;

AstNodeId parse_top_level_declaration(Parser *p, AstNodeId id) {
    Token identifier = parser_peek(p);

    StringId name_id = local_string_intern_token(thread_context.string_interner, p -> file, identifier);

    parser_advance(p); // advance past 'identifier'
    parser_advance(p); // advance past '::'

    Token token = parser_peek(p);

    switch (token.kind) {
        case TOK_KW_INTRINSIC:
        case TOK_KW_FOREIGN:
        case TOK_KW_NOINLINE:
        case TOK_KW_INLINE:
        case TOK_KW_FN:
            return parse_function_declaration(p, id, name_id);

        case TOK_KW_STRUCT:
            return parse_struct_declaration(p, id, name_id);

        case TOK_KW_UNION:
            return parse_union_declaration(p, id, name_id);

        case TOK_KW_ENUM:
            return parse_enum_declaration(p, id, name_id);

        default: {
            diagnostic_add_token(
                p -> file,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "unexpected token",
                "expected (fn | enum | struct | union | inline | noinline | foreign | intrinsic | identifier)"
            );

            return parser_error(p, id, RECOVERY_DECLARATION);
        } break;
    }
}
