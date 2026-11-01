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

    LLVMValueRef *arguments = lily_alloc(argument_count * sizeof(LLVMValueRef));

    for (u32 i = 0; i < argument_count; i += 1) {
        const AstNodeId argument_id  = node->as.func_call.arguments.ids[i];
        const AstNode *argument_node = &ctx->file->ast.nodes[argument_id];

        LLVMValueRef argument = codegen_expression(ctx, argument_node);
        assert(argument != null);

        arguments[i] = argument;
    }

    LLVMValueRef call = LLVMBuildCall2(ctx->llvm_builder, function_type, function, arguments, argument_count, "");

    free(arguments);

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

    LLVMValueRef *arguments = lily_alloc(argument_count * sizeof(LLVMValueRef));

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

    free(arguments);

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

            // at end or early break, no point in parsing rest of the string
            if (c == '0') {
                *cursor++ = '\0';
                break;
            }

            switch (c) {
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

    LLVMValueRef string = LLVMBuildGlobalString(ctx->llvm_builder, copy, name);

    ctx->string_map[id] = string;

    free(copy);

    return string;
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

        LLVMValueRef zero = LLVMConstInt(type_to_llvm_type(ctx->ws->builtin_types.type_i8), 0, 0);

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
