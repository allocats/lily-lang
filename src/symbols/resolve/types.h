#ifndef LILY_SYMBOLS_RESOLVE_TYPES_H
#define LILY_SYMBOLS_RESOLVE_TYPES_H

#include "files/types.h"
#include "ids.h"
#include "string_interner/global/types.h"
#include "types/builtins/types.h"
#include "types/table/types.h"
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
     * The type table
     */
    TypeTable *type_table;

    /*
     * The global symbol table
     */
    GlobalSymbolTable *symbol_table;

    /*
     * The global string interner
     */
    GlobalStringInterner *interner;

    /*
     * Current workspace
     */
    Workspace *ws;

    /*
     * Builtin types -> ws.builtin_types
     */
    BuiltinTypeIds builtin_types;

    /*
     * Current symbol
     */
    SymbolId current_symbol_id;
} Resolver;

#endif // !LILY_SYMBOLS_RESOLVE_TYPES_H
