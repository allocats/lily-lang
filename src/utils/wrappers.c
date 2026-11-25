#include "utils/wrappers.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

[[noreturn]]
static void out_of_memory_abort(usize size) {
    fprintf(stderr, "out of memory! (requested %zu bytes)\n", size);
    abort();
}

[[gnu::malloc, gnu::alloc_size(1)]]
void *lily_alloc(usize size) {
    void *ptr = malloc(size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(2)]]
void *lily_aligned_alloc(usize alignment, usize size) {
    void *ptr = aligned_alloc(alignment, size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(2)]]
void *lily_realloc(void *ptr, usize size) {
    ptr = realloc(ptr, size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(1, 2)]]
void *lily_calloc(usize number_of_members, usize size_of_member) {
    void *ptr = calloc(number_of_members, size_of_member);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(number_of_members * size_of_member);
    }

    return ptr;
}

void lily_free(void *ptr) {
    if (ptr == null) {
        return;
    }

    debug_log("Freeing %p", ptr);

    free(ptr);
}
