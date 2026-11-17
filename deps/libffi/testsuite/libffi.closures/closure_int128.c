/* Area:	ffi_closure
   Purpose:	Check that a closure receives a 128-bit integer argument, and
		the argument after it, correctly.  On s390x the closure path
		had no SINT128/UINT128 case, so it left the argument's avalue[]
		slot uninitialised and shifted every later argument
		(GHSA-wf8m-qr3x-cch4).
   Limitations:	none.
   PR:		GHSA-wf8m-qr3x-cch4.  */

/* { dg-do run } */
#include "ffitest.h"

#if defined(FFI_TARGET_HAS_INT128) && defined(__SIZEOF_INT128__)

typedef __int128_t i128;

static const i128 VAL =
  ((i128)0x0102030405060708ull << 64) | 0x090a0b0c0d0e0f10ull;
#define TAIL 0x5a5a5a5aL

/* long f(i128 x, long b) => low64(x) + b  */
static void h_xb (ffi_cif *cif, void *ret, void **args, void *ud)
{
  i128 x = *(i128 *) args[0];
  long b = *(long *) args[1];
  (void) cif; (void) ud;
  *(ffi_arg *) ret = (ffi_arg) ((long) x + b);
}

/* long g(long a, i128 x, long b) => a + low64(x) + b  */
static void h_axb (ffi_cif *cif, void *ret, void **args, void *ud)
{
  long a = *(long *) args[0];
  i128 x = *(i128 *) args[1];
  long b = *(long *) args[2];
  (void) cif; (void) ud;
  *(ffi_arg *) ret = (ffi_arg) (a + (long) x + b);
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *cl;

  {
    ffi_type *args[2] = { &ffi_type_sint128, &ffi_type_sint64 };
    long (*f) (i128, long);
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint64, args)
	   == FFI_OK);
    cl = ffi_closure_alloc (sizeof (ffi_closure), &code);
    CHECK (cl != NULL);
    CHECK (ffi_prep_closure_loc (cl, &cif, h_xb, NULL, code) == FFI_OK);
    f = (long (*) (i128, long)) code;
    CHECK (f (VAL, TAIL) == (long) ((long) VAL + TAIL));
    ffi_closure_free (cl);
  }

  {
    ffi_type *args[3] =
      { &ffi_type_sint64, &ffi_type_sint128, &ffi_type_sint64 };
    long (*g) (long, i128, long);
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint64, args)
	   == FFI_OK);
    cl = ffi_closure_alloc (sizeof (ffi_closure), &code);
    CHECK (cl != NULL);
    CHECK (ffi_prep_closure_loc (cl, &cif, h_axb, NULL, code) == FFI_OK);
    g = (long (*) (long, i128, long)) code;
    CHECK (g (11, VAL, TAIL) == (long) (11 + (long) VAL + TAIL));
    ffi_closure_free (cl);
  }

  printf ("closure int128 ok\n");
  exit (0);
}

#else
int main (void) { return 0; }
#endif
