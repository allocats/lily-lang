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

#define BUILTIN_TYPES(X)                              \
    X(variadic, "...", 0, 0)                          \
    X(void, "void", 0, 0)                             \
    X(bool, "bool", sizeof(bool), _Alignof(bool))     \
    X(char, "char", sizeof(char), _Alignof(char))     \
                                                      \
    X(u8, "u8", sizeof(u8), _Alignof(u8))             \
    X(u16, "u16", sizeof(u16), _Alignof(u16))         \
    X(u32, "u32", sizeof(u32), _Alignof(u32))         \
    X(u64, "u64", sizeof(u64), _Alignof(u64))         \
    X(usize, "usize", sizeof(usize), _Alignof(usize)) \
                                                      \
    X(i8, "i8", sizeof(i8), _Alignof(i8))             \
    X(i16, "i16", sizeof(i16), _Alignof(i16))         \
    X(i32, "i32", sizeof(i32), _Alignof(i32))         \
    X(i64, "i64", sizeof(i64), _Alignof(i64))         \
    X(isize, "isize", sizeof(isize), _Alignof(isize)) \
                                                      \
    X(f32, "f32", sizeof(f32), _Alignof(f32))         \
    X(f64, "f64", sizeof(f64), _Alignof(f64))         \
                                                      \
    X(va_list, "va_list", sizeof(va_list), _Alignof(va_list))

#endif // !LILY_TYPES_BUILTINS_TYPES_H
