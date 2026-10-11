#include "ids.h"
#include "ast/types.h"
#include "ast/types.h"
#include "symbols/table/common_types.h"
#include "symbols/table/local/table.h"
#include "symbols/table/local/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdlib.h>

void local_symbol_table_init(LocalSymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate, u8 id) {
    assert(table != null);
    assert(symbol_count_estimate > 0);
    assert(scope_count_estimate > 0);

    arena_init(&table -> scope_data_arena, ARENA_KB(8), ALIGN_DEFAULT);

    u64 scope_array_size  = sizeof(Scope)  * scope_count_estimate;
    u64 symbol_array_size = sizeof(Symbol) * symbol_count_estimate;

    table -> scopes = malloc(scope_array_size);
    table -> scope_count = 1; // ScopeId 0 is reserved for the compiler
    table -> scope_capacity = scope_count_estimate;

    table -> symbols = malloc(symbol_array_size);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbol_count_estimate;

    table -> id = id;

    for (u32 i = 0; i < scope_count_estimate; i += 1) {
        local_scope_init(table, i);
    }

    debug_log(
        "Created local symbol table (%p) with the following stats.\n"
        "  Scope count: %u\n  Scope size: %lu\n  Symbol count: %u\n  Symbol size: %lu",
        table,
        scope_count_estimate,
        scope_array_size,
        symbol_count_estimate,
        symbol_array_size
    );
}

void local_symbol_table_destroy(LocalSymbolTable* table) {
    arena_destroy(&table -> scope_data_arena);

    free(table -> scopes);
    free(table -> symbols);
}

/*
 *
 * Symbol and Scope creation
 *
 */

SymbolId local_symbol_table_create_symbol(LocalSymbolTable* table) {
    if (UNLIKELY(table -> symbol_count >= table -> symbol_capacity)) {
        u64 old_size = table -> symbol_capacity * sizeof(Symbol);
        u64 new_size = old_size * 2;

        table -> symbols = realloc(table -> symbols, new_size);
        table -> symbol_capacity *= 2;
    }

    return local_symbol_create_id(table -> id, table -> symbol_count++);
}

ScopeId local_symbol_table_create_scope(LocalSymbolTable* table) {
    if (UNLIKELY(table -> scope_count >= table -> scope_capacity)) {
        u64 old_size = table -> scope_capacity * sizeof(Scope);
        u64 new_size = old_size * 2;

        table -> scopes = realloc(table -> scopes, new_size);
        table -> scope_capacity *= 2;

        for (u32 i = table -> scope_count; i < table -> scope_capacity; i += 1) {
            local_scope_init(table, i);
        }
    }

    return local_scope_create_id(table -> id, table -> scope_count++);
}

SymbolId local_table_lookup(LocalSymbolTable* table, ScopeId scope_id, StringId global_string_id) {
    while (local_scope_get_id(scope_id) != LOCAL_SCOPE_ID_NONE) {
        SymbolId symbol_id = local_scope_lookup(table, scope_id, global_string_id);

        if (local_symbol_get_id(symbol_id) != LOCAL_SYMBOL_ID_NONE) {
            return symbol_id;
        }

        Scope* scope = &table -> scopes[local_scope_get_id(scope_id)];

        scope_id = scope -> parent;
    }

    return LOCAL_SYMBOL_ID_NONE;
}

/*
 *
 * ID Helpers
 *
 */

/*
 * Scope IDs
 */
inline u8 local_scope_get_table_id(ScopeId id) {
    return (id >> LOCAL_SCOPE_ID_BIT_OFFSET) & U8_MAX;
}

inline ScopeId local_scope_get_id(ScopeId id) {
    return id & LOCAL_SCOPE_ID_NONE;
}

inline SymbolId local_scope_create_id(u8 table_id, ScopeId local_scope_id) {
    return (table_id << LOCAL_SCOPE_ID_BIT_OFFSET) | local_scope_id;
}

/*
 * Symbol IDs
 */

inline u8 local_symbol_get_table_id(SymbolId id) {
    return (id >> LOCAL_SYMBOL_ID_BIT_OFFSET) & U8_MAX;
}

inline SymbolId local_symbol_get_id(SymbolId id) {
    return id & LOCAL_SYMBOL_ID_NONE;
}

inline SymbolId local_symbol_create_id(u8 table_id, SymbolId local_symbol_id) {
    return (table_id << LOCAL_SYMBOL_ID_BIT_OFFSET) | local_symbol_id;
}
