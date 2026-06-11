/* c_args.c
 *
 * Argument validation and optional-argument helpers for C functions
 * registered with the interpreter (fh_add_c_func). Declarations live in
 * value.h.
 */

#include <limits.h>
#include <math.h>

#include "program.h"
#include "value.h"
#include "fh.h"

int fh_arg_int32(struct fh_program *prog, const struct fh_value *v, const char *fn, int arg_index_0_based,
                 int32_t *out) {
    if (!fh_is_number(v)) {
        return fh_set_error(prog, "%s: expected number/integer for argument %d, got %s",
                            fn, arg_index_0_based + 1, fh_type_to_str(prog, v->type));
    }

    if (fh_is_integer(v)) {
        const int64_t x = v->data.i;
        if (x < INT32_MIN || x > INT32_MAX) {
            return fh_set_error(prog, "%s: argument %d out of int32 range", fn, arg_index_0_based + 1);
        }
        *out = (int32_t) x;
        return 0;
    }

    const double d = fh_get_float((struct fh_value*)v);
    if (!isfinite(d)) {
        return fh_set_error(prog, "%s: argument %d must be finite", fn, arg_index_0_based + 1);
    }
    if (d < (double) INT32_MIN || d > (double) INT32_MAX) {
        return fh_set_error(prog, "%s: argument %d out of int32 range", fn, arg_index_0_based + 1);
    }
    if (trunc(d) != d) {
        return fh_set_error(prog, "%s: argument %d must be an integer value", fn, arg_index_0_based + 1);
    }

    *out = (int32_t) d;
    return 0;
}

int fh_arg_double(struct fh_program *prog, const struct fh_value *v, const char *fn, int arg_index_0_based,
                  double *out) {
    if (fh_is_float(v)) {
        const double d = v->data.num;
        if (!isfinite(d)) {
            return fh_set_error(prog, "%s: argument %d must be finite", fn, arg_index_0_based + 1);
        }
        *out = d;
        return 0;
    }
    if (fh_is_integer(v)) {
        *out = (double) v->data.i;
        return 0;
    }
    return fh_set_error(prog, "%s: expected number/integer for argument %d, got %s", fn, arg_index_0_based + 1,
                        fh_type_to_str(prog, v->type));
}

double fh_optnumber(struct fh_value *args, int n_args, int check, double opt) {
    if (n_args <= check) {
        return opt;
    }
    if (args[check].type == FH_VAL_FLOAT)
        return args[check].data.num;
    if (args[check].type == FH_VAL_INTEGER)
        return (double) args[check].data.i;

    return opt;
}

int64_t fh_optinteger(struct fh_value *args, int n_args, int check, int64_t opt) {
    if (n_args <= check) {
        return opt;
    }
    if (args[check].type == FH_VAL_INTEGER)
        return args[check].data.i;
    if (args[check].type == FH_VAL_FLOAT)
        return (int64_t) args[check].data.num;

    return opt;
}

bool fh_optboolean(struct fh_value *args, int n_args, int check, bool opt) {
    if (n_args <= check) {
        return opt;
    }
    if (args[check].type == FH_VAL_BOOL)
        return args[check].data.b;
    return opt;
}

const char *fh_optstring(struct fh_value *args, int n_args, int check, const char *opt) {
    if (n_args <= check) {
        return opt;
    }
    if (args[check].type == FH_VAL_STRING)
        return GET_VAL_STRING_DATA(&args[check]);
    return opt;
}

void *fh_optcobj(struct fh_value *args, int n_args, int check, short ctype, void *opt) {
    //NOTE: In this function we also have to check for the user ctype
    if (n_args <= check) {
        return opt;
    }
    if (args[check].type == FH_VAL_C_OBJ) {
        struct fh_c_obj *o = fh_get_c_obj(&args[check]);
        return o->type == ctype ? fh_get_c_obj_value(&args[check]) : opt;
    }

    return opt;
}

bool fh_is_c_obj_of_type(struct fh_value *v, int usr_type) {
    if (!fh_is_c_obj(v))
        return false;

    struct fh_c_obj *o = fh_get_c_obj(v);
    if (o->type == usr_type)
        return true;

    return false;
}
