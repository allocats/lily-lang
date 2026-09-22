#ifndef LILY_AST_KINDS_H
#define LILY_AST_KINDS_H

#include "utils/types.h"

#include <assert.h>

#define AST(X)          \
    X(AST_IDENTIFIER)   \

typedef enum : u8 {
    AST(GENERATE_ENUM)
} AstKind;

static_assert(sizeof(AstKind) == sizeof(u8) == 1);


static const char* AST_KIND_STRINGS[] = {
    AST(GENERATE_STRING)
};

#undef AST

#endif // !LILY_AST_KINDS_H
