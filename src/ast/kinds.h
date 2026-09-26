#ifndef LILY_AST_KINDS_H
#define LILY_AST_KINDS_H

#include "ids.h"
#include "tokens/kinds.h"
#include "utils/types.h"

#include <assert.h>

/*
 *
 * AST kinds
 *
*/

#define AST(X)                  \
    X(AST_ERROR)                \
                                \
    X(AST_EXECUTE_DIRECTIVE)    \
    X(AST_IMPORT_DIRECTIVE)     \
    X(AST_INCLUDE_DIRECTIVE)    \
                                \
    X(AST_IMPORT_DECL)          \
                                \
    X(AST_FUNCTION_DECL)        \
    X(AST_PARAMETER_DECL)       \
                                \
    X(AST_STRUCT_DECL)          \
    X(AST_UNION_DECL)           \
    X(AST_FIELD_DECL)           \
                                \
    X(AST_ENUM_DECL)            \
    X(AST_VARIANT_DECL)         \
                                \
    X(AST_VARIABLE_DECL)        \
                                \
    X(AST_TYPE_ALIAS_DECL)      \
                                \
    X(AST_BLOCK)                \
                                \
    X(AST_DEFER_STMT)           \
    X(AST_RETURN_STMT)          \
                                \
    X(AST_IF_STMT)              \
    X(AST_BRANCH)               \
                                \
    X(AST_FOR_LOOP)             \
    X(AST_WHILE_LOOP)           \
                                \
    X(AST_CONTINUE_STMT)        \
    X(AST_BREAK_STMT)           \
                                \
    X(AST_IDENTIFIER)           \
    X(AST_LITERAL)              \
    X(AST_UNARY_OP)             \
    X(AST_BINARY_OP)            \
    X(AST_FUNCTION_CALL)        \
    X(AST_INDEX)                \
    X(AST_MEMBER_ACCESS)        \
                                \
    X(AST_TYPE_BASE)            \
    X(AST_TYPE_ARRAY)           \
    X(AST_TYPE_POINTER)         \
    X(AST_TYPE_FUNCTION)        \
    X(AST_TYPE_VARIADIC)        \
                                \
    X(AST_KINDS_COUNT)          \

typedef enum : u8 {
    AST(GENERATE_ENUM)
} AstKind;

static_assert(sizeof(AstKind) == sizeof(u8) == 1);


static const char* AST_KIND_STRINGS[] = {
    AST(GENERATE_STRING)
};

#undef AST

/*
 *
 * literals 
 *
*/

#define LITERALS(X)         \
    X(LITERAL_BOOL)         \
    X(LITERAL_CHAR)         \
    X(LITERAL_FLOATING)     \
    X(LITERAL_INTEGER)      \
    X(LITERAL_NULL)         \
    X(LITERAL_STRING)       \

typedef enum {
    LITERALS(GENERATE_ENUM)
} LiteralKind;

static const char* LITERAL_KIND_STRINGS[] = {
    LITERALS(GENERATE_STRING)
};

#undef LITERALS

/*
 *
 * dynamic array of ids, used for things like parameters
 *
 */

typedef struct {
    AstNodeId* ids;
    u32 count;
    u32 capacity;
} AstNodeIdList;

/*
 *
 * all the nodes themselves
 *
 */

typedef struct {
    AstNodeId expr;
} AstExecDirective;

typedef struct {
    StringId module;
    FileId file_id;
} AstImportDirective;

typedef struct {
    StringId path;
    FileId file_id;
} AstIncludeDirective;


typedef struct {
    StringId  name;
    AstNodeId import;
} AstImportDeclaration;

typedef struct {
    StringId name;
    AstNodeIdList parameters;
    AstNodeId return_type_expr;
    AstNodeId body;
} AstFunctionDeclaration;

typedef struct {
    StringId  name;
    AstNodeId type_expr;
} AstParameterDeclaration;

typedef struct {
    StringId name;
    AstNodeIdList fields;
} AstStructDeclaration, AstUnionDeclaration;

typedef struct {
    StringId  name;
    AstNodeId type_expr;
} AstFieldDeclaration;

typedef struct {
    StringId name;
    AstNodeId type_expr;
    AstNodeIdList variants;
} AstEnumDeclaration;

typedef struct {
    StringId  name;
    AstNodeId value_expr;
} AstVariantDeclaration;

typedef struct {
    StringId  name;
    AstNodeId type_expr;
    AstNodeId init_expr;
} AstVariableDeclaration;

typedef struct {
    StringId alias;
    AstNodeId type_expr;
} AstTypeAliasDeclaration;

typedef struct {
    AstNodeIdList statements;
} AstBlock;

typedef struct {
    // This could be a block
    AstNodeId expr;
} AstDeferStatement;

typedef struct {
    AstNodeId expr;
} AstReturnStatement;

typedef struct {
    AstNodeId condition;
    AstNodeId body;
} AstBranch;

typedef struct {
    AstNodeIdList branches;
    AstNodeId else_block;
} AstIfStatement;

typedef enum {
    /*
     *
     *  the standard C style for loops
     *  example:
     *      for i: i32 = 0; i < 10; i += 1 { ... }
     *
     */
    FOR_C_STYLE,

    /*
     *
     *  iterate over a range loop
     *  example:
     *      for i: i32 in xs {}
     *
     *  Need to figure out iterating with reference maybe this?
     *
     *      for &i: i32 in xs {}
     *
     */
    FOR_IN,
} ForLoopKind;

typedef struct {
    ForLoopKind kind;

    union {
        struct {
            AstNodeId init;
            AstNodeId cond;
            AstNodeId step;
        } c_style;

        struct {
            AstNodeId iterator;
            AstNodeId range;
        } in;
    } as;

    AstNodeId body;
    StringId label;
} AstForLoop;

typedef struct {
    AstNodeId condition;
    AstNodeId body;
} AstWhileLoop;

typedef struct {
    // Optional, STRING_ID_NONE means no label
    StringId label;
} AstBreak, AstContinue;

typedef struct {
    StringId name;
} AstIdentifier;

typedef struct {
    LiteralKind kind;

    union {
        StringId string;
        bool     boolean;
        char     character;
        f64      floating;
        i64      integer;
    } as;
} AstLiteral;

typedef struct {
    TokenKind op;
    AstNodeId lhs;
    AstNodeId rhs;
} AstBinaryOp;

typedef struct {
    TokenKind op;
    AstNodeId operand;
} AstUnaryOp;

typedef struct {
    AstNodeId identifier;
    AstNodeIdList arguments;
} AstFunctionCall;

typedef struct {
    AstNodeId identifier;
    AstNodeId index_expr;
} AstIndex;

typedef struct {
    AstNodeId object;
    AstNodeId member;
} AstMemberAcess;

/*
 *
 * AST Type Expressions
 *
 */

typedef struct {
    AstNodeId expr;
} AstTypeBase;

typedef struct {
    AstNodeId element;
    AstNodeId size_expr;
} AstTypeArray;

typedef struct {
    AstNodeId base_type;
} AstTypePointer;

typedef struct {
    AstNodeIdList parameters;
    AstNodeId return_type;
} AstTypeFunction;

typedef struct {
    AstNodeId element_type;
} AstTypeVariadic;

#endif // !LILY_AST_KINDS_H
