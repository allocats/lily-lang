#ifndef LILY_DRIVER_FLAGS_H
#define LILY_DRIVER_FLAGS_H

#include "utils/types.h"

static constexpr u64 DRIVER_FLAG_NONE         = (0 << 0);
static constexpr u64 DRIVER_FLAG_EMIT_LLVM_IR = (1 << 0);
static constexpr u64 DRIVER_FLAG_RELEASE_MODE = (1 << 1);

#endif // !LILY_DRIVER_FLAGS_H
