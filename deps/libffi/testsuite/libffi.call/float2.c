/* Area:	ffi_call
   Purpose:	Check return value long double.
   Limitations:	none.
   PR:		none.
   Originator:	From the original ffitest.c  */

/* { dg-excess-errors "fails" { target x86_64-*-mingw* x86_64-*-cygwin* } } */
/* { dg-do run { xfail x86_64-*-mingw* x86_64-*-cygwin* } } */

#include "ffitest.h"
#include "float.h"

static long double ldblit(float f)
{
  return (long double) (((long double) f)/ (long double) 3.0);
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  float f;
  long double ld;

  args[0] = &ffi_type_float;
  values[0] = &f;

  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		     &ffi_type_longdouble, args) == FFI_OK);

  f = 3.14159;

#if 1
  /* This is ifdef'd out for now. long double support under SunOS/gcc
     is pretty much non-existent.  You'll get the odd bus error in library
     routines like printf().  */
  printf ("%Lf\n", ldblit(f));
#endif
  ld = 666;
  ffi_call(&cif, FFI_FN(ldblit), &ld, values);

#if 1
  /* This is ifdef'd out for now. long double support under SunOS/gcc
     is pretty much non-existent.  You'll get the odd bus error in library
     routines like printf().  */
  printf ("%Lf, %Lf, %Lf, %Lf\n", ld, ldblit(f), ld - ldblit(f), LDBL_EPSILON);
#endif

  /* These are not always the same!! Check for a reasonable delta */
  if (ld - ldblit(f) < LDBL_EPSILON)
    puts("long double return value tests ok!");
  else
    CHECK(0);

  exit(0);
}
