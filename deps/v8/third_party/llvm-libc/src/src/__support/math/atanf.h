//===-- Implementation header for atanf -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATANF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATANF_H

#include "inv_trigf_utils.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    defined(LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT)

// We use float-float implementation to reduce size.
#include "atanf_float.h"

#else

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float atanf(float x) {
  using namespace inv_trigf_utils_internal;
  using FPBits = typename fputil::FPBits<float>;

  constexpr double FINAL_SIGN[2] = {1.0, -1.0};
  constexpr double SIGNED_PI_OVER_2[2] = {0x1.921fb54442d18p0,
                                          -0x1.921fb54442d18p0};

  FPBits x_bits(x);
  Sign sign = x_bits.sign();
  x_bits.set_sign(Sign::POS);
  uint32_t x_abs = x_bits.uintval();

  // x is inf or nan, |x| < 2^-4 or |x|= > 16.
  if (LIBC_UNLIKELY(x_abs <= 0x3d80'0000U || x_abs >= 0x4180'0000U)) {
    double x_d = static_cast<double>(x);
    double const_term = 0.0;
    if (LIBC_UNLIKELY(x_abs >= 0x4180'0000)) {
      // atan(+-Inf) = +-pi/2.
      if (x_bits.is_inf()) {
        volatile double sign_pi_over_2 = SIGNED_PI_OVER_2[sign.is_neg()];
        return static_cast<float>(sign_pi_over_2);
      }
      if (x_bits.is_nan())
        return x;
      // x >= 16
      x_d = -1.0 / x_d;
      const_term = SIGNED_PI_OVER_2[sign.is_neg()];
    }
    // 0 <= x < 1/16;
    if (LIBC_UNLIKELY(x_bits.is_zero()))
      return x;
    // x <= 2^-12;
    if (LIBC_UNLIKELY(x_abs < 0x3980'0000)) {
#if defined(LIBC_TARGET_CPU_HAS_FMA_FLOAT)
      return fputil::multiply_add(x, -0x1.0p-25f, x);
#else
      return static_cast<float>(fputil::multiply_add(x_d, -0x1.0p-25, x_d));
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT
    }
    // Use Taylor polynomial:
    //   atan(x) ~ x * (1 - x^2 / 3 + x^4 / 5 - x^6 / 7 + x^8 / 9 - x^10 / 11).
    constexpr double ATAN_TAYLOR[6] = {
        0x1.0000000000000p+0,  -0x1.5555555555555p-2, 0x1.999999999999ap-3,
        -0x1.2492492492492p-3, 0x1.c71c71c71c71cp-4,  -0x1.745d1745d1746p-4,
    };
    double x2 = x_d * x_d;
    double x4 = x2 * x2;
    double c0 = fputil::multiply_add(x2, ATAN_TAYLOR[1], ATAN_TAYLOR[0]);
    double c1 = fputil::multiply_add(x2, ATAN_TAYLOR[3], ATAN_TAYLOR[2]);
    double c2 = fputil::multiply_add(x2, ATAN_TAYLOR[5], ATAN_TAYLOR[4]);
    double p = fputil::polyeval(x4, c0, c1, c2);
    double r = fputil::multiply_add(x_d, p, const_term);
    return static_cast<float>(r);
  }

  // Range reduction steps:
  // 1)  atan(x) = sign(x) * atan(|x|)
  // 2)  If |x| > 1, atan(|x|) = pi/2 - atan(1/|x|)
  // 3)  For 1/16 < x <= 1, we find k such that: |x - k/16| <= 1/32.
  // 4)  Then we use polynomial approximation:
  //   atan(x) ~ atan((k/16) + (x - (k/16)) * Q(x - k/16)
  //           = P(x - k/16)
  double x_d = 0, const_term = 0, final_sign = 0;
  int idx = 0;

  if (x_abs > 0x3f80'0000U) {
    // |x| > 1, we need to invert x, so we will perform range reduction in
    // double precision.
    x_d = 1.0 / static_cast<double>(x_bits.get_val());
    double k_d = fputil::nearest_integer(x_d * 0x1.0p4);
    x_d = fputil::multiply_add(k_d, -0x1.0p-4, x_d);
    idx = static_cast<int>(k_d);
    final_sign = FINAL_SIGN[sign.is_pos()];
    // Adjust constant term of the polynomial by +- pi/2.
    const_term = fputil::multiply_add(final_sign, ATAN_COEFFS[idx][0],
                                      SIGNED_PI_OVER_2[sign.is_neg()]);
  } else {
    // Exceptional value:
    if (LIBC_UNLIKELY(x_abs == 0x3d8d'6b23U)) { // |x| = 0x1.1ad646p-4
      return sign.is_pos() ? fputil::round_result_slightly_down(0x1.1a6386p-4f)
                           : fputil::round_result_slightly_up(-0x1.1a6386p-4f);
    }
    // Perform range reduction in single precision.
    float x_f = x_bits.get_val();
    float k_f = fputil::nearest_integer(x_f * 0x1.0p4f);
    x_f = fputil::multiply_add(k_f, -0x1.0p-4f, x_f);
    x_d = static_cast<double>(x_f);
    idx = static_cast<int>(k_f);
    final_sign = FINAL_SIGN[sign.is_neg()];
    const_term = final_sign * ATAN_COEFFS[idx][0];
  }

  double p = atan_eval(x_d, idx);
  double r = fputil::multiply_add(final_sign * x_d, p, const_term);

  return static_cast<float>(r);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATANF_H
