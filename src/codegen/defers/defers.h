#ifndef LILY_CODEGEN_DEFERS_H
#define LILY_CODEGEN_DEFERS_H

#include "codegen/types.h"

void defer_context_enter(CodegenCtx *ctx);
void defer_context_exit(CodegenCtx *ctx);

void defer_context_append(CodegenCtx *ctx, const AstNodeId id);
bool defer_context_emit_current_stack(CodegenCtx *ctx);
bool defer_context_emit_all_stacks(CodegenCtx *ctx);
bool defer_context_emit_stacks_until_stack(CodegenCtx *ctx, DeferStack *stack);

#endif // !LILY_CODEGEN_DEFERS_H
