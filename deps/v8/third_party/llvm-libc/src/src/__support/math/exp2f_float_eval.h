//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Float-only implementation of exp2f.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_FLOAT_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_FLOAT_EVAL_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/exp2f_float_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace float_eval {

LIBC_INLINE float exp2f(float x) {
  using FPBits = fputil::FPBits<float>;
  FPBits xbits(x);

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = x_u & 0x7fff'ffffU;

  // When |x| >= 128, or x is nan, or |x| <= 2^-25
  if (LIBC_UNLIKELY(x_abs >= 0x4300'0000U || x_abs <= 0x3280'0000U)) {
    // |x| <= 2^-25
    if (x_abs <= 0x3280'0000U) {
      return 1.0f + x;
    }

    // x >= 128
    if (xbits.is_pos()) {
      // x is finite
      if (x_u < 0x7f80'0000U)
        return fputil::set_overflow<float>();
      // x is +inf or nan
      return x + FPBits::inf().get_val();
    }
    // x <= -150
    if (x_u >= 0xc316'0000U) {
      // exp(-Inf) = 0
      if (xbits.is_inf())
        return 0.0f;
      // exp(nan) = nan
      if (xbits.is_nan())
        return x;
      return fputil::set_underflow<float>();
    }
  }

  // Range reduction:
  //   k = round(x)
  //   x = k + u, with |u| <= 0.5
  //   2^x = 2^k * 2^u
  float kf = fputil::nearest_integer(x);
  int k = static_cast<int>(kf);

  float u = fputil::multiply_add(kf, -1.0f, x);
  return exp2f_eval(u, k);
}

} // namespace float_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_FLOAT_EVAL_H
