#ifndef LILY_TYPES_TABLE_H
#define LILY_TYPES_TABLE_H

#include "types/table/types.h"

/*
 *
 * Creation and destruction
 *
 */

void type_table_init(TypeTable *table);
void type_table_destroy(TypeTable *table);

/*
 *
 * Intering of types!
 *
 */

TypeId type_table_intern_nominal(TypeTable *table, TypeKind kind, SymbolId symbol_id);

/*
 *
 * Looking up types!
 *
 */

TypeId type_table_lookup_nominal(TypeTable *table, SymbolId symbol_id);

#endif // !LILY_TYPES_TABLE_H
