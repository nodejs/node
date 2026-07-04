//===-- Implementation header for acoshf16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "acoshf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float16 acoshf16(float16 x) {

  using namespace acoshf_internal;
  constexpr size_t N_EXCEPTS = 2;
  constexpr fputil::ExceptValues<float16, N_EXCEPTS> ACOSHF16_EXCEPTS{{
      // (input, RZ output, RU offset, RD offset, RN offset)
      // x = 0x1.6dcp+1, acoshf16(x) = 0x1.b6p+0 (RZ)
      {0x41B7, 0x3ED8, 1, 0, 0},
      // x = 0x1.39p+0, acoshf16(x) = 0x1.4f8p-1 (RZ)
      {0x3CE4, 0x393E, 1, 0, 1},
  }};

  using FPBits = fputil::FPBits<float16>;
  FPBits xbits(x);
  uint16_t x_u = xbits.uintval();

  // Check for NaN input first.
  if (LIBC_UNLIKELY(xbits.is_inf_or_nan())) {
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    if (xbits.is_neg()) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    return x;
  }

  // Domain error for inputs less than 1.0.
  if (LIBC_UNLIKELY(x <= 1.0f)) {
    if (x == 1.0f)
      return FPBits::zero().get_val();
    fputil::set_errno_if_required(EDOM);
    fputil::raise_except_if_required(FE_INVALID);
    return FPBits::quiet_nan().get_val();
  }

  if (auto r = ACOSHF16_EXCEPTS.lookup(xbits.uintval());
      LIBC_UNLIKELY(r.has_value()))
    return r.value();

  float xf = x;
  // High-precision polynomial approximation for inputs close to 1.0
  // ([1, 1.25)).
  //
  // Brief derivation:
  // 1. Expand acosh(1 + delta) using Taylor series around delta=0:
  //    acosh(1 + delta) ≈ sqrt(2 * delta) * [1 - delta/12 + 3*delta^2/160
  //                     - 5*delta^3/896 + 35*delta^4/18432 + ...]
  // 2. Truncate the series to fit accurately for delta in [0, 0.25].
  // 3. Polynomial coefficients (from sollya) used here are:
  //    P(delta) ≈ 1 - 0x1.555556p-4 * delta + 0x1.333334p-6 * delta^2
  //               - 0x1.6db6dcp-8 * delta^3 + 0x1.f1c71cp-10 * delta^4
  // 4. The Sollya commands used to generate these coefficients were:
  //      > display = hexadecimal;
  //      > round(1/12, SG, RN);
  //      > round(3/160, SG, RN);
  //      > round(5/896, SG, RN);
  //      > round(35/18432, SG, RN);
  //      With hexadecimal display mode enabled, the outputs were:
  //      0x1.555556p-4
  //      0x1.333334p-6
  //      0x1.6db6dcp-8
  //      0x1.f1c71cp-10
  // 5. The maximum absolute error, estimated using:
  //      dirtyinfnorm(acosh(1 + x) - sqrt(2*x) * P(x), [0, 0.25])
  //    is:
  //      0x1.d84281p-22
  if (LIBC_UNLIKELY(x_u < 0x3D00U)) {
    float delta = xf - 1.0f;
    float sqrt_2_delta = fputil::sqrt<float>(2.0 * delta);
    float pe = fputil::polyeval(delta, 0x1p+0f, -0x1.555556p-4f, 0x1.333334p-6f,
                                -0x1.6db6dcp-8f, 0x1.f1c71cp-10f);
    float approx = sqrt_2_delta * pe;
    return fputil::cast<float16>(approx);
  }

  // acosh(x) = log(x + sqrt(x^2 - 1))
  float sqrt_term = fputil::sqrt<float>(fputil::multiply_add(xf, xf, -1.0f));
  float result = static_cast<float>(log_eval(xf + sqrt_term));

  return fputil::cast<float16>(result);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF16_H
