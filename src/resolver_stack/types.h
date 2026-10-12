#ifndef LILY_RESOLVER_STACK_TYPES_H
#define LILY_RESOLVER_STACK_TYPES_H

#include "ids.h"
#include "utils/types.h"

static constexpr u64 RESOLVER_STACK_MAX = 256;

/*
 *
 * X-Macro for resolver states
 *
 */

#define STATES(X)         \
    X(RESOLVE_UNRESOLVED) \
    X(RESOLVE_RESOLVING)  \
    X(RESOLVE_RESOLVED)   \
    X(RESOLVE_ERRO)

typedef enum : u8 {
    STATES(GENERATE_ENUM)
} ResolveState;

static const char *REOSLVE_STATE_STRINGS[] = {
    STATES(GENERATE_STRING)
};

#undef STATES

typedef enum {
    QUERY_SYMBOL,
} ResolveQueryKind;

typedef struct {
    ResolveQueryKind kind;

    union {
        SymbolId symbol;
    } as;
} ResolveQuery;

typedef struct {
    ResolveQuery items[RESOLVER_STACK_MAX];
    u32 top;
} ResolverStack;

#endif // !LILY_RESOLVER_STACK_TYPES_H
