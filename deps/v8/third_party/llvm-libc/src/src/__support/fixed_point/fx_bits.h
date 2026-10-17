//===-- Utility class to manipulate fixed point numbers. --*- C++ -*-=========//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FIXED_POINT_FX_BITS_H
#define LLVM_LIBC_SRC___SUPPORT_FIXED_POINT_FX_BITS_H

#include "include/llvm-libc-macros/stdfix-macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h" // numeric_limits
#include "src/__support/CPP/type_traits.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"       // LIBC_NAMESPACE_DECL
#include "src/__support/macros/null_check.h"   // LIBC_CRASH_ON_VALUE
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/math_extras.h"
#include "src/__support/uint128.h"

#include "fx_rep.h"

#ifdef LIBC_COMPILER_HAS_FIXED_POINT

namespace LIBC_NAMESPACE_DECL {
namespace fixed_point {

template <typename T> struct FXBits {
private:
  using fx_rep = FXRep<T>;
  using StorageType = typename fx_rep::StorageType;

  StorageType value;

  static_assert(fx_rep::FRACTION_LEN > 0);

  static constexpr size_t FRACTION_OFFSET = 0; // Just for completeness
  static constexpr size_t INTEGRAL_OFFSET =
      fx_rep::INTEGRAL_LEN == 0 ? 0 : fx_rep::FRACTION_LEN;
  static constexpr size_t SIGN_OFFSET =
      fx_rep::SIGN_LEN == 0
          ? 0
          : ((sizeof(StorageType) * CHAR_BIT) - fx_rep::SIGN_LEN);

  static constexpr StorageType FRACTION_MASK =
      mask_trailing_ones<StorageType, fx_rep::FRACTION_LEN>()
      << FRACTION_OFFSET;
  static constexpr StorageType INTEGRAL_MASK =
      mask_trailing_ones<StorageType, fx_rep::INTEGRAL_LEN>()
      << INTEGRAL_OFFSET;
  static constexpr StorageType SIGN_MASK =
      (fx_rep::SIGN_LEN == 0 ? 0 : StorageType(1) << SIGN_OFFSET);

  // mask for <integral | fraction>
  static constexpr StorageType VALUE_MASK = INTEGRAL_MASK | FRACTION_MASK;

  // mask for <sign | integral | fraction>
  static constexpr StorageType TOTAL_MASK = SIGN_MASK | VALUE_MASK;

public:
  LIBC_INLINE constexpr FXBits() = default;

  template <typename XType> LIBC_INLINE constexpr explicit FXBits(XType x) {
    using Unqual = typename cpp::remove_cv_t<XType>;
    if constexpr (cpp::is_same_v<Unqual, T>) {
      value = cpp::bit_cast<StorageType>(x);
    } else if constexpr (cpp::is_same_v<Unqual, StorageType>) {
      value = x;
    } else {
      // We don't want accidental type promotions/conversions, so we require
      // exact type match.
      static_assert(cpp::always_false<XType>);
    }
  }

  LIBC_INLINE constexpr StorageType get_fraction() {
    return (value & FRACTION_MASK) >> FRACTION_OFFSET;
  }

  LIBC_INLINE constexpr StorageType get_integral() {
    return (value & INTEGRAL_MASK) >> INTEGRAL_OFFSET;
  }

  // returns complete bitstring representation the fixed point number
  // the bitstring is of the form: padding | sign | integral | fraction
  LIBC_INLINE constexpr StorageType get_bits() {
    return (value & TOTAL_MASK) >> FRACTION_OFFSET;
  }

  // TODO: replace bool with Sign
  LIBC_INLINE constexpr bool get_sign() {
    return static_cast<bool>((value & SIGN_MASK) >> SIGN_OFFSET);
  }

  // This represents the effective negative exponent applied to this number
  LIBC_INLINE constexpr int get_exponent() { return fx_rep::FRACTION_LEN; }

  LIBC_INLINE constexpr void set_fraction(StorageType fraction) {
    value = (value & (~FRACTION_MASK)) |
            ((fraction << FRACTION_OFFSET) & FRACTION_MASK);
  }

  LIBC_INLINE constexpr void set_integral(StorageType integral) {
    value = (value & (~INTEGRAL_MASK)) |
            ((integral << INTEGRAL_OFFSET) & INTEGRAL_MASK);
  }

  // TODO: replace bool with Sign
  LIBC_INLINE constexpr void set_sign(bool sign) {
    value = (value & (~SIGN_MASK)) |
            ((static_cast<StorageType>(sign) << SIGN_OFFSET) & SIGN_MASK);
  }

  LIBC_INLINE constexpr T get_val() const { return cpp::bit_cast<T>(value); }
};

// Bit-wise operations are not available for fixed point types yet.
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, T>
bit_and(T x, T y) {
  using BitType = typename FXRep<T>::StorageType;
  BitType x_bit = cpp::bit_cast<BitType>(x);
  BitType y_bit = cpp::bit_cast<BitType>(y);
  // For some reason, bit_cast cannot deduce BitType from the input.
  return cpp::bit_cast<T, BitType>(x_bit & y_bit);
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, T>
bit_or(T x, T y) {
  using BitType = typename FXRep<T>::StorageType;
  BitType x_bit = cpp::bit_cast<BitType>(x);
  BitType y_bit = cpp::bit_cast<BitType>(y);
  // For some reason, bit_cast cannot deduce BitType from the input.
  return cpp::bit_cast<T, BitType>(x_bit | y_bit);
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, T>
bit_not(T x) {
  using BitType = typename FXRep<T>::StorageType;
  BitType x_bit = cpp::bit_cast<BitType>(x);
  // For some reason, bit_cast cannot deduce BitType from the input.
  return cpp::bit_cast<T, BitType>(static_cast<BitType>(~x_bit));
}

template <typename T> LIBC_INLINE constexpr T abs(T x) {
  using FXRep = FXRep<T>;
  if constexpr (FXRep::SIGN_LEN == 0)
    return x;
  else {
    if (LIBC_UNLIKELY(x == FXRep::MIN()))
      return FXRep::MAX();
    return (x < FXRep::ZERO() ? -x : x);
  }
}

// Round-to-nearest, tie-to-(+Inf)
template <typename T> LIBC_INLINE constexpr T round(T x, int n) {
  using FXRep = FXRep<T>;
  if (LIBC_UNLIKELY(n < 0))
    n = 0;
  if (LIBC_UNLIKELY(n >= FXRep::FRACTION_LEN))
    return x;

  T round_bit = FXRep::EPS() << (FXRep::FRACTION_LEN - n - 1);
  // Check for overflow.
  if (LIBC_UNLIKELY(FXRep::MAX() - round_bit < x))
    return FXRep::MAX();

  T all_ones = bit_not(FXRep::ZERO());

  int shift = FXRep::FRACTION_LEN - n;
  T rounding_mask =
      (shift == FXRep::TOTAL_LEN) ? FXRep::ZERO() : (all_ones << shift);
  return bit_and((x + round_bit), rounding_mask);
}

// count leading sign bits
// TODO: support fixed_point_padding
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, int>
countls(T f) {
  using FXRep = FXRep<T>;
  using BitType = typename FXRep::StorageType;
  using FXBits = FXBits<T>;

  if constexpr (FXRep::SIGN_LEN > 0) {
    if (f < 0)
      f = bit_not(f);
  }

  BitType value_bits = FXBits(f).get_bits();
  return cpp::countl_zero(value_bits) - FXRep::SIGN_LEN;
}

// fixed-point to integer conversion
template <typename T, typename XType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, XType>
bitsfx(T f) {
  return cpp::bit_cast<XType, T>(f);
}

// divide the two fixed-point types and return an integer result
template <typename T, typename XType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<T>, XType>
idiv(T x, T y) {
  using FXBits = FXBits<T>;
  using FXRep = FXRep<T>;
  using CompType = typename FXRep::CompType;

  // If the value of the second operand of the / operator is zero, the
  // behavior is undefined. Ref: ISO/IEC TR 18037:2008(E) p.g. 16
  LIBC_CRASH_ON_VALUE(y, FXRep::ZERO());

  CompType x_comp = static_cast<CompType>(FXBits(x).get_bits());
  CompType y_comp = static_cast<CompType>(FXBits(y).get_bits());

  // If an integer result of one of these functions overflows, the behavior is
  // undefined. Ref: ISO/IEC TR 18037:2008(E) p.g. 16
  CompType result = x_comp / y_comp;

  return static_cast<XType>(result);
}

// Divide two integers and return a fixed-point value.
// For reference, see:
// https://en.wikipedia.org/wiki/Division_algorithm#Newton%E2%80%93Raphson_division
// https://stackoverflow.com/a/9231996.
template <typename FXType, typename IntType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<FXType>, FXType>
fxdivi(IntType n, IntType d) {
  using OutRep = FXRep<FXType>;
  static_assert(cpp::is_signed_v<IntType> == (OutRep::SIGN_LEN > 0),
                "IntType and FXType must have matching signedness");
  constexpr bool IS_SIGNED = OutRep::SIGN_LEN > 0;
  using UIntType = cpp::make_unsigned_t<IntType>;

  // If the value of the second operand of the / operator is zero, the
  // behavior is undefined. Ref: ISO/IEC TR 18037:2008(E) p.g. 16.
  LIBC_CRASH_ON_VALUE(d, 0);
  if (LIBC_UNLIKELY(n == 0))
    return OutRep::ZERO();

  // n == d and d != 0 means the quotient is 1. The general NR path can't
  // guarantee landing on 1 exactly so special case for this.
  if (LIBC_UNLIKELY(n == d)) {
    if constexpr (OutRep::INTEGRAL_LEN > 0)
      return static_cast<FXType>(1);
    else
      return OutRep::MAX();
  }

  // Intermediate arithmetic is done in a wide fixed-point type.
  using WideFXType =
      cpp::conditional_t<IS_SIGNED, long accum, unsigned long accum>;
  using WideRep = FXRep<WideFXType>;
  using WideStorage = typename WideRep::StorageType;
  constexpr int F = OutRep::FRACTION_LEN;
  constexpr int WF = WideRep::FRACTION_LEN;

  // Split each operand into a sign and a magnitude.
  bool result_is_negative = false;
  UIntType n_mag, d_mag;
  if constexpr (IS_SIGNED) {
    result_is_negative = (n < 0) != (d < 0);
    n_mag = (n < 0) ? -static_cast<UIntType>(n) : static_cast<UIntType>(n);
    d_mag = (d < 0) ? -static_cast<UIntType>(d) : static_cast<UIntType>(d);
  } else {
    n_mag = n;
    d_mag = d;
  }

  if constexpr (OutRep::INTEGRAL_LEN > 0) {
    constexpr UInt128 MAX_UNITS = static_cast<UInt128>(1)
                                  << OutRep::INTEGRAL_LEN;

    if (LIBC_UNLIKELY(static_cast<UInt128>(n_mag) >=
                      MAX_UNITS * static_cast<UInt128>(d_mag)))
      return result_is_negative ? OutRep::MIN() : OutRep::MAX();
  }

  WideFXType res;

  constexpr int INTERMEDIATE_BITS = cpp::numeric_limits<UIntType>::digits + WF;
  using WideIntType =
      cpp::conditional_t<(INTERMEDIATE_BITS <= 64), uint64_t, UInt128>;

  if ((d_mag & (d_mag - 1)) == 0) {
    // d is a power of 2. n/d is an exact right shift.
    int log2_d = cpp::countr_zero(d_mag);

    WideIntType scaled_n = (static_cast<WideIntType>(n_mag) << WF) >> log2_d;

    constexpr int WIDE_STORAGE_BITS = cpp::numeric_limits<WideStorage>::digits;
    if (LIBC_UNLIKELY((static_cast<UInt128>(scaled_n) >> WIDE_STORAGE_BITS) !=
                      0))
      return result_is_negative ? OutRep::MIN() : OutRep::MAX();

    res = FXBits<WideFXType>(static_cast<WideStorage>(scaled_n)).get_val();
  } else if constexpr (WF <= F) {
    WideIntType wide_n = static_cast<WideIntType>(n_mag) << WF;
    WideIntType quotient = wide_n / static_cast<WideIntType>(d_mag);
    res = FXBits<WideFXType>(static_cast<WideStorage>(quotient)).get_val();
  } else {
    // General case: Approximate 1/d, then multiply by n.

    // Normalize d_mag into a WF fraction value in [0.5, 1) and apply the same
    // shift to n_mag so that n_scaled/d_scaled = n/d.
    constexpr int W = cpp::numeric_limits<UIntType>::digits;
    int d_msb = (W - 1) - cpp::countl_zero(d_mag);
    int norm_shift = (WF - 1) - d_msb;

    auto scale = [norm_shift](UIntType v) -> WideFXType {
      WideStorage wide_v = static_cast<WideStorage>(v);
      WideStorage shifted =
          norm_shift >= 0 ? (wide_v << norm_shift) : (wide_v >> -norm_shift);
      return FXBits<WideFXType>(shifted).get_val();
    };

    WideFXType d_scaled = scale(d_mag);
    WideFXType n_scaled = scale(n_mag);

    // Initial approximation of 1/d_scaled: x0 = 48/17 - (32/17) * d_scaled.
    // d_scaled is in [0.5, 1) so x0 is in [0.941, 1.882] with a worst-case
    // relative error bounded by 1/17 (~5.88%).
    WideFXType a = static_cast<WideFXType>(0x2.d89d89d8p0lk); // 48/17
    WideFXType b = static_cast<WideFXType>(0x1.e1e1e1e1p0lk); // 32/17
    WideFXType initial_approx = a - b * d_scaled;

    auto nrstep = [](WideFXType d_, WideFXType x0) {
      return x0 * (static_cast<WideFXType>(2) - d_ * x0);
    };

    // Each iteration squares the relative error (quadratic convergence).
    WideFXType recip = nrstep(d_scaled, initial_approx); // E1 <= 0.346%

    if constexpr (F >= 7)
      recip = nrstep(d_scaled, recip); // E2 <= 1.197e-5

    if constexpr (F >= 15)
      recip = nrstep(d_scaled, recip); // E3 <= 1.434e-10

    res = n_scaled * recip;
  }

  if constexpr (IS_SIGNED) {
    if (result_is_negative)
      res = -res;
  }

  // According to clause 7.18a.6.1, saturate the result on overflow.
  WideFXType max_val = static_cast<WideFXType>(OutRep::MAX());
  if (res > max_val)
    return OutRep::MAX();
  if constexpr (IS_SIGNED) {
    WideFXType min_val = static_cast<WideFXType>(OutRep::MIN());
    if (res < min_val)
      return OutRep::MIN();
  }

  return static_cast<FXType>(res);
}

// Divide an integer operand by a fixed-point operand and return the
// mathematically exact result as an IntType rounded towards 0. Assumes
// signedness of IntType matches the signedness of FXType.
template <typename IntType, typename FXType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_fixed_point_v<FXType>, IntType>
divifx(IntType n, FXType d) {
  using FXBits = FXBits<FXType>;
  using FXRep = FXRep<FXType>;
  using CompType = typename FXRep::CompType;

  static_assert(cpp::is_signed_v<IntType> == (FXRep::SIGN_LEN > 0),
                "IntType and FXType must have matching signedness");

  // UB if denominator is 0.
  LIBC_CRASH_ON_VALUE(d, FXRep::ZERO());

  if (LIBC_UNLIKELY(n == 0)) {
    return static_cast<IntType>(0);
  }

  CompType d_comp = static_cast<CompType>(FXBits(d).get_bits());

  constexpr int F = FXRep::FRACTION_LEN;

  constexpr int INTERMEDIATE_BITS =
      sizeof(IntType) * 8 - cpp::is_signed_v<IntType> + F;

  using WideType = cpp::conditional_t<
      cpp::is_signed_v<IntType>,
      cpp::conditional_t<INTERMEDIATE_BITS <= 64, int64_t, Int128>,
      cpp::conditional_t<INTERMEDIATE_BITS <= 64, uint64_t, UInt128>>;

  WideType scaled_n = static_cast<WideType>(n) << F;
  WideType result = scaled_n / static_cast<WideType>(d_comp);

  return static_cast<IntType>(result);
}

} // namespace fixed_point
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_COMPILER_HAS_FIXED_POINT

#endif // LLVM_LIBC_SRC___SUPPORT_FIXED_POINT_FX_BITS_H
