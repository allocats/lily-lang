#include "symbols/resolve/type_expressions/type_expressions.h"
#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/table/global/table.h"
#include "types/table/table.h"
#include "utils/macros.h"
#include "utils/types.h"
#include <assert.h>
#include <stdio.h>

extern Driver driver;

static TypeId resolve_base_type(Resolver *r, AstNodeId id);
static TypeId resolve_pointer_type(Resolver *r, AstNodeId id);

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

        // case AST_TYPE_FUNCTION:
        // return resolve_function_type(r, node.as.function_type);

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
