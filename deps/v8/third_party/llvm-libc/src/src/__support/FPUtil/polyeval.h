//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Common header for polynomial evaluation implementations.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// Evaluate polynomial using Horner's scheme:
//   polyeval(x, a_0, a_1, ..., a_n) = a_0 + x * (a_1 + ... + x * a_n).
template <typename T, typename... Ts>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
polyeval([[maybe_unused]] T x, T a0, Ts... a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    T r = polyeval(x, a...);
    return multiply_add(x, r, a0);
  }
}

template <typename T, typename... Ts>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
polyeval([[maybe_unused]] const T &x, const T &a0, const Ts &...a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    T r = polyeval(x, a...);
    return multiply_add(x, r, a0);
  }
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
polyeval([[maybe_unused]] T x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  if constexpr (OFFSET + 1 == N) {
    return a[OFFSET];
  } else {
    T r = polyeval<OFFSET + 1>(x, a);
    return multiply_add(x, r, a[OFFSET]);
  }
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
polyeval([[maybe_unused]] const T &x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  if constexpr (OFFSET + 1 == N) {
    return a[OFFSET];
  } else {
    T r = polyeval<OFFSET + 1>(x, a);
    return multiply_add(x, r, a[OFFSET]);
  }
}

// Evaluate alternating polynomial:
//   altpolyeval(x, a_0, a_1, ..., a_n) = a_0 - x * a_1 + x^2 * a_2 - ...
//                                        + (-1)^n * x^n * a_n.
template <typename T, typename... Ts>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
altpolyeval([[maybe_unused]] T x, T a0, Ts... a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    T r = altpolyeval(x, a...);
    return sub_multiply(a0, x, r);
  }
}

template <typename T, typename... Ts>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
altpolyeval([[maybe_unused]] const T &x, const T &a0, const Ts &...a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    T r = altpolyeval(x, a...);
    return sub_multiply(a0, x, r);
  }
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
altpolyeval([[maybe_unused]] T x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  if constexpr (OFFSET + 1 == N) {
    return a[OFFSET];
  } else {
    T r = altpolyeval<OFFSET + 1>(x, a);
    return sub_multiply(a[OFFSET], x, r);
  }
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
altpolyeval([[maybe_unused]] const T &x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  if constexpr (OFFSET + 1 == N) {
    return a[OFFSET];
  } else {
    T r = altpolyeval<OFFSET + 1>(x, a);
    return sub_multiply(a[OFFSET], x, r);
  }
}

// Evaluate polynomial using a non-unrolled Horner loop:
//   loop_polyeval<OFFSET>(x, a) = a[OFFSET] + x * (a[OFFSET + 1] + ...
//                                 + x * a[N - 1]).
template <size_t OFFSET = 0, typename T, size_t N>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
loop_polyeval([[maybe_unused]] T x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  T r = a[N - 1];
  if constexpr (OFFSET + 1 < N) {
    LIBC_LOOP_NOUNROLL
    for (size_t i = N - 1; i > OFFSET; --i)
      r = multiply_add(x, r, a[i - 1]);
  }
  return r;
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
loop_polyeval([[maybe_unused]] const T &x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  T r = a[N - 1];
  if constexpr (OFFSET + 1 < N) {
    LIBC_LOOP_NOUNROLL
    for (size_t i = N - 1; i > OFFSET; --i)
      r = multiply_add(x, r, a[i - 1]);
  }
  return r;
}

// Evaluate alternating polynomial using a non-unrolled Horner loop:
//   loop_altpolyeval<OFFSET>(x, a) = a[OFFSET] - x * a[OFFSET + 1] + ...
//       + (-1)^(N - 1 - OFFSET) * x^(N - 1 - OFFSET) * a[N - 1].
template <size_t OFFSET = 0, typename T, size_t N>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
loop_altpolyeval([[maybe_unused]] T x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  T r = a[N - 1];
  if constexpr (OFFSET + 1 < N) {
    LIBC_LOOP_NOUNROLL
    for (size_t i = N - 1; i > OFFSET; --i)
      r = sub_multiply(a[i - 1], x, r);
  }
  return r;
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
loop_altpolyeval([[maybe_unused]] const T &x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  T r = a[N - 1];
  if constexpr (OFFSET + 1 < N) {
    LIBC_LOOP_NOUNROLL
    for (size_t i = N - 1; i > OFFSET; --i)
      r = sub_multiply(a[i - 1], x, r);
  }
  return r;
}

namespace internal {

template <typename T>
using ValOrConstRef =
    cpp::conditional_t<(sizeof(T) > sizeof(double)), const T &, T>;

// For len >= 2, let 2^h < len <= 2^(h + 1), so tree depth is h + 1 and each
// subtree has capacity at most 2^h. Return the smallest k such that
// 2^k >= len - 2^h, giving a right-heavy split:
//   LOW = 2^k <= 2^h,  1 <= HI = len - 2^k <= 2^h.
// Examples:
//   len = 2, 3    -> k = 0 (1 + 1, 1 + 2)
//   len = 4       -> k = 1 (2 + 2)
//   len = 5       -> k = 0 (1 + 4)
//   len = 6       -> k = 1 (2 + 4)
//   len = 7, 8    -> k = 2 (4 + 3, 4 + 4)
//   len = 9       -> k = 0 (1 + 8)
//   len = 10      -> k = 1 (2 + 8)
//   len = 11, 12  -> k = 2 (4 + 7, 4 + 8)
LIBC_INLINE constexpr size_t estrin_split_log2(size_t len) {
  size_t h = 0;
  while ((size_t(1) << (h + 1)) < len)
    ++h;
  size_t rem = len - (size_t(1) << h);
  size_t k = 0;
  while ((size_t(1) << k) < rem)
    ++k;
  return k;
}

// Return the largest dyadic power index K needed across the entire right-heavy
// Estrin tree of length len (for len >= 2). Powers up to x^(2^K) can form a
// full high subtree of length 2^(K + 1) plus a low subtree of length 2^K,
// covering len <= 3 * 2^K:
//   len = 2, 3    -> K = 0 (x)
//   len = 4..6    -> K = 1 (x, x^2)
//   len = 7..12   -> K = 2 (x, x^2, x^4)
//   len = 13..24  -> K = 3 (x, x^2, x^4, x^8)
LIBC_INLINE constexpr size_t estrin_max_power_log2(size_t len) {
  size_t k = 0;
  while ((size_t(3) << k) < len)
    ++k;
  return k;
}

// Precompute dyadic powers x^(2^0), x^(2^1), ..., x^(2^K) once by repeated
// squaring so that all subtrees share x^2, x^4, ..., x^(2^K). Members are
// initialized in declaration order (`prev` before `x_pow`), computing:
//   EstrinPows<T, 0>: x_pow = x
//   EstrinPows<T, K>: prev = EstrinPows<T, K - 1>(x),
//                     x_pow = prev.x_pow * prev.x_pow = x^(2^K)
//   pows.get<I>() returns x^(2^I) for 0 <= I <= K.
template <typename T, size_t K> struct EstrinPows {
  EstrinPows<T, K - 1> prev;
  T x_pow;

  LIBC_ALWAYS_INLINE constexpr explicit EstrinPows(ValOrConstRef<T> x)
      : prev(x), x_pow(prev.x_pow * prev.x_pow) {}

  template <size_t I>
  LIBC_ALWAYS_INLINE constexpr ValOrConstRef<T> get() const {
    if constexpr (I == K)
      return x_pow;
    else
      return prev.template get<I>();
  }
};

template <typename T> struct EstrinPows<T, 0> {
  T x_pow;

  LIBC_ALWAYS_INLINE constexpr explicit EstrinPows(ValOrConstRef<T> x)
      : x_pow(x) {}

  template <size_t I>
  LIBC_ALWAYS_INLINE constexpr ValOrConstRef<T> get() const {
    static_assert(I == 0, "Invalid Estrin power index.");
    return x_pow;
  }
};

// Recursively evaluate P(x) = sum_{i = 0}^{LEN - 1} a[OFFSET + i] * x^i using
// a right-heavy Estrin binary tree of minimum depth ceil(log2(LEN)):
//   - LEN == 1: a[OFFSET]
//   - LEN == 2: multiply_add(x, a[OFFSET + 1], a[OFFSET])
//             = a[OFFSET] + x * a[OFFSET + 1]
//   - LEN >= 3: let K = estrin_split_log2(LEN) and LOW = 2^K. Split into:
//       lo = estrin_eval_tree<OFFSET, LOW>(pows, a)
//          = sum_{i = 0}^{LOW - 1} a[OFFSET + i] * x^i
//       hi = estrin_eval_tree<OFFSET + LOW, LEN - LOW>(pows, a)
//          = sum_{i = 0}^{LEN - LOW - 1} a[OFFSET + LOW + i] * x^i
//     and combine via multiply_add(x^(2^K), hi, lo) = lo + x^(2^K) * hi.
//
// Placing the larger subtree in `hi` minimizes both the highest required power
// of x and the number of roundings on a[OFFSET] when |x| << 1, while keeping
// tree depth ceil(log2(LEN)). With x2 = x^2 and x4 = x^4, each (u + p * v)
// below is evaluated as multiply_add(p, v, u):
//   LEN = 3: a0 + x * (a1 + x * a2)
//   LEN = 4: (a0 + x * a1) + x2 * (a2 + x * a3)
//   LEN = 5: a0 + x * ((a1 + x * a2) + x2 * (a3 + x * a4))
//   LEN = 6: (a0 + x * a1) + x2 * ((a2 + x * a3) + x2 * (a4 + x * a5))
//   LEN = 7: ((a0 + x * a1) + x2 * (a2 + x * a3))
//            + x4 * (a4 + x * (a5 + x * a6))
//   LEN = 8: ((a0 + x * a1) + x2 * (a2 + x * a3))
//            + x4 * ((a4 + x * a5) + x2 * (a6 + x * a7))
template <size_t OFFSET, size_t LEN, typename T, size_t N, size_t MAX_K>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
estrin_eval_tree(const EstrinPows<T, MAX_K> &pows, const T (&a)[N]) {
  if constexpr (LEN == 1) {
    return a[OFFSET];
  } else if constexpr (LEN == 2) {
    return multiply_add(pows.template get<0>(), a[OFFSET + 1], a[OFFSET]);
  } else {
    constexpr size_t K = estrin_split_log2(LEN);
    constexpr size_t LOW = size_t(1) << K;
    if constexpr (LOW == 1) {
      T hi = estrin_eval_tree<OFFSET + 1, LEN - 1>(pows, a);
      return multiply_add(pows.template get<0>(), hi, a[OFFSET]);
    } else {
      T lo = estrin_eval_tree<OFFSET, LOW>(pows, a);
      T hi = estrin_eval_tree<OFFSET + LOW, LEN - LOW>(pows, a);
      return multiply_add(pows.template get<K>(), hi, lo);
    }
  }
}

template <size_t OFFSET, size_t LEN, typename T, size_t N, size_t MAX_K>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
estrin_eval_tree(const EstrinPows<T, MAX_K> &pows, const T (&a)[N]) {
  if constexpr (LEN == 1) {
    return a[OFFSET];
  } else if constexpr (LEN == 2) {
    return multiply_add(pows.template get<0>(), a[OFFSET + 1], a[OFFSET]);
  } else {
    constexpr size_t K = estrin_split_log2(LEN);
    constexpr size_t LOW = size_t(1) << K;
    if constexpr (LOW == 1) {
      T hi = estrin_eval_tree<OFFSET + 1, LEN - 1>(pows, a);
      return multiply_add(pows.template get<0>(), hi, a[OFFSET]);
    } else {
      T lo = estrin_eval_tree<OFFSET, LOW>(pows, a);
      T hi = estrin_eval_tree<OFFSET + LOW, LEN - LOW>(pows, a);
      return multiply_add(pows.template get<K>(), hi, lo);
    }
  }
}

} // namespace internal

// Evaluate polynomial using Estrin's scheme:
//   estrin_polyeval(x, a_0, a_1, ..., a_n) = a_0 + a_1 * x + ... + a_n * x^n.
template <size_t OFFSET = 0, typename T, size_t N>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
estrin_polyeval([[maybe_unused]] T x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  constexpr size_t LEN = N - OFFSET;
  if constexpr (LEN == 1) {
    return a[OFFSET];
  } else {
    constexpr size_t MAX_K = internal::estrin_max_power_log2(LEN);
    internal::EstrinPows<T, MAX_K> pows(x);
    return internal::estrin_eval_tree<OFFSET, LEN>(pows, a);
  }
}

template <size_t OFFSET = 0, typename T, size_t N>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
estrin_polyeval([[maybe_unused]] const T &x, const T (&a)[N]) {
  static_assert(OFFSET < N, "OFFSET must be less than array size N.");
  constexpr size_t LEN = N - OFFSET;
  if constexpr (LEN == 1) {
    return a[OFFSET];
  } else {
    constexpr size_t MAX_K = internal::estrin_max_power_log2(LEN);
    internal::EstrinPows<T, MAX_K> pows(x);
    return internal::estrin_eval_tree<OFFSET, LEN>(pows, a);
  }
}

template <typename T, typename... Ts>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
estrin_polyeval([[maybe_unused]] T x, T a0, Ts... a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    const T coeffs[] = {a0, a...};
    return estrin_polyeval(x, coeffs);
  }
}

template <typename T, typename... Ts>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
estrin_polyeval([[maybe_unused]] const T &x, const T &a0, const Ts &...a) {
  static_assert((cpp::is_same_v<T, Ts> && ...),
                "All coefficients must have the same type T.");
  if constexpr (sizeof...(Ts) == 0) {
    return a0;
  } else {
    const T coeffs[] = {a0, a...};
    return estrin_polyeval(x, coeffs);
  }
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_POLYEVAL_H
