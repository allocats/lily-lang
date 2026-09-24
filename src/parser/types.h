#ifndef LILY_PARSER_TYPES_H
#define LILY_PARSER_TYPES_H

#include "files/types.h"

typedef struct {
    File* file;

    u32 cursor;
    u32 token_count;

    bool is_parsing_type_expr;
    bool is_foreign_allowed;
} Parser;

#endif // !LILY_PARSER_TYPES_H
