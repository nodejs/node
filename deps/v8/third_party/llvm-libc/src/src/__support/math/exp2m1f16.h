//===-- Implementation header for exp2m1f16 ----------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP2M1F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP2M1F16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/math/expxf16_utils.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float16 exp2m1f16(float16 x) {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  constexpr fputil::ExceptValues<float16, 6> EXP2M1F16_EXCEPTS_LO = {{
      // (input, RZ output, RU offset, RD offset, RN offset)
      // x = 0x1.cf4p-13, exp2m1f16(x) = 0x1.41p-13 (RZ)
      {0x0b3dU, 0x0904U, 1U, 0U, 1U},
      // x = 0x1.4fcp-12, exp2m1f16(x) = 0x1.d14p-13 (RZ)
      {0x0d3fU, 0x0b45U, 1U, 0U, 1U},
      // x = 0x1.63p-11, exp2m1f16(x) = 0x1.ec4p-12 (RZ)
      {0x118cU, 0x0fb1U, 1U, 0U, 0U},
      // x = 0x1.6fp-7, exp2m1f16(x) = 0x1.fe8p-8 (RZ)
      {0x21bcU, 0x1ffaU, 1U, 0U, 1U},
      // x = -0x1.c6p-10, exp2m1f16(x) = -0x1.3a8p-10 (RZ)
      {0x9718U, 0x94eaU, 0U, 1U, 0U},
      // x = -0x1.cfcp-10, exp2m1f16(x) = -0x1.414p-10 (RZ)
      {0x973fU, 0x9505U, 0U, 1U, 0U},
  }};

#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
  constexpr size_t N_EXP2M1F16_EXCEPTS_HI = 6;
#else
  constexpr size_t N_EXP2M1F16_EXCEPTS_HI = 7;
#endif

  constexpr fputil::ExceptValues<float16, N_EXP2M1F16_EXCEPTS_HI>
      EXP2M1F16_EXCEPTS_HI = {{
          // (input, RZ output, RU offset, RD offset, RN offset)
          // x = 0x1.e58p-3, exp2m1f16(x) = 0x1.6dcp-3 (RZ)
          {0x3396U, 0x31b7U, 1U, 0U, 0U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
          // x = 0x1.2e8p-2, exp2m1f16(x) = 0x1.d14p-3 (RZ)
          {0x34baU, 0x3345U, 1U, 0U, 0U},
#endif
          // x = 0x1.ad8p-2, exp2m1f16(x) = 0x1.598p-2 (RZ)
          {0x36b6U, 0x3566U, 1U, 0U, 0U},
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
          // x = 0x1.edcp-2, exp2m1f16(x) = 0x1.964p-2 (RZ)
          {0x37b7U, 0x3659U, 1U, 0U, 1U},
#endif
          // x = -0x1.804p-3, exp2m1f16(x) = -0x1.f34p-4 (RZ)
          {0xb201U, 0xafcdU, 0U, 1U, 1U},
          // x = -0x1.f3p-3, exp2m1f16(x) = -0x1.3e4p-3 (RZ)
          {0xb3ccU, 0xb0f9U, 0U, 1U, 0U},
          // x = -0x1.294p-1, exp2m1f16(x) = -0x1.53p-2 (RZ)
          {0xb8a5U, 0xb54cU, 0U, 1U, 1U},
#ifndef LIBC_TARGET_CPU_HAS_FMA_FLOAT
          // x = -0x1.a34p-1, exp2m1f16(x) = -0x1.bb4p-2 (RZ)
          {0xba8dU, 0xb6edU, 0U, 1U, 1U},
#endif
      }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  using namespace math::expxf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();
  uint16_t x_abs = x_u & 0x7fffU;

  // When |x| <= 2^(-3), or |x| >= 11, or x is NaN.
  if (LIBC_UNLIKELY(x_abs <= 0x3000U || x_abs >= 0x4980U)) {
    // exp2m1(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // When x >= 16.
    if (x_u >= 0x4c00 && x_bits.is_pos()) {
      // exp2m1(+inf) = +inf
      if (x_bits.is_inf())
        return FPBits::inf().get_val();

      switch (fputil::quick_get_round()) {
      case FE_TONEAREST:
      case FE_UPWARD:
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
        return FPBits::inf().get_val();
      default:
        return FPBits::max_normal().get_val();
      }
    }

    // When x < -11.
    if (x_u > 0xc980U) {
      // exp2m1(-inf) = -1
      if (x_bits.is_inf())
        return FPBits::one(Sign::NEG).get_val();

      // When -12 < x < -11, round(2^x - 1, HP, RN) = -0x1.ffcp-1.
      if (x_u < 0xca00U)
        return fputil::round_result_slightly_down(
            fputil::cast<float16>(-0x1.ffcp-1));

      // When x <= -12, round(2^x - 1, HP, RN) = -1.
      switch (fputil::quick_get_round()) {
      case FE_TONEAREST:
      case FE_DOWNWARD:
        return FPBits::one(Sign::NEG).get_val();
      default:
        return fputil::cast<float16>(-0x1.ffcp-1);
      }
    }

    // When |x| <= 2^(-3).
    if (x_abs <= 0x3000U) {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      if (auto r = EXP2M1F16_EXCEPTS_LO.lookup(x_u);
          LIBC_UNLIKELY(r.has_value()))
        return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

      float xf = x;
      // Degree-5 minimax polynomial generated by Sollya with the following
      // commands:
      //   > display = hexadecimal;
      //   > P = fpminimax((2^x - 1)/x, 4, [|SG...|], [-2^-3, 2^-3]);
      //   > x * P;
      return fputil::cast<float16>(
          xf * fputil::polyeval(xf, 0x1.62e43p-1f, 0x1.ebfbdep-3f,
                                0x1.c6af88p-5f, 0x1.3b45d6p-7f,
                                0x1.641e7cp-10f));
    }
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = EXP2M1F16_EXCEPTS_HI.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // exp2(x) = exp2(hi + mid) * exp2(lo)
  auto [exp2_hi_mid, exp2_lo] = exp2_range_reduction(x);
  // exp2m1(x) = exp2(hi + mid) * exp2(lo) - 1
  return fputil::cast<float16>(
      fputil::multiply_add(exp2_hi_mid, exp2_lo, -1.0f));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP2M1F16_H
