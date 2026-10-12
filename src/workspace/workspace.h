#ifndef LILY_WORKSPACE_H
#define LILY_WORKSPACE_H

#include "ids.h"
#include "workspace/types.h"

void workspace_init(Workspace *ws, WorkspaceId id, const char *name);
void workspace_destroy(Workspace *ws);

void compile_workspace(Workspace *ws);

void workspace_dispatch_lex_and_parse_file(void *arg);

FileId standard_library_map_get_module(StandardLibraryMap *map, StringId id);

File *workspace_get_file(Workspace *ws, FileId id);

#endif // !LILY_WORKSPACE_H
