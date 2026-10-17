#include "symbols/resolve/resolve.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/type_expressions/type_expressions.h"
#include "symbols/resolve/types.h"
#include "symbols/symbol/kinds.h"
#include "symbols/table/global/table.h"
#include "types/builtins/types.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "workspace/types.h"

#include <assert.h>
#include <stdio.h>

extern Driver driver;

/*
 *
 * StringId for the name "main" used for storing the entrypoint 
 *
 */

static StringId main_string_id = GLOBAL_STRING_ID_NONE;

/*
 *
 * Forward declarations
 *
 */

static bool resolve_symbol(Workspace *ws, SymbolId id);
static bool resolve_symbol_body(Workspace *ws, SymbolId id);

static bool resolve_function_signature(Resolver *r, SymbolId id);
static bool resolve_function(Resolver *r, SymbolId id);

static bool resolve_block(Resolver *r, AstNodeId id);

void resolve_symbols(Workspace *ws) {
    u32 count = ws->global_symbol_table.symbol_count;

    main_string_id = global_string_intern_cstr(&ws->global_string_interner, "main");

    assert(main_string_id != GLOBAL_STRING_ID_NONE);

    for (SymbolId i = 0; i < count; i += 1) {
        resolve_symbol(ws, i);
    }

    if (ws->entry_point_id == GLOBAL_SYMBOL_ID_NONE) {
        diagnostic_add_generic(
            &driver.global_diagnostic_engine,
            DIAG_ERROR,
            "no entrypoint function (main) found for Workspace %u(%.*s)",
            ws->id,
            STR8_FMT(ws->name)
        );
    }
}

static bool resolve_symbol(Workspace *ws, SymbolId id) {
    assert(id != GLOBAL_SYMBOL_ID_NONE);

    Symbol *symbol = &ws->global_symbol_table.symbols[id];

    if (symbol->state == RESOLVE_RESOLVED)
        return true;
    if (symbol->state == RESOLVE_ERROR)
        return false;

    ResolveQuery query = {
        .kind      = QUERY_SYMBOL,
        .as.symbol = id
    };

    if (symbol->state == RESOLVE_RESOLVING) {
        i32 cycle_start = resolver_stack_find(query);

        if (cycle_start == -1) {
            UNREACHABLE("Resolver cycle");
        } else {
            DiagnosticEngine *diagnostic_engine = &driver.global_diagnostic_engine;

            File *file = ws->file_interner.entries[symbol->file_id];

            AstNode node = file->ast.nodes[symbol->ast_node_id];

            str8 name = ws->global_string_interner.entries[symbol->name_id].str;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(diagnostic_engine);
            str8 help = DIAGNOSTIC_ALLOC_MSG(diagnostic_engine);

            snprintf(msg.ptr, msg.len, "'%.*s' embeds itself recursively", STR8_FMT(name));
            snprintf(help.ptr, help.len, "add indirection: *%.*s", STR8_FMT(name));

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                file,
                DIAG_ERROR,
                node.tokens,
                msg.ptr,
                help.ptr
            );
        }

        symbol->state = RESOLVE_ERROR;

        return false;
    }

    symbol->state = RESOLVE_RESOLVING;

    if (!resolver_stack_push(query)) {
        DiagnosticEngine *diagnostic_engine = &driver.global_diagnostic_engine;

        File *file = ws->file_interner.entries[symbol->file_id];

        AstNode node = file->ast.nodes[symbol->ast_node_id];

        str8 name = ws->global_string_interner.entries[symbol->name_id].str;

        str8 msg  = DIAGNOSTIC_ALLOC_MSG(diagnostic_engine);
        str8 help = DIAGNOSTIC_ALLOC_MSG(diagnostic_engine);

        snprintf(msg.ptr, msg.len, "'%.*s' embeds itself recursively too much (reached stack limit)", STR8_FMT(name));
        snprintf(help.ptr, help.len, "please stop what you are doing and seek help");

        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            file,
            DIAG_ERROR,
            node.tokens,
            msg.ptr,
            help.ptr
        );

        symbol->state = RESOLVE_ERROR;

        return false;
    }

    bool result = resolve_symbol_body(ws, id);

    resolver_stack_pop();

    symbol->state = result ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return result;
}

static bool resolve_symbol_body(Workspace *ws, SymbolId id) {
    assert(id != GLOBAL_SYMBOL_ID_NONE);

    Symbol symbol = ws->global_symbol_table.symbols[id];

    File *file = ws->file_interner.entries[symbol.file_id];

    Resolver r = {
        .ast           = &file->ast,
        .builtin_types = ws->builtin_types,
        .file          = file,
        .interner      = &ws->global_string_interner,
        .scope_id      = file->scope_id,
        .symbol_table  = &ws->global_symbol_table,
        .type_table    = &ws->type_table,
        .ws            = ws,
    };

    switch (symbol.kind) {
    case SYMBOL_FUNCTION:
        return resolve_function(&r, id);

    case SYMBOL_ERROR:
        return false;

    default:
        UNREACHABLE("Found %s, ID=%u", SYMBOL_KIND_STRINGS[symbol.kind], id);
    }
}

static bool resolve_function_signature(Resolver *r, SymbolId id) {
    Symbol *symbol = resolver_get_symbol(r, id);

    if (symbol->as.function_symbol.signature_state == RESOLVE_RESOLVED)
        return true;
    if (symbol->as.function_symbol.signature_state == RESOLVE_ERROR)
        return false;

    if (symbol->as.function_symbol.signature_state == RESOLVE_RESOLVING) {
        UNREACHABLE("How the fweak. Please report this one");
    }

    symbol->as.function_symbol.signature_state = RESOLVE_RESOLVING;

    u32 parameter_count = symbol->as.function_symbol.parameter_count;

    if (parameter_count > 0) {
        symbol->as.function_symbol.parameters = arena_alloc(
            &r->symbol_table->symbol_data_arena,
            sizeof(SymbolId) * parameter_count
        );
    }

    bool result = true;

    /*
     *
     * backup the state
     *
     */
    File *r_file_restore    = r->file;
    ScopeId r_scope_restore = r->scope_id;

    /*
     *
     * set the new state
     *
     */
    File *file   = r->ws->file_interner.entries[symbol->file_id];
    AstNode node = file->ast.nodes[symbol->ast_node_id];

    r->file     = file;
    r->scope_id = file->scope_id;

    symbol->as.function_symbol.scope_id = resolver_enter_scope(r);

    for (u32 i = 0; i < node.as.func_decl.parameters.count; i += 1) {
        AstNodeId parameter_id  = node.as.func_decl.parameters.ids[i];
        AstNode *parameter_node = &r->file->ast.nodes[parameter_id];

        assert(parameter_node->kind == AST_PARAMETER_DECL);

        StringId parameter_name_id = global_string_from_local_id(parameter_node->as.param_decl.name);

        parameter_node->as.param_decl.name = parameter_name_id;

        SymbolId parameter_symbol_id = global_symbol_table_lookup(r->symbol_table, r->scope_id, parameter_name_id);

        if (parameter_symbol_id != GLOBAL_SYMBOL_ID_NONE) {
            resolver_error_symbol_redefined(r, parameter_node, parameter_symbol_id, parameter_name_id);

            result = false;
            continue;
        }

        TypeId parameter_type_id = resolve_type_expression(r, parameter_node->as.param_decl.type_expr);

        if (parameter_type_id == TYPE_ID_NONE) {
            result = false;
            continue;
        }

        parameter_symbol_id = global_scope_intern(r->symbol_table, r->scope_id, parameter_name_id);

        Symbol *parameter_symbol = resolver_get_symbol(r, parameter_symbol_id);

        parameter_symbol->kind        = SYMBOL_PARAMETER;
        parameter_symbol->file_id     = r->file->id;
        parameter_symbol->ast_node_id = parameter_id;
        parameter_symbol->flags       = parameter_node->flags;

        parameter_symbol->as.parameter_symbol.type_id = parameter_type_id;

        symbol = resolver_get_symbol(r, id);

        symbol->as.function_symbol.parameters[i] = parameter_symbol_id;
    }

    TypeId return_type_id = resolve_type_expression(r, node.as.func_decl.return_type_expr);

    if (return_type_id == TYPE_ID_NONE) {
        result = false;
    }

    symbol->as.function_symbol.return_type_id = return_type_id;

    resolver_exit_scope(r);

    /*
     *
     * restore the state
     *
     */
    r->file     = r_file_restore;
    r->scope_id = r_scope_restore;

    return result;
}

static bool resolve_function(Resolver *r, SymbolId id) {
    if (!resolve_function_signature(r, id)) {
        return false;
    }

    Symbol symbol = r->symbol_table->symbols[id];
    AstNode node  = r->file->ast.nodes[symbol.ast_node_id];

    /*
     *
     * Early return as `instrinic` and `foreign` functions do not have bodies
     *
     */
    if (symbol.flags & AST_FLAG_INTRINSIC || symbol.flags & AST_FLAG_FOREIGN) {

        /*
         *
         * RULE: main function cannot have any modifiers
         *
         */
        if (symbol.name_id == main_string_id) {
            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                node.tokens,
                "'main' cannot have function modifiers",
                "remove all function modifiers"
            );

            return false;
        }

        return true;
    }

    /*
     *
     * RULES:
     *   1. 'main' may not have parameters
     *   2. 'main' may not return a value
     *   3. 'main' may only be present ONCE in a workspace
     *
     */
    if (symbol.name_id == main_string_id) {

        /*
         *
         * Rule 1
         *
         */
        if (symbol.as.function_symbol.parameter_count != 0) {
            AstNodeId param_start_id = node.as.func_decl.parameters.ids[0];
            AstNodeId param_end_id   = node.as.func_decl.parameters.ids[node.as.func_decl.parameters.count - 1];

            AstNode param_start_node = r->file->ast.nodes[param_start_id];
            AstNode param_end_node   = r->file->ast.nodes[param_end_id];

            SpanU32 span = {
                .start = param_start_node.tokens.start,
                .end   = param_end_node.tokens.end,
            };

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                span,
                "'main' must have no parameters",
                "remove all parameters from 'main'"
            );

            return false;
        }

        /*
         *
         * Rule 2
         *
         */
        if (symbol.as.function_symbol.return_type_id != r->builtin_types.type_void) {
            AstNode node = r->file->ast.nodes[symbol.ast_node_id];

            AstNode return_expr = r->file->ast.nodes[node.as.func_decl.return_type_expr];

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                return_expr.tokens,
                "'main' must return 'void'",
                "remove this return type or make it '-> void'"
            );

            return false;
        }

        /*
         *
         * Rule 3
         *
         */
        if (r->ws->entry_point_id != GLOBAL_SYMBOL_ID_NONE) {
            Symbol original_declaration = r->symbol_table->symbols[r->ws->entry_point_id];
            File *original_file         = r->ws->file_interner.entries[original_declaration.file_id];
            AstNode original_node       = original_file->ast.nodes[original_declaration.ast_node_id];

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                original_file,
                DIAG_NOTE,
                original_node.tokens,
                "multiple entrypoint 'main' functions found",
                "'main' is defined here"
            );

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                node.tokens,
                "multiple entrypoint 'main' functions found",
                "remove this file/function from the workspace or rename the function"
            );

            return false;
        }

        /*
         *
         * Set the workspace's entrypoint to this function
         *
         */
        r->ws->entry_point_id = id;
    }

    /*
     * 
     * Store the original scope
     *
     */
    ScopeId r_scope_id_restore = r->scope_id;

    r->scope_id = symbol.as.function_symbol.scope_id;

    bool result = resolve_block(r, node.as.func_decl.body);

    /*
     *
     * Restore the original scope state
     *
     */
    r->scope_id = r_scope_id_restore;

    return result;
}

static bool resolve_block(Resolver *r, AstNodeId id) {
    AstNode node = r->file->ast.nodes[id];

    assert(node.kind == AST_BLOCK);

    bool result = true;

    for (u32 i = 0; i < node.as.block.statements.count; i += 1) {
        AstNodeId stmt_id = node.as.block.statements.ids[i];
        AstNode stmt      = r->file->ast.nodes[stmt_id];

        switch (stmt.kind) {
        case AST_VARIABLE_DECL:
        case AST_EXECUTE_DIRECTIVE:
        case AST_IF_STMT:
        case AST_WHILE_LOOP:
        case AST_FOR_LOOP:
        case AST_BLOCK:
        case AST_RETURN_STMT:
        case AST_DEFER_STMT:
            break;

        case AST_FUNCTION_CALL:
        case AST_BINARY_OP:
            break;

        case AST_MEMBER_ACCESS:
        case AST_INDEX:
        case AST_UNARY_OP:
        case AST_IDENTIFIER:
        case AST_LITERAL:
        case AST_STRUCT_LITERAL:
            break;

        default:
            UNREACHABLE("Impossible statement %s", AST_KIND_STRINGS[stmt.kind]);
        }
    }

    return result;
}

/*
 *
 * Helpers
 *
 */

inline Symbol *resolver_get_symbol(Resolver *r, SymbolId id) {
    return &r->ws->global_symbol_table.symbols[id];
}

ScopeId resolver_enter_scope(Resolver *r) {
    return r->scope_id = global_scope_enter(r->symbol_table, r->scope_id);
}

ScopeId resolver_exit_scope(Resolver *r) {
    return r->scope_id = global_scope_exit(r->symbol_table, r->scope_id);
}
