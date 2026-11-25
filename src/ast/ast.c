#include "ast/ast.h"
#include "ast/kinds.h"
#include "ids.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <string.h>

static constexpr u32 init_cap = 64;

/*
 *
 * creation and deletion
 *
 */

void ast_init(Ast *ast) {
    assert(ast != null);

    arena_init(&ast->gpa, ARENA_KB(2), ALIGN_DEFAULT);

    ast->nodes    = lily_aligned_alloc(alignof(AstNode), init_cap * sizeof(AstNode));
    ast->count    = 0;
    ast->capacity = init_cap;

    ast->total_declaration_count     = 0;
    ast->top_level_declaration_count = 0;

    debug_log("Allocated %lu bytes for nodes", sizeof(AstNode) * init_cap);
}

inline void ast_destroy(Ast *ast) {
    assert(ast != null);

    lily_free(ast->nodes);

    arena_destroy(&ast->gpa);
}

/*
 *
 * grow AST nodes array if need be and return the AstNodeId
 *
 */

AstNodeId ast_alloc_node(Ast *ast) {
    assert(ast != null);
    assert(ast->capacity != 0);

    if (UNLIKELY(ast->count >= ast->capacity)) {
        u64 old_size = sizeof(AstNode) * ast->capacity;
        u64 new_size = old_size * 2;

        AstNode *new_nodes = lily_aligned_alloc(alignof(AstNode), new_size);
        memcpy(new_nodes, ast->nodes, old_size);
        lily_free(ast->nodes);

        ast->nodes = new_nodes;
        ast->capacity *= 2;

        debug_log("Reallocated nodes from %lu to %lu bytes", old_size, new_size);
    }

    return ast->count++;
}

inline AstNode *ast_get_node(Ast *ast, AstNodeId id) {
    assert(ast != null);
    assert(id < ast->count);
    return &ast->nodes[id];
}

/*
 *
 * AST ID list helpers
 *
 */

void ast_id_list_init(Ast *ast, AstNodeIdList *list, u32 count) {
    assert(count != 0);

    u64 size = sizeof(AstNodeId) * count;

    list->ids      = arena_alloc(&ast->gpa, size);
    list->count    = 0;
    list->capacity = count;

    arena_memset(list->ids, U8_MAX, size);
}

void ast_append_id_list(Ast *ast, AstNodeIdList *list, AstNodeId id) {
    assert(list->ids != null);

    if (UNLIKELY(list->count >= list->capacity)) {
        u64 old_size = list->capacity * sizeof(AstNodeId);
        u64 new_size = old_size * 2;

        list->ids = arena_realloc(&ast->gpa, list->ids, old_size, new_size);
        list->capacity *= 2;
    }

    list->ids[list->count++] = id;
}
