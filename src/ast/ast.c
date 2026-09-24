#include "ast/ast.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>

static constexpr u32 init_cap = 64;

void ast_init(Ast* ast) {
    assert(ast != null);

    arena_init(&ast -> gpa, ARENA_KB(2), ALIGN_DEFAULT);
    arena_init(&ast -> nodes_arena, ARENA_KB(8), ALIGN_DEFAULT);

    ast -> nodes = arena_calloc(&ast -> nodes_arena, sizeof(AstNode) * init_cap);
    ast -> count = 0;
    ast -> capacity = init_cap;

    ast -> total_declaration_count = 0;
    ast -> top_level_declaration_count = 0;

    debug_log("Allocated %lu bytes for nodes", sizeof(AstNode) * init_cap);
}

inline void ast_destroy(Ast* ast) {
    assert(ast != null);
    
    arena_destroy(&ast -> gpa);
    arena_destroy(&ast -> nodes_arena);
}

AstNodeId ast_alloc_node(Ast* ast) {
    assert(ast != null);
    assert(ast -> capacity != 0);

    if (UNLIKELY(ast -> count >= ast -> capacity)) {
        u64 old_size = sizeof(AstNode) * ast -> capacity;
        u64 new_size = old_size * 2;

        ast -> nodes = arena_realloc(&ast -> nodes_arena, ast -> nodes, old_size, new_size);
        ast -> capacity *= 2;

        debug_log("Reallocated nodes from %lu to %lu bytes", old_size, new_size);
    }

    return ast -> count++;
}
