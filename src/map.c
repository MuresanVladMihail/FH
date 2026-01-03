/* map.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

#define OCCUPIED(e) ((e)->key.type != FH_VAL_NULL)

static uint32_t reduce_to_cap(uint32_t h, size_t cap) {
    if ((cap & (cap - 1)) == 0) {
        return h & (uint32_t) (cap - 1);
    }
    return (uint32_t) (((uint64_t) h * (uint64_t) cap) >> 32);
}

static uint32_t val_hash(const struct fh_value *val, uint32_t cap) {
    uint32_t h;

    switch (val->type) {
        case FH_VAL_STRING:
            h = GET_VAL_STRING(val)->hash;
            break;

        case FH_VAL_BOOL:
            h = fh_hash(&val->data.b, sizeof(bool));
            break;

        case FH_VAL_FLOAT:
            h = fh_hash(&val->data.num, sizeof(double));
            break;

        case FH_VAL_C_FUNC:
            h = fh_hash(&val->data.c_func, sizeof(fh_c_func));
            break;

        case FH_VAL_ARRAY:
        case FH_VAL_MAP:
        case FH_VAL_UPVAL:
        case FH_VAL_CLOSURE:
        case FH_VAL_C_OBJ:
        case FH_VAL_FUNC_DEF:
            // identity-based hashing (pointer)
            h = fh_hash(&val->data.obj, sizeof(void *));
            break;

        default:
            h = 0;
            break;
    }

    return reduce_to_cap(h, cap);
}


static uint32_t find_slot(struct fh_map_entry *entries, uint32_t cap, struct fh_value *key) {
    uint32_t i = val_hash(key, cap);
    while (OCCUPIED(&entries[i]) && !fh_vals_are_equal(key, &entries[i].key))
        i = (i + 1) & (cap - 1);
    return i;
}

static inline uint32_t find_empty_slot(struct fh_map_entry *entries, uint32_t cap, uint32_t i) {
    while (OCCUPIED(&entries[i])) {
        i = (i + 1) & (cap - 1);
    }
    return i;
}

static inline uint32_t hash_to_index_pow2(const struct fh_value *key, uint32_t cap) {
    uint32_t h;
    switch (key->type) {
        case FH_VAL_STRING: h = GET_VAL_STRING(key)->hash;
            break;
        case FH_VAL_BOOL: h = fh_hash(&key->data.b, sizeof(bool));
            break;
        case FH_VAL_FLOAT: h = fh_hash(&key->data.num, sizeof(double));
            break;
        case FH_VAL_C_FUNC: h = fh_hash(&key->data.c_func, sizeof(fh_c_func));
            break;
        default: h = fh_hash(&key->data.obj, sizeof(void *));
            break;
    }
    return h & (cap - 1); // cap power-of-two
}

static int rebuild(struct fh_map *map, uint32_t cap) {
    struct fh_map_entry *entries = calloc((size_t) cap, sizeof(*entries));
    if (!entries) return -1;

    for (uint32_t i = 0; i < map->cap; i++) {
        struct fh_map_entry *e = &map->entries[i];
        if (!OCCUPIED(e)) continue;

        uint32_t idx = hash_to_index_pow2(&e->key, cap);
        idx = find_empty_slot(entries, cap, idx);

        entries[idx] = *e;
    }

    free(map->entries);
    map->entries = entries;
    map->cap = cap;
    return 0;
}

void fh_dump_map(struct fh_map *map) {
    for (uint32_t i = 0; i < map->cap; i++) {
        struct fh_map_entry *e = &map->entries[i];
        printf("[%3u] ", i);
        if (e->key.type == FH_VAL_NULL) {
            printf("--\n");
        } else {
            fh_dump_value(&e->key);
            printf(" -> ");
            fh_dump_value(&e->val);
            printf("\n");
        }
    }
}

void fh_extends_map(struct fh_program *prog, struct fh_map *map, struct fh_map *from) {
    for (uint32_t i = 0; i < from->cap; i++) {
        struct fh_map_entry *e = &from->entries[i];

        if (OCCUPIED(e)) {
            if (map->cap > 0) {
                const uint32_t index = find_slot(map->entries, map->cap, &e->key);
                /* We want to prevent from overriding child classes with the parent one (the extend) */
                if (!OCCUPIED(&map->entries[index]))
                    fh_add_map_object_entry(prog, map, &e->key, &e->val);
            } else {
                fh_add_map_object_entry(prog, map, &e->key, &e->val);
            }
        }
    }
}

int fh_get_map_object_value(struct fh_map *map, struct fh_value *key, struct fh_value *val) {
    if (map->cap == 0)
        return -1;

    const uint32_t i = find_slot(map->entries, map->cap, key);
    if (!OCCUPIED(&map->entries[i]))
        return -1;
    *val = map->entries[i].val;
    return 0;
}

int fh_add_map_object_entry(struct fh_program *prog, struct fh_map *map,
                            struct fh_value *key, struct fh_value *val) {
    if (key->type == FH_VAL_NULL) {
        fh_set_error(prog, "can't insert null key in map");
        return -1;
    }

    uint32_t i = 0;
    if (map->cap > 0) {
        i = find_slot(map->entries, map->cap, key);
        if (OCCUPIED(&map->entries[i])) {
            map->entries[i].val = *val;
            return 0;
        }
    }

    if (map->cap == 0 || (map->len + 1) * 4 > map->cap * 3) {
        if (rebuild(map, (map->cap == 0) ? 16 : map->cap << 1) < 0) {
            fh_set_error(prog, "out of memory");
            return -1;
        }
        i = find_slot(map->entries, map->cap, key);
    }
    map->len++;
    map->entries[i].key = *key;
    map->entries[i].val = *val;
    return 0;
}

int fh_next_map_object_key(struct fh_map *map, struct fh_value *key, struct fh_value *next_key) {
    uint32_t next_i;
    if (key->type == FH_VAL_NULL || map->cap == 0) {
        next_i = 0;
    } else {
        next_i = find_slot(map->entries, map->cap, key);
        if (OCCUPIED(&map->entries[next_i]))
            next_i++;
    }

    for (uint32_t i = next_i; i < map->cap; i++) {
        if (OCCUPIED(&map->entries[i])) {
            *next_key = map->entries[i].key;
            return 0;
        }
    }
    *next_key = fh_new_null();
    return 0;
}

int fh_delete_map_object_entry(struct fh_map *map, struct fh_value *key) {
    if (map->cap == 0)
        return -1;

    //printf("--------------------------\n");
    //printf("DELETING "); fh_dump_value(key);
    //printf("\nBEFORE:\n"); fh_dump_map(map);

    uint32_t i = find_slot(map->entries, map->cap, key);
    if (!OCCUPIED(&map->entries[i]))
        return -1;
    uint32_t j = i;
    while (true) {
        map->entries[i].key.type = FH_VAL_NULL;
    start:
        j = (j + 1) & (map->cap - 1);
        if (!OCCUPIED(&map->entries[j]))
            break;
        const uint32_t k = val_hash(&map->entries[j].key, map->cap);
        if ((i < j) ? (i < k) && (k <= j) : (i < k) || (k <= j))
            goto start;
        map->entries[i] = map->entries[j];
        i = j;
    }
    map->len--;

    //printf("AFTER:\n"); fh_dump_map(map);
    //printf("--------------------------\n");
    return 0;
}

static int map_reserve_empty(struct fh_map *map, uint32_t len_pow2_cap) {
    size_t cap_size = len_pow2_cap * sizeof(struct fh_map_entry);
    struct fh_map_entry *entries = malloc(cap_size);
    if (!entries) return -1;
    memset(entries, 0, cap_size);
    free(map->entries);
    map->entries = entries;
    map->cap = len_pow2_cap;
    return 0;
}

int fh_alloc_map_object_len(struct fh_map *map, const uint32_t len) {
    if (map->cap == 0) return map_reserve_empty(map, len << 1);
    return rebuild(map, len << 1);
}

/* value functions */

int fh_alloc_map_len(struct fh_value *map, uint32_t len) {
    struct fh_map *m = GET_VAL_MAP(map);
    if (!m)
        return -1;
    return fh_alloc_map_object_len(m, len);
}

int fh_delete_map_entry(struct fh_value *map, struct fh_value *key) {
    struct fh_map *m = GET_VAL_MAP(map);
    if (!m)
        return -1;
    return fh_delete_map_object_entry(m, key);
}

int fh_next_map_key(struct fh_value *map, struct fh_value *key, struct fh_value *next_key) {
    struct fh_map *m = GET_VAL_MAP(map);
    if (!m)
        return -1;
    return fh_next_map_object_key(m, key, next_key);
}

int fh_get_map_value(struct fh_value *map, struct fh_value *key, struct fh_value *val) {
    struct fh_map *m = map->data.obj; //GET_VAL_MAP(map);
    if (!m)
        return -1;
    return fh_get_map_object_value(m, key, val);
}

int fh_add_map_entry(struct fh_program *prog, struct fh_value *map,
                     struct fh_value *key, struct fh_value *val) {
    struct fh_map *m = map->data.obj; //GET_VAL_MAP(map);
    if (!m)
        return -1;
    return fh_add_map_object_entry(prog, m, key, val);
}

void fh_reset_map(struct fh_map *map) {
    for (uint32_t i = 0; i < map->cap; i++) {
        struct fh_map_entry *e = &map->entries[i];
        e->val.type = FH_VAL_NULL;
        e->key.type = FH_VAL_NULL;
    }
    map->len = 0;
}
