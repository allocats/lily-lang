#ifndef LILY_CODEGEN_TYPES_H
#define LILY_CODEGEN_TYPES_H

#include "files/types.h"
#include "workspace/types.h"

#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>

#define STACK_ALLOCATION_THRESHOLD 16

/* 
 *
 * =========================================
 *
 *   Block result to check for termination
 *
 * =========================================
 * 
 */

typedef enum {
    BLOCK_ERROR,
    BLOCK_TERMINATED,
    BLOCK_NOT_TERMINATED,
} BlockResult;

/* 
 *
 * =========================================
 *
 *                Defer stack
 *
 * =========================================
 * 
 */

typedef struct DeferStack DeferStack;

typedef struct DeferStack {
    AstNodeId *ids;
    u32 count;
    u32 capacity;

    DeferStack *previous;
} DeferStack;

typedef struct {
    DeferStack *stack;

    Arena arena;
} DeferContext;

/* 
 *
 * ==========================================
 *
 *      Codegen Context, holds everything 
 *
 * ==========================================
 * 
 */

typedef struct {
    /*
     *
     * This is created once per thread, and destroyed once per thread
     *
     */
    LLVMContextRef llvm_ctx;

    /*
     *
     * Current module (file)
     *
     */
    LLVMModuleRef llvm_module;

    /*
     *
     * Bob the builder
     *
     */
    LLVMBuilderRef llvm_builder;

    /*
     *
     *
     *
     */
    LLVMTargetDataRef llvm_data_target;

    /*
     *
     * These three fields are created once, and destroyed at the same time.
     * Indexed using the corresponding IDs.
     *
     * i.e. For string id 46
     * > string = ctx.string_map[46];
     *
     */
    LLVMValueRef *symbol_map;
    LLVMValueRef *string_map;
    LLVMTypeRef *type_map;

    /*
     *
     * LLVM Triple on the host machine 
     *
     */
    char *llvm_host_triple_str;

    /*
     *
     * LLVM Target Reference
     *
     */
    LLVMTargetRef llvm_target;

    /*
     *
     * LLVM Target Machine Reference
     *
     */
    LLVMTargetMachineRef llvm_target_machine;

    /*
     *
     * LLVM Pass options
     *
     */
    LLVMPassBuilderOptionsRef llvm_pass_builder_options;

    /*
     *
     * VA List pointer
     *
     */
    LLVMValueRef llvm_ap_address;

    /*
     *
     * Declarations block
     *
     */
    LLVMBasicBlockRef declarations_block;

    /*
     *
     * Current block;
     *
     */
    LLVMBasicBlockRef current_block;

    /*
     *
     * Current function
     *
     */
    LLVMValueRef current_function;

    /*
     *
     * Control flow condition (used for `continue`)
     *
     */
    LLVMBasicBlockRef control_flow_condition;

    /*
     *
     * Control flow exit (used for `break`)
     *
     */
    LLVMBasicBlockRef control_flow_merge;

    /*
     *
     * The current workspace
     *
     */
    const Workspace *ws;

    /*
     *
     * The current file that is being compiled with LLVM 
     *
     */
    File *file;

    bool is_release_mode;
    bool emit_llvm_ir;
    bool verify_llvm_ir;

    char *build_directory;

    // ====== Defer Context ======
    DeferContext defer_ctx;

} __attribute__((aligned(64))) CodegenCtx;

#endif // !LILY_CODEGEN_TYPES_H
