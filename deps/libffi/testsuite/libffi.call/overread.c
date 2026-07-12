/* Area:        ffi_call
   Purpose:     Tests if ffi_call reads data beyond end.
   Limitations: needs mmap.
   PR:          887
   Originator:  Mikulas Patocka <mikulas@twibright.com>  */

/* { dg-do run } */

#include "ffitest.h"

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>

static int ABI_ATTR fn(unsigned char a, unsigned short b, unsigned int c, unsigned long d)
{
	return (int)(a + b + c + d);
}
#endif

int main(void)
{
#ifdef __linux__
	ffi_cif cif;
	ffi_type *args[MAX_ARGS];
	void *values[MAX_ARGS];
	ffi_arg rint;
	char *m;
	int ps;
	args[0] = &ffi_type_uchar;
	args[1] = &ffi_type_ushort;
	args[2] = &ffi_type_uint;
	args[3] = &ffi_type_ulong;
	CHECK(ffi_prep_cif(&cif, ABI_NUM, 4, &ffi_type_sint, args) == FFI_OK);
	ps = getpagesize();
	m = mmap(NULL, ps * 3, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	CHECK(m != MAP_FAILED);
	CHECK(mprotect(m, ps, PROT_NONE) == 0);
	CHECK(mprotect(m + ps * 2, ps, PROT_NONE) == 0);
	values[0] = m + ps * 2 - sizeof(unsigned char);
	values[1] = m + ps * 2 - sizeof(unsigned short);
	values[2] = m + ps * 2 - sizeof(unsigned int);
	values[3] = m + ps * 2 - sizeof(unsigned long);
	ffi_call(&cif, FFI_FN(fn), &rint, values);
	CHECK((int)rint == 0);
	values[0] = m + ps;
	values[1] = m + ps;
	values[2] = m + ps;
	values[3] = m + ps;
	ffi_call(&cif, FFI_FN(fn), &rint, values);
	CHECK((int)rint == 0);
#endif
	exit(0);
}
