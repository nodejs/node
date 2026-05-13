//===-- A self contained equivalent of cstddef ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_CSTDDEF_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_CSTDDEF_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "type_traits.h" // For enable_if_t, is_integral_v.

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

enum class byte : unsigned char {};

template <class IntegerType>
LIBC_INLINE constexpr enable_if_t<is_integral_v<IntegerType>, byte>
operator>>(byte b, IntegerType shift) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(b) >> shift);
}
template <class IntegerType>
LIBC_INLINE constexpr enable_if_t<is_integral_v<IntegerType>, byte &>
operator>>=(byte &b, IntegerType shift) noexcept {
  return b = b >> shift;
}
template <class IntegerType>
LIBC_INLINE constexpr enable_if_t<is_integral_v<IntegerType>, byte>
operator<<(byte b, IntegerType shift) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(b) << shift);
}
template <class IntegerType>
LIBC_INLINE constexpr enable_if_t<is_integral_v<IntegerType>, byte &>
operator<<=(byte &b, IntegerType shift) noexcept {
  return b = b << shift;
}
LIBC_INLINE constexpr byte operator|(byte l, byte r) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(l) |
                           static_cast<unsigned char>(r));
}
LIBC_INLINE constexpr byte &operator|=(byte &l, byte r) noexcept {
  return l = l | r;
}
LIBC_INLINE constexpr byte operator&(byte l, byte r) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(l) &
                           static_cast<unsigned char>(r));
}
LIBC_INLINE constexpr byte &operator&=(byte &l, byte r) noexcept {
  return l = l & r;
}
LIBC_INLINE constexpr byte operator^(byte l, byte r) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(l) ^
                           static_cast<unsigned char>(r));
}
LIBC_INLINE constexpr byte &operator^=(byte &l, byte r) noexcept {
  return l = l ^ r;
}
LIBC_INLINE constexpr byte operator~(byte b) noexcept {
  return static_cast<byte>(~static_cast<unsigned char>(b));
}
template <typename IntegerType>
LIBC_INLINE constexpr enable_if_t<is_integral_v<IntegerType>, IntegerType>
to_integer(byte b) noexcept {
  return static_cast<IntegerType>(b);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_CSTDDEF_H
