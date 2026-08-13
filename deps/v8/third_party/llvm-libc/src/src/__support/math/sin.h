//===-- Implementation header for sin ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SIN_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SIN_H

#include "range_reduction_double_common.h"
#include "sincos_eval.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
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

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
LIBC_INLINE double
sin_accurate(double x, uint16_t x_e, unsigned k,
             const range_reduction_double_internal::LargeRangeReduction
                 &range_reduction_large) {
  using namespace math::range_reduction_double_internal;
  using FPBits = typename fputil::FPBits<double>;

  DFloat128 u_f128, sin_u, cos_u;
  if (LIBC_LIKELY(x_e < FPBits::EXP_BIAS + FAST_PASS_EXPONENT))
    u_f128 = range_reduction_small_f128(x);
  else
    u_f128 = range_reduction_large.accurate();

  math::sincos_eval_internal::sincos_eval(u_f128, sin_u, cos_u);

  auto get_sin_k = [](unsigned kk) -> DFloat128 {
    unsigned idx = (kk & 64) ? 64 - (kk & 63) : (kk & 63);
    DFloat128 ans = SIN_K_PI_OVER_128_F128[idx];
    if (kk & 128)
      ans.sign = Sign::NEG;
    return ans;
  };

  // cos(k * pi/128) = sin(k * pi/128 + pi/2) = sin((k + 64) * pi/128).
  DFloat128 sin_k_f128 = get_sin_k(k);
  DFloat128 cos_k_f128 = get_sin_k(k + 64);

  // sin(x) = sin(k * pi/128 + u)
  //        = sin(u) * cos(k*pi/128) + cos(u) * sin(k*pi/128)
  DFloat128 r = fputil::quick_add(fputil::quick_mul(sin_k_f128, cos_u),
                                  fputil::quick_mul(cos_k_f128, sin_u));

  // TODO: Add assertion if Ziv's accuracy tests fail in debug mode.
  // https://github.com/llvm/llvm-project/issues/96452.

  return static_cast<double>(r);
}
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

LIBC_INLINE double sin(double x) {
  using namespace math::range_reduction_double_internal;
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint16_t x_e = xbits.get_biased_exponent();

  DoubleDouble y;
  unsigned k = 0;
  LargeRangeReduction range_reduction_large{};

  // |x| < 2^16
  if (LIBC_LIKELY(x_e < FPBits::EXP_BIAS + FAST_PASS_EXPONENT)) {
    // |x| < 2^-4
    if (LIBC_UNLIKELY(x_e < FPBits::EXP_BIAS - 4)) {
      // |x| < 2^-26, |sin(x) - x| < ulp(x)/2.
      if (LIBC_UNLIKELY(x_e < FPBits::EXP_BIAS - 26)) {
        // Signed zeros.
        if (LIBC_UNLIKELY(x == 0.0))
          return x + x; // Make sure it works with FTZ/DAZ.

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
        return fputil::multiply_add(x, -0x1.0p-54, x);
#else
        if (LIBC_UNLIKELY(x_e < 4)) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
          int rounding_mode = fputil::quick_get_round();
          if (rounding_mode == FE_TOWARDZERO ||
              (xbits.sign() == Sign::POS && rounding_mode == FE_DOWNWARD) ||
              (xbits.sign() == Sign::NEG && rounding_mode == FE_UPWARD))
            return FPBits(xbits.uintval() - 1).get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
        }
        return fputil::multiply_add(x, -0x1.0p-54, x);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
      }
      // No range reduction needed.

      // Use degree-9 polynomial approximation:
      //   sin(x) ~ x + a1 * x^3 + a2 * x^5 + a3 * x^7 + a4 * x^9
      //          ~ x + x^3 * Q(x^2).
      // > P = fpminimax(sin(x)/x, [|0, 2, 4, 6, 8|], [|1, D...|], [0, 2^-4]);
      // > dirtyinfnorm((sin(x) - x*P)/sin(x), [-2^-4, 2^-4]);
      // 0x1.3c2e...p-69
      // > P;
      constexpr double COEFFS[] = {-0x1.5555555555555p-3, 0x1.111111110f491p-7,
                                   -0x1.a01a00e16af3ep-13,
                                   0x1.71c24233f1bafp-19};
      double x_sq = x * x;
      double c0 = fputil::multiply_add(x_sq, COEFFS[1], COEFFS[0]);
      double c1 = fputil::multiply_add(x_sq, COEFFS[3], COEFFS[2]);
      double x4 = x_sq * x_sq;
      double x3 = x * x_sq;
      double r_lo = fputil::multiply_add(x4, c1, c0) * x3;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return x + r_lo;
#else
      // Overall errors <= 2 * ulp(x^3/6) + |x| * 2^-68.
      double err = fputil::multiply_add(x_sq, 0x1.0p-53, 0x1.0p-68);
      double r_lo_u = fputil::multiply_add(x, err, r_lo);
      double r_lo_l = fputil::multiply_add(-x, err, r_lo);
      double r_upper = x + r_lo_u;
      double r_lower = x + r_lo_l;

      if (LIBC_LIKELY(r_upper == r_lower))
        return r_upper;

      k = range_reduction_small(x, y);
      return sin_accurate(x, x_e, k, range_reduction_large);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    } else {
      // Small range reduction.
      k = range_reduction_small(x, y);
    }
  } else {
    // Inf or NaN
    if (LIBC_UNLIKELY(x_e > 2 * FPBits::EXP_BIAS)) {
      // sin(+-Inf) = NaN
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      if (xbits.get_mantissa() == 0) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
      }
      return x + FPBits::quiet_nan().get_val();
    }

    // Large range reduction.
    k = range_reduction_large.fast(x, y);
  }

  DoubleDouble sin_y, cos_y;

  [[maybe_unused]] double err =
      math::sincos_eval_internal::sincos_eval(y, sin_y, cos_y);

  // Look up sin(k * pi/128) and cos(k * pi/128)
#ifdef LIBC_MATH_HAS_SMALL_TABLES
  // Memory saving versions.  Use 65-entry table.
  auto get_idx_dd = [](unsigned kk) -> DoubleDouble {
    unsigned idx = (kk & 64) ? 64 - (kk & 63) : (kk & 63);
    DoubleDouble ans = SIN_K_PI_OVER_128[idx];
    if (kk & 128) {
      ans.hi = -ans.hi;
      ans.lo = -ans.lo;
    }
    return ans;
  };
  DoubleDouble sin_k = get_idx_dd(k);
  DoubleDouble cos_k = get_idx_dd(k + 64);
#else
  // Fast look up version, but needs 256-entry table.
  // cos(k * pi/128) = sin(k * pi/128 + pi/2) = sin((k + 64) * pi/128).
  DoubleDouble sin_k = SIN_K_PI_OVER_128[k & 255];
  DoubleDouble cos_k = SIN_K_PI_OVER_128[(k + 64) & 255];
#endif

  // After range reduction, k = round(x * 128 / pi) and y = x - k * (pi / 128).
  // So k is an integer and -pi / 256 <= y <= pi / 256.
  // Then sin(x) = sin((k * pi/128 + y)
  //             = sin(y) * cos(k*pi/128) + cos(y) * sin(k*pi/128)
  DoubleDouble sin_k_cos_y = fputil::quick_mult(cos_y, sin_k);
  DoubleDouble cos_k_sin_y = fputil::quick_mult(sin_y, cos_k);
  // When k != 0 mod 128,
  //   |sin( k * pi/128 )| > pi/128 - epsilon > |y| >= |sin(y)|,
  // and cos(y) > 1 - pi/128.  So we can use Fast2Sum for the addition:
  //   sin(y) * cos(k*pi/128) + cos(y) * sin(k*pi/128).
  DoubleDouble rr = fputil::exact_add(sin_k_cos_y.hi, cos_k_sin_y.hi);
  rr.lo += sin_k_cos_y.lo + cos_k_sin_y.lo;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  return rr.hi + rr.lo;
#else
  // Accurate test and pass for correctly rounded implementation.

  double rlp = rr.lo + err;
  double rlm = rr.lo - err;

  double r_upper = rr.hi + rlp; // (rr.lo + ERR);
  double r_lower = rr.hi + rlm; // (rr.lo - ERR);

  // Ziv's rounding test.
  if (LIBC_LIKELY(r_upper == r_lower))
    return r_upper;

  return sin_accurate(x, x_e, k, range_reduction_large);
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SIN_H
