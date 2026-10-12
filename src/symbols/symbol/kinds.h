#ifndef LILY_SYMBOLS_SYMBOL_KINDS_H
#define LILY_SYMBOLS_SYMBOL_KINDS_H

#include "ids.h"
#include "resolver_stack/types.h"
#include "utils/types.h"

/*
 *
 * X-Macro to create enum and strings for debugging
 *
*/

#define SYMBOLS(X)      \
    X(SYMBOL_ENUM)      \
    X(SYMBOL_ERROR)     \
    X(SYMBOL_FIELD)     \
    X(SYMBOL_FUNCTION)  \
    X(SYMBOL_IMPORT)    \
    X(SYMBOL_PARAMETER) \
    X(SYMBOL_STRUCT)    \
    X(SYMBOL_TYPE)      \
    X(SYMBOL_UNION)     \
    X(SYMBOL_VARIABLE)  \
    X(SYMBOL_VARIANT)

typedef enum : u8 {
    SYMBOLS(GENERATE_ENUM)
} SymbolKind;

static const char *SYMBOL_KIND_STRINGS[] = {
    SYMBOLS(GENERATE_STRING)
};

#undef SYMBOLS

/*
 * 
 * all the actual data structures for each type of symbol
 *
 */

typedef struct {
    /*
     * The file that is being imported
     */
    FileId file_id;

    /*
     * The file's top level scope housing all its exposed symbols
     */
    ScopeId scope_id;
} SymbolImport;

/*
 *
 * Functions and parameters
 *
 */

typedef struct {
    TypeId type_id;
} SymbolParameter;

typedef struct {
    SymbolId *parameters;
    u32 parameter_count;

    /*
     * The function scope, includes parameters and body
     */
    ScopeId scope_id;

    TypeId return_type_id;

    ResolveState signature_state;
} SymbolFunction;

/*
 *
 * Structs, unions and fields
 *
 */

typedef struct {
    TypeId type_id;

    /*
     *
     * field index within the struct, used for LLVM's GEP
     *
     * NOTE: in unions, all fields will use index 0 
     *
     */
    u32 index;
} SymbolField;

typedef struct {
    SymbolId *fields;
    u32 field_count;

    /*
     * The resolved TypeID for this type
     */
    TypeId resolved_type_id;
} SymbolStruct, SymbolUnion;

/*
 *
 * Enums and its variants
 *
 */

typedef struct {
    /*
     * The enum type it belongs to
     */
    TypeId type_id;

    /*
     * TODO: interpreter to evaluate this value!
     */
    i64 value;
} SymbolVariant;

typedef struct {
    SymbolId *variants;
    u32 variant_count;

    /*
     * The resolved TypeID for this type
     */
    TypeId resolved_type_id;
} SymbolEnum;

/*
 *
 * Variables and types
 *
 */

typedef struct {
    TypeId type_id;
} SymbolType;

typedef struct {
    TypeId type_id;

    // Maybe store values here for const eval?
} SymbolVariable;

#endif // !LILY_SYMBOLS_SYMBOL_KINDS_H
