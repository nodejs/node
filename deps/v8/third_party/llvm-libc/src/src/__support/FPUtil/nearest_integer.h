//===-- Fast rounding to nearest integer for floating point -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEAREST_INTEGER_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEAREST_INTEGER_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/cpu_features.h"

#if (defined(LIBC_TARGET_ARCH_IS_X86_64) && defined(LIBC_TARGET_CPU_HAS_SSE4_2))
#include "x86_64/nearest_integer.h"
#elif (defined(LIBC_TARGET_ARCH_IS_AARCH64) && defined(__ARM_FP))
#include "aarch64/nearest_integer.h"
#elif defined(LIBC_TARGET_ARCH_IS_GPU)

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE float nearest_integer(float x) { return __builtin_rintf(x); }

LIBC_INLINE double nearest_integer(double x) { return __builtin_rint(x); }

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#else

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// This is a fast implementation for rounding to a nearest integer that.
//
// Notice that for AARCH64 and x86-64 with SSE4.2 support, we will use their
// corresponding rounding instruction instead.  And in those cases, the results
// are rounded to the nearest integer, tie-to-even.
LIBC_INLINE static constexpr float nearest_integer(float x) {
  if (x < 0x1p24f && x > -0x1p24f) {
    float r = x < 0 ? (x - 0x1.0p23f) + 0x1.0p23f : (x + 0x1.0p23f) - 0x1.0p23f;
    float diff = x - r;
    // The expression above is correct for the default rounding mode, round-to-
    // nearest, tie-to-even.  For other rounding modes, it might be off by 1,
    // which is corrected below.
    if (LIBC_UNLIKELY(diff > 0.5f))
      return r + 1.0f;
    if (LIBC_UNLIKELY(diff < -0.5f))
      return r - 1.0f;
    return r;
  }
  return x;
}

LIBC_INLINE static constexpr double nearest_integer(double x) {
  if (x < 0x1p53 && x > -0x1p53) {
    double r = x < 0 ? (x - 0x1.0p52) + 0x1.0p52 : (x + 0x1.0p52) - 0x1.0p52;
    double diff = x - r;
    // The expression above is correct for the default rounding mode, round-to-
    // nearest, tie-to-even.  For other rounding modes, it might be off by 1,
    // which is corrected below.
    if (LIBC_UNLIKELY(diff > 0.5))
      return r + 1.0;
    if (LIBC_UNLIKELY(diff < -0.5))
      return r - 1.0;
    return r;
  }
  return x;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif
#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEAREST_INTEGER_H
