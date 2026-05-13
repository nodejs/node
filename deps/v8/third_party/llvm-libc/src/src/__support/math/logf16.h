//===-- Implementation header for logf16 ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOGF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOGF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "expxf16_utils.h"
#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace logf16_internal {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
LIBC_INLINE_VAR constexpr size_t N_LOGF16_EXCEPTS = 5;
#else
LIBC_INLINE_VAR constexpr size_t N_LOGF16_EXCEPTS = 11;
#endif
LIBC_INLINE_VAR constexpr fputil::ExceptValues<float16, N_LOGF16_EXCEPTS>
    LOGF16_EXCEPTS = {{
// (input, RZ output, RU offset, RD offset, RN offset)
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.61cp-13, logf16(x) = -0x1.16p+3 (RZ)
        {0x0987U, 0xc858U, 0U, 1U, 0U},
        // x = 0x1.f2p-12, logf16(x) = -0x1.e98p+2 (RZ)
        {0x0fc8U, 0xc7a6U, 0U, 1U, 1U},
#endif
        // x = 0x1.4d4p-9, logf16(x) = -0x1.7e4p+2 (RZ)
        {0x1935U, 0xc5f9U, 0U, 1U, 0U},
        // x = 0x1.5ep-8, logf16(x) = -0x1.4ecp+2 (RZ)
        {0x1d78U, 0xc53bU, 0U, 1U, 0U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.fdp-1, logf16(x) = -0x1.81p-8 (RZ)
        {0x3bf4U, 0x9e04U, 0U, 1U, 1U},
        // x = 0x1.fep-1, logf16(x) = -0x1.008p-8 (RZ)
        {0x3bf8U, 0x9c02U, 0U, 1U, 0U},
#endif
        // x = 0x1.ffp-1, logf16(x) = -0x1.004p-9 (RZ)
        {0x3bfcU, 0x9801U, 0U, 1U, 0U},
        // x = 0x1.ff8p-1, logf16(x) = -0x1p-10 (RZ)
        {0x3bfeU, 0x9400U, 0U, 1U, 1U},
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.4c4p+1, logf16(x) = 0x1.e84p-1 (RZ)
        {0x4131U, 0x3ba1U, 1U, 0U, 1U},
#else
        // x = 0x1.75p+2, logf16(x) = 0x1.c34p+0 (RZ)
        {0x45d4U, 0x3f0dU, 1U, 0U, 0U},
        // x = 0x1.75p+2, logf16(x) = 0x1.c34p+0 (RZ)
        {0x45d4U, 0x3f0dU, 1U, 0U, 0U},
        // x = 0x1.d5p+9, logf16(x) = 0x1.b5cp+2 (RZ)
        {0x6354U, 0x46d7U, 1U, 0U, 1U},
#endif
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace logf16_internal

LIBC_INLINE float16 logf16(float16 x) {
  using namespace math::expxf16_internal;
  using namespace math::logf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();

  // If x <= 0, or x is 1, or x is +inf, or x is NaN.
  if (LIBC_UNLIKELY(x_u == 0U || x_u == 0x3c00U || x_u >= 0x7c00U)) {
    // log(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // log(+/-0) = −inf
    if ((x_u & 0x7fffU) == 0U) {
      fputil::raise_except_if_required(FE_DIVBYZERO);
      return FPBits::inf(Sign::NEG).get_val();
    }

    if (x_u == 0x3c00U)
      return FPBits::zero().get_val();

    // When x < 0.
    if (x_u > 0x8000U) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }

    // log(+inf) = +inf
    return FPBits::inf().get_val();
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = LOGF16_EXCEPTS.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // To compute log(x), we perform the following range reduction:
  //   x = 2^m * 1.mant,
  //   log(x) = m * log(2) + log(1.mant).
  // To compute log(1.mant), let f be the highest 6 bits including the hidden
  // bit, and d be the difference (1.mant - f), i.e., the remaining 5 bits of
  // the mantissa, then:
  //   log(1.mant) = log(f) + log(1.mant / f)
  //                = log(f) + log(1 + d/f)
  // since d/f is sufficiently small.
  // We store log(f) and 1/f in the lookup tables LOGF_F and ONE_OVER_F_F
  // respectively.

  int m = -FPBits::EXP_BIAS;

  // When x is subnormal, normalize it.
  if ((x_u & FPBits::EXP_MASK) == 0U) {
    // Can't pass an integer to fputil::cast directly.
    constexpr float NORMALIZE_EXP = 1U << FPBits::FRACTION_LEN;
    x_bits = FPBits(x_bits.get_val() * fputil::cast<float16>(NORMALIZE_EXP));
    x_u = x_bits.uintval();
    m -= FPBits::FRACTION_LEN;
  }

  uint16_t mant = x_bits.get_mantissa();
  // Leading 10 - 5 = 5 bits of the mantissa.
  int f = mant >> 5;
  // Unbiased exponent.
  m += x_u >> FPBits::FRACTION_LEN;

  // Set bits to 1.mant instead of 2^m * 1.mant.
  x_bits.set_biased_exponent(FPBits::EXP_BIAS);
  float mant_f = x_bits.get_val();
  // v = 1.mant * 1/f - 1 = d/f
  float v = fputil::multiply_add(mant_f, ONE_OVER_F_F[f], -1.0f);

  // Degree-3 minimax polynomial generated by Sollya with the following
  // commands:
  //   > display = hexadecimal;
  //   > P = fpminimax(log(1 + x)/x, 2, [|SG...|], [-2^-5, 2^-5]);
  //   > x * P;
  float log1p_d_over_f =
      v * fputil::polyeval(v, 0x1p+0f, -0x1.001804p-1f, 0x1.557ef6p-2f);
  // log(1.mant) = log(f) + log(1 + d/f)
  float log_1_mant = LOGF_F[f] + log1p_d_over_f;
  return fputil::cast<float16>(
      fputil::multiply_add(static_cast<float>(m), LOGF_2, log_1_mant));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOGF16_H
