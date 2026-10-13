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
 * - `symbol_id` is the global symbol id 
 * - `name_d` is the global string id 
 *
 */
TypeId type_table_intern_nominal(TypeTable *table, TypeKind kind, SymbolId symbol_id, StringId name_id);

#endif // !LILY_TYPES_TABLE_H
