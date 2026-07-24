/* Area:	ffi_call
   Purpose:	Pass many small (8-byte) structs by value.
   Limitations:	none.
   PR:		none.
   Originator:	secscan regression (PA-RISC 32-bit slot under-count).

   Regression test: on PA-RISC 32-bit (FFI_PA32), ffi_size_stack_pa32 reserves
   one stack slot per struct (pa/ffi.c: "z += 1"), but ffi_prep_args_pa32
   consumes two slots for a 5-8 byte struct passed inline.  ffi_call_pa32 sets
   the argument base to sp + cif->bytes and ffi_prep_args_pa32 writes each
   argument at (base - slot*4), so once the marshaller's slot count exceeds
   cif->bytes/4 the writes spill below sp -- first into the 64-byte register
   save area, then over ffi_call_pa32's own saved return pointer.

   A few small structs only overflow into harmless scratch (the call still
   returns correctly), so this uses enough 8-byte structs that the overflow
   reaches the saved return pointer and corrupts the return path.  On a correct
   backend all arguments marshal within the allocated frame and the call simply
   returns the expected sums; the test passes everywhere except an unfixed
   FFI_PA32.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct { int a; int b; } small_struct;

#define NARGS 40

static small_struct ABI_ATTR
many_small (small_struct s0,  small_struct s1,  small_struct s2,  small_struct s3,
	    small_struct s4,  small_struct s5,  small_struct s6,  small_struct s7,
	    small_struct s8,  small_struct s9,  small_struct s10, small_struct s11,
	    small_struct s12, small_struct s13, small_struct s14, small_struct s15,
	    small_struct s16, small_struct s17, small_struct s18, small_struct s19,
	    small_struct s20, small_struct s21, small_struct s22, small_struct s23,
	    small_struct s24, small_struct s25, small_struct s26, small_struct s27,
	    small_struct s28, small_struct s29, small_struct s30, small_struct s31,
	    small_struct s32, small_struct s33, small_struct s34, small_struct s35,
	    small_struct s36, small_struct s37, small_struct s38, small_struct s39)
{
  small_struct r;
  r.a = s0.a  + s1.a  + s2.a  + s3.a  + s4.a  + s5.a  + s6.a  + s7.a
      + s8.a  + s9.a  + s10.a + s11.a + s12.a + s13.a + s14.a + s15.a
      + s16.a + s17.a + s18.a + s19.a + s20.a + s21.a + s22.a + s23.a
      + s24.a + s25.a + s26.a + s27.a + s28.a + s29.a + s30.a + s31.a
      + s32.a + s33.a + s34.a + s35.a + s36.a + s37.a + s38.a + s39.a;
  r.b = s0.b  + s1.b  + s2.b  + s3.b  + s4.b  + s5.b  + s6.b  + s7.b
      + s8.b  + s9.b  + s10.b + s11.b + s12.b + s13.b + s14.b + s15.b
      + s16.b + s17.b + s18.b + s19.b + s20.b + s21.b + s22.b + s23.b
      + s24.b + s25.b + s26.b + s27.b + s28.b + s29.b + s30.b + s31.b
      + s32.b + s33.b + s34.b + s35.b + s36.b + s37.b + s38.b + s39.b;
  return r;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[NARGS];
  void *values[NARGS];
  ffi_type ss_type;
  ffi_type *ss_elements[3];
  small_struct in[NARGS];
  small_struct result = { 0, 0 };
  int i, expected_a = 0, expected_b = 0;

  ss_type.size = 0;
  ss_type.alignment = 0;
  ss_type.type = FFI_TYPE_STRUCT;
  ss_type.elements = ss_elements;
  ss_elements[0] = &ffi_type_sint;
  ss_elements[1] = &ffi_type_sint;
  ss_elements[2] = NULL;

  for (i = 0; i < NARGS; i++)
    {
      in[i].a = i + 1;
      in[i].b = -(i + 1);
      expected_a += in[i].a;
      expected_b += in[i].b;
      args[i] = &ss_type;
      values[i] = &in[i];
    }

  CHECK(ffi_prep_cif(&cif, ABI_NUM, NARGS, &ss_type, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(many_small), &result, values);

  CHECK(result.a == expected_a);
  CHECK(result.b == expected_b);

  exit(0);
}
