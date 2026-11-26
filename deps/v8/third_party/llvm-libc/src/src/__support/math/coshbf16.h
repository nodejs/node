//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the coshbf16 function, which computes the hyperbolic
/// cosine of a bfloat16 value.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COSHBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COSHBF16_H

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/sinhcoshbf16_utils.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE bfloat16 coshbf16(bfloat16 x) {
  using FPBits = fputil::FPBits<bfloat16>;
  FPBits x_bits(x);

  // cosh is an even function, cosh(-x) = cosh(x), so we only need |x|.
  // We bound the cases below by comparing the bits of |x| as an integer.
  // NaN and Inf have the largest bits of |x|, so they are handled together with
  // the overflow case below.
  uint16_t x_abs_uint = x_bits.abs().uintval();

  constexpr uint16_t ONE_UINT = 0x3F80; // 0x1p0 = 1.0

  // For 0 < |x| <= 0.0879, cosh(x) = 1 + x^2/2 + ... lies strictly between 1
  // and the next bfloat16 above 1, and is closer to 1. So the result is 1 in
  // all rounding modes except for RU, where it is the next value above 1.
  // x = 0 is the exception, since cosh(0) = 1 exactly.
  constexpr uint16_t ROUNDS_TO_ONE_ABS_BOUND = 0x3DB4; // 0x1.68p-4 = 0.0879
  if (x_abs_uint <= ROUNDS_TO_ONE_ABS_BOUND) {
    if (x_abs_uint == 0U)
      return FPBits(ONE_UINT).get_val();

#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    constexpr uint16_t ONE_NEXT_UP_UINT = 0x3F81; // 0x1.02p0 = 1.0078125
    if (fputil::fenv_is_round_up())
      return FPBits(ONE_NEXT_UP_UINT).get_val();
#endif
    return FPBits(ONE_UINT).get_val();
  }

  // cosh(x) overflows bfloat16 for |x| >= 89.5. cosh(x) > 0 for all x, so the
  // overflow is always to +Inf, regardless of the sign of x.
  constexpr uint16_t OVERFLOW_ABS_BOUND = 0x42B3; // 0x1.66p6 = 89.5
  if (LIBC_UNLIKELY(x_abs_uint >= OVERFLOW_ABS_BOUND)) {
    if (x_bits.is_nan()) {
      // If it is a signaling NaN, then we raise an FE_INVALID exception
      // and return a quiet NaN
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // If not, just return the NaN
      return x;
    }

    // cosh(+-Inf) = +Inf
    if (x_bits.is_inf())
      return FPBits::inf().get_val();

    return fputil::set_overflow<bfloat16>();
  }

  // For 6.875 <= |x| < 89.5, e^(-|x|) / e^|x| = e^(-2|x|) < 2^-19 is far below
  // the precision of bfloat16, so we can use
  //   cosh(x) = (e^|x| + e^(-|x|)) / 2 ~ e^|x| / 2.
  constexpr uint16_t EXP_HALF_ABS_BOUND = 0x40DC; // 0x1.b8p2 = 6.875
  if (x_abs_uint >= EXP_HALF_ABS_BOUND) {
    float x_abs_float = fputil::cast<float>(x_bits.abs().get_val());
    return fputil::cast<bfloat16>(sinhcoshbf16_internal::exp_half(x_abs_float));
  }

  // For the remaining inputs, 0.0879 < |x| < 6.875, we compute
  //   cosh(x) = (e^x + e^(-x)) / 2
  // using the range reduction and the polynomial shared with sinhbf16 in
  // sinhcoshbf16_utils.h.
  float x_float = fputil::cast<float>(x);
  return fputil::cast<bfloat16>(
      sinhcoshbf16_internal::eval_sinh_or_cosh</*IsSinh=*/false>(x_float));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COSHBF16_H
