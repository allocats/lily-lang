#ifndef LILY_PARSER_TYPES_H
#define LILY_PARSER_TYPES_H

#include "files/types.h"
#include "workspace/types.h"

typedef struct {
    Workspace* workspace;

    File* file;

    u32 cursor;
    u32 token_count;

    AstNodeId last_allocated_node_id;

    bool is_parsing_type_expr;
    bool is_foreign_allowed;
} Parser;

#endif // !LILY_PARSER_TYPES_H
