/* Area:	ffi_call_plan
   Purpose:	Check that a reusable call plan reproduces ffi_call for the
		pure-GP64 fast path, pointer arguments and repeated reuse,
		and that a signature with no fast path still yields a usable
		plan that falls back to ffi_call.
   Limitations:	none.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include "ffitest.h"

static uint64_t gp6(uint64_t a, uint64_t b, uint64_t c,
		    uint64_t d, uint64_t e, uint64_t f)
{
  return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}

static void *ptr_ident(void *p)
{
  return p;
}

struct small_pair { long x; long y; };

static long ssum(struct small_pair s)
{
  return s.x - s.y;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[6];
  void *values[6];
  ffi_call_plan *plan;
  uint64_t a[6], r_call, r_plan;
  int i, k;

  /* Pure GP64: every argument is one 64-bit integer, so build_plan
     selects the ffi_plan_gpN direct thunk.  Reuse the plan across many
     invocations with changing values.  */
  for (i = 0; i < 6; i++)
    args[i] = &ffi_type_uint64;
  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 6, &ffi_type_uint64, args)
	== FFI_OK);
  plan = ffi_call_plan_alloc(&cif);
  CHECK(plan != NULL);

  for (k = 0; k < 100; k++)
    {
      for (i = 0; i < 6; i++)
	{
	  a[i] = (uint64_t) (k * 7 + i + 1);
	  values[i] = &a[i];
	}
      ffi_call(&cif, FFI_FN(gp6), &r_call, values);
      ffi_call_plan_invoke(plan, FFI_FN(gp6), &r_plan, values);
      CHECK(r_call == r_plan);
      CHECK(r_plan == gp6(a[0], a[1], a[2], a[3], a[4], a[5]));
    }
  ffi_call_plan_free(plan);

  /* Pointer argument and pointer return. */
  {
    ffi_cif cifp;
    ffi_type *pargs[1];
    void *pvalues[1];
    void *in, *rc, *rp;
    ffi_call_plan *planp;

    pargs[0] = &ffi_type_pointer;
    CHECK(ffi_prep_cif(&cifp, FFI_DEFAULT_ABI, 1, &ffi_type_pointer, pargs)
	  == FFI_OK);
    planp = ffi_call_plan_alloc(&cifp);
    CHECK(planp != NULL);

    in = &cifp;
    pvalues[0] = &in;
    ffi_call(&cifp, FFI_FN(ptr_ident), &rc, pvalues);
    ffi_call_plan_invoke(planp, FFI_FN(ptr_ident), &rp, pvalues);
    CHECK(rc == in);
    CHECK(rp == in);
    ffi_call_plan_free(planp);
  }

  /* No fast path: a struct-by-value argument.  build_plan returns NULL for
     the fast plan, but ffi_call_plan_alloc must still hand back a valid plan
     whose invoke falls back to ffi_call and produces the same result.  */
  {
    ffi_cif cifs;
    ffi_type *sargs[1];
    ffi_type stype;
    ffi_type *selements[3];
    void *svalues[1];
    struct small_pair s;
    ffi_arg rc, rp;
    ffi_call_plan *plans;

    selements[0] = &ffi_type_slong;
    selements[1] = &ffi_type_slong;
    selements[2] = NULL;
    stype.size = stype.alignment = 0;
    stype.type = FFI_TYPE_STRUCT;
    stype.elements = selements;

    sargs[0] = &stype;
    CHECK(ffi_prep_cif(&cifs, FFI_DEFAULT_ABI, 1, &ffi_type_slong, sargs)
	  == FFI_OK);
    plans = ffi_call_plan_alloc(&cifs);
    CHECK(plans != NULL);

    s.x = 123;
    s.y = 45;
    svalues[0] = &s;
    ffi_call(&cifs, FFI_FN(ssum), &rc, svalues);
    ffi_call_plan_invoke(plans, FFI_FN(ssum), &rp, svalues);
    CHECK(rc == rp);
    CHECK((long) rp == ssum(s));
    ffi_call_plan_free(plans);
  }

  /* Freeing NULL is documented to be harmless. */
  ffi_call_plan_free(NULL);

  exit(0);
}
