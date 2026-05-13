//===-- Implementation header for exp10f ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F_H

#include "exp10f_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr float exp10f(float x) {
  using FPBits = typename fputil::FPBits<float>;
  FPBits xbits(x);

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = x_u & 0x7fff'ffffU;

  // When |x| >= log10(2^128), or x is nan
  if (LIBC_UNLIKELY(x_abs >= 0x421a'209bU)) {
    // When x < log10(2^-150) or nan
    if (x_u > 0xc234'9e35U) {
      // exp(-Inf) = 0
      if (xbits.is_inf())
        return 0.0f;
      // exp(nan) = nan
      if (xbits.is_nan())
        return x;
      if (fputil::fenv_is_round_up())
        return FPBits::min_subnormal().get_val();
      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_UNDERFLOW);
      return 0.0f;
    }
    // x >= log10(2^128) or nan
    if (xbits.is_pos() && (x_u >= 0x421a'209bU)) {
      // x is finite
      if (x_u < 0x7f80'0000U) {
        int rounding = fputil::quick_get_round();
        if (rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO)
          return FPBits::max_normal().get_val();

        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_OVERFLOW);
      }
      // x is +inf or nan
      return x + FPBits::inf().get_val();
    }
  }

  // When |x| <= log10(2)*2^-6
  if (LIBC_UNLIKELY(x_abs <= 0x3b9a'209bU)) {
    if (LIBC_UNLIKELY(x_u == 0xb25e'5bd9U)) { // x = -0x1.bcb7b2p-27f
      if (fputil::fenv_is_round_to_nearest())
        return 0x1.fffffep-1f;
    }
    // |x| < 2^-25
    // 10^x ~ 1 + log(10) * x
    if (LIBC_UNLIKELY(x_abs <= 0x3280'0000U)) {
      return fputil::multiply_add(x, 0x1.26bb1cp+1f, 1.0f);
    }

    return static_cast<float>(Exp10Base::powb_lo(x));
  }

  // Exceptional value.
  if (LIBC_UNLIKELY(x_u == 0x3d14'd956U)) { // x = 0x1.29b2acp-5f
    if (fputil::fenv_is_round_up())
      return 0x1.1657c4p+0f;
  }

  // Exact outputs when x = 1, 2, ..., 10.
  // Quick check mask: 0x800f'ffffU = ~(bits of 1.0f | ... | bits of 10.0f)
  if (LIBC_UNLIKELY((x_u & 0x800f'ffffU) == 0)) {
    switch (x_u) {
    case 0x3f800000U: // x = 1.0f
      return 10.0f;
    case 0x40000000U: // x = 2.0f
      return 100.0f;
    case 0x40400000U: // x = 3.0f
      return 1'000.0f;
    case 0x40800000U: // x = 4.0f
      return 10'000.0f;
    case 0x40a00000U: // x = 5.0f
      return 100'000.0f;
    case 0x40c00000U: // x = 6.0f
      return 1'000'000.0f;
    case 0x40e00000U: // x = 7.0f
      return 10'000'000.0f;
    case 0x41000000U: // x = 8.0f
      return 100'000'000.0f;
    case 0x41100000U: // x = 9.0f
      return 1'000'000'000.0f;
    case 0x41200000U: // x = 10.0f
      return 10'000'000'000.0f;
    }
  }

  // Range reduction: 10^x = 2^(mid + hi) * 10^lo
  //   rr = (2^(mid + hi), lo)
  auto rr = exp_b_range_reduc<Exp10Base>(x);

  // The low part is approximated by a degree-5 minimax polynomial.
  // 10^lo ~ 1 + COEFFS[0] * lo + ... + COEFFS[4] * lo^5
  using fputil::multiply_add;
  double lo2 = rr.lo * rr.lo;
  // c0 = 1 + COEFFS[0] * lo
  double c0 = multiply_add(rr.lo, Exp10Base::COEFFS[0], 1.0);
  // c1 = COEFFS[1] + COEFFS[2] * lo
  double c1 = multiply_add(rr.lo, Exp10Base::COEFFS[2], Exp10Base::COEFFS[1]);
  // c2 = COEFFS[3] + COEFFS[4] * lo
  double c2 = multiply_add(rr.lo, Exp10Base::COEFFS[4], Exp10Base::COEFFS[3]);
  // p = c1 + c2 * lo^2
  //   = COEFFS[1] + COEFFS[2] * lo + COEFFS[3] * lo^2 + COEFFS[4] * lo^3
  double p = multiply_add(lo2, c2, c1);
  // 10^lo ~ c0 + p * lo^2
  // 10^x = 2^(mid + hi) * 10^lo
  //      ~ mh * (c0 + p * lo^2)
  //      = (mh * c0) + p * (mh * lo^2)
  return static_cast<float>(multiply_add(p, lo2 * rr.mh, c0 * rr.mh));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP10F_H
