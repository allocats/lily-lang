#ifndef LILY_DRIVER_FLAGS_H
#define LILY_DRIVER_FLAGS_H

#include "utils/types.h"

static constexpr u64 DRIVER_FLAG_NONE         = (0 << 0);
static constexpr u64 DRIVER_FLAG_EMIT_LLVM_IR = (1 << 0);
static constexpr u64 DRIVER_FLAG_RELEASE_MODE = (1 << 1);
static constexpr u64 DRIVER_FLAG_VERIFY_IR    = (1 << 2);
static constexpr u64 DRIVER_FLAG_STATIC_BUILD = (1 << 10);

#endif // !LILY_DRIVER_FLAGS_H
