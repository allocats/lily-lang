#include "symbols/table/table.h"
#include "symbols/scope/types.h"
#include "symbols/table/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

void symbol_table_init(SymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate) {
    assert(table != null);
    assert(symbol_count_estimate > 0);
    assert(scope_count_estimate > 0);

    arena_init(&table -> scope_data_arena, ARENA_KB(2), ALIGN_DEFAULT);
    arena_init(&table -> symbol_data_arena, ARENA_KB(2), ALIGN_DEFAULT);

    u64 scope_array_size  = sizeof(Scope)  * scope_count_estimate;
    u64 symbol_array_size = sizeof(Symbol) * symbol_count_estimate;

    table -> scopes = malloc(scope_array_size);
    table -> scope_count = 1; // ScopeId 0 is reserved for the compiler
    table -> scope_capacity = scope_count_estimate;

    table -> symbols = malloc(symbol_array_size);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbol_count_estimate;

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
    arena_destroy(&table -> symbol_data_arena);

    free(table -> scopes);
    free(table -> symbols);
}

/*
 *
 * Symbol and Scope creation
 *
 */

SymbolId symbol_table_create_symbol(SymbolTable* table) {
    if (UNLIKELY(table -> symbol_count >= table -> symbol_capacity)) {
        u64 old_size = table -> symbol_capacity * sizeof(Symbol);
        u64 new_size = old_size * 2;

        table -> symbols = realloc(table -> symbols, new_size);
        table -> symbol_capacity *= 2;
    }

    return table -> symbol_count++;
}

ScopeId symbol_table_create_scope(SymbolTable* table) {
    if (UNLIKELY(table -> scope_count >= table -> scope_capacity)) {
        u64 old_size = table -> scope_capacity * sizeof(Scope);
        u64 new_size = old_size * 2;

        table -> scopes = realloc(table -> scopes, new_size);
        table -> scope_capacity *= 2;
    }

    return table -> scope_count++;
}
