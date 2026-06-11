/* pool.c
 *
 * Per-program free lists for small blocks (GC objects and map entry
 * arrays). Heavy object churn otherwise hammers malloc/free; recycling
 * dead blocks makes allocation a pointer pop in the common case.
 * Freed blocks store the next-pointer in their first bytes. Blocks are
 * only returned to the OS by fh_pool_drain (program teardown).
 */

#include <stdlib.h>

#include "pool.h"
#include "program.h"

static const size_t pool_class_size[FH_NUM_POOL_CLASSES] = {64, 128, 256, 512};

static inline int pool_class(const size_t size) {
    if (size <= 64) return 0;
    if (size <= 128) return 1;
    if (size <= 256) return 2;
    if (size <= 512) return 3;
    return -1;
}

void *fh_pool_alloc(struct fh_program *prog, const size_t size) {
    const int c = pool_class(size);
    if (c < 0)
        return malloc(size);
    void *p = prog->small_pool[c];
    if (p) {
        prog->small_pool[c] = *(void **) p;
        return p;
    }
    return malloc(pool_class_size[c]);
}

void fh_pool_free(struct fh_program *prog, void *ptr, const size_t size) {
    if (!ptr)
        return;
    const int c = pool_class(size);
    if (c < 0) {
        free(ptr);
        return;
    }
    *(void **) ptr = prog->small_pool[c];
    prog->small_pool[c] = ptr;
}

void fh_pool_drain(struct fh_program *prog) {
    for (int c = 0; c < FH_NUM_POOL_CLASSES; c++) {
        void *p = prog->small_pool[c];
        while (p) {
            void *next = *(void **) p;
            free(p);
            p = next;
        }
        prog->small_pool[c] = NULL;
    }
}
