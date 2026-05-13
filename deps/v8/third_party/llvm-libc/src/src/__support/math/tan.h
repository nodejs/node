//===-- Double-precision tan function -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TAN_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TAN_H

#include "hdr/errno_macros.h"
#include "range_reduction_double_common.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"            // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h" // LIBC_TARGET_CPU_HAS_FMA

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#include "range_reduction_double_fma.h"
#else
#include "range_reduction_double_nofma.h"
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace tan_internal {

using DoubleDouble = fputil::DoubleDouble;
using Float128 = typename fputil::DyadicFloat<128>;

LIBC_INLINE double tan_eval(const DoubleDouble &u, DoubleDouble &result) {
  // Evaluate tan(y) = tan(x - k * (pi/128))
  // We use the degree-9 Taylor approximation:
  //   tan(y) ~ P(y) = y + y^3/3 + 2*y^5/15 + 17*y^7/315 + 62*y^9/2835
  // Then the error is bounded by:
  //   |tan(y) - P(y)| < 2^-6 * |y|^11 < 2^-6 * 2^-66 = 2^-72.
  // For y ~ u_hi + u_lo, fully expanding the polynomial and drop any terms
  // < ulp(u_hi^3) gives us:
  //   P(y) = y + y^3/3 + 2*y^5/15 + 17*y^7/315 + 62*y^9/2835 = ...
  // ~ u_hi + u_hi^3 * (1/3 + u_hi^2 * (2/15 + u_hi^2 * (17/315 +
  //                                                     + u_hi^2 * 62/2835))) +
  //        + u_lo (1 + u_hi^2 * (1 + u_hi^2 * 2/3))
  double u_hi_sq = u.hi * u.hi; // Error < ulp(u_hi^2) < 2^(-6 - 52) = 2^-58.
  // p1 ~ 17/315 + u_hi^2 62 / 2835.
  double p1 =
      fputil::multiply_add(u_hi_sq, 0x1.664f4882c10fap-6, 0x1.ba1ba1ba1ba1cp-5);
  // p2 ~ 1/3 + u_hi^2 2 / 15.
  double p2 =
      fputil::multiply_add(u_hi_sq, 0x1.1111111111111p-3, 0x1.5555555555555p-2);
  // q1 ~ 1 + u_hi^2 * 2/3.
  double q1 = fputil::multiply_add(u_hi_sq, 0x1.5555555555555p-1, 1.0);
  double u_hi_3 = u_hi_sq * u.hi;
  double u_hi_4 = u_hi_sq * u_hi_sq;
  // p3 ~ 1/3 + u_hi^2 * (2/15 + u_hi^2 * (17/315 + u_hi^2 * 62/2835))
  double p3 = fputil::multiply_add(u_hi_4, p1, p2);
  // q2 ~ 1 + u_hi^2 * (1 + u_hi^2 * 2/3)
  double q2 = fputil::multiply_add(u_hi_sq, q1, 1.0);
  double tan_lo = fputil::multiply_add(u_hi_3, p3, u.lo * q2);
  // Overall, |tan(y) - (u_hi + tan_lo)| < ulp(u_hi^3) <= 2^-71.
  // And the relative errors is:
  // |(tan(y) - (u_hi + tan_lo)) / tan(y) | <= 2*ulp(u_hi^2) < 2^-64
  result = fputil::exact_add(u.hi, tan_lo);
  return fputil::multiply_add(fputil::FPBits<double>(u_hi_3).abs().get_val(),
                              0x1.0p-51, 0x1.0p-102);
}

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
// Accurate evaluation of tan for small u.
[[maybe_unused]] LIBC_INLINE Float128 tan_eval(const Float128 &u) {
  Float128 u_sq = fputil::quick_mul(u, u);

  // tan(x) ~ x + x^3/3 + x^5 * 2/15 + x^7 * 17/315 + x^9 * 62/2835 +
  //          + x^11 * 1382/155925 + x^13 * 21844/6081075 +
  //          + x^15 * 929569/638512875 + x^17 * 6404582/10854718875
  // Relative errors < 2^-127 for |u| < pi/256.
  constexpr Float128 TAN_COEFFS[] = {
      {Sign::POS, -127, 0x80000000'00000000'00000000'00000000_u128}, // 1
      {Sign::POS, -129, 0xaaaaaaaa'aaaaaaaa'aaaaaaaa'aaaaaaab_u128}, // 1
      {Sign::POS, -130, 0x88888888'88888888'88888888'88888889_u128}, // 2/15
      {Sign::POS, -132, 0xdd0dd0dd'0dd0dd0d'd0dd0dd0'dd0dd0dd_u128}, // 17/315
      {Sign::POS, -133, 0xb327a441'6087cf99'6b5dd24e'ec0b327a_u128}, // 62/2835
      {Sign::POS, -134,
       0x91371aaf'3611e47a'da8e1cba'7d900eca_u128}, // 1382/155925
      {Sign::POS, -136,
       0xeb69e870'abeefdaf'e606d2e4'd1e65fbc_u128}, // 21844/6081075
      {Sign::POS, -137,
       0xbed1b229'5baf15b5'0ec9af45'a2619971_u128}, // 929569/638512875
      {Sign::POS, -138,
       0x9aac1240'1b3a2291'1b2ac7e3'e4627d0a_u128}, // 6404582/10854718875
  };

  return fputil::quick_mul(
      u, fputil::polyeval(u_sq, TAN_COEFFS[0], TAN_COEFFS[1], TAN_COEFFS[2],
                          TAN_COEFFS[3], TAN_COEFFS[4], TAN_COEFFS[5],
                          TAN_COEFFS[6], TAN_COEFFS[7], TAN_COEFFS[8]));
}

// Calculation a / b = a * (1/b) for Float128.
// Using the initial approximation of q ~ (1/b), then apply 2 Newton-Raphson
// iterations, before multiplying by a.
[[maybe_unused]] Float128 newton_raphson_div(const Float128 &a, Float128 b,
                                             double q) {
  Float128 q0(q);
  constexpr Float128 TWO(2.0);
  b.sign = (b.sign == Sign::POS) ? Sign::NEG : Sign::POS;
  Float128 q1 =
      fputil::quick_mul(q0, fputil::quick_add(TWO, fputil::quick_mul(b, q0)));
  Float128 q2 =
      fputil::quick_mul(q1, fputil::quick_add(TWO, fputil::quick_mul(b, q1)));
  return fputil::quick_mul(a, q2);
}
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace tan_internal

LIBC_INLINE double tan(double x) {
  using namespace tan_internal;
  using namespace math::range_reduction_double_internal;
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint16_t x_e = xbits.get_biased_exponent();

  DoubleDouble y;
  unsigned k;
  LargeRangeReduction range_reduction_large{};

  // |x| < 2^16
  if (LIBC_LIKELY(x_e < FPBits::EXP_BIAS + FAST_PASS_EXPONENT)) {
    // |x| < 2^-7
    if (LIBC_UNLIKELY(x_e < FPBits::EXP_BIAS - 7)) {
      // |x| < 2^-27, |tan(x) - x| < ulp(x)/2.
      if (LIBC_UNLIKELY(x_e < FPBits::EXP_BIAS - 27)) {
        // Signed zeros.
        if (LIBC_UNLIKELY(x == 0.0))
          return x + x; // Make sure it works with FTZ/DAZ.

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
        return fputil::multiply_add(x, 0x1.0p-54, x);
#else
        if (LIBC_UNLIKELY(x_e < 4)) {
          int rounding_mode = fputil::quick_get_round();
          if ((xbits.sign() == Sign::POS && rounding_mode == FE_UPWARD) ||
              (xbits.sign() == Sign::NEG && rounding_mode == FE_DOWNWARD))
            return FPBits(xbits.uintval() + 1).get_val();
        }
        return fputil::multiply_add(x, 0x1.0p-54, x);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
      }
      // No range reduction needed.
      k = 0;
      y.lo = 0.0;
      y.hi = x;
    } else {
      // Small range reduction.
      k = range_reduction_small(x, y);
    }
  } else {
    // Inf or NaN
    if (LIBC_UNLIKELY(x_e > 2 * FPBits::EXP_BIAS)) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // tan(+-Inf) = NaN
      if (xbits.get_mantissa() == 0) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
      }
      return x + FPBits::quiet_nan().get_val();
    }

    // Large range reduction.
    k = range_reduction_large.fast(x, y);
  }

  DoubleDouble tan_y;
  [[maybe_unused]] double err = tan_eval(y, tan_y);

  // Look up sin(k * pi/128) and cos(k * pi/128)
#ifdef LIBC_MATH_HAS_SMALL_TABLES
  // Memory saving versions. Use 65-entry table:
  auto get_idx_dd = [](unsigned kk) -> DoubleDouble {
    unsigned idx = (kk & 64) ? 64 - (kk & 63) : (kk & 63);
    DoubleDouble ans = SIN_K_PI_OVER_128[idx];
    if (kk & 128) {
      ans.hi = -ans.hi;
      ans.lo = -ans.lo;
    }
    return ans;
  };
  DoubleDouble msin_k = get_idx_dd(k + 128);
  DoubleDouble cos_k = get_idx_dd(k + 64);
#else
  // Fast look up version, but needs 256-entry table.
  // cos(k * pi/128) = sin(k * pi/128 + pi/2) = sin((k + 64) * pi/128).
  DoubleDouble msin_k = SIN_K_PI_OVER_128[(k + 128) & 255];
  DoubleDouble cos_k = SIN_K_PI_OVER_128[(k + 64) & 255];
#endif // LIBC_MATH_HAS_SMALL_TABLES

  // After range reduction, k = round(x * 128 / pi) and y = x - k * (pi / 128).
  // So k is an integer and -pi / 256 <= y <= pi / 256.
  // Then tan(x) = sin(x) / cos(x)
  //             = sin((k * pi/128 + y) / cos((k * pi/128 + y)
  //             = (cos(y) * sin(k*pi/128) + sin(y) * cos(k*pi/128)) /
  //               / (cos(y) * cos(k*pi/128) - sin(y) * sin(k*pi/128))
  //             = (sin(k*pi/128) + tan(y) * cos(k*pi/128)) /
  //               / (cos(k*pi/128) - tan(y) * sin(k*pi/128))
  DoubleDouble cos_k_tan_y = fputil::quick_mult(tan_y, cos_k);
  DoubleDouble msin_k_tan_y = fputil::quick_mult(tan_y, msin_k);

  // num_dd = sin(k*pi/128) + tan(y) * cos(k*pi/128)
  DoubleDouble num_dd = fputil::exact_add<false>(cos_k_tan_y.hi, -msin_k.hi);
  // den_dd = cos(k*pi/128) - tan(y) * sin(k*pi/128)
  DoubleDouble den_dd = fputil::exact_add<false>(msin_k_tan_y.hi, cos_k.hi);
  num_dd.lo += cos_k_tan_y.lo - msin_k.lo;
  den_dd.lo += msin_k_tan_y.lo + cos_k.lo;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double tan_x = (num_dd.hi + num_dd.lo) / (den_dd.hi + den_dd.lo);
  return tan_x;
#else
  // Accurate test and pass for correctly rounded implementation.

  // Accurate double-double division
  DoubleDouble tan_x = fputil::div(num_dd, den_dd);

  // Simple error bound: |1 / den_dd| < 2^(1 + floor(-log2(den_dd)))).
  uint64_t den_inv = (static_cast<uint64_t>(FPBits::EXP_BIAS + 1)
                      << (FPBits::FRACTION_LEN + 1)) -
                     (FPBits(den_dd.hi).uintval() & FPBits::EXP_MASK);

  // For tan_x = (num_dd + err) / (den_dd + err), the error is bounded by:
  //   | tan_x - num_dd / den_dd |  <= err * ( 1 + | tan_x * den_dd | ).
  double tan_err =
      err * fputil::multiply_add(FPBits(den_inv).get_val(),
                                 FPBits(tan_x.hi).abs().get_val(), 1.0);

  double err_higher = tan_x.lo + tan_err;
  double err_lower = tan_x.lo - tan_err;

  double tan_upper = tan_x.hi + err_higher;
  double tan_lower = tan_x.hi + err_lower;

  // Ziv's rounding test.
  if (LIBC_LIKELY(tan_upper == tan_lower))
    return tan_upper;

  Float128 u_f128;
  if (LIBC_LIKELY(x_e < FPBits::EXP_BIAS + FAST_PASS_EXPONENT))
    u_f128 = range_reduction_small_f128(x);
  else
    u_f128 = range_reduction_large.accurate();

  Float128 tan_u = tan_eval(u_f128);

  auto get_sin_k = [](unsigned kk) -> Float128 {
    unsigned idx = (kk & 64) ? 64 - (kk & 63) : (kk & 63);
    Float128 ans = SIN_K_PI_OVER_128_F128[idx];
    if (kk & 128)
      ans.sign = Sign::NEG;
    return ans;
  };

  // cos(k * pi/128) = sin(k * pi/128 + pi/2) = sin((k + 64) * pi/128).
  Float128 sin_k_f128 = get_sin_k(k);
  Float128 cos_k_f128 = get_sin_k(k + 64);
  Float128 msin_k_f128 = get_sin_k(k + 128);

  // num_f128 = sin(k*pi/128) + tan(y) * cos(k*pi/128)
  Float128 num_f128 =
      fputil::quick_add(sin_k_f128, fputil::quick_mul(cos_k_f128, tan_u));
  // den_f128 = cos(k*pi/128) - tan(y) * sin(k*pi/128)
  Float128 den_f128 =
      fputil::quick_add(cos_k_f128, fputil::quick_mul(msin_k_f128, tan_u));

  // tan(x) = (sin(k*pi/128) + tan(y) * cos(k*pi/128)) /
  //          / (cos(k*pi/128) - tan(y) * sin(k*pi/128))
  // TODO: The initial seed 1.0/den_dd.hi for Newton-Raphson reciprocal can be
  // reused from DoubleDouble fputil::div in the fast pass.
  Float128 result = newton_raphson_div(num_f128, den_f128, 1.0 / den_dd.hi);

  // TODO: Add assertion if Ziv's accuracy tests fail in debug mode.
  // https://github.com/llvm/llvm-project/issues/96452.
  return static_cast<double>(result);

#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TAN_H
