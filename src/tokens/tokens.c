#include "tokens/tokens.h"
#include "tokens/types.h"
#include "utils/debug.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

/*
 *
 * helper functions!
 *
 */

SourceLocation token_get_source_location(File* file, Token* token) {
    usize bytes = token - file -> tokens.items;

    u32 index = bytes / sizeof(Token);

    printf("index = %u\n", index);
    printf("count = %lu\n", file -> tokens.count);

    assert(index < file -> tokens.count);

    return file -> source_locations.items[index];
}

/*
 *
 * extracting literals from the buffer
 *
 */

i64 get_integer_literal_from_token(File* file, Token token) {
    char* start = file -> buffer.ptr + token.start;
    char* end = start + token.length;

    char putback = *end; 

    *end = 0;

    i64 value = strtoll(start, null, 10);

    *end = putback;

    return value;
}

f64 get_floating_literal_from_token(File* file, Token token) {
    char* start = file -> buffer.ptr + token.start;
    char* end = start + token.length;

    char putback = *end; 

    *end = 0;

    f64 value = strtod(start, null);

    *end = putback;

    return value;
}

char get_character_literal_from_token(File* file, Token token) {
    return *(file -> buffer.ptr + token.start + 1);
}
