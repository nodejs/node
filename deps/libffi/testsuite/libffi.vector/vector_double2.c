/* Area:	ffi_call
   Purpose:	Pass and return a 16-byte double2 vector (single Q/SSE reg).
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef double d2 __attribute__((vector_size (16)));

static d2
add_d2 (d2 a, d2 b)
{
  return a + b;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[3];
  ffi_type *args[2];
  void *values[2];
  d2 a = { 1.5, 2.5 };
  d2 b = { 10.0, 20.0 };
  d2 r, ref;
  int i;

  make_vector_type (&vec_type, vec_elems, &ffi_type_double, 2);

  args[0] = &vec_type;
  args[1] = &vec_type;
  values[0] = &a;
  values[1] = &b;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &vec_type, args) == FFI_OK);
  CHECK (vec_type.size == 16);
  CHECK (vec_type.alignment == 16);

  ffi_call (&cif, FFI_FN (add_d2), &r, values);

  ref = add_d2 (a, b);
  for (i = 0; i < 2; i++)
    {
      printf ("r[%d] = %g (want %g)\n", i, r[i], ref[i]);
      CHECK (r[i] == ref[i]);
    }

  exit (0);
}
