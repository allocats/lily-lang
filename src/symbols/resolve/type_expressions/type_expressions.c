#include "symbols/resolve/type_expressions/type_expressions.h"
#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/types.h"
#include "symbols/symbol/symbol.h"
#include "symbols/table/global/table.h"
#include "types/table/table.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stdio.h>

extern Driver driver;

static TypeId resolve_base_type(Resolver *r, AstNodeId id);
static TypeId resolve_pointer_type(Resolver *r, AstNodeId id);
static TypeId resolve_slice_type(Resolver *r, AstNodeId id);

TypeId resolve_type_expression(Resolver *r, AstNodeId expr_id) {
    if (expr_id == AST_NODE_ID_NONE) {
        return r->ws->builtin_types.type_void;
    }

    AstNode *node = &r->ast->nodes[expr_id];

    switch (node->kind) {
    case AST_TYPE_BASE:
        return resolve_base_type(r, node->as.base_type.expr);

    case AST_TYPE_POINTER:
        return resolve_pointer_type(r, node->as.pointer_type.base_type);

    case AST_TYPE_VARIADIC:
        return r->builtin_types.type_va_list;

        // case AST_TYPE_SLICE:
        //     return resolve_slice_type(r, node->as.slice_type.element);

        // case AST_TYPE_FUNCTION:
        //     return resolve_function_type(r, node.as.function_type);

    default:
        UNREACHABLE("Type Expression %s", AST_KIND_STRINGS[node->kind]);
    }
}

static TypeId resolve_base_type(Resolver *r, AstNodeId id) {
    AstNode *node = &r->ast->nodes[id];

    switch (node->kind) {
    case AST_IDENTIFIER: {
        StringId global_string_id = global_string_from_local_id(node->as.identifier.name);

        node->as.identifier.name = global_string_id;

        SymbolId symbol_id = global_symbol_table_lookup(r->symbol_table, r->scope_id, global_string_id);

        if (symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 name = r->interner->entries[global_string_id].str;

            snprintf(msg.ptr, msg.len, "'%.*s' does not exist!", STR8_FMT(name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                node->tokens,
                msg.ptr,
                null
            );

            return TYPE_ID_NONE;
        }

        return type_table_lookup_nominal(r->type_table, symbol_id);
    }

    case AST_MEMBER_ACCESS: {
        AstNodeId object_id  = node->as.member_access.object;
        AstNode *object_node = &r->ast->nodes[object_id];

        assert(object_node->kind == AST_IDENTIFIER);

        StringId object_global_string_id = global_string_from_local_id(object_node->as.identifier.name);

        object_node->as.identifier.name = object_global_string_id;

        SymbolId object_symbol_id = global_symbol_table_lookup(r->symbol_table, r->scope_id, object_global_string_id);

        if (object_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            resolver_undeclared_identifier(r, object_node, object_global_string_id);
            return TYPE_ID_NONE;
        }

        Symbol *object_symbol = &r->symbol_table->symbols[object_symbol_id];

        if (object_symbol->kind != SYMBOL_IMPORT) {
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 name = r->interner->entries[object_global_string_id].str;

            snprintf(msg.ptr, msg.len, "'%.*s' is not an import", STR8_FMT(name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                object_node->tokens,
                msg.ptr,
                "expected named import in type expressions"
            );

            return TYPE_ID_NONE;
        }

        ScopeId scope_id = object_symbol->as.import_symbol.scope_id;

        /*
         *
         * Resolve the type in the import's scope
         *
         */

        AstNodeId member_id  = node->as.member_access.member;
        AstNode *member_node = &r->ast->nodes[member_id];

        assert(member_node->kind == AST_IDENTIFIER);

        StringId member_global_string_id = global_string_from_local_id(member_node->as.identifier.name);

        member_node->as.identifier.name = member_global_string_id;

        SymbolId member_symbol_id = global_scope_lookup(r->symbol_table, scope_id, member_global_string_id);

        if (member_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            resolver_undeclared_identifier(r, member_node, member_global_string_id);
            return TYPE_ID_NONE;
        }

        Symbol *member_symbol = &r->symbol_table->symbols[member_symbol_id];

        SymbolKind kind = member_symbol->kind;

        if (kind != SYMBOL_STRUCT && kind != SYMBOL_UNION && kind != SYMBOL_ENUM && kind != SYMBOL_TYPE) {
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 name = r->interner->entries[member_global_string_id].str;

            snprintf(msg.ptr, msg.len, "'%.*s' is not a type", STR8_FMT(name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                member_node->tokens,
                msg.ptr,
                "expected a type"
            );

            return TYPE_ID_NONE;
        }

        return symbol_get_type(r->symbol_table, member_symbol_id);
    }

    default:
        UNREACHABLE("Base type");
    }
}

static TypeId resolve_pointer_type(Resolver *r, AstNodeId id) {
    TypeId pointee = resolve_type_expression(r, id);

    if (pointee == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    return type_table_intern_pointer(r->type_table, pointee);
}

static TypeId resolve_slice_type(Resolver *r, AstNodeId id) {
    TypeId element = resolve_type_expression(r, id);

    if (element == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    return type_table_intern_slice(r->type_table, element);
}
