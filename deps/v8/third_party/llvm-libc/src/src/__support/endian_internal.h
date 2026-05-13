//===-- Endianness support --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ENDIAN_INTERNAL_H
#define LLVM_LIBC_SRC___SUPPORT_ENDIAN_INTERNAL_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// We rely on compiler preprocessor defines to allow for cross compilation.
#ifdef LIBC_COMPILER_IS_MSVC
#define __BYTE_ORDER__ 0
#define __ORDER_LITTLE_ENDIAN__ 0
#define __ORDER_BIG_ENDIAN__ 1
#else // !LIBC_COMPILER_IS_MSVC
#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) ||           \
    !defined(__ORDER_BIG_ENDIAN__)
#error "Missing preprocessor definitions for endianness detection."
#endif
#endif // LIBC_COMPILER_IS_MSVC

namespace internal {

// Converts uint8_t, uint16_t, uint32_t, uint64_t to its big or little endian
// counterpart.
// We use explicit template specialization:
// - to prevent accidental integer promotion.
// - to prevent fallback in (unlikely) case of middle-endianness.

template <unsigned ORDER> struct Endian {
  static constexpr const bool IS_LITTLE = ORDER == __ORDER_LITTLE_ENDIAN__;
  static constexpr const bool IS_BIG = ORDER == __ORDER_BIG_ENDIAN__;
  template <typename T> LIBC_INLINE static T to_big_endian(T value);
  template <typename T> LIBC_INLINE static T to_little_endian(T value);
};

// Little Endian specializations
template <>
template <>
LIBC_INLINE uint8_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_big_endian<uint8_t>(uint8_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint8_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_little_endian<uint8_t>(uint8_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint16_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_big_endian<uint16_t>(uint16_t v) {
  return cpp::byteswap(v);
}
template <>
template <>
LIBC_INLINE uint16_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_little_endian<uint16_t>(uint16_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint32_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_big_endian<uint32_t>(uint32_t v) {
  return cpp::byteswap(v);
}
template <>
template <>
LIBC_INLINE uint32_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_little_endian<uint32_t>(uint32_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint64_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_big_endian<uint64_t>(uint64_t v) {
  return cpp::byteswap(v);
}
template <>
template <>
LIBC_INLINE uint64_t
Endian<__ORDER_LITTLE_ENDIAN__>::to_little_endian<uint64_t>(uint64_t v) {
  return v;
}

// Big Endian specializations
template <>
template <>
LIBC_INLINE uint8_t
Endian<__ORDER_BIG_ENDIAN__>::to_big_endian<uint8_t>(uint8_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint8_t
Endian<__ORDER_BIG_ENDIAN__>::to_little_endian<uint8_t>(uint8_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint16_t
Endian<__ORDER_BIG_ENDIAN__>::to_big_endian<uint16_t>(uint16_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint16_t
Endian<__ORDER_BIG_ENDIAN__>::to_little_endian<uint16_t>(uint16_t v) {
  return cpp::byteswap(v);
}
template <>
template <>
LIBC_INLINE uint32_t
Endian<__ORDER_BIG_ENDIAN__>::to_big_endian<uint32_t>(uint32_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint32_t
Endian<__ORDER_BIG_ENDIAN__>::to_little_endian<uint32_t>(uint32_t v) {
  return cpp::byteswap(v);
}
template <>
template <>
LIBC_INLINE uint64_t
Endian<__ORDER_BIG_ENDIAN__>::to_big_endian<uint64_t>(uint64_t v) {
  return v;
}
template <>
template <>
LIBC_INLINE uint64_t
Endian<__ORDER_BIG_ENDIAN__>::to_little_endian<uint64_t>(uint64_t v) {
  return cpp::byteswap(v);
}

} // namespace internal

using Endian = internal::Endian<__BYTE_ORDER__>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_ENDIAN_INTERNAL_H
