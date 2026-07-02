//===-- Implementation header for asinpif -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF_H

#include "inv_trigf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE float asinpif(float x) {
  using FPBits = fputil::FPBits<float>;

  FPBits xbits(x);
  bool is_neg = xbits.is_neg();
  double x_abs = fputil::cast<double>(xbits.abs().get_val());

  auto signed_result = [is_neg](auto r) -> auto { return is_neg ? -r : r; };

  if (LIBC_UNLIKELY(x_abs > 1.0)) {
    if (xbits.is_nan()) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }

    fputil::raise_except_if_required(FE_INVALID);
    fputil::set_errno_if_required(EDOM);
    return FPBits::quiet_nan().get_val();
  }

  // if |x| <= 0.5:
  //   asinpi(x) = x * (c0 + x^2 * P1(x^2))
  if (LIBC_UNLIKELY(x_abs <= 0.5)) {
    double x_d = fputil::cast<double>(x);
    double v2 = x_d * x_d;
    double result = x_d * fputil::multiply_add(
                              v2, inv_trigf_utils_internal::asinpi_eval(v2),
                              inv_trigf_utils_internal::ASINPI_COEFFS[0]);
    return fputil::cast<float>(result);
  }

  // If |x| > 0.5:
  //   asinpi(x) = 0.5 - 2 * sqrt(u) * P(u)
  //             = 0.5 - 2 * sqrt(u) * (c0 + u * P1(u))
  //             = (0.5 - 2*sqrt(u)*ONE_OVER_PI_HI)
  //               - 2*sqrt(u) * (ONE_OVER_PI_LO + DELTA_C0 + u * P1(u))
  //
  // where u = (1 - |x|) / 2, and
  //   ONE_OVER_PI_HI + ONE_OVER_PI_LO = 1/pi to ~106 bits
  //   DELTA_C0 = c0 - ONE_OVER_PI_HI
  //
  // ONE_OVER_PI_LO + DELTA_C0 is a single precomputed constant:
  //   = ONE_OVER_PI_LO + (c0 - ONE_OVER_PI_HI)
  //   = c0 - (ONE_OVER_PI_HI - ONE_OVER_PI_LO)
  //   = c0 - 1/pi  (to ~106 bits)
  constexpr double ONE_OVER_PI_HI = 0x1.45f306dc9c883p-2;
  constexpr double ONE_OVER_PI_LO = -0x1.6b01ec5417056p-56;
  // C0_MINUS_1OVERPI = c0 - 1/pi = DELTA_C0 + ONE_OVER_PI_LO
  constexpr double C0_MINUS_1OVERPI =
      (inv_trigf_utils_internal::ASINPI_COEFFS[0] - ONE_OVER_PI_HI) +
      ONE_OVER_PI_LO;

  double u = fputil::multiply_add(-0.5, x_abs, 0.5);
  double sqrt_u = fputil::sqrt<double>(u);
  double neg2_sqrt_u = -2.0 * sqrt_u;

  // tail = (c0 - 1/pi) + u * P1(u)
  double tail = fputil::multiply_add(
      u, inv_trigf_utils_internal::asinpi_eval(u), C0_MINUS_1OVERPI);

  double result_hi = fputil::multiply_add(neg2_sqrt_u, ONE_OVER_PI_HI, 0.5);
  double result = fputil::multiply_add(tail, neg2_sqrt_u, result_hi);

  return fputil::cast<float>(signed_result(result));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF_H
