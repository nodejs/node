/* Area:        ffi_call
   Purpose:     Check zero-extension of pointers on x32.
   Limitations: none.
   PR:          887
   Originator:  Mikulas Patocka <mikulas@twibright.com>  */

/* { dg-do run } */

#include "ffitest.h"

static int ABI_ATTR fn(int *a)
{
	if (a)
		return *a;
	return -1;
}

int main(void)
{
	ffi_cif cif;
	ffi_type *args[MAX_ARGS];
	void *values[MAX_ARGS];
	void *z[2] = { (void *)0, (void *)1 };
	ffi_arg rint;
	args[0] = &ffi_type_pointer;
	values[0] = z;
	CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ffi_type_sint, args) == FFI_OK);
	ffi_call(&cif, FFI_FN(fn), &rint, values);
	CHECK((int)rint == -1);
	exit(0);
}
