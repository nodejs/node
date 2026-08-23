/* Area:	closure_call
   Purpose:	A closure that receives two vector arguments (and a scalar) and
		returns a vector.  Exercises the closure argument-extraction and
		vector return paths.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef float f32x4 __attribute__((vector_size (16)));

static void
cls_vector_fn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	       void *userdata __UNUSED__)
{
  f32x4 a = *(f32x4 *) args[0];
  f32x4 b = *(f32x4 *) args[1];
  int scale = *(int *) args[2];
  f32x4 *r = (f32x4 *) resp;

  *r = (a + b) * (float) scale;
}

typedef f32x4 (*cls_vector_t) (f32x4, f32x4, int);

int
main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type vec_type;
  ffi_type *vec_elems[5];
  ffi_type *arg_types[3];
  f32x4 a = { 1, 2, 3, 4 };
  f32x4 b = { 10, 20, 30, 40 };
  f32x4 res;
  int scale = 2;
  int i;

  CHECK (pcl != NULL);

  make_vector_type (&vec_type, vec_elems, &ffi_type_float, 4);

  arg_types[0] = &vec_type;
  arg_types[1] = &vec_type;
  arg_types[2] = &ffi_type_sint;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 3, &vec_type, arg_types)
	 == FFI_OK);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_vector_fn, NULL, code)
	 == FFI_OK);

  res = ((cls_vector_t) code) (a, b, scale);

  for (i = 0; i < 4; i++)
    {
      float want = (a[i] + b[i]) * (float) scale;
      printf ("res[%d] = %g (want %g)\n", i, (double) res[i], (double) want);
      CHECK (res[i] == want);
    }

  exit (0);
}
