//===-- Single-precision tanpi function -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF_H

#include "sincosf_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/is_integer.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float tanpif(float x) {
  using namespace sincosf_utils_internal;

  using FPBits = typename fputil::FPBits<float>;
  FPBits xbits(x);

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  constexpr size_t N_EXCEPTS = 3;
  constexpr fputil::ExceptValues<float, N_EXCEPTS> TANPIF_EXCEPTS{{
      // (input, RZ output, RU offset, RD offset, RN offset)
      {0x38F26685, 0x39BE6182, 1, 0, 0},
      {0x3E933802, 0x3FA267DD, 1, 0, 0},
      {0x3F3663FF, 0xBFA267DD, 0, 1, 0},
  }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = x_u & 0x7fff'ffffU;

  // Handle NaN and Inf.
  if (LIBC_UNLIKELY(x_abs >= 0x7f80'0000U)) {
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }

    if (x_abs == 0x7f80'0000U) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
    }

    return x + FPBits::quiet_nan().get_val();
  }

  // Handle signed zeros.
  if (LIBC_UNLIKELY(x_abs == 0U))
    return x;

  if (LIBC_UNLIKELY(fputil::is_integer(x))) {
    // Preserve the input sign for even integers and flip it for odd integers.
    Sign sign =
        fputil::is_odd_integer(x) ? xbits.sign().negate() : xbits.sign();
    return FPBits::zero(sign).get_val();
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  // Handle exceptional values.
  if (LIBC_UNLIKELY(x_abs <= 0x3F3663FF)) {
    bool x_sign = x_u >> 31;

    if (auto r = TANPIF_EXCEPTS.lookup_odd(x_abs, x_sign);
        LIBC_UNLIKELY(r.has_value()))
      return r.value();
  }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // Range reduction:
  // All remaining inputs are finite nonintegers, so |x| < 2^23.
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
  // Once k and y are computed, we then deduce the answer by the formula:
  // tan(x) = sin(x) / cos(x)
  //        = (sin_y * cos_k + cos_y * sin_k) / (cos_y * cos_k - sin_y * sin_k)
  double xd = static_cast<double>(x);
  double sin_k, cos_k, sin_y, cosm1_y;
  sincospif_eval(xd, sin_k, cos_k, sin_y, cosm1_y);

  if (LIBC_UNLIKELY(sin_y == 0 && cos_k == 0)) {
    fputil::set_errno_if_required(EDOM);
    fputil::raise_except_if_required(FE_DIVBYZERO);

    int32_t x_mp5_i = static_cast<int32_t>(xd - 0.5);
    return FPBits::inf((x_mp5_i & 0x1) ? Sign::NEG : Sign::POS).get_val();
  }

  using fputil::multiply_add;
  return fputil::cast<float>(
      multiply_add(sin_y, cos_k, multiply_add(cosm1_y, sin_k, sin_k)) /
      multiply_add(sin_y, -sin_k, multiply_add(cosm1_y, cos_k, cos_k)));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF_H
