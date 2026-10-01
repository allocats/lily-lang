#ifndef LILY_SYMBOLS_TABLE_H
#define LILY_SYMBOLS_TABLE_H

#include "symbols/table/types.h"

void symbol_table_init(SymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate);
void symbol_table_destory(SymbolTable* table);

#endif // !LILY_SYMBOLS_TABLE_H
