#ifndef LILY_TYPES_BUILTINS_TYPES_H
#define LILY_TYPES_BUILTINS_TYPES_H

#include "ids.h"

#include <stdarg.h>

//X(variadic, "...", 0, 0)

typedef struct {
    char *ptr;
    usize len;
} __string;

#define BUILTIN_TYPES(X)                             \
    X(void, "void", 0, 0)                            \
    X(bool, "bool", sizeof(bool), alignof(bool))     \
    X(char, "char", sizeof(char), alignof(char))     \
                                                     \
    X(u8, "u8", sizeof(u8), alignof(u8))             \
    X(u16, "u16", sizeof(u16), alignof(u16))         \
    X(u32, "u32", sizeof(u32), alignof(u32))         \
    X(u64, "u64", sizeof(u64), alignof(u64))         \
    X(usize, "usize", sizeof(usize), alignof(usize)) \
                                                     \
    X(i8, "i8", sizeof(i8), alignof(i8))             \
    X(i16, "i16", sizeof(i16), alignof(i16))         \
    X(i32, "i32", sizeof(i32), alignof(i32))         \
    X(i64, "i64", sizeof(i64), alignof(i64))         \
    X(isize, "isize", sizeof(isize), alignof(isize)) \
                                                     \
    X(f32, "f32", sizeof(f32), alignof(f32))         \
    X(f64, "f64", sizeof(f64), alignof(f64))         \
                                                     \
    X(va_list, "va_list", sizeof(va_list), alignof(va_list))

/*
 *
 * Struct to shorthand to primitives
 *
 */

typedef struct {

#define X(id_, name_, size_, alignment_) TypeId type_##id_;
    BUILTIN_TYPES(X)
#undef X

    TypeId type_string;
} BuiltinTypeIds;

static_assert(68 == sizeof(BuiltinTypeIds));

/*
 *
 * Builtin type struct for registering symbols/type entries
 *
 */

typedef struct {
    str8 name;
    u32 size;
    u16 alignment;
} BuiltinType;

/*
 *
 * Array macro shenanigans, constructs an array of builtins from the above macro
 *
 */

static const BuiltinType builtin_types[] = {

#define X(id, name_, size_, alignment_) \
                                        \
    {                                   \
        .name = {                       \
            .ptr = name_,               \
            .len = sizeof(name_) - 1,   \
        },                              \
        .size      = size_,             \
        .alignment = alignment_,        \
    },

    BUILTIN_TYPES(X)

#undef X

};

static constexpr u32 builtin_type_count = sizeof(builtin_types) / sizeof(BuiltinType);

#endif // !LILY_TYPES_BUILTINS_TYPES_H
