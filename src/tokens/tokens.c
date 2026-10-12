#include "tokens/tokens.h"
#include "tokens/types.h"
#include "utils/debug.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <stdlib.h>

void token_array_init(TokenArray *array, u64 capacity) {
    assert(array != null);
    assert(((capacity & (capacity - 1)) == 0) && "capacity is not power of two");

    u64 size = sizeof(Token) * capacity;

    array->items    = lily_alloc(size);
    array->count    = 0;
    array->capacity = capacity;

    debug_log("Created new token array with capacty %lu", capacity);
}

inline void token_array_destroy(TokenArray *array) {
    assert(array);

    debug_log("Deleting TokenArray: Count=%lu Capacity=%lu", array->count, array->capacity);

    free(array->items);
}

/*
 *
 * extracting literals from the buffer
 *
 */

i64 get_integer_literal_from_token(File *file, Token token) {
    char *start = file->buffer.ptr + token.start;
    char *end   = start + token.length;

    char putback = *end;

    *end = 0;

    i64 value = strtoll(start, null, 10);

    *end = putback;

    return value;
}

f64 get_floating_literal_from_token(File *file, Token token) {
    char *start = file->buffer.ptr + token.start;
    char *end   = start + token.length;

    char putback = *end;

    *end = 0;

    f64 value = strtod(start, null);

    *end = putback;

    return value;
}

char get_character_literal_from_token(File *file, Token token) {
    return *(file->buffer.ptr + token.start + 1);
}
