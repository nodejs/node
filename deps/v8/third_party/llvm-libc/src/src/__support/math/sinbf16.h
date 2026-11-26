//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the sinbf16 function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINBF16_H

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/sincosf_utils.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE bfloat16 sinbf16(bfloat16 x) {
  using namespace sincosf_utils_internal;

  using FPBits = fputil::FPBits<bfloat16>;
  FPBits xbits(x);

  uint16_t x_u = xbits.uintval();
  uint16_t x_abs = x_u & 0x7fff;
  float xf = x;

  // |x| < inf (0x7f80)
  if (x_abs < 0x7f80) {
    // |x| <= 0x1.24p-3f (0x3e12)
    if (LIBC_UNLIKELY(x_abs <= 0x3e12)) {
      // sin(+/-0) = +/-0
      if (LIBC_UNLIKELY(x_abs == 0))
        return x;

#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      int rounding = fputil::quick_get_round();
      // When x > 0, and rounding upward, sin(x) == x.
      // When x < 0, and rounding downward, sin(x) == x.
      if ((rounding == FE_UPWARD && xbits.is_pos()) ||
          (rounding == FE_DOWNWARD && xbits.is_neg()))
        return x;

      // When x < 0, and rounding upward, sin(x) == (x - 1ULP)
      if (rounding == FE_UPWARD && xbits.is_neg()) {
        x_u--;
        return FPBits(x_u).get_val();
      }
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    }
    double xd = static_cast<double>(xf);
    uint32_t x_abs_f = fputil::FPBits<float>(xf).uintval() & 0x7fffffff;
    double sin_k, cos_k, sin_y, cosm1_y;

    sincosf_eval(xd, x_abs_f, sin_k, cos_k, sin_y, cosm1_y);
    // using sin(a + b) = sin(a)*cos(b) + cos(a)*sin(b)
    //  sin(x) = sin_k*cos_y + cos_k*sin_y
    //  but cosm1_y = cos_y - 1 --> cos_y = cosm1_y + 1
    //  sin(x) = sin_k*cosm1_y + sin_k + cos_k * sin_y

    if (LIBC_UNLIKELY(sin_y == 0 && sin_k == 0))
      return FPBits::zero(xbits.sign()).get_val();
    return fputil::cast<bfloat16>(fputil::multiply_add(
        sin_y, cos_k, fputil::multiply_add(cosm1_y, sin_k, sin_k)));
  }

  // NaN
  if (xbits.is_nan()) {
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    return x;
  }

  // +/- inf
  fputil::set_errno_if_required(EDOM);
  fputil::raise_except_if_required(FE_INVALID);

  return x + FPBits::quiet_nan().get_val();
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINBF16_H
