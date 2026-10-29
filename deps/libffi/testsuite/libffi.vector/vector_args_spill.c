/* Area:	ffi_call
   Purpose:	Pass many vector arguments interleaved with scalars, enough to
		exhaust the vector argument registers and spill onto the stack.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef float f32x4 __attribute__((vector_size (16)));

/* Ten vectors exceeds the 8 vector argument registers on both AArch64 and
   x86-64, so v8/v9 are passed on the stack.  The scalars are interleaved to
   make sure the two register files advance independently.  */
static float
mix (int i0, f32x4 v0, f32x4 v1, double d0, f32x4 v2, f32x4 v3,
     f32x4 v4, int i1, f32x4 v5, f32x4 v6, f32x4 v7, double d1,
     f32x4 v8, f32x4 v9)
{
  float acc = 0;
  acc += 1 * v0[0] + v0[3];
  acc += 2 * v1[0] + v1[3];
  acc += 3 * v2[0] + v2[3];
  acc += 4 * v3[0] + v3[3];
  acc += 5 * v4[0] + v4[3];
  acc += 6 * v5[0] + v5[3];
  acc += 7 * v6[0] + v6[3];
  acc += 8 * v7[0] + v7[3];
  acc += 9 * v8[0] + v8[3];
  acc += 10 * v9[0] + v9[3];
  acc += i0 + i1 + (float) d0 + (float) d1;
  return acc;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[5];
  ffi_type *args[14];
  void *values[14];
  f32x4 v[10];
  int i0 = 100, i1 = 7;
  double d0 = 3.5, d1 = 0.25;
  float r, ref;
  unsigned k;

  make_vector_type (&vec_type, vec_elems, &ffi_type_float, 4);

  for (k = 0; k < 10; k++)
    {
      f32x4 t = { (float) (k + 1), 0, 0, (float) (100 + k) };
      v[k] = t;
    }

  args[0] = &ffi_type_sint;    values[0] = &i0;
  args[1] = &vec_type;         values[1] = &v[0];
  args[2] = &vec_type;         values[2] = &v[1];
  args[3] = &ffi_type_double;  values[3] = &d0;
  args[4] = &vec_type;         values[4] = &v[2];
  args[5] = &vec_type;         values[5] = &v[3];
  args[6] = &vec_type;         values[6] = &v[4];
  args[7] = &ffi_type_sint;    values[7] = &i1;
  args[8] = &vec_type;         values[8] = &v[5];
  args[9] = &vec_type;         values[9] = &v[6];
  args[10] = &vec_type;        values[10] = &v[7];
  args[11] = &ffi_type_double; values[11] = &d1;
  args[12] = &vec_type;        values[12] = &v[8];
  args[13] = &vec_type;        values[13] = &v[9];

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 14, &ffi_type_float, args)
	 == FFI_OK);

  ffi_call (&cif, FFI_FN (mix), &r, values);

  ref = mix (i0, v[0], v[1], d0, v[2], v[3], v[4], i1, v[5], v[6], v[7],
	     d1, v[8], v[9]);
  printf ("r = %g (want %g)\n", (double) r, (double) ref);
  CHECK (r == ref);

  exit (0);
}
