#ifndef LILY_TOKENS_TYPES_H
#define LILY_TOKENS_TYPES_H

#include "meowrena/meowrena.h"
#include "tokens/kinds.h"
#include "utils/types.h"

#include <assert.h>

typedef struct {
    u32 start;

    // Does mean tokens are limited to (2^16)-1 (65535 characters) in length, migth beworth mulling 
    // over this more, but does greatly improve the cache performance of the lexer, a little bit more complex 
    // but it has its strenghts
    u16 length;

    TokenKind kind;
} Token;

static_assert(sizeof(Token) == 8);


typedef struct {
    Arena arena;

    Token* items;
    u64 count;
    u64 capacity;
} TokenArray;


typedef struct {
    u32 line;
    u32 col;
} SourceLocation;

static_assert(sizeof(SourceLocation) == 8);


typedef struct {
    Arena arena;

    SourceLocation* items;
    u64 count;
    u64 capacity;
} SourceLocationArray;


static_assert(sizeof(Token) == sizeof(SourceLocation));


#endif // !LILY_TOKENS_TYPES_H
