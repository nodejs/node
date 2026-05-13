//===-- Implementation header for atanhf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATANHF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATANHF_H

#include "acoshf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float atanhf(float x) {
  using namespace acoshf_internal;
  using FPBits = typename fputil::FPBits<float>;

  FPBits xbits(x);
  Sign sign = xbits.sign();
  uint32_t x_abs = xbits.abs().uintval();

  // |x| >= 1.0
  if (LIBC_UNLIKELY(x_abs >= 0x3F80'0000U)) {
    if (xbits.is_nan()) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }
    // |x| == 1.0
    if (x_abs == 0x3F80'0000U) {
      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_DIVBYZERO);
      return FPBits::inf(sign).get_val();
    } else {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
  }

  // |x| < ~0.10
  if (LIBC_UNLIKELY(x_abs <= 0x3dcc'0000U)) {
    // |x| <= 2^-26
    if (LIBC_UNLIKELY(x_abs <= 0x3280'0000U)) {
      return static_cast<float>(LIBC_UNLIKELY(x_abs == 0)
                                    ? x
                                    : (x + 0x1.5555555555555p-2 * x * x * x));
    }

    double xdbl = x;
    double x2 = xdbl * xdbl;
    // Pure Taylor series.
    double pe = fputil::polyeval(x2, 0.0, 0x1.5555555555555p-2,
                                 0x1.999999999999ap-3, 0x1.2492492492492p-3,
                                 0x1.c71c71c71c71cp-4, 0x1.745d1745d1746p-4);
    return static_cast<float>(fputil::multiply_add(xdbl, pe, xdbl));
  }
  double xdbl = x;
  return static_cast<float>(0.5 * log_eval((1.0 + xdbl) / (1.0 - x)));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATANHF_H
