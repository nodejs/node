//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for double-precision pow(x, y).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_POW_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_POW_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/math/common_constants.h"
#include "src/__support/math/exp_constants.h"
#include "src/__support/math/pow_utils.h"

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#include "src/__support/math/pow_fast.h"
#else

#include "src/__support/math/pow_accurate_128.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

// Overview of the main part of pow(x, y) = x^y computations.
//
// Let x = 2^(e_x) * m_x > 0.  Then:
//   x^y = 2^( y * log2(x) )
//       = 2^( y * ( e_x + log2(m_x) ) )
//       = 2^( e_h + e_l )
//       = 2^(e_h) * 2^(e_l)
// where:
//   e_h = round(y * log2(x)),
//   e_l = {y * log2(x)} = y * log2(x) - e_h.
//
// In particular, e_h is an integer, and |e_l| <= 0.5.
//
// For the final result to be fit in double precision, the exponent field can be
// bounded by:
//   -1075 <= e_h <= 1024,
// and anything outside that can be passed to quick overflow/underflow logic.
//
// Since |e_l| <= 0.5:
//   0.5 < 2^(e_l) < 2,
// the relative error of x^y = 2^(e_h) * 2^(e_l) is about
//    ~ absolute error of e_l
//    ~ absolute error of y * log2(x)
//    ~ relative error(log2(x)) * |y|
//    ~ relative error(log2(x)) * |e_h|
//    < relative error(log2(x)) * 2^11.
//
// Roughly speaking, to compute x^y with relative error < 2^(-n), we will need
// to compute log2(x) with relative error < 2^(-n - 11), approximately.
//
// To compute log2(x), we will perform range reduction for log2(m_x):
//   dx = r * m_x - 1.
// Then m_x = (1 + dx) / r, and
//   log2(m_x) = log2( (1 + dx) / r )
//             = log2(1 + dx) - log2(r),
// where -log2(r) is obtained from look up tables.
// The computation of dx = r * m_x - 1 is exact, and we choose the size of the
// look up table for r's such that:
//    -2^-8 <= dx < 2^-7.
// Combining them together, we have that:
//   log2(x) = e_x - log2(r) + log2(1 + dx)
//           = e_x - log2(r) + log2(e) * (dx - dx^2/2 + dx^3/3 - ...)
// So in the worst case, where e_x = log2(r) = 0, the relative error of log2(x)
// computation will be:
//   ~ absolute_error(log2(1 + dx)) / |dx|.
// So if we compute log2(1 + dx) accurately up to dx^n term, and use a
// polynomial approximation for the dx^(n + 1) and higher terms:
//   log2(1 + dx) ~ log2(e) * (dx - dx^2/2 + ... + (-1)^(n + 1) dx^n / n) +
//                  dx^(n + 1) * P(dx)
// such that the absolute error of P(dx) is smaller than double precision ulp,
// then the overall relative error of our log2(1 + dx) approximation is:
//   ~ ulp(dx^(n + 1)) / |dx| ~ 2^(-52) * |dx^n|
//
// Let's consider 3 cases:
// - For a fast path of a correctly rounded implementation, we will want the
//   relative error of x^y bounded above by:
//     ~ 2^(-precision - 10) = 2^(-53 - 10) = 2^(-63)
//   so that the Ziv accuracy test passes with probability > 1 - 2^-10 ~ 99.99%.
//   And from the above argument, we need to compute log2(1 + dx) with relative
//   error ~ 2^(-63 - 11) = 2^(-74).
//   So we will need to compute log2(1 + dx) accurately up to dx^n such that:
//     2^(-52) * |dx^n| ~ 2^(-74).
//   Or equivalently:
//     |dx^n| < 2^(-22).
//   With our bounds from range reduction |dx| < 2^(-7), n = 3 will be quite
//   close to our desired precision needed.
//
//   In summary, for a fast path of correctly rounded implementation, we will
//   compute log2(e) * (dx - dx^2/2 + dx^3/3) accurately, and approximate higher
//   terms with dx^4 * P(dx), where:
//     |(log2(1 + dx) - log2(e) * (dx - dx^2/2 + dx^3/3))/dx^4 - P(dx)| < 2^-53.
//
// - For a strictly < 1 ULP error fast version across the full exponent range
//   (as implemented in pow_fast), we will want the relative error of x^y
//   bounded above by:
//     ~ 2^(-precision) = 2^(-53).
//   So the relative error needed to approximate log2(1 + dx) is:
//     ~ 2^(-53 - 11) = 2^(-64),
//   and we will need to compute log2(1 + dx) accurately up to dx^n such that:
//     2^(-52) * |dx^n| ~ 2^(-64),
//   or equivalently:
//     |dx^n| < 2^-12.
//   With |dx| < 2^(-7), n = 2 satisfies |dx^2| < 2^(-14) < 2^(-12).
//   Hence, we compute log2(e) * (dx - dx^2/2) accurately in DoubleDouble, and
//   approximate higher terms with dx^3 * P(dx), where:
//     |(log2(1 + dx) - log2(e) * (dx - dx^2/2))/dx^3 - P(dx)| < 2^-53.
//   This guarantees error < 1 ULP everywhere (empirically bounded by 0.75 ULP).
//
// - If we were to choose n = 1 (computing only log2(e) * dx accurately, and
//   approximating higher terms with dx^2 * P(dx)), the relative error of
//   log2(1 + dx) is only bounded by:
//     ~ 2^(-52) * |dx| < 2^(-52) * 2^(-7) = 2^(-59).
//   The resulting relative error of x^y is about:
//     ~ 2^(-59) * |e_h|.
//   While this achieves < 1 ULP for typical inputs with |e_h| <= 64, near
//   extreme exponent boundaries (|e_h| ~ 2^10 and |dx| ~ 2^-7) the error can
//   drift up to ~7.8 ULPs (bounded theoretically by ~16 ULPs).

LIBC_INLINE double pow(double x, double y) {
  using namespace pow_internal;
  using FPBits = fputil::FPBits<double>;

  FPBits xbits(x), ybits(y);
  uint64_t x_u = xbits.uintval();
  uint64_t y_u = ybits.uintval();
  uint64_t y_a = ybits.abs().uintval();

  if (LIBC_UNLIKELY((x_u & 0x0003'FFFF'FFFF'FFFF) == 0 ||
                    (y_u & 0x000F'FFFF'FFFF'FFFF) == 0)) {
    if (auto r = check_special_inputs(x, y); LIBC_UNLIKELY(r.has_value()))
      return r.value();
  }

  double e_x = static_cast<double>(xbits.get_exponent());
  uint64_t x_mant = xbits.get_mantissa();
  bool is_neg = false;
  double sign_d = 1.0;

  if (LIBC_UNLIKELY(y_a <= Y_LOWER_BOUND || y_a >= Y_UPPER_BOUND ||
                    x_u >= FPBits::inf().uintval() ||
                    x_u < FPBits::min_normal().uintval())) {
    if (auto r = check_exceptional_cases(x, y, e_x, x_mant, is_neg, sign_d);
        LIBC_UNLIKELY(r.has_value()))
      return r.value();
  }

  // x^y = 2^( y * log2(x) )
  //     = 2^( y * ( e_x + log2(m_x) ) )
  // First we compute log2(x) = e_x + log2(m_x)

  // Extract exponent field of x.

  // Use the highest 7 fractional bits of m_x as the index for look up tables.
  unsigned idx_x = static_cast<unsigned>(x_mant >> (FPBits::FRACTION_LEN - 7));
  // Add the hidden bit to the mantissa.
  // 1 <= m_x < 2
  FPBits m_x = FPBits(x_mant | 0x3ff0'0000'0000'0000);

  // Reduced argument for log2(m_x):
  //   dx = r * m_x - 1.
  // The computation is exact, and -2^-8 <= dx < 2^-7.
  // Then m_x = (1 + dx) / r, and
  //   log2(m_x) = log2( (1 + dx) / r )
  //             = log2(1 + dx) - log2(r).

  // As analyzed in the overview comment above, we evaluate the cubic part
  // (dx * C1 + dx^2 * C2 + dx^3 * C3) accurately using DoubleDouble (n = 3)
  // and approximate higher terms with dx^4 * P(dx) to ensure relative error
  // < 2^-63 for this fast path.

  // Degree-5 polynomial approximation for:
  //   P(dx) ~ (log2(1 + dx) - (dx - dx^2/2 + dx^3/3)/log(2)) / dx^4
  // Generated by Sollya with:
  // > P = fpminimax((log2(1 + x) - (x - x^2/2 + x^3/3)/log(2))/x^4, 5,
  //                 [|D...|], [-2^-8, 2^-7]);
  // > dirtyinfnorm((log2(1 + x) - (x - x^2/2 + x^3/3)/log(2))/x - x^3*P,
  //                [-2^-8, 2^-7]);
  //   0x1.a643c...p-74
  // > dirtyinfnorm((log2(1 + x) - (x - x^2/2 + x^3/3)/log(2))/x^4 - P,
  //                [-2^-8, 2^-7]);
  //   0x1.b81c5...p-53
  constexpr double COEFFS[] = {-0x1.71547652b82fdp-2, 0x1.2776c50ef8f9bp-2,
                               -0x1.ec709dc4f0fedp-3, 0x1.a617677f716dep-3,
                               -0x1.715423d54d5p-3,   0x1.44e6355fc4d03p-3};

  // Constants for C_k = (-1)^(k-1) / (k * log(2)) in DoubleDouble:
  // C1 = 1 / log(2)
  constexpr DoubleDouble C1 = {0x1.777d0ffda0d24p-56, 0x1.71547652b82fep0};
  // C2 = -1 / (2 * log(2))
  constexpr DoubleDouble C2 = {-0x1.777d0ffda0d24p-57, -0x1.71547652b82fep-1};
  // C3 = 1 / (3 * log(2))
  constexpr DoubleDouble C3 = {0x1.b749fc15522bcp-50, 0x1.ec709dc3a03e2p-2};

  // Perform exact range reduction.
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double dx = fputil::multiply_add(RD[idx_x], m_x.get_val(), -1.0); // Exact
#else
  double c = FPBits(m_x.uintval() & 0x3fff'e000'0000'0000).get_val();
  double dx =
      fputil::multiply_add(RD[idx_x], m_x.get_val() - c, CD[idx_x]); // Exact
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  // Evaluate the cubic part (dx * C1 + dx^2 * C2 + dx^3 * C3) and polynomial
  // tail using a parallel Double-Double Estrin scheme.
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // Error-free transformation for r = C1.hi + dx * C2.hi:
  double r_hi = fputil::multiply_add(dx, C2.hi, C1.hi);
  double r_lo = fputil::multiply_add(dx, C2.hi, C1.hi - r_hi); // Exact error
#else
  DoubleDouble dx_c2 = fputil::exact_mult(dx, C2.hi);
  DoubleDouble r_sum = fputil::exact_add(C1.hi, dx_c2.hi);
  double r_hi = r_sum.hi;
  double r_lo = r_sum.lo + dx_c2.lo;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  // Low parts polynomial evaluated in parallel:
  //   C1.lo + dx * (C2.lo + dx * C3.lo) + r_lo
  double lo_tail = fputil::multiply_add(dx, C3.lo, C2.lo);
  double lo_poly = fputil::multiply_add(dx, lo_tail, C1.lo + r_lo);

  // Evaluate polynomial tail P(dx) using Estrin's scheme:
  double dx2 = dx * dx;
  double c0 = fputil::multiply_add(dx, COEFFS[1], COEFFS[0]);
  double c1 = fputil::multiply_add(dx, COEFFS[3], COEFFS[2]);
  double c2 = fputil::multiply_add(dx, COEFFS[5], COEFFS[4]);

  double dx4 = dx2 * dx2;
  double d0 = fputil::multiply_add(dx2, c1, c0);
  double p = fputil::multiply_add(dx4, c2, d0);

  // High part of cubic term: C3.hi + dx * P(dx)
  double q = fputil::multiply_add(dx, p, C3.hi);

  // Combine dx^2 * q into lo_poly:
  lo_poly = fputil::multiply_add(dx2, q, lo_poly);

  // Multiply by dx to get log2(1 + dx) in DoubleDouble:
  DoubleDouble log2_1p = fputil::exact_mult(dx, r_hi);
  log2_1p.lo = fputil::multiply_add(dx, lo_poly, log2_1p.lo);

  // Combine with e_x - log2(r):
  DoubleDouble log2_x_hi =
      fputil::exact_add(e_x + LOG2_R_DD[idx_x].hi, log2_1p.hi);
  double log2_x_lo = log2_1p.lo + LOG2_R_DD[idx_x].lo;
  DoubleDouble log2_x = fputil::exact_add(log2_x_hi.hi, log2_x_lo);
  log2_x.lo += log2_x_hi.lo;

  // To compute 2^(y * log2(x)), we break the exponent into 3 parts:
  //   y * log2(x) = hi + mid + lo, where
  //   hi is an integer
  //   mid * 2^6 is an integer
  //   |lo| <= 2^-7
  // Then:
  //   x^y = 2^(y * log2(x)) = 2^hi * 2^mid * 2^lo,
  // In which 2^mid is obtained from a look-up table of size 2^6 = 64 elements,
  // and 2^lo ~ 1 + lo * P(lo).
  // Thus, we have:
  //   hi + mid = 2^-6 * round( 2^6 * y * log2(x) )
  // If we restrict the output such that |hi| < 512, (hi + mid) uses (9 + 6)
  // bits, hence, if we use double precision to perform
  //   round( 2^6 * y * log2(x))
  // the lo part is bounded by 2^-7 + 2^(-(52 - 15)) = 2^-7 + 2^-37

  // In the following computations:
  //   y6  = 2^6 * y
  //   hm  = 2^6 * (hi + mid) = round(2^6 * y * log2(x)) ~ round(y6 * s)
  //   lo6 = 2^6 * lo = 2^6 * (y - (hi + mid)) = y6 * log2(x) - hm.
  constexpr double SCALE = 0x1.0p6;
  double y6 = y * SCALE; // Exact.

  DoubleDouble y6_log2_x = fputil::exact_mult(y6, log2_x.hi);
  y6_log2_x.lo = fputil::multiply_add(y6, log2_x.lo, y6_log2_x.lo);

  // Check overflow/underflow.
  double scale = 1.0;
  bool is_denorm = false;

  // |2^(hi + mid) - exp2_hi_mid| <= ulp(exp2_hi_mid) / 2

  // The fast computation for 2^hi below requires that:
  //   |hi| < 512, or equivalently, |hm| < 512 * 2^6.
  // This guarantees that the biased exponent:
  //   exp_biased = (hm_i >> 6) + EXP_BIAS = hi + 1023
  // is strictly within the normal double range:
  //   1023 - 511 <= exp_biased <= 1023 + 511, or 512 <= exp_biased <= 1534.
  // Hence, 2^hi is always a normal, non-zero, finite power of 2, and the
  // multiplication upper * exp2_hi is exact and never underflows or overflows.
  //
  // From the edge case checks above:
  //   2^(-54) / 1074       <= |y|       <= 1075 * 2^53, and
  //   |log_2(1 - 2^(-53))| <= |log2(x)| <= 1074.
  // So their product is bounded by:
  //   2^-117 < |y * log2(x)| < 2^74.
  //
  // The meaningful range for y * log2(x) in double precision is:
  //   -1075 <= y * log2(x) <= 1024.
  // Any value > 1024 overflows, and any value < -1075 underflows.
  //
  // When |y * log2(x)| >= 511, we shift the exponent by an offset S:
  //   y * log2(x) = (y * log2(x) - S) + S
  // and multiply by scale = 2^S at the end.
  // To ensure the shifted exponent (y * log2(x) - S) stays within [-511, 511]:
  // - For positive range [511, 1024]:
  //     511 - S > -511  ==>  S < 1022
  //    1024 - S <  511  ==>  S > 513
  // - For negative range [-1076, -511]:
  //    -511 + S <  511  ==>  S <= 1022
  //   -1076 + S > -511  ==>  S >= 565
  // Combined, the shift must satisfy: 565 <= S <= 1022.
  // We choose S = 600, which yields:
  //   y * log2(x) - 600 in [-89, 424] for the positive range, and
  //   y * log2(x) + 600 in [-476, 89] for the negative range,
  // both fitting well within [-511, 511].
  //
  // For exponents that are completely out of range:
  //   y * log2(x) > 1025  or  y * log2(x) < -1076,
  // the product can be as large as 2^74, so subtracting or adding 600 * 64
  // would still overflow a 32-bit int when computing hm_i.
  // We return early with overflow or underflow in these cases.
  //
  // Alternatively, for less branching or in SIMD / vector implementations, one
  // could clamp y6_log2_x.hi to:
  // - UPPER_EXP_BOUND (511 * 64) for overflow, which when multiplied by
  //   scale = 2^600 yields 2^1111 and correctly overflows.
  // - -500 * 64 for underflow, which when multiplied by scale = 2^-600 yields
  //   2^-1100 and correctly underflows.
  // However, in this correctly rounded version, such clamping can cause the
  // Ziv accuracy test to fail on the clamped exponent, unnecessarily
  // triggering the slow accurate paths for overflow or underflow cases.

  constexpr double UPPER_EXP_BOUND = 511.0 * SCALE;
  if (LIBC_UNLIKELY(FPBits(y6_log2_x.hi).abs().get_val() >= UPPER_EXP_BOUND)) {
    if (FPBits(y6_log2_x.hi).sign() == Sign::POS) {
      if (y6_log2_x.hi > 1025.0 * SCALE)
        return set_overflow(is_neg);
      scale = 0x1.0p600;
      y6_log2_x.hi -= 600.0 * SCALE;
    } else {
      if (y6_log2_x.hi <= -1021.0 * SCALE) {
        if (y6_log2_x.hi < -1076.0 * SCALE)
          return set_underflow(is_neg);
        is_denorm = true;
      } else {
        scale = 0x1.0p-600;
        y6_log2_x.hi += 600.0 * SCALE;
      }
    }
  }

  double hm = fputil::nearest_integer(y6_log2_x.hi);

  // lo6 = 2^6 * lo.
  DoubleDouble lo6 = fputil::exact_add(y6_log2_x.hi - hm, y6_log2_x.lo);

  int hm_i = static_cast<int>(hm);
  unsigned idx_y = static_cast<unsigned>(hm_i) & 0x3f;

  // 2^hi
  int hi = hm_i >> 6;
  double exp2_hi = 1.0;
  if (LIBC_LIKELY(!is_denorm)) {
    int64_t exp2_hi_i = static_cast<int64_t>(
        static_cast<uint64_t>(hi + FPBits::EXP_BIAS) << FPBits::FRACTION_LEN);
    exp2_hi = FPBits(static_cast<uint64_t>(exp2_hi_i)).get_val();
  }

  // 2^mid
  DoubleDouble exp2_mid{EXP2_MID1[idx_y].mid * sign_d,
                        EXP2_MID1[idx_y].hi * sign_d};

  // Polynomial expansion for 2^(lo6/64):
  // 2^(lo6/64) ~ 1 + lo6 * (log(2)/64) + lo6^2 * P(lo6)
  // The linear term is computed in DoubleDouble, and the degree-3 polynomial
  // P(lo6) is evaluated with standard double precision.
  //
  // hi and lo parts of log(2)/64, generated by Sollya with:
  // > a = D(log(2)/64);
  // > b = D(log(2)/64 - a);
  constexpr DoubleDouble LOG_2_OVER_64 = {0x1.abc9e3b39803fp-62,
                                          0x1.62e42fefa39efp-7};

  // Degree-3 polynomial approximation for (2^(lo6/64) - 1 - lo6*log(2)/64) /
  // lo6^2: Generated by Sollya with:
  //   > f = (2^(x/64) - 1 - x*log(2)/64) / x^2;
  //   > P = fpminimax(f, 5, [|D...|], [-0.5, 0.5]);
  //   > dirtyinfnorm((f - P) * x^2, [-0.5, 0.5]);
  //     0x1.1778...p-73
  constexpr double EXP2_COEFFS[] = {
      0x1.ebfbdff82c58fp-15, 0x1.c6b08d704a0cp-23,  0x1.3b2ab6fb4d08fp-31,
      0x1.5d87fe77a735dp-40, 0x1.430d835610044p-49, 0x1.ffe67c38112c3p-59};

  DoubleDouble lo_log_2 = fputil::quick_mult(lo6, LOG_2_OVER_64);
  DoubleDouble lo_log_2_p1 = fputil::exact_add(1.0, lo_log_2.hi);
  lo_log_2_p1.lo += lo_log_2.lo;

  double lo6_sq = lo6.hi * lo6.hi;
  double e0 = fputil::multiply_add(lo6.hi, EXP2_COEFFS[1], EXP2_COEFFS[0]);
  double e1 = fputil::multiply_add(lo6.hi, EXP2_COEFFS[3], EXP2_COEFFS[2]);
  double e2 = fputil::multiply_add(lo6.hi, EXP2_COEFFS[5], EXP2_COEFFS[4]);

  double lo6_4 = lo6_sq * lo6_sq;
  double f0 = fputil::multiply_add(lo6_sq, e0, lo_log_2_p1.lo);
  double f1 = fputil::multiply_add(lo6_sq, e2, e1);

  lo_log_2_p1.lo = fputil::multiply_add(lo6_4, f1, f0);
  DoubleDouble r = fputil::quick_mult(exp2_mid, lo_log_2_p1);

  // Absolute error bound for r:
  // - Base error from exp2 stage is bounded by 0x1.0p-64.
  // - Propagated error from log2(x):
  //     |y| * err(log2(x)) * (2^mid * log(2)) <= |y| * 2^-73.5
  // Dynamic error bound adapting to exponent scale |y|:
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double err_r =
      fputil::multiply_add(FPBits(y).abs().get_val(), 0x1.8p-73, 0x1.0p-64);
#else  // !LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // Without FMA, intermediate roundings increase log2(x) and exp2 errors.
  double err_r =
      fputil::multiply_add(FPBits(y).abs().get_val(), 0x1.cp-72, 0x1.0p-63);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  if (LIBC_UNLIKELY(is_denorm)) {
    if (auto r_denorm = ziv_test_denorm(hi, r.hi, r.lo, err_r, is_neg);
        LIBC_LIKELY(r_denorm.has_value())) {
      double res = r_denorm.value();

      // Since is_denorm is triggered when y * log2(x) <= -1021 * 2^6, the
      // rounded result can still be a normal number (>= 2^-1022). In that
      // case, no underflow has occurred, so return directly.
      if (LIBC_UNLIKELY(FPBits(res).is_normal()))
        return res;

      // When the rounded result is denormal or zero, underflow and ERANGE
      // should only be set if the result is inexact. We check for exact
      // results:
      // 1. If x = 2^e_x (x_mant == 0), then x^y = 2^(e_x * y) is an exact
      //    power of 2 iff e_x * y is an integer and >= -1074.0 (the smallest
      //    representable power of 2 in double precision).
      if (LIBC_UNLIKELY(x_mant == 0)) {
        double ex_y = e_x * y;
        if (ex_y >= -1074.0 && pow_internal::is_integer(ex_y))
          return res;
      } else if (LIBC_UNLIKELY(y > 0.0 && y <= 35.0)) {
        // 2. If x is not a power of 2, exact results in double precision can
        //    only occur for 0 < y <= 35 (Lauter and Lefevre). If it falls on
        //    an exact boundary, convert via DyadicFloat to ensure that
        //    underflow and inexact exceptions are not signaled.
        uint64_t exact_m = 0;
        int exact_exp = 0;
        if (pow_internal::is_exact_rounding_boundary(x, y, exact_m,
                                                     exact_exp)) {
          int l = 64 - cpp::countl_zero(exact_m);
          pow_internal::DFloat128 exact_f128(
              is_neg ? Sign::NEG : Sign::POS, exact_exp + l - 128,
              pow_internal::MantissaType(exact_m) << (128 - l));
          exact_f128.normalize();
          return static_cast<double>(exact_f128);
        }
      }

      // Otherwise, the denormal or zero result is inexact, so we signal the
      // underflow exception and set ERANGE if required.
      fputil::set_errno_if_required(ERANGE);
      fputil::raise_underflow_except_if_required<double>();
      return res;
    }
    return pow_accurate(x, y, is_neg, static_cast<int>(e_x), idx_x, dx);
  }

  double upper = r.hi + (r.lo + err_r);
  double lower = r.hi + (r.lo - err_r);
  if (LIBC_LIKELY(upper == lower)) {
    double tmp1 = upper * exp2_hi;
    return tmp1 * scale;
  }

  return pow_accurate(x, y, is_neg, static_cast<int>(e_x), idx_x, dx);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_POW_H
