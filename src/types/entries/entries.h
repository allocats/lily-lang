#ifndef LILY_TYPES_ENTRIES_H
#define LILY_TYPES_ENTRIES_H

#include "ids.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"
#include "types/table/types.h"

bool type_is_kind(const TypeTable *table, TypeId id, TypeKind kind);
bool type_is_int(BuiltinTypeIds builtins, TypeId id);
bool type_is_signed_int(BuiltinTypeIds builtins, TypeId id);
bool type_is_unsigned_int(BuiltinTypeIds builtins, TypeId id);
bool type_is_float(BuiltinTypeIds builtins, TypeId id);
bool type_is_numeric(BuiltinTypeIds builtins, TypeId id);

bool type_is_structure(TypeTable *table, TypeId id);

bool types_are_assignable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs);
bool types_are_arithmeticable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs);
bool types_are_comparable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs);

bool type_can_cast_to(TypeTable *table, BuiltinTypeIds *builtins, TypeId from, TypeId to);

#endif // !LILY_TYPES_ENTRIES_H
