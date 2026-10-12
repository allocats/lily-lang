#ifndef LILY_PARSER_TYPES_H
#define LILY_PARSER_TYPES_H

#include "files/types.h"
#include "workspace/types.h"

typedef struct {
    Workspace *workspace;

    File *file;

    u32 cursor;
    u32 token_count;

    AstNodeId last_allocated_node_id;

    bool in_loop_context;
} Parser;

#endif // !LILY_PARSER_TYPES_H
