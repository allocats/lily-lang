#include "symbols/resolve/statements/statements.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/expressions/expressions.h"
#include "symbols/resolve/type_expressions/type_expressions.h"
#include "symbols/symbol/kinds.h"
#include "symbols/table/global/table.h"
#include "types/entries/entries.h"

#include <assert.h>

bool resolve_variable_declaration(Resolver *r, AstNode *node) {
    assert(node->kind == AST_VARIABLE_DECL);

    StringId global_string_id = node->as.variable_decl.name;

    if (node->as.variable_decl.name_convertd_to_global == false) {
        global_string_id = global_string_from_local_id(node->as.variable_decl.name);

        node->as.variable_decl.name                    = global_string_id;
        node->as.variable_decl.name_convertd_to_global = true;
    }

    SymbolId symbol_id = global_symbol_table_lookup(r->symbol_table, r->scope_id, global_string_id);

    if (symbol_id != GLOBAL_SYMBOL_ID_NONE) {
        resolver_error_symbol_redefined(r, node, symbol_id, global_string_id);
        return false;
    }

    symbol_id = global_scope_intern(r->symbol_table, r->scope_id, global_string_id);

    Symbol *symbol = &r->symbol_table->symbols[symbol_id];

    symbol->kind        = SYMBOL_VARIABLE;
    symbol->state       = RESOLVE_RESOLVING;
    symbol->flags       = node->flags;
    symbol->file_id     = r->file->id;
    symbol->ast_node_id = node->id;

    TypeId type_id = resolve_type_expression(r, node->as.variable_decl.type_expr);

    node->resolved_type_id   = type_id;
    node->resolved_symbol_id = symbol_id;

    symbol->as.variable_symbol.type_id = type_id;

    if (type_id == TYPE_ID_NONE) {
        symbol->state = RESOLVE_ERROR;
        return false;
    }

    if (node->as.variable_decl.init_expr != AST_NODE_ID_NONE) {
        AstNode *init_expr_node = &r->ast->nodes[node->as.variable_decl.init_expr];

        TypeId expr_type_id = resolve_expression(r, init_expr_node, type_id);

        if (expr_type_id == TYPE_ID_NONE) {
            symbol->state = RESOLVE_ERROR;
            return false;
        }

        if (!types_are_assignable(r->type_table, &r->builtin_types, type_id, expr_type_id)) {
            resolver_mismatched_types(r, init_expr_node, type_id, expr_type_id);
            symbol->state = RESOLVE_ERROR;
            return false;
        }
    }

    symbol->state = RESOLVE_RESOLVED;

    return true;
}
