//===-- Implementation header for exp2f16 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "expxf16_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float16 exp2f16(float16 x) {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  constexpr fputil::ExceptValues<float16, 3> EXP2F16_EXCEPTS = {{
      // (input, RZ output, RU offset, RD offset, RN offset)
      // x = 0x1.714p-11, exp2f16(x) = 0x1p+0 (RZ)
      {0x11c5U, 0x3c00U, 1U, 0U, 1U},
      // x = -0x1.558p-4, exp2f16(x) = 0x1.e34p-1 (RZ)
      {0xad56U, 0x3b8dU, 1U, 0U, 0U},
      // x = -0x1.d5cp-4, exp2f16(x) = 0x1.d8cp-1 (RZ)
      {0xaf57U, 0x3b63U, 1U, 0U, 0U},
  }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  using namespace math::expxf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();
  uint16_t x_abs = x_u & 0x7fffU;

  // When |x| >= 16, or x is NaN.
  if (LIBC_UNLIKELY(x_abs >= 0x4c00U)) {
    // exp2(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // When x >= 16.
    if (x_bits.is_pos()) {
      // exp2(+inf) = +inf
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

    // When x <= -25.
    if (x_u >= 0xce40U) {
      // exp2(-inf) = +0
      if (x_bits.is_inf())
        return FPBits::zero().get_val();

      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);

      if (fputil::fenv_is_round_up())
        return FPBits::min_subnormal().get_val();
      return FPBits::zero().get_val();
    }
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = EXP2F16_EXCEPTS.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // exp2(x) = exp2(hi + mid) * exp2(lo)
  auto [exp2_hi_mid, exp2_lo] = exp2_range_reduction(x);
  return fputil::cast<float16>(exp2_hi_mid * exp2_lo);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F16_H
