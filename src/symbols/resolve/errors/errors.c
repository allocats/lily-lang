#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "symbols/resolve/types.h"
#include "utils/macros.h"
#include <stdio.h>

extern Driver driver;

void resolver_error_symbol_redefined(Resolver *r, AstNode *node, SymbolId symbol_id, StringId name) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    Symbol *symbol = &r->symbol_table->symbols[symbol_id];

    str8 name_str = r->ws->global_string_interner.entries[name].str;

    str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
    str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "'%.*s' is redefined", STR8_FMT(name_str));
    snprintf(help.ptr, help.len, "'%.*s' is redefined here", STR8_FMT(name_str));

    if (symbol->ast_node_id == AST_NODE_ID_NONE) {
        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            help.ptr
        );
    } else {
        str8 original_msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 original_help = DIAGNOSTIC_ALLOC_MSG(engine);

        snprintf(original_msg.ptr, original_msg.len, "'%.*s' is defined here", STR8_FMT(name_str));
        snprintf(original_help.ptr, original_help.len, "'%.*s' is defined here", STR8_FMT(name_str));

        File *file            = r->ws->file_interner.entries[symbol->file_id];
        AstNode *defined_node = &file->ast.nodes[symbol->ast_node_id];

        diagnostic_add_token_span(
            engine,
            file,
            DIAG_NOTE,
            defined_node->tokens,
            original_msg.ptr,
            original_help.ptr
        );

        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            help.ptr
        );
    }
}
