#include "symbols/register/register.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/register/types.h"
#include "symbols/symbol/kinds.h"
#include "symbols/table/local/table.h"
#include "threadpool/types.h"
#include "utils/macros.h"
#include "workspace/workspace.h"

extern thread_local ThreadContext thread_context;

static void register_symbol(Registrar* r, AstNode* node);
static void register_function(Registrar* r, AstNode* node);

static void error_symbol_redefined(Registrar* r, AstNode* node, SymbolId symbol_id);

inline ScopeId scope_enter(Registrar* r) {
    return r -> scope_id = local_scope_enter(r -> table, r -> scope_id);
}

inline ScopeId scope_exit(Registrar* r) {
    return r -> scope_id = local_scope_exit(r -> table, r -> scope_id);
}

void symbols_register_top_level(Workspace* ws, File* file) {
    if (file -> scope_id != GLOBAL_SCOPE_ID_NONE) {
        return;
    }

    Registrar r = {
        .file     = file,
        .ast      = &file -> ast,
        .scope_id = COMPILER_SCOPE_ID,
        .table    = thread_context.symbol_table,
        .ws       = ws
    };

    scope_enter(&r);

    for (u32 i = 0; i < r.ast -> count; i += 1) {
        AstNode* node = &r.ast -> nodes[i];

        if (node -> flags & AST_FLAG_TOP_DECL) {
            register_symbol(&r, node);
        }
    }

    scope_exit(&r);
}

static void register_symbol(Registrar* r, AstNode* node) {
    switch (node -> kind) {
        case AST_FUNCTION_DECL: {
            register_function(r, node);
        } break;

        case AST_INCLUDE_DIRECTIVE:
        case AST_IMPORT_DIRECTIVE:
        case AST_ERROR:
            return;

        default:
            UNREACHABLE(
                "Found an invalid node during top level symbol registration [%s]",
                AST_KIND_STRINGS[node -> kind]
            );
    }
}

static void register_function(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.func_decl.name);

    node -> as.func_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_FUNCTION;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        symbol -> as.function_symbol.return_type_id = TYPE_ID_NONE;
        symbol -> as.function_symbol.scope_id = local_scope_create_id(r -> table -> id, LOCAL_SCOPE_ID_NONE);
        symbol -> as.function_symbol.signature_state = RESOLVE_UNRESOLVED;

        u32 param_count = node -> as.func_decl.parameters.count;

        symbol -> as.function_symbol.parameter_count = param_count;

        if (param_count == 0) {
            symbol -> as.function_symbol.parameters = null;
        } else {
            symbol -> as.function_symbol.parameters = arena_alloc(
                &r -> table -> symbol_data_arena,
                sizeof(SymbolId) * param_count
            );
        }
    }
}

static void error_symbol_redefined(Registrar* r, AstNode* node, SymbolId symbol_id) {
    Symbol* symbol = local_symbol_get(symbol_id);

    if (symbol -> ast_node_id == AST_NODE_ID_NONE) {
        diagnostic_add_token_span(
            r -> file,
            DIAG_ERROR,
            node -> tokens,
            "symbol is redefined",
            "this symbol is redefined here"
        );
    } else {
        File* file = workspace_get_file(r -> ws, symbol -> file_id);
        AstNode* defined_node = &file -> ast.nodes[symbol -> ast_node_id];

        diagnostic_add_token_span(
            r -> file,
            DIAG_NOTE,
            defined_node -> tokens,
            "symbol is defined here",
            "symbol is defined here"
        );

        diagnostic_add_token_span(
            r -> file,
            DIAG_ERROR,
            node -> tokens,
            "symbol is redefined",
            "this symbol is redefined here"
        );
    }
}
