#include "codegen/codegen.h"
#include "ast/kinds.h"
#include "ast/types.h"
#include "codegen/declarations/declarations.h"
#include "codegen/declarations/intrinsics/intrinsics.h"
#include "codegen/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/types.h"
#include "threadpool/threadpool.h"
#include "threadpool/types.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "utils/wrappers.h"
#include "workspace/types.h"

#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>

atomic_bool llvm_failed = false;

static usize string_map_size = 0;
static usize symbol_map_size = 0;

extern Driver driver;
extern thread_local ThreadContext thread_context;

static void dispatch_codegen_file(void *argument);
static bool codegen_context_init(CodegenCtx *ctx, Workspace *ws, char *build_directory);
static void codegen_context_destroy(CodegenCtx *ctx);

static bool codegen_file(const File *file);
static bool codegen_ast(CodegenCtx *ctx);
static bool codegen_node(CodegenCtx *ctx, const AstNode *node);

bool codegen(Workspace *ws) {
    intrinsic_table_init(&ws->global_string_interner);

    llvm_failed = false;

    char build_dir[PATH_MAX] = {0};

    snprintf(build_dir, sizeof(build_dir), "./.build/workspace_%u/", ws->id);

    if (mkdir("./.build", 0700) != 0) {
        if (errno != EEXIST) {
            diagnostic_add_generic(
                &driver.global_diagnostic_engine,
                DIAG_ERROR,
                "Failed to create '.build' directory"
            );

            return false;
        }
    }

    if (mkdir(build_dir, 0700) != 0) {
        if (errno != EEXIST) {
            diagnostic_add_generic(
                &driver.global_diagnostic_engine,
                DIAG_ERROR,
                "Failed to create '%s' directory",
                build_dir
            );

            return false;
        }
    }

    string_map_size = ws->global_string_interner.count * sizeof(LLVMValueRef);
    symbol_map_size = ws->global_symbol_table.symbol_count * sizeof(LLVMValueRef);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    for (u8 i = 0; i < driver.codegen_context_count; i += 1) {
        codegen_context_init(&driver.codegen_contexts[i], ws, build_dir);
    }

    for (u32 i = 0; i < ws->file_interner.count; i += 1) {
        if (atomic_load(&llvm_failed) == true) {
            threadpool_wait(&driver.thread_pool);
            break;
        }

        File *file = ws->file_interner.entries[i];

        threadpool_add_task(&driver.thread_pool, dispatch_codegen_file, file);
    }

    threadpool_wait(&driver.thread_pool);

    for (u8 i = 0; i < driver.codegen_context_count; i += 1) {
        codegen_context_destroy(&driver.codegen_contexts[i]);
    }

    return !llvm_failed;
}

static bool codegen_file(const File *file) {
    char *msg = null; // msg for LLVM error reporting

    CodegenCtx *ctx = thread_context.codegen_ctx;

    ctx->file        = (File *)file;
    ctx->llvm_module = LLVMModuleCreateWithNameInContext(file->path.ptr, ctx->llvm_ctx);

    LLVMSetTarget(ctx->llvm_module, ctx->llvm_host_triple_str);
    LLVMSetModuleDataLayout(ctx->llvm_module, ctx->llvm_data_target);

    memset(ctx->string_map, 0, string_map_size);
    memset(ctx->symbol_map, 0, symbol_map_size);

    // ===== va list ptr ======
    ctx->llvm_ap_address = null;

    // ===== blocks ======
    ctx->declarations_block = null;
    ctx->current_block      = null;

    /*
     *
     * Walk the AST and perform codegen
     *
     */

    if (!codegen_ast(ctx)) {
        LLVMDisposeModule(ctx->llvm_module);
        return false;
    }

    if (LLVMVerifyModule(ctx->llvm_module, LLVMReturnStatusAction, &msg) != 0) {
        diagnostic_add_generic(thread_context.diagnostic_engine, DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeModule(ctx->llvm_module);
        LLVMDisposeMessage(msg);
        return false;
    }

    /*
     *
     * Run LLVM passes
     *
     */
    LLVMErrorRef error = LLVMRunPasses(
        ctx->llvm_module,
        ctx->is_release_mode ? "default<O2>" : "default<O0>",
        ctx->llvm_target_machine,
        ctx->llvm_pass_builder_options
    );

    if (error != null) {
        msg = LLVMGetErrorMessage(error);
        diagnostic_add_generic(thread_context.diagnostic_engine, DIAG_ERROR, "LLVM: %s\n", msg);

        LLVMDisposeModule(ctx->llvm_module);
        LLVMDisposeErrorMessage(msg);

        return false;
    }

    /*
     *
     * Emit to .o file
     *
     */
    char path[PATH_MAX] = {0};

    i32 len = snprintf(path, sizeof(path), "%sfile_%u.o", ctx->build_directory, file->id);

    ctx->file->object_path = (str8){
        .ptr = strdup(path),
        .len = len
    };

    if (LLVMTargetMachineEmitToFile(ctx->llvm_target_machine, ctx->llvm_module, path, LLVMObjectFile, &msg) != 0) {
        diagnostic_add_generic(thread_context.diagnostic_engine, DIAG_ERROR, "LLVM: %s\n", msg);

        LLVMDisposeModule(ctx->llvm_module);
        LLVMDisposeMessage(msg);

        return false;
    }

    if (ctx->emit_llvm_ir) {
        snprintf(path, sizeof(path), "file_%u.ll", ctx->file->id);

        char *ir = LLVMPrintModuleToString(ctx->llvm_module);

        FILE *fp = fopen(path, "w+");

        if (!fp) {
            diagnostic_add_generic(
                thread_context.diagnostic_engine,
                DIAG_ERROR,
                "LLVM: Unable to dump LLVM IR to %s",
                path
            );

            LLVMDisposeModule(ctx->llvm_module);
            LLVMDisposeMessage(ir);

            return false;
        }

        fprintf(stdout, "Emitted LLVM IR to %s (%.*s)\n", path, STR8_FMT(ctx->file->path));
        fprintf(fp, "%s", ir);

        fclose(fp);
        LLVMDisposeMessage(ir);
    }

    LLVMDisposeModule(ctx->llvm_module);

    return true;
}

/*
 *
 * Thread dispatcher
 *
 */
static void dispatch_codegen_file(void *argument) {
    File *file = (File *)argument;

    if (!codegen_file(file)) {
        atomic_store(&llvm_failed, true);
    }
}

/*
 *
 * Codegen Context Functions
 *
 */
static bool codegen_context_init(CodegenCtx *ctx, Workspace *ws, char *build_directory) {
    assert(ctx);

    bool result = true;

    ctx->is_release_mode = false; // TODO: make this actually work
    ctx->emit_llvm_ir    = false; // TODO: make this actually work
    ctx->build_directory = build_directory;

    ctx->declarations_block = null;
    ctx->current_block      = null;

    /*
     *
     * Main contexts
     *
     */

    ctx->llvm_ctx     = LLVMContextCreate();
    ctx->llvm_builder = LLVMCreateBuilderInContext(ctx->llvm_ctx);

    /*
     *
     *  Maps
     *
     */

    ctx->type_map   = lily_calloc(ws->type_table.entry_count, sizeof(LLVMTypeRef));
    ctx->string_map = lily_calloc(ws->global_string_interner.count, sizeof(LLVMValueRef));
    ctx->symbol_map = lily_calloc(ws->global_symbol_table.symbol_count, sizeof(LLVMValueRef));

    ctx->ws = ws;

    /*
     *
     * Pass builder
     *
     */
    ctx->llvm_pass_builder_options = LLVMCreatePassBuilderOptions();

    /*
     *
     * Target machine stuff
     *
     */

    char *msg = null;

    ctx->llvm_host_triple_str = LLVMGetDefaultTargetTriple();
    ctx->llvm_target          = null;

    if (LLVMGetTargetFromTriple(ctx->llvm_host_triple_str, &ctx->llvm_target, &msg) != 0) {
        diagnostic_add_generic(&driver.global_diagnostic_engine, DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);

        result = false;
    }

    char *cpu          = LLVMGetHostCPUName();
    char *cpu_features = LLVMGetHostCPUFeatures();

    ctx->llvm_target_machine = LLVMCreateTargetMachine(
        ctx->llvm_target,
        ctx->llvm_host_triple_str,
        cpu,
        cpu_features,
        ctx->is_release_mode ? LLVMCodeGenLevelDefault : LLVMCodeGenLevelNone,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    LLVMDisposeMessage(cpu);
    LLVMDisposeMessage(cpu_features);

    ctx->llvm_data_target = LLVMCreateTargetDataLayout(ctx->llvm_target_machine);

    return result;
}

static void codegen_context_destroy(CodegenCtx *ctx) {
    free(ctx->type_map);
    free(ctx->string_map);
    free(ctx->symbol_map);

    LLVMDisposeBuilder(ctx->llvm_builder);
    LLVMContextDispose(ctx->llvm_ctx);
    LLVMDisposePassBuilderOptions(ctx->llvm_pass_builder_options);
    LLVMDisposeTargetMachine(ctx->llvm_target_machine);
    LLVMDisposeMessage(ctx->llvm_host_triple_str);
    LLVMDisposeTargetData(ctx->llvm_data_target);
}

/*
 *
 * ACTUAL CODEGEN GRAAAA
 *
 */

static bool codegen_ast(CodegenCtx *ctx) {
    const Ast *ast = &ctx->file->ast;

    for (u32 i = 0; i < ast->count; i += 1) {
        const AstNode *node = &ast->nodes[i];

        if (node->flags & AST_FLAG_TOP_DECL) {
            if (!codegen_node(ctx, node)) {
                return false;
            }
        }
    }

    return true;
}

static bool codegen_node(CodegenCtx *ctx, const AstNode *node) {
    switch (node->kind) {
    case AST_IMPORT_DIRECTIVE:
    case AST_INCLUDE_DIRECTIVE:
    case AST_EXECUTE_DIRECTIVE:
        return true;

    case AST_FUNCTION_DECL:
        return codegen_function(ctx, node);

    default:
        return true;
        // UNREACHABLE("Impossible top level node %s", AST_KIND_STRINGS[node->kind]);
    }
}
