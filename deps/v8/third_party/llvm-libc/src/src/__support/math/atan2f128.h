//===-- Implementation header for atan2f128 ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F128_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "atan_utils.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/integer_literals.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/uint128.h"

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
// And for the fast pass, we use degree-13 minimax polynomial to compute the
// RHS:
//   atan(u) ~ P(u) = u - c_3 * u^3 + c_5 * u^5 - c_7 * u^7 + c_9 *u^9 -
//                    - c_11 * u^11 + c_13 * u^13
// with absolute errors bounded by:
//   |atan(u) - P(u)| < 2^-121
// and relative errors bounded by:
//   |(atan(u) - P(u)) / P(u)| < 2^-114.

LIBC_INLINE constexpr float128 atan2f128(float128 y, float128 x) {
  using Float128 = fputil::DyadicFloat<128>;

  constexpr Float128 ZERO = {Sign::POS, 0, 0_u128};
  constexpr Float128 MZERO = {Sign::NEG, 0, 0_u128};
  constexpr Float128 PI = {Sign::POS, -126,
                           0xc90fdaa2'2168c234'c4c6628b'80dc1cd1_u128};
  constexpr Float128 MPI = {Sign::NEG, -126,
                            0xc90fdaa2'2168c234'c4c6628b'80dc1cd1_u128};
  constexpr Float128 PI_OVER_2 = {Sign::POS, -127,
                                  0xc90fdaa2'2168c234'c4c6628b'80dc1cd1_u128};
  constexpr Float128 MPI_OVER_2 = {Sign::NEG, -127,
                                   0xc90fdaa2'2168c234'c4c6628b'80dc1cd1_u128};
  constexpr Float128 PI_OVER_4 = {Sign::POS, -128,
                                  0xc90fdaa2'2168c234'c4c6628b'80dc1cd1_u128};
  constexpr Float128 THREE_PI_OVER_4 = {
      Sign::POS, -128, 0x96cbe3f9'990e91a7'9394c9e8'a0a5159d_u128};

  // Adjustment for constant term:
  //   CONST_ADJ[x_sign][y_sign][recip]
  constexpr Float128 CONST_ADJ[2][2][2] = {
      {{ZERO, MPI_OVER_2}, {MZERO, MPI_OVER_2}},
      {{MPI, PI_OVER_2}, {MPI, PI_OVER_2}}};

  using namespace atan_internal;
  using FPBits = fputil::FPBits<float128>;
  using Float128 = fputil::DyadicFloat<128>;

  FPBits x_bits(x), y_bits(y);
  bool x_sign = x_bits.sign().is_neg();
  bool y_sign = y_bits.sign().is_neg();
  x_bits = x_bits.abs();
  y_bits = y_bits.abs();
  UInt128 x_abs = x_bits.uintval();
  UInt128 y_abs = y_bits.uintval();
  bool recip = x_abs < y_abs;
  UInt128 min_abs = recip ? x_abs : y_abs;
  UInt128 max_abs = !recip ? x_abs : y_abs;
  unsigned min_exp = static_cast<unsigned>(min_abs >> FPBits::FRACTION_LEN);
  unsigned max_exp = static_cast<unsigned>(max_abs >> FPBits::FRACTION_LEN);

  Float128 num(FPBits(min_abs).get_val());
  Float128 den(FPBits(max_abs).get_val());

  // Check for exceptional cases, whether inputs are 0, inf, nan, or close to
  // overflow, or close to underflow.
  if (LIBC_UNLIKELY(max_exp >= 0x7fffU || min_exp == 0U)) {
    if (x_bits.is_nan() || y_bits.is_nan())
      return FPBits::quiet_nan().get_val();
    unsigned x_except = x == 0 ? 0 : (FPBits(x_abs).is_inf() ? 2 : 1);
    unsigned y_except = y == 0 ? 0 : (FPBits(y_abs).is_inf() ? 2 : 1);

    // Exceptional cases:
    //   EXCEPT[y_except][x_except][x_is_neg]
    // with x_except & y_except:
    //   0: zero
    //   1: finite, non-zero
    //   2: infinity
    constexpr Float128 EXCEPTS[3][3][2] = {
        {{ZERO, PI}, {ZERO, PI}, {ZERO, PI}},
        {{PI_OVER_2, PI_OVER_2}, {ZERO, ZERO}, {ZERO, PI}},
        {{PI_OVER_2, PI_OVER_2},
         {PI_OVER_2, PI_OVER_2},
         {PI_OVER_4, THREE_PI_OVER_4}},
    };

    if ((x_except != 1) || (y_except != 1)) {
      Float128 r = EXCEPTS[y_except][x_except][x_sign];
      if (y_sign)
        r.sign = r.sign.negate();
      return static_cast<float128>(r);
    }
  }

  bool final_sign = ((x_sign != y_sign) != recip);
  Float128 const_term = CONST_ADJ[x_sign][y_sign][recip];
  int exp_diff = den.exponent - num.exponent;
  // We have the following bound for normalized n and d:
  //   2^(-exp_diff - 1) < n/d < 2^(-exp_diff + 1).
  if (LIBC_UNLIKELY(exp_diff > FPBits::FRACTION_LEN + 2)) {
    Float128 quotient = rounded_div(num, den);
    Float128 result = quick_add(const_term, quotient);
    if (final_sign)
      result.sign = result.sign.negate();
    return static_cast<float128>(result);
  }

  // Take 24 leading bits of num and den to convert to float for fast division.
  // We also multiply the numerator by 64 using integer addition directly to the
  // exponent field.
  float num_f =
      cpp::bit_cast<float>(static_cast<uint32_t>(num.mantissa >> 104) +
                           (6U << fputil::FPBits<float>::FRACTION_LEN));
  float den_f = cpp::bit_cast<float>(
      static_cast<uint32_t>(den.mantissa >> 104) +
      (static_cast<uint32_t>(exp_diff) << fputil::FPBits<float>::FRACTION_LEN));

  float k = fputil::nearest_integer(num_f / den_f);
  unsigned idx = static_cast<unsigned>(k);

  // k_f128 = idx / 64
  Float128 k_f128(Sign::POS, -6, Float128::MantissaType(idx));

  // Range reduction:
  // atan(n/d) - atan(k) = atan((n/d - k/64) / (1 + (n/d) * (k/64)))
  //                     = atan((n - d * k/64)) / (d + n * k/64))
  // num_f128 = n - d * k/64
  Float128 num_f128 = fputil::multiply_add(den, -k_f128, num);
  // den_f128 = d + n * k/64
  Float128 den_f128 = fputil::multiply_add(num, k_f128, den);

  // q = (n - d * k) / (d + n * k)
  Float128 q = fputil::quick_mul(num_f128, fputil::approx_reciprocal(den_f128));
  // p ~ atan(q)
  Float128 p = atan_eval(q);

  Float128 r =
      fputil::quick_add(const_term, fputil::quick_add(ATAN_I_F128[idx], p));
  if (final_sign)
    r.sign = r.sign.negate();

  return static_cast<float128>(r);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F128_H
