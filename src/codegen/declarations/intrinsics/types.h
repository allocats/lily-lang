#ifndef LILY_CODEGEN_DECLARATIONS_INTRINSICS_TYPES_H
#define LILY_CODEGEN_DECLARATIONS_INTRINSICS_TYPES_H

#include "ids.h"
#include "utils/types.h"

// ====== Kind ======
#define INTRINSICS(X)   \
    X(INTRINSIC_NONE)   \
    X(INTRINSIC_MEMSET) \
    X(INTRINSIC_MEMCPY) \
    X(INTRINSIC_MEMMOVE)

typedef enum {
    INTRINSICS(GENERATE_ENUM)
} IntrinsicKind;

static const char *INTRINSIC_KIND_STRINGS[] = {
    INTRINSICS(GENERATE_STRING)
};

// ====== Info ======
typedef struct {
    char *name_cstr;
    StringId name_id;
    u16 parameter_overload_mask;
    bool return_is_overloaded;

    IntrinsicKind kind;
} IntrinsicInformation;

#endif // !LILY_CODEGEN_DECLARATIONS_INTRINSICS_TYPES_H
