/* Area:	ffi_call_plan
   Purpose:	Check that a reusable call plan reproduces ffi_call for a
		variadic signature prepared with ffi_prep_cif_var.  The
		variadic double arguments travel in SSE registers, so the
		fast path must set the vector-register count (al) correctly.
   Limitations:	none.
   PR:		none.
   Originator:	ffi_call_plan tests  */

/* { dg-do run } */
#include <stdarg.h>
#include "ffitest.h"

static double vsum(int n, ...)
{
  va_list ap;
  double s = 0.0;
  int i;

  va_start(ap, n);
  for (i = 0; i < n; i++)
    s += va_arg(ap, double);
  va_end(ap);
  return s;
}

static void
run (int nvar)
{
  ffi_cif cif;
  ffi_type *args[1 + 8];
  void *values[1 + 8];
  double d[8];
  int n = nvar;
  double rc, rp, expect;
  ffi_call_plan *plan;
  int i;

  CHECK(nvar <= 8);

  args[0] = &ffi_type_sint;
  values[0] = &n;
  expect = 0.0;
  for (i = 0; i < nvar; i++)
    {
      d[i] = (double) (i + 1) * 1.5;
      args[1 + i] = &ffi_type_double;
      values[1 + i] = &d[i];
      expect += d[i];
    }

  CHECK(ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 1, 1 + nvar,
			 &ffi_type_double, args) == FFI_OK);
  plan = ffi_call_plan_alloc(&cif);
  CHECK(plan != NULL);

  ffi_call(&cif, FFI_FN(vsum), &rc, values);
  ffi_call_plan_invoke(plan, FFI_FN(vsum), &rp, values);
  CHECK_DOUBLE_EQ(rc, rp);
  CHECK_DOUBLE_EQ(rp, expect);

  ffi_call_plan_free(plan);
}

int main (void)
{
  run (0);
  run (1);
  run (3);
  run (5);
  run (8);
  exit(0);
}
