#include "tokens/tokens.h"
#include "tokens/types.h"
#include "utils/debug.h"

#include <assert.h>

static constexpr u32 init_cap = 128;
static constexpr u32 init_size = init_cap * sizeof(Token);
static constexpr u32 init_kb = (init_cap * sizeof(Token)) / ARENA_KB(1);

static_assert(init_cap > 0);
static_assert(init_cap % 2 == 0);

void token_array_init(TokenArray* array) {
    assert(array != null);

    arena_init(&array -> arena, ARENA_KB(init_kb), ALIGN_DEFAULT);

    array -> items = arena_alloc(&array -> arena, init_size);
    array -> count = 0;
    array -> capacity = init_cap;

    debug_log("Created new token array");
}

void source_location_array_init(SourceLocationArray* array) {
    assert(array != null);

    arena_init(&array -> arena, ARENA_KB(init_kb), ALIGN_DEFAULT);

    array -> items = arena_alloc(&array -> arena, init_size);
    array -> count = 0;
    array -> capacity = init_cap;

    debug_log("Created new source location array");
}

inline void token_array_destroy(TokenArray* array) {
    assert(array);
    arena_destroy(&array -> arena);
}

inline void source_location_array_destroy(SourceLocationArray* array) {
    assert(array);
    arena_destroy(&array -> arena);
}
