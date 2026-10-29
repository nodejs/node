/* Area:	ffi_call
   Purpose:	Test longjmp over ffi_call frames */

/* Test code adapted from Lars Kanis' bug report:
   https://github.com/libffi/libffi/issues/905 */

/* { dg-do run } */

#include "ffitest.h"
#include "ffi_common.h"

#include <setjmp.h>

static jmp_buf buf;

static void ABI_ATTR lev2(const char *str) {
  printf("lev2 %s\n", str);
  // jumps back to where setjmp was called - making setjmp now return 1
  longjmp(buf, 1);
}

static void ABI_ATTR lev1(const char *str) {
  lev2(str);

  // will not be reached
  printf("lev1 %s\n", str);
}

int main()
{
  ffi_cif cif;
  ffi_type *args[1];
  void *values[1];
  char *s;
  ffi_arg rc;
  /* Initialize the argument info vectors */
  args[0] = &ffi_type_pointer;
  values[0] = &s;
  /* Initialize the cif */
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
    &ffi_type_sint, args) == FFI_OK)
  {
    s = "direct call";
    if (!setjmp(buf)){
      // works on x64 and arm64
      lev1(s);
    } else {
      printf("back to main\n");
    }

    s = "through libffi";
    if (!setjmp(buf)){
      // works on x64 but segfaults on arm64
      ffi_call(&cif, (void (*)(void))lev1, &rc, values);
    } else {
      printf("back to main\n");
    }
  }
  return 0;
}
