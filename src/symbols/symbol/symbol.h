#ifndef LILY_SYMBOLS_SYMBOL_H
#define LILY_SYMBOLS_SYMBOL_H

#include "ids.h"
#include "symbols/table/global/types.h"

TypeId symbol_get_type(GlobalSymbolTable *table, SymbolId id);

ScopeId symbol_get_members_scope(Symbol *symbol);

#endif // !LILY_SYMBOLS_SYMBOL_H
