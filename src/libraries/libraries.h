#ifndef LILY_LIBRARIES_H
#define LILY_LIBRARIES_H

#include "libraries/types.h"
#include "workspace/types.h"

void foreign_library_init(ForeignLibraries *libraries);
void foreign_library_destroy(ForeignLibraries *libraries);

/*
 *
 * Add a new library to the libraries list
 *
 * path - Use STRING_ID_NONE when adding a local library
 * name - The name of the library
 *
 */
void foreign_library_add(ForeignLibraries *libraries, StringId path, StringId name);

void foreign_libraries_resolve(Workspace *ws);

#endif // !LILY_LIBRARIES_H
