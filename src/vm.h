/* vm.h */

#ifndef VM_H_FILE
#define VM_H_FILE

#include "fh_internal.h"
#include "stack.h"
#include "value.h"

struct fh_vm_call_frame {
    struct fh_closure *closure;
    int base;
    uint32_t *ret_addr;
    int stack_top;
};

DECLARE_STACK(call_frame_stack, struct fh_vm_call_frame);

struct fh_program;

// Hot loop tracking for trace-style optimization
#define MAX_HOT_LOOPS 32
#define HOT_LOOP_THRESHOLD 100  // Lower threshold for faster activation

struct fh_hot_loop {
    uint32_t *loop_start_pc;  // PC where loop begins
    uint32_t exec_count;       // Execution count
    bool is_hot;               // Above threshold?
};

struct fh_vm {
    struct fh_program *prog;
    struct fh_value *stack;
    size_t stack_size;
    struct fh_upval *open_upvals;
    struct call_frame_stack call_stack;
    uint32_t *pc;
    struct fh_src_loc last_error_loc;
    int last_error_addr;
    int last_error_frame_index;
    struct fh_value char_cache[256];

    // Hot loop optimization tracking
    struct fh_hot_loop hot_loops[MAX_HOT_LOOPS];
    int num_hot_loops;
    bool in_hot_loop;          // Currently executing hot loop?
};

void fh_init_vm(struct fh_vm *vm, struct fh_program *prog);

void fh_destroy_vm(struct fh_vm *vm);

void fh_destroy_char_cache(struct fh_vm *vm);

int fh_call_vm_function(struct fh_vm *vm, struct fh_closure *closure,
                        struct fh_value *args, int n_args, struct fh_value *ret);

int fh_run_vm(struct fh_vm *vm);

#endif /* VM_H_FILE */
