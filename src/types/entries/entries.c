#include "types/entries/entries.h"
#include "ids.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"

inline bool type_is_kind(TypeTable *table, TypeId id, TypeKind kind) {
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
        return table->entries[lhs].as.pointer_type.pointee_id == builtins->type_void;
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
    return false;
}
