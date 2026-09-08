//===-- Implementation header for lgammaf -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LGAMMAF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LGAMMAF_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/gamma_util.h"
#include "src/__support/math/log.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace lgammaf_internal {

// P_M2(d), d = t - 1.5, approximates lgamma(t)/((t-1)(t-2)) on [1, 2].
// Degree 18 centered-monomial coefficients from a 250-bit Chebyshev fit,
// max relative error 2^-51.6
LIBC_INLINE double lgamma_m2_poly(double d) {
  constexpr double POLY_M2[19] = {
      0x1.eeb95b094c191p-2,   -0x1.2aed059bd613dp-3,  0x1.01af62a292eb9p-4,
      -0x1.007aa83cc458bp-5,  0x1.13342351db068p-6,   -0x1.34dbda42244dep-7,
      0x1.64f066cf96646p-8,   -0x1.a5033bab00972p-9,  0x1.f814855fdfeb8p-10,
      -0x1.31418be9f2e14p-10, 0x1.7510b1d1fa536p-11,  -0x1.caccf4395927bp-12,
      0x1.1c0d19e4fef05p-12,  -0x1.67d43c4be68f7p-13, 0x1.c3d6281f9ac35p-14,
      -0x1.dd9e09255e712p-15, 0x1.2550c8f33e895p-15,  -0x1.6cd0f980284cbp-15,
      0x1.dbd1c21b8dfa7p-16};
  double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4, d16 = d8 * d8;
  double p01 = fputil::multiply_add(d, POLY_M2[1], POLY_M2[0]);
  double p23 = fputil::multiply_add(d, POLY_M2[3], POLY_M2[2]);
  double p45 = fputil::multiply_add(d, POLY_M2[5], POLY_M2[4]);
  double p67 = fputil::multiply_add(d, POLY_M2[7], POLY_M2[6]);
  double p89 = fputil::multiply_add(d, POLY_M2[9], POLY_M2[8]);
  double p1011 = fputil::multiply_add(d, POLY_M2[11], POLY_M2[10]);
  double p1213 = fputil::multiply_add(d, POLY_M2[13], POLY_M2[12]);
  double p1415 = fputil::multiply_add(d, POLY_M2[15], POLY_M2[14]);
  double p1617 = fputil::multiply_add(d, POLY_M2[17], POLY_M2[16]);
  double q03 = fputil::multiply_add(d2, p23, p01);
  double q47 = fputil::multiply_add(d2, p67, p45);
  double q811 = fputil::multiply_add(d2, p1011, p89);
  double q1215 = fputil::multiply_add(d2, p1415, p1213);
  double q1618 = fputil::multiply_add(d2, POLY_M2[18], p1617);
  double r07 = fputil::multiply_add(d4, q47, q03);
  double r815 = fputil::multiply_add(d4, q1215, q811);
  double s015 = fputil::multiply_add(d8, r815, r07);
  return fputil::multiply_add(d16, q1618, s015);
}

} // namespace lgammaf_internal

LIBC_INLINE float lgammaf(float x) {
  using namespace gamma_internal;
  using namespace lgammaf_internal;
  using FPBits = fputil::FPBits<float>;

  FPBits xbits(x);
  uint32_t x_abs = xbits.abs().uintval();

  // NaN / Inf
  if (LIBC_UNLIKELY(x_abs >= 0x7f800000u)) {
    if (x_abs == 0x7f800000u)
      return FPBits::inf().get_val();
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    return x;
  }

  // +/- 0 -> +Inf pole
  if (LIBC_UNLIKELY(x_abs == 0)) {
    fputil::raise_except_if_required(FE_DIVBYZERO);
    fputil::set_errno_if_required(ERANGE);
    return FPBits::inf().get_val();
  }

  // Negative integers and lgamma(1) = lgamma(2) = 0.
  if (LIBC_UNLIKELY(is_integer(x))) {
    if (xbits.is_neg()) {
      fputil::raise_except_if_required(FE_DIVBYZERO);
      fputil::set_errno_if_required(ERANGE);
      return FPBits::inf().get_val();
    }
    if (x_abs == 0x3f800000u || x_abs == 0x40000000u)
      return FPBits::zero().get_val();
  }

  double xd = fputil::cast<double>(x);
  double abs_xd = xd < 0.0 ? -xd : xd;
  double lgamma_val;

  // For very tiny |x| (< 2^-23), use the truncated Laurent series:
  //   lgamma(x)  = -log(x) - gamma*x + O(x^2)  for tiny x > 0
  //   lgamma(-y) = -log(y) + gamma*y + O(y^2)  for tiny y > 0
  // Even though gamma*|x| < 2^-25 is below 1 float ULP of |result|, it tips
  // rounding at boundary cases. The x^2 term is at most gamma*2^-46 << 2^-50.
  if (x_abs < 0x34000000u) { // |x| < 2^-23
    constexpr fputil::ExceptValues<float, 4> LGAMMAF_EXCEPTS_TINY{{
        // input,      toward-zero result, RU, RD, RN
        {0x9b7679ffu, 0x4247c72cu, 1, 0, 1},
        {0x9e88452du, 0x4236bd8bu, 1, 0, 1},
        {0xa77a8e47u, 0x42052b94u, 1, 0, 1},
        {0xb0e17820u, 0x41a1d37bu, 1, 0, 0},
    }};
    if (auto r = LGAMMAF_EXCEPTS_TINY.lookup(xbits.uintval());
        LIBC_UNLIKELY(r.has_value()))
      return r.value();
    constexpr double EULER_GAMMA = 0x1.2788cfc6fb619p-1;
    double sign_corr = xbits.is_neg() ? EULER_GAMMA : -EULER_GAMMA;
    return fputil::cast<float>(
        fputil::multiply_add(sign_corr, abs_xd, -math::log(abs_xd)));
  }

  if (x_abs < 0x3f290000u) {
    if (xbits.is_neg()) {
      // Small negative: x in (-0.66015625, -2^-23). Degree-25 monomial fit
      // of g(x) = (lgamma(x) + log(-x)) / x (smooth on [-0.66, 0]), then
      // lgamma(x) = x*g(x) - log(-x). d = x - MID is exact.
      constexpr double MID_SN = -0x1.5200000000000p-2;
      constexpr double POLY_SN[26] = {
          -0x1.cf99908bbb1d7p-1,  0x1.386ced0346de0p+0,  -0x1.d0b4c2d337e0fp-1,
          0x1.db2dcaaa022a9p-1,   -0x1.1188790a072bap+0, 0x1.4e50420966212p+0,
          -0x1.a787465391482p+0,  0x1.12d6375bce19ep+1,  -0x1.6b05729f76a9cp+1,
          0x1.e602a94d73479p+1,   -0x1.48d5f84ddbdefp+2, 0x1.c0e7f7d05fe6dp+2,
          -0x1.34d52338a0d93p+3,  0x1.ab7e04eff787dp+3,  -0x1.2724d2ace1c81p+4,
          0x1.9b17bee5ae20ap+4,   -0x1.34beabd4deff5p+5, 0x1.bc510ff62dcf0p+5,
          -0x1.7406b7594b6ddp+5,  0x1.b97b7265194a1p+5,  -0x1.5e2ba44fee700p+8,
          0x1.18310d7d43c46p+9,   0x1.59472c8ca4419p+9,  -0x1.2a61147e20ef1p+10,
          -0x1.7cfcf784946edp+11, 0x1.2436ec1277b00p+12};
      double d = xd - MID_SN;
      double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4, d16 = d8 * d8;
      double p01 = fputil::multiply_add(d, POLY_SN[1], POLY_SN[0]);
      double p23 = fputil::multiply_add(d, POLY_SN[3], POLY_SN[2]);
      double p45 = fputil::multiply_add(d, POLY_SN[5], POLY_SN[4]);
      double p67 = fputil::multiply_add(d, POLY_SN[7], POLY_SN[6]);
      double p89 = fputil::multiply_add(d, POLY_SN[9], POLY_SN[8]);
      double p1011 = fputil::multiply_add(d, POLY_SN[11], POLY_SN[10]);
      double p1213 = fputil::multiply_add(d, POLY_SN[13], POLY_SN[12]);
      double p1415 = fputil::multiply_add(d, POLY_SN[15], POLY_SN[14]);
      double p1617 = fputil::multiply_add(d, POLY_SN[17], POLY_SN[16]);
      double p1819 = fputil::multiply_add(d, POLY_SN[19], POLY_SN[18]);
      double p2021 = fputil::multiply_add(d, POLY_SN[21], POLY_SN[20]);
      double p2223 = fputil::multiply_add(d, POLY_SN[23], POLY_SN[22]);
      double p2425 = fputil::multiply_add(d, POLY_SN[25], POLY_SN[24]);
      double q03 = fputil::multiply_add(d2, p23, p01);
      double q47 = fputil::multiply_add(d2, p67, p45);
      double q811 = fputil::multiply_add(d2, p1011, p89);
      double q1215 = fputil::multiply_add(d2, p1415, p1213);
      double q1619 = fputil::multiply_add(d2, p1819, p1617);
      double q2023 = fputil::multiply_add(d2, p2223, p2021);
      double r07 = fputil::multiply_add(d4, q47, q03);
      double r815 = fputil::multiply_add(d4, q1215, q811);
      double r1623 = fputil::multiply_add(d4, q2023, q1619);
      double s015 = fputil::multiply_add(d8, r815, r07);
      double s1625 = fputil::multiply_add(d8, p2425, r1623);
      double poly_g = fputil::multiply_add(d16, s1625, s015);
      // lgamma(x) = x*g(x) - log(-x) with single rounding via FMA.
      lgamma_val = fputil::multiply_add(xd, poly_g, -math::log(abs_xd));
    } else {
      // x = 0x1.f8a754p-9f
      if (LIBC_UNLIKELY(xbits.uintval() == 0x3b7c53aau))
        return fputil::round_result_slightly_up(0x1.63acc2p+2f);
      // Small: t = x < 0.66015625. Degree-17 monomial fit of
      // h(t) = (lgamma(t) + log(t)) / t (smooth on [0, 0.66]).
      // d = t - MID is exact.
      constexpr double MID_S = 0x1.5200000000000p-2;
      constexpr double POLY_S[18] = {
          -0x1.5dcd7586bfd88p-2, 0x1.3f88851c787c3p-1,   -0x1.cdfaca3081737p-3,
          0x1.cfac7b321198bp-4,  -0x1.0939a239f89b2p-4,  0x1.44cddc5f52a43p-5,
          -0x1.9e4cc54acdcebp-6, 0x1.0f61691962355p-6,   -0x1.6a429a6c89071p-7,
          0x1.ea573d148429cp-8,  -0x1.4f6ad0c206ae6p-8,  0x1.cedaf9fc19cb5p-9,
          -0x1.42333c4e88f7dp-9, 0x1.c2a0e3ffc858fp-10,  -0x1.32153d4cb9316p-10,
          0x1.add42f56d3c7ap-11, -0x1.972ff2b1a91edp-11, 0x1.25f56c66e3049p-11};
      double d = abs_xd - MID_S;
      double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4, d16 = d8 * d8;
      double p01 = fputil::multiply_add(d, POLY_S[1], POLY_S[0]);
      double p23 = fputil::multiply_add(d, POLY_S[3], POLY_S[2]);
      double p45 = fputil::multiply_add(d, POLY_S[5], POLY_S[4]);
      double p67 = fputil::multiply_add(d, POLY_S[7], POLY_S[6]);
      double p89 = fputil::multiply_add(d, POLY_S[9], POLY_S[8]);
      double p1011 = fputil::multiply_add(d, POLY_S[11], POLY_S[10]);
      double p1213 = fputil::multiply_add(d, POLY_S[13], POLY_S[12]);
      double p1415 = fputil::multiply_add(d, POLY_S[15], POLY_S[14]);
      double p1617 = fputil::multiply_add(d, POLY_S[17], POLY_S[16]);
      double q03 = fputil::multiply_add(d2, p23, p01);
      double q47 = fputil::multiply_add(d2, p67, p45);
      double q811 = fputil::multiply_add(d2, p1011, p89);
      double q1215 = fputil::multiply_add(d2, p1415, p1213);
      double r07 = fputil::multiply_add(d4, q47, q03);
      double r815 = fputil::multiply_add(d4, q1215, q811);
      double s015 = fputil::multiply_add(d8, r815, r07);
      double poly_h = fputil::multiply_add(d16, p1617, s015);
      // poly_val - log(abs_xd) with single rounding via FMA.
      lgamma_val = fputil::multiply_add(abs_xd, poly_h, -math::log(abs_xd));
    }
  } else if (x_abs < 0x3f800000u) {
    if (xbits.is_neg()) {
      // x in (-1, -0.66015625]: Gamma(x) = Gamma(x+2)/(x(x+1)), so
      // lgamma(x) = x(x+1)*P_M2(x+0.5) - log(-x(x+1)). x+0.5 and x+1 are
      // exact (Sterbenz); x(x+1) = (t-1)(t-2) is the M2 prefactor.
      double w = xd * (xd + 1.0);
      double poly = lgamma_m2_poly(xd + 0.5);
      lgamma_val = fputil::multiply_add(w, poly, -lg_ln(-w));
    } else {
      // M1: t in [0.66015625, 1.0). lgamma(t) = (t-1) * P_M1(d), d = t - MID.
      // Degree-14 monomial fit of lgamma(t)/(t-1), max error 2^-51.3.
      constexpr double MID_M1 = 0x1.a900000000000p-1;
      constexpr double POLY_M1[15] = {
          -0x1.75cb89aad8dc7p-1, 0x1.f958114ec790dp-1,  -0x1.2b968b172a05cp-1,
          0x1.eaf0e2c8dc49fp-2,  -0x1.c6efb493bfadfp-2, 0x1.c0a621f0e593ep-2,
          -0x1.cb2416349da49p-2, 0x1.e19328033256dp-2,  -0x1.010d0c64743f5p-1,
          0x1.162e14c80bdc9p-1,  -0x1.3034dec6eb9a3p-1, 0x1.4ca601da8405ep-1,
          -0x1.70f73638718d4p-1, 0x1.ded7569ab5a36p-1,  -0x1.0fdd39040adfbp+0};
      double d = abs_xd - MID_M1;
      double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4;
      double p01 = fputil::multiply_add(d, POLY_M1[1], POLY_M1[0]);
      double p23 = fputil::multiply_add(d, POLY_M1[3], POLY_M1[2]);
      double p45 = fputil::multiply_add(d, POLY_M1[5], POLY_M1[4]);
      double p67 = fputil::multiply_add(d, POLY_M1[7], POLY_M1[6]);
      double p89 = fputil::multiply_add(d, POLY_M1[9], POLY_M1[8]);
      double p1011 = fputil::multiply_add(d, POLY_M1[11], POLY_M1[10]);
      double p1213 = fputil::multiply_add(d, POLY_M1[13], POLY_M1[12]);
      double q03 = fputil::multiply_add(d2, p23, p01);
      double q47 = fputil::multiply_add(d2, p67, p45);
      double q811 = fputil::multiply_add(d2, p1011, p89);
      double q1214 = fputil::multiply_add(d2, POLY_M1[14], p1213);
      double r07 = fputil::multiply_add(d4, q47, q03);
      double r814 = fputil::multiply_add(d4, q1214, q811);
      double poly = fputil::multiply_add(d8, r814, r07);
      lgamma_val = (abs_xd - 1.0) * poly;
    }
  } else if (x_abs < 0x40000000u) {
    if (xbits.is_neg()) {
      // x in (-2, -1): Gamma(x) = Gamma(x+3)/(x(x+1)(x+2)), so
      // lgamma(x) = (x+1)(x+2)*P_M2(x+1.5) - log(x(x+1)(x+2)). The shifts
      // are exact (Sterbenz); (x+1)(x+2) = (t-1)(t-2) is the M2 prefactor.
      double u = (xd + 1.0) * (xd + 2.0);
      double poly = lgamma_m2_poly(xd + 1.5);
      lgamma_val = fputil::multiply_add(u, poly, -lg_ln(xd * u));
    } else {
      // M2: t in [1.0, 2.0). lgamma(t) = (t-1)*(t-2) * P_M2(t - 1.5).
      double d = abs_xd - 0x1.8p+0;
      lgamma_val = (abs_xd - 1.0) * (abs_xd - 2.0) * lgamma_m2_poly(d);
    }
  } else if (x_abs < 0x4057e000u) {
    // M3: t in [2.0, 3.373046875). lgamma(t) = (t-2) * P_M3(d), d = t - MID.
    // Degree-15 monomial fit of lgamma(t)/(t-2), max error 2^-49.7.
    constexpr double MID_M3 = 0x1.57e0000000000p+1;
    constexpr double POLY_M3[16] = {
        0x1.3c4e36a0b4775p-1,   0x1.01f945be1325fp-2,   -0x1.4203a95730c77p-5,
        0x1.227f82db7c7a1p-7,   -0x1.32f092b6ec5a3p-9,  0x1.61df821ae7829p-11,
        -0x1.aed17eeca55e6p-13, 0x1.100a8e7dcd62cp-14,  -0x1.609aa16f960a0p-16,
        0x1.d1e293fa801bbp-18,  -0x1.38b436ce1b3b9p-19, 0x1.a838eec563338p-21,
        -0x1.1a6a9cf4aee4bp-22, 0x1.8387c2a068b06p-24,  -0x1.5ee6ae8c133f7p-25,
        0x1.f195cf3f4b24ep-27};
    if (!xbits.is_neg()) {
      double d = abs_xd - MID_M3;
      double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4;
      double p01 = fputil::multiply_add(d, POLY_M3[1], POLY_M3[0]);
      double p23 = fputil::multiply_add(d, POLY_M3[3], POLY_M3[2]);
      double p45 = fputil::multiply_add(d, POLY_M3[5], POLY_M3[4]);
      double p67 = fputil::multiply_add(d, POLY_M3[7], POLY_M3[6]);
      double p89 = fputil::multiply_add(d, POLY_M3[9], POLY_M3[8]);
      double p1011 = fputil::multiply_add(d, POLY_M3[11], POLY_M3[10]);
      double p1213 = fputil::multiply_add(d, POLY_M3[13], POLY_M3[12]);
      double p1415 = fputil::multiply_add(d, POLY_M3[15], POLY_M3[14]);
      double q03 = fputil::multiply_add(d2, p23, p01);
      double q47 = fputil::multiply_add(d2, p67, p45);
      double q811 = fputil::multiply_add(d2, p1011, p89);
      double q1215 = fputil::multiply_add(d2, p1415, p1213);
      double r07 = fputil::multiply_add(d4, q47, q03);
      double r815 = fputil::multiply_add(d4, q1215, q811);
      double poly = fputil::multiply_add(d8, r815, r07);
      lgamma_val = (abs_xd - 2.0) * poly;
    } else {
      // Near the regular lgamma zero at x ~= -2.7475: subtractive cancellation
      // in the reflection formula kills precision. Use a Taylor expansion
      // centered at the zero. Range bits in (0x402f95c2, 0x40301b93).
      // Coefficients adopted from CORE-MATH (Sibidanov, 2023).
      if (LIBC_UNLIKELY(x_abs > 0x402f95c2u && x_abs < 0x40301b93u)) {
        double h = (xd + 0x1.5fb410a1bd901p+1) - 0x1.a19a96d2e6f85p-54;
        constexpr double C[8] = {-0x1.ea12da904b18cp+0,  0x1.3267f3c265a54p+3,
                                 -0x1.4185ac30cadb3p+4,  0x1.f504accc3f2e4p+5,
                                 -0x1.8588444c679b4p+7,  0x1.43740491dc22p+9,
                                 -0x1.12400ea23f9e6p+11, 0x1.dac829f365795p+12};
        double h2 = h * h, h4 = h2 * h2;
        double p01 = fputil::multiply_add(h, C[1], C[0]);
        double p23 = fputil::multiply_add(h, C[3], C[2]);
        double p45 = fputil::multiply_add(h, C[5], C[4]);
        double p67 = fputil::multiply_add(h, C[7], C[6]);
        double p03 = fputil::multiply_add(h2, p23, p01);
        double p47 = fputil::multiply_add(h2, p67, p45);
        lgamma_val = h * fputil::multiply_add(h4, p47, p03);
      } else if (LIBC_UNLIKELY(x_abs > 0x401ceccbu && x_abs < 0x401d95cau)) {
        // Near the regular lgamma zero at x ~= -2.3614: same issue
        double h = (xd + 0x1.3a7fc9600f86cp+1) + 0x1.55f64f98af8dp-55;
        constexpr double C[7] = {0x1.83fe966af535fp+0, 0x1.36eebb002f61ap+2,
                                 0x1.694a60589a0b3p+0, 0x1.1718d7aedb0b5p+3,
                                 0x1.733a045eca0d3p+2, 0x1.8d4297421205bp+4,
                                 0x1.7feea5fb29965p+4};
        double h2 = h * h, h4 = h2 * h2;
        double p01 = fputil::multiply_add(h, C[1], C[0]);
        double p23 = fputil::multiply_add(h, C[3], C[2]);
        double p45 = fputil::multiply_add(h, C[5], C[4]);
        double p46 = fputil::multiply_add(h2, C[6], p45);
        double p03 = fputil::multiply_add(h2, p23, p01);
        lgamma_val = h * fputil::multiply_add(h4, p46, p03);
      } else if (LIBC_UNLIKELY(x_abs > 0x40492009u && x_abs < 0x404940efu)) {
        // Near the regular lgamma zero at x ~= -3.1431: same issue
        double h = (xd + 0x1.9260dbc9e59afp+1) + 0x1.f717cd335a7b3p-53;
        constexpr double C[7] = {0x1.f20a65f2fac55p+2,  0x1.9d4d297715105p+4,
                                 0x1.c1137124d5b21p+6,  0x1.267203d24de38p+9,
                                 0x1.99a63399a0b44p+11, 0x1.2941214faaf0cp+14,
                                 0x1.bb912c0c9cdd1p+16};
        double h2 = h * h, h4 = h2 * h2;
        double p01 = fputil::multiply_add(h, C[1], C[0]);
        double p23 = fputil::multiply_add(h, C[3], C[2]);
        double p45 = fputil::multiply_add(h, C[5], C[4]);
        double p46 = fputil::multiply_add(h2, C[6], p45);
        double p03 = fputil::multiply_add(h2, p23, p01);
        lgamma_val = h * fputil::multiply_add(h4, p46, p03);
      } else {
        // x in (-3.373, -2): Gamma(x) = Gamma(x+5)/(x(x+1)(x+2)(x+3)(x+4)),
        // t = x+5 in (1.627, 3), so lgamma(x) = (x+3)*Q_M3N(d) - log|prod|
        // with d = x + 2.6865234375 (exact). Q_M3N is a degree-17 monomial
        // fit of lgamma(t)/(t-2) on [1.627, 3]; the (t-2) = x+3 prefactor
        // removes the lgamma zero at t = 2.
        constexpr double POLY_M3N[18] = {
            0x1.091ff92b41f07p-1,   0x1.245eed13c42b0p-2,
            -0x1.a6c3cf8a165cfp-5,  0x1.bc8d19a3ade02p-7,
            -0x1.1231651010470p-8,  0x1.710f57a2abe2bp-10,
            -0x1.061ddab9ab12cp-11, 0x1.81edb077ed799p-13,
            -0x1.235fbc13dbf04p-14, 0x1.c02e6f8fb5dbfp-16,
            -0x1.5d75e6a94d352p-17, 0x1.1384596ce083dp-18,
            -0x1.b8da81a716039p-20, 0x1.61ac57fe0e288p-21,
            -0x1.077101d536ad8p-22, 0x1.a59e2356ba870p-24,
            -0x1.0cfcf8f102a43p-24, 0x1.c141a51f827f1p-26};
        double d = xd + 0x1.57ep+1;
        double d2 = d * d, d4 = d2 * d2, d8 = d4 * d4, d16 = d8 * d8;
        double p01 = fputil::multiply_add(d, POLY_M3N[1], POLY_M3N[0]);
        double p23 = fputil::multiply_add(d, POLY_M3N[3], POLY_M3N[2]);
        double p45 = fputil::multiply_add(d, POLY_M3N[5], POLY_M3N[4]);
        double p67 = fputil::multiply_add(d, POLY_M3N[7], POLY_M3N[6]);
        double p89 = fputil::multiply_add(d, POLY_M3N[9], POLY_M3N[8]);
        double p1011 = fputil::multiply_add(d, POLY_M3N[11], POLY_M3N[10]);
        double p1213 = fputil::multiply_add(d, POLY_M3N[13], POLY_M3N[12]);
        double p1415 = fputil::multiply_add(d, POLY_M3N[15], POLY_M3N[14]);
        double p1617 = fputil::multiply_add(d, POLY_M3N[17], POLY_M3N[16]);
        double q03 = fputil::multiply_add(d2, p23, p01);
        double q47 = fputil::multiply_add(d2, p67, p45);
        double q811 = fputil::multiply_add(d2, p1011, p89);
        double q1215 = fputil::multiply_add(d2, p1415, p1213);
        double r07 = fputil::multiply_add(d4, q47, q03);
        double r815 = fputil::multiply_add(d4, q1215, q811);
        double s015 = fputil::multiply_add(d8, r815, r07);
        double poly = fputil::multiply_add(d16, p1617, s015);
        double pa = xd * (xd + 1.0);
        double pb = (xd + 2.0) * (xd + 3.0);
        double prod = (pa * pb) * (xd + 4.0);
        double aprod = prod < 0.0 ? -prod : prod;
        lgamma_val = fputil::multiply_add(xd + 3.0, poly, -lg_ln(aprod));
      }
    }
  } else {
    // Large: |x| >= 3.373046875. Stirling + Bernoulli correction.
    // lgamma(x) = (x-0.5)*log(x) - x + log(2*pi)/2 + (1/x)*P(1/x^2)
    //          = (x-0.5)*(log(x)-1) + STIR_CONST + (1/x)*P(1/x^2)
    // STIR_CONST = log(2*pi)/2 - 0.5.
    // For huge positive x, lgamma(x) overflows float. Use a linear
    // approximation in double that maps to the correct Inf/max_normal.
    if (LIBC_UNLIKELY(!xbits.is_neg() && x >= 0x1.895f1cp+121f)) {
      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
      double r = fputil::multiply_add(xd, 0x1.4d3398p+6, 0x1.10f35ep+103);
      return fputil::cast<float>(r);
    }

    // No cancellation in (x-0.5)*(log(x)-1): log(x)-1 > 0.2 on this range.
    // Relative error ~2^-48; anything closer to a rounding boundary is in
    // the exceptional cases tables below.
    double lz = lg_ln(abs_xd);
    double xm = abs_xd - 0.5;
    lgamma_val = fputil::multiply_add(xm, lz - 1.0, 0x1.acfe390c97d69p-2);

    // For |x| >= 2^20 the 1/(12x) correction is below ~2^-47 of the result;
    // skip it and the 1/x divide.
    if (x_abs < 0x49800000u) {
      double inv_x = 1.0 / abs_xd;
      double inv_x2 = inv_x * inv_x;
      if (x_abs > 0x44fa0000u) {
        constexpr fputil::ExceptValues<float, 3> LGAMMAF_EXCEPTS_BERN2{{
            // input,      toward-zero result, RU, RD, RN
            {0x46541516u, 0x47e1c01bu, 1, 0, 0},
            {0x46b16323u, 0x48483adeu, 1, 0, 1},
            {0xc6f7e151u, 0xc89116deu, 0, 1, 0},
        }};
        if (auto r = LGAMMAF_EXCEPTS_BERN2.lookup(xbits.uintval());
            LIBC_UNLIKELY(r.has_value()))
          return r.value();
        // |x| > 2000 -> 2-term BERN2.
        constexpr double BERN2[2] = {0x1.5555555555555p-4,
                                     -0x1.6c16bfb7c65a8p-9};
        lgamma_val += inv_x * fputil::multiply_add(inv_x2, BERN2[1], BERN2[0]);
      } else if (x_abs > 0x42920000u) {
        // Exceptional cases of this range.
        constexpr fputil::ExceptValues<float, 2> LGAMMAF_EXCEPTS_BERN4{{
            // input,      toward-zero result, RU, RD, RN
            {0x449acf07u, 0x45ecd680u, 1, 0, 1},
            {0xc33139a3u, 0xc43991afu, 0, 1, 0},
        }};
        if (auto r = LGAMMAF_EXCEPTS_BERN4.lookup(xbits.uintval());
            LIBC_UNLIKELY(r.has_value()))
          return r.value();
        // |x| > 73 -> 4-term BERN4.
        constexpr double BERN4[4] = {
            0x1.5555555555555p-4, -0x1.6c16c16c15f75p-9, 0x1.a01a00593b36fp-11,
            -0x1.37e91273668efp-11};
        double inv_x4 = inv_x2 * inv_x2;
        double p01 = fputil::multiply_add(inv_x2, BERN4[1], BERN4[0]);
        double p23 = fputil::multiply_add(inv_x2, BERN4[3], BERN4[2]);
        lgamma_val += inv_x * fputil::multiply_add(inv_x4, p23, p01);
      } else {
        // Exceptional cases of this range.
        constexpr fputil::ExceptValues<float, 2> LGAMMAF_EXCEPTS_B10{{
            // input,      toward-zero result, RU, RD, RN
            {0x42468b59u, 0x430f25a7u, 1, 0, 0},
            {0xc134eb14u, 0xc1875615u, 0, 1, 0},
        }};
        if (auto r = LGAMMAF_EXCEPTS_B10.lookup(xbits.uintval());
            LIBC_UNLIKELY(r.has_value()))
          return r.value();
        // |x| in (3.373, 73]: degree-10 monomial fit of
        // h(s) = stir_resid(1/sqrt(s)) / sqrt(s), s = 1/x^2, on [0, 0.088];
        // correction = h(s) * (1/x). Max error 2^-53.2.
        constexpr double MID_B10 = 0x1.6880000000000p-5;
        constexpr double POLY_B10[11] = {
            0x1.54d6b78cee955p-4,  -0x1.635a5fb0cdf9fp-9,
            0x1.7b5253f44b255p-11, -0x1.f32907e7a7adap-12,
            0x1.1f269b6438739p-11, -0x1.e95dc64c5042cp-11,
            0x1.17ebce09c49a7p-9,  -0x1.91599e7728747p-8,
            0x1.5a01e1f07b127p-6,  -0x1.90e956754bc6ap-4,
            0x1.dfbed80c6f035p-2};
        double u = inv_x2 - MID_B10;
        double u2 = u * u, u4 = u2 * u2, u8 = u4 * u4;
        double p01 = fputil::multiply_add(u, POLY_B10[1], POLY_B10[0]);
        double p23 = fputil::multiply_add(u, POLY_B10[3], POLY_B10[2]);
        double p45 = fputil::multiply_add(u, POLY_B10[5], POLY_B10[4]);
        double p67 = fputil::multiply_add(u, POLY_B10[7], POLY_B10[6]);
        double p89 = fputil::multiply_add(u, POLY_B10[9], POLY_B10[8]);
        double q03 = fputil::multiply_add(u2, p23, p01);
        double q47 = fputil::multiply_add(u2, p67, p45);
        double q810 = fputil::multiply_add(u2, POLY_B10[10], p89);
        double r07 = fputil::multiply_add(u4, q47, q03);
        double poly = fputil::multiply_add(u8, q810, r07);
        lgamma_val += inv_x * poly;
      }
    } else {
      constexpr fputil::ExceptValues<float, 3> LGAMMAF_EXCEPTS_HUGE{{
          // input,      toward-zero result, RU, RD, RN
          {0x65fca09fu, 0x68cead59u, 1, 0, 1},
          {0x716e5dd5u, 0x747e2bb9u, 1, 0, 0},
          {0x77ac5674u, 0x7acf27b2u, 1, 0, 1},
      }};
      if (auto r = LGAMMAF_EXCEPTS_HUGE.lookup(xbits.uintval());
          LIBC_UNLIKELY(r.has_value()))
        return r.value();
    }

    if (xbits.is_neg()) {
      // Reflection: lgamma(x) = log(pi) - lgamma(|x|) - log(|x|) -
      // log(|sin(pi*frac_x)|). Reusing the already-computed lz = log(|x|)
      // keeps the multiply out of the serial sinpi -> log chain.
      double frac_x = xd - fputil::floor(xd);
      lgamma_val = (0x1.250d048e7a1bdp+0 - lgamma_val) - lz;
      lgamma_val -= lg_ln(lg_sinpi(frac_x));
    }
  }

  float result = fputil::cast<float>(lgamma_val);
  if (LIBC_UNLIKELY(FPBits(result).is_inf())) {
    fputil::raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
    fputil::set_errno_if_required(ERANGE);
  }
  return result;
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LGAMMAF_H
