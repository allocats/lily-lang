#ifndef LILY_WORKSPACE_H
#define LILY_WORKSPACE_H

#include "ids.h"
#include "workspace/types.h"

void workspace_init(Workspace* ws, WorkspaceId id, const char* name);
void workspace_destroy(Workspace* ws);

void compile_workspace(Workspace* ws);

File* workspace_get_file(Workspace* ws, FileId id);

#endif // !LILY_WORKSPACE_H
