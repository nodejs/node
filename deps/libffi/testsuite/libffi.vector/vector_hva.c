/* Area:	ffi_call
   Purpose:	Pass and return a homogeneous vector aggregate: a struct of two
		identical 16-byte vectors.  On AArch64 this is an HVA carried in
		a pair of Q registers; on x86-64 the existing SSE struct
		classification handles it (four SSE eightbytes).  Both round-trip.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef float f32x4 __attribute__((vector_size (16)));

struct hva2
{
  f32x4 a;
  f32x4 b;
};

static struct hva2
bump (struct hva2 s)
{
  s.a = s.a + 1;
  s.b = s.b + 2;
  return s;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[5];
  ffi_type struct_type;
  ffi_type *struct_elems[3];
  ffi_type *args[1];
  void *values[1];
  struct hva2 in = { { 1, 2, 3, 4 }, { 10, 20, 30, 40 } };
  struct hva2 out, ref;
  int i;

  make_vector_type (&vec_type, vec_elems, &ffi_type_float, 4);

  struct_elems[0] = &vec_type;
  struct_elems[1] = &vec_type;
  struct_elems[2] = NULL;
  struct_type.size = 0;
  struct_type.alignment = 0;
  struct_type.type = FFI_TYPE_STRUCT;
  struct_type.elements = struct_elems;

  args[0] = &struct_type;
  values[0] = &in;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &struct_type, args)
	 == FFI_OK);
  CHECK (struct_type.size == 32);

  ffi_call (&cif, FFI_FN (bump), &out, values);

  ref = bump (in);
  for (i = 0; i < 4; i++)
    {
      printf ("a[%d] = %g (want %g), b[%d] = %g (want %g)\n",
	      i, (double) out.a[i], (double) ref.a[i],
	      i, (double) out.b[i], (double) ref.b[i]);
      CHECK (out.a[i] == ref.a[i]);
      CHECK (out.b[i] == ref.b[i]);
    }

  exit (0);
}
