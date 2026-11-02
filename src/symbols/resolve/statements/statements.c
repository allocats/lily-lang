#include "symbols/resolve/statements/statements.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "driver/types.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/expressions/expressions.h"
#include "symbols/resolve/resolve.h"
#include "symbols/resolve/type_expressions/type_expressions.h"
#include "symbols/symbol/kinds.h"
#include "symbols/symbol/symbol.h"
#include "symbols/table/global/table.h"
#include "types/entries/entries.h"

#include <assert.h>

extern Driver driver;

/*
 *  ==============================
 *
 *      Variable Declarations
 *
 *  ==============================
 */

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
/*
 *  =========================
 *
 *      Defer statements      
 *
 *  =========================
 */

bool resolve_defer_statement(Resolver *r, AstNode *node) {
    assert(node->kind == AST_DEFER_STMT);

    AstNode *expression = &r->ast->nodes[node->as.defer_stmt.expr];

    TypeId expression_type_id = resolve_expression(r, expression, TYPE_ID_NONE);

    if (expression_type_id == TYPE_ID_NONE) {
        return false;
    }

    node->resolved_type_id = expression_type_id;

    return true;
}

/*
 *  ==============================
 *
 *      Return statements      
 *
 *  ==============================
 */

bool resolve_return_statement(Resolver *r, AstNode *node) {
    assert(node->kind == AST_RETURN_STMT);

    TypeId return_type_id = symbol_get_type(r->symbol_table, r->current_symbol_id);

    if (return_type_id == TYPE_ID_NONE) {
        return false;
    }

    node->resolved_type_id = return_type_id;

    if (node->as.return_stmt.expr == AST_NODE_ID_NONE) {
        if (return_type_id == r->builtin_types.type_void) {
            return true;
        } else {
            AstNode *expression = &r->ast->nodes[node->as.return_stmt.expr];

            TypeId expression_type_id = resolve_expression(r, expression, return_type_id);

            if (expression_type_id == TYPE_ID_NONE) {
                return false;
            }

            resolver_mismatched_types(r, node, return_type_id, expression_type_id);

            return false;
        }
    }

    AstNode *expression = &r->ast->nodes[node->as.return_stmt.expr];

    TypeId expression_type_id = resolve_expression(r, expression, return_type_id);

    if (expression_type_id == TYPE_ID_NONE) {
        return false;
    }

    if (!types_are_assignable(r->type_table, &r->builtin_types, return_type_id, expression_type_id)) {
        resolver_mismatched_types(r, node, return_type_id, expression_type_id);
        return false;
    }

    return true;
}

/*
 *  ==============================
 *
 *          If statements      
 *
 *  ==============================
 */

bool resolve_if_statement(Resolver *r, AstNode *node) {
    assert(node->kind == AST_IF_STMT);

    TypeId bool_type = r->builtin_types.type_bool;

    u32 branch_count = node->as.if_stmt.branches.count;

    for (u32 i = 0; i < branch_count; i += 1) {
        AstNodeId branch_id  = node->as.if_stmt.branches.ids[i];
        AstNode *branch_node = &r->file->ast.nodes[branch_id];

        AstNodeId condition_id  = branch_node->as.branch.condition;
        AstNode *condition_node = &r->file->ast.nodes[condition_id];

        TypeId condition_type = resolve_expression(r, condition_node, bool_type);

        if (condition_type == TYPE_ID_NONE) {
            return false;
        }

        if (condition_type != bool_type) {
            resolver_mismatched_types(r, condition_node, bool_type, condition_type);
            return false;
        }

        resolver_enter_scope(r);

        bool block_result = resolve_block(r, branch_node->as.branch.body);

        resolver_exit_scope(r);

        if (!block_result) {
            return false;
        }
    }

    const AstNodeId else_block_id = node->as.if_stmt.else_block;

    if (else_block_id != AST_NODE_ID_NONE) {
        resolver_enter_scope(r);

        bool block_result = resolve_block(r, else_block_id);

        resolver_exit_scope(r);

        if (!block_result) {
            return false;
        }
    }

    return true;
}
