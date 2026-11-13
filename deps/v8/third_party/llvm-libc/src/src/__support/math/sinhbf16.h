//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of sinhbf16.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINHBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINHBF16_H

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

LIBC_INLINE bfloat16 sinhbf16(bfloat16 x) {
  using FPBits = fputil::FPBits<bfloat16>;
  FPBits x_bits(x);
  uint16_t x_u = x_bits.uintval();
  uint16_t x_abs = x_u & 0x7fff;

  // |x| <= 0.11328125
  if (x_abs <= 0x3de8) {
    if (x_abs == 0U)
      return x;

#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    if (x_u & 0x8000) {
      if (fputil::fenv_is_round_down())
        return FPBits(static_cast<uint16_t>(x_u + 1)).get_val();
    } else {
      if (fputil::fenv_is_round_up())
        return FPBits(static_cast<uint16_t>(x_u + 1)).get_val();
    }
#endif
    return FPBits(static_cast<uint16_t>(x_u)).get_val();
  }

  // |x| >= 89.5
  if (LIBC_UNLIKELY(x_abs >= 0x42b3)) {

    // sinh(inf) = inf
    if (x_bits.is_inf())
      return x;

    // sinh(NaN) = NaN
    if (x_bits.is_nan()) {
      if (x_bits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }

    return fputil::set_overflow<bfloat16>(x_bits.sign());
  }

  float xf = static_cast<float>(x);

  // |x| >= 6.875
  // return e^x / 2
  if (x_abs >= 0x40dc) {
    float abs_result =
        math::sinhcoshbf16_internal::exp_half(x_bits.abs().get_val());
    if (x_bits.is_neg()) {
      abs_result = -abs_result;
    }
    return fputil::cast<bfloat16>(abs_result);
  }

  // sinh(x) = (e^x - e^(-x)) / 2.
  float result =
      math::sinhcoshbf16_internal::eval_sinh_or_cosh</*is_sinh*/ true>(xf);

  return fputil::cast<bfloat16>(result);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINHBF16_H
