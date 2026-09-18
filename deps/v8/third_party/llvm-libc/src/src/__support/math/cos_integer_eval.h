//===-- Implementation header for cos using integer-only --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COS_INTEGER_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COS_INTEGER_EVAL_H

#include "sincos_integer_utils.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/frac128.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace integer_only {

LIBC_INLINE double cos(double x) {
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint16_t x_e = xbits.get_biased_exponent();
  uint64_t x_u = xbits.get_mantissa();
  x_u |= uint64_t(1) << FPBits::FRACTION_LEN;

  Frac128 x_frac({0, 0});
  unsigned k = 0;
  bool is_neg = false;
  bool x_frac_is_neg = false;

  // x < 0.5
  if (x_e < FPBits::EXP_BIAS - 1) {
    // |x| < 2^-26, cos(x) ~ 1.
    if (LIBC_UNLIKELY(x_e < FPBits::EXP_BIAS - 26))
      return 1.0;
    // Normalize so that the MSB is 0.5.
    x_u <<= 10;
    unsigned shifts = FPBits::EXP_BIAS - 2 - x_e;
    if (shifts > 0) {
      if (shifts > 10)
        x_frac.val[0] = (x_u << (64 - shifts));
      x_frac.val[1] = x_u >> shifts;
    } else {
      x_frac.val[1] = x_u;
    }
  } else {
    // x is inf or nan.
    if (LIBC_UNLIKELY(x_e > 2 * FPBits::EXP_BIAS)) {
      // Silence signaling NaNs
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // sin(+-Inf) = NaN
      if (xbits.get_mantissa() == 0) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // x is quiet NaN
      return x;
    }

    // Perform range reduction mod pi/2:
    //   x = k * pi/2 + x_frac (mod 2*pi)
    x_frac_is_neg = trig_range_reduction(x_u, x_e, k, x_frac);
  }

  // cos(x) = sin(x + pi/2)
  return sin_eval(x_frac, k + 1, is_neg, x_frac_is_neg);
}

} // namespace integer_only

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COS_INTEGER_EVAL_H
