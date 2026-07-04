//===-- Implementation of the C++20 bit header  -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This is inspired by LLVM ADT/bit.h header.
// Some functions are missing, we can add them as needed.

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/limits.h" // numeric_limits
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/sanitizer.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

#if __has_builtin(__builtin_memcpy_inline)
#define LLVM_LIBC_HAS_BUILTIN_MEMCPY_INLINE
#endif

template <unsigned N>
LIBC_INLINE constexpr void inline_copy(const char *from, char *to) {
#if __has_builtin(__builtin_memcpy_inline) &&                                  \
    defined(LIBC_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
  if (cpp::is_constant_evaluated())
    for (unsigned i = 0; i < N; ++i)
      to[i] = from[i];
  else
    __builtin_memcpy_inline(to, from, N);
#else
  for (unsigned i = 0; i < N; ++i)
    to[i] = from[i];
#endif // __has_builtin(__builtin_memcpy_inline)
}

// This implementation of bit_cast requires trivially-constructible To, to avoid
// UB in the implementation.
template <typename To, typename From>
LIBC_INLINE static constexpr cpp::enable_if_t<
    (sizeof(To) == sizeof(From)) &&
        cpp::is_trivially_constructible<To>::value &&
        cpp::is_trivially_copyable<To>::value &&
        cpp::is_trivially_copyable<From>::value,
    To>
bit_cast(const From &from) {
#if LIBC_HAS_BUILTIN_BIT_CAST
  return __builtin_bit_cast(To, from);
#else
  To to{};
  char *dst = reinterpret_cast<char *>(&to);
  const char *src = reinterpret_cast<const char *>(&from);
  inline_copy<sizeof(From)>(src, dst);
  return to;
#endif // __has_builtin(__builtin_bit_cast)
}

// The following simple bit copy from a smaller type to maybe-larger type.
template <typename To, typename From>
LIBC_INLINE cpp::enable_if_t<(sizeof(To) >= sizeof(From)) &&
                                 cpp::is_trivially_constructible<To>::value &&
                                 cpp::is_trivially_copyable<To>::value &&
                                 cpp::is_trivially_copyable<From>::value,
                             void>
bit_copy(const From &from, To &to) {
  char *dst = reinterpret_cast<char *>(&to);
  const char *src = reinterpret_cast<const char *>(&from);
  inline_copy<sizeof(From)>(src, dst);
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>,
                                                     bool>
has_single_bit(T value) {
  return (value != 0) && ((value & (value - 1)) == 0);
}

// A temporary macro to add template function specialization when compiler
// builtin is available.
#define ADD_SPECIALIZATION(NAME, TYPE, BUILTIN)                                \
  template <> [[nodiscard]] LIBC_INLINE constexpr int NAME<TYPE>(TYPE value) { \
    static_assert(cpp::is_unsigned_v<TYPE>);                                   \
    return value == 0 ? cpp::numeric_limits<TYPE>::digits : BUILTIN(value);    \
  }

/// Count number of 0's from the least significant bit to the most
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of 0.
// clang-19+, gcc-14+
#if __has_builtin(__builtin_ctzg)
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countr_zero(T value) {
  return __builtin_ctzg(value, cpp::numeric_limits<T>::digits);
}
#else
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countr_zero(T value) {
  if (!value)
    return cpp::numeric_limits<T>::digits;
  if (value & 0x1)
    return 0;
  // Bisection method.
  unsigned zero_bits = 0;
  unsigned shift = cpp::numeric_limits<T>::digits >> 1;
  T mask = cpp::numeric_limits<T>::max() >> shift;
  while (shift) {
    if ((value & mask) == 0) {
      value >>= shift;
      zero_bits |= shift;
    }
    shift >>= 1;
    mask >>= shift;
  }
  return static_cast<int>(zero_bits);
}
#if __has_builtin(__builtin_ctzs)
ADD_SPECIALIZATION(countr_zero, unsigned short, __builtin_ctzs)
#endif // __has_builtin(__builtin_ctzs)
#if __has_builtin(__builtin_ctz)
ADD_SPECIALIZATION(countr_zero, unsigned int, __builtin_ctz)
#endif // __has_builtin(__builtin_ctz)
#if __has_builtin(__builtin_ctzl)
ADD_SPECIALIZATION(countr_zero, unsigned long, __builtin_ctzl)
#endif // __has_builtin(__builtin_ctzl)
#if __has_builtin(__builtin_ctzll)
ADD_SPECIALIZATION(countr_zero, unsigned long long, __builtin_ctzll)
#endif // __has_builtin(__builtin_ctzll)
#endif // __has_builtin(__builtin_ctzg)

/// Count number of 0's from the most significant bit to the least
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of 0.
// clang-19+, gcc-14+
#if __has_builtin(__builtin_clzg)
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countl_zero(T value) {
  return __builtin_clzg(value, cpp::numeric_limits<T>::digits);
}
#else
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countl_zero(T value) {
  if (!value)
    return cpp::numeric_limits<T>::digits;
  // Bisection method.
  unsigned zero_bits = 0;
  for (unsigned shift = cpp::numeric_limits<T>::digits >> 1; shift;
       shift >>= 1) {
    T tmp = value >> shift;
    if (tmp)
      value = tmp;
    else
      zero_bits |= shift;
  }
  return static_cast<int>(zero_bits);
}
#if __has_builtin(__builtin_clzs)
ADD_SPECIALIZATION(countl_zero, unsigned short, __builtin_clzs)
#endif // __has_builtin(__builtin_clzs)
#if __has_builtin(__builtin_clz)
ADD_SPECIALIZATION(countl_zero, unsigned int, __builtin_clz)
#endif // __has_builtin(__builtin_clz)
#if __has_builtin(__builtin_clzl)
ADD_SPECIALIZATION(countl_zero, unsigned long, __builtin_clzl)
#endif // __has_builtin(__builtin_clzl)
#if __has_builtin(__builtin_clzll)
ADD_SPECIALIZATION(countl_zero, unsigned long long, __builtin_clzll)
#endif // __has_builtin(__builtin_clzll)
#endif // __has_builtin(__builtin_clzg)

#undef ADD_SPECIALIZATION

/// Count the number of ones from the most significant bit to the first
/// zero bit.
///
/// Ex. countl_one(0xFF0FFF00) == 8.
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of all ones.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countl_one(T value) {
  return cpp::countl_zero<T>(static_cast<T>(~value));
}

/// Count the number of ones from the least significant bit to the first
/// zero bit.
///
/// Ex. countr_one(0x00FF00FF) == 8.
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of all ones.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
countr_one(T value) {
  return cpp::countr_zero<T>(static_cast<T>(~value));
}

/// Returns the number of bits needed to represent value if value is nonzero.
/// Returns 0 otherwise.
///
/// Ex. bit_width(5) == 3.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
bit_width(T value) {
  return cpp::numeric_limits<T>::digits - cpp::countl_zero(value);
}

/// Returns the largest integral power of two no greater than value if value is
/// nonzero.  Returns 0 otherwise.
///
/// Ex. bit_floor(5) == 4.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
bit_floor(T value) {
  if (!value)
    return 0;
  return static_cast<T>(T(1) << (cpp::bit_width(value) - 1));
}

/// Returns the smallest integral power of two no smaller than value if value is
/// nonzero.  Returns 1 otherwise.
///
/// Ex. bit_ceil(5) == 8.
///
/// The return value is undefined if the input is larger than the largest power
/// of two representable in T.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
bit_ceil(T value) {
  if (value < 2)
    return 1;
  return static_cast<T>(T(1) << cpp::bit_width(value - 1U));
}

// Rotate algorithms make use of "Safe, Efficient, and Portable Rotate in C/C++"
// from https://blog.regehr.org/archives/1063.

// Forward-declare rotr so that rotl can use it.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
rotr(T value, int rotate);

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
rotl(T value, int rotate) {
  constexpr int N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotr<T>(value, -rotate);
  return static_cast<T>((value << rotate) | (value >> (N - rotate)));
}

template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, T>
rotr(T value, int rotate) {
  constexpr int N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotl<T>(value, -rotate);
  return static_cast<T>((value >> rotate) | (value << (N - rotate)));
}

// TODO: Do we need this function at all? How is it different from
// 'static_cast'?
template <class To, class From>
LIBC_INLINE constexpr To bit_or_static_cast(const From &from) {
  if constexpr (sizeof(To) == sizeof(From)) {
    return bit_cast<To>(from);
  } else {
    return static_cast<To>(from);
  }
}

/// Count number of 1's aka population count or Hamming weight.
///
/// Only unsigned integral types are allowed.
// clang-19+, gcc-14+
#if __has_builtin(__builtin_popcountg)
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
popcount(T value) {
  return __builtin_popcountg(value);
}
#else // !__has_builtin(__builtin_popcountg)
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_unsigned_v<T>, int>
popcount(T value) {
  int count = 0;
  while (value) {
    value &= value - 1;
    ++count;
  }
  return count;
}
#define ADD_SPECIALIZATION(TYPE, BUILTIN)                                      \
  template <>                                                                  \
  [[nodiscard]] LIBC_INLINE constexpr int popcount<TYPE>(TYPE value) {         \
    return BUILTIN(value);                                                     \
  }
#if __has_builtin(__builtin_popcount)
ADD_SPECIALIZATION(unsigned char, __builtin_popcount)
ADD_SPECIALIZATION(unsigned short, __builtin_popcount)
ADD_SPECIALIZATION(unsigned, __builtin_popcount)
#endif // __builtin_popcount
#if __has_builtin(__builtin_popcountl)
ADD_SPECIALIZATION(unsigned long, __builtin_popcountl)
#endif // __builtin_popcountl
#if __has_builtin(__builtin_popcountll)
ADD_SPECIALIZATION(unsigned long long, __builtin_popcountll)
#endif // __builtin_popcountll
#endif // __builtin_popcountg
#undef ADD_SPECIALIZATION

/// Reverses the bytes in the given integer value.
///
/// All integral types are allowed, matching C++23 std::byteswap semantics.
/// Signed types delegate to the unsigned path via static_cast.
///
/// The recursive decomposition generates optimal 'bswap' or 'rolw'
/// instructions on Clang at -O2 without requiring compiler intrinsics.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_integral_v<T>, T>
byteswap(T value) {
  static_assert(sizeof(T) <= 16, "byteswap: unsupported type size");
  if constexpr (!cpp::is_unsigned_v<T>) {
    using U = cpp::make_unsigned_t<T>;
    return static_cast<T>(byteswap(static_cast<U>(value)));
  } else if constexpr (sizeof(T) == 1) {
    return value;
  } else {
    constexpr unsigned half_bits = sizeof(T) * 8 / 2;
    using Half = cpp::conditional_t<
        sizeof(T) == 2, uint8_t,
        cpp::conditional_t<
            sizeof(T) == 4, uint16_t,
            cpp::conditional_t<sizeof(T) == 8, uint32_t, uint64_t>>>;
    Half lo = static_cast<Half>(value);
    Half hi = static_cast<Half>(value >> half_bits);
    return static_cast<T>((static_cast<T>(byteswap(lo)) << half_bits) |
                          static_cast<T>(byteswap(hi)));
  }
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H
