#ifndef LILY_SYMBOLS_TABLE_GLOBAL_H
#define LILY_SYMBOLS_TABLE_GLOBAL_H

#include "symbols/table/global/types.h"

void global_symbol_table_init(GlobalSymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate);
void global_symbol_table_destroy(GlobalSymbolTable* table);

SymbolId global_symbol_table_create_symbol(GlobalSymbolTable* table);
ScopeId  global_symbol_table_create_scope(GlobalSymbolTable* table);

void global_scope_init(GlobalSymbolTable* table, ScopeId id);

#endif // !LILY_SYMBOLS_TABLE_GLOBAL_H
