//===-- Implementation header for rsqrtf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_RSQRTF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_RSQRTF_H

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr float rsqrtf(float x) {
  using FPBits = fputil::FPBits<float>;
  FPBits xbits(x);

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = x_u & 0x7fff'ffffU;

  constexpr uint32_t INF_BITS = FPBits::inf().uintval();

  // x is 0, inf/nan, or negative.
  if (LIBC_UNLIKELY(x_u == 0 || x_u >= INF_BITS)) {
    // x is NaN
    if (x_abs > INF_BITS) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }

    // |x| = 0
    if (x_abs == 0) {
      fputil::raise_except_if_required(FE_DIVBYZERO);
      fputil::set_errno_if_required(ERANGE);
      return FPBits::inf(xbits.sign()).get_val();
    }

    // -inf <= x < 0
    if (x_u > 0x7fff'ffffU) {
      fputil::raise_except_if_required(FE_INVALID);
      fputil::set_errno_if_required(EDOM);
      return FPBits::quiet_nan().get_val();
    }

    // x = +inf => rsqrt(x) = 0
    return FPBits::zero(xbits.sign()).get_val();
  }

  // TODO: add float based approximation when
  // LIBC_TARGET_CPU_HAS_FPU_DOUBLE is not defined
  double result = 1.0 / fputil::sqrt<double>(fputil::cast<double>(x));

  return fputil::cast<float>(result);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_RSQRTF_H
