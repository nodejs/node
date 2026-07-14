/* Area:	ffi_call_plan
   Purpose:	Check that a reusable call plan reproduces ffi_call for
		signatures that mix general-purpose and SSE registers, for
		float and double returns, for signed narrow arguments, and
		for integer returns narrower than a register (which the plan
		must widen to ffi_arg exactly as ffi_call does).
   Limitations:	none.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include "ffitest.h"

static double mixed(int a, double b, long c, float d,
		    long long e, double f)
{
  return (double) a + b + (double) c + (double) d + (double) e + f;
}

static float faddf(float a, float b)
{
  return a + b;
}

static signed char ret_sc(signed char x)
{
  return (signed char) (x + 1);
}

static unsigned char ret_uc(unsigned char x)
{
  return (unsigned char) (x + 1);
}

int main (void)
{
  /* Mixed GP + SSE arguments, double return: exercises the fast local-image
     path with both integer and SSE moves and the ssecount (al) setup.  */
  {
    ffi_cif cif;
    ffi_type *args[6];
    void *values[6];
    ffi_call_plan *plan;
    int a = 3;
    double b = 1.5, f = -2.25, rc, rp;
    long c = 7;
    float d = 0.5f;
    long long e = -11;

    args[0] = &ffi_type_sint;
    args[1] = &ffi_type_double;
    args[2] = &ffi_type_slong;
    args[3] = &ffi_type_float;
    args[4] = &ffi_type_sint64;
    args[5] = &ffi_type_double;
    values[0] = &a; values[1] = &b; values[2] = &c;
    values[3] = &d; values[4] = &e; values[5] = &f;

    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 6, &ffi_type_double, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(mixed), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(mixed), &rp, values);
    CHECK_DOUBLE_EQ(rc, rp);
    CHECK_DOUBLE_EQ(rp, mixed(a, b, c, d, e, f));
    ffi_call_plan_free(plan);
  }

  /* Float arguments and float return. */
  {
    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    ffi_call_plan *plan;
    float a = 1.25f, b = 2.5f, rc, rp;

    args[0] = &ffi_type_float;
    args[1] = &ffi_type_float;
    values[0] = &a;
    values[1] = &b;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_float, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(faddf), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(faddf), &rp, values);
    CHECK_FLOAT_EQ(rc, rp);
    CHECK_FLOAT_EQ(rp, faddf(a, b));
    ffi_call_plan_free(plan);
  }

  /* Signed narrow argument and signed narrow return: the plan sign-extends
     the argument and widens the return to ffi_arg just as ffi_call does. */
  {
    ffi_cif cif;
    ffi_type *args[1];
    void *values[1];
    ffi_call_plan *plan;
    signed char in;
    ffi_arg rc, rp;
    int v;

    args[0] = &ffi_type_schar;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_schar, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    for (v = -128; v < 128; v++)
      {
	in = (signed char) v;
	values[0] = &in;
	ffi_call(&cif, FFI_FN(ret_sc), &rc, values);
	ffi_call_plan_invoke(plan, FFI_FN(ret_sc), &rp, values);
	CHECK(rc == rp);
	CHECK((signed char) rp == ret_sc(in));
      }
    ffi_call_plan_free(plan);
  }

  /* Unsigned narrow argument and unsigned narrow return. */
  {
    ffi_cif cif;
    ffi_type *args[1];
    void *values[1];
    ffi_call_plan *plan;
    unsigned char in;
    ffi_arg rc, rp;
    int v;

    args[0] = &ffi_type_uchar;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_uchar, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    for (v = 0; v < 256; v++)
      {
	in = (unsigned char) v;
	values[0] = &in;
	ffi_call(&cif, FFI_FN(ret_uc), &rc, values);
	ffi_call_plan_invoke(plan, FFI_FN(ret_uc), &rp, values);
	CHECK(rc == rp);
	CHECK((unsigned char) rp == ret_uc(in));
      }
    ffi_call_plan_free(plan);
  }

  exit(0);
}
