/* -----------------------------------------------------------------*-C-*-
   ffitarget.h - Copyright (c) 2012, 2026  Anthony Green
                 Copyright (c) 1996-2003  Red Hat, Inc.
   Target configuration macros for hppa.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   ----------------------------------------------------------------------- */

#ifndef LIBFFI_TARGET_H
#define LIBFFI_TARGET_H

#ifndef LIBFFI_H
#error "Please do not include ffitarget.h directly into your source.  Use ffi.h instead."
#endif

/* ---- System specific configurations ----------------------------------- */

#ifndef LIBFFI_ASM
typedef unsigned long          ffi_arg;
typedef signed long            ffi_sarg;

typedef enum ffi_abi {
  FFI_FIRST_ABI = 0,

#ifdef PA_LINUX
  FFI_PA32,
  FFI_LAST_ABI,
  FFI_DEFAULT_ABI = FFI_PA32
#endif

#ifdef PA_HPUX
  FFI_PA32,
  FFI_LAST_ABI,
  FFI_DEFAULT_ABI = FFI_PA32
#endif

#ifdef PA64_HPUX
  FFI_PA64,
  FFI_LAST_ABI,
  FFI_DEFAULT_ABI = FFI_PA64
#endif
} ffi_abi;
#endif

#define FFI_TARGET_SPECIFIC_STACK_SPACE_ALLOCATION

/* ---- Definitions for closures ----------------------------------------- */

#define FFI_CLOSURES 1
#define FFI_NATIVE_RAW_API 0
#if defined(PA64_HPUX)
#define FFI_TRAMPOLINE_SIZE 32
#else
#define FFI_TRAMPOLINE_SIZE 12
#endif

#define FFI_TYPE_SMALL_STRUCT1 -1
#define FFI_TYPE_SMALL_STRUCT2 -2
#define FFI_TYPE_SMALL_STRUCT3 -3
#define FFI_TYPE_SMALL_STRUCT4 -4
#define FFI_TYPE_SMALL_STRUCT5 -5
#define FFI_TYPE_SMALL_STRUCT6 -6
#define FFI_TYPE_SMALL_STRUCT7 -7
#define FFI_TYPE_SMALL_STRUCT8 -8

/* The return-value jump tables in linux.S and hpux32.S are indexed by
   cif->flags, which ffi_prep_cif_machdep derives from the return type.  Any
   return type it does not handle explicitly -- including FFI_TYPE_COMPLEX and
   the 128-bit integer types FFI_TYPE_UINT128/FFI_TYPE_SINT128 -- falls through
   to the default case and is mapped to FFI_TYPE_INT, so cif->flags never
   exceeds FFI_TYPE_COMPLEX and the existing tables remain sufficient.  Bump
   FFI_PA_TYPE_LAST to the current FFI_TYPE_LAST once you have confirmed any
   newly added generic type is likewise handled (or the tables extended).  */
#define FFI_PA_TYPE_LAST FFI_TYPE_SINT128

/* Tripwire: when a new generic type is added FFI_TYPE_LAST changes and this
   fires, forcing a review of ffi_prep_cif_machdep and the linux.S / hpux32.S
   jump tables before FFI_PA_TYPE_LAST above is bumped.  */
#if FFI_TYPE_LAST != FFI_PA_TYPE_LAST
# error "You likely have broken jump tables"
#endif

#endif
