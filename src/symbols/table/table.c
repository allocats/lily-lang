#include "symbols/table/table.h"
#include "symbols/scope/types.h"
#include "symbols/table/types.h"
#include "utils/debug.h"

#include <assert.h>
#include <pthread.h>

static constexpr f64 load_factor = 0.75f;

void symbol_table_init(SymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate) {
    assert(table != null);
    assert(symbol_count_estimate > 0);
    assert(scope_count_estimate > 0);

    arena_init(&table -> scope_data_arena, ARENA_KB(2), ALIGN_DEFAULT);
    arena_init(&table -> symbol_data_arena, ARENA_KB(2), ALIGN_DEFAULT);

    u64 scope_array_size  = sizeof(Scope)  * scope_count_estimate;
    u64 symbol_array_size = sizeof(Symbol) * symbol_count_estimate;

    arena_init(&table -> scope_array_arena, scope_array_size, ALIGN_DEFAULT);
    arena_init(&table -> symbol_array_arena, symbol_array_size, ALIGN_DEFAULT);

    table -> scopes = arena_alloc(&table -> scope_array_arena, scope_array_size);
    table -> scope_count = 1; // ScopeId 0 is reserved for the compiler
    table -> scope_capacity = scope_count_estimate;
    table -> scope_resize_threshold_as_u32 = (u32)(scope_count_estimate * load_factor);

    table -> symbols = arena_alloc(&table -> symbol_array_arena, symbol_array_size);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbol_count_estimate;
    table -> symbol_resize_threshold_as_u32 = (u32)(symbol_count_estimate * load_factor);

    // pthread_rwlock_init(&table -> mutex, null);

    debug_log(
        "Created symbol table (%p) with the following stats.\n"
        "  Scope count: %u\n  Scope size: %lu\n  Symbol count: %u\n  Symbol size: %lu",
        table,
        scope_count_estimate,
        scope_array_size,
        symbol_count_estimate,
        symbol_array_size
    );
}

void symbol_table_destory(SymbolTable* table) {
    // pthread_rwlock_destroy(&table -> mutex);
    
    arena_destroy(&table -> scope_data_arena);
    arena_destroy(&table -> scope_array_arena);
    arena_destroy(&table -> symbol_data_arena);
    arena_destroy(&table -> symbol_array_arena);
}
