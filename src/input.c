/* input.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "fh.h"

struct string_input_data {
    char *data;
    size_t len;
};

struct fh_input {
    void *user_data;
    struct fh_input_funcs *funcs;
};

struct fh_input *fh_new_input(const char *filename, void *user_data,
                              struct fh_input_funcs *funcs) {
    struct fh_input *in = malloc(sizeof(struct fh_input) + strlen(filename) + 1);
    if (!in)
        return NULL;
    in->user_data = user_data;
    in->funcs = funcs;
    strcpy((char *)in + sizeof(struct fh_input), filename);
    return in;
}

const char *fh_get_input_filename(struct fh_input *in) {
    return (char *) in + sizeof(struct fh_input);;
}

void *fh_get_input_user_data(struct fh_input *in) {
    return in->user_data;
}

static int is_abs_path(const char *p) {
    if (!p || !p[0]) return 0;
    if (p[0] == '/' || p[0] == '\\') return 1;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
        p[1] == ':' && (p[2] == '\\' || p[2] == '/'))
        return 1;
    return 0;
}

struct fh_input *fh_open_input(struct fh_input *in, const char *filename) {
    /*
     * If 'filename' is not an absolute path, base it on the directory
     * of the parent input
     */
    if (!is_abs_path(filename)) {
        const char *base_filename = fh_get_input_filename(in);
        const char *last_slash = strrchr(base_filename, '/');
        if (!last_slash)
            last_slash = strrchr(base_filename, '\\');
        if (last_slash) {
            size_t base_len = last_slash + 1 - base_filename;

            char path[1024];
            if (base_len + strlen(filename) + 1 > sizeof(path))
                return NULL; // path is too big
            memcpy(path, base_filename, base_len);
            strcpy(path + base_len, filename);
            return in->funcs->open(in, path);
        }
    }
    return in->funcs->open(in, filename);
}

int fh_close_input(struct fh_input *in) {
    const int ret = in->funcs->close(in);
    free(in);
    return ret;
}

int fh_read_input(struct fh_input *in, char *line, int max_len) {
    return in->funcs->read(in, line, max_len);
}

/* ======================================= */
/* === file input ======================== */

static int file_read(struct fh_input *in, char *buf, int max_len) {
    FILE *f = in->user_data;
    const size_t n = fread(buf, 1, max_len, f);

    if (n > 0) return (int) n;

    if (feof(f)) return 0; // EOF clean
    if (ferror(f)) return -1; // real error
    return 0;
}

static int file_close(struct fh_input *in) {
    FILE *f = in->user_data;

    return fclose(f);
}

static struct fh_input *file_open(struct fh_input *in, const char *filename) {
    (void) in;
    return fh_open_input_file(filename);
}

struct fh_input *fh_open_input_file(const char *filename) {
    static struct fh_input_funcs file_input_funcs = {
        .open = file_open,
        .read = file_read,
        .close = file_close,
    };

    FILE *f = fopen(filename, "rb");
    if (!f)
        return NULL;

    struct fh_input *in = fh_new_input(filename, f, &file_input_funcs);
    if (!in) {
        fclose(f);
        return NULL;
    }
    return in;
}

/* ======================================= */
/* === pack input ====================== */

static int pack_read(struct fh_input *in, char *line, int max_len) {
    if (!in || !line || max_len <= 0) return -1;
    struct string_input_data *str = in->user_data;
    if (!str) return -1;

    if (str->len == 0)
        return 0; /* EOF */

    int len = (max_len < str->len) ? max_len : str->len;
    memcpy(line, str->data, len);
    str->data += len;
    str->len -= len;
    return len;
}

static int pack_close(struct fh_input *in) {
    struct string_input_data *str = in->user_data;
    free(str);
    return 0;
}

static struct fh_input *pack_open(struct fh_input *in, const char *filename) {
    (void) in;
    return fh_open_input_pack(filename);
}

struct fh_input *fh_open_input_pack(const char *path) {
    static struct fh_input_funcs file_input_funcs = {
        .open = pack_open,
        .read = pack_read,
        .close = pack_close,
    };

    if (mtar_find(&fh_tar, path, &fh_tar_header) != MTAR_ESUCCESS)
        return NULL;
    size_t input_len = fh_tar_header.size;
    if (input_len == 0 || input_len > INT_MAX)
        return NULL;
    char *input = malloc(input_len);
    if (!input)
        return NULL;

    memset(input, 0, input_len);
    if (mtar_read_data(&fh_tar, input, input_len) != 0) {
        free(input);
        return NULL;
    }


    struct string_input_data *str = malloc(sizeof(struct string_input_data) + input_len);
    if (!str) {
        free(input);
        return NULL;
    }
    str->data = (char *) str + sizeof(struct string_input_data);
    str->len = input_len;
    memcpy(str->data, input, input_len);

    struct fh_input *in = fh_new_input(path, str, &file_input_funcs);
    if (!in) {
        free(input);
        free(str);
        return NULL;
    }

    free(input);
    return in;
}


/* ======================================= */
/* === string input ====================== */

static int string_read(struct fh_input *in, char *line, int max_len) {
    if (!in || !line || max_len <= 0) return -1;
    struct string_input_data *str = in->user_data;
    if (!str) return -1;

    if (str->len == 0)
        return 0; /* EOF */

    int len = (max_len < str->len) ? max_len : str->len;
    memcpy(line, str->data, len);
    str->data += len;
    str->len -= len;
    return len;
}

static int string_close(struct fh_input *in) {
    free(in->user_data);
    return 0;
}

static struct fh_input *string_open(struct fh_input *in, const char *filename) {
    (void) in;
    return fh_open_input_file(filename);
}

struct fh_input *fh_open_input_string(const char *input) {
    static struct fh_input_funcs string_input_funcs = {
        .open = string_open,
        .read = string_read,
        .close = string_close,
    };

    size_t input_len = strlen(input);
    if (input_len > INT_MAX)
        return NULL;
    struct string_input_data *str = malloc(sizeof(struct string_input_data) + input_len + 1);
    if (!str)
        return NULL;
    str->data = (char *) str + sizeof(struct string_input_data);
    str->len = input_len;
    memcpy(str->data, input, input_len);
    str->data[input_len] = '\0';


    struct fh_input *in = fh_new_input("(string)", str, &string_input_funcs);
    if (!in) {
        free(str);
        return NULL;
    }
    return in;
}

