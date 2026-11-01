#include "codegen/declarations/intrinsics/intrinsics.h"
#include "codegen/declarations/intrinsics/types.h"
#include "ids.h"
#include "string_interner/global/interner.h"
#include "string_interner/global/types.h"
#include "utils/macros.h"

#include <assert.h>

// ================= Overload information ===================
static IntrinsicInformation intrinsics[] = {
    {
        .kind                    = INTRINSIC_MEMSET,
        .name_cstr               = "memset",
        .parameter_overload_mask = 0b101,
        .return_is_overloaded    = false,
    },
    {
        .kind                    = INTRINSIC_MEMCPY,
        .name_cstr               = "memcpy",
        .parameter_overload_mask = 0b111,
        .return_is_overloaded    = false,
    },
    {
        .kind                    = INTRINSIC_MEMMOVE,
        .name_cstr               = "memmove",
        .parameter_overload_mask = 0b111,
        .return_is_overloaded    = false,
    },
};

static constexpr u32 intrinsic_count = sizeof(intrinsics) / sizeof(IntrinsicInformation);

static u32 intrinsic_min_string_id = 0;
static u32 intrinsic_max_string_id = 0;

/*
 *
 * =================================
 *
 *          Declarations
 *
 * =================================
 *
 */

void intrinsic_table_init(GlobalStringInterner *interner) {
    for (u32 i = 0; i < intrinsic_count; i += 1) {
        StringId id = global_string_intern_cstr(interner, intrinsics[i].name_cstr);

        intrinsics[i].name_id = id;

        intrinsic_min_string_id = MIN(intrinsic_min_string_id, id);
        intrinsic_max_string_id = MAX(intrinsic_max_string_id, id);
    }
}

IntrinsicInformation *intrinsic_get_information(StringId base_name) {
    for (u32 i = 0; i < intrinsic_count; i += 1) {
        if (intrinsics[i].name_id == base_name) {
            return &intrinsics[i];
        }
    }

    return null;
}

/*
 *
 * ================================
 *
 *      Calling the intrinsics
 *
 * ================================
 *
 */

IntrinsicKind intrinsic_get_kind(StringId id) {
    if (id < intrinsic_min_string_id || id > intrinsic_max_string_id) {
        return INTRINSIC_NONE;
    }

    for (u32 i = 0; i < intrinsic_count; i += 1) {
        if (intrinsics[i].name_id == id) {
            return intrinsics[i].kind;
        }
    }

    return INTRINSIC_NONE;
}
