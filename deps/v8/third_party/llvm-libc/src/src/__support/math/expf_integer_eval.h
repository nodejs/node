//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the statically rounded, integer-only implementation of
/// expf(x)
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_INTEGER_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_INTEGER_EVAL_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/frac64.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/check/exp_exceptions.h"

namespace LIBC_NAMESPACE_DECL {

namespace shared {

namespace math {

namespace static_rounding {

// print(2+round(1/log(2), 64, RN));
// LSB(INV_LN2) = 2^-63
LIBC_INLINE_VAR constexpr Frac64 INV_LN2 = Frac64(0xb8aa'3b29'5c17'f0bc);

// 64-bit polynomial approximation of 2^x coefficients generated with Sollya:
// > P = fpminimax(2^x, 11, [|1, 64...|], [0, 1], absolute, fixed);
// Store the fractional part of the coefficients below
// > dirtyinfnorm(2^x - P(x), [0, 1]);
// 0x1.6238...p-58
// LSB(EXPF_COEFFS[i]) = 2^-64
LIBC_INLINE_VAR constexpr Frac64 EXPF_COEFFS[] = {
    Frac64(0xb172'17f7'd1cf'b7cf), // x
    Frac64(0x3d7f'7bff'057d'4a5e), // x^2
    Frac64(0x0e35'846b'8363'9484), // x^3
    Frac64(0x0276'556d'ec97'dcd4), // x^4
    Frac64(0x0057'61ff'dc04'c7ff), // x^5
    Frac64(0x000a'1847'b6e7'92ec), // x^6
    Frac64(0x0000'ffe8'14e5'7033), // x^7
    Frac64(0x0000'1628'b6e9'70c8), // x^8
    Frac64(0x0000'01b8'8ce7'4088), // x^9
    Frac64(0x0000'001c'18d5'cb29), // x^10
    Frac64(0x0000'0002'b43f'4490), // x^11
};

// Statically rounded, no except implementation of expf using integer-only
// arithmetic.
LIBC_INLINE float expf(float x, [[maybe_unused]] int rounding) {
  using FPBits = typename fputil::FPBits<float>;
  using FPBounds = LIBC_NAMESPACE::math::check::exp_internal::Bounds<float>;
  FPBits xbits(x);

  bool is_neg = xbits.is_neg();
  uint32_t x_val = xbits.uintval();
  uint32_t x_val_abs = x_val & 0x7fff'ffffU;

  // When |x| >= smallest value that will cause overflow, |x| <= 2^-25, or x is
  // NaN
  if (LIBC_UNLIKELY(x_val_abs >= FPBounds::UPPER_BITS ||
                    x_val_abs <= 0x3300'0000U)) {
    // |x| <= 2^-25
    if (x_val_abs <= 0x3300'0000U) {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      return 1.0f;
#else
      if (x_val_abs == 0)
        return 1.0f;

      if (rounding == FE_UPWARD && !is_neg)
        return 0x1.000002p0f;

      if ((rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO) && is_neg)
        return 0x1.fffffep-1f;

      return 1.0f;
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    }

    if (xbits.is_nan()) {
      // Per conversation with lntue, we don't need to raise exception here,
      // as we're assuming no FPUs/fenv in this kind of environment
      if (xbits.is_signaling_nan()) {
        // silencing
        return FPBits::quiet_nan().get_val();
      }

      // quiet NaN
      return x;
    }

    // e^-inf = 0
    // e^+inf = +inf
    if (xbits.is_inf()) {
      return is_neg ? 0.0f : FPBits::inf().get_val();
    }

    // Large finite positive
    if (!is_neg) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      if (rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO)
        return FPBits::max_normal().get_val();

      return FPBits::inf().get_val();
#else
      return FPBits::inf().get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    }

    // x < log(2^-150) or NaN (NaN is already handled above)
    if (xbits.uintval() >= 0xc2cf'f1b5U) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      if (rounding == FE_UPWARD)
        return FPBits::min_subnormal().get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY

      return 0.0f;
    }
  }

  // Main calculations

  uint16_t x_e = xbits.get_biased_exponent();
  uint64_t x_u = xbits.get_mantissa();

  // Range reduction
  // The algorithm near the end of this function estimates 2^r,
  // where r is the fractional part of x * log2(e) and is in [0, 1].
  // See EXPF_COEFFS for more details on the approximation polynomial used.

  // add leading bit = 1
  x_u |= uint64_t(1) << FPBits::FRACTION_LEN;

  // shift to top 32 bit --> decimal point at hidden bit that we've added
  x_u <<= 32;

  int x_e_unbiased = static_cast<int>(x_e) - FPBits::EXP_BIAS;

  // shift for the decimal point to be at the hidden bit
  if (x_e_unbiased > 0) {
    x_u <<= x_e_unbiased;
  } else if (x_e_unbiased < 0) {
    x_u >>= -x_e_unbiased;
  }

  // LSB(x_u_frac) = 2^-55
  Frac64 x_u_frac(x_u);

  // LSB(x_ln2) = 2^-54
  Frac64 x_ln2 = x_u_frac * INV_LN2;

  constexpr uint64_t FRAC_MASK = (uint64_t(1) << 54) - 1;
  uint64_t x_ln2_bit = x_ln2.val[0];

  uint64_t e_y, l2y_r;
  uint32_t e_y_unbiased;

  // As Frac64 can't store the sign, we need to handle the sign separately:
  // - Both branches are computing floor(x * log2(e)).
  // - For negative x, we round up to the next multiple of 2^54, then clear the
  // last 54 bits.
  // - For positive x, we round down (just clear) the last 54 bits.
  //
  // Then, l2y_r is the remainder of x * log2(e) after removing the integer
  // part, which is used to compute 2^l2y_r_frac - 1.
  //
  // e_y_unbiased is biased exponent field, but already bit-positioned to the
  // exponent field of the float representation.
  if (LIBC_UNLIKELY(is_neg)) {
    e_y = (x_ln2_bit + FRAC_MASK) & ~FRAC_MASK;
    l2y_r = e_y - x_ln2_bit;
    e_y_unbiased = (FPBits::EXP_BIAS << 23) - static_cast<uint32_t>(e_y >> 31);
  } else {
    e_y = x_ln2_bit & ~FRAC_MASK;
    l2y_r = x_ln2_bit - e_y;
    e_y_unbiased = (FPBits::EXP_BIAS << 23) + static_cast<uint32_t>(e_y >> 31);
  }

  uint32_t k = static_cast<uint32_t>(e_y >> 54);
  int d = static_cast<int>(k) - FPBits::EXP_BIAS;

  // d >= 24 --> k >= 151
  // --> guaranteed to below 2^-150
  //
  // underflow
  if (LIBC_UNLIKELY(is_neg && d >= 24)) {
    return 0.0f;
  }

  // LSB(l2y_r_frac) = LSB(l2y_r) * 2^-10 = 2^-64
  Frac64 l2y_r_frac(l2y_r << 10);

  // p = 2^l2y_r_frac - 1
  Frac64 p = l2y_r_frac *
             fputil::polyeval(l2y_r_frac, EXPF_COEFFS[0], EXPF_COEFFS[1],
                              EXPF_COEFFS[2], EXPF_COEFFS[3], EXPF_COEFFS[4],
                              EXPF_COEFFS[5], EXPF_COEFFS[6], EXPF_COEFFS[7],
                              EXPF_COEFFS[8], EXPF_COEFFS[9], EXPF_COEFFS[10]);

  uint32_t shift_length = 40;
  uint32_t leading_one = 0;

  // We're computing with errors < worst-cast errors, so tie-rounding never
  // happens. Hence, round-to-nearest, tie-to-even is equivalent to
  // round-to-nearest, tie-to-away. Which is what we're implementing below
  // in the following order:
  //
  // 1. Shift so that the rounding bit is at bit-0
  // 2. Add 1 for rounding
  // 3. Perform another shift by 1
  // 4. Depending on the rounding modes, adjust accoringly:
  //  a. Add 1 if rounding-up (0 if not)
  //  b. Add e_y_unbiased to the result (0 if the result is subnormal)

  // subnormal
  if (LIBC_UNLIKELY(is_neg && d >= 0)) {
    e_y_unbiased = 0;
    leading_one = 1 << (23 - d);

    // In the below shifts, we're shifting by (shift_length + 1) at max, while
    // shift_length is already 40, and if d = 23 --> shift_length + d = 63, and
    // we'll shift by whole 64 bits, which is undefined behavior in C++.
    //
    // So, we'll truncate the last 2 bits.
    if (d >= 22) {
      d -= 2;
      p.val[0] >>= 2;
    }

    shift_length += d + 1;
  }

#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  uint32_t result =
      (static_cast<uint32_t>(p.val[0] >> shift_length) + (leading_one + 1));
  result >>= 1;
  result += e_y_unbiased;

  return cpp::bit_cast<float>(result);
#else
  if (rounding == FE_TONEAREST) {
    uint32_t result =
        (static_cast<uint32_t>(p.val[0] >> shift_length) + (leading_one + 1));
    result >>= 1;
    result += e_y_unbiased;

    return cpp::bit_cast<float>(result);
  }

  uint32_t should_round_up = 0;

  if (LIBC_UNLIKELY(rounding == FE_UPWARD)) {
    uint64_t round_up_mask = (uint64_t(1) << (shift_length + 1)) - 1;
    should_round_up = static_cast<uint32_t>((p.val[0] & round_up_mask) != 0);
  }

  uint32_t result = (static_cast<uint32_t>(p.val[0] >> (shift_length + 1)) +
                     should_round_up + (leading_one >> 1));
  result += e_y_unbiased;

  return cpp::bit_cast<float>(result);
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

} // namespace static_rounding

} // namespace math

} // namespace shared

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_INTEGER_EVAL_H
