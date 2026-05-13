//===-- Implementation header for atan2 -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2_H

#include "atan_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

// There are several range reduction steps we can take for atan2(y, x) as
// follow:

// * Range reduction 1: signness
// atan2(y, x) will return a number between -PI and PI representing the angle
// forming by the 0x axis and the vector (x, y) on the 0xy-plane.
// In particular, we have that:
//   atan2(y, x) = atan( y/x )         if x >= 0 and y >= 0 (I-quadrant)
//               = pi + atan( y/x )    if x < 0 and y >= 0  (II-quadrant)
//               = -pi + atan( y/x )   if x < 0 and y < 0   (III-quadrant)
//               = atan( y/x )         if x >= 0 and y < 0  (IV-quadrant)
// Since atan function is odd, we can use the formula:
//   atan(-u) = -atan(u)
// to adjust the above conditions a bit further:
//   atan2(y, x) = atan( |y|/|x| )         if x >= 0 and y >= 0 (I-quadrant)
//               = pi - atan( |y|/|x| )    if x < 0 and y >= 0  (II-quadrant)
//               = -pi + atan( |y|/|x| )   if x < 0 and y < 0   (III-quadrant)
//               = -atan( |y|/|x| )        if x >= 0 and y < 0  (IV-quadrant)
// Which can be simplified to:
//   atan2(y, x) = sign(y) * atan( |y|/|x| )             if x >= 0
//               = sign(y) * (pi - atan( |y|/|x| ))      if x < 0

// * Range reduction 2: reciprocal
// Now that the argument inside atan is positive, we can use the formula:
//   atan(1/x) = pi/2 - atan(x)
// to make the argument inside atan <= 1 as follow:
//   atan2(y, x) = sign(y) * atan( |y|/|x|)            if 0 <= |y| <= x
//               = sign(y) * (pi/2 - atan( |x|/|y| )   if 0 <= x < |y|
//               = sign(y) * (pi - atan( |y|/|x| ))    if 0 <= |y| <= -x
//               = sign(y) * (pi/2 + atan( |x|/|y| ))  if 0 <= -x < |y|

// * Range reduction 3: look up table.
// After the previous two range reduction steps, we reduce the problem to
// compute atan(u) with 0 <= u <= 1, or to be precise:
//   atan( n / d ) where n = min(|x|, |y|) and d = max(|x|, |y|).
// An accurate polynomial approximation for the whole [0, 1] input range will
// require a very large degree.  To make it more efficient, we reduce the input
// range further by finding an integer idx such that:
//   | n/d - idx/64 | <= 1/128.
// In particular,
//   idx := round(2^6 * n/d)
// Then for the fast pass, we find a polynomial approximation for:
//   atan( n/d ) ~ atan( idx/64 ) + (n/d - idx/64) * Q(n/d - idx/64)
// For the accurate pass, we use the addition formula:
//   atan( n/d ) - atan( idx/64 ) = atan( (n/d - idx/64)/(1 + (n*idx)/(64*d)) )
//                                = atan( (n - d*(idx/64))/(d + n*(idx/64)) )
// And for the fast pass, we use degree-9 Taylor polynomial to compute the RHS:
//   atan(u) ~ P(u) = u - u^3/3 + u^5/5 - u^7/7 + u^9/9
// with absolute errors bounded by:
//   |atan(u) - P(u)| < |u|^11 / 11 < 2^-80
// and relative errors bounded by:
//   |(atan(u) - P(u)) / P(u)| < u^10 / 11 < 2^-73.

LIBC_INLINE constexpr double atan2(double y, double x) {
  using namespace atan_internal;
  using FPBits = fputil::FPBits<double>;

  constexpr double IS_NEG[2] = {1.0, -1.0};
  constexpr DoubleDouble ZERO = {0.0, 0.0};
  constexpr DoubleDouble MZERO = {-0.0, -0.0};
  constexpr DoubleDouble PI = {0x1.1a62633145c07p-53, 0x1.921fb54442d18p+1};
  constexpr DoubleDouble MPI = {-0x1.1a62633145c07p-53, -0x1.921fb54442d18p+1};
  constexpr DoubleDouble PI_OVER_2 = {0x1.1a62633145c07p-54,
                                      0x1.921fb54442d18p0};
  constexpr DoubleDouble MPI_OVER_2 = {-0x1.1a62633145c07p-54,
                                       -0x1.921fb54442d18p0};
  constexpr DoubleDouble PI_OVER_4 = {0x1.1a62633145c07p-55,
                                      0x1.921fb54442d18p-1};
  constexpr DoubleDouble THREE_PI_OVER_4 = {0x1.a79394c9e8a0ap-54,
                                            0x1.2d97c7f3321d2p+1};
  // Adjustment for constant term:
  //   CONST_ADJ[x_sign][y_sign][recip]
  constexpr DoubleDouble CONST_ADJ[2][2][2] = {
      {{ZERO, MPI_OVER_2}, {MZERO, MPI_OVER_2}},
      {{MPI, PI_OVER_2}, {MPI, PI_OVER_2}}};

  FPBits x_bits(x), y_bits(y);
  bool x_sign = x_bits.sign().is_neg();
  bool y_sign = y_bits.sign().is_neg();
  x_bits = x_bits.abs();
  y_bits = y_bits.abs();
  uint64_t x_abs = x_bits.uintval();
  uint64_t y_abs = y_bits.uintval();
  bool recip = x_abs < y_abs;
  uint64_t min_abs = recip ? x_abs : y_abs;
  uint64_t max_abs = !recip ? x_abs : y_abs;
  unsigned min_exp = static_cast<unsigned>(min_abs >> FPBits::FRACTION_LEN);
  unsigned max_exp = static_cast<unsigned>(max_abs >> FPBits::FRACTION_LEN);

  double num = FPBits(min_abs).get_val();
  double den = FPBits(max_abs).get_val();

  // Check for exceptional cases, whether inputs are 0, inf, nan, or close to
  // overflow, or close to underflow.
  if (LIBC_UNLIKELY(max_exp > 0x7ffU - 128U || min_exp < 128U)) {
    if (x_bits.is_nan() || y_bits.is_nan()) {
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan())
        fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    unsigned x_except = x == 0.0 ? 0 : (FPBits(x_abs).is_inf() ? 2 : 1);
    unsigned y_except = y == 0.0 ? 0 : (FPBits(y_abs).is_inf() ? 2 : 1);

    // Exceptional cases:
    //   EXCEPT[y_except][x_except][x_is_neg]
    // with x_except & y_except:
    //   0: zero
    //   1: finite, non-zero
    //   2: infinity
    constexpr DoubleDouble EXCEPTS[3][3][2] = {
        {{ZERO, PI}, {ZERO, PI}, {ZERO, PI}},
        {{PI_OVER_2, PI_OVER_2}, {ZERO, ZERO}, {ZERO, PI}},
        {{PI_OVER_2, PI_OVER_2},
         {PI_OVER_2, PI_OVER_2},
         {PI_OVER_4, THREE_PI_OVER_4}},
    };

    if ((x_except != 1) || (y_except != 1)) {
      DoubleDouble r = EXCEPTS[y_except][x_except][x_sign];
      return fputil::multiply_add(IS_NEG[y_sign], r.hi, IS_NEG[y_sign] * r.lo);
    }
    bool scale_up = min_exp < 128U;
    bool scale_down = max_exp > 0x7ffU - 128U;
    // At least one input is denormal, multiply both numerator and denominator
    // by some large enough power of 2 to normalize denormal inputs.
    if (scale_up) {
      num *= 0x1.0p64;
      if (!scale_down)
        den *= 0x1.0p64;
    } else if (scale_down) {
      den *= 0x1.0p-64;
      if (!scale_up)
        num *= 0x1.0p-64;
    }

    min_abs = FPBits(num).uintval();
    max_abs = FPBits(den).uintval();
    min_exp = static_cast<unsigned>(min_abs >> FPBits::FRACTION_LEN);
    max_exp = static_cast<unsigned>(max_abs >> FPBits::FRACTION_LEN);
  }

  double final_sign = IS_NEG[(x_sign != y_sign) != recip];
  DoubleDouble const_term = CONST_ADJ[x_sign][y_sign][recip];
  unsigned exp_diff = max_exp - min_exp;
  // We have the following bound for normalized n and d:
  //   2^(-exp_diff - 1) < n/d < 2^(-exp_diff + 1).
  if (LIBC_UNLIKELY(exp_diff > 54)) {
    return fputil::multiply_add(final_sign, const_term.hi,
                                final_sign * (const_term.lo + num / den));
  }

  double k = fputil::nearest_integer(64.0 * num / den);
  unsigned idx = static_cast<unsigned>(k);
  // k = idx / 64
  k *= 0x1.0p-6;

  // Range reduction:
  // atan(n/d) - atan(k/64) = atan((n/d - k/64) / (1 + (n/d) * (k/64)))
  //                        = atan((n - d * k/64)) / (d + n * k/64))
  DoubleDouble num_k = fputil::exact_mult(num, k);
  DoubleDouble den_k = fputil::exact_mult(den, k);

  // num_dd = n - d * k
  DoubleDouble num_dd = fputil::exact_add(num - den_k.hi, -den_k.lo);
  // den_dd = d + n * k
  DoubleDouble den_dd = fputil::exact_add(den, num_k.hi);
  den_dd.lo += num_k.lo;

  // q = (n - d * k) / (d + n * k)
  DoubleDouble q = fputil::div(num_dd, den_dd);
  // p ~ atan(q)
  DoubleDouble p = atan_eval(q);

  DoubleDouble r = fputil::add(const_term, fputil::add(ATAN_I[idx], p));
  r.hi *= final_sign;
  r.lo *= final_sign;

  return r.hi + r.lo;
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2_H
