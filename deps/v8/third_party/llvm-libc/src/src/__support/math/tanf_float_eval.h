//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Float-only implementation of tanf.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TANF_FLOAT_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TANF_FLOAT_EVAL_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/math/sincosf_float_eval.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace float_eval {

// Float-only tanf, requiring FMA. The maximum error, checked exhaustively in
// round-to-nearest mode against MPFR, is 3.37 ULP.
LIBC_INLINE float tanf(float x) {
  using namespace sincosf_float_eval;
  using FPBits = fputil::FPBits<float>;
  FPBits xbits(x);
  uint32_t x_abs = xbits.uintval() & 0x7fff'ffffU;

  // |x| < 2^-12
  if (LIBC_UNLIKELY(x_abs < 0x3980'0000U)) {
    // For signed zeros.
    if (LIBC_UNLIKELY(x_abs == 0U))
      return x;
    // When |x| < 2^-12, the relative error of the approximation tan(x) ~ x is:
    //   |tan(x) - x| / |tan(x)| < x^2 / 3 < 2^-25 < epsilon(1)/2.
    // fma(x, 2^-25, x) gives the correctly rounded result in all rounding modes
    // without underflowing for subnormal x.
    return fputil::multiply_add(x, 0x1.0p-25f, x);
  }

  float y;
  unsigned k;
  if (x_abs < 0x4880'0000U) {
    k = sincosf_range_reduction_small(x, y);
  } else {
    // Inf or NaN
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

  // x = k * pi/8 + y with |y| <= pi/16.
  float sin_k = SIN_K_PI_OVER_8[k & 15];
  // cos(k * pi/8) = sin((k + 4) * pi/8).
  float cos_k = SIN_K_PI_OVER_8[(k + 4) & 15];

  // Taylor polynomial of tan(y) for |y| <= pi/16:
  //   tan(y) ~ y + y^3/3 + 2y^5/15 + 17y^7/315 + 62y^9/2835.
  // The coefficients of tan's Taylor series decrease by a factor of about
  // (2/pi)^2 per term, so the relative truncation error is bounded by:
  //   1.02 * 1382/155925 * (pi/16)^10 < 2^-30.
  float y_sq = y * y;
  float y3 = y_sq * y;
  float p = fputil::multiply_add(y_sq, 0x1.664f48p-6f, 0x1.ba1ba2p-5f);
  p = fputil::multiply_add(y_sq, p, 0x1.111112p-3f);
  p = fputil::multiply_add(y_sq, p, 0x1.555556p-2f);
  // t ~ tan(y)
  float t = fputil::multiply_add(y3, p, y);

  // tan(x) = sin(x) / cos(x)
  //        = (sin_k * cos(y) + cos_k * sin(y)) /
  //          (cos_k * cos(y) - sin_k * sin(y))
  //        = (sin_k + cos_k * tan(y)) / (cos_k - sin_k * tan(y)).
  // Cancellation can only happen in the numerator when k = 0 (mod 8) and in
  // the denominator when k = 4 (mod 8). In those cases, sin_k or cos_k is
  // exactly 0 respectively, so the remaining term is computed with a small
  // relative error.
  float num = fputil::multiply_add(cos_k, t, sin_k);
  float den = fputil::multiply_add(-sin_k, t, cos_k);
  return num / den;
}

} // namespace float_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TANF_FLOAT_EVAL_H
