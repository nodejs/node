//===-- Implementation header for hypot -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_HYPOT_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_HYPOT_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/Hypot.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS

namespace hypot_internal {

// Both input are denormals and non-zero.  We scale them up and down by the
// inverse of the smallest normal number: 2^-1022, so that the correct "hidden"
// bit position is now 1.  And by adding and subtracting 1 from the results, we
// can emulate the rounding behavior in the denormal range.
LIBC_INLINE double hypot_denorm(double a, double b) {
  using fputil::DoubleDouble;
  constexpr double SCALE = 0x1.0p1022;
  constexpr double SCALE_BACK = 0x1.0p-1022;

  a *= SCALE;
  b *= SCALE;

  // See the comments in the main function for the detail explanation of the
  // computations.

  // sum.hi + sum.lo ~ a^2 + b^2.
  DoubleDouble a_sq = fputil::exact_mult(a, a);
  DoubleDouble b_sq = fputil::exact_mult(b, b);
  DoubleDouble sum = fputil::exact_add(a_sq.hi, b_sq.hi);
  sum.lo += a_sq.lo + b_sq.lo;

  // |sqrt(sum.hi) - r_hi| < 2^-52.
  double r_hi = fputil::sqrt<double>(sum.hi);
  // r_inv ~ 1 / (2 * r_hi)
  double r_inv = 0.5 / r_hi;
  // Adjust correction if needed.
  DoubleDouble r_h{0.0, r_hi};
  double correction = 0.0;
  if (r_hi < 1.0) {
    // When r_hi < 1, the output is denormal.  We mimick rounding in denormal
    // range with 1.0 + r_hi.
    r_h = fputil::exact_add(1.0, r_hi);
    correction = 1.0;
  }
  // r_hi^2
  DoubleDouble r_sq = fputil::exact_mult(r_hi, r_hi);
  // (hi + lo - r_hi^2)
  double num_lo = (sum.lo - r_sq.lo) - (r_sq.hi - sum.hi);
  // (hi + lo - r_hi^2) / (2 * r_hi)
  double r_lo = fputil::multiply_add(num_lo, r_inv, r_h.lo);

  constexpr double ERR = 0x1.0p-102;

  // Ziv's rounding test.
  double upper = r_h.hi + (r_lo + ERR);
  double lower = r_h.hi + (r_lo - ERR);

  if (LIBC_LIKELY(upper == lower)) {
#ifdef LIBC_MATH_HAS_NO_EXCEPT
    return (upper - correction) * SCALE_BACK;
#else
    // Check to raise underflow correctly.
    DoubleDouble r = fputil::exact_add(r_h.hi, r_lo);
    r.hi -= correction;
    // Raise underflow if needed:
    if ((r.hi < 1.0 && r.lo != 0.0) || (r.hi == 1.0 && r.lo < 0.0))
      fputil::raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);

    return r.hi * SCALE_BACK;
#endif // LIBC_MATH_HAS_NO_EXCEPT
  }

  return fputil::hypot(a * SCALE_BACK, b * SCALE_BACK);
}

} // namespace hypot_internal

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

LIBC_INLINE double hypot(double x, double y) {
  using FPBits = fputil::FPBits<double>;
  using DoubleDouble = fputil::DoubleDouble;

  uint64_t x_u = FPBits(x).uintval();
  uint64_t y_u = FPBits(y).uintval();

  // Shift the exponent field to the top 11 bits of the lower 32-bit.
  // Casting it to 32-bit effectively remove the sign bit.
  uint32_t x_e = static_cast<uint32_t>(x_u >> 31);
  uint32_t y_e = static_cast<uint32_t>(y_u >> 31);

  // a = maximum_mag(x, y);
  // b = minimum_mag(x, y);
  double a, b;
  uint32_t a_e, b_e;

  if (x_e >= y_e) {
    a_e = x_e;
    b_e = y_e;
    a = x;
    b = y;
  } else {
    a_e = y_e;
    b_e = x_e;
    a = y;
    b = x;
  }

  double scale = 1.0;
  double scale_back = 1.0;

  // For a_e, b_e, the top 11 bits are exponent fields.
  if (LIBC_UNLIKELY(a_e >= ((500U + FPBits::EXP_BIAS) << (32 - 11)))) {
    // The larger magnitude is above 2^500 (or Inf/NaN), need to scale down to
    // prevent overflow when squaring.
    if (a_e >= static_cast<uint32_t>(FPBits::EXP_MASK >> 31)) {
      // Inf or NaN;
      FPBits x_bits(x);
      FPBits y_bits(y);
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      if (x_bits.is_inf() || y_bits.is_inf())
        return FPBits::inf().get_val();
      if (x_bits.is_nan())
        return x;
      return y;
    }

    // Check the exponent gap here so that all the follow up pre-scaling and
    // overflow check won't generate spurious underflow exceptions.
    if (LIBC_UNLIKELY(a_e - b_e >= (54U << (32 - 11)))) {
      double x_abs = FPBits(x_u & FPBits::EXP_SIG_MASK).get_val();
      double y_abs = FPBits(y_u & FPBits::EXP_SIG_MASK).get_val();
      return x_abs + y_abs;
    }
    //  Any scaling factor < 2^(-1024/2) = 2^-512 would work.
    scale = 0x1.0p-600;
    scale_back = 0x1.0p600;
    a *= scale;
    b *= scale;
    // Check for overflow to raise the exception correctly.
#if !defined(LIBC_MATH_HAS_NO_EXCEPT)
    // No overflow when calculating a^2 + b^2.
    double asq = a * a;
    double bsq = b * b;
    double sumsq = asq + bsq;
    // Overflow happens when:
    //   2^600 * sqrt(a^2 + b^2) >= 2^1023 * (2 - 2^-53)
    // Which is equivalent to:
    //   sqrt(a^2 + b^2) >= 2^424 * (1 - 2^-54).
    // Square both sides:
    //   a^2 + b^2 >= 2^848 * (1 - 2^-53 + 2^-108).
    // For a fast sufficient condition that can be done in double precision:
    //   a^2 + b^2 >= 2^848.
    if (sumsq >= 0x1.0p848)
      return sumsq * scale_back;
#endif // !LIBC_MATH_HAS_NO_EXCEPT
  } else if (LIBC_UNLIKELY(b_e <= ((FPBits::EXP_BIAS - 400) << (32 - 11)))) {
    // The smaller magnitude is below 2^-400 (or 0), need to scale up to prevent
    // underflow when squaring.
    if (LIBC_UNLIKELY(a_e < (1U << (32 - 11)))) {
      // Larger input is denormal, extra care is needed to perform the Ziv's
      // accuracy test correctly as double-rounding errors might happen.
      if ((x == 0.0) || (y == 0.0)) {
        double x_abs = FPBits(x_u & FPBits::EXP_SIG_MASK).get_val();
        double y_abs = FPBits(y_u & FPBits::EXP_SIG_MASK).get_val();
        return x_abs + y_abs;
      }
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return hypot_internal::hypot_denorm(a, b);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }
    //  Any scaling factor > 2^((1072 + 52)/2) = 2^562 would work.
    scale = 0x1.0p600;
    scale_back = 0x1.0p-600;
    a *= scale;
    b *= scale;
  }

  // When the gap in the exponent of `a` and `b` is >= 54,
  //   |b| < ufp(a) * 2^(-53) = ulp(a)/2
  // So:
  //   hypot(x, y) =  sqrt(a^2 + b^2)
  //               <= sqrt( (|a| + |b|)^2 )
  //               = |a| + |b|
  //               < |a| + ulp(a)
  //  Hence, we can return:
  //    |a| + |b| = |x| + |y|
  //  to perform correct rounding to all rounding modes.
  if (LIBC_UNLIKELY(a_e - b_e >= (54U << (32 - 11)))) {
    double x_abs = FPBits(x_u & FPBits::EXP_SIG_MASK).get_val();
    double y_abs = FPBits(y_u & FPBits::EXP_SIG_MASK).get_val();
    return x_abs + y_abs;
  }

  // sum.hi + sum.lo ~ a^2 + b^2.
  DoubleDouble a_sq = fputil::exact_mult(a, a);
  DoubleDouble b_sq = fputil::exact_mult(b, b);
  DoubleDouble sum = fputil::exact_add(a_sq.hi, b_sq.hi);
  sum.lo += a_sq.lo + b_sq.lo;

  // Let hi = sum.hi and lo = sum.lo.
  // To compute r_hi + r_lo ~ sqrt(hi + lo):
  // - First we use fast sqrt instruction to get:
  //     r_hi ~ sqrt(hi)
  // - Then use Taylor expansion:
  //     f(hi + lo) = f(hi) + f'(hi) * lo + f''(hi) * lo^2 / 2 + ...
  //   with f(x) = sqrt(x):
  //     sqrt(hi + lo) ~ sqrt(hi) + lo / (2 * sqrt(hi)).
  // - Subtract by r_hi to find the correction term:
  //     sqrt(hi + lo) - r_hi ~ (sqrt(hi) - r_hi) + lo / (2 * sqrt(hi))
  // - Instead of finding the rounding errors sqrt(hi) - r_hi, we use the
  //   squared residual d = hi - r_hi^2, which can be calculated accurately in
  //   double-double.  Then, using the same Taylor approximation of sqrt(x) as
  //   above:
  //     sqrt(hi) - r_hi = sqrt(r_hi^2 + d) - r_hi
  //                     ~ sqrt(r_hi^2) + d / (2 * sqrt(r_hi^2)) - r_hi
  //                     = d / (2 * r_hi).
  // - Similarly,
  //     1 / sqrt(hi) = 1 / sqrt(r_hi^2 + d)
  //                  ~ 1 / sqrt(r_hi^2) - d / (2 * (r_hi^2)^(3/2))
  //                  = 1 / r_hi - d / (2 * r_hi^3)
  // - Putting them together, we have the correction term:
  //     sqrt(hi + lo) - r_hi + lo / (2 * sqrt(hi)) ~
  //   ~ (lo + d) / (2 * r_hi) + lo * d / (4 * r_hi^3)
  //   ~ (hi + lo - r_hi^2) / (2 * r_hi).
  // - When computing hi + lo - r_hi^2, we will pair (hi - r_sq.hi) and
  //   (lo - r_sq.lo), since `r_sq.hi` is very close to `hi`, and the
  //   subtraction is exact.
  // - Taking intermediate roundings with directed rounding modes into
  //   consideration, the overall errors should be bounded by
  //     (2^-51)^2 = 2^-102.

  // |sqrt(sum.hi) - r_hi| < 2^-52.
  double r_hi = fputil::sqrt<double>(sum.hi);
  // r_inv ~ 1 / (2 * r_hi)
  double r_inv = 0.5 / r_hi;
  // r_hi^2
  DoubleDouble r_sq = fputil::exact_mult(r_hi, r_hi);
  // (hi + lo - r_hi^2)
  double num_lo = (sum.lo - r_sq.lo) - (r_sq.hi - sum.hi);
  // (hi + lo - r_hi^2) / (2 * r_hi)
  double r_lo = num_lo * r_inv;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  // TODO: What's the worst error if we just do:
  //   return sqrt(a*a + b*b) * scale_back;
  // without all the double-double computations?
  return (r_hi + r_lo) * scale_back;
#else
  constexpr double ERR = 0x1.0p-102;

  // Ziv's rounding test.
  double upper = r_hi + fputil::multiply_add(r_hi, ERR, r_lo);
  double lower = r_hi + fputil::multiply_add(r_hi, -ERR, r_lo);

  if (LIBC_LIKELY(upper == lower)) {
    return upper * scale_back;
  }

  return fputil::hypot(x, y);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_HYPOT_H
