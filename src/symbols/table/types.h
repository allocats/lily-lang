#ifndef LILY_SYMBOLS_TABLE_TYPES_H
#define LILY_SYMBOLS_TABLE_TYPES_H

#include "meowrena/meowrena.h"
#include "symbols/scope/types.h"
#include "symbols/symbol/types.h"

typedef struct {
    Scope* scopes;
    Symbol* symbols;

    u32 scope_count;
    u32 scope_capacity;

    u32 symbol_count;
    u32 symbol_capacity;

    u32 symbol_resize_threshold_as_u32;
    u32 scope_resize_threshold_as_u32;

    // Arena scope_array_arena;
    Arena scope_data_arena;
    // Arena symbol_array_arena;
    Arena symbol_data_arena;
} SymbolTable;

#endif // !LILY_SYMBOLS_TABLE_TYPES_H
