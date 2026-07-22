/* Area:	ffi_call
   Purpose:	Check structures with array and callback.
   Limitations:	none.
   PR:		none.
   Originator:	David Tenty <daltenty@ibm.com>  */

/* { dg-do run } */
#include "ffitest.h"

static int i=5;

static void callback(void) { i++; }

typedef struct
{
  unsigned char c1;
  double s[2];
  unsigned char c2;
} test_structure_12;

static test_structure_12 ABI_ATTR struct12 (test_structure_12 ts, void (*func)(void))
{
  ts.c1 += 1;
  ts.c2 += 1;
  ts.s[0] += 1;
  ts.s[1] += 1;

  func();
  return ts;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_type ts12_type,ts12a_type;
  ffi_type *ts12_type_elements[4];
  ffi_type *ts12a_type_elements[3];

  test_structure_12 ts12_arg;
  void (*ptr)(void)=&callback;

  test_structure_12 *ts12_result =
    (test_structure_12 *) malloc (sizeof(test_structure_12));

  ts12a_type.size = 0;
  ts12a_type.alignment = 0;
  ts12a_type.type = FFI_TYPE_STRUCT;
  ts12a_type.elements = ts12a_type_elements;
  ts12a_type_elements[0] = &ffi_type_double;
  ts12a_type_elements[1] = &ffi_type_double;
  ts12a_type_elements[2] = NULL;

  ts12_type.size = 0;
  ts12_type.alignment = 0;
  ts12_type.type = FFI_TYPE_STRUCT;
  ts12_type.elements = ts12_type_elements;
  ts12_type_elements[0] = &ffi_type_uchar;
  ts12_type_elements[1] = &ts12a_type;
  ts12_type_elements[2] = &ffi_type_uchar;
  ts12_type_elements[3] = NULL;


  args[0] = &ts12_type;
  args[1] = &ffi_type_pointer;
  values[0] = &ts12_arg;
  values[1] = &ptr;

  CHECK(ffi_prep_cif(&cif, ABI_NUM, 2, &ts12_type, args) == FFI_OK);

  ts12_arg.c1 = 5;
  ts12_arg.c2 = 6;
  ts12_arg.s[0] = 7.77;
  ts12_arg.s[1] = 8.88;

  printf ("%u\n", ts12_arg.c1);
  printf ("%u\n", ts12_arg.c2);
  printf ("%g\n", ts12_arg.s[0]);
  printf ("%g\n", ts12_arg.s[1]);
  printf ("%d\n", i);

  ffi_call(&cif, FFI_FN(struct12), ts12_result, values);

  printf ("%u\n", ts12_result->c1);
  printf ("%u\n", ts12_result->c2);
  printf ("%g\n", ts12_result->s[0]);
  printf ("%g\n", ts12_result->s[1]);
  printf ("%d\n", i);
  CHECK(ts12_result->c1 == 5 + 1);
  CHECK(ts12_result->c2 == 6 + 1);
  CHECK(ts12_result->s[0] == 7.77 + 1);
  CHECK(ts12_result->s[1] == 8.88 + 1);
  CHECK(i == 5 + 1);
  CHECK(ts12_type.size == sizeof(test_structure_12));

  free (ts12_result);
  exit(0);
}
