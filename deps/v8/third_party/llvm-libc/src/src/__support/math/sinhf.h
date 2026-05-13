//===-- Single-precision sinh function ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINHF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINHF_H

#include "sinhfcoshf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float sinhf(float x) {
  using FPBits = typename fputil::FPBits<float>;
  FPBits xbits(x);
  uint32_t x_abs = xbits.abs().uintval();

  // When |x| >= 90, or x is inf or nan
  if (LIBC_UNLIKELY(x_abs >= 0x42b4'0000U || x_abs <= 0x3da0'0000U)) {
    // |x| <= 0.078125
    if (x_abs <= 0x3da0'0000U) {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      // |x| = 0.0005589424981735646724700927734375
      if (LIBC_UNLIKELY(x_abs == 0x3a12'85ffU)) {
        if (fputil::fenv_is_round_to_nearest())
          return x;
      }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

      // |x| <= 2^-26
      if (LIBC_UNLIKELY(x_abs <= 0x3280'0000U)) {
        return static_cast<float>(
            LIBC_UNLIKELY(x_abs == 0) ? x : (x + 0.25 * x * x * x));
      }

      double xdbl = x;
      double x2 = xdbl * xdbl;
      // Sollya: fpminimax(sinh(x),[|3,5,7|],[|D...|],[-1/16-1/64;1/16+1/64],x);
      // Sollya output: x * (0x1p0 + x^0x1p1 * (0x1.5555555556583p-3 + x^0x1p1
      //                  * (0x1.111110d239f1fp-7
      //                  + x^0x1p1 * 0x1.a02b5a284013cp-13)))
      // Therefore, output of Sollya = x * pe;
      double pe = fputil::polyeval(x2, 0.0, 0x1.5555555556583p-3,
                                   0x1.111110d239f1fp-7, 0x1.a02b5a284013cp-13);
      return static_cast<float>(fputil::multiply_add(xdbl, pe, xdbl));
    }

    if (xbits.is_nan())
      return x + 1.0f; // sNaN to qNaN + signal

    if (xbits.is_inf())
      return x;

    int rounding = fputil::quick_get_round();
    if (xbits.is_neg()) {
      if (LIBC_UNLIKELY(rounding == FE_UPWARD || rounding == FE_TOWARDZERO))
        return -FPBits::max_normal().get_val();
    } else {
      if (LIBC_UNLIKELY(rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO))
        return FPBits::max_normal().get_val();
    }

    fputil::set_errno_if_required(ERANGE);
    fputil::raise_except_if_required(FE_OVERFLOW);

    return x + FPBits::inf(xbits.sign()).get_val();
  }

  // sinh(x) = (e^x - e^(-x)) / 2.
  return static_cast<float>(
      math::sinhfcoshf_internal::exp_pm_eval</*is_sinh*/ true>(x));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINHF_H
