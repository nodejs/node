//===-- Compute sin + cos for small angles ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINCOS_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINCOS_EVAL_H

#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/integer_literals.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace sincos_eval_internal {

using fputil::DoubleDouble;
using Float128 = fputil::DyadicFloat<128>;

LIBC_INLINE double sincos_eval(const DoubleDouble &u, DoubleDouble &sin_u,
                               DoubleDouble &cos_u) {
  // Evaluate sin(y) = sin(x - k * (pi/128))
  // We use the degree-7 Taylor approximation:
  //   sin(y) ~ y - y^3/3! + y^5/5! - y^7/7!
  // Then the error is bounded by:
  //   |sin(y) - (y - y^3/3! + y^5/5! - y^7/7!)| < |y|^9/9! < 2^-54/9! < 2^-72.
  // For y ~ u_hi + u_lo, fully expanding the polynomial and drop any terms
  // < ulp(u_hi^3) gives us:
  //   y - y^3/3! + y^5/5! - y^7/7! = ...
  // ~ u_hi + u_hi^3 * (-1/6 + u_hi^2 * (1/120 - u_hi^2 * 1/5040)) +
  //        + u_lo (1 + u_hi^2 * (-1/2 + u_hi^2 / 24))
  double u_hi_sq = u.hi * u.hi; // Error < ulp(u_hi^2) < 2^(-6 - 52) = 2^-58.
  // p1 ~ 1/120 + u_hi^2 / 5040.
  double p1 = fputil::multiply_add(u_hi_sq, -0x1.a01a01a01a01ap-13,
                                   0x1.1111111111111p-7);
  // q1 ~ -1/2 + u_hi^2 / 24.
  double q1 = fputil::multiply_add(u_hi_sq, 0x1.5555555555555p-5, -0x1.0p-1);
  double u_hi_3 = u_hi_sq * u.hi;
  // p2 ~ -1/6 + u_hi^2 (1/120 - u_hi^2 * 1/5040)
  double p2 = fputil::multiply_add(u_hi_sq, p1, -0x1.5555555555555p-3);
  // q2 ~ 1 + u_hi^2 (-1/2 + u_hi^2 / 24)
  double q2 = fputil::multiply_add(u_hi_sq, q1, 1.0);
  double sin_lo = fputil::multiply_add(u_hi_3, p2, u.lo * q2);
  // Overall, |sin(y) - (u_hi + sin_lo)| < 2*ulp(u_hi^3) < 2^-69.

  // Evaluate cos(y) = cos(x - k * (pi/128))
  // We use the degree-8 Taylor approximation:
  //   cos(y) ~ 1 - y^2/2 + y^4/4! - y^6/6! + y^8/8!
  // Then the error is bounded by:
  //   |cos(y) - (...)| < |y|^10/10! < 2^-81
  // For y ~ u_hi + u_lo, fully expanding the polynomial and drop any terms
  // < ulp(u_hi^3) gives us:
  //   1 - y^2/2 + y^4/4! - y^6/6! + y^8/8! = ...
  // ~ 1 - u_hi^2/2 + u_hi^4(1/24 + u_hi^2 (-1/720 + u_hi^2/40320)) +
  //     + u_hi u_lo (-1 + u_hi^2/6)
  // We compute 1 - u_hi^2 accurately:
  //   v_hi + v_lo ~ 1 - u_hi^2/2
  // with error <= 2^-105.
  double u_hi_neg_half = (-0.5) * u.hi;
  DoubleDouble v;

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  v.hi = fputil::multiply_add(u.hi, u_hi_neg_half, 1.0);
  v.lo = 1.0 - v.hi; // Exact
  v.lo = fputil::multiply_add(u.hi, u_hi_neg_half, v.lo);
#else
  DoubleDouble u_hi_sq_neg_half = fputil::exact_mult(u.hi, u_hi_neg_half);
  v = fputil::exact_add(1.0, u_hi_sq_neg_half.hi);
  v.lo += u_hi_sq_neg_half.lo;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  // r1 ~ -1/720 + u_hi^2 / 40320
  double r1 = fputil::multiply_add(u_hi_sq, 0x1.a01a01a01a01ap-16,
                                   -0x1.6c16c16c16c17p-10);
  // s1 ~ -1 + u_hi^2 / 6
  double s1 = fputil::multiply_add(u_hi_sq, 0x1.5555555555555p-3, -1.0);
  double u_hi_4 = u_hi_sq * u_hi_sq;
  double u_hi_u_lo = u.hi * u.lo;
  // r2 ~ 1/24 + u_hi^2 (-1/720 + u_hi^2 / 40320)
  double r2 = fputil::multiply_add(u_hi_sq, r1, 0x1.5555555555555p-5);
  // s2 ~ v_lo + u_hi * u_lo * (-1 + u_hi^2 / 6)
  double s2 = fputil::multiply_add(u_hi_u_lo, s1, v.lo);
  double cos_lo = fputil::multiply_add(u_hi_4, r2, s2);
  // Overall, |cos(y) - (v_hi + cos_lo)| < 2*ulp(u_hi^4) < 2^-75.

  sin_u = fputil::exact_add(u.hi, sin_lo);
  cos_u = fputil::exact_add(v.hi, cos_lo);

  return fputil::multiply_add(fputil::FPBits<double>(u_hi_3).abs().get_val(),
                              0x1.0p-51, 0x1.0p-105);
}

LIBC_INLINE void sincos_eval(const Float128 &u, Float128 &sin_u,
                             Float128 &cos_u) {
  Float128 u_sq = fputil::quick_mul(u, u);

  // sin(u) ~ x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - x^11/11! + x^13/13!
  constexpr Float128 SIN_COEFFS[] = {
      {Sign::POS, -127, 0x80000000'00000000'00000000'00000000_u128}, // 1
      {Sign::NEG, -130, 0xaaaaaaaa'aaaaaaaa'aaaaaaaa'aaaaaaab_u128}, // -1/3!
      {Sign::POS, -134, 0x88888888'88888888'88888888'88888889_u128}, // 1/5!
      {Sign::NEG, -140, 0xd00d00d0'0d00d00d'00d00d00'd00d00d0_u128}, // -1/7!
      {Sign::POS, -146, 0xb8ef1d2a'b6399c7d'560e4472'800b8ef2_u128}, // 1/9!
      {Sign::NEG, -153, 0xd7322b3f'aa271c7f'3a3f25c1'bee38f10_u128}, // -1/11!
      {Sign::POS, -160, 0xb092309d'43684be5'1c198e91'd7b4269e_u128}, // 1/13!
  };

  // cos(u) ~ 1 - x^2/2 + x^4/4! - x^6/6! + x^8/8! - x^10/10! + x^12/12!
  constexpr Float128 COS_COEFFS[] = {
      {Sign::POS, -127, 0x80000000'00000000'00000000'00000000_u128}, // 1.0
      {Sign::NEG, -128, 0x80000000'00000000'00000000'00000000_u128}, // 1/2
      {Sign::POS, -132, 0xaaaaaaaa'aaaaaaaa'aaaaaaaa'aaaaaaab_u128}, // 1/4!
      {Sign::NEG, -137, 0xb60b60b6'0b60b60b'60b60b60'b60b60b6_u128}, // 1/6!
      {Sign::POS, -143, 0xd00d00d0'0d00d00d'00d00d00'd00d00d0_u128}, // 1/8!
      {Sign::NEG, -149, 0x93f27dbb'c4fae397'780b69f5'333c725b_u128}, // 1/10!
      {Sign::POS, -156, 0x8f76c77f'c6c4bdaa'26d4c3d6'7f425f60_u128}, // 1/12!
  };

  sin_u = fputil::quick_mul(u, fputil::polyeval(u_sq, SIN_COEFFS[0],
                                                SIN_COEFFS[1], SIN_COEFFS[2],
                                                SIN_COEFFS[3], SIN_COEFFS[4],
                                                SIN_COEFFS[5], SIN_COEFFS[6]));
  cos_u = fputil::polyeval(u_sq, COS_COEFFS[0], COS_COEFFS[1], COS_COEFFS[2],
                           COS_COEFFS[3], COS_COEFFS[4], COS_COEFFS[5],
                           COS_COEFFS[6]);
}

} // namespace sincos_eval_internal

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINCOS_EVAL_H
