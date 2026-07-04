//===-- Comparison operations on floating point numbers ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_COMPARISONOPERATIONS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_COMPARISONOPERATIONS_H

#include "FEnvImpl.h"
#include "FPBits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// All predicates are hereby implemented as per IEEE Std 754-2019
// Implements compareQuietEqual predicate
// Rules for comparison within the same floating point type
// 1. +0 = −0
// 2. (i)   +inf  = +inf
//    (ii)  -inf  = -inf
//    (iii) -inf != +inf
// 3. Any comparison with NaN returns false
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, bool>
equals(T x, T y) {
  using FPBits = FPBits<T>;
  FPBits x_bits(x);
  FPBits y_bits(y);

  if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan())
    fputil::raise_except_if_required(FE_INVALID);

  // NaN == x returns false for every x
  if (x_bits.is_nan() || y_bits.is_nan())
    return false;

  // +/- 0 == +/- 0
  if (x_bits.is_zero() && y_bits.is_zero())
    return true;

  return x_bits.uintval() == y_bits.uintval();
}

// Implements compareSignalingLess predicate
// Section 5.11 Rules:
// 1. -inf < x (x != -inf)
// 2. x < +inf (x != +inf)
// 3. Any comparison with NaN return false
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, bool>
less_than(T x, T y) {
  using FPBits = FPBits<T>;
  FPBits x_bits(x);
  FPBits y_bits(y);

  // Any comparison with NaN returns false
  if (x_bits.is_nan() || y_bits.is_nan()) {
    fputil::raise_except_if_required(FE_INVALID);
    return false;
  }

  if (x_bits.is_zero() && y_bits.is_zero())
    return false;

  if (x_bits.is_neg() && y_bits.is_pos())
    return true;

  if (x_bits.is_pos() && y_bits.is_neg())
    return false;

  // since floating-point numbers are stored in the format: s | e | m
  // we can directly compare the uintval's

  // both negative
  if (x_bits.is_neg())
    return x_bits.uintval() > y_bits.uintval();

  // both positive
  return x_bits.uintval() < y_bits.uintval();
}

// Implements compareSignalingGreater predicate
// x < y => y > x
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, bool>
greater_than(T x, T y) {
  return less_than(y, x);
}

// Implements compareSignalingLessEqual predicate
// x <= y => (x < y) || (x == y)
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, bool>
less_than_or_equals(T x, T y) {
  return less_than(x, y) || equals(x, y);
}

// Implements compareSignalingGreaterEqual predicate
// x >= y => (x > y) || (x == y) => (y < x) || (x == y)
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, bool>
greater_than_or_equals(T x, T y) {
  return less_than(y, x) || equals(x, y);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_COMPARISONOPERATIONS_H
