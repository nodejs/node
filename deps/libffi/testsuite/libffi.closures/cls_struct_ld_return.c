/* Area:	ffi_call, closure_call
   Purpose:	Check the return value of a struct with one long double field.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run { xfail mips64*-*-* x86_64-*-linux-android* } } */
/* XFAIL: mips N64 mishandles long-double struct returns (libffi issue
   #1071).  x86_64 Android mishandles the long-double struct return where
   x86_64-linux-gnu does not (libffi issue #1076).  */
/* { dg-options -mlong-double-128 { target ppc64_longdouble128 } } */

#include "ffitest.h"

/* x86-64 returns this struct in %st(0), like a plain long double.  */

typedef struct { long double a; } s_ld;

static s_ld ABI_ATTR
fn_s_ld (int k)
{
  s_ld r = { 1.5L * k };
  return r;
}

static void
cls_s_ld (ffi_cif *cif __UNUSED__, void *resp, void **argv,
	  void *userdata __UNUSED__)
{
  *(s_ld *) resp = fn_s_ld (*(int *) argv[0]);
}

int
main (void)
{
  ffi_type *elements[2] = { &ffi_type_longdouble, NULL };
  ffi_type type;
  ffi_type *args[1] = { &ffi_type_sint };
  int k = 3;
  void *values[1] = { &k };
  ffi_cif cif;
  ffi_closure *pcl;
  void *code;
  s_ld got;

  type.size = 0;
  type.alignment = 0;
  type.type = FFI_TYPE_STRUCT;
  type.elements = elements;
  CHECK (ffi_prep_cif (&cif, ABI_NUM, 1, &type, args) == FFI_OK);

  memset (&got, 0, sizeof (got));
  ffi_call (&cif, FFI_FN (fn_s_ld), &got, values);
  printf ("ffi_call: %Lg\n", got.a);
  CHECK (got.a == 4.5L);

  pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (pcl != NULL);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_s_ld, NULL, code) == FFI_OK);
  memset (&got, 0, sizeof (got));
  got = ((s_ld (ABI_ATTR *)(int)) code) (k + 1);
  printf ("closure: %Lg\n", got.a);
  CHECK (got.a == 6.0L);
  ffi_closure_free (pcl);

  exit (0);
}
