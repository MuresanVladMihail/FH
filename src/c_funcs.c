/* c_funcs.c */

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#include "fh.h"
#include "value.h"
#include "fh_internal.h"
#include "program.h"
#include "regex/re.h"
#include "crypto/md5.h"
#include "crypto/bcrypt.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON 1E-6
#endif

#define DEG_TO_RAD(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
#define RAD_TO_DEG(angleInRadians) ((angleInRadians) * 180.0 / M_PI)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_ITEM 512

#if defined(WIN32) || defined(_WIN32)
#include <direct.h>
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

/*
 * gettimeofday.c
 *    Win32 gettimeofday() replacement
 *
 * src/port/gettimeofday.c
 *
 * Copyright (c) 2003 SRA, Inc.
 * Copyright (c) 2003 SKC, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose, without fee, and without a
 * written agreement is hereby granted, provided that the above
 * copyright notice and this paragraph and the following two
 * paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS
 * IS" BASIS, AND THE AUTHOR HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <windows.h>
#include <stdint.h>
#include <io.h>
#include <tchar.h>
#include <sys/stat.h>

/* FILETIME of Jan 1 1970 00:00:00. */
static const unsigned __int64 epoch = ((unsigned __int64) 116444736000000000ULL);

/*
 * timezone information is stored outside the kernel so tzp isn't used anymore.
 *
 * Note: this function is not for Win32 high precision timing purpose. See
 * elapsed_time().
 */
int
gettimeofday(struct timeval *tp, struct timezone *tzp) {
    FILETIME file_time;
    SYSTEMTIME system_time;
    ULARGE_INTEGER ularge;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    tp->tv_sec = (uint64_t) ((ularge.QuadPart - epoch) / 10000000L);
    tp->tv_usec = (uint64_t) (system_time.wMilliseconds * 1000);

    return 0;
}
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif


#ifndef S_IFMT
#define S_IFMT 0160000 /* type of file: */
#endif
#ifndef S_IFDIR
#define S_IFDIR 0040000 /* directory */
#endif

static void print_value(struct fh_value *val) {
    if (val->type == FH_VAL_UPVAL)
        val = GET_OBJ_UPVAL(val->data.obj)->val;

    switch (val->type) {
        case FH_VAL_NULL: printf("null");
            return;
        case FH_VAL_BOOL: printf("%s", (val->data.b) ? "true" : "false");
            return;
        case FH_VAL_FLOAT: printf("%g", val->data.num);
            return;
        case FH_VAL_STRING: printf("%s", GET_VAL_STRING_DATA(val));
            return;
        case FH_VAL_ARRAY: {
            struct fh_array *v = GET_VAL_ARRAY(val);
            for (uint32_t i = 0; i < v->len; i++) {
                printf("[%u] ", i);
                fh_dump_value(&v->items[i]);
                printf("\n");
            }
            return;
        }
        case FH_VAL_MAP: {
            struct fh_map *v = GET_VAL_MAP(val);
            for (uint32_t i = 0; i < v->cap; i++) {
                struct fh_map_entry *e = &v->entries[i];
                if (e->key.type != FH_VAL_NULL) {
                    printf("[%u] ", i);

                    fh_dump_value(&e->key);
                    printf(" -> ");
                    fh_dump_value(&e->val);
                    printf("\n");
                }
            }
            return;
        }
        case FH_VAL_CLOSURE: printf("<closure %p>", val->data.obj);
            return;
        case FH_VAL_UPVAL: printf("<internal error (upval)>");
            return;
        case FH_VAL_FUNC_DEF: printf("<func def %p>", val->data.obj);
            return;
        case FH_VAL_C_FUNC: {
            printf("<C function 0x");
            unsigned char *p = (unsigned char *) &val->data.c_func;
            p += sizeof(val->data.c_func);
            for (size_t i = 0; i < sizeof(val->data.c_func); i++)
                if (*--p)
                    printf("%02x", *p);
            printf(">");
            return;
        }
        case FH_VAL_C_OBJ: {
            printf("<C obj 0x");
            unsigned char *p = (unsigned char *) &val->data.obj;
            p += sizeof(val->data.obj);
            for (size_t i = 0; i < sizeof(val->data.obj); i++)
                if (*--p)
                    printf("%02x", *p);
            printf(">");
            return;
        }
    }
    printf("<invalid value %d>", val->type);
}

static int check_n_args(struct fh_program *prog, const char *func_name, int n_expected, int n_received) {
    if (n_expected >= 0 && n_received != n_expected)
        return fh_set_error(prog, "%s: expected %d argument(s), got %d", func_name, n_expected, n_received);
    if (n_received != INT_MIN && n_received < -n_expected)
        return fh_set_error(prog, "%s: expected at least %d argument(s), got %d", func_name, -n_expected, n_received);
    return 0;
}

/*********** Math functions ***********/
static int fn_math_md5(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_string(&args[0])) {
        return fh_set_error(prog, "math_md5(): expected string as first argument, got %s",
                            fh_type_to_str(prog, args[0].type));
    }

    const char *key = fh_get_string(&args[0]);
    uint8_t *res_i = md5String((char *) key);
    char res_s[33]; // 16 * 2 + 1
    char *ptr = &res_s[0];
    for (int i = 0; i < 16; ++i) {
        /* "sprintf" converts each byte in the "buf" array into a 2 hex string
         * characters appended with a null byte, for example 10 => "0A\0".
         *
         * This string would then be added to the output array starting from the
         * position pointed at by "ptr". For example if "ptr" is pointing at the 0
         * index then "0A\0" would be written as output[0] = '0', output[1] = 'A' and
         * output[2] = '\0'.
         *
         * "sprintf" returns the number of chars in its output excluding the null
         * byte, in our case this would be 2. So we move the "ptr" location two
         * steps ahead so that the next hex string would be written at the new
         * location, overriding the null byte from the previous hex string.
         *
         * We don't need to add a terminating null byte because it's been already
         * added for us from the last hex string. */
        ptr += sprintf(ptr, "%02x", res_i[i]);
    }

    *ret = fh_new_string(prog, res_s);
    free(res_i);
    return 0;
}

static int fn_math_bcrypt_gen_salt(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_bcrypt_gen_salt(): expected number as first argument, got %s",
                            fh_type_to_str(prog, args[0].type));
    }

    char salt[BCRYPT_HASHSIZE];

    int factor = fh_get_number(&args[0]);
    if (factor < 4 || factor > 31) {
        return fh_set_error(
            prog, "math_bcrypt_gen_salt(): expected first argument, 'factor', to be between 4 and 31, got %d", factor);
    }

    if (bcrypt_gensalt(factor, salt) != 0) {
        return fh_set_error(prog, "math_bcrypt_gen_salt(): failed to generate salt");
    }

    *ret = fh_new_string(prog, salt);

    return 0;
}

static int fn_math_bcrypt_hashpw(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_bcrypt_hashpw()", 2, n_args))
        return -1;

    if (!fh_is_string(&args[0])) {
        return fh_set_error(prog, "math_bcrypt_hashpw(): expected string as first argument, got %s",
                            fh_type_to_str(prog, args[0].type));
    }
    if (!fh_is_string(&args[1])) {
        return fh_set_error(prog, "math_bcrypt_hashpw(): expected string as second argument, got %s",
                            fh_type_to_str(prog, args[1].type));
    }

    const char *passwd = fh_get_string(&args[0]);
    const char *salt = fh_get_string(&args[1]);
    char hash[BCRYPT_HASHSIZE];

    if (bcrypt_hashpw(passwd, salt, hash) != 0) {
        return fh_set_error(prog, "math_bcrypt_hashpw(): failed to hash");
    }

    *ret = fh_new_string(prog, hash);
    return 0;
}

static int fn_math_abs(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_abs(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(fabs(args[0].data.num));

    return 0;
}

static int fn_math_acons(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_acons(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(acos(args[0].data.num));

    return 0;
}

static int fn_math_asin(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_asin()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_asin(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(asin(args[0].data.num));

    return 0;
}

static int fn_math_atan(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_atan()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_atan(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(atan(args[0].data.num));

    return 0;
}

static int fn_math_atan2(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_atan2()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_atan2(): expected number, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    *ret = fh_make_number(atan2(args[0].data.num, args[1].data.num));

    return 0;
}

static int fn_math_ceil(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_ceil()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_ceil(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(ceil(args[0].data.num));

    return 0;
}

static int fn_math_cos(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_cos()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_cos(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(cos(args[0].data.num));

    return 0;
}

static int fn_math_cosh(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_cosh()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_cosh(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(cosh(args[0].data.num));

    return 0;
}

static int fn_math_deg(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_deg()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_deg(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(DEG_TO_RAD(args[0].data.num));
    return 0;
}

static int fn_math_exp(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_exp()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_exp(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(exp(args[0].data.num));

    return 0;
}

static int fn_math_floor(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_floor()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_floor(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(floor(args[0].data.num));
    return 0;
}

static int fn_math_fmod(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_fmod()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_fmod(): expected number, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    *ret = fh_make_number(fmod(args[0].data.num,args[1].data.num));
    return 0;
}

static int fn_math_frexp(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_frexp()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_frexp(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    double fract_part = 0;
    int e = 0;
    fract_part = frexp(args[0].data.num, &e);

    int pin_state = fh_get_pin_state(prog);
    struct fh_array *arr = fh_make_array(prog, true);
    struct fh_value fp = fh_new_number(fract_part);
    struct fh_value ip = fh_make_number(e);

    struct fh_value *new_items = fh_grow_array_object(prog, arr, 2);
    if (!new_items)
        return fh_set_error(prog, "out of memory");

    arr->items[1] = fp;
    arr->items[0] = ip;

    struct fh_value v = fh_new_array(prog);
    v.data.obj = arr;

    *ret = v;
    fh_restore_pin_state(prog, pin_state);
    return 0;
}

static int fn_math_huge(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "math_huge()", 0, n_args))
        return -1;

    *ret = fh_make_number(HUGE_VAL);
    return 0;
}

static int fn_math_ldexp(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_ldexp()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_ldexp(): expected number, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    double result = 0;
    int n = (int) fh_get_number(&args[1]);
    result = ldexp(args[0].data.num, n);

    *ret = fh_new_number(result);
    return 0;
}

static int fn_math_log(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_log()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_log(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(log(args[0].data.num));
    return 0;
}

static int fn_math_log10(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_log10()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_log10(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(log10(args[0].data.num));
    return 0;
}

static int fn_math_clamp(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_clamp()", 3, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1]) || !fh_is_number(&args[2]))
        return fh_set_error(prog, "math_clamp(): expected 3 numbers");

    double a = fh_get_number(&args[0]);
    double x = fh_get_number(&args[1]);
    double y = fh_get_number(&args[2]);
    *ret = fh_new_number(MAX(x, MIN(y, a)));
    return 0;
}

static int fn_math_max(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_max()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_max(): expected number, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    *ret = fh_make_number(MAX(args[0].data.num, args[1].data.num));
    return 0;
}

static int fn_math_min(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_min()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_min(): expected number, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    *ret = fh_make_number(MIN(args[0].data.num, args[1].data.num));
    return 0;
}

static int fn_math_modf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_modf()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_modf(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    double fract_part = 0, int_part = 0;
    fract_part = modf(args[0].data.num, &int_part);

    int pin_state = fh_get_pin_state(prog);
    struct fh_array *arr = fh_make_array(prog, true);
    struct fh_value fp = fh_new_number(fract_part);
    struct fh_value ip = fh_make_number(int_part);

    struct fh_value *new_items = fh_grow_array_object(prog, arr, 2);
    if (!new_items)
        return fh_set_error(prog, "out of memory");

    arr->items[1] = fp;
    arr->items[0] = ip;

    struct fh_value v = fh_new_array(prog);
    v.data.obj = arr;

    *ret = v;
    fh_restore_pin_state(prog, pin_state);
    return 0;
}

static int fn_math_pi(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "math_pi()", 0, n_args))
        return -1;

    *ret = fh_make_number(M_PI);
    return 0;
}

static int fn_math_flt_epsilon(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "flt_epsilon()", 0, n_args))
        return -1;

    *ret = fh_make_number(FLT_EPSILON);
    return 0;
}

static int fn_math_pow(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_pow()", 2, n_args))
        return -1;

    if (!fh_is_number(&args[0]) || !fh_is_number(&args[1])) {
        return fh_set_error(prog, "math_pow(): expected two numbers, got %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));
    }

    *ret = fh_make_number(pow(args[0].data.num, args[1].data.num));
    return 0;
}

static int fn_math_rad(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_rad()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_rad(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(RAD_TO_DEG(args[0].data.num));
    return 0;
}

static int fn_math_random(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args > 2) {
        return fh_set_error(prog, "math_random(): invalid number of arguments");
    }

    if (n_args == 0) {
        /* return 0 or 1 */
        *ret = fh_make_number(mt19937_next32(mt19937_generator) % 2);
    } else if (n_args == 1 && fh_is_number(&args[0])) {
        /* return 0 .. arg0 */
        *ret = fh_make_number((float)(mt19937_next32(mt19937_generator) % (int)(args[0].data.num+1)));
    } else if (n_args == 2) {
        /* return arg0 .. arg1 */
        int min = (int) args[0].data.num;
        int max = (int) args[1].data.num;
        *ret = fh_make_number((float)(mt19937_next32(mt19937_generator) % ((max+1) - min)) + min);
    } else {
        return fh_set_error(prog, "math_random(): invalid call");
    }

    return 0;
}

static int fn_math_randomseed(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args == 1 && !fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_randomseed(): expected number or null, got %s",
                            fh_type_to_str(prog, args[0].type));
    }
    uint32_t seed = n_args > 0 ? args[0].data.num : time(NULL);
    mt19937_seed(mt19937_generator, seed);
    *ret = fh_make_null();
    return 0;
}

static int fn_math_sin(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_sin()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_sin(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(sin(args[0].data.num));
    return 0;
}

static int fn_math_sinh(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_sinh()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_sinh(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(sinh(args[0].data.num));
    return 0;
}

static int fn_math_sqrt(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_sqrt()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_sqrt(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(sqrt(args[0].data.num));
    return 0;
}

static int fn_math_tan(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_tan()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_tan(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(tan(args[0].data.num));
    return 0;
}

static int fn_math_tanh(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "math_tanh()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0])) {
        return fh_set_error(prog, "math_tanh(): expected number, got %s", fh_type_to_str(prog, args[0].type));
    }

    *ret = fh_make_number(tanh(args[0].data.num));
    return 0;
}

static int fn_math_maxval(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "math_maxval()", 0, n_args))
        return -1;

    *ret = fh_new_number(DBL_MAX);
    return 0;
}

/*********** End Math functions ***********/

/*********** I/O functions ***********/

static int fn_io_tar_open(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_string(&args[0])) {
        return fh_set_error(prog, "io_tar_open expected tar path to open");
    }

    const char *path = fh_get_string(&args[0]);
    const char *mode = fh_optstring(args, n_args, 1, "r");

    mtar_t *tar = malloc(sizeof(mtar_t));

    if (mtar_open(tar, path, mode) != MTAR_ESUCCESS) {
        free(tar);
        return fh_set_error(prog, "Couldn't open tar file at location: %s", path);
    }

    *ret = fh_new_c_obj(prog, tar, NULL, FH_IO_TAR_STRUCT_ID);
    return 0;
}

static int fn_io_tar_read(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args != 2) {
        return fh_set_error(prog, "Invalid number of arguments");
    }
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }
    if (!fh_is_string(&args[1])) {
        return fh_set_error(prog, "Expected string as second argument");
    }

    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    const char *file = fh_get_string(&args[1]);

    mtar_header_t h;

    int err = mtar_find(tar, file, &h);
    if (err != MTAR_ESUCCESS) {
        return fh_set_error(prog, "Couldn't read file: %s in tar", file);
    }
    char *c = malloc(h.size + 1);
    c[h.size] = 0;
    err = mtar_read_data(tar, c, h.size);
    if (err != MTAR_ESUCCESS) {
        return fh_set_error(prog, "Couldn't read file: %s", file);
    }

    *ret = fh_new_string(prog, c);
    free(c);
    return 0;
}

static int fn_io_tar_list(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }

    struct fh_value arr = fh_new_array(prog);
    struct fh_array *arr_val = GET_VAL_ARRAY(&arr);

    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    mtar_header_t h;

    size_t len = 0;
    while ((mtar_read_header(tar, &h)) != MTAR_ENULLRECORD) {
        fh_grow_array(prog, &arr, len | 1);
        arr_val->items[len++] = fh_new_string(prog, h.name);
        mtar_next(tar);
    }

    *ret = arr;
    return 0;
}

static int fn_io_tar_write_header(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }
    if (!fh_is_string(&args[1])) {
        return fh_set_error(prog, "Expected string as second argument");
    }

    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    const char *file_name = fh_get_string(&args[1]);

    const int err = mtar_write_file_header(tar, file_name, strlen(file_name));
    *ret = fh_new_bool(err == MTAR_ESUCCESS ? true : false);
    return 0;
}

static int fn_io_tar_write_data(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }
    if (!fh_is_string(&args[1])) {
        return fh_set_error(prog, "Expected string as second argument");
    }

    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    const char *file_name = fh_get_string(&args[1]);

    int err = mtar_write_data(tar, file_name, strlen(file_name));
    *ret = fh_new_bool(err == MTAR_ESUCCESS ? true : false);
    return 0;
}

static int fn_io_tar_write_finalize(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }

    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    mtar_finalize(tar);
    *ret = fh_new_null();
    return 0;
}

static int fn_io_tar_close(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_c_obj_of_type(&args[0], FH_IO_TAR_STRUCT_ID)) {
        return fh_set_error(prog, "Expected tar object as first argument");
    }
    mtar_t *tar = fh_get_c_obj_value(&args[0]);
    mtar_close(tar);
    free(tar);
    *ret = fh_new_null();
    return 0;
}

static int fn_io_open(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args == 0 || n_args > 2) {
        return fh_set_error(prog, "io_open(): expected 1 or 2 arguments, got %d", n_args);
    }

    if (!fh_is_string(&args[0])) {
        return fh_set_error(prog, "io_open(): expected string, got %s", fh_type_to_str(prog, args[0].type));
    }
    const char *path = fh_get_string(&args[0]);
    const char *mode = "r";
    if (n_args == 2) {
        if (!fh_is_string(&args[1])) {
            return fh_set_error(prog, "io_open(): expected string as the second argument, got %s",
                                fh_type_to_str(prog, args[1].type));
        }
        mode = fh_get_string(&args[1]);
    }

    FILE *fp = fopen(path, mode);
    if (!fp) {
        return fh_set_error(prog, "io_open(): failed to open file: %s", path);
    }

    *ret = fh_new_c_obj(prog, fp, NULL, FH_IO_STRUCT_ID);
    return 0;
}

char *scan_line(char *line) {
    int ch; // as getchar() returns `int`
    long capacity = 0; // capacity of the buffer
    long length = 0; // maintains the length of the string
    char *temp = NULL; // use additional pointer to perform allocations in order to avoid memory leaks

    while (((ch = getchar()) != '\n') && (ch != EOF)) {
        if ((length + 1) >= capacity) {
            // resetting capacity
            if (capacity == 0) {
                capacity = 8;
            } else {
                capacity <<= 1; // double the size
            }

            if ((temp = realloc(line, capacity * sizeof(char))) == NULL) {
                return NULL;
            }

            line = temp;
        }
        line[length] = (char) ch;
        length++;
    }
    if (length == 0) {
        return NULL;
    }
    line[length] = '\0';

    temp = realloc(line, (length + 1) * sizeof(char));
    if (!temp) {
        return line; // keep original buffer; it's still valid
    }
    line = temp;
    return line;
}


static int fn_io_scan_line(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    char *line = NULL;
    line = scan_line(line);

    if (line != NULL) {
        *ret = fh_new_string(prog, line);
        free(line);
    } else {
        *ret = fh_new_string(prog, "");
    }
    return 0;
}

static int fn_io_read(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (fh_is_c_obj(&args[0])) {
        if (fh_get_c_obj(&args[0])->type != FH_IO_STRUCT_ID)
            return fh_set_error(prog, "Expected IO handler");

        FILE *fp = fh_get_c_obj_value(&args[0]);

        fseek(fp, 0, SEEK_END);
        size_t len = ftell(fp);
        rewind(fp);

        char *read = malloc(sizeof(char) * len + 1);

        size_t ret_read = fread(read, sizeof(char), len, fp);
        read[len] = '\0';

        if (len != ret_read) {
            free(read);
            return fh_set_error(prog, "io_read(): couldn't read the file");
        }
        *ret = fh_new_string(prog, read);

        free(read);
        return 0;
    }

    if (n_args == 0) {
        size_t max_len = 1024;
        char *read = malloc(max_len * sizeof(char));
        size_t i = 0;
        int c = 0;
        while ((c = getchar()) != '\n' && i < max_len) {
            if (i + 1 >= max_len) {
                if (max_len >= SIZE_MAX) break;
                max_len = max_len << 1;
                char *tmp = realloc(read, max_len);
                if (!tmp) {
                    free(read);
                    return fh_set_error(prog, "io_read(): out of memory");
                }
                read = tmp;
                if (read == NULL) break;
            }
            read[i] = c;
            i++;
        }
        read[i] = '\0';
        *ret = fh_new_string(prog, read);
        free(read);
    }
    return 0;
}

static int fn_io_write(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "io_write()", 2, n_args))
        return -1;

    if (!fh_is_c_obj(&args[0]) || fh_get_c_obj(&args[0])->type != FH_IO_STRUCT_ID)
        return fh_set_error(prog, "Expected IO handler");

    FILE *fp = fh_get_c_obj_value(&args[0]);

    struct fh_value v = args[1];

    switch (v.type) {
        case FH_VAL_NULL:
            fprintf(fp, "null");
            break;
        case FH_VAL_BOOL:
            fprintf(fp, "%s", v.data.b == true ? "true" : "false");
            break;
        case FH_VAL_FLOAT:
            fprintf(fp, "%f", v.data.num);
            break;
        case FH_VAL_STRING:
            fprintf(fp, "%s", GET_OBJ_STRING_DATA(v.data.obj));
            break;
        default:
            fh_set_error(prog, "cannot write type: %s", fh_type_to_str(prog, v.type));
            break;
    }

    *ret = fh_new_null();
    return 0;
}

static int fn_io_close(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "io_close()", 1, n_args))
        return -1;

    if (!fh_is_c_obj(&args[0]) || fh_get_c_obj(&args[0])->type != FH_IO_STRUCT_ID)
        return fh_set_error(prog, "Expected IO handler");

    FILE *fp = fh_get_c_obj_value(&args[0]);

    fclose(fp);

    *ret = fh_new_null();
    return 0;
}


static int fn_io_seek(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "io_seek()", 3, n_args))
        return -1;

    if (!fh_is_c_obj(&args[0]) || fh_get_c_obj(&args[0])->type != FH_IO_STRUCT_ID)
        return fh_set_error(prog, "Expected IO handler");

    FILE *fp = fh_get_c_obj_value(&args[0]);
    if (!fh_is_number(&args[1])) {
        return fh_set_error(prog, "expected number for the second argument, got: %s",
                            fh_type_to_str(prog, args[1].type));
    }
    int offset = (int) fh_get_number(&args[1]);
    if (!fh_is_string(&args[2])) {
        return fh_set_error(prog, "expected string for the second argument, got: %s",
                            fh_type_to_str(prog, args[2].type));
    }
    const char *whence = GET_OBJ_STRING_DATA(args[2].data.obj);

    if (strcmp(whence, "set") == 0) {
        *ret = fh_make_number(fseek(fp, offset, SEEK_SET));
    } else if (strcmp(whence, "cur") == 0) {
        *ret = fh_make_number(fseek(fp, offset, SEEK_CUR));
    } else if (strcmp(whence, "end") == 0) {
        *ret = fh_make_number(fseek(fp, offset, SEEK_END));
    } else {
        return fh_set_error(prog, "expected 'set', 'cur' or 'end', got: %s", whence);
    }
    return 0;
}

static int fn_io_remove(struct fh_program *prog,
                        struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "Illegal parameter, expected filename:string");
    const char *filename = fh_get_string(&args[0]);

    int err = remove(filename);
    if (err != 0)
        return fh_set_error(prog, "Couldn't remove file %s\n", filename);

    *ret = fh_new_bool(true);
    return 0;
}

static int fn_io_rename(struct fh_program *prog,
                        struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_string(&args[0]) || !fh_is_string(&args[1]))
        return fh_set_error(prog, "Illegal parameter, expected old_filename:string and new_filename:string");
    const char *old_filename = fh_get_string(&args[0]);
    const char *new_filename = fh_get_string(&args[1]);

    int err = rename(old_filename, new_filename);
    if (err != 0)
        return fh_set_error(prog, "Couldn't rename %s to %s\n", old_filename, new_filename);

    *ret = fh_new_bool(true);
    return 0;
}

static int fn_io_mkdir(struct fh_program *prog, struct fh_value *ret,
                       struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "Illegal parameter, expected string");

    const char *name = fh_get_string(&args[0]);
    int err = 0;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
    err = _mkdir(name);
#else
    err = mkdir(name, 0777);
#endif
    *ret = fh_new_bool(err == 0 ? true : false);
    return 0;
}

static int fn_io_filetype(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "Illegal parameter, expected string");

    const char *path = fh_get_string(&args[0]);

#if defined(WIN32) || defined(_WIN32)
    if (_access_s(path, 0) == 0) {
        struct stat s;
        _tstat(path, &s);
        if (s.st_mode & S_IFDIR) {
            *ret = fh_new_string(prog, "directory");
        } else if (s.st_mode & S_IFMT) {
            *ret = fh_new_string(prog, "file");
        } else {
            *ret = fh_new_string(prog, "unknown");
        }
    } else {
        return fh_set_error(prog, "Couldn't fetch information about path: %s\n", path);
    }

#else

    struct stat s;
    if (stat(path, &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            *ret = fh_new_string(prog, "directory");
        } else if (s.st_mode & S_IFMT) {
            *ret = fh_new_string(prog, "file");
        } else {
            *ret = fh_new_string(prog, "unknown");
        }
    } else {
        return fh_set_error(prog, "Couldn't fetch information about path: %s\n", path);
    }
#endif
    return 0;
}

/*********** End I/O functions ***********/

/*********** String functions ***********/
static int fn_string_slice(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_string(&args[0]) || !fh_is_number(&args[1]))
        return fh_set_error(prog, "expected string and number delimiters");

    const char *string = fh_get_string(&args[0]);
    int start = (int) fh_get_number(&args[1]);
    int end = (int) fh_optnumber(args, n_args, 2, 0);

    size_t str_len = strlen(string);

    if (start < 0 || start >= str_len)
        return fh_set_error(prog, "Start index out of bounds!");

    if (end != 0 && (end < start || end >= str_len))
        return fh_set_error(prog, "Invalid end index value");

    char *new_string = malloc(str_len + 1);
    memset(new_string, 0, str_len + 1);

    struct fh_value arr = fh_new_array(prog);
    fh_grow_array(prog, &arr, 3);

    struct fh_array *arr_val = GET_VAL_ARRAY(&arr);

    if (n_args == 2) {
        for (int i = 0; i < start; i++) {
            new_string[i] = string[i];
        }
        arr_val->items[0] = fh_new_string(prog, new_string);
        memset(new_string, 0, str_len + 1);
        for (int i = start; i < str_len; i++) {
            new_string[i - start] = string[i];
        }
        arr_val->items[1] = fh_new_string(prog, new_string);
    } else if (n_args == 3) {
        for (int i = 0; i < start; i++) {
            new_string[i] = string[i];
        }
        arr_val->items[0] = fh_new_string(prog, new_string);
        memset(new_string, 0, str_len + 1);
        for (int i = start; i < end; i++) {
            new_string[i - start] = string[i];
        }
        arr_val->items[1] = fh_new_string(prog, new_string);
        memset(new_string, 0, str_len + 1);
        if (end < str_len) {
            for (int i = end; i < str_len; i++) {
                new_string[i - end] = string[i];
            }
            arr_val->items[2] = fh_new_string(prog, new_string);
        }
    }

    *ret = arr;
    free(new_string);
    return 0;
}

static int fn_string_split(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_split()", 2, n_args))
        return -1;

    if (!fh_is_string(&args[0]) || !fh_is_string(&args[1]))
        return fh_set_error(prog, "expected two strings value, got: %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));

    const char *string = fh_get_string(&args[0]);
    const char *delimiter = fh_get_string(&args[1]);

    char *buffer[1024];
    uint32_t arr_len = 0;

    char *str_cpy = malloc(strlen(string) + 1);
    strcpy(str_cpy, string);

    char *token = strtok((char *) str_cpy, delimiter);
    while (token) {
        if (arr_len >= 1024) {
            return fh_set_error(prog, "Cannot have more than 1024 split objects");
        }

        buffer[arr_len] = malloc(strlen(token) + 1);
        strcpy(buffer[arr_len], token);
        arr_len++;
        token = strtok(NULL, delimiter);
    }
    free(str_cpy);

    struct fh_value arr = fh_new_array(prog);

    struct fh_array *arr_val = GET_VAL_ARRAY(&arr);
    fh_grow_array(prog, &arr, arr_len);

    for (size_t i = 0; i < arr_len; i++) {
        arr_val->items[i] = fh_new_string(prog, buffer[i]);
        free(buffer[i]);
    }
    *ret = arr;
    return 0;
}

static int fn_string_upper(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_upper()", 1, n_args))
        return -1;

    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string value, got: %s", fh_type_to_str(prog, args[0].type));

    char *str = GET_VAL_STRING_DATA(&args[0]);
    size_t i = 0, len = strlen(str);
    char *s = malloc(sizeof(char) * len + 1);
    while (i < len) {
        s[i] = (char) toupper(str[i]);
        i++;
    }
    s[len] = '\0';
    *ret = fh_new_string(prog, s);
    free(s);
    return 0;
}

static int fn_string_lower(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_lower()", 1, n_args))
        return -1;

    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string value, got: %s", fh_type_to_str(prog, args[0].type));

    char *str = GET_VAL_STRING_DATA(&args[0]);
    size_t i = 0, len = strlen(str);
    char *s = malloc(sizeof(char) * len + 1);
    while (i < len) {
        s[i] = (char) tolower(str[i]);
        i++;
    }
    s[len] = '\0';
    *ret = fh_new_string(prog, s);
    free(s);
    return 0;
}

static int fn_string_match(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_match()", 2, n_args))
        return -1;

    if (!fh_is_string(&args[0]) || !fh_is_string(&args[1]))
        return fh_set_error(prog, "expected string values, got: %s and %s",
                            fh_type_to_str(prog, args[0].type), fh_type_to_str(prog, args[1].type)
        );

    const char *input_str = GET_VAL_STRING_DATA(&args[0]);
    const char *input_pat = GET_VAL_STRING_DATA(&args[1]);

    re_t pattern = re_compile(input_pat);
    int match_idx = re_matchp(pattern, input_str);
    if (match_idx != -1) {
        *ret = fh_new_number(match_idx);
        return 0;
    }
    *ret = fh_new_null();
    return 0;
}

static int fn_string_find(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_find()", 2, n_args))
        return -1;

    if (!fh_is_string(&args[0]) || !fh_is_string(&args[1]))
        return fh_set_error(prog, "expected string values, got: %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type));

    const char *str = GET_VAL_STRING_DATA(&args[0]);
    const char *find = GET_VAL_STRING_DATA(&args[1]);

    char *r = strstr(str, find);
    if (r)
        *ret = fh_new_number((int)(r - str));
    else
        *ret = fh_new_number(-1);
    return 0;
}

static void reverse_string(char *str) {
    size_t len = strlen(str);
    if (len == 0)
        return;

    /* get range */
    char *start = str;
    char *end = start + len - 1; /* -1 for \0 */
    char temp;

    /* reverse */
    while (end > start) {
        /* swap */
        temp = *start;
        *start = *end;
        *end = temp;

        /* move */
        ++start;
        --end;
    }
}

static int fn_string_reverse(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_reverse()", 1, n_args))
        return -1;

    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string values, got: %s", fh_type_to_str(prog, args[0].type));

    char *str = GET_VAL_STRING_DATA(&args[0]);

    /* We have to alloc a new space in memory because otherwise there's no way to avoid this:
     * let a = "hi";
     * let b = string_reverse(a);
     * b becomes "ih" but 'a' as well!
     */
    char *reverse = malloc(sizeof(char) * strlen(str) + 1);
    strcpy(reverse, str);

    reverse_string(reverse);

    *ret = fh_new_string(prog, reverse);
    free(reverse);
    return 0;
}

static char *substr(char const *input, size_t start, size_t len) {
    char *ret = malloc(len + 1);
    memcpy(ret, input+start, len);
    ret[len] = '\0';
    return ret;
}

static int fn_string_substr(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_substr()", 3, n_args))
        return -1;

    if (!fh_is_string(&args[0]) || !fh_is_number(&args[1]) || !fh_is_number(&args[2]))
        return fh_set_error(prog, "expected string values, got: %s %s and %s", fh_type_to_str(prog, args[0].type),
                            fh_type_to_str(prog, args[1].type), fh_type_to_str(prog, args[2].type));

    const char *str = GET_VAL_STRING_DATA(&args[0]);
    int start = (int) fh_get_number(&args[1]);
    int len = (int) fh_get_number(&args[2]);
    if (len < 0 || start < 0)
        return fh_set_error(prog, "cannot have a negative start or length");
    char *ret_str = substr(str, start, len);
    *ret = fh_new_string(prog, ret_str);
    free(ret_str);
    return 0;
}

static char *str_append(char *to, const char *val) {
    size_t to_len = 0;
    if (to)
        to_len = strlen(to);
    size_t val_len = strlen(val);
    size_t total_len = to_len + val_len;

    to = realloc(to, total_len + 1);

    strcpy(to + to_len, val);
    to[total_len] = '\0';

    return to;
}

static int fn_string_join(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args < 2) {
        return fh_set_error(prog, "Expected at least 2 arguments of type string for string_join()\n");
    }

    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "Expected string for the first parameter, got %s\n",
                            fh_type_to_str(prog, args[0].type));

    const char *join = GET_OBJ_STRING_DATA((&args[0])->data.obj);

    char *res = NULL;

    for (size_t i = 1; i < n_args; i++) {
        if (!fh_is_string(&args[i])) {
            free(res);
            return fh_set_error(prog, "Expected string for parameter %zu, got %s\n", i,
                                fh_type_to_str(prog, args[i].type));
        }
        const char *val = GET_OBJ_STRING_DATA((&args[i])->data.obj);

        res = str_append(res, val);
        /* Prevent adding the "join" string to the last element when there's more than one value,
         *  eg: join = ':', val: 23, 00, 32 -> 23:00:32: */
        if (n_args > 2) {
            if (i < n_args - 1)
                res = str_append(res, join);
        } else
            res = str_append(res, join);
    }

    *ret = fh_new_string(prog, res);
    free(res);
    return 0;
}

static int fn_string_char(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_char()", 1, n_args))
        return -1;

    if (fh_is_string(&args[0])) {
        const char *c = GET_VAL_STRING_DATA(&args[0]);
        *ret = fh_new_number(*c - '0');
    } else if (fh_is_number(&args[0])) {
        int c = (int) fh_get_number(&args[0]) + '0';
        char ret_str[32];
        snprintf(ret_str, 32, "%c", c);

        *ret = fh_new_string(prog, ret_str);
    } else
        return fh_set_error(prog, "expected string or number value, got: %s", fh_type_to_str(prog, args[0].type));

    return 0;
}

static int fn_string_format(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args == 0 || !fh_is_string(&args[0]))
        goto end;

    const char *format = fh_get_string(&args[0]);

    int next_arg = 1;
    char buffer[MAX_ITEM];
    long occupied = 0;
    long cs = 0; /* current size */

    for (const char *c = format; *c != '\0'; c++) {
        if (*c != '%') {
            cs = snprintf(buffer + occupied, MAX_ITEM, "%c", *c);
            occupied += cs;
            continue;
        }
        c++;
        if (next_arg >= n_args)
            return fh_set_error(prog, "string_format(): no argument supplied for '%%%c'", *c);

        switch (*c) {
            case 'd':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "string_format(): invalid argument type for '%%%c'", *c);
                cs = snprintf(buffer + occupied, MAX_ITEM, "%lld", (long long) (int64_t) args[next_arg].data.num);
                occupied += cs;
                break;
            case 'u':
            case 'x':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "string_format(): invalid argument type for '%%%c'", *c);
                cs = snprintf(buffer + occupied, MAX_ITEM, (*c == 'u') ?
                              "%llu" :
                              "%llx",
                              (unsigned long long) (int64_t) args[next_arg].data.num);
                occupied += cs;
                break;
            case 'f':
            case 'g':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "string_format(): invalid argument type for '%%%c'", *c);
                cs = snprintf(buffer + occupied, MAX_ITEM, (*c == 'f') ? "%f" : "%g", args[next_arg].data.num);
                occupied += cs;
                break;

            case 's':
            case 'c':
                if (!fh_is_string(&args[next_arg]))
                    return fh_set_error(prog, "string_format(): invalid argument type for '%%%c'", *c);
                cs = snprintf(buffer + occupied, MAX_ITEM,
                              "%s", GET_VAL_STRING_DATA(&args[next_arg]));
                occupied += cs;
                break;
            default:
                return fh_set_error(prog, "string_format(): invalid format specifier: '%%%c'", *c);
        }

        next_arg++;
    }
    *ret = fh_new_string(prog, buffer);
    return 0;

end:
    *ret = fh_make_null();
    return 0;
}

static size_t trimwhitespace(char *out, size_t len, const char *str) {
    if (len == 0)
        return 0;

    const char *end;
    size_t out_size;

    // Trim leading space
    while (isspace((unsigned char) *str)) str++;

    if (*str == 0) {
        // All spaces?
        *out = 0;
        return 1;
    }

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end)) end--;
    end++;

    // Set output size to minimum of trimmed string length and buffer size minus 1
    out_size = (end - str) < len - 1 ? (end - str) : len - 1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

static int fn_string_trim(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string value, got: %s", fh_type_to_str(prog, args[0].type));

    const char *input_str = GET_VAL_STRING_DATA(&args[0]);
    size_t len = strlen(input_str);
    char *out = malloc(len + 1);
    size_t ret_size = trimwhitespace(out, len + 1, input_str);

    // normally this should be called only if there's nothing to trim, so just for cases: 0 and 1
    if (ret_size <= 1) {
        strcpy(out, input_str);
    }

    *ret = fh_new_string(prog, out);
    free(out);
    return 0;
}

/*********** End String functions ***********/

/*********** OS functions ***********/
static double timedifference_usec(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000000.0 + (t1.tv_usec - t0.tv_usec);
}

static fh_c_obj_gc_callback os_time_gc(struct timeval *t) {
    free(t);
    return (fh_c_obj_gc_callback) 1;
}

static int fn_os_time(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(prog);
    UNUSED(args);
    UNUSED(n_args);

    struct timeval *t = malloc(sizeof(struct timeval));
    gettimeofday(t, NULL);
    *ret = fh_new_c_obj(prog, t, (fh_c_obj_gc_callback) os_time_gc, FH_TIME_STRUCT_ID);
    return 0;
}

static int fn_os_difftime(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_difftime()", 2, n_args))
        return -1;

    if (!fh_is_c_obj_of_type(&args[0], FH_TIME_STRUCT_ID) || !fh_is_c_obj_of_type(&args[1], FH_TIME_STRUCT_ID))
        return fh_set_error(prog, "expected two time objects");

    struct timeval *start = (struct timeval *) fh_get_c_obj_value(&args[0]);
    struct timeval *final = (struct timeval *) fh_get_c_obj_value(&args[1]);
    *ret = fh_new_number(timedifference_usec(*start, *final));
    return 0;
}

static int fn_os_localtime(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(prog);
    UNUSED(args);
    UNUSED(n_args);

    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    *ret = fh_new_string(prog, asctime(timeinfo));
    return 0;
}

static int fn_os_command(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_command()", 1, n_args))
        return -1;
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string value, got: %s", fh_type_to_str(prog, args[0].type));

    const char *command = fh_get_string(&args[0]);
    const int rc = system(command);
    *ret = fh_make_bool(rc == 0);
    return 0;
}

static int fn_os_getenv(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "string_getenv()", 1, n_args))
        return -1;
    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string value, got: %s", fh_type_to_str(prog, args[0].type));

    const char *env = fh_get_string(&args[0]);
    char *path = getenv(env);
    if (path) {
        *ret = fh_new_string(prog, path);
        return 0;
    }
    *ret = fh_new_null();
    return 0;
}

static int fn_os_getOS(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "os_getOS()", 0, n_args))
        return -1;

    *ret = fh_new_string(prog, FH_OS);
    return 0;
}

/*********** End OS functions ***********/
static int fn_getversion(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "getversion()", 0, n_args))
        return -1;

    *ret = fh_new_string(prog, FH_VERSION);
    return 0;
}

static int fn_tostring(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "tostring()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0]))
        return fh_set_error(prog, "expected number, got: %s", fh_type_to_str(prog, args[0].type));

    int require = snprintf(NULL, 0, "%f", args[0].data.num);
    require = require <= 0 ? 32 : require;
    char *buffer = malloc(sizeof(char) * (size_t) require + 1);
    snprintf(buffer, require, "%g", args[0].data.num);
    *ret = fh_new_string(prog, buffer);
    free(buffer);

    return 0;
}

static int fn_tonumber(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "tonumber()", 1, n_args))
        return -1;

    if (!fh_is_string(&args[0]))
        return fh_set_error(prog, "expected string, got: %s", fh_type_to_str(prog, args[0].type));

    const char *str = GET_VAL_STRING_DATA(&args[0]);
    double d;
    sscanf(str, "%lf", &d);
    *ret = fh_new_number(d);

    return 0;
}

static int fn_tointeger(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "tointeger()", 1, n_args))
        return -1;

    if (!fh_is_number(&args[0]))
        return fh_set_error(prog, "expected number, got: %s", fh_type_to_str(prog, args[0].type));

    double num = fh_get_number(&args[0]);
    *ret = fh_new_number((int)num);

    return 0;
}

static int fn_gc(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    (void) args;
    (void) n_args;

    fh_collect_garbage(prog);
    *ret = fh_new_null();
    return 0;
}

static int fn_gc_frequency(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    (void) args;
    (void) n_args;

    int64_t frequency = (uint64_t) fh_get_number(&args[0]);
    if (frequency < 1000000) {
        frequency = 1000000;
    }

    prog->gc_collect_at = frequency;

    *ret = fh_new_null();
    return 0;
}

static int fn_gc_pause(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(n_args);
    if (!fh_is_bool(&args[0]))
        return fh_set_error(prog, "Expected boolean");
    prog->gc_isPaused = fh_get_bool(&args[0]);
    *ret = fh_new_null();
    return 0;
}

static int fn_gc_info(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    UNUSED(args);
    if (check_n_args(prog, "gc_pause()", 0, n_args))
        return fh_set_error(prog, "Expected 0 arguments");
    *ret = fh_new_number(0);
    return 0;
}

static int fn_type(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "type()", 1, n_args))
        return -1;

    *ret = fh_new_string(prog, fh_type_to_str(prog, args[0].type));

    return 0;
}

static int fn_docstring(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "docstring()", 1, n_args))
        return -1;

    if (args[0].type != FH_VAL_CLOSURE) {
        return fh_set_error(prog, "Only closures support docstrings");
    }

    struct fh_closure *c = (struct fh_closure *) args[0].data.obj;

    *ret = fh_new_string(prog, c->doc_string ? GET_OBJ_STRING_DATA(c->doc_string) : "");
    return 0;
}

/**
 * has(array/map, object);
 * Searches for @object inside an array or a map in O(n) time.
 * If it's found then it returns the found object and an index to it, otherwise
 * returns 'false'.
 */
static int fn_has(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "has()", 2, n_args))
        return -1;

    int pin_state = fh_get_pin_state(prog);
    struct fh_array *ret_arr = fh_make_array(prog, true);
    if (!fh_grow_array_object(prog, ret_arr, 2))
        return fh_set_error(prog, "out of memory");

    struct fh_value new_val = fh_new_array(prog);

    struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
    if (arr) {
        for (size_t i = 0; i < arr->len; i++) {
            struct fh_value v = arr->items[i];
            if (fh_vals_are_equal(&args[1], &v)) {
                ret_arr->items[0] = v;
                ret_arr->items[1] = fh_new_number(i);
                new_val.data.obj = ret_arr;

                *ret = new_val;
                fh_restore_pin_state(prog, pin_state);
                return 0;
            }
        }
        *ret = fh_new_bool(false);
        fh_restore_pin_state(prog, pin_state);
        return 0;
    }
    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (map) {
        for (uint32_t i = 0; i < map->cap; i++) {
            struct fh_map_entry *e = &map->entries[i];
            if (fh_vals_are_equal(&args[1], &e->key)) {
                ret_arr->items[0] = e->val;
                ret_arr->items[1] = fh_new_number(i);
                new_val.data.obj = ret_arr;

                *ret = new_val;
                fh_restore_pin_state(prog, pin_state);
                return 0;
            }
        }
        *ret = fh_new_bool(false);
        fh_restore_pin_state(prog, pin_state);
        return 0;
    }
    fh_restore_pin_state(prog, pin_state);
    return fh_set_error(prog, "Expected an array or a map as the second argument.");
}

static int fn_assert(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args == 0) {
        return fh_set_error(prog, "Expected condition to assert agains");
    }

    if (!GET_VAL_OBJ(&args[0])) {
        if (n_args == 1)
            return fh_set_error(prog, "assert() failed!");
        else if (n_args == 2) {
            if (fh_is_string(&args[1]))
                return fh_set_error(prog, "assert() failed! Reason: %s", fh_get_string(&args[1]));
            return fh_set_error(prog, "assert() failed!");
        }
    }

    *ret = fh_new_bool(true);

    return 0;
}

static int fn_error(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    (void) ret;

    if (check_n_args(prog, "error()", 1, n_args))
        return -1;

    const char *str = GET_VAL_STRING_DATA(&args[0]);
    if (!str)
        return fh_set_error(prog, "error(): argument 1 must be a string");
    return fh_set_error(prog, "%s", str);
}

static int fn_len(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "len()", 1, n_args))
        return -1;

    struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
    if (arr) {
        *ret = fh_make_number(arr->len);
        return 0;
    }
    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (map) {
        *ret = fh_make_number(map->len);
        return 0;
    }
    struct fh_string *string = GET_VAL_STRING(&args[0]);
    if (string) {
        *ret = fh_make_number(string->size - 1);
        return 0;
    }
    return fh_set_error(prog, "len(): argument 1 must be an array, map or string, got %s",
                        fh_type_to_str(prog, args[0].type));
}

static int fn_delete(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "delete()", 2, n_args))
        return -1;

    struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
    if (arr) {
        if (!fh_is_number(&args[1]))
            return fh_set_error(prog, "delete(): argument 2 must be a number");
        uint32_t index = (uint32_t) (int) fh_get_number(&args[1]);
        if (index >= arr->len)
            return fh_set_error(prog, "delete(): array index out of bounds: %d", index);
        *ret = arr->items[index];
        if (index + 1 < arr->len)
            memmove(arr->items + index, arr->items + index + 1, (arr->len - (index + 1)) * sizeof(struct fh_value));
        arr->len--;
        return 0;
    }
    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (map) {
        if (fh_get_map_object_value(map, &args[1], ret) < 0
            || fh_delete_map_object_entry(map, &args[1]) < 0)
            return fh_set_error(prog, "delete(): key not in map");
        return 0;
    }
    return fh_set_error(prog, "delete(): argument 1 must be an array or map, got %s and %s",
                        fh_type_to_str(prog, args[0].type), fh_type_to_str(prog, args[1].type));
}

static int fn_extends(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "extends()", 2, n_args))
        return -1;

    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (!map)
        return fh_set_error(prog, "extends(): argument 1 must be a map");

    struct fh_map *to_extend = GET_VAL_MAP(&args[1]);
    if (!to_extend)
        return fh_set_error(prog, "extends(): argument 2 must be a map");

    fh_extends_map(prog, map, to_extend);

    *ret = fh_new_null();
    return 0;
}

/**
* Resets a map or an array. Doing this you won't have to reallocate an object.
* ex:
* let map = {};
* while (true) {
*  <.... some code ....>
*  map = {}; # reset map, old way
*  reset(map); # reset map, new way
* }
*/
static int fn_reset(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "reset()", 1, n_args))
        return -1;

    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (!map) {
        struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
        fh_reset_array(arr);
        if (!arr)
            return fh_set_error(prog, "reset(): argument 1 must be a map or array");
    } else {
        fh_reset_map(map);
    }

    *ret = fh_new_null();
    return 0;
}

static int fn_next_key(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "next_key()", 2, n_args))
        return -1;

    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (!map)
        return fh_set_error(prog, "next_key(): argument 1 must be a map");
    if (fh_next_map_object_key(map, &args[1], ret) < 0)
        return fh_set_error(prog, "next_key(): key not in map, got %s and %s",
                            fh_type_to_str(prog, args[0].type), fh_type_to_str(prog, args[1].type));
    return 0;
}

static int fn_contains_key(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "contains_key()", 2, n_args))
        return -1;

    struct fh_map *map = GET_VAL_MAP(&args[0]);
    if (!map)
        return fh_set_error(prog, "contains_key(): argument 1 must be a map");
    if (fh_get_map_object_value(map, &args[1], ret) < 0) {
        *ret = fh_make_bool(false);
        return 0;
    }
    //printf("key "); print_value(&args[1]); printf(" has value "); print_value(ret); printf("\n");
    *ret = fh_make_bool(true);

    return 0;
}

static int fn_append(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "append()", -2, n_args))
        return -1;
    struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
    if (!arr)
        return fh_set_error(prog, "append(): argument 1 must be an array");
    struct fh_value *new_items = fh_grow_array_object(prog, arr, n_args - 1);
    if (!new_items)
        return fh_set_error(prog, "out of memory");
    memcpy(new_items, args + 1, sizeof(struct fh_value) * (n_args-1));
    *ret = args[0];
    return 0;
}

static int fn_insert(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "insert()", -3, n_args))
        return -1;
    struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
    if (!arr)
        return fh_set_error(prog, "insert(): argument 1 must be an array");
    if (!fh_is_number(&args[1]))
        return fh_set_error(prog, "insert(): argument 2 must be a number");

    int index = fh_get_number(&args[1]);
    struct fh_value *new_items = fh_grow_array_object(prog, arr, index + 1);
    if (!new_items)
        return fh_set_error(prog, "out of memory");
    arr->items[index] = args[2];
    *ret = args[0];
    return 0;
}

static int fn_grow(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "grow()", -2, n_args))
        return -1;

    if (!fh_is_number(&args[1]))
        return fh_set_error(prog, "grow(): argument 2 (size) must be a number");
    if (fh_get_number(&args[1]) < 0)
        return fh_set_error(prog, "Expected positive number value for argument 2 (size)");

    unsigned int size = fh_get_number(&args[1]);

    if (fh_is_array(&args[0])) {
        struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
        fh_grow_array_object(prog, arr, size);
        *ret = args[0];
    } else if (fh_is_string(&args[0])) {
        const char *string = fh_get_string(&args[0]);
        char ns[size];
        strcpy(ns, string);
        *ret = fh_new_string(prog, ns);
    } else {
        return fh_set_error(prog, "grow(): argument 1 must be an array or string");
    }
    return 0;
}

static int fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    (void) prog;

    for (int i = 0; i < n_args; i++)
        print_value(&args[i]);
    *ret = fh_make_null();
    return 0;
}


static int fn_println(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    for (int i = 0; i < n_args; i++)
        print_value(&args[i]);

    int frame_index = call_frame_stack_size(&prog->vm.call_stack) - 1;
    struct fh_vm_call_frame *frame;
    do {
        frame = call_frame_stack_item(&prog->vm.call_stack, frame_index);
        frame_index--;
    } while (frame && !frame->closure);
    if (frame) {
        struct fh_func_def *func_def = frame->closure->func_def;
        printf(" %s:%d:%d\n", fh_get_symbol_name(&prog->src_file_names,
                                                 func_def->code_creation_loc.file_id),
               func_def->code_creation_loc.line, func_def->code_creation_loc.col);
    } else {
        puts("");
    }
    *ret = fh_make_null();
    return 0;
}

static int fn_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (n_args == 0 || !fh_is_string(&args[0]))
        goto end;

    int next_arg = 1;
    for (const char *c = fh_get_string(&args[0]); *c != '\0'; c++) {
        if (*c != '%') {
            putchar(*c);
            continue;
        }
        c++;
        if (*c == '%') {
            putchar('%');
            continue;
        }
        if (next_arg >= n_args)
            return fh_set_error(prog, "printf(): no argument supplied for '%%%c'", *c);

        switch (*c) {
            case 'd':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
                printf("%lld", (long long) (int64_t) args[next_arg].data.num);
                break;

            case 'u':
            case 'x':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
                printf((*c == 'u') ? "%llu" : "%llx", (unsigned long long) (int64_t) args[next_arg].data.num);
                break;

            case 'f':
            case 'g':
                if (!fh_is_number(&args[next_arg]))
                    return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
                printf((*c == 'f') ? "%f" : "%g", args[next_arg].data.num);
                break;

            case 's':
                print_value(&args[next_arg]);
                break;

            default:
                return fh_set_error(prog, "printf(): invalid format specifier: '%%%c'", *c);
        }
        next_arg++;
    }

end:
    *ret = fh_make_null();
    return 0;
}

static int fn_eval(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args) {
    if (check_n_args(prog, "eval()", 2, n_args))
        return -1;

    if (!fh_is_string(&args[0]) || !fh_is_string(&args[1])) {
        return fh_set_error(prog, "Expected string code and function to call from string code");
    }

    const char *code = fh_get_string(&args[0]);
    const char *fn_name = fh_get_string(&args[1]);

    struct fh_input *in = fh_open_input_string(code);

    if (!in) {
        return fh_set_error(prog, "Couldn't read input string: %s",
                            code);
    }

    struct fh_program *p = fh_new_program();
    if (fh_compile_input(p, in) < 0) {
        return fh_set_error(prog, "Couldn't compile input string: %s",
                            code);
    }

    /*
     * We don't allow passing parameters from program to the string code
     * because this could expose security breaches!
     */
    if (fh_call_function(p, fn_name, NULL, 0, ret) < 0) {
        return fh_set_error(prog, "Couldn't call function %s\n", fn_name);
    }

    // See more about it in fh.h
    if (vec_push(fh_programs_vector, p) != 0) {
        fh_free_program(p);
        return fh_set_error(prog, "eval(): out of memory");
    }
    return 0;
}

#define DEF_FN(name)  { #name, fn_##name }
const struct fh_named_c_func fh_std_c_funcs[] = {
    DEF_FN(math_md5),
    DEF_FN(math_bcrypt_gen_salt),
    DEF_FN(math_bcrypt_hashpw),

    DEF_FN(math_clamp),
    DEF_FN(math_abs),
    DEF_FN(math_acons),
    DEF_FN(math_asin),
    DEF_FN(math_atan),
    DEF_FN(math_atan2),
    DEF_FN(math_ceil),
    DEF_FN(math_cos),
    DEF_FN(math_cosh),
    DEF_FN(math_deg),
    DEF_FN(math_exp),
    DEF_FN(math_floor),
    DEF_FN(math_fmod),
    DEF_FN(math_frexp),
    DEF_FN(math_huge),
    DEF_FN(math_ldexp),
    DEF_FN(math_log),
    DEF_FN(math_log10),
    DEF_FN(math_max),
    DEF_FN(math_min),
    DEF_FN(math_modf),
    DEF_FN(math_pi),
    DEF_FN(math_flt_epsilon),
    DEF_FN(math_pow),
    DEF_FN(math_rad),
    DEF_FN(math_random),
    DEF_FN(math_randomseed),
    DEF_FN(math_sin),
    DEF_FN(math_sinh),
    DEF_FN(math_sqrt),
    DEF_FN(math_tan),
    DEF_FN(math_tanh),
    DEF_FN(math_maxval),

    DEF_FN(io_tar_open),
    DEF_FN(io_tar_read),
    DEF_FN(io_tar_list),
    DEF_FN(io_tar_write_header),
    DEF_FN(io_tar_write_data),
    DEF_FN(io_tar_write_finalize),
    DEF_FN(io_tar_close),
    DEF_FN(io_open),
    DEF_FN(io_read),
    DEF_FN(io_scan_line),
    DEF_FN(io_write),
    DEF_FN(io_close),
    DEF_FN(io_seek),
    DEF_FN(io_rename),
    DEF_FN(io_remove),
    DEF_FN(io_mkdir),
    DEF_FN(io_filetype),

    DEF_FN(string_slice),
    DEF_FN(string_split),
    DEF_FN(string_upper),
    DEF_FN(string_lower),
    DEF_FN(string_find),
    DEF_FN(string_match),
    DEF_FN(string_reverse),
    DEF_FN(string_substr),
    DEF_FN(string_char),
    DEF_FN(string_trim),
    DEF_FN(string_format),
    DEF_FN(string_join),

    DEF_FN(os_time),
    DEF_FN(os_difftime),
    DEF_FN(os_localtime),
    DEF_FN(os_command),
    DEF_FN(os_getenv),
    DEF_FN(os_getOS),

    DEF_FN(eval),
    DEF_FN(has),
    DEF_FN(getversion),
    DEF_FN(gc),
    DEF_FN(gc_info),
    DEF_FN(gc_pause),
    DEF_FN(gc_frequency),
    DEF_FN(tonumber),
    DEF_FN(tointeger),
    DEF_FN(tostring),
    DEF_FN(type),
    DEF_FN(docstring),
    DEF_FN(error),
    DEF_FN(assert),
    DEF_FN(print),
    DEF_FN(println),
    DEF_FN(printf),
    DEF_FN(len),
    DEF_FN(extends),
    DEF_FN(reset),
    DEF_FN(next_key),
    DEF_FN(contains_key),
    DEF_FN(append),
    DEF_FN(insert),
    DEF_FN(grow),
    DEF_FN(delete),
};
const int fh_std_c_funcs_len = sizeof(fh_std_c_funcs) / sizeof(fh_std_c_funcs[0]);

