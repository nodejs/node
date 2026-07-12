/* Area:	ffi_call_plan
   Purpose:	Check that a reusable call plan reproduces ffi_call for struct
		returns.  A struct return does not disable planning (only a
		struct *argument* does), so this drives both the in-memory
		return path (RET_IN_MEM, including a NULL rvalue) and the
		register-pair struct return path, plus a large struct argument
		that forces the ffi_call by-value copy fallback.
   Limitations:	none.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include "ffitest.h"

static int call_count = 0;

/* 24 bytes: returned in memory (a hidden pointer in the first argument). */
struct big3 { double a, b, c; };

static struct big3 make_big3(double a, double b, double c)
{
  struct big3 r;
  call_count++;
  r.a = a + 1.0;
  r.b = b + 2.0;
  r.c = c + 3.0;
  return r;
}

/* A struct larger than 16 bytes passed by value forces ffi_call to make a
   copy; build_plan has no fast path for a struct argument, so this exercises
   the plan's fallback to ffi_call. */
static double sum_big3(struct big3 s)
{
  return s.a + s.b + s.c;
}

/* 16 bytes: returned in a register pair (RAX:RDX on x86-64). */
struct pair2 { long x, y; };

static struct pair2 make_pair2(long x, long y)
{
  struct pair2 r;
  r.x = x * 2;
  r.y = y * 3;
  return r;
}

int main (void)
{
  ffi_type *big3_elements[4];
  ffi_type big3_t;
  ffi_type *pair2_elements[3];
  ffi_type pair2_t;

  big3_elements[0] = &ffi_type_double;
  big3_elements[1] = &ffi_type_double;
  big3_elements[2] = &ffi_type_double;
  big3_elements[3] = NULL;
  big3_t.size = big3_t.alignment = 0;
  big3_t.type = FFI_TYPE_STRUCT;
  big3_t.elements = big3_elements;

  pair2_elements[0] = &ffi_type_slong;
  pair2_elements[1] = &ffi_type_slong;
  pair2_elements[2] = NULL;
  pair2_t.size = pair2_t.alignment = 0;
  pair2_t.type = FFI_TYPE_STRUCT;
  pair2_t.elements = pair2_elements;

  /* In-memory struct return with scalar arguments. */
  {
    ffi_cif cif;
    ffi_type *args[3];
    void *values[3];
    ffi_call_plan *plan;
    double a = 10.0, b = 20.0, c = 30.0;
    struct big3 rc, rp;
    int before;

    args[0] = &ffi_type_double;
    args[1] = &ffi_type_double;
    args[2] = &ffi_type_double;
    values[0] = &a;
    values[1] = &b;
    values[2] = &c;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &big3_t, args) == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(make_big3), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(make_big3), &rp, values);
    CHECK_DOUBLE_EQ(rc.a, rp.a);
    CHECK_DOUBLE_EQ(rc.b, rp.b);
    CHECK_DOUBLE_EQ(rc.c, rp.c);
    CHECK_DOUBLE_EQ(rp.a, a + 1.0);
    CHECK_DOUBLE_EQ(rp.b, b + 2.0);
    CHECK_DOUBLE_EQ(rp.c, c + 3.0);

    /* A NULL rvalue for an in-memory struct return must not crash: libffi
       supplies scratch space and discards the result, but the callee still
       runs.  Confirm the call actually happened. */
    before = call_count;
    ffi_call_plan_invoke(plan, FFI_FN(make_big3), NULL, values);
    CHECK(call_count == before + 1);

    ffi_call_plan_free(plan);
  }

  /* Large struct argument: no fast path, falls back to ffi_call. */
  {
    ffi_cif cif;
    ffi_type *args[1];
    void *values[1];
    ffi_call_plan *plan;
    struct big3 s;
    double rc, rp;

    s.a = 1.5;
    s.b = 2.5;
    s.c = 3.5;
    args[0] = &big3_t;
    values[0] = &s;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_double, args)
	  == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(sum_big3), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(sum_big3), &rp, values);
    CHECK_DOUBLE_EQ(rc, rp);
    CHECK_DOUBLE_EQ(rp, sum_big3(s));
    ffi_call_plan_free(plan);
  }

  /* Register-pair struct return. */
  {
    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    ffi_call_plan *plan;
    long x = 7, y = 11;
    struct pair2 rc, rp;

    args[0] = &ffi_type_slong;
    args[1] = &ffi_type_slong;
    values[0] = &x;
    values[1] = &y;
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &pair2_t, args) == FFI_OK);
    plan = ffi_call_plan_alloc(&cif);
    CHECK(plan != NULL);

    ffi_call(&cif, FFI_FN(make_pair2), &rc, values);
    ffi_call_plan_invoke(plan, FFI_FN(make_pair2), &rp, values);
    CHECK(rc.x == rp.x);
    CHECK(rc.y == rp.y);
    CHECK(rp.x == x * 2);
    CHECK(rp.y == y * 3);
    ffi_call_plan_free(plan);
  }

  exit(0);
}
