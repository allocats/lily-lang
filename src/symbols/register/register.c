#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/register/register.h"
#include "symbols/register/types.h"
#include "symbols/symbol/kinds.h"
#include "symbols/table/local/table.h"
#include "threadpool/types.h"
#include "utils/macros.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <stdio.h>

extern thread_local ThreadContext thread_context;

static void register_symbol(Registrar* r, AstNode* node);
static void register_function(Registrar* r, AstNode* node);
static void register_struct(Registrar* r, AstNode* node);
static void register_union(Registrar* r, AstNode* node);
static void register_enum(Registrar* r, AstNode* node);
static void register_variable(Registrar* r, AstNode* node);
static void register_import(Registrar* r, AstNode* node);

static void error_symbol_redefined(Registrar* r, AstNode* node, SymbolId symbol_id, StringId name);

/*
 *
 * Registrar scope navigation
 *
 */

inline ScopeId scope_enter(Registrar* r) {
    return r -> scope_id = local_scope_enter(r -> table, r -> scope_id);
}

inline ScopeId scope_exit(Registrar* r) {
    return r -> scope_id = local_scope_exit(r -> table, r -> scope_id);
}

/*
 *
 * Entrypoint for first pass
 *
 */

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

    file -> scope_id = r.scope_id;

    scope_exit(&r);
}

/*
 *
 * Symbol regster dispatcher
 *
 */

static void register_symbol(Registrar* r, AstNode* node) {
    switch (node -> kind) {
        case AST_FUNCTION_DECL: {
            register_function(r, node);
        } break;

        case AST_STRUCT_DECL: {
            register_struct(r, node);
        } break;

        case AST_UNION_DECL: {
            register_union(r, node);
        } break;

        case AST_ENUM_DECL: {
            register_enum(r, node);
        } break;

        case AST_VARIABLE_DECL: {
            register_variable(r, node);
        } break;

        case AST_IMPORT_DECL: {
            register_import(r, node);
        } break;

        case AST_EXECUTE_DIRECTIVE:
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

/*
 *
 * FUNCTION symbol
 *
 */

static void register_function(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.func_decl.name);

    node -> as.func_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
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
        symbol -> as.function_symbol.parameter_count = node -> as.func_decl.parameters.count;
        symbol -> as.function_symbol.parameters = null;
    }
}

/*
 *
 * STRUCT symbol
 *
 */

static void register_struct(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.struct_decl.name);

    node -> as.struct_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_STRUCT;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        symbol -> as.struct_symbol.resolved_type_id = TYPE_ID_NONE;
        symbol -> as.struct_symbol.field_count = node -> as.struct_decl.fields.count;
        symbol -> as.struct_symbol.fields = null;
    }
}

/*
 *
 * UNION symbol
 *
 */

static void register_union(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.union_decl.name);

    node -> as.union_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_UNION;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        symbol -> as.union_symbol.resolved_type_id = TYPE_ID_NONE;
        symbol -> as.union_symbol.field_count = node -> as.union_decl.fields.count;
        symbol -> as.union_symbol.fields = null;
    }
}

/*
 *
 * ENUM symbol
 *
 */

static void register_enum(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.enum_decl.name);

    node -> as.enum_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_ENUM;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        symbol -> as.enum_symbol.resolved_type_id = TYPE_ID_NONE;
        symbol -> as.enum_symbol.variant_count = node -> as.enum_decl.variants.count;
        symbol -> as.enum_symbol.variants = null;
    }
}

/*
 *
 * VARIABLE symbol
 *
 */

static void register_variable(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.variable_decl.name);

    node -> as.variable_decl.name = global_name_id;

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_VARIABLE;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        symbol -> as.variable_symbol.type_id = TYPE_ID_NONE;
    }
}

/*
 *
 * IMPORT symbols
 *
 */

static void register_import(Registrar* r, AstNode* node) {
    StringId global_name_id = global_string_from_local_id(node -> as.import_decl.name);

    node -> as.import_decl.name = global_name_id;

    if (global_name_id == GLOBAL_STRING_ID_NONE) {
        return;
    }

    SymbolId id = local_table_lookup(r -> table, r -> scope_id, global_name_id);

    if (local_symbol_get_id(id) != LOCAL_SYMBOL_ID_NONE) {
        error_symbol_redefined(r, node, id, global_name_id);
    } else {
        id = local_scope_intern(r -> table, r -> scope_id, global_name_id);

        Symbol* symbol = local_symbol_get(id);

        symbol -> kind = SYMBOL_IMPORT;

        symbol -> ast_node_id = node -> id;
        symbol -> file_id = r -> file -> id;
        symbol -> flags = node -> flags;

        AstNodeId import_directive_id  = node -> as.import_decl.import;
        AstNode* import_directive_node = &r -> ast -> nodes[import_directive_id];

        /*
         * More complex metaprogramming will be added later
         */
        assert(import_directive_node -> kind == AST_IMPORT_DIRECTIVE);

        symbol -> as.import_symbol.file_id  = import_directive_node -> as.import_directive.file_id;
        symbol -> as.import_symbol.scope_id = LOCAL_SCOPE_ID_NONE; 
    }
}



/*
 *
 * diagnostics and error handling
 *
 */

static void error_symbol_redefined(Registrar* r, AstNode* node, SymbolId symbol_id, StringId name) {
    DiagnosticEngine* engine = thread_context.diagnostic_engine;

    Symbol* symbol = local_symbol_get(symbol_id);

    str8 name_str = r -> ws -> global_string_interner.entries[name].str;

    str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
    str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "'%.*s' is redefined", STR8_FMT(name_str));
    snprintf(help.ptr, help.len, "'%.*s' is redefined here", STR8_FMT(name_str));

    if (symbol -> ast_node_id == AST_NODE_ID_NONE) {
        diagnostic_add_token_span(
            engine,
            r -> file,
            DIAG_ERROR,
            node -> tokens,
            msg.ptr,
            help.ptr
        );
    } else {
        str8 original_msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 original_help = DIAGNOSTIC_ALLOC_MSG(engine);

        snprintf(original_msg.ptr, original_msg.len, "'%.*s' is defined here", STR8_FMT(name_str));
        snprintf(original_help.ptr, original_help.len, "'%.*s' is defined here", STR8_FMT(name_str));

        File* file = workspace_get_file(r -> ws, symbol -> file_id);
        AstNode* defined_node = &file -> ast.nodes[symbol -> ast_node_id];

        diagnostic_add_token_span(
            engine,
            r -> file,
            DIAG_NOTE,
            defined_node -> tokens,
            original_msg.ptr,
            original_help.ptr
        );

        diagnostic_add_token_span(
            engine,
            r -> file,
            DIAG_ERROR,
            node -> tokens,
            msg.ptr,
            help.ptr
        );
    }
}
