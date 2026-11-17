/* Area:	ffi_call, closure_call
   Purpose:	Check integer complex types smaller than a register.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run } */

#include "ffitest.h"
#include <complex.h>

/* _Complex signed char and _Complex short are passed and returned in a
   single register or stack slot on most targets, which for complex values
   is a different code path from the larger types.  The last function
   passes both after eight ints, so that they go to the stack on targets
   with eight argument registers.  */

#define FFI_COMPLEX_TYPEDEF(name, type, ffitype)	     \
  static ffi_type *ffi_elements_complex_##name [2] = {	     \
    (ffi_type *)(&ffitype), NULL			     \
  };							     \
  struct struct_align_complex_##name {			     \
    char c;						     \
    _Complex type x;					     \
  };							     \
  static ffi_type ffi_type_complex_##name = {		     \
    sizeof(_Complex type),				     \
    offsetof(struct struct_align_complex_##name, x),	     \
    FFI_TYPE_COMPLEX,					     \
    (ffi_type **)ffi_elements_complex_##name		     \
  }

FFI_COMPLEX_TYPEDEF(schar, signed char, ffi_type_schar);
FFI_COMPLEX_TYPEDEF(sshort, short, ffi_type_sshort);

typedef _Complex signed char cc;
typedef _Complex short cs;

/* ffi_call requires its return storage to be at least register-sized and
   suitably aligned, even when the declared return type is smaller.  */
typedef union
{
  ffi_arg align;
  cc cchar;
  cs cshort;
} complex_result;

static cc
f_cc (cc a, int k)
{
  __real__ a = __real__ a * k;
  __imag__ a = __imag__ a - k;
  return a;
}

static cs
f_cs (cs a, int k)
{
  __real__ a = __real__ a * k;
  __imag__ a = __imag__ a - k;
  return a;
}

static cs
f_stack (int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8,
	 cc a, cs b)
{
  __real__ b = __real__ b + __real__ a + i1 + i2 + i3 + i4;
  __imag__ b = __imag__ b + __imag__ a + i5 + i6 + i7 + i8;
  return b;
}

static void
cls_cc (ffi_cif *cif __UNUSED__, void *resp, void **args,
	void *userdata __UNUSED__)
{
  *(cc *) resp = f_cc (*(cc *) args[0], *(int *) args[1]);
}

static void
cls_cs (ffi_cif *cif __UNUSED__, void *resp, void **args,
	void *userdata __UNUSED__)
{
  *(cs *) resp = f_cs (*(cs *) args[0], *(int *) args[1]);
}

static void
cls_stack (ffi_cif *cif __UNUSED__, void *resp, void **args,
	   void *userdata __UNUSED__)
{
  *(cs *) resp = f_stack (*(int *) args[0], *(int *) args[1],
			  *(int *) args[2], *(int *) args[3],
			  *(int *) args[4], *(int *) args[5],
			  *(int *) args[6], *(int *) args[7],
			  *(cc *) args[8], *(cs *) args[9]);
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *types[10];
  void *values[10];
  ffi_closure *pcl;
  void *code;
  int ints[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  cc a, ra;
  cs b, rb;
  complex_result result;
  int k = 3, i;

  pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (pcl != NULL);

  __real__ a = 5;
  __imag__ a = -7;
  __real__ b = 300;
  __imag__ b = -400;

  types[0] = &ffi_type_complex_schar;
  types[1] = &ffi_type_sint;
  values[0] = &a;
  values[1] = &k;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_complex_schar,
		       types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_cc), &result, values);
  ra = result.cchar;
  printf ("cc: %d,%di\n", (int) __real__ ra, (int) __imag__ ra);
  CHECK (__real__ ra == 15 && __imag__ ra == -10);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_cc, NULL, code) == FFI_OK);
  ra = ((cc (*)(cc, int)) code) (a, k);
  printf ("closure cc: %d,%di\n", (int) __real__ ra, (int) __imag__ ra);
  CHECK (__real__ ra == 15 && __imag__ ra == -10);

  types[0] = &ffi_type_complex_sshort;
  values[0] = &b;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_complex_sshort,
		       types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_cs), &result, values);
  rb = result.cshort;
  printf ("cs: %d,%di\n", (int) __real__ rb, (int) __imag__ rb);
  CHECK (__real__ rb == 900 && __imag__ rb == -403);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_cs, NULL, code) == FFI_OK);
  rb = ((cs (*)(cs, int)) code) (b, k);
  printf ("closure cs: %d,%di\n", (int) __real__ rb, (int) __imag__ rb);
  CHECK (__real__ rb == 900 && __imag__ rb == -403);

  for (i = 0; i < 8; i++)
    {
      types[i] = &ffi_type_sint;
      values[i] = &ints[i];
    }
  types[8] = &ffi_type_complex_schar;
  values[8] = &a;
  types[9] = &ffi_type_complex_sshort;
  values[9] = &b;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 10, &ffi_type_complex_sshort,
		       types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_stack), &result, values);
  rb = result.cshort;
  printf ("stack: %d,%di\n", (int) __real__ rb, (int) __imag__ rb);
  CHECK (__real__ rb == 315 && __imag__ rb == -381);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_stack, NULL, code) == FFI_OK);
  rb = ((cs (*)(int, int, int, int, int, int, int, int, cc, cs)) code)
    (1, 2, 3, 4, 5, 6, 7, 8, a, b);
  printf ("closure stack: %d,%di\n", (int) __real__ rb, (int) __imag__ rb);
  CHECK (__real__ rb == 315 && __imag__ rb == -381);

  ffi_closure_free (pcl);
  exit (0);
}
