//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the acoshbf16 function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHBF16_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/acoshf_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR bfloat16 acoshbf16(bfloat16 x) {
  using namespace acoshf_internal;
  using FPBits = fputil::FPBits<bfloat16>;

  FPBits xbits(x);

  // Handle NaNs before checking the domain. The sign of a NaN does not
  // determine whether the input is a domain error.
  if (LIBC_UNLIKELY(xbits.is_inf_or_nan())) {
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }

    // Negative infinity is outside the domain.
    if (xbits.is_inf() && xbits.is_neg()) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    // positive infinity and quiet NaNs are returned unchanged.
    return x;
  }

  float xf = static_cast<float>(x);

  // acosh(x) is defined for x >= 1. Its value at x = 1 is exactly zero.
  if (LIBC_UNLIKELY(xf <= 1.0f)) {
    if (xf == 1.0f)
      return FPBits::zero().get_val();

    fputil::set_errno_if_required(EDOM);
    fputil::raise_except_if_required(FE_INVALID);
    return FPBits::quiet_nan().get_val();
  }

  uint16_t x_u = xbits.uintval();

  // For x close to 1, let delta = x - 1. Then
  //
  //   acosh(1 + delta) = sqrt(2 * delta) * P(delta),
  //
  // where the Taylor expansion of P is
  //
  //   P(delta) = 1 - delta / 12 + 3 * delta^2 / 160
  //                - 5 * delta^3 / 896 + O(delta^4).
  //
  // Factoring out sqrt(2 * delta) captures the behavior near x = 1
  // and leaves a smooth function that can be approximated by a
  // low-degree polynomial. This fast path uses the degree-2 Taylor
  // polynomial
  //
  //   P2(delta) = 1 - delta / 12 + 3 * delta^2 / 160.
  //
  // for delta in [0, 0.25), corresponding to 1 < x < 1.25.
  //
  // The following Sollya commands estimate the absolute error of this
  // polynomial over the closed interval [0, 0.25], using the same
  // single-precision rounded coefficients as the implementation:
  //
  //   prec = 200;
  //   display = hexadecimal;
  //   c1 = round(-1/12, single, RN);       // -0x1.555556p-4
  //   c2 = round(3/160, single, RN);       // 0x1.333334p-6
  //   P2 = 1 + c1*x + c2*x^2;
  //   dirtyinfnorm(acosh(1 + x) - sqrt(2 * x) * P2, [0; 0.25]);
  //
  // Sollya estimated the maximum absolute approximation error over
  // this interval (including single-precision float coefficient rounding) as:
  //
  //   0x1.dce3cfd50cfc0248598bad104690338afe96a2a1efe4359a3cp-15
  //   (approximately 5.68e-5, below 2^-14).
  //
  // The coefficients used below are rounded to float:
  //
  //   round(1 / 12, SG, RN);       // 0x1.555556p-4
  //   round(3 / 160, SG, RN);      // 0x1.333334p-6
  //
  // Precision Context:
  // Since bfloat16 machine epsilon is 2^-7 (~7.81e-3) and fp16 machine
  // epsilon is 2^-10 (~9.77e-4), an error of ~5.68e-5 (< 2^-14) guarantees
  // full precision. It does not include float-operation rounding
  // or the final conversion to bfloat16, and it does not by itself
  // establish correct rounding. Separately, exhaustive comparison of
  // all 31 bfloat16 inputs in (1, 1.25) against a 100-digit acosh
  // reference found no output mismatches for separate float
  // multiply/add evaluation or fused multiply_add evaluation.
  // This polynomial avoids the logarithm and provides a
  // candidate fast path for BFloat16 inputs in (1, 1.25).
  //
  // The threshold 0x3FA0 is the BFloat16 encoding of 1.25.
  if (x_u < 0x3FA0U) {
    float delta = xf - 1.0f;
    float sqrt_2_delta = fputil::sqrt<float>(2.0f * delta);

    float p = fputil::polyeval(delta, 1.0f, -0x1.555556p-4f, 0x1.333334p-6f);

    return fputil::cast<bfloat16>(sqrt_2_delta * p);
  }

  // Away from x = 1, evaluate
  //
  //   acosh(x) = log(x + sqrt(x^2 - 1)).
  //
  // Use double precision for the intermediate calculations. In particular,
  // this avoids overflow when squaring the largest finite BFloat16 inputs.
  // Convert directly to BFloat16 to avoid an intermediate float rounding.
  double xd = static_cast<double>(xf);
  double sqrt_term = fputil::sqrt<double>(fputil::multiply_add(xd, xd, -1.0));
  double result = log_eval(xd + sqrt_term);

  return fputil::cast<bfloat16>(result);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHBF16_H
