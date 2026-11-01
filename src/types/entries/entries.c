#include "types/entries/entries.h"
#include "ids.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"

inline bool type_is_kind(const TypeTable *table, TypeId id, TypeKind kind) {
    return table->entries[id].kind == kind;
}

inline bool type_is_int(BuiltinTypeIds builtins, TypeId id) {
    return (id >= builtins.type_u8 && id <= builtins.type_isize);
}

inline bool type_is_signed_int(BuiltinTypeIds builtins, TypeId id) {
    return (id >= builtins.type_i8 && id <= builtins.type_isize);
}

inline bool type_is_unsigned_int(BuiltinTypeIds builtins, TypeId id) {
    return (id >= builtins.type_u8 && id <= builtins.type_usize);
}

inline bool type_is_float(BuiltinTypeIds builtins, TypeId id) {
    return (id == builtins.type_f32 || id == builtins.type_f64);
}

inline bool type_is_numeric(BuiltinTypeIds builtins, TypeId id) {
    return (id >= builtins.type_u8 && id <= builtins.type_f64);
}

bool types_are_assignable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs) {
    if (lhs == TYPE_ID_NONE || rhs == TYPE_ID_NONE) {
        return false;
    }

    if (lhs == rhs) {
        return true;
    }

    /*
     *
     * check if lhs is a `*void`, aka generic pointer
     *
     */
    if (type_is_kind(table, lhs, TYPE_POINTER) && type_is_kind(table, rhs, TYPE_POINTER)) {
        return table->entries[lhs].as.pointer_type.pointee_id == builtins->type_void || table->entries[rhs].as.pointer_type.pointee_id == builtins->type_void;
    }

    u32 lhs_size = table->entries[lhs].size;
    u32 rhs_size = table->entries[rhs].size;

    /*
     *
     * both are signed ints
     *
     */
    if (type_is_signed_int(*builtins, lhs) && type_is_signed_int(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * both are unsigned ints
     *
     */
    if (type_is_unsigned_int(*builtins, lhs) && type_is_unsigned_int(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * both are floats
     *
     */
    if (type_is_float(*builtins, lhs) && type_is_float(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * unsigned -> signed IF the signed type is larger, therefore lossless conversion
     *
     */
    if (type_is_signed_int(*builtins, lhs) && type_is_unsigned_int(*builtins, rhs)) {
        return lhs_size > rhs_size;
    }

    return false;
}

bool type_can_cast_to(TypeTable *table, BuiltinTypeIds *builtins, TypeId from, TypeId to) {
    /*
     *
     * This call will check if `from == to` and for any TYPE_ID_NONE
     *
     */
    if (types_are_assignable(table, builtins, to, from)) {
        return true;
    }

    bool from_is_pointer = type_is_kind(table, from, TYPE_POINTER);
    bool to_is_pointer   = type_is_kind(table, to, TYPE_POINTER);

    if (from_is_pointer && to == builtins->type_usize) {
        return true;
    }

    if (from_is_pointer && to_is_pointer) {
        if (table->entries[from].as.pointer_type.pointee_id == builtins->type_void) {
            return true;
        }

        if (table->entries[to].as.pointer_type.pointee_id == builtins->type_void) {
            return true;
        }

        return false;
    }

    bool from_is_integer = type_is_int(*builtins, from);
    bool to_is_integer   = type_is_int(*builtins, to);

    if (from_is_integer && to_is_integer) {
        return true;
    }

    bool from_is_float = type_is_float(*builtins, from);
    bool to_is_float   = type_is_float(*builtins, to);

    if (from_is_float && to_is_float) {
        return true;
    }

    if ((from_is_float || to_is_float) && (from_is_integer || to_is_integer)) {
        return true;
    }

    bool from_is_char = from == builtins->type_char;
    bool to_is_char   = to == builtins->type_char;

    if ((from_is_char || to_is_char) && (from_is_integer || to_is_integer)) {
        return true;
    }

    return false;
}

bool types_are_arithmeticable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs) {
    if (lhs == TYPE_ID_NONE || rhs == TYPE_ID_NONE) {
        return false;
    }

    if (lhs == rhs) {
        return true;
    }

    u32 lhs_size = table->entries[lhs].size;
    u32 rhs_size = table->entries[rhs].size;

    /*
     *
     * both are signed ints
     *
     */
    if (type_is_signed_int(*builtins, lhs) && type_is_signed_int(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * both are unsigned ints
     *
     */
    if (type_is_unsigned_int(*builtins, lhs) && type_is_unsigned_int(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * both are floats
     *
     */
    if (type_is_float(*builtins, lhs) && type_is_float(*builtins, rhs)) {
        return lhs_size >= rhs_size;
    }

    /*
     *
     * unsigned -> signed IF the signed type is larger, therefore lossless conversion
     *
     */
    if (type_is_signed_int(*builtins, lhs) && type_is_unsigned_int(*builtins, rhs)) {
        return lhs_size > rhs_size;
    }

    return false;
}

bool types_are_comparable(TypeTable *table, BuiltinTypeIds *builtins, TypeId lhs, TypeId rhs) {
    if (lhs == TYPE_ID_NONE || rhs == TYPE_ID_NONE) {
        return false;
    }

    if (lhs == rhs) {
        return true;
    }

    if (type_is_unsigned_int(*builtins, lhs) && type_is_unsigned_int(*builtins, rhs)) {
        return true;
    }

    if (type_is_signed_int(*builtins, lhs) && type_is_signed_int(*builtins, rhs)) {
        return true;
    }

    if (type_is_float(*builtins, lhs) && type_is_float(*builtins, rhs)) {
        return true;
    }

    return false;
}
