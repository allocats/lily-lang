#include "codegen/defers/defers.h"
#include "ast/types.h"
#include "codegen/expressions/expressions.h"
#include "codegen/types.h"
#include "ids.h"

static bool emit_stack(CodegenCtx *ctx, DeferStack *stack);

void defer_context_enter(CodegenCtx *ctx) {
    DeferStack *new_stack = arena_alloc(&ctx->defer_ctx.arena, sizeof(DeferStack));

    new_stack->ids      = arena_alloc(&ctx->defer_ctx.arena, sizeof(AstNodeId) * 4);
    new_stack->count    = 0;
    new_stack->capacity = 4;
    new_stack->previous = ctx->defer_ctx.stack;

    ctx->defer_ctx.stack = new_stack;
}

void defer_context_exit(CodegenCtx *ctx) {
    ctx->defer_ctx.stack = ctx->defer_ctx.stack->previous;
}

void defer_context_append(CodegenCtx *ctx, const AstNodeId id) {
    DeferStack *stack = ctx->defer_ctx.stack;

    if (stack->count >= stack->capacity) {
        u64 old_size = sizeof(AstNodeId) * stack->capacity;
        u64 new_size = old_size * 2;

        stack->ids = arena_realloc(&ctx->defer_ctx.arena, stack->ids, old_size, new_size);
        stack->capacity *= 2;
    }

    stack->ids[stack->count++] = id;
}

bool defer_context_emit_current_stack(CodegenCtx *ctx) {
    DeferStack *stack = ctx->defer_ctx.stack;

    return emit_stack(ctx, stack);
}

bool defer_context_emit_all_stacks(CodegenCtx *ctx) {
    return defer_context_emit_stacks_until_stack(ctx, null);
}

bool defer_context_emit_stacks_until_stack(CodegenCtx *ctx, DeferStack *stack) {
    for (DeferStack *current = ctx->defer_ctx.stack; current != stack; current = current->previous) {
        if (!emit_stack(ctx, current)) {
            return false;
        }
    }

    return true;
}

static bool emit_stack(CodegenCtx *ctx, DeferStack *stack) {
    for (u32 i = stack->count; i > 0; i -= 1) {
        const AstNodeId id  = stack->ids[i - 1];
        const AstNode *node = &ctx->file->ast.nodes[id];

        if (!codegen_expression(ctx, node)) {
            return false;
        }
    }

    return true;
}
