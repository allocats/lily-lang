#ifndef LILY_SYMBOLS_RESOLVE_MERGE_TYPES_H
#define LILY_SYMBOLS_RESOLVE_MERGE_TYPES_H

#include "files/types.h"
#include "ids.h"
#include "workspace/types.h"

typedef struct {
    /*
     * Current file
     */
    File *file;

    /*
     * Shorthand pointer to file -> ast
     */
    Ast *ast;

    /*
     * Current scope, use `enter/exit` functions to navigate lexical scopes
     */
    ScopeId scope_id;

    /*
     * The global symbol table
     */
    GlobalSymbolTable *table;

    /*
     * Current workspace
     */
    Workspace *ws;
} Resolver;

#endif // !LILY_SYMBOLS_RESOLVE_MERGE_TYPES_H
