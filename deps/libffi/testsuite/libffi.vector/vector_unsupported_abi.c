/* Area:	ffi_prep_cif
   Purpose:	On x86-64 System V, reject vector types whose calling
		convention is undefined (so GCC and Clang disagree) with
		FFI_BAD_TYPEDEF, while still accepting the shapes both
		compilers agree on.  Rejecting <1 x double> closes a
		type-confusion that turned a caller's integer argument into
		the store address of the return value (GHSA-5pc3-hr6f-q3mm).
   Limitations:	The classification is x86-64 System V specific, so the
		assertions only run there; the test passes trivially on
		other targets and ABIs.
   PR:		GHSA-5pc3-hr6f-q3mm.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

#if defined(__x86_64__) && !defined(_WIN64) && !defined(__CYGWIN__)

/* Expect ffi_prep_cif for RET f(ARG) to return STATUS.  Either type may be
   NULL to mean "no argument" / void return as appropriate.  */
static void
check_cif (ffi_type *ret, ffi_type *arg, ffi_status expected)
{
  ffi_cif cif;
  ffi_type *args[1];
  unsigned nargs = 0;

  if (arg != NULL)
    {
      args[0] = arg;
      nargs = 1;
    }
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, nargs,
		       ret ? ret : &ffi_type_void, args) == expected);
}

int
main (void)
{
  ffi_type vt, vt2;
  ffi_type *elems[17], *elems2[17];
  ffi_type *selems[2];
  ffi_type st;

  /* --- Shapes with an undefined ABI: must be rejected. --- */

  /* <1 x double> (8 bytes): GCC returns/passes it in memory, Clang in %xmm0.
     This is the one that produced the out-of-bounds write. */
  make_vector_type (&vt, elems, &ffi_type_double, 1);
  check_cif (&vt, NULL, FFI_BAD_TYPEDEF);   /* as return value */
  check_cif (NULL, &vt, FFI_BAD_TYPEDEF);   /* as argument     */

  /* <1 x float> (4 bytes): GCC memory, Clang INTEGER (%eax). */
  make_vector_type (&vt, elems, &ffi_type_float, 1);
  check_cif (NULL, &vt, FFI_BAD_TYPEDEF);

  /* <4 x uint8> (4 bytes): sub-8-byte, unspecified. */
  make_vector_type (&vt, elems, &ffi_type_uint8, 4);
  check_cif (NULL, &vt, FFI_BAD_TYPEDEF);

  /* A struct that contains <1 x double> must be rejected recursively. */
  make_vector_type (&vt, elems, &ffi_type_double, 1);
  selems[0] = &vt;
  selems[1] = NULL;
  st.size = 0;
  st.alignment = 0;
  st.type = FFI_TYPE_STRUCT;
  st.elements = selems;
  check_cif (&st, NULL, FFI_BAD_TYPEDEF);
  check_cif (NULL, &st, FFI_BAD_TYPEDEF);

  /* --- Shapes both compilers agree on: must still be accepted. --- */

  /* <2 x float> (8 bytes, not <1 x double>) -> SSE on GCC and Clang. */
  make_vector_type (&vt, elems, &ffi_type_float, 2);
  check_cif (&vt, NULL, FFI_OK);
  check_cif (NULL, &vt, FFI_OK);

  /* <8 x uint8> (8 bytes) -> SSE on both. */
  make_vector_type (&vt, elems, &ffi_type_uint8, 8);
  check_cif (NULL, &vt, FFI_OK);

  /* <4 x float> (16 bytes) -> SSE + SSEUP on both. */
  make_vector_type (&vt2, elems2, &ffi_type_float, 4);
  check_cif (&vt2, NULL, FFI_OK);
  check_cif (NULL, &vt2, FFI_OK);

  /* <2 x double> (16 bytes) -> SSE + SSEUP on both. */
  make_vector_type (&vt2, elems2, &ffi_type_double, 2);
  check_cif (&vt2, NULL, FFI_OK);

  printf ("unsupported vector abi ok\n");
  exit (0);
}

#else

int
main (void)
{
  exit (0);
}

#endif /* x86-64 System V */
