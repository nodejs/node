/* Area:	ffi_call_plan
   Purpose:	Check that a reusable call plan reproduces ffi_call when
		arguments spill past the argument registers onto the stack.
		This drives the non-fast plan path (a move list handed to
		ffi_call_unix64) for GP spill, SSE spill, and a mix of both.
   Limitations:	none.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include "ffitest.h"

/* 10 integers: 6 in registers, 4 spilled to the stack. */
static long long gp10(long long a1, long long a2, long long a3, long long a4,
		      long long a5, long long a6, long long a7, long long a8,
		      long long a9, long long a10)
{
  return a1 + a2 * 2 + a3 * 3 + a4 * 4 + a5 * 5
       + a6 * 6 + a7 * 7 + a8 * 8 + a9 * 9 + a10 * 10;
}

/* 12 doubles: 8 in SSE registers, 4 spilled to the stack. */
static double sse12(double a1, double a2, double a3, double a4,
		    double a5, double a6, double a7, double a8,
		    double a9, double a10, double a11, double a12)
{
  return a1 + a2 * 2 + a3 * 3 + a4 * 4 + a5 * 5 + a6 * 6
       + a7 * 7 + a8 * 8 + a9 * 9 + a10 * 10 + a11 * 11 + a12 * 12;
}

/* 8 ints + 8 doubles: both classes spill. */
static double mix16(long i1, long i2, long i3, long i4,
		    long i5, long i6, long i7, long i8,
		    double d1, double d2, double d3, double d4,
		    double d5, double d6, double d7, double d8)
{
  double s = 0.0;
  s += (double) (i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8);
  s += d1 + d2 + d3 + d4 + d5 + d6 + d7 + d8;
  return s;
}

int main (void)
{
  /* GP spill. */
  {
    ffi_cif cif;
    ffi_type *args[10];
    void *values[10];
    long long a[10];
    long long rc, rp;
    ffi_call_plan *plan;
    int i;

    for (i = 0; i < 10; i++)
      {
	args[i] = &ffi_type_sint64;
	a[i] = (long long) (i + 1) * 100 - 3;
	values[i] = &a[i];
      }
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 10, &ffi_type_sint64, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(gp10), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(gp10), &rp, values);
    CHECK(rc == rp);
    CHECK(rp == gp10(a[0], a[1], a[2], a[3], a[4],
		     a[5], a[6], a[7], a[8], a[9]));
    ffi_call_plan_free(plan);
  }

  /* SSE spill. */
  {
    ffi_cif cif;
    ffi_type *args[12];
    void *values[12];
    double a[12];
    double rc, rp;
    ffi_call_plan *plan;
    int i;

    for (i = 0; i < 12; i++)
      {
	args[i] = &ffi_type_double;
	a[i] = (double) i + 0.25;
	values[i] = &a[i];
      }
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 12, &ffi_type_double, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(sse12), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(sse12), &rp, values);
    CHECK_DOUBLE_EQ(rc, rp);
    CHECK_DOUBLE_EQ(rp, sse12(a[0], a[1], a[2], a[3], a[4], a[5],
			      a[6], a[7], a[8], a[9], a[10], a[11]));
    ffi_call_plan_free(plan);
  }

  /* Both classes spill. */
  {
    ffi_cif cif;
    ffi_type *args[16];
    void *values[16];
    long iv[8];
    double dv[8];
    double rc, rp;
    ffi_call_plan *plan;
    int i;

    for (i = 0; i < 8; i++)
      {
	args[i] = &ffi_type_slong;
	iv[i] = (long) (i + 1);
	values[i] = &iv[i];
      }
    for (i = 0; i < 8; i++)
      {
	args[8 + i] = &ffi_type_double;
	dv[i] = (double) (i + 1) * 0.5;
	values[8 + i] = &dv[i];
      }
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 16, &ffi_type_double, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(mix16), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(mix16), &rp, values);
    CHECK_DOUBLE_EQ(rc, rp);
    CHECK_DOUBLE_EQ(rp, mix16(iv[0], iv[1], iv[2], iv[3],
			      iv[4], iv[5], iv[6], iv[7],
			      dv[0], dv[1], dv[2], dv[3],
			      dv[4], dv[5], dv[6], dv[7]));
    ffi_call_plan_free(plan);
  }

  exit(0);
}
