//===-- Implementation header for asinpi ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ASINPI_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ASINPI_H

#include "asin_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"            // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h" // LIBC_TARGET_CPU_HAS_FMA
#include "src/__support/math/asin_utils.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE double asinpi(double x) {
  using namespace asin_internal;
  using FPBits = fputil::FPBits<double>;

  FPBits xbits(x);
  int x_exp = xbits.get_biased_exponent();

  // |x| < 0.5.
  if (x_exp < FPBits::EXP_BIAS - 1) {
    // |x| < 2^-26.
    if (LIBC_UNLIKELY(x_exp < FPBits::EXP_BIAS - 26)) {
      // asinpi(+-0) = +-0.
      if (LIBC_UNLIKELY(xbits.abs().uintval() == 0))
        return x;
      // When |x| < 2^-26, asinpi(x) ~ x/pi.
      // The relative error of x/pi is:
      //   |asinpi(x) - x/pi| / |asinpi(x)| < x^2/6 < 2^-54.
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return x * ASINPI_COEFFS[0];
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    return x * asinpi_eval(x * x);
#else
    using Float128 = fputil::DyadicFloat<128>;
    using DoubleDouble = fputil::DoubleDouble;

    // For |x| < 2^-511, x^2 would underflow to subnormal, raising a
    // spurious underflow exception. Since asinpi(x) = x/pi with correction
    // x^2/(6*pi) < 2^-1024 relative (negligible), compute x/pi directly
    // in Float128.
    if (LIBC_UNLIKELY(x_exp < 512)) {
      Float128 x_f128(x);
      Float128 r = fputil::quick_mul(x_f128, ONE_OVER_PI_F128);
      double result = static_cast<double>(r);

      // IEEE 754 "after rounding" tininess: the 53-bit unlimited-exponent
      // result is strictly between +-2^-1022. DyadicFloat's conversion
      // checks the *IEEE subnormal* result (52-bit at the boundary), not
      // the 53-bit unlimited-exponent result, so we detect it here.
      int exp_hi = r.exponent + 127 + FPBits::EXP_BIAS;
      if (LIBC_UNLIKELY(exp_hi <= 0) && !r.mantissa.is_zero()) {
        bool raise_underflow = true;
        // When exp_hi == 0, a carry in 53-bit rounding can push the
        // result to exactly 2^-1022 (not tiny). Check for this.
        if (exp_hi == 0) {
          constexpr unsigned SHIFT_53 = 128 - FPBits::SIG_LEN - 1;
          using MantT = typename Float128::MantissaType;
          MantT m53 = r.mantissa >> SHIFT_53;
          constexpr MantT ALL_ONES_53 = (MantT(1) << (FPBits::SIG_LEN + 1)) - 1;
          if (m53 == ALL_ONES_53) {
            // All 53 bits set. carry happens if rounding rounds away
            // from zero at this precision.
            bool round_bit =
                static_cast<bool>((r.mantissa >> (SHIFT_53 - 1)) & 1);
            MantT sticky_mask = (MantT(1) << (SHIFT_53 - 1)) - 1;
            bool sticky = (r.mantissa & sticky_mask) != 0;
            bool lsb = static_cast<bool>(m53 & 1);
            switch (fputil::quick_get_round()) {
            case FE_TONEAREST:
              // Carry if round_bit && (lsb || sticky) (round half to even).
              raise_underflow = !(round_bit && (lsb || sticky));
              break;
            case FE_UPWARD:
              raise_underflow = xbits.is_neg() || !(round_bit || sticky);
              break;
            case FE_DOWNWARD:
              raise_underflow = !xbits.is_neg() || !(round_bit || sticky);
              break;
            case FE_TOWARDZERO:
            default:
              raise_underflow = true; // truncation never carries
              break;
            }
          }
        }
        if (raise_underflow)
          fputil::raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);
      }
      return result;
    }

    unsigned idx = 0;
    DoubleDouble x_sq = fputil::exact_mult(x, x);
    double err = xbits.abs().get_val() * 0x1.0p-51;
    // Polynomial approximation:
    //   p ~ asin(x)/(pi*x)

    DoubleDouble p = asinpi_eval(x_sq, idx, err);
    // asinpi(x) ~ x * p
    DoubleDouble r0 = fputil::exact_mult(x, p.hi);
    double r_lo = fputil::multiply_add(x, p.lo, r0.lo);

    // Ziv's accuracy test.
    double r_upper = r0.hi + (r_lo + err);
    double r_lower = r0.hi + (r_lo - err);

    if (LIBC_LIKELY(r_upper == r_lower))
      return r_upper;

    // Ziv's accuracy test failed, perform 128-bit calculation.

    // Recalculate mod 1/64.
    idx = static_cast<unsigned>(fputil::nearest_integer(x_sq.hi * 0x1.0p6));

    Float128 x_f128(x);

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    Float128 u_hi(
        fputil::multiply_add(static_cast<double>(idx), -0x1.0p-6, x_sq.hi));
    Float128 u = fputil::quick_add(u_hi, Float128(x_sq.lo));
#else
    Float128 x_sq_f128 = fputil::quick_mul(x_f128, x_f128);
    Float128 u = fputil::quick_add(
        x_sq_f128, Float128(static_cast<double>(idx) * (-0x1.0p-6)));
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

    Float128 p_f128 = asinpi_eval(u, idx);
    Float128 r = fputil::quick_mul(x_f128, p_f128);

    return static_cast<double>(r);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  }
  // |x| >= 0.5

  double x_abs = xbits.abs().get_val();

  // Maintaining the sign:
  constexpr double SIGN[2] = {1.0, -1.0};
  double x_sign = SIGN[xbits.is_neg()];

  // |x| >= 1
  if (LIBC_UNLIKELY(x_exp >= FPBits::EXP_BIAS)) {
    // x = +-1, asinpi(x) = +- 0.5
    if (x_abs == 1.0) {
      return x_sign * 0.5;
    }
    // |x| > 1, return NaN.
    if (xbits.is_quiet_nan())
      return x;

    // Set domain error for non-NaN input.
    if (!xbits.is_nan())
      fputil::set_errno_if_required(EDOM);

    fputil::raise_except_if_required(FE_INVALID);
    return FPBits::quiet_nan().get_val();
  }

  // When |x| >= 0.5, we perform range reduction as follow:
  //
  // Assume further that 0.5 <= x < 1, and let:
  //   y = asin(x)
  // Using the identity:
  //   asin(x) = pi/2 - 2 * asin( sqrt( (1 - x)/2 ) )
  // We get:
  //   asinpi(x) = asin(x)/pi = 0.5 - 2 * asin(sqrt(u)) / pi
  //             = 0.5 - 2 * sqrt(u) * [asin(sqrt(u)) / (pi * sqrt(u))]
  //             = 0.5 - 2 * sqrt(u) * asinpi_eval(u)
  // where u = (1 - |x|) / 2.

  // u = (1 - |x|)/2
  double u = fputil::multiply_add(x_abs, -0.5, 0.5);
  // v_hi ~ sqrt(u).
  double v_hi = fputil::sqrt<double>(u);

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double p = asinpi_eval(u);
  double r = x_sign * fputil::multiply_add(-2.0 * v_hi, p, 0.5);
  return r;
#else
  using Float128 = fputil::DyadicFloat<128>;
  using DoubleDouble = fputil::DoubleDouble;

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double h = fputil::multiply_add(v_hi, -v_hi, u);
#else
  DoubleDouble v_hi_sq = fputil::exact_mult(v_hi, v_hi);
  double h = (u - v_hi_sq.hi) - v_hi_sq.lo;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  // Scale v_lo and v_hi by 2 from the formula:
  //   vh = v_hi * 2
  //   vl = 2*v_lo = h / v_hi.
  double vh = v_hi * 2.0;
  double vl = h / v_hi;

  // Polynomial approximation:
  //   p ~ asin(sqrt(u))/(pi*sqrt(u))
  unsigned idx = 0;
  double err = vh * 0x1.0p-51;

  DoubleDouble p = asinpi_eval(DoubleDouble{0.0, u}, idx, err);

  // Perform computations in double-double arithmetic:
  //   asinpi(x) = 0.5 - (vh + vl) * p
  DoubleDouble r0 = fputil::quick_mult(DoubleDouble{vl, vh}, p);
  DoubleDouble r = fputil::exact_add(0.5, -r0.hi);

  double r_lo = -r0.lo + r.lo;

  // Ziv's accuracy test.

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double r_upper = fputil::multiply_add(
      r.hi, x_sign, fputil::multiply_add(r_lo, x_sign, err));
  double r_lower = fputil::multiply_add(
      r.hi, x_sign, fputil::multiply_add(r_lo, x_sign, -err));
#else
  r_lo *= x_sign;
  r.hi *= x_sign;
  double r_upper = r.hi + (r_lo + err);
  double r_lower = r.hi + (r_lo - err);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  if (LIBC_LIKELY(r_upper == r_lower))
    return r_upper;

  // Ziv's accuracy test failed, we redo the computations in Float128.
  // Recalculate mod 1/64.
  idx = static_cast<unsigned>(fputil::nearest_integer(u * 0x1.0p6));

  // After the first step of Newton-Raphson approximating v = sqrt(u):
  //   sqrt(u) = v_hi + h / (sqrt(u) + v_hi)
  //   v_lo = h / (2 * v_hi)
  // Add second-order correction:
  //   v_ll = -v_lo * (h / (4u))

  // Get the rounding error of vl = 2 * v_lo ~ h / vh
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double vl_lo = fputil::multiply_add(-v_hi, vl, h) / v_hi;
#else
  DoubleDouble vh_vl = fputil::exact_mult(v_hi, vl);
  double vl_lo = ((h - vh_vl.hi) - vh_vl.lo) / v_hi;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // vll = 2*v_ll = -vl * (h / (4u)).
  double t = h * (-0.25) / u;
  double vll = fputil::multiply_add(vl, t, vl_lo);
  // m_v = -(v_hi + v_lo + v_ll).
  Float128 m_v = fputil::quick_add(
      Float128(vh), fputil::quick_add(Float128(vl), Float128(vll)));
  m_v.sign = Sign::NEG;

  // Perform computations in Float128:
  //   asinpi(x) = 0.5 - (v_hi + v_lo + vll) * P_pi(u).
  Float128 y_f128(fputil::multiply_add(static_cast<double>(idx), -0x1.0p-6, u));

  Float128 p_f128 = asinpi_eval(y_f128, idx);
  Float128 r0_f128 = fputil::quick_mul(m_v, p_f128);
  Float128 r_f128 = fputil::quick_add(HALF_F128, r0_f128);

  if (xbits.is_neg())
    r_f128.sign = Sign::NEG;

  return static_cast<double>(r_f128);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ASINPI_H
