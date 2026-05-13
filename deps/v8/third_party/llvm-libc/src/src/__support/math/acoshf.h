//===-- Implementation header for acoshf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF_H

#include "acoshf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float acoshf(float x) {
  using namespace acoshf_internal;
  using FPBits_t = typename fputil::FPBits<float>;
  FPBits_t xbits(x);

  if (LIBC_UNLIKELY(x <= 1.0f)) {
    if (x == 1.0f)
      return 0.0f;
    // x < 1.
    fputil::set_errno_if_required(EDOM);
    fputil::raise_except_if_required(FE_INVALID);
    return FPBits_t::quiet_nan().get_val();
  }

  uint32_t x_u = xbits.uintval();
  double x_d = static_cast<double>(x);

  if (LIBC_UNLIKELY(x_u >= 0x4580'0000U)) {
    // x >= 2^12.
    if (LIBC_UNLIKELY(xbits.is_inf_or_nan())) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits_t::quiet_nan().get_val();
      }
      return x;
    }

    // acosh(x) = log(x + sqrt(x^2 - 1))
    // For large x:
    //   log(x + sqrt(x^2 - 1)) = log(2x) + log((x + sqrt(x^2 - 1)) / (2x)).
    // Let U = (x + sqrt(x^2 - 1))/(2x).
    // Then U = 1 - (x - sqrt(x^2 - 1))/(2x)
    //        = 1 - (1 - sqrt(1 - 1/x^2))/2
    //        = 1 - (1/2) * (1/(2x^2) + 1/(8x^4) + ...)
    //        = 1 - 1/(2x)^2 - 1/(2x)^4 - ...
    // Hence log(U) = log(1 - 1/(2x)^2 - 1/(2x)^4 - ...)
    //              = -(1/(2x)^2 - 1/(2x)^4 - ...) -
    //                - (1/(2x)^2 - 1/(2x)^4 - ...)^2/2 - ...
    //              ~ -1/(2x)^2 - 1/(2x^4) - ...
    // For x >= 2^12:
    //   acosh(x) ~ log(2x) - 1/(2x)^2.
    // > g = log(2*x) + 1/(4 * x^2);
    // > dirtyinfnorm((acosh(x) - g)/acosh(x), [2^12, 2^20]);
    // 0x1.54eb81b0c0df3c9bf68c149748e507fa136e2294fp-55
    //
    // For x >= 2^26, 1/(2x)^2 <= 2^-54. So we just need log(2x).

    double y = 2.0 * x_d;

    if (x_u <= 0x4c80'0000U) {
      // x <= 2^26
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      if (LIBC_UNLIKELY(x_u == 0x45dc'6414U)) // x = 0x1.b8c828p12f
        return fputil::round_result_slightly_up(0x1.31bcb6p3f);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      double y_inv = 0.5 / x_d;
      return static_cast<float>(
          fputil::multiply_add(y_inv, -y_inv, log_eval(y)));

    } else {
// x > 2^26
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      switch (x_u) {
      case 0x4c803f2c: // x = 0x1.007e58p26f
        return fputil::round_result_slightly_down(0x1.2b786cp4f);
      case 0x4f8ffb03: // x = 0x1.1ff606p32f
        return fputil::round_result_slightly_up(0x1.6fdd34p4f);
      case 0x5c569e88: // x = 0x1.ad3d1p57f
        return fputil::round_result_slightly_up(0x1.45c146p5f);
      case 0x5e68984e: // x = 0x1.d1309cp61f
        return fputil::round_result_slightly_up(0x1.5c9442p5f);
      case 0x655890d3: // x = 0x1.b121a6p75f
        return fputil::round_result_slightly_down(0x1.a9a3f2p5f);
      case 0x6eb1a8ec: // x = 0x1.6351d8p94f
        return fputil::round_result_slightly_down(0x1.08b512p6f);
      case 0x7997f30a: // x = 0x1.2fe614p116f
        return fputil::round_result_slightly_up(0x1.451436p6f);
#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
      case 0x65de7ca6: // x = 0x1.bcf94cp76f
        return fputil::round_result_slightly_up(0x1.af66cp5f);
      case 0x7967ec37: // x = 0x1.cfd86ep115f
        return fputil::round_result_slightly_up(0x1.43ff6ep6f);
#endif // !LIBC_TARGET_CPU_HAS_FMA_DOUBLE
      }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return static_cast<float>(log_eval(y));
    }
  }

  // For 1 < x < 2^12, we use the formula:
  //   acosh(x) = log(x + sqrt(x^2 - 1))
  return static_cast<float>(log_eval(
      x_d + fputil::sqrt<double>(fputil::multiply_add(x_d, x_d, -1.0))));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ACOSHF_H
