//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of 256-bit unsigned fractional type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FRAC256_H
#define LLVM_LIBC_SRC___SUPPORT_FRAC256_H

#include "big_int.h"
#include "frac128.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct Frac256 : public UInt<256> {
  using UInt<256>::UInt;

  // Convert Frac256 number to Frac128 with truncation.
  LIBC_INLINE constexpr Frac128 to_frac128() const {
    return Frac128({val[2], val[3]});
  }

  LIBC_INLINE constexpr explicit operator Frac128() const {
    return to_frac128();
  }

  // Convert Frac256 number to Frac64 with truncation.
  LIBC_INLINE constexpr Frac64 to_frac64() const { return Frac64(val[3]); }

  LIBC_INLINE constexpr explicit operator Frac64() const { return to_frac64(); }

  LIBC_INLINE constexpr Frac256 operator~() const {
    Frac256 r{};
    r.val[0] = ~val[0];
    r.val[1] = ~val[1];
    r.val[2] = ~val[2];
    r.val[3] = ~val[3];
    return r;
  }

  LIBC_INLINE constexpr Frac256 operator+(const Frac256 &other) const {
    UInt<256> r = UInt<256>(*this) + UInt<256>(other);
    return Frac256(r.val);
  }

  LIBC_INLINE constexpr Frac256 operator-(const Frac256 &other) const {
    UInt<256> r = UInt<256>(*this) - UInt<256>(other);
    return Frac256(r.val);
  }

  LIBC_INLINE constexpr Frac256 operator*(const Frac256 &other) const {
    UInt<256> r = UInt<256>::quick_mul_hi(UInt<256>(other));
    return Frac256(r.val);
  }

  LIBC_INLINE constexpr Frac256 &operator+=(const Frac256 &other) {
    *this = *this + other;
    return *this;
  }

  LIBC_INLINE constexpr Frac256 &operator-=(const Frac256 &other) {
    *this = *this - other;
    return *this;
  }

  LIBC_INLINE constexpr Frac256 &operator*=(const Frac256 &other) {
    *this = *this * other;
    return *this;
  }

  LIBC_INLINE constexpr Frac256 operator<<(size_t s) const {
    return Frac256((UInt<256>(*this) << s).val);
  }

  LIBC_INLINE constexpr Frac256 operator>>(size_t s) const {
    return Frac256((UInt<256>(*this) >> s).val);
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FRAC256_H
