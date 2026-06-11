/* pool.h
 *
 * Small-allocation pool: per-program free lists for small blocks
 * (GC objects and map entry arrays). See pool.c for details.
 */

#ifndef POOL_H_FILE
#define POOL_H_FILE

#include <stddef.h>

struct fh_program;

void *fh_pool_alloc(struct fh_program *prog, size_t size);

void fh_pool_free(struct fh_program *prog, void *ptr, size_t size);

void fh_pool_drain(struct fh_program *prog);

#endif /* POOL_H_FILE */
