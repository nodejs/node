/* Area:	ffi_prep_types
   Purpose:	Check that _Complex long double follows the abi's long double width.
   Limitations:	powerpc64 only -- the one target where long double has more
		than one width at run time.
   PR:		none.
   Originator:	Anthony Green 20260916	 */

/* { dg-do run { target powerpc64*-*-linux-gnu* } } */

#include "ffitest.h"

/* ffi_prep_cif() and ffi_get_struct_offsets() resize ffi_type_longdouble to
   match the FFI_LINUX_LONG_DOUBLE_128 bit of the abi they are handed, so that
   one libffi can describe calls into libraries built with either width.  That
   is also why these two types, alone among the ffi_type objects, are not
   const.  ffi_type_complex_longdouble has to keep up: two halves of whatever
   long double currently is.  */

static void
check_width (ffi_abi abi, size_t ld_size, unsigned short ld_align)
{
  ffi_cif cif;
  ffi_type *args[1];
  ffi_type *elements[2];
  ffi_type s;
  size_t offsets[1];

  args[0] = &ffi_type_complex_longdouble;
  CHECK (ffi_prep_cif (&cif, abi, 1, &ffi_type_complex_longdouble, args)
	 == FFI_OK);

  CHECK (ffi_type_longdouble.size == ld_size);
  CHECK (ffi_type_longdouble.alignment == ld_align);

  CHECK (ffi_type_complex_longdouble.size == 2 * ld_size);
  CHECK (ffi_type_complex_longdouble.alignment == ld_align);
  CHECK (ffi_type_complex_longdouble.elements[0]->size == ld_size);

  /* A struct built out of that type has to come out the same size, which is
     what ffi_get_struct_offsets() and every struct-passing path rely on.  */
  elements[0] = &ffi_type_complex_longdouble;
  elements[1] = NULL;
  s.size = 0;
  s.alignment = 0;
  s.type = FFI_TYPE_STRUCT;
  s.elements = elements;
  CHECK (ffi_get_struct_offsets (abi, &s, offsets) == FFI_OK);
  CHECK (s.size == 2 * ld_size);
  CHECK (offsets[0] == 0);
}

int
main (void)
{
  size_t native = (FFI_DEFAULT_ABI & FFI_LINUX_LONG_DOUBLE_128) ? 16 : 8;

  /* A 64-bit long double, as musl selects on ppc64le.  */
  check_width ((ffi_abi) FFI_LINUX, 8, 8);
  /* A 128-bit long double, either IBM-128 or IEEE-128.  */
  check_width ((ffi_abi) (FFI_LINUX | FFI_LINUX_LONG_DOUBLE_128), 16, 16);
  /* Leave the globals as this build's default abi found them.  */
  check_width (FFI_DEFAULT_ABI, native, (unsigned short) native);

  printf ("success\n");
  exit (0);
}

/* { dg-output "success" } */
