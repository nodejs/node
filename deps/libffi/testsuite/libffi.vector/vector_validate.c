/* Area:	ffi_prep_cif
   Purpose:	Validate that malformed vector type descriptors are rejected
		with FFI_BAD_TYPEDEF, and that a well-formed vector is accepted
		with the computed power-of-two size and min(size,16) alignment.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

int
main (void)
{
  ffi_cif cif;

  /* Heterogeneous lanes (float mixed with double) -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    elems[0] = &ffi_type_float;
    elems[1] = &ffi_type_double;
    elems[2] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* An empty (zero-lane) vector -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type vt;
    ffi_type *elems[1];
    ffi_type *args[1];
    elems[0] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* A non-scalar (struct) lane type -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type inner;
    ffi_type *inner_elems[2];
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    inner_elems[0] = &ffi_type_float;
    inner_elems[1] = NULL;
    inner.size = 0;
    inner.alignment = 0;
    inner.type = FFI_TYPE_STRUCT;
    inner.elements = inner_elems;
    elems[0] = &inner;
    elems[1] = &inner;
    elems[2] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* A well-formed 3 x float vector is accepted with computed layout.  */
  {
    ffi_type vt;
    ffi_type *elems[4];
    ffi_type *args[1];
    make_vector_type (&vt, elems, &ffi_type_float, 3);
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_OK);
    CHECK (vt.size == 16);         /* 12 rounded up to 16 */
    CHECK (vt.alignment == 16);    /* min(16, 16) */
  }

  /* An 8-byte vector gets alignment 8 = min(8, 16).  */
  {
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    make_vector_type (&vt, elems, &ffi_type_float, 2);
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_OK);
    CHECK (vt.size == 8);
    CHECK (vt.alignment == 8);
  }

  printf ("vector validation ok\n");
  exit (0);
}
