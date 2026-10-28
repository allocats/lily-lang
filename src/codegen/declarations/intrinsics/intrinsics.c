#include "codegen/declarations/intrinsics/intrinsics.h"
#include "string_interner/global/interner.h"
#include "string_interner/global/types.h"

static IntrinsicInformation intrinsic_overloads[] = {
    {
        .name_cstr               = "memset",
        .parameter_overload_mask = 0b101,
        .return_is_overloaded    = false,
    },
    {
        .name_cstr               = "memcpy",
        .parameter_overload_mask = 0b111,
        .return_is_overloaded    = false,
    },
    {
        .name_cstr               = "memmove",
        .parameter_overload_mask = 0b111,
        .return_is_overloaded    = false,
    },
};

static constexpr u32 intrinsic_overload_count = sizeof(intrinsic_overloads) / sizeof(IntrinsicInformation);

void intrinsic_table_init(GlobalStringInterner *interner) {
    for (u32 i = 0; i < intrinsic_overload_count; i += 1) {
        intrinsic_overloads[i].name_id = global_string_intern_cstr(interner, intrinsic_overloads[i].name_cstr);
    }
}

const IntrinsicInformation *intrinsic_get_information(StringId base_name) {
    for (u32 i = 0; i < intrinsic_overload_count; i += 1) {
        if (intrinsic_overloads[i].name_id == base_name) {
            return &intrinsic_overloads[i];
        }
    }

    return null;
}
