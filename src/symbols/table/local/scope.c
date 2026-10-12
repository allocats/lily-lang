#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "symbols/table/common_types.h"
#include "symbols/table/local/table.h"
#include "symbols/table/local/types.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <string.h>

extern Driver driver;

static void local_scope_resize(LocalSymbolTable *table, Scope *scope);

void local_scope_init(LocalSymbolTable *table, u32 index) {
    Scope *scope = &table->scopes[index];

    scope->buckets = arena_alloc(&table->scope_data_arena, sizeof(ScopeBucket) * scope_init_capacity);
    scope->entries = arena_alloc(&table->scope_data_arena, sizeof(SymbolId) * scope_init_capacity);

    scope->count    = 0;
    scope->capacity = scope_init_capacity;

    scope->parent = LOCAL_SCOPE_ID_NONE;

    scope->resize_threshold_as_u32 = (u32)(scope_init_capacity * scope_load_factor);

    memset(scope->entries, U8_MAX, sizeof(SymbolId) * scope_init_capacity);
}

ScopeId local_scope_enter(LocalSymbolTable *table, ScopeId parent) {
    ScopeId new_scope_id                                   = local_symbol_table_create_scope(table);
    table->scopes[local_scope_get_id(new_scope_id)].parent = parent;
    return new_scope_id;
}

ScopeId local_scope_exit(LocalSymbolTable *table, ScopeId id) {
    Scope *scope = &table->scopes[local_scope_get_id(id)];
    return scope->parent;
}

SymbolId local_scope_intern(LocalSymbolTable *table, ScopeId scope_id, StringId global_string_id) {
    ScopeId local_scope_id = local_scope_get_id(scope_id);
    Scope *scope           = &table->scopes[local_scope_id];

    if (UNLIKELY(scope->count >= scope->resize_threshold_as_u32)) {
        local_scope_resize(table, scope);
    }

    u32 hash  = hash_u32(global_string_id);
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (local_symbol_get_id(scope->entries[index]) != LOCAL_SYMBOL_ID_NONE) {
        // ScopeBucket bucket = scope -> buckets[index];
        //
        // if (bucket.hash == hash) {
        //     if (bucket.string_id == global_string_id) {
        //         return scope -> entries[index];
        //     }
        // }

        index = (index + 1) & mask;
    }

    SymbolId id = local_symbol_table_create_symbol(table);

    scope->buckets[index].string_id = global_string_id;
    scope->buckets[index].hash      = hash;
    scope->entries[index]           = id;
    scope->count += 1;

    Symbol *symbol = &table->symbols[local_symbol_get_id(id)];

    symbol->flags   = AST_FLAG_NONE;
    symbol->hash    = hash;
    symbol->name_id = global_string_id;
    symbol->state   = RESOLVE_UNRESOLVED;

    return id;
}

SymbolId local_scope_lookup(LocalSymbolTable *table, ScopeId scope_id, StringId global_string_id) {
    ScopeId local_scope_id = local_scope_get_id(scope_id);
    Scope *scope           = &table->scopes[local_scope_id];

    u32 hash  = hash_u32(global_string_id);
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (local_symbol_get_id(scope->entries[index]) != LOCAL_SYMBOL_ID_NONE) {
        ScopeBucket bucket = scope->buckets[index];

        if (bucket.hash == hash) {
            if (bucket.string_id == global_string_id) {
                return scope->entries[index];
            }
        }

        index = (index + 1) & mask;
    }

    return LOCAL_SYMBOL_ID_NONE;
}

Symbol *local_symbol_get(SymbolId id) {
    SymbolId local_id = local_symbol_get_id(id);
    u8 table_id       = local_symbol_get_table_id(id);

    LocalSymbolTable *table = &driver.local_symbol_tables[table_id];

    return &table->symbols[local_id];
}

/*
 *
 * scope reallocations
 *
 */

static void local_scope_resize(LocalSymbolTable *table, Scope *scope) {
    u32 old_capacity = scope->capacity;
    u32 new_capacity = old_capacity * 2;

    ScopeBucket *new_buckets = arena_alloc(&table->scope_data_arena, new_capacity * sizeof(ScopeBucket));
    SymbolId *new_entries    = arena_alloc(&table->scope_data_arena, new_capacity * sizeof(SymbolId));

    memset(new_entries, U8_MAX, new_capacity * sizeof(SymbolId));

    u32 mask = new_capacity - 1;

    for (u32 i = 0; i < old_capacity; i++) {
        SymbolId id = scope->entries[i];

        if (local_symbol_get_id(id) == LOCAL_SYMBOL_ID_NONE)
            continue;

        ScopeBucket bucket = scope->buckets[i];

        u32 index = bucket.hash & mask;

        while (local_symbol_get_id(new_entries[index]) != LOCAL_SYMBOL_ID_NONE) {
            index = (index + 1) & mask;
        }

        new_entries[index] = id;
        new_buckets[index] = bucket;
    }

    scope->entries  = new_entries;
    scope->buckets  = new_buckets;
    scope->capacity = new_capacity;

    scope->resize_threshold_as_u32 = (u32)(new_capacity * scope_load_factor);
}
