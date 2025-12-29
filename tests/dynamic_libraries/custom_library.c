/*
   This example shows how to create a custom library
   then load it into FH.
*/
#include "custom_library.h"

#include <stdlib.h>
#include <stdio.h>

static int fn_custom_library_func1(struct fh_program *prog, struct fh_value *ret,
 struct fh_value *args, int n_args)
{
	puts("LOADING CODE FROM CUSTOM LIBRARY WORKS");
	*ret = fh_new_number(42);
	return 0;
}

#define DEF_FN(name) { #name, fn_##name }
static const struct fh_named_c_func c_funcs[] = {
	DEF_FN(custom_library_func1),
};

int fh_register_library(struct fh_program* prog) {
	return fh_add_c_funcs(prog, c_funcs, sizeof(c_funcs) / sizeof(c_funcs[0]));
}
