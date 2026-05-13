//===-- Half-precision sinh(x) function -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINHF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINHF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "expxf16_utils.h"
#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE constexpr float16 sinhf16(float16 x) {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  constexpr fputil::ExceptValues<float16, 17> SINHF16_EXCEPTS_POS = {{
      // x = 0x1.714p-5, sinhf16(x) = 0x1.714p-5 (RZ)
      {0x29c5U, 0x29c5U, 1U, 0U, 1U},
      // x = 0x1.25p-4, sinhf16(x) = 0x1.25p-4 (RZ)
      {0x2c94U, 0x2c94U, 1U, 0U, 1U},
      // x = 0x1.f5p-4, sinhf16(x) = 0x1.f64p-4 (RZ)
      {0x2fd4U, 0x2fd9U, 1U, 0U, 0U},
      // x = 0x1.b1cp-3, sinhf16(x) = 0x1.b4cp-3 (RZ)
      {0x32c7U, 0x32d3U, 1U, 0U, 1U},
      // x = 0x1.6e8p-2, sinhf16(x) = 0x1.764p-2 (RZ)
      {0x35baU, 0x35d9U, 1U, 0U, 1U},
      // x = 0x1.6b4p-1, sinhf16(x) = 0x1.8a4p-1 (RZ)
      {0x39adU, 0x3a29U, 1U, 0U, 1U},
      // x = 0x1.a58p-1, sinhf16(x) = 0x1.d68p-1 (RZ)
      {0x3a96U, 0x3b5aU, 1U, 0U, 1U},
      // x = 0x1.574p+0, sinhf16(x) = 0x1.c78p+0 (RZ)
      {0x3d5dU, 0x3f1eU, 1U, 0U, 1U},
      // x = 0x1.648p+1, sinhf16(x) = 0x1.024p+3 (RZ)
      {0x4192U, 0x4809U, 1U, 0U, 0U},
      // x = 0x1.cdcp+1, sinhf16(x) = 0x1.26cp+4 (RZ)
      {0x4337U, 0x4c9bU, 1U, 0U, 0U},
      // x = 0x1.d0cp+1, sinhf16(x) = 0x1.2d8p+4 (RZ)
      {0x4343U, 0x4cb6U, 1U, 0U, 1U},
      // x = 0x1.018p+2, sinhf16(x) = 0x1.bfp+4 (RZ)
      {0x4406U, 0x4efcU, 1U, 0U, 0U},
      // x = 0x1.2fcp+2, sinhf16(x) = 0x1.cc4p+5 (RZ)
      {0x44bfU, 0x5331U, 1U, 0U, 1U},
      // x = 0x1.4ecp+2, sinhf16(x) = 0x1.75cp+6 (RZ)
      {0x453bU, 0x55d7U, 1U, 0U, 0U},
      // x = 0x1.8a4p+2, sinhf16(x) = 0x1.d94p+7 (RZ)
      {0x4629U, 0x5b65U, 1U, 0U, 1U},
      // x = 0x1.5fp+3, sinhf16(x) = 0x1.c54p+14 (RZ)
      {0x497cU, 0x7715U, 1U, 0U, 1U},
      // x = 0x1.3c8p+1, sinhf16(x) = 0x1.78ap+2 (RZ)
      {0x40f2U, 0x45e2U, 1U, 0U, 1U},
  }};

  constexpr fputil::ExceptValues<float16, 13> SINHF16_EXCEPTS_NEG = {{
      // x = -0x1.714p-5, sinhf16(x) = -0x1.714p-5 (RZ)
      {0xa9c5U, 0xa9c5U, 0U, 1U, 1U},
      // x = -0x1.25p-4, sinhf16(x) = -0x1.25p-4 (RZ)
      {0xac94U, 0xac94U, 0U, 1U, 1U},
      // x = -0x1.f5p-4, sinhf16(x) = -0x1.f64p-4 (RZ)
      {0xafd4U, 0xafd9U, 0U, 1U, 0U},
      // x = -0x1.6e8p-2, sinhf16(x) = -0x1.764p-2 (RZ)
      {0xb5baU, 0xb5d9U, 0U, 1U, 1U},
      // x = -0x1.a58p-1, sinhf16(x) = -0x1.d68p-1 (RZ)
      {0xba96U, 0xbb5aU, 0U, 1U, 1U},
      // x = -0x1.cdcp+1, sinhf16(x) = -0x1.26cp+4 (RZ)
      {0xc337U, 0xcc9bU, 0U, 1U, 0U},
      // x = -0x1.d0cp+1, sinhf16(x) = -0x1.2d8p+4 (RZ)
      {0xc343U, 0xccb6U, 0U, 1U, 1U},
      // x = -0x1.018p+2, sinhf16(x) = -0x1.bfp+4 (RZ)
      {0xc406U, 0xcefcU, 0U, 1U, 0U},
      // x = -0x1.2fcp+2, sinhf16(x) = -0x1.cc4p+5 (RZ)
      {0xc4bfU, 0xd331U, 0U, 1U, 1U},
      // x = -0x1.4ecp+2, sinhf16(x) = -0x1.75cp+6 (RZ)
      {0xc53bU, 0xd5d7U, 0U, 1U, 0U},
      // x = -0x1.8a4p+2, sinhf16(x) = -0x1.d94p+7 (RZ)
      {0xc629U, 0xdb65U, 0U, 1U, 1U},
      // x = -0x1.5fp+3, sinhf16(x) = -0x1.c54p+14 (RZ)
      {0xc97cU, 0xf715U, 0U, 1U, 1U},
      // x = -0x1.3c8p+1, sinhf16(x) = -0x1.78ap+2 (RZ)
      {0xc0f2U, 0xc5e2U, 0U, 1U, 1U},
  }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  using namespace math::expxf16_internal;
  using FPBits = fputil::FPBits<float16>;
  FPBits x_bits(x);

  uint16_t x_u = x_bits.uintval();
  uint16_t x_abs = x_u & 0x7fffU;

  // When |x| = 0, or -2^(-14) <= x <= -2^(-9), or |x| >= asinh(2^16), or x is
  // NaN.
  if (LIBC_UNLIKELY(x_abs == 0U || (x_u >= 0x8400U && x_u <= 0xa400U) ||
                    x_abs >= 0x49e5U)) {
    // sinh(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }

    // sinh(+/-0) = sinh(+/-0)
    if (x_abs == 0U)
      return FPBits::zero(x_bits.sign()).get_val();

    // When |x| >= asinh(2^16).
    if (x_abs >= 0x49e5U) {
      // sinh(+/-inf) = +/-inf
      if (x_bits.is_inf())
        return FPBits::inf(x_bits.sign()).get_val();

      int rounding_mode = fputil::quick_get_round();
      if (rounding_mode == FE_TONEAREST ||
          (x_bits.is_pos() && rounding_mode == FE_UPWARD) ||
          (x_bits.is_neg() && rounding_mode == FE_DOWNWARD)) {
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
        return FPBits::inf(x_bits.sign()).get_val();
      }
      return FPBits::max_normal(x_bits.sign()).get_val();
    }

    // When -2^(-14) <= x <= -2^(-9).
    if (fputil::fenv_is_round_down())
      return FPBits(static_cast<uint16_t>(x_u + 1)).get_val();
    return FPBits(static_cast<uint16_t>(x_u)).get_val();
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (x_bits.is_pos()) {
    if (auto r = SINHF16_EXCEPTS_POS.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
      return r.value();
  } else {
    if (auto r = SINHF16_EXCEPTS_NEG.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
      return r.value();
  }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  return eval_sinh_or_cosh</*IsSinh=*/true>(x);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINHF16_H
