#include "types/table/table.h"
#include "types/table/types.h"
#include "utils/types.h"

#include <assert.h>
#include <pthread.h>

/*
 *
 * default initial values for sizing
 *
 */
static constexpr u32 init_capacity = 128;
static constexpr u32 init_resize_threshold = (u64)(init_capacity * 0.75f);

/*
 *
 * Initial array sizes
 *
 */
static constexpr u64 entry_size = sizeof(Type) * init_capacity;
static constexpr u64 bucket_size = sizeof(TypeBucket) * init_capacity;

void type_table_init(TypeTable *table) {
    assert(table != null);

    arena_init(&table -> gpa, ARENA_KB(1), ALIGN_DEFAULT);
    arena_init(&table -> entry_arena, entry_size, ALIGN_DEFAULT);
    arena_init(&table -> nominal_arena, bucket_size, ALIGN_DEFAULT);
    arena_init(&table -> structural_arena, bucket_size, ALIGN_DEFAULT);

    table -> entries = arena_calloc(&table -> entry_arena, entry_size);
    table -> nominal_buckets = arena_alloc(&table -> nominal_arena, bucket_size);
    table -> structural_buckets = arena_alloc(&table -> structural_arena, bucket_size);

    table -> entry_count = 0;
    table -> entry_capacity = init_capacity;
    table -> entry_resize_threshold_as_u32 = init_resize_threshold;
    
    table -> nominal_count = 0;
    table -> nominal_capacity = init_capacity;
    table -> nominal_resize_threshold_as_u32 = init_resize_threshold;
    
    table -> structural_count = 0;
    table -> structural_capacity = init_capacity;
    table -> structural_resize_threshold_as_u32 = init_resize_threshold;

    arena_memset(table -> nominal_buckets, U8_MAX, bucket_size);
    arena_memset(table -> structural_buckets, U8_MAX, bucket_size);

    // pthread_rwlock_init(&table -> mutex, null);
}

void type_table_destroy(TypeTable* table) {
    // pthread_rwlock_destroy(&table -> mutex);

    arena_destroy(&table -> gpa);
    arena_destroy(&table -> entry_arena);
    arena_destroy(&table -> nominal_arena);
    arena_destroy(&table -> structural_arena);
}
