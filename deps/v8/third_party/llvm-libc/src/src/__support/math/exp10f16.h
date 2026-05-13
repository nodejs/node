//===-- Implementation header for exp10f16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "exp10f16_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
LIBC_INLINE_VAR constexpr size_t N_EXP10F16_EXCEPTS = 5;
#else
LIBC_INLINE_VAR constexpr size_t N_EXP10F16_EXCEPTS = 8;
#endif

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float16, N_EXP10F16_EXCEPTS>
    EXP10F16_EXCEPTS = {{
        // x = 0x1.8f4p-2, exp10f16(x) = 0x1.3ap+1 (RZ)
        {0x363dU, 0x40e8U, 1U, 0U, 1U},
        // x = 0x1.95cp-2, exp10f16(x) = 0x1.3ecp+1 (RZ)
        {0x3657U, 0x40fbU, 1U, 0U, 0U},
        // x = -0x1.018p-4, exp10f16(x) = 0x1.bbp-1 (RZ)
        {0xac06U, 0x3aecU, 1U, 0U, 0U},
        // x = -0x1.c28p+0, exp10f16(x) = 0x1.1ccp-6 (RZ)
        {0xbf0aU, 0x2473U, 1U, 0U, 0U},
        // x = -0x1.e1cp+1, exp10f16(x) = 0x1.694p-13 (RZ)
        {0xc387U, 0x09a5U, 1U, 0U, 0U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
        // x = 0x1.0cp+1, exp10f16(x) = 0x1.f04p+6 (RZ)
        {0x4030U, 0x57c1U, 1U, 0U, 1U},
        // x = 0x1.1b8p+1, exp10f16(x) = 0x1.47cp+7 (RZ)
        {0x406eU, 0x591fU, 1U, 0U, 1U},
        // x = 0x1.1b8p+2, exp10f16(x) = 0x1.a4p+14 (RZ)
        {0x446eU, 0x7690U, 1U, 0U, 1U},
#endif
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

LIBC_INLINE constexpr float16 exp10f16(float16 x) {
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();
  uint16_t x_abs = x_u & 0x7fffU;

  // When |x| >= 5, or x is NaN.
  if (LIBC_UNLIKELY(x_abs >= 0x4500U)) {
    // exp10(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // When x >= 5.
    if (x_bits.is_pos()) {
      // exp10(+inf) = +inf
      if (x_bits.is_inf())
        return FPBits::inf().get_val();

      switch (fputil::quick_get_round()) {
      case FE_TONEAREST:
      case FE_UPWARD:
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_OVERFLOW);
        return FPBits::inf().get_val();
      default:
        return FPBits::max_normal().get_val();
      }
    }

    // When x <= -8.
    if (x_u >= 0xc800U) {
      // exp10(-inf) = +0
      if (x_bits.is_inf())
        return FPBits::zero().get_val();

      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);

      if (fputil::fenv_is_round_up())
        return FPBits::min_subnormal().get_val();
      return FPBits::zero().get_val();
    }
  }

  // When x is 1, 2, 3, or 4. These are hard-to-round cases with exact results.
  if (LIBC_UNLIKELY((x_u & ~(0x3c00U | 0x4000U | 0x4200U | 0x4400U)) == 0)) {
    switch (x_u) {
    case 0x3c00U: // x = 1.0f16
      return fputil::cast<float16>(10.0);
    case 0x4000U: // x = 2.0f16
      return fputil::cast<float16>(100.0);
    case 0x4200U: // x = 3.0f16
      return fputil::cast<float16>(1'000.0);
    case 0x4400U: // x = 4.0f16
      return fputil::cast<float16>(10'000.0);
    }
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = EXP10F16_EXCEPTS.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // 10^x = 2^((hi + mid) * log2(10)) * 10^lo
  auto [exp2_hi_mid, exp10_lo] = exp10_range_reduction(x);
  return fputil::cast<float16>(exp2_hi_mid * exp10_lo);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F16_H
