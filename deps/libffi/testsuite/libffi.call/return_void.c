/* Area:        ffi_call
   Purpose:     Check that a void call ignores its return buffer.
   Limitations: none.  */

/* { dg-do run } */
#include "ffitest.h"

static unsigned int called;

static void
return_void (unsigned int value)
{
  called = value;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *args[1] = { &ffi_type_uint };
  unsigned int value = 42;
  void *values[1] = { &value };
  ffi_arg result = ~(ffi_arg)0;

  CHECK(ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1,
                     &ffi_type_void, args) == FFI_OK);

  ffi_call (&cif, FFI_FN(return_void), &result, values);
  CHECK(called == value);
  CHECK(result == ~(ffi_arg)0);

  value = 43;
  ffi_call (&cif, FFI_FN(return_void), NULL, values);
  CHECK(called == value);

  exit (0);
}
