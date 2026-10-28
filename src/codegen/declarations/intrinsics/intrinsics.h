#ifndef LILY_CODEGEN_DECLARATIONS_INTRINSICS_H
#define LILY_CODEGEN_DECLARATIONS_INTRINSICS_H

#include "ids.h"
#include "string_interner/global/types.h"

typedef struct {
    char *name_cstr;
    StringId name_id;
    u16 parameter_overload_mask;
    bool return_is_overloaded;
} IntrinsicInformation;

void intrinsic_table_init(GlobalStringInterner *interner);

const IntrinsicInformation *intrinsic_get_information(StringId base_name);

#endif // !LILY_CODEGEN_DECLARATIONS_INTRINSICS_H
