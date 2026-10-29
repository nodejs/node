//===-- Implementation header for sinpif16 -------------------------* C++*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINPIF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINPIF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "sincosf16_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE float16 sinpif16(float16 x) {
  using namespace sincosf16_internal;
  using FPBits = typename fputil::FPBits<float16>;
  FPBits xbits(x);

  uint16_t x_u = xbits.uintval();
  uint16_t x_abs = x_u & 0x7fff;
  float xf = x;

  // Range reduction:
  // For |x| > 1/32, we perform range reduction as follows:
  // Find k and y such that:
  //   x = (k + y) * 1/32
  //   k is an integer
  //   |y| < 0.5
  //
  // This is done by performing:
  //   k = round(x * 32)
  //   y = x * 32 - k
  //
  // Once k and y are computed, we then deduce the answer by the sine of sum
  // formula:
  //   sin(x * pi) = sin((k + y) * pi/32)
  //               = sin(k * pi/32) * cos(y * pi/32) +
  //                 sin(y * pi/32) * cos(k * pi/32)

  // For signed zeros
  if (LIBC_UNLIKELY(x_abs == 0U))
    return x;

  // Numbers greater or equal to 2^10 are integers, or infinity, or NaN
  if (LIBC_UNLIKELY(x_abs >= 0x6400)) {
    // Check for NaN or infinity values
    if (LIBC_UNLIKELY(x_abs >= 0x7c00)) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // If value is equal to infinity
      if (x_abs == 0x7c00) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
      }

      return x + FPBits::quiet_nan().get_val();
    }
    return FPBits::zero(xbits.sign()).get_val();
  }

  float sin_k = 0, cos_k = 0, sin_y = 0, cosm1_y = 0;
  sincospif16_eval(xf, sin_k, cos_k, sin_y, cosm1_y);

  if (LIBC_UNLIKELY(sin_y == 0 && sin_k == 0))
    return FPBits::zero(xbits.sign()).get_val();

  // Since, cosm1_y = cos_y - 1, therefore:
  // 	sin(x * pi) = cos_k * sin_y + sin_k + (cosm1_y * sin_k)
  return fputil::cast<float16>(fputil::multiply_add(
      sin_y, cos_k, fputil::multiply_add(cosm1_y, sin_k, sin_k)));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINPIF16_H
