#include "utils/wrappers.h"
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
    assert(size != 0 && "Received zero in allocation request");

    void *ptr = malloc(size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(2)]]
void *lily_aligned_alloc(usize alignment, usize size) {
    assert(size != 0 && "Received zero in allocation request");

    void *ptr = aligned_alloc(alignment, size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(2)]]
void *lily_realloc(void *ptr, usize size) {
    assert(ptr != null && "Received null in reallocation request");
    assert(size != 0 && "Received zero in reallocation request");

    ptr = realloc(ptr, size);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(size);
    }

    return ptr;
}

[[gnu::alloc_size(1, 2)]]
void *lily_calloc(usize number_of_members, usize size_of_member) {
    assert(number_of_members != 0 && "Received zero in reallocation request");
    assert(size_of_member != 0 && "Received zero in reallocation request");

    void *ptr = calloc(number_of_members, size_of_member);

    if (UNLIKELY(ptr == null)) {
        out_of_memory_abort(number_of_members * size_of_member);
    }

    return ptr;
}
