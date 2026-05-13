//===-- Nearest integer floating-point operations ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEARESTINTEGEROPERATIONS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEARESTINTEGEROPERATIONS_H

#include "FEnvImpl.h"
#include "FPBits.h"
#include "rounding_mode.h"

#include "hdr/math_macros.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T trunc(T x) {
  using StorageType = typename FPBits<T>::StorageType;
  FPBits<T> bits(x);

  // If x is infinity or NaN, return it.
  // If it is zero also we should return it as is, but the logic
  // later in this function takes care of it. But not doing a zero
  // check, we improve the run time of non-zero values.
  if (bits.is_inf_or_nan())
    return x;

  int exponent = bits.get_exponent();

  // If the exponent is greater than the most negative mantissa
  // exponent, then x is already an integer.
  if (exponent >= static_cast<int>(FPBits<T>::FRACTION_LEN))
    return x;

  // If the exponent is such that abs(x) is less than 1, then return 0.
  if (exponent <= -1)
    return FPBits<T>::zero(bits.sign()).get_val();

  int trim_size = FPBits<T>::FRACTION_LEN - exponent;
  StorageType trunc_mantissa =
      static_cast<StorageType>((bits.get_mantissa() >> trim_size) << trim_size);
  bits.set_mantissa(trunc_mantissa);
  return bits.get_val();
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T ceil(T x) {
  using StorageType = typename FPBits<T>::StorageType;
  FPBits<T> bits(x);

  // If x is infinity NaN or zero, return it.
  if (bits.is_inf_or_nan() || bits.is_zero())
    return x;

  bool is_neg = bits.is_neg();
  int exponent = bits.get_exponent();

  // If the exponent is greater than the most negative mantissa
  // exponent, then x is already an integer.
  if (exponent >= static_cast<int>(FPBits<T>::FRACTION_LEN))
    return x;

  if (exponent <= -1) {
    if (is_neg)
      return T(-0.0);
    else
      return T(1.0);
  }

  uint32_t trim_size = FPBits<T>::FRACTION_LEN - exponent;
  StorageType x_u = bits.uintval();
  StorageType trunc_u =
      static_cast<StorageType>((x_u >> trim_size) << trim_size);

  // If x is already an integer, return it.
  if (trunc_u == x_u)
    return x;

  bits.set_uintval(trunc_u);
  T trunc_value = bits.get_val();

  // If x is negative, the ceil operation is equivalent to the trunc operation.
  if (is_neg)
    return trunc_value;

  return trunc_value + T(1.0);
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T floor(T x) {
  FPBits<T> bits(x);
  if (bits.is_neg()) {
    return -ceil(-x);
  } else {
    return trunc(x);
  }
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T round(T x) {
  using StorageType = typename FPBits<T>::StorageType;
  FPBits<T> bits(x);

  // If x is infinity NaN or zero, return it.
  if (bits.is_inf_or_nan() || bits.is_zero())
    return x;

  int exponent = bits.get_exponent();

  // If the exponent is greater than the most negative mantissa
  // exponent, then x is already an integer.
  if (exponent >= static_cast<int>(FPBits<T>::FRACTION_LEN))
    return x;

  if (exponent == -1) {
    // Absolute value of x is greater than equal to 0.5 but less than 1.
    return FPBits<T>::one(bits.sign()).get_val();
  }

  if (exponent <= -2) {
    // Absolute value of x is less than 0.5.
    return FPBits<T>::zero(bits.sign()).get_val();
  }

  uint32_t trim_size = FPBits<T>::FRACTION_LEN - exponent;
  bool half_bit_set =
      bool(bits.get_mantissa() & (StorageType(1) << (trim_size - 1)));
  StorageType x_u = bits.uintval();
  StorageType trunc_u =
      static_cast<StorageType>((x_u >> trim_size) << trim_size);

  // If x is already an integer, return it.
  if (trunc_u == x_u)
    return x;

  bits.set_uintval(trunc_u);
  T trunc_value = bits.get_val();

  if (!half_bit_set) {
    // Franctional part is less than 0.5 so round value is the
    // same as the trunc value.
    return trunc_value;
  } else {
    return bits.is_neg() ? trunc_value - T(1.0) : trunc_value + T(1.0);
  }
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, T>
round_using_specific_rounding_mode(T x, int rnd) {
  using StorageType = typename FPBits<T>::StorageType;
  FPBits<T> bits(x);

  // If x is infinity NaN or zero, return it.
  if (bits.is_inf_or_nan() || bits.is_zero())
    return x;

  bool is_neg = bits.is_neg();
  int exponent = bits.get_exponent();

  // If the exponent is greater than the most negative mantissa
  // exponent, then x is already an integer.
  if (exponent >= static_cast<int>(FPBits<T>::FRACTION_LEN))
    return x;

  if (exponent <= -1) {
    switch (rnd) {
    case FP_INT_DOWNWARD:
      return is_neg ? T(-1.0) : T(0.0);
    case FP_INT_UPWARD:
      return is_neg ? T(-0.0) : T(1.0);
    case FP_INT_TOWARDZERO:
      return is_neg ? T(-0.0) : T(0.0);
    case FP_INT_TONEARESTFROMZERO:
      if (exponent < -1)
        return is_neg ? T(-0.0) : T(0.0); // abs(x) < 0.5
      return is_neg ? T(-1.0) : T(1.0);   // abs(x) >= 0.5
    case FP_INT_TONEAREST:
    default:
      if (exponent <= -2 || bits.get_mantissa() == 0)
        return is_neg ? T(-0.0) : T(0.0); // abs(x) <= 0.5
      else
        return is_neg ? T(-1.0) : T(1.0); // abs(x) > 0.5
    }
  }

  uint32_t trim_size = FPBits<T>::FRACTION_LEN - exponent;
  StorageType x_u = bits.uintval();
  StorageType trunc_u =
      static_cast<StorageType>((x_u >> trim_size) << trim_size);

  // If x is already an integer, return it.
  if (trunc_u == x_u)
    return x;

  FPBits<T> new_bits(trunc_u);
  T trunc_value = new_bits.get_val();

  StorageType trim_value =
      bits.get_mantissa() &
      static_cast<StorageType>(((StorageType(1) << trim_size) - 1));
  StorageType half_value =
      static_cast<StorageType>((StorageType(1) << (trim_size - 1)));
  // If exponent is 0, trimSize will be equal to the mantissa width, and
  // truncIsOdd` will not be correct. So, we handle it as a special case
  // below.
  StorageType trunc_is_odd =
      new_bits.get_mantissa() & (StorageType(1) << trim_size);

  switch (rnd) {
  case FP_INT_DOWNWARD:
    return is_neg ? trunc_value - T(1.0) : trunc_value;
  case FP_INT_UPWARD:
    return is_neg ? trunc_value : trunc_value + T(1.0);
  case FP_INT_TOWARDZERO:
    return trunc_value;
  case FP_INT_TONEARESTFROMZERO:
    if (trim_value >= half_value)
      return is_neg ? trunc_value - T(1.0) : trunc_value + T(1.0);
    return trunc_value;
  case FP_INT_TONEAREST:
  default:
    if (trim_value > half_value) {
      return is_neg ? trunc_value - T(1.0) : trunc_value + T(1.0);
    } else if (trim_value == half_value) {
      if (exponent == 0)
        return is_neg ? T(-2.0) : T(2.0);
      if (trunc_is_odd)
        return is_neg ? trunc_value - T(1.0) : trunc_value + T(1.0);
      else
        return trunc_value;
    } else {
      return trunc_value;
    }
  }
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, T>
round_using_current_rounding_mode(T x) {
  int rounding_mode = quick_get_round();

  switch (rounding_mode) {
  case FE_DOWNWARD:
    return round_using_specific_rounding_mode(x, FP_INT_DOWNWARD);
  case FE_UPWARD:
    return round_using_specific_rounding_mode(x, FP_INT_UPWARD);
  case FE_TOWARDZERO:
    return round_using_specific_rounding_mode(x, FP_INT_TOWARDZERO);
  case FE_TONEAREST:
    return round_using_specific_rounding_mode(x, FP_INT_TONEAREST);
  default:
    __builtin_unreachable();
  }
}

template <bool IsSigned, typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, T>
fromfp(T x, int rnd, unsigned int width) {
  using StorageType = typename FPBits<T>::StorageType;

  constexpr StorageType EXPLICIT_BIT =
      FPBits<T>::SIG_MASK - FPBits<T>::FRACTION_MASK;

  if (width == 0U) {
    raise_except_if_required(FE_INVALID);
    return FPBits<T>::quiet_nan().get_val();
  }

  FPBits<T> bits(x);

  if (bits.is_inf_or_nan()) {
    raise_except_if_required(FE_INVALID);
    return FPBits<T>::quiet_nan().get_val();
  }

  T rounded_value = round_using_specific_rounding_mode(x, rnd);

  if constexpr (IsSigned) {
    // T can't hold a finite number >= 2.0 * 2^EXP_BIAS.
    if (width - 1 > FPBits<T>::EXP_BIAS)
      return rounded_value;

    StorageType range_exp =
        static_cast<StorageType>(width - 1 + FPBits<T>::EXP_BIAS);
    // rounded_value < -2^(width - 1)
    T range_min =
        FPBits<T>::create_value(Sign::NEG, range_exp, EXPLICIT_BIT).get_val();
    if (rounded_value < range_min) {
      raise_except_if_required(FE_INVALID);
      return FPBits<T>::quiet_nan().get_val();
    }
    // rounded_value > 2^(width - 1) - 1
    T range_max =
        FPBits<T>::create_value(Sign::POS, range_exp, EXPLICIT_BIT).get_val() -
        T(1.0);
    if (rounded_value > range_max) {
      raise_except_if_required(FE_INVALID);
      return FPBits<T>::quiet_nan().get_val();
    }

    return rounded_value;
  }

  if (rounded_value < T(0.0)) {
    raise_except_if_required(FE_INVALID);
    return FPBits<T>::quiet_nan().get_val();
  }

  // T can't hold a finite number >= 2.0 * 2^EXP_BIAS.
  if (width > FPBits<T>::EXP_BIAS)
    return rounded_value;

  StorageType range_exp = static_cast<StorageType>(width + FPBits<T>::EXP_BIAS);
  // rounded_value > 2^width - 1
  T range_max =
      FPBits<T>::create_value(Sign::POS, range_exp, EXPLICIT_BIT).get_val() -
      T(1.0);
  if (rounded_value > range_max) {
    raise_except_if_required(FE_INVALID);
    return FPBits<T>::quiet_nan().get_val();
  }

  return rounded_value;
}

template <bool IsSigned, typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<T>, T>
fromfpx(T x, int rnd, unsigned int width) {
  T rounded_value = fromfp<IsSigned>(x, rnd, width);
  FPBits<T> bits(rounded_value);

  if (!bits.is_nan() && rounded_value != x)
    raise_except_if_required(FE_INEXACT);

  return rounded_value;
}

namespace internal {

template <typename FloatType, typename IntType,
          cpp::enable_if_t<cpp::is_floating_point_v<FloatType> &&
                               cpp::is_integral_v<IntType>,
                           int> = 0>
LIBC_INLINE constexpr IntType rounded_float_to_signed_integer(FloatType x) {
  constexpr IntType INTEGER_MIN = (IntType(1) << (sizeof(IntType) * 8 - 1));
  constexpr IntType INTEGER_MAX = -(INTEGER_MIN + 1);
  FPBits<FloatType> bits(x);
  auto set_domain_error_and_raise_invalid = []() {
    set_errno_if_required(EDOM);
    raise_except_if_required(FE_INVALID);
  };

  if (bits.is_inf_or_nan()) {
    set_domain_error_and_raise_invalid();
    return bits.is_neg() ? INTEGER_MIN : INTEGER_MAX;
  }

  int exponent = bits.get_exponent();
  constexpr int EXPONENT_LIMIT = sizeof(IntType) * 8 - 1;
  if (exponent > EXPONENT_LIMIT) {
    set_domain_error_and_raise_invalid();
    return bits.is_neg() ? INTEGER_MIN : INTEGER_MAX;
  } else if (exponent == EXPONENT_LIMIT) {
    if (bits.is_pos() || bits.get_mantissa() != 0) {
      set_domain_error_and_raise_invalid();
      return bits.is_neg() ? INTEGER_MIN : INTEGER_MAX;
    }
    // If the control reaches here, then it means that the rounded
    // value is the most negative number for the signed integer type IntType.
  }

  // For all other cases, if `x` can fit in the integer type `IntType`,
  // we just return `x`. static_cast will convert the floating
  // point value to the exact integer value.
  return static_cast<IntType>(x);
}

} // namespace internal

template <typename FloatType, typename IntType,
          cpp::enable_if_t<cpp::is_floating_point_v<FloatType> &&
                               cpp::is_integral_v<IntType>,
                           int> = 0>
LIBC_INLINE constexpr IntType round_to_signed_integer(FloatType x) {
  return internal::rounded_float_to_signed_integer<FloatType, IntType>(
      round(x));
}

template <typename FloatType, typename IntType,
          cpp::enable_if_t<cpp::is_floating_point_v<FloatType> &&
                               cpp::is_integral_v<IntType>,
                           int> = 0>
LIBC_INLINE constexpr IntType
round_to_signed_integer_using_current_rounding_mode(FloatType x) {
  return internal::rounded_float_to_signed_integer<FloatType, IntType>(
      round_using_current_rounding_mode(x));
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_NEARESTINTEGEROPERATIONS_H
