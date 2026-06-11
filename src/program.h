/* program.h */

#ifndef PROGRAM_H_FILE
#define PROGRAM_H_FILE

#include "fh.h"
#include "ast.h"
#include "bytecode.h"
#include "vm.h"
#include "parser.h"
#include "compiler.h"
#include "value.h"

#include "map/map.h"
#include "vec/vec.h"

#ifdef FH_OS_UNIX
#include <dlfcn.h> /* used for dlopen */
#elif FH_OS_WINDOWS
#include <windows.h>
#endif

struct named_c_func {
    const char *name;
    fh_c_func func;
};

DECLARE_STACK(named_c_func_stack, struct named_c_func);
DECLARE_STACK(p_closure_stack, struct fh_closure *);
DECLARE_STACK(p_object_stack, union fh_object *);

// Free-list pool for small allocations (GC objects and map entry arrays).
// Size classes: 64, 128, 256, 512 bytes. Freed blocks are kept in per-class
// linked lists (next pointer stored in the block itself) and reused by
// fh_pool_alloc instead of hitting malloc.
#define FH_NUM_POOL_CLASSES 4

struct fh_program {
    char last_error_msg[512];
    size_t gc_frequency;     // bytes allocated since last collection
    size_t gc_live_bytes;    // bytes held by live objects (object structs only)
    size_t gc_collect_at;    // minimum bytes between collections (user-settable)
    bool gc_isPaused;
    int alive_objects;
    struct fh_value null_value;
    struct fh_parser parser;
    struct fh_compiler compiler;
    struct fh_symtab src_file_names;
    struct named_c_func_stack c_funcs;
    struct fh_vm vm; // GC roots (VM stack)
    //struct p_closure_stack global_funcs;   // GC roots (global functions)
    vec_void_t pinned_objs; //struct p_object_stack pinned_objs;     // GC roots (temporarily pinned objects)
    vec_void_t c_vals; // GC roots (values held by running C functions)
    union fh_object *objects; // all created objects
    void *small_pool[FH_NUM_POOL_CLASSES]; // free lists for small allocations
    map_t(struct fh_closure*) global_funcs_map;
    map_t(struct fh_value*) global_vars_map;  // GC roots (global variables)

    map_void_t c_funcs_map;
};

void *fh_load_dynamic_library(const char *path, struct fh_program *prog);

#endif /* PROGRAM_H_FILE */
