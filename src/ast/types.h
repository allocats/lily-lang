#ifndef LILY_AST_TYPES_H
#define LILY_AST_TYPES_H

#include "ast/kinds.h"
#include "ids.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

/*
 *
 * flags, might want to make this a u8? leaving it as a u16 
 * for now just to keep breathing room open for new flags
 * and features
 *
 */

typedef enum : u16 {
    AST_FLAG_NONE      = 0 << 0,
    AST_FLAG_TOP_DECL  = 1 << 0,
    AST_FLAG_FOREIGN   = 1 << 1,
    AST_FLAG_INTRINSIC = 1 << 2,
    AST_FLAG_INLINE    = 1 << 3,
    AST_FLAG_NOINLINE  = 1 << 4,
    AST_FLAG_VARIADIC  = 1 << 5,
    AST_FLAG_CONSTANT  = 1 << 6,
} AstFlag;

/*
 *
 * AST Node declaration
 *
 */

typedef struct {
    AstNodeId id;
    AstKind kind;
    AstFlag flags;

    SpanU32 tokens;

    union {
        /*
         * directives
         */
        AstExecDirective    exec_directive;
        AstImportDirective  import_directive;
        AstIncludeDirective include_directive;

        /*
         * declarations
         */
        AstImportDeclaration import_decl;;

        AstFunctionDeclaration  func_decl;
        AstParameterDeclaration param_decl;

        AstStructDeclaration struct_decl;
        AstUnionDeclaration  union_decl;
        AstFieldDeclaration  field_decl;

        AstEnumDeclaration    enum_decl;
        AstVariantDeclaration variant_decl;

        AstVariableDeclaration variable_decl;

        AstTypeAliasDeclaration type_alias_decl;

        /*
         * statements 
         */
        AstBlock block;

        AstDeferStatement  defer_stmt;
        AstReturnStatement return_stmt;

        AstIfStatement if_stmt;
        AstBranch branch;

        AstWhileLoop while_loop;
        AstForLoop   for_loop;

        AstBreak    break_stmt;
        AstContinue continue_stmt;

        /*
         * expressions
         */
        AstIdentifier identifier;
        AstLiteral literal;

        AstBinaryOp binary_op;
        AstUnaryOp unary_op;

        AstFunctionCall func_call;
        AstMemberAcess member_access;
        AstIndex index;

        AstStructLiteral struct_literal;
        AstFieldInit     field_init;

        /*
         * types
         */
        AstTypeBase base_type;
        AstTypeArray array_type;
        AstTypePointer pointer_type;
        AstTypeFunction function_type;
        AstTypeVariadic variadic_type;
    } as;
} AstNode;


[[deprecated("Do not use this, just to see size here")]] 
static constexpr u64 __ast_node_size = sizeof(AstNode);

/*
 *
 * AST Tree declaration
 *
 */

typedef struct {
    // used for small vec allocations (i.e. parameter list)
    Arena gpa;

    // purely for the array
    Arena nodes_arena;

    AstNode* nodes;
    u32 count;
    u32 capacity;

    /* 
     * this covers ALL declarations, including top level declarations and scope
     * declarations, i.e. variables inside of functions, for loops, parameters etc.
    */
    u32 total_declaration_count;

    /* 
     * purely for symbols to be exported from the file: 
     *   - functions
     *   - structs
     *   - enums
     *   - unions 
     *   - globals
     *   - type aliases (e.g. `TypeId :: u32;`)
    */
    u32 top_level_declaration_count;
} Ast;

#endif // !LILY_AST_TYPES_H
