//===-- 128-bit unsigned fractional type -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FRAC128_H
#define LLVM_LIBC_SRC___SUPPORT_FRAC128_H

#include "big_int.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct Frac128 : public UInt<128> {
  using UInt<128>::UInt;

  LIBC_INLINE constexpr Frac128 operator~() const {
    Frac128 r;
    r.val[0] = ~val[0];
    r.val[1] = ~val[1];
    return r;
  }

  LIBC_INLINE constexpr Frac128 operator+(const Frac128 &other) const {
    UInt<128> r = UInt<128>(*this) + (UInt<128>(other));
    return Frac128(r.val);
  }

  LIBC_INLINE constexpr Frac128 operator-(const Frac128 &other) const {
    UInt<128> r = UInt<128>(*this) - (UInt<128>(other));
    return Frac128(r.val);
  }

  LIBC_INLINE constexpr Frac128 operator*(const Frac128 &other) const {
    UInt<128> r = UInt<128>::quick_mul_hi(UInt<128>(other));
    return Frac128(r.val);
  }

  LIBC_INLINE constexpr Frac128 &operator+=(const Frac128 &other) {
    *this = *this + other;
    return *this;
  }

  LIBC_INLINE constexpr Frac128 &operator-=(const Frac128 &other) {
    *this = *this - other;
    return *this;
  }

  LIBC_INLINE constexpr Frac128 &operator*=(const Frac128 &other) {
    *this = *this * other;
    return *this;
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FRAC128_H
