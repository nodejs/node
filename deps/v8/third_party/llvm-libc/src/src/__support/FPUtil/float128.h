//===-- Definition for Float128 data type -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_FLOAT128_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_FLOAT128_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/comparison_operations.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/generic/add_sub.h"
#include "src/__support/FPUtil/generic/div.h"
#include "src/__support/FPUtil/generic/mul.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

struct Float128 {
  UInt128 bits;

  LIBC_INLINE Float128() = default;
  LIBC_INLINE constexpr Float128(const Float128 &) = default;
  LIBC_INLINE constexpr Float128(Float128 &&) = default;
  LIBC_INLINE constexpr Float128 &operator=(const Float128 &) = default;
  LIBC_INLINE constexpr Float128 &operator=(Float128 &&) = default;

  // Floating point type and integer type
  template <typename T>
  LIBC_INLINE constexpr explicit Float128(T value) : bits(0U) {
    if constexpr (cpp::is_floating_point_v<T>) {
      bits = fputil::cast<Float128>(value).bits;
    } else if constexpr (cpp::is_integral_v<T>) {
      Sign sign = Sign::POS;
      auto unsigned_value = static_cast<cpp::make_unsigned_t<T>>(value);

      if constexpr (cpp::is_signed_v<T>) {
        if (value < 0) {
          sign = Sign::NEG;
          unsigned_value = -unsigned_value;
        }
      }

      fputil::DyadicFloat<FPBits<Float128>::STORAGE_LEN> xd(sign, 0,
                                                            unsigned_value);
      bits = xd.template as<Float128, /*ShouldSignalExceptions=*/true>().bits;

    } else if constexpr (cpp::is_convertible_v<T, Float128>) {
      bits = value.operator Float128().bits;
    } else {
      bits = fputil::cast<Float128>(static_cast<float>(value)).bits;
    }
  }

  template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T> &&
                                             !cpp::is_same_v<T, Float128>,
                                         int> = 0>
  LIBC_INLINE LIBC_CONSTEXPR_DEFAULT operator T() const {
    return fputil::cast<T>(*this);
  }

  template <typename T, cpp::enable_if_t<cpp::is_integral_v<T>, int> = 0>
  LIBC_INLINE constexpr explicit operator T() const {
    FPBits<Float128> x_bits(*this);
    // Raise FE_INVALID for inf and NaN
    if (x_bits.is_inf_or_nan()) {
      raise_except_if_required(FE_INVALID);
    }
    int x_bits_exp =
        x_bits.get_explicit_exponent() - FPBits<Float128>::FRACTION_LEN;
    // sign * 2^(exp-bias) * mantissa
    DyadicFloat<FPBits<Float128>::STORAGE_LEN> xd(
        x_bits.sign(), x_bits_exp, x_bits.get_explicit_mantissa());
    return static_cast<T>(xd.as_mantissa_type());
  }

  // unary
  LIBC_INLINE LIBC_BIT_CAST_CONSTEXPR Float128 operator-() const {
    fputil::FPBits<Float128> result(*this);
    result.set_sign(result.is_pos() ? Sign::NEG : Sign::POS);
    return result.get_val();
  }
  // operator overloads
  LIBC_INLINE constexpr Float128 operator+(const Float128 &other) const {
    return fputil::generic::add<Float128>(*this, other);
  }

  LIBC_INLINE constexpr Float128 operator-(const Float128 &other) const {
    return fputil::generic::sub<Float128>(*this, other);
  }

  LIBC_INLINE constexpr Float128 operator*(const Float128 &other) const {
    return fputil::generic::mul<Float128>(*this, other);
  }

  LIBC_INLINE constexpr Float128 operator/(const Float128 &other) const {
    return fputil::generic::div<Float128>(*this, other);
  }

  LIBC_INLINE constexpr Float128 &operator*=(const Float128 &other) {
    *this = *this * other;
    return *this;
  }

  LIBC_INLINE constexpr Float128 &operator+=(const Float128 &other) {
    *this = *this + other;
    return *this;
  }

  LIBC_INLINE constexpr Float128 &operator-=(const Float128 &other) {
    *this = *this - other;
    return *this;
  }

  LIBC_INLINE constexpr Float128 &operator/=(const Float128 &other) {
    *this = *this / other;
    return *this;
  }

  LIBC_INLINE constexpr bool operator==(const Float128 &other) const {
    return fputil::equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator!=(const Float128 &other) const {
    return !fputil::equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator<(const Float128 &other) const {
    return fputil::less_than(*this, other);
  }

  LIBC_INLINE constexpr bool operator<=(const Float128 &other) const {
    return fputil::less_than_or_equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator>(const Float128 &other) const {
    return fputil::greater_than(*this, other);
  }

  LIBC_INLINE constexpr bool operator>=(const Float128 &other) const {
    return fputil::greater_than_or_equals(*this, other);
  }
};

static_assert(LIBC_NAMESPACE::cpp::is_trivially_constructible<
              LIBC_NAMESPACE::fputil::Float128>::value);
static_assert(LIBC_NAMESPACE::cpp::is_trivially_copyable<
              LIBC_NAMESPACE::fputil::Float128>::value);

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_FLOAT128_H
