/* Area:	ffi_call
   Purpose:	Check stdcall many call on X86_WIN32 systems.
   Limitations:	none.
   PR:		none.
   Originator:	From the original ffitest.c  */

/* { dg-do run { target i?86-*-cygwin* i?86-*-mingw* } } */

#include "ffitest.h"
#include <float.h>

static float __attribute__((fastcall)) fastcall_many(float f1,
						   float f2,
						   float f3,
						   float f4,
						   float f5,
						   float f6,
						   float f7,
						   float f8,
						   float f9,
						   float f10,
						   float f11,
						   float f12,
						   float f13)
{
  return ((f1/f2+f3/f4+f5/f6+f7/f8+f9/f10+f11/f12) * f13);
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[13];
  void *values[13];
  float fa[13];
  float f, ff;
  unsigned long ul;

  for (ul = 0; ul < 13; ul++)
    {
      args[ul] = &ffi_type_float;
      values[ul] = &fa[ul];
	fa[ul] = (float) ul;
    }

  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, FFI_FASTCALL, 13,
		     &ffi_type_float, args) == FFI_OK);

  ff =  fastcall_many(fa[0], fa[1],
		     fa[2], fa[3],
		     fa[4], fa[5],
		     fa[6], fa[7],
		     fa[8], fa[9],
		     fa[10], fa[11], fa[12]);

  ffi_call(&cif, FFI_FN(fastcall_many), &f, values);

  if (f - ff < FLT_EPSILON)
    printf("fastcall many arg tests ok!\n");
  else
    CHECK(0);
  exit(0);
}
