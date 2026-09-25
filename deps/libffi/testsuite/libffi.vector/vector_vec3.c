/* Area:	ffi_call
   Purpose:	Pass and return a three-lane float vector.  Clang's
		ext_vector_type(3) has 12 bytes of data padded to 16-byte
		storage; libffi's power-of-two size rule must reproduce that
		layout so a natively compiled callee agrees.
   Limitations:	Clang only (GCC's vector_size requires power-of-two totals and
		rejects a 12-byte vector).  A no-op on other compilers.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

#ifdef __clang__

typedef float f3 __attribute__((ext_vector_type (3)));

static f3
scale3 (f3 v)
{
  f3 r;
  r[0] = v[0] + 1;
  r[1] = v[1] + 2;
  r[2] = v[2] + 3;
  return r;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[4];
  ffi_type *args[1];
  void *values[1];
  f3 a = { 10, 20, 30 };
  f3 r, ref;
  int i;

  make_vector_type (&vec_type, vec_elems, &ffi_type_float, 3);

  args[0] = &vec_type;
  values[0] = &a;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &vec_type, args) == FFI_OK);
  /* 3 x float = 12, rounded up to 16 (matches ext_vector_type storage).  */
  CHECK (vec_type.size == 16);
  CHECK (vec_type.alignment == 16);
  CHECK (sizeof (f3) == 16);

  ffi_call (&cif, FFI_FN (scale3), &r, values);

  ref = scale3 (a);
  for (i = 0; i < 3; i++)
    {
      printf ("r[%d] = %g (want %g)\n", i, (double) r[i], (double) ref[i]);
      CHECK (r[i] == ref[i]);
    }

  exit (0);
}

#else

int
main (void)
{
  /* ext_vector_type is a Clang extension; nothing to test elsewhere.  */
  exit (0);
}

#endif
