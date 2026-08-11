/* Area:	ffi_call
   Purpose:	Pass many large by-value structs on AArch64.
   Limitations:	none.
   PR:		none.
   Originator:	AArch64 large-struct stack accounting regression.

   Regression test: on AArch64, composites larger than 16 bytes are passed
   by invisible reference.  ffi_call copies each payload into the argument
   slab (growing down from the top) and, once X0-X7 are exhausted, also
   spills the by-ref pointer into the same slab (the NSAA, growing up).
   The generic prep_cif budget in cif->bytes only charged the payload copy,
   not the 8-byte pointer slot, so with enough large structs the two regions
   collided and a later payload copy overwrote an already-spilled pointer,
   leaving the callee with a corrupt pointer for a by-value argument.
   Passing sixteen 32-byte (non-HFA) structs by value -- eight more than the
   argument registers -- must marshal every argument intact.  */

/* { dg-do run } */
#include "ffitest.h"

#define NARGS 16
#define SSIZE 32

typedef struct { unsigned char b[SSIZE]; } big_struct;

/* Sum every byte of every argument.  A corrupted by-ref pointer makes the
   callee read the wrong memory, so the sum no longer matches.  */
static int ABI_ATTR
sum_bytes (big_struct s0, big_struct s1, big_struct s2, big_struct s3,
	   big_struct s4, big_struct s5, big_struct s6, big_struct s7,
	   big_struct s8, big_struct s9, big_struct s10, big_struct s11,
	   big_struct s12, big_struct s13, big_struct s14, big_struct s15)
{
  big_struct *all[NARGS];
  int i, j, sum = 0;

  all[0] = &s0;   all[1] = &s1;   all[2] = &s2;   all[3] = &s3;
  all[4] = &s4;   all[5] = &s5;   all[6] = &s6;   all[7] = &s7;
  all[8] = &s8;   all[9] = &s9;   all[10] = &s10; all[11] = &s11;
  all[12] = &s12; all[13] = &s13; all[14] = &s14; all[15] = &s15;

  for (i = 0; i < NARGS; i++)
    for (j = 0; j < SSIZE; j++)
      sum += all[i]->b[j];

  return sum;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[NARGS];
  void *values[NARGS];
  ffi_type bs_type;
  ffi_type *bs_elements[SSIZE + 1];
  big_struct in[NARGS];
  ffi_arg result = 0;
  int i, j, expected = 0;

  bs_type.size = 0;
  bs_type.alignment = 0;
  bs_type.type = FFI_TYPE_STRUCT;
  for (i = 0; i < SSIZE; i++)
    bs_elements[i] = &ffi_type_uchar;
  bs_elements[SSIZE] = NULL;
  bs_type.elements = bs_elements;

  /* Fill struct i with the distinct byte value (i + 1) so any pointer
     mix-up between arguments changes the total.  */
  for (i = 0; i < NARGS; i++)
    {
      for (j = 0; j < SSIZE; j++)
	{
	  in[i].b[j] = (unsigned char) (i + 1);
	  expected += (i + 1);
	}
      args[i] = &bs_type;
      values[i] = &in[i];
    }

  CHECK(ffi_prep_cif(&cif, ABI_NUM, NARGS, &ffi_type_sint, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(sum_bytes), &result, values);

  CHECK((int) result == expected);

  exit(0);
}
