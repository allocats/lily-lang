#include "symbols/resolve/merge/merge.h"
#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "symbols/resolve/merge/types.h"
#include "symbols/table/global/table.h"
#include "symbols/table/global/types.h"
#include "utils/macros.h"

extern Driver driver;
extern thread_local ThreadContext thread_context;

static void resolve_imports_for_file(Resolver *r);
static void resolve_include(Resolver *r, AstNode *node);
static void resolve_import_directive(Resolver *r, AstNode *node);

static void merge_scopes(Resolver *r, ScopeId dest_id, ScopeId src_id);

static void error_symbol_redefined(Resolver *r, SymbolId original_id, SymbolId redefined_id, StringId name);

/*
 *
 * The first pass after registering top level symbols
 *
 */
void resolve_imports(Workspace *ws) {
    Resolver r = {
        .scope_id = COMPILER_SCOPE_ID,
        .table    = &ws->global_symbol_table,
        .ws       = ws
    };

    for (u32 i = 0; i < ws->file_interner.count; i += 1) {
        File *file = ws->file_interner.entries[i];

        r.file = file;
        r.ast  = &file->ast;

        resolve_imports_for_file(&r);
    }
}

static void resolve_imports_for_file(Resolver *r) {
    r->scope_id = r->file->scope_id;

    for (u32 i = 0; i < r->ast->count; i += 1) {
        AstNode *node = &r->ast->nodes[i];

        switch (node->kind) {
        case AST_INCLUDE_DIRECTIVE: {
            resolve_include(r, node);
        } break;

        case AST_IMPORT_DIRECTIVE: {
            resolve_import_directive(r, node);
        } break;

        case AST_IMPORT_DECL: {
            i = node->as.import_decl.import;
        } break;

        default:
            continue;
        }
    }
}

static void resolve_include(Resolver *r, AstNode *node) {
    File *file = r->ws->file_interner.entries[node->as.include_directive.file_id];

    merge_scopes(r, r->scope_id, file->scope_id);
}

static void resolve_import_directive(Resolver *r, AstNode *node) {
    File *file = r->ws->file_interner.entries[node->as.import_directive.file_id];

    merge_scopes(r, r->scope_id, file->scope_id);
}

static void merge_scopes(Resolver *r, ScopeId dest_id, ScopeId src_id) {
    GlobalSymbolTable *table = r->table;

    Scope src_scope = table->scopes[src_id];

    for (u32 i = 0; i < src_scope.capacity; i += 1) {
        SymbolId id = src_scope.entries[i];

        if (id != GLOBAL_SYMBOL_ID_NONE) {
            Symbol symbol = table->symbols[id];

            StringId name_id = symbol.name_id;

            SymbolId entry_id = global_scope_lookup(table, dest_id, name_id);

            if (entry_id != GLOBAL_SYMBOL_ID_NONE) {
                error_symbol_redefined(r, id, entry_id, name_id);
            } else {
                global_scope_add_symbol(table, dest_id, id);
            }
        }
    }
}

/*
 *
 * Diagnostics
 *
 */

static void error_symbol_redefined(Resolver *r, SymbolId original_id, SymbolId redefined_id, StringId name) {
    DiagnosticEngine *engine = &driver.local_diagnostic_engines[0];

    Symbol original  = r->table->symbols[original_id];
    Symbol redefined = r->table->symbols[redefined_id];

    str8 name_str = r->ws->global_string_interner.entries[name].str;

    assert(redefined.ast_node_id != AST_NODE_ID_NONE);

    File *redefined_file    = r->ws->file_interner.entries[redefined.file_id];
    AstNode *redefined_node = &redefined_file->ast.nodes[redefined.ast_node_id];

    str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
    str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "'%.*s' is redefined", STR8_FMT(name_str));
    snprintf(help.ptr, help.len, "'%.*s' is already defined in this scope", STR8_FMT(name_str));

    if (original.ast_node_id != AST_NODE_ID_NONE) {
        File *original_file    = r->ws->file_interner.entries[original.file_id];
        AstNode *original_node = &original_file->ast.nodes[original.ast_node_id];

        str8 note_msg = DIAGNOSTIC_ALLOC_MSG(engine);

        snprintf(note_msg.ptr, note_msg.len, "'%.*s' is first defined here", STR8_FMT(name_str));

        diagnostic_add_token_span(
            engine,
            original_file,
            DIAG_NOTE,
            original_node->tokens,
            note_msg.ptr,
            null
        );
    }

    diagnostic_add_token_span(
        engine,
        redefined_file,
        DIAG_ERROR,
        redefined_node->tokens,
        msg.ptr,
        help.ptr
    );
}
