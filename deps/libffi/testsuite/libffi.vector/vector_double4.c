/* Area:	ffi_call
   Purpose:	A 32-byte double4 vector.  On AArch64 a bare vector wider than
		16 bytes is passed by reference and returned in memory (no
		short-vector register class), so the call must round-trip.  On
		x86-64 wider-than-16-byte vectors are not implemented, so
		ffi_prep_cif must report FFI_BAD_TYPEDEF.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

typedef double d4 __attribute__((vector_size (32)));

/* Only called on ports that can actually marshal a 32-byte vector.  */
static d4 add_d4 (d4 a, d4 b) __UNUSED__;

static d4
add_d4 (d4 a, d4 b)
{
  return a + b;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type vec_type;
  ffi_type *vec_elems[5];
  ffi_type *args[2];

  make_vector_type (&vec_type, vec_elems, &ffi_type_double, 4);
  args[0] = &vec_type;
  args[1] = &vec_type;

#if defined(__aarch64__) || defined(_M_ARM64)
  {
    void *values[2];
    d4 a = { 1, 2, 3, 4 };
    d4 b = { 10, 20, 30, 40 };
    d4 r, ref;
    int i;

    values[0] = &a;
    values[1] = &b;

    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &vec_type, args) == FFI_OK);
    CHECK (vec_type.size == 32);
    CHECK (vec_type.alignment == 16);

    ffi_call (&cif, FFI_FN (add_d4), &r, values);

    ref = add_d4 (a, b);
    for (i = 0; i < 4; i++)
      {
	printf ("r[%d] = %g (want %g)\n", i, r[i], ref[i]);
	CHECK (r[i] == ref[i]);
      }
  }
#else
  {
    /* x86-64 (and any other opted-in port without >16B support): the >16-byte
       vector must be rejected, both as a return type and as an argument.  */
    ffi_status s_ret, s_arg;

    s_ret = ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 0, &vec_type, NULL);
    printf ("32-byte vector return: status %d (want %d = FFI_BAD_TYPEDEF)\n",
	    s_ret, FFI_BAD_TYPEDEF);
    CHECK (s_ret == FFI_BAD_TYPEDEF);

    s_arg = ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args);
    printf ("32-byte vector argument: status %d (want %d = FFI_BAD_TYPEDEF)\n",
	    s_arg, FFI_BAD_TYPEDEF);
    CHECK (s_arg == FFI_BAD_TYPEDEF);
  }
#endif

  exit (0);
}
