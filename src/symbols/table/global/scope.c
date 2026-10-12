#include "ast/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "symbols/table/common_types.h"
#include "symbols/table/global/table.h"
#include "symbols/table/global/types.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void global_scope_resize(GlobalSymbolTable *table, Scope *scope);

void global_scope_init(GlobalSymbolTable *table, ScopeId id) {
    Scope *scope = &table->scopes[id];

    scope->buckets = arena_alloc(&table->scope_data_arena, sizeof(ScopeBucket) * scope_init_capacity);
    scope->entries = arena_alloc(&table->scope_data_arena, sizeof(SymbolId) * scope_init_capacity);

    scope->count    = 0;
    scope->capacity = scope_init_capacity;

    scope->parent = GLOBAL_SCOPE_ID_NONE;

    scope->resize_threshold_as_u32 = (u32)(scope_init_capacity * scope_load_factor);

    memset(scope->entries, U8_MAX, sizeof(SymbolId) * scope_init_capacity);
}

SymbolId global_scope_intern_from_local_symbol(GlobalSymbolTable *table, ScopeId scope_id, Symbol *local_symbol) {
    Scope *scope = &table->scopes[scope_id];

    if (UNLIKELY(scope->count >= scope->resize_threshold_as_u32)) {
        global_scope_resize(table, scope);
    }

    u32 hash  = local_symbol->hash;
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (scope->entries[index] != GLOBAL_SYMBOL_ID_NONE) {
        index = (index + 1) & mask;
    }

    SymbolId id = global_symbol_table_create_symbol(table);

    scope->buckets[index].string_id = local_symbol->name_id;
    scope->buckets[index].hash      = hash;
    scope->entries[index]           = id;
    scope->count += 1;

    Symbol *symbol = &table->symbols[id];

    memcpy(symbol, local_symbol, sizeof(Symbol));
    assert(symbol->kind == local_symbol->kind);

    return id;
}

SymbolId global_scope_intern(GlobalSymbolTable *table, ScopeId scope_id, StringId global_string_id) {
    Scope *scope = &table->scopes[scope_id];

    if (UNLIKELY(scope->count >= scope->resize_threshold_as_u32)) {
        global_scope_resize(table, scope);
    }

    u32 hash  = hash_u32(global_string_id);
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (scope->entries[index] != GLOBAL_SYMBOL_ID_NONE) {
        index = (index + 1) & mask;
    }

    SymbolId id = global_symbol_table_create_symbol(table);

    scope->buckets[index].string_id = global_string_id;
    scope->buckets[index].hash      = hash;
    scope->entries[index]           = id;
    scope->count += 1;

    Symbol *symbol = &table->symbols[id];

    symbol->flags   = AST_FLAG_NONE;
    symbol->hash    = hash;
    symbol->name_id = global_string_id;
    symbol->state   = RESOLVE_UNRESOLVED;

    return id;
}

SymbolId global_scope_lookup(GlobalSymbolTable *table, ScopeId scope_id, StringId global_string_id) {
    Scope *scope = &table->scopes[scope_id];

    u32 hash  = hash_u32(global_string_id);
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (scope->entries[index] != GLOBAL_SYMBOL_ID_NONE) {
        ScopeBucket bucket = scope->buckets[index];

        if (bucket.hash == hash) {
            if (bucket.string_id == global_string_id) {
                return scope->entries[index];
            }
        }

        index = (index + 1) & mask;
    }

    return GLOBAL_SYMBOL_ID_NONE;
}

SymbolId global_scope_add_symbol(GlobalSymbolTable *table, ScopeId scope_id, SymbolId id) {
    Scope *scope = &table->scopes[scope_id];

    if (UNLIKELY(scope->count >= scope->resize_threshold_as_u32)) {
        global_scope_resize(table, scope);
    }

    Symbol symbol = table->symbols[id];

    u32 hash  = symbol.hash;
    u32 mask  = scope->capacity - 1;
    u32 index = hash & mask;

    while (scope->entries[index] != GLOBAL_SYMBOL_ID_NONE) {
        index = (index + 1) & mask;
    }

    scope->buckets[index].string_id = symbol.name_id;
    scope->buckets[index].hash      = hash;
    scope->entries[index]           = id;

    scope->count += 1;

    return id;
}

/*
 *
 *  Scope resizing
 *
 */

static void global_scope_resize(GlobalSymbolTable *table, Scope *scope) {
    u32 old_capacity = scope->capacity;
    u32 new_capacity = old_capacity * 2;

    ScopeBucket *new_buckets = arena_alloc(&table->scope_data_arena, new_capacity * sizeof(ScopeBucket));
    SymbolId *new_entries    = arena_alloc(&table->scope_data_arena, new_capacity * sizeof(SymbolId));

    memset(new_entries, U8_MAX, new_capacity * sizeof(SymbolId));

    u32 mask = new_capacity - 1;

    for (u32 i = 0; i < old_capacity; i++) {
        SymbolId id = scope->entries[i];

        if (id == GLOBAL_SYMBOL_ID_NONE)
            continue;

        ScopeBucket bucket = scope->buckets[i];

        u32 index = bucket.hash & mask;

        while (new_entries[index] != GLOBAL_SYMBOL_ID_NONE) {
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
