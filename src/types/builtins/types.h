#ifndef LILY_TYPES_BUILTINS_TYPES_H
#define LILY_TYPES_BUILTINS_TYPES_H

#include "ids.h"

typedef struct {
    TypeId va_list_type_id;

    TypeId void_type_id;
    TypeId bool_type_id;
    TypeId char_type_id;

    TypeId u8_type_id;
    TypeId u16_type_id;
    TypeId u32_type_id;
    TypeId u64_type_id;
    TypeId usize_type_id;

    TypeId i8_type_id;
    TypeId i16_type_id;
    TypeId i32_type_id;
    TypeId i64_type_id;
    TypeId isize_type_id;

    TypeId f32_type_id;
    TypeId f64_type_id;
} BuiltinTypes;

#endif // !LILY_TYPES_BUILTINS_TYPES_H
