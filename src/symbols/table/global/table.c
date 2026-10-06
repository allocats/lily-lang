#include "symbols/table/global/table.h"
#include "symbols/table/local/table.h"
#include "driver/types.h"
#include "symbols/table/local/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

extern Driver driver;

void global_symbol_table_init(GlobalSymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate) {
    assert(table != null);
    assert(symbol_count_estimate > 0);
    assert(scope_count_estimate > 0);

    arena_init(&table -> scope_data_arena, ARENA_KB(16), ALIGN_DEFAULT);
    arena_init(&table -> symbol_data_arena, ARENA_KB(16), ALIGN_DEFAULT);

    u64 scope_array_size  = sizeof(Scope)  * scope_count_estimate;
    u64 symbol_array_size = sizeof(Symbol) * symbol_count_estimate;

    table -> scopes = malloc(scope_array_size);
    table -> scope_count = 1; // ScopeId 0 is reserved for the compiler
    table -> scope_capacity = scope_count_estimate;

    table -> symbols = malloc(symbol_array_size);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbol_count_estimate;

    for (u32 i = 0; i < scope_count_estimate; i += 1) {
        global_scope_init(table, i);
    }

    debug_log(
        "Created global symbol table (%p) with the following stats.\n"
        "  Scope count: %u\n  Scope size: %lu\n  Symbol count: %u\n  Symbol size: %lu",
        table,
        scope_count_estimate,
        scope_array_size,
        symbol_count_estimate,
        symbol_array_size
    );

    for (u8 i = 0; i < driver.local_symbol_table_count; i += 1) {
        LocalSymbolTable* local_table = &driver.local_symbol_tables[i];

        local_symbol_table_destroy(local_table);
    }

    free(driver.local_symbol_tables);
}

void global_symbol_table_destroy(GlobalSymbolTable* table) {
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

SymbolId global_symbol_table_create_symbol(GlobalSymbolTable* table) {
    if (UNLIKELY(table -> symbol_count >= table -> symbol_capacity)) {
        u64 old_size = table -> symbol_capacity * sizeof(Symbol);
        u64 new_size = old_size * 2;

        table -> symbols = realloc(table -> symbols, new_size);
        table -> symbol_capacity *= 2;
    }

    return table -> symbol_count++;
}

ScopeId global_symbol_table_create_scope(GlobalSymbolTable* table) {
    if (UNLIKELY(table -> scope_count >= table -> scope_capacity)) {
        u64 old_size = table -> scope_capacity * sizeof(Scope);
        u64 new_size = old_size * 2;

        table -> scopes = realloc(table -> scopes, new_size);
        table -> scope_capacity *= 2;
    }

    return table -> scope_count++;
}
