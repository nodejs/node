//===-- Definition of macros to be used with complex functions ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __LLVM_LIBC_MACROS_COMPLEX_MACROS_H
#define __LLVM_LIBC_MACROS_COMPLEX_MACROS_H

#include "cfloat128-macros.h"
#include "cfloat16-macros.h"

#ifndef __STDC_NO_COMPLEX__

#define __STDC_VERSION_COMPLEX_H__ 202311L

#define complex _Complex
#define _Complex_I ((_Complex float)1.0fi)
#define I _Complex_I

// TODO: Add imaginary macros once GCC or Clang support _Imaginary builtin-type.

#if __has_builtin(__builtin_complex)
#define __CMPLX(r, i, t) (__builtin_complex((t)(r), (t)(i)))
#else
#define __CMPLX(r, i, t) ((_Complex t){(t)(r), (t)(i)})
#endif

#define CMPLX(r, i) __CMPLX(r, i, double)
#define CMPLXF(r, i) __CMPLX(r, i, float)
#define CMPLXL(r, i) __CMPLX(r, i, long double)

#ifdef LIBC_TYPES_HAS_CFLOAT16
#if !defined(__clang__) || (__clang_major__ >= 22 && __clang_minor__ > 0)
#define CMPLXF16(r, i) __CMPLX(r, i, _Float16)
#else
#define CMPLXF16(r, i) ((complex _Float16)(__CMPLX(r, i, float)))
#endif
#endif // LIBC_TYPES_HAS_CFLOAT16

#ifdef LIBC_TYPES_HAS_CFLOAT128
#ifdef LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE
#define CMPLXF128(r, i) __CMPLX(r, i, long double)
#else
#define CMPLXF128(r, i) __CMPLX(r, i, float128)
#endif // LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE
#endif // LIBC_TYPES_HAS_CFLOAT128

#endif // __STDC_NO_COMPLEX__

#endif // __LLVM_LIBC_MACROS_COMPLEX_MACROS_H
