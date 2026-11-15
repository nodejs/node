/* -*-c-*- */
#include "ffitest.h"
#include <complex.h>

/* A complex argument that arrives when the argument registers are nearly
   used up.  Seven doubles leave one floating-point argument register on
   RISC-V and LoongArch, so the complex value has to move to integer
   registers as a whole.  Five ints leave an odd number of GPRs on 32-bit
   PowerPC, where a two-word complex skips to the next even register pair
   and a larger one goes to the stack, followed by every later argument.
   Eight doubles and eight ints push the complex value onto the stack on
   every target.  */

typedef _Complex T_C_TYPE C;

static C
fn_d7 (double d1, double d2, double d3, double d4, double d5, double d6,
       double d7, C c, int k)
{
  return c * k + (d1 + d2 + d3 + d4 + d5 + d6 + d7);
}

static C
fn_i5 (int i1, int i2, int i3, int i4, int i5, C c, int k)
{
  return c * k + (i1 + i2 + i3 + i4 + i5);
}

static C
fn_d8i8 (double d1, double d2, double d3, double d4, double d5, double d6,
	 double d7, double d8, int i1, int i2, int i3, int i4, int i5, int i6,
	 int i7, int i8, C c, int k)
{
  return c * k + (d1 + d2 + d3 + d4 + d5 + d6 + d7 + d8
		  + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8);
}

/* SHAPE[0] doubles, SHAPE[1] ints, then the complex value and an int.  */
static void
cls_fn (ffi_cif *cif __UNUSED__, void *resp, void **args, void *userdata)
{
  const int *shape = (const int *) userdata;
  T_C_TYPE s = 0;
  int i, n = 0;

  for (i = 0; i < shape[0]; i++)
    s += *(double *) args[n++];
  for (i = 0; i < shape[1]; i++)
    s += *(int *) args[n++];
  *(C *) resp = *(C *) args[n] * *(int *) args[n + 1] + s;
}

static void
check (const char *name, C got, C want)
{
  printf ("%s: %.3f,%.3fi (want %.3f,%.3fi)\n", name,
	  T_CONV creal (got), T_CONV cimag (got),
	  T_CONV creal (want), T_CONV cimag (want));
  CHECK (creal (got) == creal (want));
  CHECK (cimag (got) == cimag (want));
}

static void
prep (ffi_cif *cif, ffi_type **types, void **values, int nd, int ni,
      double *d, int *i, C *c, int *k)
{
  int j, n = 0;

  for (j = 0; j < nd; j++, n++)
    {
      types[n] = &ffi_type_double;
      values[n] = &d[j];
    }
  for (j = 0; j < ni; j++, n++)
    {
      types[n] = &ffi_type_sint;
      values[n] = &i[j];
    }
  types[n] = &T_FFI_TYPE;
  values[n++] = c;
  types[n] = &ffi_type_sint;
  values[n++] = k;
  CHECK (ffi_prep_cif (cif, FFI_DEFAULT_ABI, n, &T_FFI_TYPE, types) == FFI_OK);
}

int
main (void)
{
  static const int shape_d7[2] = { 7, 0 };
  static const int shape_i5[2] = { 0, 5 };
  static const int shape_d8i8[2] = { 8, 8 };
  double d[8] = { 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5 };
  int i[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
  C c = 1.25 + 2.5 * I;
  int k = 77;
  ffi_type *types[18];
  void *values[18];
  ffi_cif cif;
  ffi_closure *pcl;
  void *code;
  C r;

  pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (pcl != NULL);

  prep (&cif, types, values, 7, 0, d, i, &c, &k);
  ffi_call (&cif, FFI_FN (fn_d7), &r, values);
  check ("call d7", r, fn_d7 (d[0], d[1], d[2], d[3], d[4], d[5], d[6], c, k));
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_fn, (void *) shape_d7, code)
	 == FFI_OK);
  r = ((C (*)(double, double, double, double, double, double, double, C,
	      int)) code) (d[0], d[1], d[2], d[3], d[4], d[5], d[6], c, k);
  check ("closure d7", r,
	 fn_d7 (d[0], d[1], d[2], d[3], d[4], d[5], d[6], c, k));

  prep (&cif, types, values, 0, 5, d, i, &c, &k);
  ffi_call (&cif, FFI_FN (fn_i5), &r, values);
  check ("call i5", r, fn_i5 (i[0], i[1], i[2], i[3], i[4], c, k));
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_fn, (void *) shape_i5, code)
	 == FFI_OK);
  r = ((C (*)(int, int, int, int, int, C, int)) code)
    (i[0], i[1], i[2], i[3], i[4], c, k);
  check ("closure i5", r, fn_i5 (i[0], i[1], i[2], i[3], i[4], c, k));

  prep (&cif, types, values, 8, 8, d, i, &c, &k);
  ffi_call (&cif, FFI_FN (fn_d8i8), &r, values);
  check ("call d8i8", r,
	 fn_d8i8 (d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
		  i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], c, k));
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_fn, (void *) shape_d8i8, code)
	 == FFI_OK);
  r = ((C (*)(double, double, double, double, double, double, double, double,
	      int, int, int, int, int, int, int, int, C, int)) code)
    (d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
     i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], c, k);
  check ("closure d8i8", r,
	 fn_d8i8 (d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
		  i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], c, k));

  ffi_closure_free (pcl);
  exit (0);
}
