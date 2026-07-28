//===-- Common header for PolyEval implementations --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H

#include "multiply_add.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

// Evaluate polynomial using Horner's Scheme:
// With polyeval(x, a_0, a_1, ..., a_n) = a_n * x^n + ... + a_1 * x + a_0, we
// evaluated it as:  a_0 + x * (a_1 + x * ( ... (a_(n-1) + x * a_n) ... ) ) ).
// We will use FMA instructions if available.
// Example: to evaluate x^3 + 2*x^2 + 3*x + 4, call
//   polyeval( x, 4.0, 3.0, 2.0, 1.0 )

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(void *)), T>
polyeval(const T &, const T &a0) {
  return a0;
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(void *)), T>
polyeval(T, T a0) {
  return a0;
}

template <typename T, typename... Ts>
LIBC_INLINE static constexpr cpp::enable_if_t<(sizeof(T) > sizeof(void *)), T>
polyeval(const T &x, const T &a0, const Ts &...a) {
  return multiply_add(x, polyeval(x, a...), a0);
}

template <typename T, typename... Ts>
LIBC_INLINE LIBC_CONSTEXPR cpp::enable_if_t<(sizeof(T) <= sizeof(void *)), T>
polyeval(T x, T a0, Ts... a) {
  return multiply_add(x, polyeval(x, a...), a0);
}

// Evaluating alternating polynomials using subtraction directly.
// altpolyeval(x, a_0, a_1, ..., a_n) = a_0 - x * a_1 + x^2 * a_2 - ... +
//                                      + (-1)^n x_n * a_n.
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(void *)), T>
altpolyeval(const T &, const T &a0) {
  return a0;
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(void *)), T>
altpolyeval(T, T a0) {
  return a0;
}

// TODO: Make use of FMA instructions when using these for floating points.
template <typename T, typename... Ts>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(void *)), T>
altpolyeval(const T &x, const T &a0, const Ts &...a) {
  return a0 - x * altpolyeval(x, a...);
}

template <typename T, typename... Ts>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(void *)), T>
altpolyeval(T x, T a0, Ts... a) {
  return a0 - x * altpolyeval(x, a...);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H
