//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of 128-bit unsigned fractional type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FRAC128_H
#define LLVM_LIBC_SRC___SUPPORT_FRAC128_H

#include "big_int.h"
#include "frac64.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct Frac128 : public UInt<128> {
  using UInt<128>::UInt;

  // Convert Frac128 number to Frac64 with truncation.
  LIBC_INLINE constexpr Frac64 to_frac64() const { return Frac64(val[1]); }

  LIBC_INLINE constexpr explicit operator Frac64() const { return to_frac64(); }

  LIBC_INLINE constexpr Frac128 operator~() const {
    Frac128 r{};
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

  LIBC_INLINE constexpr Frac128 operator<<(size_t s) const {
    return Frac128((UInt<128>(*this) << s).val);
  }

  LIBC_INLINE constexpr Frac128 operator>>(size_t s) const {
    return Frac128((UInt<128>(*this) >> s).val);
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FRAC128_H
