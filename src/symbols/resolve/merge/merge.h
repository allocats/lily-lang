#ifndef LILY_SYMBOLS_RESOLVE_MERGE_H
#define LILY_SYMBOLS_RESOLVE_MERGE_H

#include "workspace/types.h"

/*
 *
 * The first pass after registering top level symbols.
 * 
 * NOTE: This does not touch `import **declarations**`
 *
 */
void resolve_imports(Workspace *ws);

#endif // !LILY_SYMBOLS_RESOLVE_MERGE_H
