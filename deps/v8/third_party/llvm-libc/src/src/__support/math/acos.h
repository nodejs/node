//===-- Implementation header for acos --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ACOS_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ACOS_H

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

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr double acos(double x) {
  using DoubleDouble = fputil::DoubleDouble;
  using namespace asin_internal;
  using FPBits = fputil::FPBits<double>;

  FPBits xbits(x);
  int x_exp = xbits.get_biased_exponent();

  // |x| < 0.5.
  if (x_exp < FPBits::EXP_BIAS - 1) {
    // |x| < 2^-55.
    if (LIBC_UNLIKELY(x_exp < FPBits::EXP_BIAS - 55)) {
      // When |x| < 2^-55, acos(x) = pi/2
#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS)
      return PI_OVER_TWO.hi;
#else
      // Force the evaluation and prevent constant propagation so that it
      // is rounded correctly for FE_UPWARD rounding mode.
      return (xbits.abs().get_val() + 0x1.0p-160) + PI_OVER_TWO.hi;
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    // acos(x) = pi/2 - asin(x)
    //         = pi/2 - x * P(x^2)
    double p = asin_eval(x * x);
    return PI_OVER_TWO.hi + fputil::multiply_add(-x, p, PI_OVER_TWO.lo);
#else
    unsigned idx = 0;
    DoubleDouble x_sq = fputil::exact_mult(x, x);
    double err = xbits.abs().get_val() * 0x1.0p-51;
    // Polynomial approximation:
    //   p ~ asin(x)/x
    DoubleDouble p = asin_eval(x_sq, idx, err);
    // asin(x) ~ x * p
    DoubleDouble r0 = fputil::exact_mult(x, p.hi);
    // acos(x) = pi/2 - asin(x)
    //         ~ pi/2 - x * p
    //         = pi/2 - x * (p.hi + p.lo)
    double r_hi = fputil::multiply_add(-x, p.hi, PI_OVER_TWO.hi);
    // Use Dekker's 2SUM algorithm to compute the lower part.
    double r_lo = ((PI_OVER_TWO.hi - r_hi) - r0.hi) - r0.lo;
    r_lo = fputil::multiply_add(-x, p.lo, r_lo + PI_OVER_TWO.lo);

    // Ziv's accuracy test.

    double r_upper = r_hi + (r_lo + err);
    double r_lower = r_hi + (r_lo - err);

    if (LIBC_LIKELY(r_upper == r_lower))
      return r_upper;

    // Ziv's accuracy test failed, perform 128-bit calculation.

    // Recalculate mod 1/64.
    idx = static_cast<unsigned>(fputil::nearest_integer(x_sq.hi * 0x1.0p6));

    // Get x^2 - idx/64 exactly.  When FMA is available, double-double
    // multiplication will be correct for all rounding modes.  Otherwise we use
    // Float128 directly.
    Float128 x_f128(x);

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    // u = x^2 - idx/64
    Float128 u_hi(
        fputil::multiply_add(static_cast<double>(idx), -0x1.0p-6, x_sq.hi));
    Float128 u = fputil::quick_add(u_hi, Float128(x_sq.lo));
#else
    Float128 x_sq_f128 = fputil::quick_mul(x_f128, x_f128);
    Float128 u = fputil::quick_add(
        x_sq_f128, Float128(static_cast<double>(idx) * (-0x1.0p-6)));
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

    Float128 p_f128 = asin_eval(u, idx);
    // Flip the sign of x_f128 to perform subtraction.
    x_f128.sign = x_f128.sign.negate();
    Float128 r =
        fputil::quick_add(PI_OVER_TWO_F128, fputil::quick_mul(x_f128, p_f128));

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
    // x = +-1, asin(x) = +- pi/2
    if (x_abs == 1.0) {
      // x = 1, acos(x) = 0,
      // x = -1, acos(x) = pi
      return x == 1.0 ? 0.0 : fputil::multiply_add(-x_sign, PI.hi, PI.lo);
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
  // When 0.5 <= x < 1, let:
  //   y = acos(x)
  // We will use the double angle formula:
  //   cos(2y) = 1 - 2 sin^2(y)
  // and the complement angle identity:
  //   x = cos(y) = 1 - 2 sin^2 (y/2)
  // So:
  //   sin(y/2) = sqrt( (1 - x)/2 )
  // And hence:
  //   y/2 = asin( sqrt( (1 - x)/2 ) )
  // Equivalently:
  //   acos(x) = y = 2 * asin( sqrt( (1 - x)/2 ) )
  // Let u = (1 - x)/2, then:
  //   acos(x) = 2 * asin( sqrt(u) )
  // Moreover, since 0.5 <= x < 1:
  //   0 < u <= 1/4, and 0 < sqrt(u) <= 0.5,
  // And hence we can reuse the same polynomial approximation of asin(x) when
  // |x| <= 0.5:
  //   acos(x) ~ 2 * sqrt(u) * P(u).
  //
  // When -1 < x <= -0.5, we reduce to the previous case using the formula:
  //   acos(x) = pi - acos(-x)
  //           = pi - 2 * asin ( sqrt( (1 + x)/2 ) )
  //           ~ pi - 2 * sqrt(u) * P(u),
  // where u = (1 - |x|)/2.

  // u = (1 - |x|)/2
  double u = fputil::multiply_add(x_abs, -0.5, 0.5);
  // v_hi + v_lo ~ sqrt(u).
  // Let:
  //   h = u - v_hi^2 = (sqrt(u) - v_hi) * (sqrt(u) + v_hi)
  // Then:
  //   sqrt(u) = v_hi + h / (sqrt(u) + v_hi)
  //            ~ v_hi + h / (2 * v_hi)
  // So we can use:
  //   v_lo = h / (2 * v_hi).
  double v_hi = fputil::sqrt<double>(u);

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  constexpr DoubleDouble CONST_TERM[2] = {{0.0, 0.0}, PI};
  DoubleDouble const_term = CONST_TERM[xbits.is_neg()];

  double p = asin_eval(u);
  double scale = x_sign * 2.0 * v_hi;
  double r = const_term.hi + fputil::multiply_add(scale, p, const_term.lo);
  return r;
#else

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
  //   p ~ asin(sqrt(u))/sqrt(u)
  unsigned idx = 0;
  double err = vh * 0x1.0p-51;

  DoubleDouble p = asin_eval(DoubleDouble{0.0, u}, idx, err);

  // Perform computations in double-double arithmetic:
  //   asin(x) = pi/2 - (v_hi + v_lo) * (ASIN_COEFFS[idx][0] + p)
  DoubleDouble r0 = fputil::quick_mult(DoubleDouble{vl, vh}, p);

  double r_hi = 0, r_lo = 0;
  if (xbits.is_pos()) {
    r_hi = r0.hi;
    r_lo = r0.lo;
  } else {
    DoubleDouble r = fputil::exact_add(PI.hi, -r0.hi);
    r_hi = r.hi;
    r_lo = (PI.lo - r0.lo) + r.lo;
  }

  // Ziv's accuracy test.

  double r_upper = r_hi + (r_lo + err);
  double r_lower = r_hi + (r_lo - err);

  if (LIBC_LIKELY(r_upper == r_lower))
    return r_upper;

  // Ziv's accuracy test failed, we redo the computations in Float128.
  // Recalculate mod 1/64.
  idx = static_cast<unsigned>(fputil::nearest_integer(u * 0x1.0p6));

  // After the first step of Newton-Raphson approximating v = sqrt(u), we have
  // that:
  //   sqrt(u) = v_hi + h / (sqrt(u) + v_hi)
  //      v_lo = h / (2 * v_hi)
  // With error:
  //   sqrt(u) - (v_hi + v_lo) = h * ( 1/(sqrt(u) + v_hi) - 1/(2*v_hi) )
  //                           = -h^2 / (2*v * (sqrt(u) + v)^2).
  // Since:
  //   (sqrt(u) + v_hi)^2 ~ (2sqrt(u))^2 = 4u,
  // we can add another correction term to (v_hi + v_lo) that is:
  //   v_ll = -h^2 / (2*v_hi * 4u)
  //        = -v_lo * (h / 4u)
  //        = -vl * (h / 8u),
  // making the errors:
  //   sqrt(u) - (v_hi + v_lo + v_ll) = O(h^3)
  // well beyond 128-bit precision needed.

  // Get the rounding error of vl = 2 * v_lo ~ h / vh
  // Get full product of vh * vl
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
  m_v.sign = xbits.sign();

  // Perform computations in Float128:
  //   acos(x) = (v_hi + v_lo + vll) * P(u)         , when 0.5 <= x < 1,
  //           = pi - (v_hi + v_lo + vll) * P(u)    , when -1 < x <= -0.5.
  Float128 y_f128(fputil::multiply_add(static_cast<double>(idx), -0x1.0p-6, u));

  Float128 p_f128 = asin_eval(y_f128, idx);
  Float128 r_f128 = fputil::quick_mul(m_v, p_f128);

  if (xbits.is_neg())
    r_f128 = fputil::quick_add(PI_F128, r_f128);

  return static_cast<double>(r_f128);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ACOS_H
