//===-- Mimics llvm/Support/MathExtras.h ------------------------*- C++ -*-===//
// Provides useful math functions.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXTRAS_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXTRAS_H

#include "src/__support/CPP/bit.h"         // countl_one, countr_zero
#include "src/__support/CPP/limits.h"      // CHAR_BIT, numeric_limits
#include "src/__support/CPP/type_traits.h" // is_unsigned_v, is_constant_evaluated
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// Create a bitmask with the count right-most bits set to 1, and all other bits
// set to 0.  Only unsigned types are allowed.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
mask_trailing_ones() {
  constexpr unsigned T_BITS = CHAR_BIT * sizeof(T);
  static_assert(count <= T_BITS && "Invalid bit index");
  // MSVC complains about out of range shifts.
  if constexpr (count == 0)
    return 0;
  else if constexpr (count >= T_BITS)
    return T(-1);
  else
    return T(-1) >> (T_BITS - count);
}

// Create a bitmask with the count left-most bits set to 1, and all other bits
// set to 0.  Only unsigned types are allowed.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
mask_leading_ones() {
  return T(~mask_trailing_ones<T, CHAR_BIT * sizeof(T) - count>());
}

// Create a bitmask with the count right-most bits set to 0, and all other bits
// set to 1.  Only unsigned types are allowed.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
mask_trailing_zeros() {
  return mask_leading_ones<T, CHAR_BIT * sizeof(T) - count>();
}

// Create a bitmask with the count left-most bits set to 0, and all other bits
// set to 1.  Only unsigned types are allowed.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
mask_leading_zeros() {
  return mask_trailing_ones<T, CHAR_BIT * sizeof(T) - count>();
}

// Returns whether 'a + b' overflows, the result is stored in 'res'.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr bool add_overflow(T a, T b, T &res) {
#if __has_builtin(__builtin_add_overflow)
  return __builtin_add_overflow(a, b, &res);
#else
  res = a + b;
  return (res < a) || (res < b);
#endif // __builtin_add_overflow
}

// Returns whether 'a - b' overflows, the result is stored in 'res'.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr bool sub_overflow(T a, T b, T &res) {
#if __has_builtin(__builtin_sub_overflow)
  return __builtin_sub_overflow(a, b, &res);
#else
  res = a - b;
  return (res > a);
#endif // __builtin_sub_overflow
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr bool mul_overflow(T a, T b, T &res) {
#if __has_builtin(__builtin_mul_overflow)
  return __builtin_mul_overflow(a, b, &res);
#else
  T max = cpp::numeric_limits<T>::max();
  T min = cpp::numeric_limits<T>::min();
  bool overflow = (b > 0 && (a > max / b || a < min / b)) ||
                  (b < 0 && (a < max / b || a > min / b));
  if (!overflow)
    res = a * b;
  return overflow;
#endif
}

#define RETURN_IF(TYPE, BUILTIN)                                               \
  if constexpr (cpp::is_same_v<T, TYPE>)                                       \
    return BUILTIN(a, b, carry_in, &carry_out);

// Returns the result of 'a + b' taking into account 'carry_in'.
// The carry out is stored in 'carry_out' it not 'nullptr', dropped otherwise.
// We keep the pass by pointer interface for consistency with the intrinsic.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
add_with_carry(T a, T b, T carry_in, T &carry_out) {
  if (!cpp::is_constant_evaluated()) {
#if __has_builtin(__builtin_addcb)
    RETURN_IF(unsigned char, __builtin_addcb)
#elif __has_builtin(__builtin_addcs)
    RETURN_IF(unsigned short, __builtin_addcs)
#elif __has_builtin(__builtin_addc)
    RETURN_IF(unsigned int, __builtin_addc)
#elif __has_builtin(__builtin_addcl)
    RETURN_IF(unsigned long, __builtin_addcl)
#elif __has_builtin(__builtin_addcll)
    RETURN_IF(unsigned long long, __builtin_addcll)
#endif
  }
  T sum = {};
  T carry1 = add_overflow(a, b, sum);
  T carry2 = add_overflow(sum, carry_in, sum);
  carry_out = carry1 | carry2;
  return sum;
}

// Returns the result of 'a - b' taking into account 'carry_in'.
// The carry out is stored in 'carry_out' it not 'nullptr', dropped otherwise.
// We keep the pass by pointer interface for consistency with the intrinsic.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
sub_with_borrow(T a, T b, T carry_in, T &carry_out) {
  if (!cpp::is_constant_evaluated()) {
#if __has_builtin(__builtin_subcb)
    RETURN_IF(unsigned char, __builtin_subcb)
#elif __has_builtin(__builtin_subcs)
    RETURN_IF(unsigned short, __builtin_subcs)
#elif __has_builtin(__builtin_subc)
    RETURN_IF(unsigned int, __builtin_subc)
#elif __has_builtin(__builtin_subcl)
    RETURN_IF(unsigned long, __builtin_subcl)
#elif __has_builtin(__builtin_subcll)
    RETURN_IF(unsigned long long, __builtin_subcll)
#endif
  }
  T sub = {};
  T carry1 = sub_overflow(a, b, sub);
  T carry2 = sub_overflow(sub, carry_in, sub);
  carry_out = carry1 | carry2;
  return sub;
}

#undef RETURN_IF

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
first_leading_zero(T value) {
  return value == cpp::numeric_limits<T>::max() ? 0
                                                : cpp::countl_one(value) + 1;
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
first_leading_one(T value) {
  return first_leading_zero(static_cast<T>(~value));
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
first_trailing_zero(T value) {
  return value == cpp::numeric_limits<T>::max()
             ? 0
             : cpp::countr_zero(static_cast<T>(~value)) + 1;
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
first_trailing_one(T value) {
  return value == 0 ? 0 : cpp::countr_zero(value) + 1;
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
count_zeros(T value) {
  return cpp::popcount<T>(static_cast<T>(~value));
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXTRAS_H
