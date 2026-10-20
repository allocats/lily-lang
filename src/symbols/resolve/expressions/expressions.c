#include "symbols/resolve/expressions/expressions.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/resolve.h"
#include "symbols/symbol/kinds.h"
#include "symbols/symbol/symbol.h"
#include "symbols/table/global/table.h"
#include "tokens/kinds.h"
#include "types/builtins/types.h"
#include "types/entries/entries.h"
#include "types/entries/kinds.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdio.h>

extern Driver driver;

static TypeId resolve_identifier(Resolver *r, AstNode *node);
static TypeId resolve_literal(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_unary_op(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_binary_op(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_function_call(Resolver *r, AstNode *node, TypeId expected_type);

static SymbolId resolve_symbol_from_identifier(Resolver *r, AstNode *node);

static bool is_expression_addressable(Resolver *r, AstNode *node);

/*
 *
 * The actual expression dispatcher
 *
 */

static TypeId resolve(Resolver *r, AstNode *node, TypeId expected_type) {
    switch (node->kind) {
    case AST_IDENTIFIER:
        return resolve_identifier(r, node);

    case AST_LITERAL:
        return resolve_literal(r, node, expected_type);

    case AST_UNARY_OP:
        return resolve_unary_op(r, node, expected_type);

    case AST_BINARY_OP:
        return resolve_binary_op(r, node, expected_type);

    case AST_FUNCTION_CALL:
        return resolve_function_call(r, node, expected_type);

    case AST_INDEX:
    case AST_MEMBER_ACCESS:
    case AST_STRUCT_LITERAL:
        break;

    default:
        UNREACHABLE("Impossilbe expression %s", AST_KIND_STRINGS[node->kind]);
    }

    return TYPE_ID_NONE;
}

/*
 *
 * Entrypoint into expression resolving, wraps around the dispatched to easily tag the AST
 *
 */

TypeId resolve_expression(Resolver *r, AstNodeId id, TypeId expected_type) {
    AstNode *node = &r->ast->nodes[id];

    TypeId type_id = resolve(r, node, expected_type);

    node->resolved_type_id = type_id;

    return type_id;
}

static TypeId resolve_identifier(Resolver *r, AstNode *node) {
    StringId global_string_id = global_string_from_local_id(node->as.identifier.name);

    node->as.identifier.name = global_string_id;

    SymbolId symbol_id = global_symbol_table_lookup(r->symbol_table, r->scope_id, global_string_id);

    if (symbol_id == GLOBAL_SYMBOL_ID_NONE) {
        resolver_undeclared_identifier(r, node, global_string_id);

        return TYPE_ID_NONE;
    }

    const Symbol *symbol = &r->symbol_table->symbols[symbol_id];

    if (symbol->kind != SYMBOL_FUNCTION && !resolve_symbol(r->ws, symbol_id)) {
        return TYPE_ID_NONE;
    }

    node->resolved_symbol_id = symbol_id;

    return symbol_get_type(r->symbol_table, symbol_id);
}

static TypeId resolve_literal(Resolver *r, AstNode *node, TypeId expected_type) {
    switch (node->as.literal.kind) {
    case LITERAL_BOOL: {
        return r->builtin_types.type_bool;
    }

    case LITERAL_CHAR: {
        return r->builtin_types.type_char;
    }

    case LITERAL_NULL: {
        if (expected_type != TYPE_ID_NONE && type_is_kind(r->type_table, expected_type, TYPE_POINTER)) {
            return expected_type;
        } else {
            return type_table_intern_pointer(r->type_table, r->builtin_types.type_void);
        }
    }

    case LITERAL_STRING: {
        return r->builtin_types.type_string;
    }

    case LITERAL_FLOATING: {
        if (expected_type != TYPE_ID_NONE && type_is_float(r->builtin_types, expected_type)) {
            return expected_type;
        } else {
            return r->builtin_types.type_f64;
        }
    }

    case LITERAL_INTEGER: {
        if (expected_type != TYPE_ID_NONE && type_is_int(r->builtin_types, expected_type)) {
            return expected_type;
        } else {
            return r->builtin_types.type_i64;
        }
    }
    }
}

static TypeId resolve_unary_op(Resolver *r, AstNode *node, TypeId expected_type) {
    AstNode *operand_node = &r->ast->nodes[node->as.unary_op.operand];

    TypeId operand_type_id = resolve_expression(r, node->as.unary_op.operand, TYPE_ID_NONE);

    if (operand_type_id == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    switch (node->as.unary_op.op) {
    case TOK_AMP: {
        if (!is_expression_addressable(r, operand_node)) {
            resolver_cannot_reference_rvalue(r, operand_node);
            return TYPE_ID_NONE;
        }

        return type_table_intern_pointer(r->type_table, operand_type_id);
    }

    case TOK_STAR: {
        if (!is_expression_addressable(r, operand_node)) {
            resolver_cannot_dereference_rvalue(r, operand_node);
            return TYPE_ID_NONE;
        }

        if (!type_is_kind(r->type_table, operand_type_id, TYPE_POINTER)) {
            resolver_cannot_derefence_non_pointer(r, operand_node, operand_type_id);
            return TYPE_ID_NONE;
        }

        return r->type_table->entries[operand_type_id].as.pointer_type.pointee_id;
    }

    case TOK_BANG: {
        TypeId bool_type_id = r->builtin_types.type_bool;

        if (operand_type_id != bool_type_id) {
            resolver_mismatched_types(r, node, bool_type_id, operand_type_id);
            return TYPE_ID_NONE;
        }

        return bool_type_id;
    }

    case TOK_PLUS: {
        BuiltinTypeIds builtins = r->builtin_types;

        if (!type_is_float(builtins, operand_type_id) && !type_is_int(builtins, operand_type_id)) {
            resolver_cannot_apply_unary_op(r, node, '+', operand_type_id);
            return TYPE_ID_NONE;
        }

        return operand_type_id;
    }

    case TOK_MINUS: {
        BuiltinTypeIds builtins = r->builtin_types;

        if (!type_is_float(builtins, operand_type_id) && !type_is_signed_int(builtins, operand_type_id)) {
            resolver_cannot_apply_unary_op(r, node, '-', operand_type_id);
            return TYPE_ID_NONE;
        }

        return operand_type_id;
    }

    case TOK_TILDE: {
        BuiltinTypeIds builtins = r->builtin_types;

        if (operand_type_id != builtins.type_bool && !type_is_signed_int(builtins, operand_type_id)) {
            resolver_cannot_apply_unary_op(r, node, '~', operand_type_id);
            return TYPE_ID_NONE;
        }

        return operand_type_id;
    }

    default:
        UNREACHABLE("Impossible unary op operator %s", TOKEN_KIND_STRINGS[node->as.unary_op.op]);
    }
}

static TypeId resolve_binary_op(Resolver *r, AstNode *node, TypeId expected_type) {
}

static TypeId resolve_function_call(Resolver *r, AstNode *node, TypeId expected_type) {
    AstNodeId callee_id  = node->as.func_call.identifier;
    AstNode *callee_node = &r->ast->nodes[callee_id];

    /*
     *
     * Get the callee's symbol
     *
     */
    SymbolId callee_symbol_id = resolve_symbol_from_identifier(r, callee_node);

    if (callee_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
        return TYPE_ID_NONE;
    }

    const Symbol *callee_symbol = &r->symbol_table->symbols[callee_symbol_id];
    assert(callee_symbol->kind == SYMBOL_FUNCTION);

    node->resolved_symbol_id = callee_symbol_id;

    /*
     *
     * Backup the resolver state, and set it to the function's file
     *
     */

    Ast *r_ast         = r->ast;
    File *r_file       = r->file;
    ScopeId r_scope_id = r->scope_id;

    // get the callee's file
    File *callee_file = r->ws->file_interner.entries[callee_symbol->file_id];

    r->ast      = &callee_file->ast;
    r->file     = callee_file;
    r->scope_id = callee_file->scope_id;

    if (!resolve_function_signature(r, callee_symbol_id)) {
        /*
         * Restore the resolver's state
         */
        r->ast      = r_ast;
        r->file     = r_file;
        r->scope_id = r_scope_id;

        return TYPE_ID_NONE;
    }

    /*
     *
     * Restore the resolver's state;
     *
     */
    r->ast      = r_ast;
    r->file     = r_file;
    r->scope_id = r_scope_id;

    // refetch
    callee_symbol = &r->symbol_table->symbols[callee_symbol_id];

    bool is_variadic = callee_symbol->flags & AST_FLAG_VARIADIC;

    u32 argument_count  = node->as.func_call.arguments.count;
    u32 parameter_count = callee_symbol->as.function_symbol.parameter_count;

    if (is_variadic) {
        parameter_count -= 1;
    }

    if (is_variadic && argument_count < parameter_count) {
        DiagnosticEngine *engine = &driver.global_diagnostic_engine;

        str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 name = r->interner->entries[callee_symbol->name_id].str;

        snprintf(msg.ptr, msg.len, "'%.*s' takes %u or more arguments", STR8_FMT(name), parameter_count);

        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            null
        );
    } else if (!is_variadic && argument_count != parameter_count) {
        DiagnosticEngine *engine = &driver.global_diagnostic_engine;

        str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 name = r->interner->entries[callee_symbol->name_id].str;

        snprintf(
            msg.ptr,
            msg.len,
            "'%.*s' takes %u arguments, but %u were provided",
            STR8_FMT(name),
            parameter_count,
            argument_count
        );

        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            null
        );
    }

    for (u32 i = 0; i < argument_count; i += 1) {
        AstNodeId argument_node_id = node->as.func_call.arguments.ids[i];
        AstNode *argument_node     = &r->ast->nodes[argument_node_id];

        if (i < parameter_count) {
            SymbolId parameter_symbol_id = callee_symbol->as.function_symbol.parameters[i];
            Symbol *parameter_symbol     = &r->symbol_table->symbols[parameter_symbol_id];

            TypeId parameter_type_id = parameter_symbol->as.parameter_symbol.type_id;
            TypeId argument_type_id  = resolve_expression(r, argument_node_id, parameter_type_id);

            if (argument_type_id == TYPE_ID_NONE) {
                return TYPE_ID_NONE;
            }

            if (!types_are_assignable(r->type_table, &r->builtin_types, parameter_type_id, argument_type_id)) {
                resolver_mismatched_types(r, argument_node, parameter_type_id, argument_type_id);

                return TYPE_ID_NONE;
            }
        } else {
            resolve_expression(r, argument_node_id, TYPE_ID_NONE);
        }
    }

    TypeId return_type_id = callee_symbol->as.function_symbol.return_type_id;

    if (expected_type != TYPE_ID_NONE) {
        if (!types_are_assignable(r->type_table, &r->builtin_types, expected_type, return_type_id)) {
            resolver_mismatched_types(r, node, expected_type, return_type_id);

            return TYPE_ID_NONE;
        }
    }

    return return_type_id;
}

/*
 *
 * Herlper functions
 *
 */

/*
 *
 * Get the symbol for a named thingy! (member access and identifier)
 *
 */
static SymbolId resolve_symbol_from_identifier(Resolver *r, AstNode *node) {
    switch (node->kind) {
    case AST_IDENTIFIER: {
        StringId global_string_id = global_string_from_local_id(node->as.identifier.name);

        node->as.identifier.name = global_string_id;

        SymbolId id = global_symbol_table_lookup(r->symbol_table, r->scope_id, global_string_id);

        if (id == GLOBAL_SYMBOL_ID_NONE) {
            resolver_undeclared_identifier(r, node, global_string_id);
            return GLOBAL_SYMBOL_ID_NONE;
        }

        node->resolved_symbol_id = id;

        return id;
    }

    case AST_MEMBER_ACCESS: {
        AstNode *object_node = &r->ast->nodes[node->as.member_access.object];
        AstNode *member_node = &r->ast->nodes[node->as.member_access.member];
        assert(member_node->kind == AST_IDENTIFIER);

        SymbolId object_id = resolve_symbol_from_identifier(r, object_node);

        if (object_id == GLOBAL_SYMBOL_ID_NONE) {
            return GLOBAL_SYMBOL_ID_NONE;
        }

        if (!resolve_symbol(r->ws, object_id)) {
            return GLOBAL_SYMBOL_ID_NONE;
        }

        Symbol *object_symbol = &r->symbol_table->symbols[object_id];

        ScopeId object_scope_id = symbol_get_members_scope(object_symbol);

        if (object_scope_id == GLOBAL_SCOPE_ID_NONE) {
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 name = r->interner->entries[object_symbol->name_id].str;

            snprintf(msg.ptr, msg.len, "'%.*s' has no member scope", STR8_FMT(name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                object_node->tokens,
                msg.ptr,
                "type is not a structure"
            );

            return GLOBAL_SYMBOL_ID_NONE;
        }

        StringId member_string_id = global_string_from_local_id(member_node->as.identifier.name);

        member_node->as.identifier.name = member_string_id;

        SymbolId member_id = global_scope_lookup(r->symbol_table, object_scope_id, member_string_id);

        if (member_id == GLOBAL_SYMBOL_ID_NONE) {
            resolver_undeclared_identifier(r, member_node, member_string_id);
            return GLOBAL_SYMBOL_ID_NONE;
        }

        object_node->resolved_symbol_id = object_id;
        member_node->resolved_symbol_id = member_id;

        node->resolved_symbol_id = member_id;

        return member_id;
    }

    case AST_ERROR: {
        return GLOBAL_SYMBOL_ID_NONE;
    }

    default: {
        UNREACHABLE("Invalid callee %s", AST_KIND_STRINGS[node->kind]);
    }
    }
}

static bool is_expression_addressable(Resolver *r, AstNode *node) {
    switch (node->kind) {
    case AST_IDENTIFIER: {
        if (node->resolved_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            return false;
        }

        const Symbol *symbol = &r->symbol_table->symbols[node->resolved_symbol_id];

        return symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER || symbol->kind == SYMBOL_FUNCTION;
    }

    case AST_MEMBER_ACCESS: {
        if (node->resolved_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            return false;
        }

        const Symbol *symbol = &r->symbol_table->symbols[node->resolved_symbol_id];

        switch (symbol->kind) {
        case SYMBOL_VARIABLE:
            return true;

        case SYMBOL_FIELD: {
            AstNode *object_node = &r->ast->nodes[node->as.member_access.object];

            if (type_is_kind(r->type_table, object_node->resolved_type_id, TYPE_POINTER)) {
                return true;
            }

            return is_expression_addressable(r, object_node);
        }

        default:
            return false;
        }
    }

    case AST_INDEX: {
        AstNode *identifier = &r->ast->nodes[node->as.index.identifier];
        return is_expression_addressable(r, identifier);
    }

    case AST_UNARY_OP: {
        return node->as.unary_op.op == TOK_STAR;
    }

    default:
        return false;
    }
}
