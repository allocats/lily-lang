#ifndef LILY_CODEGEN_DECLARATIONS_INTRINSICS_H
#define LILY_CODEGEN_DECLARATIONS_INTRINSICS_H

#include "codegen/declarations/intrinsics/types.h"
#include "ids.h"
#include "string_interner/global/types.h"

void intrinsic_table_init(GlobalStringInterner *interner);

IntrinsicInformation *intrinsic_get_information(StringId base_name);
IntrinsicKind intrinsic_get_kind(StringId id);

#endif // !LILY_CODEGEN_DECLARATIONS_INTRINSICS_H
