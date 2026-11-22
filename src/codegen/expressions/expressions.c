#include "codegen/expressions/expressions.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "codegen/declarations/declarations.h"
#include "codegen/declarations/intrinsics/intrinsics.h"
#include "codegen/declarations/intrinsics/types.h"
#include "codegen/types.h"
#include "codegen/types/types.h"
#include "ids.h"
#include "symbols/resolve/expressions/types.h"
#include "symbols/symbol/kinds.h"
#include "tokens/kinds.h"
#include "types/entries/entries.h"
#include "types/entries/kinds.h"
#include "utils/macros.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Types.h>
#include <stdio.h>

// =========== LValues (actual address) =============
static LLVMValueRef codegen_lvalue(CodegenCtx *ctx, const AstNode *node);

// =========== Identifiers =============
static LLVMValueRef codegen_identifier(CodegenCtx *ctx, const AstNode *node);

// =========== Function Calls =============
static LLVMValueRef codegen_function_call(CodegenCtx *ctx, const AstNode *node);

// =========== Intrinsic Calls =============
static LLVMValueRef codegen_intrinsic_call(CodegenCtx *ctx, const AstNode *node);

// =========== Literals =============
static LLVMValueRef codegen_literal(CodegenCtx *ctx, const AstNode *node);
static LLVMValueRef codegen_string_literal(CodegenCtx *ctx, const StringId id);

// =========== Member Access =============
static LLVMValueRef codegen_member_access(CodegenCtx *ctx, const AstNode *node);

// =========== Struct Literal =============
static LLVMValueRef codegen_struct_literal(CodegenCtx *ctx, const AstNode *node);

// =========== Unary Op =============
static LLVMValueRef codegen_unary_op(CodegenCtx *ctx, const AstNode *node);

// =========== Binary Op =============
static LLVMValueRef codegen_binary_op(CodegenCtx *ctx, const AstNode *node);
static LLVMValueRef codegen_binary_op_assign(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_additive(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_multiplicative(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_bitwise(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_shift(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_comparison(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);
static LLVMValueRef codegen_binary_op_logical(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op);

/* 
 *
 * ===========================================
 *
 *          Expression entrypoint
 *
 * ===========================================
 * 
 */

LLVMValueRef codegen_expression(CodegenCtx *ctx, const AstNode *node) {
    switch (node->kind) {
    case AST_LITERAL:
        return codegen_literal(ctx, node);

    case AST_IDENTIFIER:
        return codegen_identifier(ctx, node);

    case AST_FUNCTION_CALL:
        return codegen_function_call(ctx, node);

    case AST_MEMBER_ACCESS:
        return codegen_member_access(ctx, node);

    case AST_STRUCT_LITERAL:
        return codegen_struct_literal(ctx, node);

    case AST_UNARY_OP:
        return codegen_unary_op(ctx, node);

    case AST_BINARY_OP:
        return codegen_binary_op(ctx, node);

    default:
        return null;
    }
}

/* 
 *
 * =====================================
 *
 *                LValues 
 *
 * =====================================
 * 
 */

static LLVMValueRef codegen_lvalue(CodegenCtx *ctx, const AstNode *node) {
    switch (node->kind) {
    case AST_IDENTIFIER: {
        return ctx->symbol_map[node->resolved_symbol_id];
    }

    case AST_UNARY_OP: {
        assert(node->as.unary_op.op == TOK_STAR);

        const AstNodeId operand_id  = node->as.unary_op.operand;
        const AstNode *operand_node = &ctx->file->ast.nodes[operand_id];

        return codegen_expression(ctx, operand_node);
    }

    case AST_MEMBER_ACCESS: {
        const TypeTable *types = &ctx->ws->type_table;
        const AstNode *object  = &ctx->file->ast.nodes[node->as.member_access.object];

        TypeId type_id = object->resolved_type_id;

        LLVMValueRef address = null;

        if (type_is_kind(types, type_id, TYPE_POINTER)) {
            address = codegen_expression(ctx, object);

            type_id = types->entries[type_id].as.pointer_type.pointee_id;

            while (type_is_kind(types, type_id, TYPE_POINTER)) {
                address = LLVMBuildLoad2(ctx->llvm_builder, type_to_llvm_type(type_id), address, "");

                type_id = types->entries[type_id].as.pointer_type.pointee_id;
            }
        } else {
            address = codegen_lvalue(ctx, object);
        }

        assert(address != null);

        const Symbol *field = &ctx->ws->global_symbol_table.symbols[node->resolved_symbol_id];

        LLVMTypeRef type = type_to_llvm_type(type_id);

        return LLVMBuildStructGEP2(ctx->llvm_builder, type, address, field->as.field_symbol.index, "");
    }

    default:
        UNREACHABLE("Impossible case: %s", AST_KIND_STRINGS[node->kind]);
    }
}

/* 
 *
 * ===========================================
 *
 *                  Identifiers
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_identifier(CodegenCtx *ctx, const AstNode *node) {
    const SymbolId symbol_id = node->resolved_symbol_id;
    assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[symbol_id];

    switch (symbol->kind) {
    case SYMBOL_VARIABLE: {
        LLVMValueRef address = ctx->symbol_map[symbol_id];
        assert(address != null);

        LLVMTypeRef type = type_to_llvm_type(symbol->as.variable_symbol.type_id);

        return LLVMBuildLoad2(ctx->llvm_builder, type, address, "");
    }

    case SYMBOL_PARAMETER: {
        if (symbol->as.parameter_symbol.type_id == ctx->ws->builtin_types.type_va_list) {
            return ctx->llvm_ap_address;
        }

        LLVMValueRef address = ctx->symbol_map[symbol_id];
        assert(address != null);

        LLVMTypeRef type = type_to_llvm_type(symbol->as.parameter_symbol.type_id);

        return LLVMBuildLoad2(ctx->llvm_builder, type, address, "");
    }

    case SYMBOL_FUNCTION: {
        if (ctx->symbol_map[symbol_id] != null) {
            return ctx->symbol_map[symbol_id];
        }

        return codegen_function_signature(ctx, symbol_id);
    }

    case SYMBOL_VARIANT: {
        if (ctx->symbol_map[symbol_id] != null) {
            return ctx->symbol_map[symbol_id];
        }

        Type *type_entry = &ctx->ws->type_table.entries[symbol->as.variant_symbol.type_id];

        LLVMTypeRef underlying_type = type_to_llvm_type(type_entry->as.enum_type.underlying_type_id);
        LLVMValueRef value          = LLVMConstInt(underlying_type, symbol->as.variant_symbol.value, 0);

        ctx->symbol_map[symbol_id] = value;

        return value;
    }

    default:
        UNREACHABLE("Impossible SymbolKind %s", SYMBOL_KIND_STRINGS[symbol->kind]);
    }

    return null;
}

/* 
 *
 * ================================================
 *
 *                  Function Calls
 *
 * ================================================
 * 
 */

static LLVMValueRef codegen_function_call(CodegenCtx *ctx, const AstNode *node) {
    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[node->resolved_symbol_id];

    if (symbol->flags & AST_FLAG_INTRINSIC) {
        return codegen_intrinsic_call(ctx, node);
    }

    const AstNode *identifier = &ctx->file->ast.nodes[node->as.func_call.identifier];

    LLVMValueRef function = codegen_expression(ctx, identifier);
    assert(function != null);

    LLVMTypeRef function_type = LLVMGlobalGetValueType(function);

    u32 argument_count = node->as.func_call.arguments.count;

    // We use a stack allocatin if the parameter count fits in a certain threshold
    LLVMValueRef arguments_stack_allocation[STACK_ALLOCATION_THRESHOLD];

    u32 argument_count_doubled = argument_count * 2;

    LLVMValueRef *arguments = argument_count_doubled <= STACK_ALLOCATION_THRESHOLD
                                  ? arguments_stack_allocation
                                  : lily_alloc(argument_count_doubled * sizeof(LLVMValueRef));

    u32 argument_index = 0;

    for (u32 i = 0; i < argument_count; i += 1) {
        const AstNodeId argument_id  = node->as.func_call.arguments.ids[i];
        const AstNode *argument_node = &ctx->file->ast.nodes[argument_id];

        LLVMValueRef argument = codegen_expression(ctx, argument_node);
        assert(argument != null);

        if (type_is_string(ctx, argument_node->resolved_type_id)) {
            arguments[argument_index++] = LLVMBuildExtractValue(ctx->llvm_builder, argument, 0, "");
            arguments[argument_index++] = LLVMBuildExtractValue(ctx->llvm_builder, argument, 1, "");
        } else {
            arguments[argument_index++] = argument;
        }
    }

    LLVMValueRef call = LLVMBuildCall2(ctx->llvm_builder, function_type, function, arguments, argument_index, "");

    if (arguments != arguments_stack_allocation) {
        free(arguments);
    }

    return call;
}

/* 
 *
 * ================================================
 *
 *                Intrinsic Calls
 *
 * ================================================
 * 
 */

static LLVMValueRef codegen_intrinsic_call(CodegenCtx *ctx, const AstNode *node) {
    const AstNode *identifier = &ctx->file->ast.nodes[node->as.func_call.identifier];

    IntrinsicKind kind = INTRINSIC_ID_NONE;

    switch (identifier->kind) {
    case AST_IDENTIFIER: {
        kind = intrinsic_get_kind(identifier->as.identifier.name);
        break;
    }

    case AST_MEMBER_ACCESS: {
        const AstNodeId member_id  = identifier->as.member_access.member;
        const AstNode *member_node = &ctx->file->ast.nodes[member_id];

        assert(member_node->kind == AST_IDENTIFIER);

        kind = intrinsic_get_kind(member_node->as.identifier.name);
        break;
    }

    default:
        kind = INTRINSIC_NONE;
        break;
    }

    if (kind == INTRINSIC_NONE) {
        return null;
    }

    u32 argument_count = node->as.func_call.arguments.count;

    LLVMValueRef arguments_stack_allocation[8];

    LLVMValueRef *arguments = argument_count <= 8
                                  ? arguments_stack_allocation
                                  : lily_alloc(argument_count * sizeof(LLVMValueRef));

    for (u32 i = 0; i < argument_count; i += 1) {
        const AstNodeId argument_id  = node->as.func_call.arguments.ids[i];
        const AstNode *argument_node = &ctx->file->ast.nodes[argument_id];

        LLVMValueRef argument = codegen_expression(ctx, argument_node);
        assert(argument != null);

        arguments[i] = argument;
    }

    LLVMValueRef call = null;

    switch (kind) {
    case INTRINSIC_MEMSET: {
        LLVMValueRef dest = arguments[0];
        LLVMValueRef byte = arguments[1];
        LLVMValueRef size = arguments[2];

        u32 alignment = LLVMGetAlignment(dest);

        call = LLVMBuildMemSet(ctx->llvm_builder, dest, byte, size, alignment);
        break;
    }

    case INTRINSIC_MEMCPY: {
        LLVMValueRef dest = arguments[0];
        LLVMValueRef src  = arguments[1];
        LLVMValueRef size = arguments[2];

        u32 dest_alignment = LLVMGetAlignment(dest);
        u32 src_alignment  = LLVMGetAlignment(src);

        call = LLVMBuildMemCpy(ctx->llvm_builder, dest, dest_alignment, src, src_alignment, size);
        break;
    }

    case INTRINSIC_MEMMOVE: {
        LLVMValueRef dest = arguments[0];
        LLVMValueRef src  = arguments[1];
        LLVMValueRef size = arguments[2];

        u32 dest_alignment = LLVMGetAlignment(dest);
        u32 src_alignment  = LLVMGetAlignment(src);

        call = LLVMBuildMemMove(ctx->llvm_builder, dest, dest_alignment, src, src_alignment, size);
        break;
    }

    default:
        UNREACHABLE("Impossible case: %s", INTRINSIC_KIND_STRINGS[kind]);
    }

    if (arguments != arguments_stack_allocation) {
        free(arguments);
    }

    return call;
}

/* 
 *
 * ===========================================
 *
 *                  Literals
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_literal(CodegenCtx *ctx, const AstNode *node) {
    switch (node->as.literal.kind) {
    case LITERAL_STRING:
        return codegen_string_literal(ctx, node->as.literal.as.string);

    case LITERAL_INTEGER:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.integer, 0);

    case LITERAL_BOOL:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.boolean, 0);

    case LITERAL_CHAR:
        return LLVMConstInt(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.character, 0);

    case LITERAL_FLOATING:
        return LLVMConstReal(type_to_llvm_type(node->resolved_type_id), node->as.literal.as.floating);

    case LITERAL_NULL:
        return LLVMConstPointerNull(type_to_llvm_type(node->resolved_type_id));
    }
}

static LLVMValueRef codegen_string_literal(CodegenCtx *ctx, const StringId id) {
    if (ctx->string_map[id] != null) {
        return ctx->string_map[id];
    }

    char name[64] = {0};
    snprintf(name, sizeof(name), "str.%u", id);

    str8 str = ctx->ws->global_string_interner.entries[id].str;

    char *copy   = lily_calloc(str.len + 1, 1);
    char *cursor = copy;

    for (u32 i = 0; i < str.len; i += 1) {
        if (str.ptr[i] == '\\') {
            i += 1;

            char c = str.ptr[i];

            switch (c) {
            case '0': {
                *cursor++ = '\0';
            } break;

            case 'a': {
                *cursor++ = '\a';
            } break;

            case 'b': {
                *cursor++ = '\b';
            } break;

            case 'f': {
                *cursor++ = '\f';
            } break;

            case 'n': {
                *cursor++ = '\n';
            } break;

            case 'r': {
                *cursor++ = '\r';
            } break;

            case 't': {
                *cursor++ = '\t';
            } break;

            case 'v': {
                *cursor++ = '\v';
            } break;

            case '\\': {
                *cursor++ = '\\';
            } break;

            case '"': {
                *cursor++ = '\"';
            } break;

            default: {
                *cursor++ = '\\';
                *cursor++ = c;
            } break;
            }
        } else {
            *cursor++ = str.ptr[i];
        }
    }

    const usize length = cursor - copy;

    LLVMValueRef data   = LLVMConstStringInContext(ctx->llvm_ctx, copy, length, 0);
    LLVMValueRef global = LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(data), name);

    LLVMSetInitializer(global, data);
    LLVMSetGlobalConstant(global, 1);
    LLVMSetLinkage(global, LLVMPrivateLinkage);
    LLVMSetUnnamedAddr(global, 1);

    LLVMValueRef fields[2] = {
        global,
        LLVMConstInt(type_to_llvm_type(ctx->ws->builtin_types.type_usize), length, 0),
    };

    LLVMValueRef string = LLVMConstNamedStruct(type_to_llvm_type(ctx->ws->builtin_types.type_string), fields, 2);

    ctx->string_map[id] = string;

    free(copy);

    return string;
}

/* 
 *
 * ===========================================
 *
 *               Member Access
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_member_access(CodegenCtx *ctx, const AstNode *node) {
    const Symbol *symbol = &ctx->ws->global_symbol_table.symbols[node->resolved_symbol_id];

    if (symbol->kind != SYMBOL_FIELD) {
        return codegen_identifier(ctx, &ctx->file->ast.nodes[node->as.member_access.member]);
    }

    LLVMValueRef address = codegen_lvalue(ctx, node);

    return LLVMBuildLoad2(ctx->llvm_builder, type_to_llvm_type(node->resolved_type_id), address, "");
}

/* 
 *
 * ===========================================
 *
 *              Struct Literal
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_struct_literal(CodegenCtx *ctx, const AstNode *node) {
    const TypeId type_id = node->resolved_type_id;

    if (type_is_kind(&ctx->ws->type_table, type_id, TYPE_STRUCT)) {
        // ===== Struct type =====
        LLVMTypeRef struct_type = type_to_llvm_type(type_id);

        const SymbolId symbol_id = node->resolved_symbol_id;
        const Symbol *symbol     = &ctx->ws->global_symbol_table.symbols[symbol_id];

        const u32 field_count = symbol->as.struct_symbol.field_count;

        LLVMValueRef *values = lily_calloc(field_count, sizeof(LLVMValueRef));
        LLVMTypeRef *types   = lily_calloc(field_count, sizeof(LLVMTypeRef));

        LLVMGetStructElementTypes(struct_type, types);

        // ===== All the expressions =====
        const u32 count = node->as.struct_literal.field_inits.count;

        bool all_fields_are_constant = true;

        for (u32 i = 0; i < count; i += 1) {
            const AstNodeId field_init_id  = node->as.struct_literal.field_inits.ids[i];
            const AstNode *field_init_node = &ctx->file->ast.nodes[field_init_id];

            assert(field_init_node->kind == AST_BINARY_OP);

            const AstNodeId field_id  = field_init_node->as.binary_op.lhs;
            const AstNode *field_node = &ctx->file->ast.nodes[field_id];

            const Symbol *field_symbol = &ctx->ws->global_symbol_table.symbols[field_node->resolved_symbol_id];
            const u32 field_index      = field_symbol->as.field_symbol.index;

            const AstNodeId expression_id  = field_init_node->as.binary_op.rhs;
            const AstNode *expression_node = &ctx->file->ast.nodes[expression_id];

            LLVMValueRef expression_value = codegen_expression(ctx, expression_node);

            if (expression_value == null) {
                free(values);
                free(types);

                return null;
            }

            if (!LLVMIsConstant(expression_value)) {
                all_fields_are_constant = false;
            }

            values[field_index] = expression_value;
        }

        for (u32 i = 0; i < field_count; i += 1) {
            if (values[i] == null) {
                values[i] = LLVMConstNull(types[i]);
            }
        }

        LLVMValueRef result = null;

        if (all_fields_are_constant) {
            result = LLVMConstNamedStruct(struct_type, values, field_count);
        } else {
            result = LLVMGetPoison(struct_type);

            for (u32 i = 0; i < field_count; i += 1) {
                result = LLVMBuildInsertValue(ctx->llvm_builder, result, values[i], i, "");
            }
        }

        free(values);
        free(types);

        return result;
    } else {
        UNREACHABLE("");
    }
}

/* 
 *
 * ===========================================
 *
 *                 Unary Op
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_unary_op(CodegenCtx *ctx, const AstNode *node) {
    TokenKind op = node->as.unary_op.op;

    const AstNodeId operand_id  = node->as.unary_op.operand;
    const AstNode *operand_node = &ctx->file->ast.nodes[operand_id];

    switch (op) {
    case TOK_AMP: {
        return codegen_lvalue(ctx, operand_node);
    }

    case TOK_STAR: {
        LLVMValueRef address = codegen_expression(ctx, operand_node);

        if (!address) {
            return null;
        }

        LLVMTypeRef type = type_to_llvm_type(node->resolved_type_id);

        if (!type) {
            return null;
        }

        return LLVMBuildLoad2(ctx->llvm_builder, type, address, "");
    }

    case TOK_PLUS: {
        return codegen_expression(ctx, operand_node);
    }

    case TOK_MINUS: {
        LLVMValueRef expression = codegen_expression(ctx, operand_node);

        if (!expression) {
            return null;
        }

        return LLVMBuildNeg(ctx->llvm_builder, expression, "");
    }

    case TOK_BANG: {
        LLVMValueRef expression = codegen_expression(ctx, operand_node);

        if (!expression) {
            return null;
        }

        LLVMValueRef zero = LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_ctx), 0, 0);

        if (!zero) {
            return null;
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, expression, zero, "");
    }

    case TOK_TILDE: {
        LLVMValueRef expression = codegen_expression(ctx, operand_node);

        if (!expression) {
            return null;
        }

        return LLVMBuildNot(ctx->llvm_builder, expression, "");
    }

    default: {
        UNREACHABLE("Impossible operator %s", TOKEN_KIND_STRINGS[op]);
    }
    }
}

/* 
 *
 * ===========================================
 *
 *                 Binary Op
 *
 * ===========================================
 * 
 */

static LLVMValueRef codegen_binary_op(CodegenCtx *ctx, const AstNode *node) {
    TokenKind op = node->as.binary_op.op;

    const AstNode *lhs = &ctx->file->ast.nodes[node->as.binary_op.lhs];
    const AstNode *rhs = &ctx->file->ast.nodes[node->as.binary_op.rhs];

    switch (binary_op_kind_lut[op]) {
    case BINARY_OP_ASSIGN:
        return codegen_binary_op_assign(ctx, lhs, rhs, op);

    case BINARY_OP_ADDITIVE:
        return codegen_binary_op_additive(ctx, lhs, rhs, op);

    case BINARY_OP_MULTIPLICATIVE:
        return codegen_binary_op_multiplicative(ctx, lhs, rhs, op);

    case BINARY_OP_BITWISE:
        return codegen_binary_op_bitwise(ctx, lhs, rhs, op);

    case BINARY_OP_SHIFT:
        return codegen_binary_op_shift(ctx, lhs, rhs, op);

    case BINARY_OP_COMPARISON:
        return codegen_binary_op_comparison(ctx, lhs, rhs, op);

    case BINARY_OP_LOGICAL:
        return codegen_binary_op_logical(ctx, lhs, rhs, op);

    default:
        UNREACHABLE("Impossible case %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_assign(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    switch (op) {
    case TOK_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        LLVMValueRef value = codegen_expression(ctx, rhs);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_PLUS_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_PLUS,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_MINUS_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_MINUS,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_STAR_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_STAR,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_SLASH_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_SLASH,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_PERCENT_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_PERCENT,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_AMP_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_AMP,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_PIPE_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_PIPE,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_CARET_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_CARET,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_SHL_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_SHL,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    case TOK_SHR_EQ: {
        LLVMValueRef address = codegen_lvalue(ctx, lhs);
        assert(address != null);

        const AstNode node = {
            .kind = AST_BINARY_OP,

            .as.binary_op.op  = TOK_SHR,
            .as.binary_op.lhs = lhs->id,
            .as.binary_op.rhs = rhs->id,

            .resolved_type_id = lhs->resolved_type_id
        };

        LLVMValueRef value = codegen_expression(ctx, &node);
        assert(value != null);

        return LLVMBuildStore(ctx->llvm_builder, value, address);
    }

    default:
        UNREACHABLE("Impossible operator %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_additive(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->llvm_ctx);

    LLVMValueRef left  = codegen_expression(ctx, lhs);
    LLVMValueRef right = codegen_expression(ctx, rhs);

    bool lhs_is_pointer = type_is_kind(&ctx->ws->type_table, lhs->resolved_type_id, TYPE_POINTER);
    bool rhs_is_pointer = type_is_kind(&ctx->ws->type_table, rhs->resolved_type_id, TYPE_POINTER);

    switch (op) {
    case TOK_PLUS: {
        if (lhs_is_pointer || rhs_is_pointer) {
            LLVMValueRef pointer = lhs_is_pointer ? left : right;
            LLVMValueRef offset  = lhs_is_pointer ? right : left;

            LLVMValueRef indices[1] = {offset};

            return LLVMBuildGEP2(ctx->llvm_builder, i8_type, pointer, indices, 1, "");
        }

        return LLVMBuildAdd(ctx->llvm_builder, left, right, "");
    }

    case TOK_MINUS: {
        if (lhs_is_pointer && rhs_is_pointer) {
            return LLVMBuildPtrDiff2(ctx->llvm_builder, i8_type, left, right, "");
        }

        if (lhs_is_pointer || rhs_is_pointer) {
            LLVMValueRef pointer = lhs_is_pointer ? left : right;
            LLVMValueRef offset  = lhs_is_pointer ? right : left;

            offset = LLVMBuildNeg(ctx->llvm_builder, offset, "");

            LLVMValueRef indices[1] = {offset};

            return LLVMBuildGEP2(ctx->llvm_builder, i8_type, pointer, indices, 1, "");
        }

        return LLVMBuildSub(ctx->llvm_builder, left, right, "");
    }

    default:
        UNREACHABLE("Impossible operator %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_multiplicative(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    LLVMValueRef left  = codegen_expression(ctx, lhs);
    LLVMValueRef right = codegen_expression(ctx, rhs);

    bool lhs_is_float = type_is_float(ctx->ws->builtin_types, lhs->resolved_type_id);
    bool rhs_is_float = type_is_float(ctx->ws->builtin_types, rhs->resolved_type_id);

    bool lhs_is_signed = type_is_signed_int(ctx->ws->builtin_types, lhs->resolved_type_id);
    bool rhs_is_signed = type_is_signed_int(ctx->ws->builtin_types, rhs->resolved_type_id);

    switch (op) {
    case TOK_STAR: {
        return LLVMBuildMul(ctx->llvm_builder, left, right, "");
    }

    case TOK_SLASH: {
        if (lhs_is_float || rhs_is_float) {
            return LLVMBuildFDiv(ctx->llvm_builder, left, right, "");
        }

        if (lhs_is_signed || rhs_is_signed) {
            return LLVMBuildSDiv(ctx->llvm_builder, left, right, "");
        }

        return LLVMBuildUDiv(ctx->llvm_builder, left, right, "");
    }

    case TOK_PERCENT: {
        if (lhs_is_float || rhs_is_float) {
            return LLVMBuildFRem(ctx->llvm_builder, left, right, "");
        }

        if (lhs_is_signed || rhs_is_signed) {
            return LLVMBuildSRem(ctx->llvm_builder, left, right, "");
        }

        return LLVMBuildURem(ctx->llvm_builder, left, right, "");
    }

    default:
        UNREACHABLE("Impossible case: %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_bitwise(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    LLVMValueRef left  = codegen_expression(ctx, lhs);
    LLVMValueRef right = codegen_expression(ctx, rhs);

    switch (op) {
    case TOK_AMP: {
        return LLVMBuildAnd(ctx->llvm_builder, left, right, "");
    }

    case TOK_PIPE: {
        return LLVMBuildOr(ctx->llvm_builder, left, right, "");
    }

    case TOK_CARET: {
        return LLVMBuildXor(ctx->llvm_builder, left, right, "");
    }

    default:
        UNREACHABLE("Impossible case: %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_shift(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    LLVMValueRef left  = codegen_expression(ctx, lhs);
    LLVMValueRef right = codegen_expression(ctx, rhs);

    bool lhs_is_signed = type_is_signed_int(ctx->ws->builtin_types, lhs->resolved_type_id);

    switch (op) {
    case TOK_SHL: {
        return LLVMBuildShl(ctx->llvm_builder, left, right, "");
    }

    case TOK_SHR: {
        if (lhs_is_signed) {
            return LLVMBuildAShr(ctx->llvm_builder, left, right, "");
        }

        return LLVMBuildLShr(ctx->llvm_builder, left, right, "");
    }

    default:
        UNREACHABLE("Impossible case: %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_comparison(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    LLVMValueRef left  = codegen_expression(ctx, lhs);
    LLVMValueRef right = codegen_expression(ctx, rhs);

    // Resolver forces LHS and RHS to be of the same family of types. Meaning it has to be 2 floats, 2 uints, 2 ints
    bool lhs_is_float  = type_is_float(ctx->ws->builtin_types, lhs->resolved_type_id);
    bool lhs_is_signed = type_is_signed_int(ctx->ws->builtin_types, lhs->resolved_type_id);

    switch (op) {
    case TOK_EQ_EQ: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOEQ, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, left, right, "");
    }

    case TOK_BANG_EQ: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealONE, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, left, right, "");
    }

    case TOK_LT: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOLT, left, right, "");
        }

        if (lhs_is_signed) {
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSLT, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntULT, left, right, "");
    }

    case TOK_LT_EQ: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOLE, left, right, "");
        }

        if (lhs_is_signed) {
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSLE, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntULE, left, right, "");
    }

    case TOK_GT: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOGT, left, right, "");
        }

        if (lhs_is_signed) {
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSGT, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntUGT, left, right, "");
    }

    case TOK_GT_EQ: {
        if (lhs_is_float) {
            return LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOGE, left, right, "");
        }

        if (lhs_is_signed) {
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSGE, left, right, "");
        }

        return LLVMBuildICmp(ctx->llvm_builder, LLVMIntUGE, left, right, "");
    }

    default:
        UNREACHABLE("Impossible case: %s", TOKEN_KIND_STRINGS[op]);
    }
}

static LLVMValueRef codegen_binary_op_logical(CodegenCtx *ctx, const AstNode *lhs, const AstNode *rhs, TokenKind op) {
    assert(op == TOK_AMP_AMP || op == TOK_PIPE_PIPE);

    bool is_and_op = op == TOK_AMP_AMP;

    // ==== LHS ====
    LLVMValueRef left = codegen_expression(ctx, lhs);

    if (!left) {
        return null;
    }

    LLVMBasicBlockRef lhs_exit_block = LLVMGetInsertBlock(ctx->llvm_builder);
    LLVMBasicBlockRef rhs_block      = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "");
    LLVMBasicBlockRef merge_block    = LLVMAppendBasicBlockInContext(ctx->llvm_ctx, ctx->current_function, "");

    if (is_and_op) {
        LLVMBuildCondBr(ctx->llvm_builder, left, rhs_block, merge_block);
    } else {
        LLVMBuildCondBr(ctx->llvm_builder, left, merge_block, rhs_block);
    }

    // ==== RHS ====
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, rhs_block);
    ctx->current_block = rhs_block;

    LLVMValueRef right = codegen_expression(ctx, rhs);

    if (!right) {
        return null;
    }

    LLVMBasicBlockRef rhs_exit_block = LLVMGetInsertBlock(ctx->llvm_builder);

    LLVMBuildBr(ctx->llvm_builder, merge_block);

    LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_block);
    ctx->current_block = merge_block;

    // ==== Merge ====
    LLVMTypeRef bool_type = LLVMInt1TypeInContext(ctx->llvm_ctx);

    LLVMValueRef phi = LLVMBuildPhi(ctx->llvm_builder, bool_type, "phi");

    /*
     * If it was '&&' we use false, because if we came from the LHS it should not  
     * evaluate the RHS, and it means the expression failed.
     *
     * If it was '||' we use true, because if we came from the LHS, it means the expression
     * succeeded, and there is no need to evaluate the RHS.
     *
     */
    LLVMValueRef short_circuit_value = LLVMConstInt(bool_type, is_and_op ? false : true, 0);

    LLVMValueRef incoming_values[2]      = {short_circuit_value, right};
    LLVMBasicBlockRef incoming_blocks[2] = {lhs_exit_block, rhs_exit_block};

    LLVMAddIncoming(phi, incoming_values, incoming_blocks, 2);

    return phi;
}
