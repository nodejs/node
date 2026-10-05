//===-- Definition for Float80 data type ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_FLOAT80_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_FLOAT80_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/comparison_operations.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/float128.h"
#include "src/__support/FPUtil/generic/add_sub.h"
#include "src/__support/FPUtil/generic/div.h"
#include "src/__support/FPUtil/generic/mul.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

struct Float80 {
  UInt128 bits;

  LIBC_INLINE Float80() = default;
  LIBC_INLINE constexpr Float80(const Float80 &) = default;
  LIBC_INLINE constexpr Float80(Float80 &&) = default;
  LIBC_INLINE constexpr Float80 &operator=(const Float80 &) = default;
  LIBC_INLINE constexpr Float80 &operator=(Float80 &&) = default;

  // Floating point type and integer type
  template <typename T>
  LIBC_INLINE constexpr explicit Float80(T value) : bits(0U) {
    if constexpr (cpp::is_floating_point_v<T>) {
      bits = fputil::cast<Float80>(value).bits;
    } else if constexpr (cpp::is_integral_v<T>) {
      Sign sign = Sign::POS;
      auto unsigned_value = static_cast<cpp::make_unsigned_t<T>>(value);

      if constexpr (cpp::is_signed_v<T>) {
        if (value < 0) {
          sign = Sign::NEG;
          unsigned_value = -unsigned_value;
        }
      }

      fputil::DyadicFloat<FPBits<Float80>::STORAGE_LEN> xd(sign, 0,
                                                           unsigned_value);
      bits = xd.template as<Float80, /*ShouldSignalExceptions=*/true>().bits;

    } else if constexpr (cpp::is_convertible_v<T, Float80>) {
      bits = value.operator Float80().bits;
    } else {
      bits = fputil::cast<Float80>(static_cast<float>(value)).bits;
    }
  }

  template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T> &&
                                             !cpp::is_same_v<T, Float80>,
                                         int> = 0>
  LIBC_INLINE LIBC_CONSTEXPR_DEFAULT operator T() const {
    return fputil::cast<T>(*this);
  }

  template <typename T, cpp::enable_if_t<cpp::is_integral_v<T>, int> = 0>
  LIBC_INLINE constexpr explicit operator T() const {
    constexpr T MIN_T = cpp::numeric_limits<T>::min();
    constexpr T MAX_T = cpp::numeric_limits<T>::max();
    FPBits<Float80> x_bits(*this);
    // Raise FE_INVALID for inf and NaN
    if (x_bits.is_inf_or_nan()) {
      raise_except_if_required(FE_INVALID);
      return x_bits.is_neg() ? MIN_T : MAX_T;
    }
    int exponent = x_bits.get_explicit_exponent();
    constexpr int EXPONENT_LIMIT = cpp::numeric_limits<T>::digits;
    if (exponent > EXPONENT_LIMIT) {
      raise_except_if_required(FE_INVALID);
      return x_bits.is_neg() ? MIN_T : MAX_T;
    } else if (exponent == EXPONENT_LIMIT) {
      if (x_bits.is_pos() || x_bits.get_mantissa() != 0) {
        raise_except_if_required(FE_INVALID);
        return x_bits.is_neg() ? MIN_T : MAX_T;
      }
    }

    int x_bits_exp = exponent - FPBits<Float80>::FRACTION_LEN;
    // sign * 2^(exp-bias) * mantissa
    DyadicFloat<FPBits<Float80>::STORAGE_LEN> xd(
        x_bits.sign(), x_bits_exp, x_bits.get_explicit_mantissa());
    return static_cast<T>(xd.as_mantissa_type());
  }

  // unary operator
  LIBC_INLINE LIBC_BIT_CAST_CONSTEXPR Float80 operator-() const {
    fputil::FPBits<Float80> result(*this);
    result.set_sign(result.is_pos() ? Sign::NEG : Sign::POS);
    return result.get_val();
  }

  LIBC_INLINE constexpr Float80 operator+(const Float80 &other) const {
    return fputil::generic::add<Float80>(fputil::cast<Float128>(*this),
                                         fputil::cast<Float128>(other));
  }

  LIBC_INLINE constexpr Float80 operator-(const Float80 &other) const {
    return fputil::generic::sub<Float80>(fputil::cast<Float128>(*this),
                                         fputil::cast<Float128>(other));
  }

  LIBC_INLINE constexpr Float80 operator*(const Float80 &other) const {
    return fputil::generic::mul<Float80>(fputil::cast<Float128>(*this),
                                         fputil::cast<Float128>(other));
  }

  LIBC_INLINE constexpr Float80 operator/(const Float80 &other) const {
    return fputil::generic::div<Float80>(fputil::cast<Float128>(*this),
                                         fputil::cast<Float128>(other));
  }

  // Comparison operators
  LIBC_INLINE constexpr bool operator==(const Float80 &other) const {
    return fputil::equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator!=(const Float80 &other) const {
    return !fputil::equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator<(const Float80 &other) const {
    return fputil::less_than(*this, other);
  }

  LIBC_INLINE constexpr bool operator<=(const Float80 &other) const {
    return fputil::less_than_or_equals(*this, other);
  }

  LIBC_INLINE constexpr bool operator>(const Float80 &other) const {
    return fputil::greater_than(*this, other);
  }

  LIBC_INLINE constexpr bool operator>=(const Float80 &other) const {
    return fputil::greater_than_or_equals(*this, other);
  }
};

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_Float80_H
