#ifndef LILY_TYPE_SYSTEM_TYPES_KINDS_H
#define LILY_TYPE_SYSTEM_TYPES_KINDS_H

#include "ids.h"
#include "utils/types.h"

/*
 *
 * X-Macro
 *
*/

#define TYPES(X)     \
    X(TYPE_ARRAY)    \
    X(TYPE_BASE)     \
    X(TYPE_ENUM)     \
    X(TYPE_ERROR)    \
    X(TYPE_FUNCTION) \
    X(TYPE_POINTER)  \
    X(TYPE_STRUCT)   \
    X(TYPE_UNION)    \
    X(TYPE_KIND_COUNT)

typedef enum : u8 {
    TYPES(GENERATE_ENUM)
} TypeKind;

static const char *TYPE_KIND_STRINGS[] = {
    TYPES(GENERATE_STRING)
};

#undef TYPES

/*
 * 
 * all the actual data structures for each type of Type 
 *
 */

typedef struct {
    TypeId pointee_id;
} TypePointer;

typedef struct {
    TypeId element_id;
    u64 size;
} TypeArray;

typedef struct {
    TypeId *fields;
    u32 field_count;
} TypeStruct;

typedef struct {
    TypeId *fields;
    u32 field_count;
} TypeUnion;

typedef struct {
    TypeId underlying_type_id;
} TypeEnum;

typedef struct {
    TypeId *arguments;
    u32 argument_count;

    TypeId return_type;
} TypeFunction;

#endif // !LILY_TYPE_SYSTEM_TYPES_KINDS_H
