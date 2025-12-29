/**
 * Copyright (c) 2014 rxi
 *
 * Modified for runtime-grade safety/perf (size_t, overflow checks, fixed macros)
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef VEC_H
#define VEC_H

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define VEC_VERSION "0.3.0"

/* Internal helper: unpack vector fields for generic functions */
#define vec_unpack_(v) \
  (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)

/* Vector type */
#define vec_t(T) \
  struct { T *data; size_t length, capacity; }

/* Init / deinit */
#define vec_init(v) \
  memset((v), 0, sizeof(*(v)))

#define vec_deinit(v) \
  ( free((v)->data), vec_init(v) )

/* Basic ops */
#define vec_clear(v) \
  ((v)->length = 0)

#define vec_truncate(v, len) \
  ((v)->length = (size_t)(len) < (v)->length ? (size_t)(len) : (v)->length)

#define vec_first(v) \
  ((v)->data[0])

#define vec_last(v) \
  ((v)->data[(v)->length - 1])

#define vec_pop(v) \
  ((v)->data[--(v)->length])

/* Safe variants (recommended in VM/runtime code) */
#define vec_pop_safe(v, out_ptr) \
  ( ((v)->length == 0) ? -1 : (*(out_ptr) = (v)->data[--(v)->length], 0) )

#define vec_last_safe(v, out_ptr) \
  ( ((v)->length == 0) ? -1 : (*(out_ptr) = (v)->data[(v)->length - 1], 0) )

/* Reserve/compact */
#define vec_reserve(v, n) \
  vec_reserve_(vec_unpack_(v), (size_t)(n))

#define vec_compact(v) \
  vec_compact_(vec_unpack_(v))

/* Push/insert */
#define vec_push(v, val) \
  ( vec_expand_(vec_unpack_(v)) ? -1 : ((v)->data[(v)->length++] = (val), 0) )

#define vec_insert(v, idx, val) \
  ( vec_insert_(vec_unpack_(v), (size_t)(idx)) ? -1 : ((v)->data[(idx)] = (val), (v)->length++, 0) )

/* Splice/swapsplice */
#define vec_splice(v, start, count) \
  ( vec_splice_(vec_unpack_(v), (size_t)(start), (size_t)(count)) ? -1 : 0 )

#define vec_swapsplice(v, start, count) \
  ( vec_swapsplice_(vec_unpack_(v), (size_t)(start), (size_t)(count)) ? -1 : 0 )

/* Swap */
#define vec_swap(v, idx1, idx2) \
  vec_swap_(vec_unpack_(v), (size_t)(idx1), (size_t)(idx2))

/* Sort */
#define vec_sort(v, fn) \
  qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)

/* Push array / extend */
#define vec_pusharr(v, arr, count) \
  do { \
    size_t n__ = (size_t)(count); \
    if (n__ == 0) break; \
    if (vec_reserve_po2_(vec_unpack_(v), (v)->length + n__) != 0) break; \
    memcpy(&(v)->data[(v)->length], (arr), n__ * sizeof(*(v)->data)); \
    (v)->length += n__; \
  } while (0)

#define vec_extend(v, v2) \
  vec_pusharr((v), (v2)->data, (v2)->length)

/* Find/remove (pointer equality for pointer vectors; value equality otherwise) */
#define vec_find(v, val, idx) \
  do { \
    size_t _i; \
    (idx) = (size_t)-1; \
    for (_i = 0; _i < (v)->length; _i++) { \
      if ((v)->data[_i] == (val)) { (idx) = _i; break; } \
    } \
  } while (0)

#define vec_remove(v, val) \
  do { \
    size_t idx__; \
    vec_find((v), (val), idx__); \
    if (idx__ != (size_t)-1) vec_splice((v), idx__, 1); \
  } while (0)

/* Reverse */
#define vec_reverse(v) \
  do { \
    size_t i__ = (v)->length / 2; \
    while (i__--) { \
      vec_swap((v), i__, (v)->length - (i__ + 1)); \
    } \
  } while (0)

/* Foreach helpers (safe for signed/unsigned iter) */
#define vec_foreach(v, var, iter) \
  for ((iter) = 0; (iter) < (v)->length && (((var) = (v)->data[(iter)]), 1); ++(iter))

#define vec_foreach_ptr(v, var, iter) \
  for ((iter) = 0; (iter) < (v)->length && (((var) = &(v)->data[(iter)]), 1); ++(iter))

/* Reverse iteration without relying on iter being signed */
#define vec_foreach_rev(v, var, iter) \
  for ((iter) = (v)->length; (iter)-- > 0 && (((var) = (v)->data[(iter)]), 1); )

#define vec_foreach_ptr_rev(v, var, iter) \
  for ((iter) = (v)->length; (iter)-- > 0 && (((var) = &(v)->data[(iter)]), 1); )

/* API: internal generic functions (return 0 on success, -1 on error) */
int vec_expand_(char **data, size_t *length, size_t *capacity, size_t memsz);

int vec_reserve_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t n);

int vec_reserve_po2_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t n);

int vec_compact_(char **data, size_t *length, size_t *capacity, size_t memsz);

int vec_insert_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t idx);

int vec_splice_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t start, size_t count);

int vec_swapsplice_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t start, size_t count);

void vec_swap_(char **data, size_t *length, size_t *capacity, size_t memsz, size_t idx1, size_t idx2);

/* Common typedefs */
typedef vec_t(void*) vec_void_t;

typedef vec_t(char*) vec_str_t;

typedef vec_t(int) vec_int_t;

typedef vec_t(char) vec_char_t;

typedef vec_t(float) vec_float_t;

typedef vec_t(double) vec_double_t;

#endif /* VEC_H */
