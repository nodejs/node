//===-- Implementation header for log10f16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F16_H

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

namespace log10f16_internal {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
LIBC_INLINE_VAR constexpr size_t N_LOG10F16_EXCEPTS = 11;
#else
LIBC_INLINE_VAR constexpr size_t N_LOG10F16_EXCEPTS = 17;
#endif

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float16, N_LOG10F16_EXCEPTS>
    LOG10F16_EXCEPTS = {{
        // (input, RZ output, RU offset, RD offset, RN offset)
        // x = 0x1.e3cp-3, log10f16(x) = -0x1.40cp-1 (RZ)
        {0x338fU, 0xb903U, 0U, 1U, 0U},
        // x = 0x1.fep-3, log10f16(x) = -0x1.35p-1 (RZ)
        {0x33f8U, 0xb8d4U, 0U, 1U, 1U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.394p-1, log10f16(x) = -0x1.b4cp-3 (RZ)
        {0x38e5U, 0xb2d3U, 0U, 1U, 1U},
#endif
        // x = 0x1.ea8p-1, log10f16(x) = -0x1.31p-6 (RZ)
        {0x3baaU, 0xa4c4U, 0U, 1U, 1U},
        // x = 0x1.ebp-1, log10f16(x) = -0x1.29cp-6 (RZ)
        {0x3bacU, 0xa4a7U, 0U, 1U, 1U},
        // x = 0x1.f3p-1, log10f16(x) = -0x1.6dcp-7 (RZ)
        {0x3bccU, 0xa1b7U, 0U, 1U, 1U},
// x = 0x1.f38p-1, log10f16(x) = -0x1.5f8p-7 (RZ)
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        {0x3bceU, 0xa17eU, 0U, 1U, 1U},
        // x = 0x1.fd8p-1, log10f16(x) = -0x1.168p-9 (RZ)
        {0x3bf6U, 0x985aU, 0U, 1U, 1U},
        // x = 0x1.ff8p-1, log10f16(x) = -0x1.bccp-12 (RZ)
        {0x3bfeU, 0x8ef3U, 0U, 1U, 1U},
        // x = 0x1.374p+0, log10f16(x) = 0x1.5b8p-4 (RZ)
        {0x3cddU, 0x2d6eU, 1U, 0U, 1U},
        // x = 0x1.3ecp+1, log10f16(x) = 0x1.958p-2 (RZ)
        {0x40fbU, 0x3656U, 1U, 0U, 1U},
#endif
        // x = 0x1.4p+3, log10f16(x) = 0x1p+0 (RZ)
        {0x4900U, 0x3c00U, 0U, 0U, 0U},
        // x = 0x1.9p+6, log10f16(x) = 0x1p+1 (RZ)
        {0x5640U, 0x4000U, 0U, 0U, 0U},
        // x = 0x1.f84p+6, log10f16(x) = 0x1.0ccp+1 (RZ)
        {0x57e1U, 0x4033U, 1U, 0U, 0U},
        // x = 0x1.f4p+9, log10f16(x) = 0x1.8p+1 (RZ)
        {0x63d0U, 0x4200U, 0U, 0U, 0U},
        // x = 0x1.388p+13, log10f16(x) = 0x1p+2 (RZ)
        {0x70e2U, 0x4400U, 0U, 0U, 0U},
        // x = 0x1.674p+13, log10f16(x) = 0x1.03cp+2 (RZ)
        {0x719dU, 0x440fU, 1U, 0U, 0U},
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
} // namespace log10f16_internal

LIBC_INLINE float16 log10f16(float16 x) {
  using namespace math::expxf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();

  // If x <= 0, or x is 1, or x is +inf, or x is NaN.
  if (LIBC_UNLIKELY(x_u == 0U || x_u == 0x3c00U || x_u >= 0x7c00U)) {
    // log10(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // log10(+/-0) = −inf
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

    // log10(+inf) = +inf
    return FPBits::inf().get_val();
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = log10f16_internal::LOG10F16_EXCEPTS.lookup(x_u);
      LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // To compute log10(x), we perform the following range reduction:
  //   x = 2^m * 1.mant,
  //   log10(x) = m * log10(2) + log10(1.mant).
  // To compute log10(1.mant), let f be the highest 6 bits including the hidden
  // bit, and d be the difference (1.mant - f), i.e., the remaining 5 bits of
  // the mantissa, then:
  //   log10(1.mant) = log10(f) + log10(1.mant / f)
  //                 = log10(f) + log10(1 + d/f)
  // since d/f is sufficiently small.
  // We store log10(f) and 1/f in the lookup tables LOG10F_F and ONE_OVER_F_F
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
  //   > P = fpminimax(log10(1 + x)/x, 2, [|SG...|], [-2^-5, 2^-5]);
  //   > x * P;
  float log10p1_d_over_f =
      v * fputil::polyeval(v, 0x1.bcb7bp-2f, -0x1.bce168p-3f, 0x1.28acb8p-3f);
  // log10(1.mant) = log10(f) + log10(1 + d/f)
  float log10_1_mant = LOG10F_F[f] + log10p1_d_over_f;
  return fputil::cast<float16>(
      fputil::multiply_add(static_cast<float>(m), LOG10F_2, log10_1_mant));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F16_H
