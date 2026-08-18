/* Area:	ffi_call
   Purpose:	Pass and return a 16-byte int32x4 integer vector.  Integer
		lanes still travel in a vector register, unlike an HFA of ints.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef int i32x4 __attribute__((vector_size (16)));

static i32x4
add_i32x4 (i32x4 a, i32x4 b)
{
  return a + b;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[5];
  ffi_type *args[2];
  void *values[2];
  i32x4 a = { 1, 2, 3, 4 };
  i32x4 b = { 5, 6, 7, 8 };
  i32x4 r, ref;
  int i;

  make_vector_type (&vec_type, vec_elems, &ffi_type_sint32, 4);

  args[0] = &vec_type;
  args[1] = &vec_type;
  values[0] = &a;
  values[1] = &b;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &vec_type, args) == FFI_OK);
  CHECK (vec_type.size == 16);
  CHECK (vec_type.alignment == 16);

  ffi_call (&cif, FFI_FN (add_i32x4), &r, values);

  ref = add_i32x4 (a, b);
  for (i = 0; i < 4; i++)
    {
      printf ("r[%d] = %d (want %d)\n", i, r[i], ref[i]);
      CHECK (r[i] == ref[i]);
    }

  exit (0);
}
