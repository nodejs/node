//===---- Free-standing function to detect rounding mode --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_ROUNDING_MODE_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_ROUNDING_MODE_H

#include "hdr/fenv_macros.h"
#include "src/__support/CPP/type_traits.h"   // is_constant_evaluated
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace generic {

// Quick free-standing test whether fegetround() == FE_UPWARD.
// Using the following observation:
//   1.0f + 2^-25 = 1.0f        for FE_TONEAREST, FE_DOWNWARD, FE_TOWARDZERO
//                = 0x1.000002f for FE_UPWARD.
LIBC_INLINE bool fenv_is_round_up() {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  return false;
#else
  static volatile float x = 0x1.0p-25f;
  return (1.0f + x != 1.0f);
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

// Quick free-standing test whether fegetround() == FE_DOWNWARD.
// Using the following observation:
//   -1.0f - 2^-25 = -1.0f        for FE_TONEAREST, FE_UPWARD, FE_TOWARDZERO
//                 = -0x1.000002f for FE_DOWNWARD.
LIBC_INLINE bool fenv_is_round_down() {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  return false;
#else
  static volatile float x = 0x1.0p-25f;
  return (-1.0f - x != -1.0f);
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

// Quick free-standing test whether fegetround() == FE_TONEAREST.
// Using the following observation:
//   1.5f + 2^-24 = 1.5f           for FE_TONEAREST, FE_DOWNWARD, FE_TOWARDZERO
//                = 0x1.100002p0f  for FE_UPWARD,
//   1.5f - 2^-24 = 1.5f           for FE_TONEAREST, FE_UPWARD
//                = 0x1.0ffffep-1f for FE_DOWNWARD, FE_TOWARDZERO
LIBC_INLINE bool fenv_is_round_to_nearest() {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  return true;
#else
  static volatile float x = 0x1.0p-24f;
  float y = 1.5f + x;
  return (y == 1.5f - x);
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

// Quick free-standing test whether fegetround() == FE_TOWARDZERO.
// Using the following observation:
//   1.0f + 2^-23 + 2^-24 = 0x1.000002p0f for FE_DOWNWARD, FE_TOWARDZERO
//                        = 0x1.000004p0f for FE_TONEAREST, FE_UPWARD,
//  -1.0f - 2^-24 = -1.0f          for FE_TONEAREST, FE_UPWARD, FE_TOWARDZERO
//                = -0x1.000002p0f for FE_DOWNWARD
// So:
// (0x1.000002p0f + 2^-24) + (-1.0f - 2^-24) = 2^-23 for FE_TOWARDZERO
//                                           = 2^-22 for FE_TONEAREST, FE_UPWARD
//                                           = 0 for FE_DOWNWARD
LIBC_INLINE bool fenv_is_round_to_zero() {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  return false;
#else
  static volatile float x = 0x1.0p-24f;
  float y = x;
  return ((0x1.000002p0f + y) + (-1.0f - y) == 0x1.0p-23f);
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

// Quick free standing get rounding mode based on the above observations.
LIBC_INLINE int quick_get_round() {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  return FE_TONEAREST;
#else
  static volatile float x = 0x1.0p-24f;
  float y = x;
  float z = (0x1.000002p0f + y) + (-1.0f - y);

  if (z == 0.0f)
    return FE_DOWNWARD;
  if (z == 0x1.0p-23f)
    return FE_TOWARDZERO;
  return (2.0f + y == 2.0f) ? FE_TONEAREST : FE_UPWARD;
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

} // namespace generic

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT bool fenv_is_round_up() {
  if (cpp::is_constant_evaluated()) {
    return false;
  } else {
    return generic::fenv_is_round_up();
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT bool fenv_is_round_down() {
  if (cpp::is_constant_evaluated()) {
    return false;
  } else {
    return generic::fenv_is_round_down();
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT bool fenv_is_round_to_nearest() {
  if (cpp::is_constant_evaluated()) {
    return true;
  } else {
    return generic::fenv_is_round_to_nearest();
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT bool fenv_is_round_to_zero() {
  if (cpp::is_constant_evaluated()) {
    return false;
  } else {
    return generic::fenv_is_round_to_zero();
  }
}

// Quick free standing get rounding mode based on the above observations.
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT int quick_get_round() {
  if (cpp::is_constant_evaluated()) {
    return FE_TONEAREST;
  } else {
    return generic::quick_get_round();
  }
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_ROUNDING_MODE_H
