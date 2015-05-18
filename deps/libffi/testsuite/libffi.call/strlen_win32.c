/* Area:	ffi_call
   Purpose:	Check stdcall strlen call on X86_WIN32 systems.
   Limitations:	none.
   PR:		none.
   Originator:	From the original ffitest.c  */

/* { dg-do run { target i?86-*-cygwin* i?86-*-mingw* } } */

#include "ffitest.h"

static size_t __attribute__((stdcall)) my_stdcall_strlen(char *s)
{
  return (strlen(s));
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_arg rint;
  char *s;
  args[0] = &ffi_type_pointer;
  values[0] = (void*) &s;
  
  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, FFI_STDCALL, 1,
		       &ffi_type_sint, args) == FFI_OK);
  
  s = "a";
  ffi_call(&cif, FFI_FN(my_stdcall_strlen), &rint, values);
  CHECK(rint == 1);
  
  s = "1234567";
  ffi_call(&cif, FFI_FN(my_stdcall_strlen), &rint, values);
  CHECK(rint == 7);
  
  s = "1234567890123456789012345";
  ffi_call(&cif, FFI_FN(my_stdcall_strlen), &rint, values);
  CHECK(rint == 25);
  
  printf("stdcall strlen tests passed\n");
  exit(0);
}
