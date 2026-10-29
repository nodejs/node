//===-- Definition of function macros from math.h -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_MATH_FUNCTION_MACROS_H
#define LLVM_LIBC_MACROS_MATH_FUNCTION_MACROS_H

#include "float16-macros.h" // LIBC_TYPES_HAS_FLOAT16, LIBC_TYPES_HAS_FLOAT128
#include "math-macros.h"

#ifndef __cplusplus
#define issignaling(x)                                                         \
  _Generic((x),                                                                \
      float: issignalingf,                                                     \
      double: issignaling,                                                     \
      long double: issignalingl)(x)

// 'iscanonical' is a C23 type-generic macro (7.12.3.1).  The '_Float16' and
// 'float128' associations are only added when those types exist, and 'float128'
// only when it is distinct from 'long double' (otherwise _Generic would have
// two associations with compatible types).  The 'float128' guard mirrors
// LIBC_TYPES_FLOAT128_IS_NOT_LONG_DOUBLE in
// src/__support/macros/properties/types.h, which public headers cannot include.
#ifdef LIBC_TYPES_HAS_FLOAT16
#define __LIBC_MATH_ISCANONICAL_F16 _Float16 : iscanonicalf16,
#else
#define __LIBC_MATH_ISCANONICAL_F16
#endif
#if defined(LIBC_TYPES_HAS_FLOAT128) && (LDBL_MANT_DIG != 113)
#define __LIBC_MATH_ISCANONICAL_F128                                           \
  float128:                                                                    \
  iscanonicalf128,
#else
#define __LIBC_MATH_ISCANONICAL_F128
#endif
// clang-format off
#define iscanonical(x)                                                         \
  _Generic((x),                                                                \
      __LIBC_MATH_ISCANONICAL_F16                                              \
      __LIBC_MATH_ISCANONICAL_F128                                             \
      float: iscanonicalf,                                                     \
      double: iscanonical,                                                     \
      long double: iscanonicall)(x)
// clang-format on
#endif

#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define signbit(x) __builtin_signbit(x)
#define iszero(x) (x == 0)
#define fpclassify(x)                                                          \
  __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isnormal(x) __builtin_isnormal(x)
#define issubnormal(x) (fpclassify(x) == FP_SUBNORMAL)

#endif // LLVM_LIBC_MACROS_MATH_FUNCTION_MACROS_H
