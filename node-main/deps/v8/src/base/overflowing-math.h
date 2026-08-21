// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_OVERFLOWING_MATH_H_
#define V8_BASE_OVERFLOWING_MATH_H_

#include <stdint.h>

#include <cmath>
#include <type_traits>

#include "src/base/macros.h"

namespace v8 {
namespace base {

// Helpers for performing overflowing arithmetic operations without relying
// on C++ undefined behavior.
#define ASSERT_SIGNED_INTEGER_TYPE(Type)                            \
  static_assert(std::is_integral_v<Type> && std::is_signed_v<Type>, \
                "use this for signed integer types");
#define OP_WITH_WRAPAROUND(Name, OP)                                      \
  template <typename signed_type>                                         \
  inline signed_type Name##WithWraparound(signed_type a, signed_type b) { \
    ASSERT_SIGNED_INTEGER_TYPE(signed_type);                              \
    using unsigned_type = typename std::make_unsigned_t<signed_type>;     \
    unsigned_type a_unsigned = static_cast<unsigned_type>(a);             \
    unsigned_type b_unsigned = static_cast<unsigned_type>(b);             \
    unsigned_type result = a_unsigned OP b_unsigned;                      \
    return static_cast<signed_type>(result);                              \
  }

OP_WITH_WRAPAROUND(Add, +)
OP_WITH_WRAPAROUND(Sub, -)
OP_WITH_WRAPAROUND(Mul, *)

// 16-bit integers are special due to C++'s implicit conversion rules.
// See https://bugs.llvm.org/show_bug.cgi?id=25580.
template <>
inline int16_t MulWithWraparound(int16_t a, int16_t b) {
  uint32_t a_unsigned = static_cast<uint32_t>(a);
  uint32_t b_unsigned = static_cast<uint32_t>(b);
  uint32_t result = a_unsigned * b_unsigned;
  return static_cast<int16_t>(static_cast<uint16_t>(result));
}

#undef OP_WITH_WRAPAROUND

template <typename signed_type>
inline signed_type NegateWithWraparound(signed_type a) {
  ASSERT_SIGNED_INTEGER_TYPE(signed_type);
  if (a == std::numeric_limits<signed_type>::min()) return a;
  return -a;
}

template <typename signed_type>
inline signed_type ShlWithWraparound(signed_type a, signed_type b) {
  ASSERT_SIGNED_INTEGER_TYPE(signed_type);
  using unsigned_type = std::make_unsigned_t<signed_type>;
  const unsigned_type kMask = (sizeof(a) * 8) - 1;
  return static_cast<signed_type>(static_cast<unsigned_type>(a) << (b & kMask));
}

#undef ASSERT_SIGNED_INTEGER_TYPE

// Returns the quotient x/y, avoiding C++ undefined behavior if y == 0.
template <typename T>
inline T Divide(T x, T y) {
  if (y != 0) return x / y;
  if (x == 0 || x != x) return std::numeric_limits<T>::quiet_NaN();
  if ((x >= 0) == (std::signbit(y) == 0)) {
    return std::numeric_limits<T>::infinity();
  }
  return -std::numeric_limits<T>::infinity();
}

inline float Recip(float a) { return Divide(1.0f, a); }

inline float RecipSqrt(float a) {
  if (a != 0) return 1.0f / std::sqrt(a);
  if (std::signbit(a) == 0) return std::numeric_limits<float>::infinity();
  return -std::numeric_limits<float>::infinity();
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_OVERFLOWING_MATH_H_
