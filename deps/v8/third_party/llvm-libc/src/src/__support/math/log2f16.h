//===-- Implementation header for log2f16 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F16_H

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

namespace log2f16_internal {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
LIBC_INLINE_VAR constexpr size_t N_LOG2F16_EXCEPTS = 2;
#else
LIBC_INLINE_VAR constexpr size_t N_LOG2F16_EXCEPTS = 9;
#endif

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float16, N_LOG2F16_EXCEPTS>
    LOG2F16_EXCEPTS = {{
// (input, RZ output, RU offset, RD offset, RN offset)
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.224p-1, log2f16(x) = -0x1.a34p-1 (RZ)
        {0x3889U, 0xba8dU, 0U, 1U, 0U},
        // x = 0x1.e34p-1, log2f16(x) = -0x1.558p-4 (RZ)
        {0x3b8dU, 0xad56U, 0U, 1U, 0U},
#endif
        // x = 0x1.e8cp-1, log2f16(x) = -0x1.128p-4 (RZ)
        {0x3ba3U, 0xac4aU, 0U, 1U, 0U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.f98p-1, log2f16(x) = -0x1.2ep-6 (RZ)
        {0x3be6U, 0xa4b8U, 0U, 1U, 0U},
        // x = 0x1.facp-1, log2f16(x) = -0x1.e7p-7 (RZ)
        {0x3bebU, 0xa39cU, 0U, 1U, 1U},
#endif
        // x = 0x1.fb4p-1, log2f16(x) = -0x1.b88p-7 (RZ)
        {0x3bedU, 0xa2e2U, 0U, 1U, 1U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.fecp-1, log2f16(x) = -0x1.cep-9 (RZ)
        {0x3bfbU, 0x9b38U, 0U, 1U, 1U},
        // x = 0x1.ffcp-1, log2f16(x) = -0x1.714p-11 (RZ)
        {0x3bffU, 0x91c5U, 0U, 1U, 1U},
        // x = 0x1.224p+0, log2f16(x) = 0x1.72cp-3 (RZ)
        {0x3c89U, 0x31cbU, 1U, 0U, 1U},
#endif
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace log2f16_internal

LIBC_INLINE float16 log2f16(float16 x) {
  using namespace math::expxf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();

  // If x <= 0, or x is 1, or x is +inf, or x is NaN.
  if (LIBC_UNLIKELY(x_u == 0U || x_u == 0x3c00U || x_u >= 0x7c00U)) {
    // log2(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // log2(+/-0) = −inf
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

    // log2(+inf) = +inf
    return FPBits::inf().get_val();
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = log2f16_internal::LOG2F16_EXCEPTS.lookup(x_u);
      LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // To compute log2(x), we perform the following range reduction:
  //   x = 2^m * 1.mant,
  //   log2(x) = m + log2(1.mant).
  // To compute log2(1.mant), let f be the highest 6 bits including the hidden
  // bit, and d be the difference (1.mant - f), i.e., the remaining 5 bits of
  // the mantissa, then:
  //   log2(1.mant) = log2(f) + log2(1.mant / f)
  //                = log2(f) + log2(1 + d/f)
  // since d/f is sufficiently small.
  // We store log2(f) and 1/f in the lookup tables LOG2F_F and ONE_OVER_F_F
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
  //   > P = fpminimax(log2(1 + x)/x, 2, [|SG...|], [-2^-5, 2^-5]);
  //   > x * P;
  float log2p1_d_over_f =
      v * fputil::polyeval(v, 0x1.715476p+0f, -0x1.71771ap-1f, 0x1.ecb38ep-2f);
  // log2(1.mant) = log2(f) + log2(1 + d/f)
  float log2_1_mant = LOG2F_F[f] + log2p1_d_over_f;
  return fputil::cast<float16>(static_cast<float>(m) + log2_1_mant);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F16_H
