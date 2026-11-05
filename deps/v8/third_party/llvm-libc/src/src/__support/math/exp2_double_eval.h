//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Double-precision implementation of exp2.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP2_DOUBLE_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP2_DOUBLE_EVAL_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/fp_bitwise.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#include "src/__support/math/exp_double_utils.h"
#else
#include "src/__support/math/exp_utils.h"
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace double_eval {

LIBC_ALWAYS_INLINE double exp2(double x) {
  using FPBits = fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();
  uint64_t x_abs = xbits.abs().uintval();

  // Bound constants (with biased exponent and mantissa bits):
  // - Tiny bound: nextafter(2^-54, +inf) = 0x1.0000000000001p-54
  //   (since |x| <= 2^-54 is handled by 1.0 + x in set_exceptional)
  constexpr uint64_t TINY_BOUND = 0x3c90'0000'0000'0001ULL;
  // - Overflow bound: 1024.0
  constexpr uint64_t OVERFLOW_BOUND = 0x4090'0000'0000'0000ULL;
  // -1024.0
  constexpr uint64_t NEG_OVERFLOW_BOUND = 0xc090'0000'0000'0000ULL;
  // - Underflow bound: -1075.0
  constexpr uint64_t UNDERFLOW_BOUND = 0xc090'cc00'0000'0000ULL;

  // Normal range: 2^-54 <= |x| < 1024.
  if (LIBC_UNLIKELY(x_abs - TINY_BOUND >= OVERFLOW_BOUND - TINY_BOUND)) {
    // Exclude non-underflowing negative inputs in (-1075, -1024].
    if (LIBC_LIKELY(x_u < NEG_OVERFLOW_BOUND || x_u >= UNDERFLOW_BOUND))
      return set_exceptional(x);
  }

  // Now -1075 < x < -2^-54 or 2^-54 < x < 1024.

  // Range reduction:
  // Let x = k * 2^-N + dx
  // in which:
  //   k   = round(x * 2^N)
  //   hi  = k >> N
  //   idx = k & (2^N - 1)
  // then:
  //   2^x = 2^hi * 2^(idx / 2^N) * 2^dx.
  //
  // Using ROUND_BIAS, ROUND_SHIFT, and ROUND_MASK (see exp_utils.h), we compute
  // k_biased = fma(x, EXP_SCALE, ROUND_BIAS) and extract in parallel:
  //   kd  = (k_biased & ROUND_MASK) - ROUND_SHIFT
  //   hi  = (int32_t)(k_bits >> 19) >> (13 + N)
  //   idx = (k_bits >> 32) & (2^N - 1).
  double k_biased = fputil::multiply_add(x, EXP_SCALE, ROUND_BIAS);
  double kd = fputil::fp_and(k_biased, ROUND_MASK) - ROUND_SHIFT;
  uint64_t k_bits = cpp::bit_cast<uint64_t>(k_biased);

  int hi = static_cast<int32_t>(k_bits >> 19) >> (13 + EXP_N_BITS);
  unsigned idx = static_cast<unsigned>(k_bits >> 32) & EXP_MASK;

  double dx = fputil::multiply_add(kd, -EXP_SCALE_INV, x); // exact

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double u = dx * LOG_2.hi;
  return exp2_eval(x, u, hi, idx, /*is_exp2=*/true);
#else // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  DoubleDouble u = fputil::exact_mult(dx, LOG_2.hi);
  u.lo = fputil::multiply_add(dx, LOG_2.lo, u.lo);
#else  // !LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // Without FMA, we use fixed-point splitting to avoid Veltkamp's splitting:
  // Let:
  //   m = 2^(idx / 2^7) = m_hi + m_lo
  //   u = dx * log(2)   = u_hi + u_lo
  //
  // In exp2_eval, we approximate 2^(idx / 2^7) * 2^dx = m * exp(u) by:
  //   m * (1 + u + u^2 * P(u)) ~ r_hi + r_lo
  // where:
  //   r_hi = m_hi + m_hi * u_hi
  //   r_lo = m * (u_lo + u^2 * P(u)) + m_lo * (1 + u_hi)
  //
  // Since r_hi is in [1 - 2^-8, 2), for r_hi to be exact in double precision,
  // we need:
  //   lsb(m_hi * u_hi) >= 2^-52.
  // We choose:
  //   lsb(m_hi) >= 2^-22  =>  |m_lo| < 2^-22
  //   lsb(u_hi) >= 2^-30
  // so that lsb(m_hi * u_hi) >= 2^-52, and the rounding errors from
  // m_lo * (1 + u_hi) are bounded by 2^(-22 - 52) = 2^-74.
  //
  // To get lsb(u_hi) >= 2^-30 from u = dx * log(2), since |dx| <= 2^-8 and
  // log(2) < 1, we split dx at 2^-18 and log(2) at 2^-12:
  //   dx     = dx_hi + dx_lo             (|dx_lo| <= 2^-19)
  //   log(2) = LOG_2_HI_12 + LOG_2_LO_12 (|LOG_2_LO_12| < 2^-13)
  // Then:
  //   u_hi = dx_hi * LOG_2_HI_12
  // is an exact multiple of 2^-30, and the two remainder products in:
  //   u_lo = dx_lo * LOG_2_HI_12 + dx * LOG_2_LO_12
  // are both bounded by 2^-19, keeping the rounding errors of u_lo below 2^-71.
  double dx_hi = (dx + SPLIT_18) - SPLIT_18; // lsb = 2^-18
  double dx_lo = dx - dx_hi;
  double u_hi = dx_hi * LOG_2_HI_12; // exact, lsb = 2^-30
  double t0 = dx_lo * LOG_2_HI_12;
  double t1 = dx * LOG_2_LO_12;
  double u_lo = t0 + t1;
  DoubleDouble u = {u_lo, u_hi};
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  return exp2_eval(x, u, hi, idx);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace double_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP2_DOUBLE_EVAL_H
