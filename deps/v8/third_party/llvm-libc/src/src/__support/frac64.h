//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of 64-bit unsigned fractional type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FRAC64_H
#define LLVM_LIBC_SRC___SUPPORT_FRAC64_H

#include "src/__support/big_int.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct Frac64 : public UInt<64> {
  using UInt<64>::UInt;

  LIBC_ALWAYS_INLINE constexpr Frac64 operator~() const {
    return Frac64(~val[0]);
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 operator+(Frac64 other) const {
    return Frac64(val[0] + other.val[0]);
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 operator-(Frac64 other) const {
    return Frac64(val[0] - other.val[0]);
  }

  // Fast hi part of the 128-bit product:
  //   0 <= ((a * b) >> 64) - (Frac64(a) * Frac64(b)).val[0] <= 1.
  LIBC_ALWAYS_INLINE constexpr Frac64 operator*(Frac64 other) const {
    return Frac64(UInt<64>::quick_mul_hi(other).val);
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 &operator+=(Frac64 other) {
    *this = *this + other;
    return *this;
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 &operator-=(Frac64 other) {
    *this = *this - other;
    return *this;
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 &operator*=(Frac64 other) {
    *this = *this * other;
    return *this;
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 operator<<(size_t s) const {
    return Frac64(val[0] << s);
  }

  LIBC_ALWAYS_INLINE constexpr Frac64 operator>>(size_t s) const {
    return Frac64(val[0] >> s);
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FRAC64_H
