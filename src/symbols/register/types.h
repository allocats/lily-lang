#ifndef LILY_SYMBOLS_REGISTER_TYPES_H
#define LILY_SYMBOLS_REGISTER_TYPES_H

#include "files/types.h"
#include "symbols/table/local/types.h"
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
     * The thread local symbol table
     */
    LocalSymbolTable *table;

    /*
     * Current workspace
     */
    Workspace *ws;
} Registrar;

#endif // !LILY_SYMBOLS_REGISTER_TYPES_H
