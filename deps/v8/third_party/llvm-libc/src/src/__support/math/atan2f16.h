//===-- Implementation header for atan2f16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "inv_trigf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float16 atan2f16(float16 y, float16 x) {
  using namespace inv_trigf_utils_internal;
  using FPBits = fputil::FPBits<float16>;

  constexpr double IS_NEG[2] = {1.0, -1.0};
  constexpr double PI = 0x1.921fb54442d18p+1;
  constexpr double PI_OVER_2 = 0x1.921fb54442d18p+0;
  constexpr double PI_OVER_4 = 0x1.921fb54442d18p-1;
  constexpr double THREE_PI_OVER_4 = 0x1.2d97c7f3321d2p+1;

  // const_term[x_sign][recip]; recip = (|x| < |y|)
  constexpr double CONST_TERM[2][2] = {
      {0.0, -PI_OVER_2},
      {-PI, PI_OVER_2},
  };

  FPBits x_bits(x), y_bits(y);
  bool x_sign = x_bits.sign().is_neg();
  bool y_sign = y_bits.sign().is_neg();
  x_bits.set_sign(Sign::POS);
  y_bits.set_sign(Sign::POS);
  uint16_t x_abs = x_bits.uintval();
  uint16_t y_abs = y_bits.uintval();
  uint16_t max_abs = x_abs > y_abs ? x_abs : y_abs;
  uint16_t min_abs = x_abs <= y_abs ? x_abs : y_abs;

  if (LIBC_UNLIKELY(max_abs >= 0x7c00U || min_abs == 0)) {
    if (x_bits.is_nan() || y_bits.is_nan()) {
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan())
        fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    size_t x_except = (x_abs == 0) ? 0 : (x_abs == 0x7c00U ? 2 : 1);
    size_t y_except = (y_abs == 0) ? 0 : (y_abs == 0x7c00U ? 2 : 1);
    constexpr double EXCEPTS[3][3][2] = {
        {{0.0, PI}, {0.0, PI}, {0.0, PI}},
        {{PI_OVER_2, PI_OVER_2}, {0.0, 0.0}, {0.0, PI}},
        {{PI_OVER_2, PI_OVER_2},
         {PI_OVER_2, PI_OVER_2},
         {PI_OVER_4, THREE_PI_OVER_4}},
    };
    double r = IS_NEG[y_sign] * EXCEPTS[y_except][x_except][x_sign];
    return fputil::cast<float16>(r);
  }

  bool recip = x_abs < y_abs;
  double final_sign = IS_NEG[x_sign ^ y_sign ^ recip];
  double const_term = CONST_TERM[x_sign][recip];

  // atan2(y,x) = final_sign * (const_term + atan(n/d)),
  //   where n = min(|x|,|y|), d = max(|x|,|y|), so 0 <= n/d <= 1.
  //
  // To compute atan(n/d), we use a lookup table with 16 equally-spaced knots:
  //   idx = round(16 * n/d),  so  |n/d - idx/16| <= 1/32.
  //   q_d = n/d - idx/16.
  // Then by the atan addition formula:
  //   atan(n/d) = atan(idx/16) + q_d * Q(q_d)
  // where Q(q_d) approximates
  //   [atan(idx/16 + q_d) - atan(idx/16)] / q_d
  // via atan_eval(q_d, idx) from inv_trigf_utils.
  double n = static_cast<double>(FPBits(min_abs).get_val());
  double d = static_cast<double>(FPBits(max_abs).get_val());
  double q_d = n / d;

  double k_d = fputil::nearest_integer(q_d * 0x1.0p4);
  int idx = static_cast<int>(k_d);
  q_d = fputil::multiply_add(k_d, -0x1.0p-4, q_d);

  double p = atan_eval(q_d, static_cast<unsigned>(idx));
  double r = final_sign *
             fputil::multiply_add(q_d, p, const_term + ATAN_K_OVER_16[idx]);
  return fputil::cast<float16>(r);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATAN2F16_H
