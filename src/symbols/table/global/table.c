#include "symbols/table/global/table.h"
#include "symbols/symbol/types.h"
#include "symbols/table/global/types.h"
#include "symbols/table/local/table.h"
#include "driver/types.h"
#include "symbols/table/local/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

extern Driver driver;

static GlobalSymbolTableRemapper remapper = {0};

void global_symbol_table_init(GlobalSymbolTable* table) {
    assert(table != null);

    remapper.scope_ids = malloc(driver.local_symbol_table_count * sizeof(ScopeId*));

    u32 symbol_count = 0;
    u32 scope_count  = 0;

    for (u8 i = 0; i < driver.local_symbol_table_count; i += 1) {
        LocalSymbolTable* local_table = &driver.local_symbol_tables[i];

        symbol_count += local_table -> symbol_count;
        scope_count  += local_table -> scope_count;

        remapper.scope_ids[i] = malloc(sizeof(ScopeId) * local_table -> scope_count);
    }

    symbol_count = NEXT_POWER_OF_TWO(symbol_count);
    scope_count  = NEXT_POWER_OF_TWO(scope_count);

    arena_init(&table -> scope_data_arena, ARENA_KB(16), ALIGN_DEFAULT);
    arena_init(&table -> symbol_data_arena, ARENA_KB(16), ALIGN_DEFAULT);

    u64 scope_array_size  = sizeof(Scope)  * scope_count;
    u64 symbol_array_size = sizeof(Symbol) * symbol_count;

    table -> scopes = malloc(scope_array_size);
    table -> scope_count = 1; // ScopeId 0 is reserved for the compiler
    table -> scope_capacity = scope_count;

    table -> symbols = malloc(symbol_array_size);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbol_count;

    for (u32 i = 0; i < scope_count; i += 1) {
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

        for (ScopeId n = 1; n < local_table -> scope_count; n += 1) {
            Scope* l_scope = &local_table -> scopes[n];

            ScopeId g_scope_id = global_symbol_table_create_scope(table);

            for (u32 j = 0; j < l_scope -> capacity; j += 1) {
                if (local_symbol_get_id(l_scope -> entries[j]) == LOCAL_SYMBOL_ID_NONE) {
                    continue;
                }

                Symbol* symbol = &local_table -> symbols[local_symbol_get_id(l_scope -> entries[j])];

                global_scope_intern_from_local_symbol(table, g_scope_id, symbol);
            }

            remapper.scope_ids[local_table -> id][n] = g_scope_id;
        }

        local_symbol_table_destroy(local_table);
    }

    free(driver.local_symbol_tables);
}

void global_symbol_table_destroy(GlobalSymbolTable* table) {
    arena_destroy(&table -> scope_data_arena);
    arena_destroy(&table -> symbol_data_arena);

    free(table -> scopes);
    free(table -> symbols);

    for (u8 i = 0; i < driver.local_symbol_table_count; i += 1) {
        free(remapper.scope_ids[i]);
    }

    free(remapper.scope_ids);
}

ScopeId global_scope_from_local_scope(ScopeId id) {
    return remapper.scope_ids[local_scope_get_table_id(id)][local_scope_get_id(id)];
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
