#include "symbols/resolve/expressions/expressions.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "symbols/resolve/errors/errors.h"
#include "symbols/resolve/expressions/types.h"
#include "symbols/resolve/resolve.h"
#include "symbols/resolve/types.h"
#include "symbols/symbol/kinds.h"
#include "symbols/symbol/symbol.h"
#include "symbols/symbol/types.h"
#include "symbols/table/global/table.h"
#include "tokens/kinds.h"
#include "tokens/types.h"
#include "types/builtins/types.h"
#include "types/entries/entries.h"
#include "types/entries/kinds.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

extern Driver driver;

// ====================== Expressions =====================
static TypeId resolve_identifier(Resolver *r, AstNode *node);
static TypeId resolve_literal(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_unary_op(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_binary_op(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_function_call(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_member_access(Resolver *r, AstNode *node, TypeId expected_type);
static TypeId resolve_struct_literal(Resolver *r, AstNode *node, TypeId expected_type);

// ====================== Binary Operations =========================
static TypeId resolve_binary_op_assign(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op);
static TypeId resolve_binary_op_additive(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op);
static TypeId resolve_binary_op_multiplicative(Resolver *r, AstNode *lhs, AstNode *rhs);
static TypeId resolve_binary_op_bitwise(Resolver *r, AstNode *lhs, AstNode *rhs);
static TypeId resolve_binary_op_shift(Resolver *r, AstNode *lhs, AstNode *rhs);
static TypeId resolve_binary_op_comparison(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op);
static TypeId resolve_binary_op_logical(Resolver *r, AstNode *lhs, AstNode *rhs);

// ======================= Helpers =========================
static SymbolId resolve_symbol_from_identifier(Resolver *r, AstNode *node);

static bool is_expression_addressable(Resolver *r, AstNode *node);
static bool is_expression_assignable(Resolver *r, AstNode *node, bool *emitted_const_error);

/*
 * ===============================================
 *
 *                  Dispatcher
 *
 * ===============================================
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

    case AST_MEMBER_ACCESS:
        return resolve_member_access(r, node, expected_type);

    case AST_INDEX:
        UNREACHABLE("Index not yet supported");

    case AST_STRUCT_LITERAL:
        return resolve_struct_literal(r, node, expected_type);

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

TypeId resolve_expression(Resolver *r, AstNode *node, TypeId expected_type) {
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
        node->as.literal.as.string = global_string_from_local_id(node->as.literal.as.string);
        // return type_table_intern_pointer(r->type_table, r->builtin_types.type_u8);
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

    return TYPE_ID_NONE;
}

static TypeId resolve_unary_op(Resolver *r, AstNode *node, TypeId expected_type) {
    AstNode *operand_node = &r->ast->nodes[node->as.unary_op.operand];

    // ===== Operand type hint =====
    TypeId operand_type_hint = TYPE_ID_NONE;

    switch (node->as.unary_op.op) {
    case TOK_AMP: {
        if (expected_type != TYPE_ID_NONE && type_is_kind(r->type_table, expected_type, TYPE_POINTER)) {
            operand_type_hint = r->type_table->entries[expected_type].as.pointer_type.pointee_id;
        }

        break;
    }

    case TOK_BANG: {
        operand_type_hint = r->builtin_types.type_bool;
        break;
    }

    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_TILDE: {
        operand_type_hint = expected_type;
        break;
    }

    default:
        break;
    }

    // ===== resolve the operand =====
    TypeId operand_type_id = resolve_expression(r, operand_node, operand_type_hint);

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

        TypeId pointee_id = r->type_table->entries[operand_type_id].as.pointer_type.pointee_id;

        if (pointee_id == r->builtin_types.type_void) {
            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                node->tokens,
                "cannot derefence a pointer to 'void'",
                "cast this to another type before dereferencing"
            );

            return TYPE_ID_NONE;
        }

        return pointee_id;
    }

    case TOK_BANG: {
        TypeId bool_type_id = r->builtin_types.type_bool;

        if (operand_type_id != bool_type_id) {
            resolver_mismatched_types(r, operand_node, bool_type_id, operand_type_id);
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

        if (operand_type_id != builtins.type_bool && !type_is_int(builtins, operand_type_id)) {
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
    TokenKind op         = node->as.binary_op.op;
    BinaryOpKind op_kind = binary_op_kind_lut[op];

    AstNodeId lhs_id = node->as.binary_op.lhs;
    AstNodeId rhs_id = node->as.binary_op.rhs;

    AstNode *lhs_node = &r->ast->nodes[lhs_id];
    AstNode *rhs_node = &r->ast->nodes[rhs_id];

    TypeId lhs_hint = TYPE_ID_NONE;

    switch (op_kind) {
    case BINARY_OP_ADDITIVE:
    case BINARY_OP_MULTIPLICATIVE:
    case BINARY_OP_BITWISE:
    case BINARY_OP_SHIFT:
        if (expected_type == TYPE_ID_NONE || !type_is_kind(r->type_table, expected_type, TYPE_POINTER)) {
            lhs_hint = expected_type;
        }
        break;

    case BINARY_OP_LOGICAL:
        lhs_hint = r->builtin_types.type_bool;
        break;

    case BINARY_OP_ASSIGN:
    case BINARY_OP_COMPARISON:
    default:
        lhs_hint = TYPE_ID_NONE;
        break;
    }

    TypeId lhs_type = resolve_expression(r, lhs_node, lhs_hint);
    if (lhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId rhs_hint = (op_kind == BINARY_OP_SHIFT) ? TYPE_ID_NONE : lhs_type;

    TypeId rhs_type = resolve_expression(r, rhs_node, rhs_hint);
    if (rhs_type == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (lhs_type != rhs_type && lhs_node->kind == AST_LITERAL) {
        lhs_type = resolve_expression(r, lhs_node, rhs_type);
    }

    switch (op_kind) {
    case BINARY_OP_ASSIGN:
        return resolve_binary_op_assign(r, lhs_node, rhs_node, op);

    case BINARY_OP_ADDITIVE:
        return resolve_binary_op_additive(r, lhs_node, rhs_node, op);

    case BINARY_OP_MULTIPLICATIVE:
        return resolve_binary_op_multiplicative(r, lhs_node, rhs_node);

    case BINARY_OP_BITWISE:
        return resolve_binary_op_bitwise(r, lhs_node, rhs_node);

    case BINARY_OP_SHIFT:
        return resolve_binary_op_shift(r, lhs_node, rhs_node);

    case BINARY_OP_COMPARISON:
        return resolve_binary_op_comparison(r, lhs_node, rhs_node, op);

    case BINARY_OP_LOGICAL:
        return resolve_binary_op_logical(r, lhs_node, rhs_node);

    default:
        UNREACHABLE("Impossilbe binary operator '%s'", TOKEN_KIND_STRINGS[op]);
    }

    return TYPE_ID_NONE;
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

    if (callee_symbol->kind != SYMBOL_FUNCTION) {
        DiagnosticEngine *engine = &driver.global_diagnostic_engine;

        str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 name = r->ws->global_string_interner.entries[callee_symbol->name_id].str;

        snprintf(msg.ptr, msg.len, "'%.*s' is not a callable function", STR8_FMT(name));

        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            callee_node->tokens,
            msg.ptr,
            "only functions can be called"
        );

        return TYPE_ID_NONE;
    }

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

    TypeId callee_type_id = symbol_get_type(r->symbol_table, callee_symbol_id);

    callee_node->resolved_type_id   = callee_type_id;
    callee_node->resolved_symbol_id = callee_symbol_id;

    bool is_variadic = callee_symbol->flags & AST_FLAG_VARIADIC;
    bool failed      = false;

    u32 argument_count  = node->as.func_call.arguments.count;
    u32 parameter_count = callee_symbol->as.function_symbol.parameter_count;

    if (is_variadic && parameter_count > 0) {
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

        failed = true;
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

        failed = true;
    }

    for (u32 i = 0; i < argument_count; i += 1) {
        AstNodeId argument_node_id = node->as.func_call.arguments.ids[i];
        AstNode *argument_node     = &r->ast->nodes[argument_node_id];

        if (i < parameter_count) {
            SymbolId parameter_symbol_id = callee_symbol->as.function_symbol.parameters[i];
            Symbol *parameter_symbol     = &r->symbol_table->symbols[parameter_symbol_id];

            TypeId parameter_type_id = parameter_symbol->as.parameter_symbol.type_id;
            TypeId argument_type_id  = resolve_expression(r, argument_node, parameter_type_id);

            if (argument_type_id == TYPE_ID_NONE) {
                failed = true;
                continue;
            }

            if (!types_are_assignable(r->type_table, &r->builtin_types, parameter_type_id, argument_type_id)) {
                resolver_mismatched_types(r, argument_node, parameter_type_id, argument_type_id);

                failed = true;
            }
        } else {
            if (resolve_expression(r, argument_node, TYPE_ID_NONE) == TYPE_ID_NONE) {
                failed = true;
            }
        }
    }

    if (failed) {
        return TYPE_ID_NONE;
    }

    callee_symbol = &r->symbol_table->symbols[callee_symbol_id];

    TypeId return_type_id = callee_symbol->as.function_symbol.return_type_id;

    if (expected_type != TYPE_ID_NONE) {
        if (!types_are_assignable(r->type_table, &r->builtin_types, expected_type, return_type_id)) {
            resolver_mismatched_types(r, node, expected_type, return_type_id);

            return TYPE_ID_NONE;
        }
    }

    return return_type_id;
}

static TypeId resolve_member_access(Resolver *r, AstNode *node, TypeId expected_type) {
    SymbolId id = resolve_symbol_from_identifier(r, node);

    if (id == GLOBAL_SYMBOL_ID_NONE) {
        return TYPE_ID_NONE;
    }

    TypeId type = symbol_get_type(r->symbol_table, id);

    node->resolved_symbol_id = id;
    node->resolved_type_id   = type;

    return type;
}

static TypeId resolve_struct_literal(Resolver *r, AstNode *node, TypeId expected_type) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    AstNode *type_expr_node = &r->ast->nodes[node->as.struct_literal.type_expr];

    TypeId struct_type_id = resolve_expression(r, type_expr_node, expected_type);

    if (struct_type_id == TYPE_ID_NONE) {
        return TYPE_ID_NONE;
    }

    if (!type_is_structure(r->type_table, struct_type_id)) {
        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            type_expr_node->tokens,
            "type is not a struct or union in struct literal",
            null
        );

        return TYPE_ID_NONE;
    }

    if (expected_type != TYPE_ID_NONE && expected_type != struct_type_id) {
        resolver_mismatched_types(r, type_expr_node, expected_type, struct_type_id);
        return TYPE_ID_NONE;
    }

    const Type *struct_type         = &r->type_table->entries[struct_type_id];
    const SymbolId struct_symbol_id = struct_type->symbol_id;

    assert(struct_symbol_id != GLOBAL_SYMBOL_ID_NONE);

    bool is_union = struct_type->kind == TYPE_UNION;

    node->resolved_type_id   = struct_type_id;
    node->resolved_symbol_id = struct_symbol_id;

    type_expr_node->resolved_type_id   = struct_type_id;
    type_expr_node->resolved_symbol_id = struct_symbol_id;

    const Symbol *struct_symbol   = &r->symbol_table->symbols[struct_symbol_id];
    const ScopeId struct_scope_id = symbol_get_members_scope((Symbol *)struct_symbol);
    const StringId struct_name_id = struct_symbol->name_id;

    const AstNodeIdList *inits = &node->as.struct_literal.field_inits;

    bool failed = false;

    if (is_union && inits->count > 1) {
        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            "a union literal can only initialize one field",
            null
        );

        failed = true;
    }

    for (u32 i = 0; i < inits->count; i += 1) {
        AstNode *init_node = &r->ast->nodes[inits->ids[i]];

        assert(init_node->kind == AST_BINARY_OP);
        assert(init_node->as.binary_op.op == TOK_EQ);

        AstNode *field_node = &r->ast->nodes[init_node->as.binary_op.lhs];
        AstNode *value_node = &r->ast->nodes[init_node->as.binary_op.rhs];

        assert(field_node->kind == AST_IDENTIFIER);

        StringId field_name_id = global_string_from_local_id(field_node->as.identifier.name);

        field_node->as.identifier.name = field_name_id;

        SymbolId field_id = GLOBAL_SYMBOL_ID_NONE;

        if (struct_scope_id != GLOBAL_SCOPE_ID_NONE) {
            field_id = global_scope_lookup(r->symbol_table, struct_scope_id, field_name_id);
        }

        if (field_id == GLOBAL_SYMBOL_ID_NONE || r->symbol_table->symbols[field_id].kind != SYMBOL_FIELD) {
            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

            str8 struct_name = r->interner->entries[struct_name_id].str;
            str8 field_name  = r->interner->entries[field_name_id].str;

            snprintf(
                msg.ptr,
                msg.len,
                "'%.*s' has no field named '%.*s'",
                STR8_FMT(struct_name),
                STR8_FMT(field_name)
            );

            snprintf(help.ptr, help.len, "'%.*s' does not exist", STR8_FMT(field_name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                field_node->tokens,
                msg.ptr,
                help.ptr
            );

            // resolve it anyways to tag it, trying to fix some segfaulting behaviour rn
            resolve_expression(r, value_node, TYPE_ID_NONE);

            failed = true;
            continue;
        }

        /*
         * Fields can only be set once!
         */
        bool duplicate = false;

        for (u32 j = 0; j < i; j += 1) {
            const AstNode *previous = &r->ast->nodes[inits->ids[j]];

            if (previous->resolved_symbol_id == field_id) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);

            str8 field_name = r->interner->entries[field_name_id].str;

            snprintf(msg.ptr, msg.len, "field '%.*s' is initialized more than once", STR8_FMT(field_name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                field_node->tokens,
                msg.ptr,
                "this field was already initialized earlier in the literal"
            );

            failed = true;
        }

        init_node->resolved_symbol_id  = field_id;
        field_node->resolved_symbol_id = field_id;

        TypeId field_type_id = TYPE_ID_NONE;

        if (resolve_symbol(r->ws, field_id)) {
            field_type_id = symbol_get_type(r->symbol_table, field_id);
        }

        if (field_type_id == TYPE_ID_NONE) {
            resolve_expression(r, value_node, TYPE_ID_NONE);

            failed = true;
            continue;
        }

        init_node->resolved_type_id  = field_type_id;
        field_node->resolved_type_id = field_type_id;

        TypeId value_type_id = resolve_expression(r, value_node, field_type_id);

        if (value_type_id == TYPE_ID_NONE) {
            failed = true;
            continue;
        }

        if (!types_are_assignable(r->type_table, &r->builtin_types, field_type_id, value_type_id)) {
            resolver_mismatched_types(r, value_node, field_type_id, value_type_id);

            failed = true;
        }
    }

    if (failed) {
        return TYPE_ID_NONE;
    }

    return struct_type_id;
}

/*
 *
 * Binary operatorions!
 *
 */
static TypeId resolve_binary_op_assign(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op) {
    bool emitted_const_error = false;

    if (!is_expression_assignable(r, lhs, &emitted_const_error)) {
        if (!emitted_const_error) {
            resolver_lhs_is_not_lvalue(r, lhs);
        }

        return TYPE_ID_NONE;
    }

    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    if (type_is_kind(r->type_table, lhs_type, TYPE_POINTER)) {
        if (op == TOK_EQ) {
            if (!types_are_assignable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
                resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
                return TYPE_ID_NONE;
            }
        } else if (op == TOK_PLUS_EQ || op == TOK_MINUS_EQ) {
            if (!type_is_int(r->builtin_types, rhs_type)) {
                DiagnosticEngine *engine = &driver.global_diagnostic_engine;

                diagnostic_add_token_span(
                    engine,
                    r->file,
                    DIAG_ERROR,
                    rhs->tokens,
                    "pointer arithmetic requires an integer",
                    "expression does not evaluate to an integer"
                );

                return TYPE_ID_NONE;
            }
        } else {
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            u32 index = lhs->tokens.end + 1;

            Token *token = &r->file->tokens.items[index];

            str8 token_string = {
                .ptr = r->file->buffer.ptr + token->start,
                .len = token->length
            };

            str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);

            snprintf(msg.ptr, msg.len, "can't use the '%.*s' operator in pointer assignment", STR8_FMT(token_string));

            diagnostic_add_token(
                engine,
                r->file,
                DIAG_ERROR,
                token,
                DIAG_LOC_WHOLE_TOK,
                msg.ptr,
                "only the '=', '+=', and '-=' operators are allowed here"
            );

            return TYPE_ID_NONE;
        }
    } else {
        if (op != TOK_EQ && !type_is_numeric(r->builtin_types, lhs_type)) {
            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                lhs->tokens,
                "invalid operand in compound assignment",
                "expected a numeric e.g. i32, f32, isize"
            );

            return TYPE_ID_NONE;
        }

        if (!types_are_assignable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
            resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
            return TYPE_ID_NONE;
        }
    }

    return lhs_type;
}

static TypeId resolve_binary_op_additive(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op) {
    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    bool lhs_is_ptr = type_is_kind(r->type_table, lhs_type, TYPE_POINTER);
    bool rhs_is_ptr = type_is_kind(r->type_table, rhs_type, TYPE_POINTER);

    if (lhs_is_ptr) {
        switch (op) {
        case TOK_PLUS: {
            if (!type_is_int(r->builtin_types, rhs_type)) {
                diagnostic_add_token_span(
                    &driver.global_diagnostic_engine,
                    r->file,
                    DIAG_ERROR,
                    rhs->tokens,
                    "pointer arithmetic requires an integer",
                    "expression does not evaluate to an integer"
                );

                return TYPE_ID_NONE;
            }

            return lhs_type;
        }

        case TOK_MINUS: {
            if (type_is_int(r->builtin_types, rhs_type)) {
                return lhs_type;
            }

            if (rhs_is_ptr) {
                // TODO: maybe add a check here for the same pointer types? My pointers work as integers tho
                return r->builtin_types.type_isize;
            }

            diagnostic_add_token_span(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                rhs->tokens,
                "invalid operand for pointer subtraction",
                "expected an integer or a pointer here"
            );

            return TYPE_ID_NONE;
        }

        default: {
            UNREACHABLE("Impossible case %s", TOKEN_KIND_STRINGS[op]);
        }
        }
    }

    if (rhs_is_ptr) {
        if (type_is_int(r->builtin_types, lhs_type) && op == TOK_PLUS) {
            return rhs_type;
        }

        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            rhs->tokens,
            "can't subtract a pointer from a non-pointer",
            "try swapping the operands"
        );

        return TYPE_ID_NONE;
    }

    if (!type_is_numeric(r->builtin_types, lhs_type)) {
        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            lhs->tokens,
            "invalid operand in arithmetic",
            "expected a numeric type e.g i32, f64"
        );

        return TYPE_ID_NONE;
    }

    if (!type_is_numeric(r->builtin_types, rhs_type)) {
        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            rhs->tokens,
            "invalid operand in arithmetic",
            "expected a numeric type e.g i32, f64"
        );

        return TYPE_ID_NONE;
    }

    if (!types_are_arithmeticable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
        resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_binary_op_multiplicative(Resolver *r, AstNode *lhs, AstNode *rhs) {
    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    if (!type_is_numeric(r->builtin_types, lhs_type)) {
        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            lhs->tokens,
            "invalid operand in compound assignment",
            "expected a numeric e.g. i32, f32, isize"
        );

        return TYPE_ID_NONE;
    }

    if (!type_is_numeric(r->builtin_types, rhs_type)) {
        diagnostic_add_token_span(
            &driver.global_diagnostic_engine,
            r->file,
            DIAG_ERROR,
            rhs->tokens,
            "invalid operand in compound assignment",
            "expected a numeric e.g. i32, f32, isize"
        );

        return TYPE_ID_NONE;
    }

    if (!types_are_arithmeticable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
        resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_binary_op_bitwise(Resolver *r, AstNode *lhs, AstNode *rhs) {
    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    if (!type_is_int(r->builtin_types, lhs_type) && lhs_type != r->builtin_types.type_bool) {
        resolver_expected_int(r, lhs, lhs_type);
        return TYPE_ID_NONE;
    }

    if (!type_is_int(r->builtin_types, rhs_type) && rhs_type != r->builtin_types.type_bool) {
        resolver_expected_int(r, rhs, rhs_type);
        return TYPE_ID_NONE;
    }

    if (!types_are_arithmeticable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
        resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_binary_op_shift(Resolver *r, AstNode *lhs, AstNode *rhs) {
    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    if (!type_is_int(r->builtin_types, lhs_type)) {
        resolver_expected_int(r, lhs, lhs_type);
        return TYPE_ID_NONE;
    }

    bool rhs_is_literal = rhs->kind == AST_LITERAL && rhs->as.literal.kind == LITERAL_INTEGER;

    if (!type_is_unsigned_int(r->builtin_types, rhs_type) && !rhs_is_literal) {
        resolver_expected_unsigned_int(r, rhs, rhs_type);
        return TYPE_ID_NONE;
    }

    if (!types_are_arithmeticable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
        resolver_mismatched_types(r, rhs, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }

    return lhs_type;
}

static TypeId resolve_binary_op_comparison(Resolver *r, AstNode *lhs, AstNode *rhs, TokenKind op) {
    static constexpr bool lut[TYPE_KIND_COUNT] = {
        [TYPE_ARRAY]    = false,
        [TYPE_BASE]     = true,
        [TYPE_ENUM]     = true,
        [TYPE_ERROR]    = false,
        [TYPE_FUNCTION] = false,
        [TYPE_POINTER]  = true,
        [TYPE_SLICE]    = false,
        [TYPE_STRUCT]   = false,
        [TYPE_UNION]    = false,
    };

    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    TypeKind lhs_kind = r->type_table->entries[lhs_type].kind;
    TypeKind rhs_kind = r->type_table->entries[rhs_type].kind;

    if (!lut[lhs_kind]) {
        resolver_type_is_not_comparable(r, lhs, lhs_type);
        return TYPE_ID_NONE;
    }

    if (!lut[rhs_kind]) {
        resolver_type_is_not_comparable(r, rhs, rhs_type);
        return TYPE_ID_NONE;
    }

    if (!types_are_comparable(r->type_table, &r->builtin_types, lhs_type, rhs_type)) {
        resolver_cannot_compare_types(r, rhs, lhs_type, rhs_type);
        return TYPE_ID_NONE;
    }

    if (lhs_type == r->builtin_types.type_bool) {
        if (op != TOK_BANG_EQ && op != TOK_EQ_EQ) {
            Token *token = &r->file->tokens.items[lhs->tokens.end + 1];

            str8 operator = {
                .ptr = r->file->buffer.ptr + token->start,
                .len = token->length
            };

            str8 msg = DIAGNOSTIC_ALLOC_MSG((&driver.global_diagnostic_engine));

            snprintf(msg.ptr, msg.len, "cannot compare booleans with the '%.*s' operator", STR8_FMT(operator));

            diagnostic_add_token(
                &driver.global_diagnostic_engine,
                r->file,
                DIAG_ERROR,
                token,
                DIAG_LOC_WHOLE_TOK,
                msg.ptr,
                "only the '!=' and '==' operators can be used on booleans"
            );

            return TYPE_ID_NONE;
        }
    }

    return r->builtin_types.type_bool;
}

static TypeId resolve_binary_op_logical(Resolver *r, AstNode *lhs, AstNode *rhs) {
    TypeId lhs_type = lhs->resolved_type_id;
    TypeId rhs_type = rhs->resolved_type_id;

    assert(lhs_type != TYPE_ID_NONE);
    assert(rhs_type != TYPE_ID_NONE);

    TypeId bool_type_id = r->builtin_types.type_bool;

    if (lhs_type != bool_type_id) {
        resolver_mismatched_types(r, lhs, bool_type_id, lhs_type);
        return TYPE_ID_NONE;
    }

    if (rhs_type != bool_type_id) {
        resolver_mismatched_types(r, rhs, bool_type_id, rhs_type);
        return TYPE_ID_NONE;
    }

    return bool_type_id;
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

        const Symbol *object_symbol = &r->symbol_table->symbols[object_id];

        ScopeId object_scope_id = GLOBAL_SCOPE_ID_NONE;

        const SymbolKind kind = object_symbol->kind;

        if (kind == SYMBOL_VARIABLE || kind == SYMBOL_PARAMETER || kind == SYMBOL_FIELD) {
            TypeId type_id = symbol_get_type(r->symbol_table, object_id);

            if (type_id == TYPE_ID_NONE) {
                return GLOBAL_SYMBOL_ID_NONE;
            }

            object_node->resolved_type_id = type_id;

            const Type *type = &r->type_table->entries[type_id];

            /*
             *
             * Auto dereferencing 
             *
             */
            while (type->kind == TYPE_POINTER) {
                type = &r->type_table->entries[type->as.pointer_type.pointee_id];
            }

            const SymbolId type_symbol_id = type->symbol_id;

            if (type_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
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

            if (!resolve_symbol(r->ws, type_symbol_id)) {
                return GLOBAL_SYMBOL_ID_NONE;
            }

            object_symbol = &r->symbol_table->symbols[type_symbol_id];

            object_scope_id = symbol_get_members_scope(&r->symbol_table->symbols[type_symbol_id]);
        } else {
            object_scope_id = symbol_get_members_scope((Symbol *)object_symbol);
        }

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
            DiagnosticEngine *engine = &driver.global_diagnostic_engine;

            str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
            str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

            str8 name        = r->interner->entries[object_symbol->name_id].str;
            str8 member_name = r->interner->entries[member_string_id].str;

            snprintf(msg.ptr, msg.len, "'%.*s' has no member named '%.*s'", STR8_FMT(name), STR8_FMT(member_name));
            snprintf(help.ptr, help.len, "'%.*s' does not exist", STR8_FMT(member_name));

            diagnostic_add_token_span(
                engine,
                r->file,
                DIAG_ERROR,
                member_node->tokens,
                msg.ptr,
                help.ptr
            );

            return GLOBAL_SYMBOL_ID_NONE;
        }

        object_node->resolved_symbol_id = object_id;
        member_node->resolved_symbol_id = member_id;

        node->resolved_symbol_id = member_id;

        if (resolve_symbol(r->ws, member_id)) {
            const TypeId member_type = symbol_get_type(r->symbol_table, member_id);

            member_node->resolved_type_id = member_type;
            node->resolved_type_id        = member_type;
        }

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
        case SYMBOL_FUNCTION:
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

    case AST_BINARY_OP: {
        TokenKind op = node->as.binary_op.op;

        if (binary_op_kind_lut[op] != BINARY_OP_ADDITIVE) {
            return false;
        }

        AstNode *lhs_node = &r->ast->nodes[node->as.binary_op.lhs];
        AstNode *rhs_node = &r->ast->nodes[node->as.binary_op.rhs];

        TypeId lhs_type = lhs_node->resolved_type_id;
        TypeId rhs_type = rhs_node->resolved_type_id;

        if (lhs_type == TYPE_ID_NONE || rhs_type == TYPE_ID_NONE) {
            return false;
        }

        bool lhs_is_ptr = type_is_kind(r->type_table, lhs_type, TYPE_POINTER);
        bool rhs_is_ptr = type_is_kind(r->type_table, rhs_type, TYPE_POINTER);

        bool lhs_is_int = type_is_int(r->builtin_types, lhs_type);
        bool rhs_is_int = type_is_int(r->builtin_types, rhs_type);

        if (lhs_is_ptr && rhs_is_int) {
            if (op == TOK_PLUS || op == TOK_MINUS) {
                return true;
            } else {
                return false;
            }
        }

        if (lhs_is_int && rhs_is_ptr) {
            if (op == TOK_PLUS) {
                return true;
            } else {
                return false;
            }
        }

        return false;
    }

    default:
        return false;
    }
}

static bool is_expression_assignable(Resolver *r, AstNode *node, bool *emitted_const_error) {
    if (!is_expression_addressable(r, node)) {
        return false;
    }

    switch (node->kind) {
    case AST_IDENTIFIER: {
        if (node->resolved_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            return false;
        }

        const Symbol *symbol = &r->symbol_table->symbols[node->resolved_symbol_id];

        if (symbol->kind == SYMBOL_FUNCTION) {
            return false;
        }

        if (symbol->flags & AST_FLAG_CONSTANT) {
            if (type_is_kind(r->type_table, node->resolved_type_id, TYPE_POINTER)) {
                return true;
            }

            resolver_cannot_reassign_constant(r, node, symbol->name_id);

            *emitted_const_error = true;

            return false;
        }

        return true;
    }

    case AST_MEMBER_ACCESS: {
        if (node->resolved_symbol_id == GLOBAL_SYMBOL_ID_NONE) {
            return false;
        }

        const Symbol *symbol = &r->symbol_table->symbols[node->resolved_symbol_id];

        switch (symbol->kind) {
        case SYMBOL_VARIABLE: {
            if (symbol->flags & AST_FLAG_CONSTANT) {
                if (type_is_kind(r->type_table, node->resolved_type_id, TYPE_POINTER)) {
                    return true;
                }

                resolver_cannot_reassign_constant(r, node, symbol->name_id);

                *emitted_const_error = true;

                return false;
            }

            return true;
        }

        case SYMBOL_FIELD: {
            return is_expression_assignable(r, &r->ast->nodes[node->as.member_access.object], emitted_const_error);
        }

        default: {
            return false;
        }
        }
    }

    case AST_INDEX: {
        return is_expression_assignable(r, &r->ast->nodes[node->as.index.identifier], emitted_const_error);
    }

    case AST_UNARY_OP: {
        return node->as.unary_op.op == TOK_STAR &&
               is_expression_assignable(r, &r->ast->nodes[node->as.unary_op.operand], emitted_const_error);
    }

    case AST_BINARY_OP: {
        TokenKind op = node->as.binary_op.op;

        if (binary_op_kind_lut[op] != BINARY_OP_ADDITIVE) {
            return false;
        }

        AstNode *lhs_node = &r->ast->nodes[node->as.binary_op.lhs];
        AstNode *rhs_node = &r->ast->nodes[node->as.binary_op.rhs];

        TypeId lhs_type = lhs_node->resolved_type_id;
        TypeId rhs_type = rhs_node->resolved_type_id;

        if (lhs_type == TYPE_ID_NONE || rhs_type == TYPE_ID_NONE) {
            return false;
        }

        bool lhs_is_ptr = type_is_kind(r->type_table, lhs_type, TYPE_POINTER);
        bool rhs_is_ptr = type_is_kind(r->type_table, rhs_type, TYPE_POINTER);

        bool lhs_is_int = type_is_int(r->builtin_types, lhs_type);
        bool rhs_is_int = type_is_int(r->builtin_types, rhs_type);

        if (lhs_is_ptr && rhs_is_int) {
            if (op == TOK_PLUS || op == TOK_MINUS) {
                return true;
            } else {
                return false;
            }
        }

        if (lhs_is_int && rhs_is_ptr) {
            if (op == TOK_PLUS) {
                return true;
            } else {
                return false;
            }
        }

        return false;
    }

    default: {
        return false;
    }
    }
}
