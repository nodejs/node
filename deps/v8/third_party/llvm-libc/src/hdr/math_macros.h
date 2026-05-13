//===-- Definition of macros from math.h ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_MATH_MACROS_H
#define LLVM_LIBC_HDR_MATH_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/math-macros.h"

#else // Overlay mode

// GCC will include CXX headers when __cplusplus is defined. This behavior
// can be suppressed by defining _GLIBCXX_INCLUDE_NEXT_C_HEADERS.
#if defined(__GNUC__) && !defined(__clang__)
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS
#endif
#include <math.h>

// Some older math.h header does not have FP_INT_* constants yet.
#ifndef FP_INT_UPWARD
#define FP_INT_UPWARD 0
#endif // FP_INT_UPWARD

#ifndef FP_INT_DOWNWARD
#define FP_INT_DOWNWARD 1
#endif // FP_INT_DOWNWARD

#ifndef FP_INT_TOWARDZERO
#define FP_INT_TOWARDZERO 2
#endif // FP_INT_TOWARDZERO

#ifndef FP_INT_TONEARESTFROMZERO
#define FP_INT_TONEARESTFROMZERO 3
#endif // FP_INT_TONEARESTFROMZERO

#ifndef FP_INT_TONEAREST
#define FP_INT_TONEAREST 4
#endif // FP_INT_TONEAREST

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_MATH_MACROS_H
