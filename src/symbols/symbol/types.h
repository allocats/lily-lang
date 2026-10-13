#ifndef LILY_SYMBOLS_SYMBOL_TYPES_H
#define LILY_SYMBOLS_SYMBOL_TYPES_H

#include "ids.h"
#include "resolver_stack/types.h"
#include "symbols/symbol/kinds.h"

#include <assert.h>

typedef struct {
    SymbolKind kind;

    /*
     *
     * String hash
     *
     */
    u32 hash;

    /*
     *
     * copied straight from the AstNode, mainly for ease of use 
     * and reduce touching other arrys when checking flags
     *
     */
    u16 flags;

    /*
     *
     * Used to catch recursive symbol includes, and to lazily resolve symbols 
     *
     */
    ResolveState state;

    /*
     *
     * The file in which it belongs to
     *
     */
    FileId file_id;

    /*
     *
     * Symbol's binding, i.e. Foo :: struct {}
     *                        ^^^
     *
     */
    StringId name_id;

    /*
     *
     * Declaration of the symbol within the AST, useful for backpeddling
     * during codegen especially.
     *
     */
    AstNodeId ast_node_id;

    union {
        SymbolImport import_symbol;

        SymbolParameter parameter_symbol;
        SymbolFunction function_symbol;

        SymbolField field_symbol;
        SymbolUnion union_symbol;
        SymbolStruct struct_symbol;

        SymbolVariant variant_symbol;
        SymbolEnum enum_symbol;

        SymbolVariable variable_symbol;

        SymbolType type_symbol;
    } as;
} Symbol;

static_assert(48 == sizeof(Symbol)); // Sizeof Symbol changed!

#endif // !LILY_SYMBOLS_SYMBOL_TYPES_H
