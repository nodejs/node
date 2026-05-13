//===-- Floating-point manipulation functions -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_MANIPULATIONFUNCTIONS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_MANIPULATIONFUNCTIONS_H

#include "FPBits.h"
#include "NearestIntegerOperations.h"
#include "NormalFloat.h"
#include "cast.h"
#include "dyadic_float.h"
#include "rounding_mode.h"

#include "hdr/math_macros.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h" // INT_MAX, INT_MIN
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T frexp(T x, int &exp) {
  FPBits<T> bits(x);
  if (bits.is_inf_or_nan()) {
#ifdef LIBC_FREXP_INF_NAN_EXPONENT
    // The value written back to the second parameter when calling
    // frexp/frexpf/frexpl` with `+/-Inf`/`NaN` is unspecified in the standard.
    // Set the exp value for Inf/NaN inputs explicitly to
    // LIBC_FREXP_INF_NAN_EXPONENT if it is defined.
    exp = LIBC_FREXP_INF_NAN_EXPONENT;
#endif // LIBC_FREXP_INF_NAN_EXPONENT
    return x;
  }
  if (bits.is_zero()) {
    exp = 0;
    return x;
  }

  NormalFloat<T> normal(bits);
  exp = normal.exponent + 1;
  normal.exponent = -1;
  return normal;
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T modf(T x, T &iptr) {
  FPBits<T> bits(x);
  if (bits.is_zero() || bits.is_nan()) {
    iptr = x;
    return x;
  } else if (bits.is_inf()) {
    iptr = x;
    return FPBits<T>::zero(bits.sign()).get_val();
  } else {
    iptr = trunc(x);
    if (x == iptr) {
      // If x is already an integer value, then return zero with the right
      // sign.
      return FPBits<T>::zero(bits.sign()).get_val();
    } else {
      return x - iptr;
    }
  }
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T copysign(T x, T y) {
  FPBits<T> xbits(x);
  xbits.set_sign(FPBits<T>(y).sign());
  return xbits.get_val();
}

template <typename T> struct IntLogbConstants;

template <> struct IntLogbConstants<int> {
  LIBC_INLINE_VAR static constexpr int FP_LOGB0 = FP_ILOGB0;
  LIBC_INLINE_VAR static constexpr int FP_LOGBNAN = FP_ILOGBNAN;
  LIBC_INLINE_VAR static constexpr int T_MAX = INT_MAX;
  LIBC_INLINE_VAR static constexpr int T_MIN = INT_MIN;
};

template <> struct IntLogbConstants<long> {
  LIBC_INLINE_VAR static constexpr long FP_LOGB0 = FP_ILOGB0;
  LIBC_INLINE_VAR static constexpr long FP_LOGBNAN = FP_ILOGBNAN;
  LIBC_INLINE_VAR static constexpr long T_MAX = LONG_MAX;
  LIBC_INLINE_VAR static constexpr long T_MIN = LONG_MIN;
};

template <typename T, typename U>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<U>, T>
intlogb(U x) {
  FPBits<U> bits(x);
  if (LIBC_UNLIKELY(bits.is_zero() || bits.is_inf_or_nan())) {
    set_errno_if_required(EDOM);
    raise_except_if_required(FE_INVALID);

    if (bits.is_zero())
      return IntLogbConstants<T>::FP_LOGB0;
    if (bits.is_nan())
      return IntLogbConstants<T>::FP_LOGBNAN;
    // bits is inf.
    return IntLogbConstants<T>::T_MAX;
  }

  DyadicFloat<FPBits<U>::STORAGE_LEN> normal(bits.get_val());
  int exponent = normal.get_unbiased_exponent();
  // The C standard does not specify the return value when an exponent is
  // out of int range. However, XSI conformance required that INT_MAX or
  // INT_MIN are returned.
  // NOTE: It is highly unlikely that exponent will be out of int range as
  // the exponent is only 15 bits wide even for the 128-bit floating point
  // format.
  if (LIBC_UNLIKELY(exponent > IntLogbConstants<T>::T_MAX ||
                    exponent < IntLogbConstants<T>::T_MIN)) {
    set_errno_if_required(ERANGE);
    raise_except_if_required(FE_INVALID);
    return exponent > 0 ? IntLogbConstants<T>::T_MAX
                        : IntLogbConstants<T>::T_MIN;
  }

  return static_cast<T>(exponent);
}

template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T logb(T x) {
  FPBits<T> bits(x);
  if (LIBC_UNLIKELY(bits.is_zero() || bits.is_inf_or_nan())) {
    if (bits.is_nan())
      return x;

    raise_except_if_required(FE_DIVBYZERO);

    if (bits.is_zero()) {
      set_errno_if_required(ERANGE);
      return FPBits<T>::inf(Sign::NEG).get_val();
    }
    // bits is inf.
    return FPBits<T>::inf().get_val();
  }

  DyadicFloat<FPBits<T>::STORAGE_LEN> normal(bits.get_val());
  return static_cast<T>(normal.get_unbiased_exponent());
}

template <typename T, typename U>
LIBC_INLINE constexpr cpp::enable_if_t<
    cpp::is_floating_point_v<T> && cpp::is_integral_v<U>, T>
ldexp(T x, U exp) {
  FPBits<T> bits(x);
  if (LIBC_UNLIKELY((exp == 0) || bits.is_zero() || bits.is_inf_or_nan()))
    return x;

  // NormalFloat uses int32_t to store the true exponent value. We should ensure
  // that adding |exp| to it does not lead to integer rollover. But, if |exp|
  // value is larger the exponent range for type T, then we can return infinity
  // early. Because the result of the ldexp operation can be a subnormal number,
  // we need to accommodate the (mantissaWidth + 1) worth of shift in
  // calculating the limit.
  constexpr int EXP_LIMIT =
      FPBits<T>::MAX_BIASED_EXPONENT + FPBits<T>::FRACTION_LEN + 1;
  // Make sure that we can safely cast exp to int when not returning early.
  static_assert(EXP_LIMIT <= INT_MAX && -EXP_LIMIT >= INT_MIN);
  if (LIBC_UNLIKELY(exp > EXP_LIMIT)) {
    int rounding_mode = quick_get_round();
    Sign sign = bits.sign();

    if ((sign == Sign::POS && rounding_mode == FE_DOWNWARD) ||
        (sign == Sign::NEG && rounding_mode == FE_UPWARD) ||
        (rounding_mode == FE_TOWARDZERO))
      return FPBits<T>::max_normal(sign).get_val();

    set_errno_if_required(ERANGE);
    raise_except_if_required(FE_OVERFLOW);
    return FPBits<T>::inf(sign).get_val();
  }

  // Similarly on the negative side we return zero early if |exp| is too small.
  if (LIBC_UNLIKELY(exp < -EXP_LIMIT)) {
    int rounding_mode = quick_get_round();
    Sign sign = bits.sign();

    if ((sign == Sign::POS && rounding_mode == FE_UPWARD) ||
        (sign == Sign::NEG && rounding_mode == FE_DOWNWARD))
      return FPBits<T>::min_subnormal(sign).get_val();

    set_errno_if_required(ERANGE);
    raise_except_if_required(FE_UNDERFLOW);
    return FPBits<T>::zero(sign).get_val();
  }

  // For all other values, NormalFloat to T conversion handles it the right way.
  DyadicFloat<FPBits<T>::STORAGE_LEN> normal(bits.get_val());
  normal.exponent += static_cast<int>(exp);
  // TODO: Add tests for exceptions.
  return normal.template as<T, /*ShouldRaiseExceptions=*/true>();
}

template <typename T, typename U,
          cpp::enable_if_t<cpp::is_floating_point_v<T> &&
                               cpp::is_floating_point_v<U> &&
                               (sizeof(T) <= sizeof(U)),
                           int> = 0>
LIBC_INLINE constexpr T nextafter(T from, U to) {
  FPBits<T> from_bits(from);
  if (from_bits.is_nan())
    return from;

  FPBits<U> to_bits(to);
  if (to_bits.is_nan())
    return cast<T>(to);

  // NOTE: This would work only if `U` has a greater or equal precision than
  // `T`. Otherwise `from` could loose its precision and the following statement
  // could incorrectly evaluate to `true`.
  if (cast<U>(from) == to)
    return cast<T>(to);

  using StorageType = typename FPBits<T>::StorageType;
  if (from != T(0)) {
    if ((cast<U>(from) < to) == (from > T(0))) {
      from_bits = FPBits<T>(StorageType(from_bits.uintval() + 1));
    } else {
      from_bits = FPBits<T>(StorageType(from_bits.uintval() - 1));
    }
  } else {
    from_bits = FPBits<T>::min_subnormal(to_bits.sign());
  }

  if (from_bits.is_subnormal())
    raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);
  else if (from_bits.is_inf())
    raise_except_if_required(FE_OVERFLOW | FE_INEXACT);

  return from_bits.get_val();
}

template <bool IsDown, typename T,
          cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE constexpr T nextupdown(T x) {
  constexpr Sign sign = IsDown ? Sign::NEG : Sign::POS;

  FPBits<T> xbits(x);
  if (xbits.is_nan() || xbits == FPBits<T>::max_normal(sign) ||
      xbits == FPBits<T>::inf(sign))
    return x;

  using StorageType = typename FPBits<T>::StorageType;
  if (x != T(0)) {
    if (xbits.sign() == sign) {
      xbits = FPBits<T>(StorageType(xbits.uintval() + 1));
    } else {
      xbits = FPBits<T>(StorageType(xbits.uintval() - 1));
    }
  } else {
    xbits = FPBits<T>::min_subnormal(sign);
  }

  return xbits.get_val();
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
#include "x86_64/NextAfterLongDouble.h"
#include "x86_64/NextUpDownLongDouble.h"
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_MANIPULATIONFUNCTIONS_H
