/* Area:	ffi_call_plan_size
   Purpose:	Check that a plan reports its own allocation size, that the
		size is stable across invocations, and that a NULL plan has
		no footprint.
   Limitations:	The exact byte count is implementation defined, so this only
		checks the invariants callers may rely on.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include "ffitest.h"

static uint64_t gp2(uint64_t a, uint64_t b)
{
  return a + b * 2;
}

static uint64_t gp6(uint64_t a, uint64_t b, uint64_t c,
		    uint64_t d, uint64_t e, uint64_t f)
{
  return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}

int main (void)
{
  ffi_cif cif2, cif6;
  ffi_type *args[6];
  void *values[6];
  ffi_call_plan *plan2, *plan6;
  size_t size2, size6;
  uint64_t a[6], r;
  int i;

  for (i = 0; i < 6; i++)
    {
      args[i] = &ffi_type_uint64;
      a[i] = (uint64_t) (i + 1);
      values[i] = &a[i];
    }

  CHECK(ffi_prep_cif(&cif2, FFI_DEFAULT_ABI, 2, &ffi_type_uint64, args)
	== FFI_OK);
  CHECK(ffi_prep_cif(&cif6, FFI_DEFAULT_ABI, 6, &ffi_type_uint64, args)
	== FFI_OK);

  /* A NULL plan has no footprint, mirroring ffi_call_plan_free(NULL).  */
  CHECK(ffi_call_plan_size(NULL) == 0);

  plan2 = ffi_call_plan_alloc(&cif2);
  CHECK(plan2 != NULL);
  plan6 = ffi_call_plan_alloc(&cif6);
  CHECK(plan6 != NULL);

  size2 = ffi_call_plan_size(plan2);
  size6 = ffi_call_plan_size(plan6);

  /* Every plan owns at least its handle, and a wider signature never needs
     less memory than a narrower one of the same shape.  Targets without a
     fast path report the same constant for both.  */
  CHECK(size2 > 0);
  CHECK(size6 >= size2);

  /* The plan is immutable, so querying it must not disturb invocation and
     the reported size must not drift across calls.  */
  ffi_call_plan_invoke(plan6, FFI_FN(gp6), &r, values);
  CHECK(r == gp6(a[0], a[1], a[2], a[3], a[4], a[5]));
  CHECK(ffi_call_plan_size(plan6) == size6);

  ffi_call_plan_invoke(plan2, FFI_FN(gp2), &r, values);
  CHECK(r == gp2(a[0], a[1]));
  CHECK(ffi_call_plan_size(plan2) == size2);

  ffi_call_plan_free(plan2);
  ffi_call_plan_free(plan6);

  exit(0);
}
