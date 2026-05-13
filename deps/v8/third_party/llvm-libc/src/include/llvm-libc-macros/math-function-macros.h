//===-- Definition of function macros from math.h -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_MATH_FUNCTION_MACROS_H
#define LLVM_LIBC_MACROS_MATH_FUNCTION_MACROS_H

#include "math-macros.h"

#ifndef __cplusplus
#define issignaling(x)                                                         \
  _Generic((x),                                                                \
      float: issignalingf,                                                     \
      double: issignaling,                                                     \
      long double: issignalingl)(x)
#define iscanonical(x)                                                         \
  _Generic((x),                                                                \
      float: iscanonicalf,                                                     \
      double: iscanonical,                                                     \
      long double: iscanonicall)(x)
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
