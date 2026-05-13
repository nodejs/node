//===-- Implementation header for tanpif16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF16_H

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

LIBC_INLINE float16 tanpif16(float16 x) {
  using namespace sincosf16_internal;
  using FPBits = typename fputil::FPBits<float16>;
  FPBits xbits(x);

  uint16_t x_u = xbits.uintval();
  uint16_t x_abs = x_u & 0x7fff;

  // Handle exceptional values
  if (LIBC_UNLIKELY(x_abs <= 0x4335)) {
    if (LIBC_UNLIKELY(x_abs == 0U))
      return x;

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    constexpr size_t N_EXCEPTS = 21;

    constexpr fputil::ExceptValues<float16, N_EXCEPTS> TANPIF16_EXCEPTS{{
        // (input, RZ output, RU offset, RD offset, RN offset)
        {0x07f2, 0x0e3d, 1, 0, 0}, {0x086a, 0x0eee, 1, 0, 1},
        {0x08db, 0x0fa0, 1, 0, 0}, {0x094c, 0x1029, 1, 0, 0},
        {0x0b10, 0x118c, 1, 0, 0}, {0x1ce0, 0x23a8, 1, 0, 1},
        {0x1235, 0x18e0, 1, 0, 0}, {0x2579, 0x2c4e, 1, 0, 0},
        {0x28b2, 0x2f68, 1, 0, 1}, {0x2a43, 0x30f4, 1, 0, 1},
        {0x31b7, 0x3907, 1, 0, 0}, {0x329d, 0x3a12, 1, 0, 1},
        {0x34f1, 0x3dd7, 1, 0, 0}, {0x3658, 0x41ee, 1, 0, 0},
        {0x38d4, 0xc1ee, 0, 1, 0}, {0x3d96, 0x41ee, 1, 0, 0},
        {0x3e6a, 0xc1ee, 0, 1, 0}, {0x40cb, 0x41ee, 1, 0, 0},
        {0x4135, 0xc1ee, 0, 1, 0}, {0x42cb, 0x41ee, 1, 0, 0},
        {0x4335, 0xc1ee, 0, 1, 0},
    }};

    bool x_sign = x_u >> 15;

    if (auto r = TANPIF16_EXCEPTS.lookup_odd(x_abs, x_sign);
        LIBC_UNLIKELY(r.has_value()))
      return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  }

  // Numbers greater or equal to 2^10 are integers, or infinity, or NaN
  if (LIBC_UNLIKELY(x_abs >= 0x6400)) {
    // Check for NaN or infinity values
    if (LIBC_UNLIKELY(x_abs >= 0x7c00)) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // is inf
      if (x_abs == 0x7c00) {
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
      }

      return x + FPBits::quiet_nan().get_val();
    }

    return FPBits::zero(xbits.sign()).get_val();
  }
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
  // Once k and y are computed, we then deduce the answer by the formula:
  // tan(x) = sin(x) / cos(x)
  //        = (sin_y * cos_k + cos_y * sin_k) / (cos_y * cos_k - sin_y * sin_k)
  float xf = x;
  float sin_k = 0, cos_k = 0, sin_y = 0, cosm1_y = 0;
  sincospif16_eval(xf, sin_k, cos_k, sin_y, cosm1_y);

  if (LIBC_UNLIKELY(sin_y == 0 && cos_k == 0)) {
    fputil::set_errno_if_required(EDOM);
    fputil::raise_except_if_required(FE_DIVBYZERO);

    int16_t x_mp5_u = static_cast<int16_t>(x - 0.5);
    return ((x_mp5_u & 0x1) ? -1 : 1) * FPBits::inf().get_val();
  }

  using fputil::multiply_add;
  return fputil::cast<float16>(
      multiply_add(sin_y, cos_k, multiply_add(cosm1_y, sin_k, sin_k)) /
      multiply_add(sin_y, -sin_k, multiply_add(cosm1_y, cos_k, cos_k)));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TANPIF16_H
