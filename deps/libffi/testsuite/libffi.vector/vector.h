/* -*-c-*- */
/* Shared helpers for the libffi vector (SIMD) tests.

   Vectors are built with the portable GCC/Clang spelling
   __attribute__((vector_size (N))) so the tests compile on both compilers.
   A vector ffi_type is described exactly like a struct, except every element
   points at the SAME scalar ffi_type and the count is the lane count; the
   caller leaves size and alignment at zero and libffi computes them.  */

#ifndef LIBFFI_VECTOR_H
#define LIBFFI_VECTOR_H

#include "ffitest.h"

/* Build (into the caller-provided ELEMS array of length COUNT + 1 and the
   ffi_type object TY) a vector type descriptor of COUNT lanes of scalar type
   ELEM.  ELEMS must have room for COUNT + 1 pointers (NULL terminator).  */
static inline void
make_vector_type (ffi_type *ty, ffi_type **elems, ffi_type *elem,
		  unsigned count)
{
  unsigned i;
  for (i = 0; i < count; i++)
    elems[i] = elem;
  elems[count] = NULL;
  ty->size = 0;
  ty->alignment = 0;
  ty->type = FFI_TYPE_VECTOR;
  ty->elements = elems;
}

#endif /* LIBFFI_VECTOR_H */
