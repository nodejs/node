/* Area:	ffi_call
   Purpose:	Pass and return a 16-byte float32x4 vector (the vec4 shape of
		libffi/libffi#773).
   Limitations:	none.
   PR:		libffi/libffi#773.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef float f32x4 __attribute__((vector_size (16)));

static f32x4
add_f32x4 (f32x4 a, f32x4 b)
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
  f32x4 a = { 1, 2, 3, 4 };
  f32x4 b = { 10, 20, 30, 40 };
  f32x4 r, ref;
  int i;

  make_vector_type (&vec_type, vec_elems, &ffi_type_float, 4);

  args[0] = &vec_type;
  args[1] = &vec_type;
  values[0] = &a;
  values[1] = &b;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &vec_type, args) == FFI_OK);
  CHECK (vec_type.size == 16);
  CHECK (vec_type.alignment == 16);

  ffi_call (&cif, FFI_FN (add_f32x4), &r, values);

  ref = add_f32x4 (a, b);
  for (i = 0; i < 4; i++)
    {
      printf ("r[%d] = %g (want %g)\n", i, (double) r[i], (double) ref[i]);
      CHECK (r[i] == ref[i]);
    }

  exit (0);
}
