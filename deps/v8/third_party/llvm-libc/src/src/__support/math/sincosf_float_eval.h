//===-- Compute sin + cos for small angles ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINCOSF_FLOAT_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINCOSF_FLOAT_EVAL_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace sincosf_float_eval {

// Since the worst case of `x mod pi` in single precision is > 2^-28, in order
// to be bounded by 1 ULP, the range reduction accuracy will need to be at
// least 2^(-28 - 23) = 2^-51.
// For fast small range reduction, we will compute as follow:
//   Let pi ~ c0 + c1 + c2
// with |c1| < ulp(c0)/2 and |c2| < ulp(c1)/2
// then:
//   k := nearest_int(x * 1/pi);
//   u = (x - k * c0) - k * c1 - k * c2
// We requires k * c0, k * c1 to be exactly representable in single precision.
// Let p_k be the precision of k, then the precision of c0 and c1 are:
//   24 - p_k,
// and the ulp of (k * c2) is 2^(-3 * (24 - p_k)).
// This give us the following bound on the precision of k:
//   3 * (24 - p_k) >= 51,
// or equivalently:
//   p_k <= 7.
// We set the bound for p_k to be 6 so that we can have some more wiggle room
// for computations.
LIBC_INLINE unsigned sincosf_range_reduction_small(float x, float &u) {
  // > display=hexadecimal;
  // > a = round(pi/8, 18, RN);
  // > b = round(pi/8 - a, 18, RN);
  // > c = round(pi/8 - a - b, SG, RN);
  // > round(8/pi, SG, RN);
  constexpr float MPI[3] = {-0x1.921f8p-2f, -0x1.aa22p-21f, -0x1.68c234p-41f};
  constexpr float ONE_OVER_PI = 0x1.45f306p+1f;
  float prod_hi = x * ONE_OVER_PI;
  float k = fputil::nearest_integer(prod_hi);

  float y_hi = fputil::multiply_add(k, MPI[0], x); // Exact
  u = fputil::multiply_add(k, MPI[1], y_hi);
  u = fputil::multiply_add(k, MPI[2], u);
  return static_cast<unsigned>(static_cast<int>(k));
}

// TODO: Add non-FMA version of large range reduction.
LIBC_INLINE unsigned sincosf_range_reduction_large(float x, float &u) {
  // > for i from 0 to 13 do {
  //     if i < 2 then { pi_inv = 0.25 + 2^(8*(i - 2)) / pi; }
  //     else { pi_inv = 2^(8*(i-2)) / pi; };
  //     pn = nearestint(pi_inv);
  //     pi_frac = pi_inv - pn;
  //     a = round(pi_frac, SG, RN);
  //     b = round(pi_frac - a, SG, RN);
  //     c = round(pi_frac - a - b, SG, RN);
  //     d = round(pi_frac - a - b - c, SG, RN);
  //     print("{", 2^3 * a, ",", 2^3 * b, ",", 2^3 * c, ",", 2^3 * d, "},");
  // };
  constexpr float EIGHT_OVER_PI[14][4] = {
      {0x1.000146p1f, -0x1.9f246cp-28f, -0x1.bbead6p-54f, -0x1.ec5418p-85f},
      {0x1.0145f4p1f, -0x1.f246c6p-24f, -0x1.df56bp-49f, -0x1.ec5418p-77f},
      {0x1.45f306p1f, 0x1.b9391p-24f, 0x1.529fc2p-50f, 0x1.d5f47ep-76f},
      {0x1.f306dcp1f, 0x1.391054p-24f, 0x1.4fe13ap-49f, 0x1.7d1f54p-74f},
      {-0x1.f246c6p0f, -0x1.df56bp-25f, -0x1.ec5418p-53f, 0x1.f534dep-78f},
      {-0x1.236378p1f, 0x1.529fc2p-26f, 0x1.d5f47ep-52f, -0x1.65912p-77f},
      {0x1.391054p0f, 0x1.4fe13ap-25f, 0x1.7d1f54p-50f, -0x1.6447e4p-75f},
      {0x1.1054a8p0f, -0x1.ec5418p-29f, 0x1.f534dep-54f, -0x1.f924ecp-81f},
      {0x1.529fc2p-2f, 0x1.d5f47ep-28f, -0x1.65912p-53f, 0x1.b6c52cp-79f},
      {-0x1.ac07b2p1f, 0x1.5f47d4p-24f, 0x1.a6ee06p-49f, 0x1.b6295ap-74f},
      {-0x1.ec5418p-5f, 0x1.f534dep-30f, -0x1.f924ecp-57f, 0x1.5993c4p-82f},
      {0x1.3abe9p-1f, -0x1.596448p-27f, 0x1.b6c52cp-55f, -0x1.9b0ef2p-80f},
      {-0x1.505c16p1f, 0x1.a6ee06p-25f, 0x1.b6295ap-50f, -0x1.b0ef1cp-76f},
      {-0x1.70565ap-1f, 0x1.dc0db6p-26f, 0x1.4acc9ep-53f, 0x1.0e4108p-80f},
  };

  using FPBits = typename fputil::FPBits<float>;
  using fputil::FloatFloat;
  FPBits xbits(x);

  int x_e_m32 = xbits.get_biased_exponent() - (FPBits::EXP_BIAS + 32);
  unsigned idx = static_cast<unsigned>((x_e_m32 >> 3) + 2);
  // Scale x down by 2^(-(8 * (idx - 2))
  xbits.set_biased_exponent((x_e_m32 & 7) + FPBits::EXP_BIAS + 32);
  // 2^32 <= |x_reduced| < 2^(32 + 8) = 2^40
  float x_reduced = xbits.get_val();
  // x * c_hi = ph.hi + ph.lo exactly.
  FloatFloat ph = fputil::exact_mult<float>(x_reduced, EIGHT_OVER_PI[idx][0]);
  // x * c_mid = pm.hi + pm.lo exactly.
  FloatFloat pm = fputil::exact_mult<float>(x_reduced, EIGHT_OVER_PI[idx][1]);
  // x * c_lo = pl.hi + pl.lo exactly.
  FloatFloat pl = fputil::exact_mult<float>(x_reduced, EIGHT_OVER_PI[idx][2]);
  // Extract integral parts and fractional parts of (ph.lo + pm.hi).
  float sum_hi = ph.lo + pm.hi;
  float k = fputil::nearest_integer(sum_hi);

  // x * 8/pi mod 1 ~ y_hi + y_mid + y_lo
  float y_hi = (ph.lo - k) + pm.hi; // Exact
  FloatFloat y_mid = fputil::exact_add(pm.lo, pl.hi);
  float y_lo = pl.lo;

  // y_l = x * c_lo_2 + pl.lo
  float y_l = fputil::multiply_add(x_reduced, EIGHT_OVER_PI[idx][3], y_lo);
  FloatFloat y = fputil::exact_add(y_hi, y_mid.hi);
  y.lo += (y_mid.lo + y_l);

  // Digits of pi/8, generated by Sollya with:
  // > a = round(pi/8, SG, RN);
  // > b = round(pi/8 - SG, D, RN);
  constexpr FloatFloat PI_OVER_8 = {-0x1.777a5cp-27f, 0x1.921fb6p-2f};

  // Error bound: with {a} denote the fractional part of a, i.e.:
  //   {a} = a - round(a)
  // Then,
  //   | {x * 8/pi} - (y_hi + y_lo) | <=  ulp(ulp(y_hi)) <= 2^-47
  //   | {x mod pi/8} - (u.hi + u.lo) | < 2 * 2^-5 * 2^-47 = 2^-51
  u = fputil::multiply_add(y.hi, PI_OVER_8.hi, y.lo * PI_OVER_8.hi);

  return static_cast<unsigned>(static_cast<int>(k));
}

template <bool IS_SIN> LIBC_INLINE float sincosf_eval(float x) {
  // sin(k * pi/8) for k = 0..15, generated by Sollya with:
  // > for k from 0 to 16 do {
  //     print(round(sin(k * pi/8), SG, RN));
  // };
  constexpr float SIN_K_PI_OVER_8[16] = {
      0.0f,  0x1.87de2ap-2f,  0x1.6a09e6p-1f,  0x1.d906bcp-1f,
      1.0f,  0x1.d906bcp-1f,  0x1.6a09e6p-1f,  0x1.87de2ap-2f,
      0.0f,  -0x1.87de2ap-2f, -0x1.6a09e6p-1f, -0x1.d906bcp-1f,
      -1.0f, -0x1.d906bcp-1f, -0x1.6a09e6p-1f, -0x1.87de2ap-2f,
  };

  using FPBits = fputil::FPBits<float>;
  FPBits xbits(x);
  uint32_t x_abs = cpp::bit_cast<uint32_t>(x) & 0x7fff'ffffU;

  float y;
  unsigned k = 0;
  if (x_abs < 0x4880'0000U) {
    k = sincosf_range_reduction_small(x, y);
  } else {

    if (LIBC_UNLIKELY(x_abs >= 0x7f80'0000U)) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      if (x_abs == 0x7f80'0000U) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
      }
      return x + FPBits::quiet_nan().get_val();
    }

    k = sincosf_range_reduction_large(x, y);
  }

  float sin_k = SIN_K_PI_OVER_8[k & 15];
  // cos(k * pi/8) = sin(k * pi/8 + pi/2) = sin((k + 4) * pi/8).
  // cos_k = cos(k * pi/8)
  float cos_k = SIN_K_PI_OVER_8[(k + 4) & 15];

  float y_sq = y * y;

  // Polynomial approximation of sin(y) and cos(y) for |y| <= pi/16:
  //
  // Using Taylor polynomial for sin(y):
  //   sin(y) ~ y - y^3 / 6 + y^5 / 120
  // Using minimax polynomial generated by Sollya for cos(y) with:
  // > Q = fpminimax(cos(x), [|0, 2, 4|], [|1, SG...|], [0, pi/16]);
  //
  // Error bounds:
  // * For sin(y)
  // > P = x - SG(1/6)*x^3 + SG(1/120) * x^5;
  // > dirtyinfnorm((sin(x) - P)/sin(x), [-pi/16, pi/16]);
  // 0x1.825...p-27
  // * For cos(y)
  // > Q = fpminimax(cos(x), [|0, 2, 4|], [|1, SG...|], [0, pi/16]);
  // > dirtyinfnorm((sin(x) - P)/sin(x), [-pi/16, pi/16]);
  // 0x1.aa8...p-29

  // p1 = y^2 * 1/120 - 1/6
  float p1 = fputil::multiply_add(y_sq, 0x1.111112p-7f, -0x1.555556p-3f);
  // q1 = y^2 * coeff(Q, 4) + coeff(Q, 2)
  float q1 = fputil::multiply_add(y_sq, 0x1.54b8bep-5f, -0x1.ffffc4p-2f);
  float y3 = y_sq * y;
  // c1 ~ cos(y)
  float c1 = fputil::multiply_add(y_sq, q1, 1.0f);
  // s1 ~ sin(y)
  float s1 = fputil::multiply_add(y3, p1, y);

  if constexpr (IS_SIN) {
    // sin(x) = cos(k * pi/8) * sin(y) + sin(k * pi/8) * cos(y).
    return fputil::multiply_add(cos_k, s1, sin_k * c1);
  } else {
    // cos(x) = cos(k * pi/8) * cos(y) - sin(k * pi/8) * sin(y).
    return fputil::multiply_add(cos_k, c1, -sin_k * s1);
  }
}

} // namespace sincosf_float_eval

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINCOSF_FLOAT_EVAL_H
