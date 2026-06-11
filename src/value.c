/* value.c */

#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"
#include "pool.h"
#include "fh.h"

static void free_func_def(struct fh_program *prog, struct fh_func_def *func_def) {
    if (func_def->consts)
        free(func_def->consts);
    if (func_def->code)
        free(func_def->code);
    if (func_def->upvals)
        free(func_def->upvals);
    if (func_def->code_src_loc)
        free(func_def->code_src_loc);
    fh_pool_free(prog, func_def, sizeof(struct fh_func_def));
}

static void free_closure(struct fh_program *prog, struct fh_closure *closure) {
    fh_pool_free(prog, closure,
                 sizeof(struct fh_closure) + closure->n_upvals * sizeof(struct fh_upval *));
}

static void free_upval(struct fh_program *prog, struct fh_upval *upval) {
    fh_pool_free(prog, upval, sizeof(struct fh_upval));
}

static void free_array(struct fh_program *prog, struct fh_array *arr) {
    if (arr->items)
        free(arr->items);
    fh_pool_free(prog, arr, sizeof(struct fh_array));
}

static void free_map(struct fh_program *prog, struct fh_map *map) {
    if (map->entries)
        fh_pool_free(prog, map->entries, map->cap * sizeof(struct fh_map_entry));
    fh_pool_free(prog, map, sizeof(struct fh_map));
}


// Must mirror the sizes passed to fh_make_object for each type.
static size_t object_alloc_size(const union fh_object *obj) {
    switch (obj->header.type) {
        case FH_VAL_C_OBJ: return sizeof(struct fh_c_obj);
        case FH_VAL_STRING: return sizeof(struct fh_string) + obj->str.size;
        case FH_VAL_CLOSURE:
            return sizeof(struct fh_closure) + obj->closure.n_upvals * sizeof(struct fh_upval *);
        case FH_VAL_UPVAL: return sizeof(struct fh_upval);
        case FH_VAL_FUNC_DEF: return sizeof(struct fh_func_def);
        case FH_VAL_ARRAY: return sizeof(struct fh_array);
        case FH_VAL_MAP: return sizeof(struct fh_map);
        default: return 0;
    }
}

void fh_free_object(struct fh_program *prog, union fh_object *obj) {
    prog->alive_objects--;
    const size_t obj_alloc_size = object_alloc_size(obj);
    if (prog->gc_live_bytes >= obj_alloc_size)
        prog->gc_live_bytes -= obj_alloc_size;
    else
        prog->gc_live_bytes = 0;

    switch (obj->header.type) {
        case FH_VAL_NULL:
        case FH_VAL_BOOL:
        case FH_VAL_FLOAT:
        case FH_VAL_INTEGER:
        case FH_VAL_C_FUNC:
            fprintf(stderr, "**** ERROR: freeing object of NON-OBJECT type %d\n", obj->header.type);
            free(obj);
            return;

        case FH_VAL_C_OBJ: {
            if (obj->c_obj.free_callback) {
                obj->c_obj.free_callback(obj->c_obj.ptr);
            }
            fh_pool_free(prog, obj, sizeof(struct fh_c_obj));
            return;
        }
        case FH_VAL_STRING:
            fh_pool_free(prog, obj, sizeof(struct fh_string) + obj->str.size);
            return;
        case FH_VAL_CLOSURE: free_closure(prog, GET_OBJ_CLOSURE(obj));
            return;
        case FH_VAL_UPVAL: free_upval(prog, GET_OBJ_UPVAL(obj));
            return;
        case FH_VAL_FUNC_DEF: free_func_def(prog, GET_OBJ_FUNC_DEF(obj));
            return;
        case FH_VAL_ARRAY: free_array(prog, GET_OBJ_ARRAY(obj));
            return;
        case FH_VAL_MAP: free_map(prog, GET_OBJ_MAP(obj));
            return;
    }

    fprintf(stderr, "**** ERROR: freeing object of INVALID type %d\n", obj->header.type);
    free(obj);
}


const char *fh_get_string(const struct fh_value *val) {
    if (val->type != FH_VAL_STRING)
        return NULL;
    return GET_OBJ_STRING_DATA(val->data.obj);
}

const char *fh_get_func_def_name(struct fh_func_def *func_def) {
    if (func_def->header.type != FH_VAL_FUNC_DEF || !func_def->name)
        return NULL;
    return GET_OBJ_STRING_DATA(func_def->name);
}

/*************************************************************************
 * OBJECT CREATION
 *
 * The following functions create a new object and adds it to the list
 * of program objects.
 *************************************************************************/

static void *fh_make_object(struct fh_program *prog, const bool pinned, const enum fh_value_type type,
                            const size_t size) {
    // Adaptive GC trigger: collect only after allocating at least as many
    // bytes as the live heap holds (and at least gc_collect_at). This keeps
    // mark-and-sweep cost proportional to allocation instead of re-walking a
    // large live heap every gc_collect_at bytes.
    if (prog->gc_frequency >= prog->gc_collect_at &&
        prog->gc_frequency >= prog->gc_live_bytes) {
        fh_collect_garbage(prog);
        prog->gc_frequency = 0;
    }

    union fh_object *obj = fh_pool_alloc(prog, size);
    if (!obj) {
        fh_set_error(prog, "out of memory");
        return NULL;
    }
    if (pinned && vec_push(&prog->pinned_objs, obj) != 0) {
        fh_pool_free(prog, obj, size);
        fh_set_error(prog, "out of memory");
        return NULL;
    }

    obj->header.next = prog->objects;
    prog->objects = obj;
    obj->header.type = type;
    obj->header.gc_bits = 0;
    prog->gc_frequency += size;
    prog->gc_live_bytes += size;

    prog->alive_objects++;
    return obj;
}

struct fh_upval *fh_make_upval(struct fh_program *prog, bool pinned) {
    struct fh_upval *uv = fh_make_object(prog, pinned, FH_VAL_UPVAL, sizeof(struct fh_upval));
    if (!uv)
        return NULL;
    uv->gc_next_container = NULL;
    return uv;
}

struct fh_closure *fh_make_closure(struct fh_program *prog, bool pinned,
                                   struct fh_func_def *func_def) {
    struct fh_closure *c = fh_make_object(prog, pinned, FH_VAL_CLOSURE,
                                          sizeof(struct fh_closure) + func_def->n_upvals * sizeof(struct fh_upval *));
    if (!c)
        return NULL;
    c->gc_next_container = NULL;
    c->func_def = func_def;
    c->n_upvals = func_def->n_upvals;
    c->doc_string = NULL;
    return c;
}

struct fh_func_def *fh_make_func_def(struct fh_program *prog, bool pinned) {
    struct fh_func_def *func_def = fh_make_object(prog, pinned, FH_VAL_FUNC_DEF, sizeof(struct fh_func_def));
    if (!func_def)
        return NULL;
    func_def->gc_next_container = NULL;
    return func_def;
}

struct fh_array *fh_make_array(struct fh_program *prog, const bool pinned) {
    struct fh_array *arr = fh_make_object(prog, pinned, FH_VAL_ARRAY, sizeof(struct fh_array));
    if (!arr)
        return NULL;
    arr->gc_next_container = NULL;
    arr->len = 0;
    arr->cap = 0;
    arr->items = NULL;
    return arr;
}

struct fh_map *fh_make_map(struct fh_program *prog, const bool pinned) {
    struct fh_map *map = fh_make_object(prog, pinned, FH_VAL_MAP, sizeof(struct fh_map));
    if (!map)
        return NULL;
    map->gc_next_container = NULL;
    map->len = 0;
    map->cap = 0;
    map->entries = NULL;
    return map;
}

struct fh_c_obj *fh_make_c_obj(struct fh_program *prog, bool pinned,
                               void *ptr, fh_c_obj_gc_callback callback) {
    struct fh_c_obj *o = fh_make_object(prog, pinned, FH_VAL_C_OBJ, sizeof(struct fh_c_obj));
    if (!o)
        return NULL;
    o->gc_next_container = NULL;
    o->ptr = ptr;
    o->free_callback = callback;
    return o;
}

struct fh_string *fh_make_string_n(struct fh_program *prog, bool pinned,
                                   const char *str, size_t str_len) {
    const size_t size = sizeof(struct fh_string) + str_len;
    if (size > UINT32_MAX)
        return NULL;
    struct fh_string *s = fh_make_object(prog, pinned, FH_VAL_STRING, size);
    if (!s)
        return NULL;
    memcpy(GET_OBJ_STRING_DATA(s), str, str_len);
    s->size = (uint32_t) str_len;
    s->hash = fh_hash(str, str_len);
    return s;
}

struct fh_string *fh_make_string(struct fh_program *prog, bool pinned, const char *str) {
    return fh_make_string_n(prog, pinned, str, strlen(str) + 1);
}

/*************************************************************************
 * C INTERFACE FUNCTIONS
 *
 * The following functions create a new value and, if the value is an
 * object, add the object to the C temp array to keep it anchored
 * while the C function is running.
 *************************************************************************/
/**
 * @brief fh_new_c_obj Maps a user defined pointer to a useful FH value
 * @param prog the program to which to bind the ptr
 * @param ptr the actual data you want to save
 * @param callback called when the object is about to be deleted, you may pass NULL if you don't want to do something about it
 * @param type *USER* defined type to later recognize the ptr. THIS SHOULD NOT BE A FH TYPE, eg: FH_VAL_FLOAT/INTEGER/STRING !
 * @return a new fh_value which holds the ptr of the user defined object
 */
struct fh_value fh_new_c_obj(struct fh_program *prog, void *ptr, fh_c_obj_gc_callback callback, int type) {
    struct fh_c_obj *o = fh_make_c_obj(prog, false, ptr, callback);
    if (!o) return prog->null_value;

    o->type = type;

    return (struct fh_value){
        .type = FH_VAL_C_OBJ,
        .data = {.obj = o},
    };
}

struct fh_value fh_new_string(struct fh_program *prog, const char *str) {
    return fh_new_string_n(prog, str, strlen(str) + 1);
}

struct fh_value fh_new_string_n(struct fh_program *prog, const char *str, size_t str_len) {
    struct fh_value *val = malloc(sizeof(struct fh_value));
    if (!val) {
        fh_set_error(prog, "out of memory");
        return prog->null_value;
    }
    struct fh_string *s = fh_make_string_n(prog, false, str, str_len);
    if (!s) {
        // value_stack_pop(&prog->c_vals, NULL);
        free(val);
        return prog->null_value;
    }
    val->type = FH_VAL_STRING;
    val->data.obj = s;

    vec_push(&prog->c_vals, val);

    return *val;
}

struct fh_value fh_new_array(struct fh_program *prog) {
    struct fh_value *val = malloc(sizeof(struct fh_value));
    if (!val) {
        fh_set_error(prog, "out of memory");
        return prog->null_value;
    }
    struct fh_array *arr = fh_make_array(prog, false);
    if (!arr) {
        free(val);
        return prog->null_value;
    }
    val->type = FH_VAL_ARRAY;
    val->data.obj = arr;

    vec_push(&prog->c_vals, val);

    return *val;
}

struct fh_value fh_new_map(struct fh_program *prog) {
    struct fh_value *val = malloc(sizeof(struct fh_value));
    if (!val) {
        fh_set_error(prog, "out of memory");
        return prog->null_value;
    }
    struct fh_map *map = fh_make_map(prog, false);
    if (!map) {
        free(val);
        return prog->null_value;
    }
    val->type = FH_VAL_MAP;
    val->data.obj = map;

    vec_push(&prog->c_vals, val);

    return *val;
}

const char *fh_type_to_str(struct fh_program *prog, enum fh_value_type type) {
    switch (type) {
        case FH_VAL_NULL:
            return "null";
        case FH_VAL_BOOL:
            return "bool";
        case FH_VAL_FLOAT:
            return "number";
        case FH_VAL_INTEGER:
            return "integer";
        case FH_VAL_C_FUNC:
            return "cfunc";
        case FH_VAL_C_OBJ:
            return "cobj";
        case FH_VAL_STRING:
            return "string";
        case FH_VAL_ARRAY:
            return "array";
        case FH_VAL_MAP:
            return "map";
        case FH_VAL_CLOSURE:
            return "closure";
        case FH_VAL_FUNC_DEF:
            return "funcdef";
        default:
            fh_set_error(prog, "can't get type for object!");
            return "";
    }
}
