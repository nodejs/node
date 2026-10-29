/* Area:	ffi_call
   Purpose:	Pass a large struct by value (more words than arg registers).
   Limitations:	none.
   PR:		none.
   Originator:	secscan regression (ARCompact used_stack overflow).

   Regression test: on ARCompact (ARC 32-bit), ffi_call_int sized the stack
   argument area at two words per argument, but a by-value struct is marshalled
   one word ("atom") at a time, and words beyond the argument registers were
   written to that under-sized area without bound.  Passing a 64-byte struct by
   value (sixteen 32-bit words, eight more than the eight argument registers)
   must marshal correctly and return the expected sum.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct { int v[8]; } big_struct;

static int ABI_ATTR
sum_big (big_struct s)
{
  int i, sum = 0;
  for (i = 0; i < 8; i++)
    sum += s.v[i];
  return sum;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[1];
  void *values[1];
  ffi_type bs_type;
  ffi_type *bs_elements[9];
  big_struct in;
  ffi_arg result = 0;
  int i, expected = 0;

  bs_type.size = 0;
  bs_type.alignment = 0;
  bs_type.type = FFI_TYPE_STRUCT;
  for (i = 0; i < 8; i++)
    bs_elements[i] = &ffi_type_sint;
  bs_elements[8] = NULL;
  bs_type.elements = bs_elements;

  for (i = 0; i < 8; i++)
    {
      in.v[i] = 0x1111 * (i + 1);
      expected += in.v[i];
    }

  args[0] = &bs_type;
  values[0] = &in;

  CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ffi_type_sint, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(sum_big), &result, values);

  CHECK((int) result == expected);

  exit(0);
}
