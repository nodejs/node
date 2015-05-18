#include <stdio.h>
#include <ffi/ffi.h>

int main()
{
  ffi_cif cif;
  ffi_type *args[1];
  void *values[1];
  char *s;
  int rc;

  /* Initialize the argument info vectors */
  args[0] = &ffi_type_pointer;
  values[0] = &s;

  /* Initialize the cif */
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
        &ffi_type_uint, args) == FFI_OK)
  {
    s = "Hello World!";
    ffi_call(&cif, puts, &rc, values);
    /* rc now holds the result of the call to puts */

    /* values holds a pointer to the function's arg, so to
       call puts() again all we need to do is change the
       value of s */
    s = "This is cool!";
    ffi_call(&cif, puts, &rc, values);
  }

  return 0;
}
