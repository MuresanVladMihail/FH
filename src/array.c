/* array.c
 *
 * Array object helpers (growth, capacity, element access), the array
 * counterpart of map.c. Declarations live in value.h / fh.h.
 */

#include <stdlib.h>

#include "program.h"
#include "value.h"
#include "fh.h"

int fh_get_array_len(const struct fh_value *val) {
    if (val->type != FH_VAL_ARRAY)
        return -1;
    return GET_OBJ_ARRAY(val->data.obj)->len;
}

struct fh_value *fh_get_array_item(struct fh_value *val, uint32_t index) {
    // if (val->type != FH_VAL_ARRAY)
    // return NULL;

    const struct fh_array *arr = GET_OBJ_ARRAY(val->data.obj);
    if (index >= arr->len)
        return NULL;
    return &arr->items[index];
}

void fh_reset_array(struct fh_array *arr) {
    for (int i = 0; i < arr->len; i++) {
        arr->items[i].type = FH_VAL_NULL;
    }
    arr->len = 0;
}

int fh_reserve_array_capacity(struct fh_program *prog, struct fh_array *arr, uint32_t min_cap) {
    if (min_cap <= arr->cap)
        return 0;

    size_t new_cap = arr->cap ? arr->cap : 8;
    while (new_cap < min_cap)
        new_cap *= 2;

    void *new_items = realloc(arr->items, new_cap * sizeof(struct fh_value));
    if (!new_items) {
        fh_set_error(prog, "out of memory");
        return -1;
    }

    arr->items = new_items;
    arr->cap = (uint32_t) new_cap;
    return 0;
}

struct fh_value *fh_grow_array_object_uninit(struct fh_program *prog, struct fh_array *arr, const uint32_t num_items) {
    const uint32_t len = arr->len;
    if (len < arr->cap) {
        arr->len = len + 1;
        return &arr->items[len];
    }

    const size_t need = (size_t) arr->len + num_items;
    if (need > UINT32_MAX) {
        fh_set_error(prog, "out of memory");
        return NULL;
    }

    if (need > arr->cap) {
        size_t new_cap = arr->cap ? arr->cap : 16;
        while (new_cap < need) new_cap *= 2;
        void *new_items = realloc(arr->items, new_cap * sizeof(struct fh_value));
        if (!new_items) {
            fh_set_error(prog, "out of memory");
            return NULL;
        }
        arr->items = new_items;
        arr->cap = (uint32_t) new_cap;
    }

    struct fh_value *ret = &arr->items[arr->len];
    arr->len = need;
    return ret;
}

struct fh_value *fh_grow_array_object(struct fh_program *prog, struct fh_array *arr, uint32_t num_items) {
    if (arr->header.type != FH_VAL_ARRAY)
        return NULL;

    if ((size_t) arr->len + num_items + 15 < (size_t) arr->len
        || (size_t) arr->len + num_items + 15 > UINT32_MAX) {
        fh_set_error(prog, "out of memory");
        return NULL;
    }
    if (arr->len + num_items >= arr->cap) {
        const size_t new_cap = ((size_t) arr->len + num_items + 15) / 16 * 16;
        void *new_items = realloc(arr->items, new_cap * sizeof(struct fh_value));
        if (!new_items) {
            fh_set_error(prog, "out of memory");
            return NULL;
        }
        arr->items = new_items;
        arr->cap = (uint32_t) new_cap;
    }
    struct fh_value *ret = &arr->items[arr->len];
    for (uint32_t i = 0; i < num_items; i++) {
        ret[i].type = FH_VAL_NULL;
    }
    arr->len += num_items;
    return ret;
}

struct fh_value *fh_grow_array(struct fh_program *prog, struct fh_value *val, uint32_t num_items) {
    return fh_grow_array_object(prog, GET_OBJ_ARRAY(val->data.obj), num_items);
}
