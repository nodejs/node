//===-- Half-precision sin(x) function ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "sincosf16_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace sinf16_internal {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
LIBC_INLINE_VAR constexpr size_t N_EXCEPTS = 4;

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float16, N_EXCEPTS>
    SINF16_EXCEPTS{{
        // (input, RZ output, RU offset, RD offset, RN offset)
        {0x2b45, 0x2b43, 1, 0, 1},
        {0x585c, 0x3ba3, 1, 0, 1},
        {0x5cb0, 0xbbff, 0, 1, 0},
        {0x51f5, 0xb80f, 0, 1, 0},
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace sinf16_internal

LIBC_INLINE float16 sinf16(float16 x) {
  using namespace sinf16_internal;
  using namespace sincosf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits xbits(x);

  uint16_t x_u = xbits.uintval();
  uint16_t x_abs = x_u & 0x7fff;
  float xf = x;

  // Range reduction:
  // For |x| > pi/32, we perform range reduction as follows:
  // Find k and y such that:
  //   x = (k + y) * pi/32
  //   k is an integer, |y| < 0.5
  //
  // This is done by performing:
  //   k = round(x * 32/pi)
  //   y = x * 32/pi - k
  //
  // Once k and y are computed, we then deduce the answer by the sine of sum
  // formula:
  //   sin(x) = sin((k + y) * pi/32)
  //   	      = sin(k * pi/32) * cos(y * pi/32) +
  //   	        sin(y * pi/32) * cos(k * pi/32)

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  // Handle exceptional values
  bool x_sign = x_u >> 15;

  if (auto r = SINF16_EXCEPTS.lookup_odd(x_abs, x_sign);
      LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  int rounding = fputil::quick_get_round();

  // Exhaustive tests show that for |x| <= 0x1.f4p-11, 1ULP rounding errors
  // occur. To fix this, the following apply:
  if (LIBC_UNLIKELY(x_abs <= 0x13d0)) {
    // sin(+/-0) = +/-0
    if (LIBC_UNLIKELY(x_abs == 0U))
      return x;

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
  }

  if (xbits.is_inf_or_nan()) {
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }

    if (xbits.is_inf()) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
    }

    return x + FPBits::quiet_nan().get_val();
  }

  float sin_k, cos_k, sin_y, cosm1_y;
  sincosf16_eval(xf, sin_k, cos_k, sin_y, cosm1_y);

  if (LIBC_UNLIKELY(sin_y == 0 && sin_k == 0))
    return FPBits::zero(xbits.sign()).get_val();

  // Since, cosm1_y = cos_y - 1, therefore:
  //   sin(x) = cos_k * sin_y + sin_k + (cosm1_y * sin_k)
  return fputil::cast<float16>(fputil::multiply_add(
      sin_y, cos_k, fputil::multiply_add(cosm1_y, sin_k, sin_k)));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINF16_H
