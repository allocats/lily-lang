#ifndef LILY_IDS_H
#define LILY_IDS_H

#include "utils/types.h"

typedef u32 AstNodeId;
typedef u32 FileId;
typedef u32 IntrinsicId;
typedef u32 ModuleId;
typedef u32 ScopeId;
typedef u32 StringId;
typedef u32 SymbolId;
typedef u32 TypeId;
typedef u32 WorkspaceId;

static constexpr u32 AST_NODE_ID_NONE       = U32_MAX;
static constexpr u32 FILE_ID_NONE           = U32_MAX;
static constexpr u32 GLOBAL_STRING_ID_NONE  = U32_MAX;
static constexpr u32 INTRINSIC_ID_NONE      = U32_MAX;
static constexpr u32 LOCAL_STRING_ID_NONE   = 0xffffff;
static constexpr u32 MODULE_ID_NONE         = U32_MAX;
static constexpr u32 SCOPE_ID_NONE          = U32_MAX;
static constexpr u32 SYMBOL_ID_NONE         = U32_MAX;
static constexpr u32 TYPE_ID_NONE           = U32_MAX;
static constexpr u32 WORKSPACE_ID_NONE      = U32_MAX;

static constexpr u32 COMPILER_SCOPE_ID = 0;

#endif // !LILY_IDS_H
