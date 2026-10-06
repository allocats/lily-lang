#include "symbols/table/global/table.h"

void global_scope_init(GlobalSymbolTable* table, ScopeId id) {
    Scope* scope = &table -> scopes[id];

    scope -> buckets = arena_alloc(&table -> scope_data_arena, sizeof(ScopeBucket) * scope_init_capacity);
    scope -> entries = arena_alloc(&table -> scope_data_arena, sizeof(SymbolId)    * scope_init_capacity);

    scope -> count = 0;
    scope -> capacity = scope_init_capacity;

    scope -> parent = GLOBAL_SCOPE_ID_NONE;

    scope -> resize_threshold_as_u32 = (u32)(scope_init_capacity * scope_load_factor);
}
