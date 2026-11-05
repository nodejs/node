//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Double-precision implementation of exp.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP_DOUBLE_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP_DOUBLE_EVAL_H

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

LIBC_ALWAYS_INLINE double exp(double x) {
  using FPBits = fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();
  uint64_t x_abs = xbits.abs().uintval();

  // Bound constants (with biased exponent and mantissa bits):
  // - Tiny bound: 2^-53 = 0x1.0p-53 (for |x| < 2^-53, exp(x) rounds to 1.0 + x)
  constexpr uint64_t TINY_BOUND = 0x3ca0'0000'0000'0000ULL;
  // - Overflow bound:
  //   > round(log(2^1023 * (2 - 2^-52)), D, RU) = 0x1.62e42fefa39f0p+9
  //   > round(log(2^1023 * (2 - 2^-52)), D, RD) = 0x1.62e42fefa39efp+9
  //   > round(log(2^1023 * (2 - 2^-52)), D, RN) = 0x1.62e42fefa39efp+9
  //   > round(exp(0x1.62e42fefa39f0p+9), D, RN) = +inf
  constexpr uint64_t OVERFLOW_BOUND = 0x4086'2e42'fefa'39f0ULL;
  // -0x1.62e42fefa39f0p+9:
  constexpr uint64_t NEG_OVERFLOW_BOUND = 0xc086'2e42'fefa'39f0ULL;
  // - Underflow bound: min denormal number / 2 = 2^-1075
  //   > round(log(2^-1075), D, RN) = -0x1.74910d52d3052p+9
  constexpr uint64_t UNDERFLOW_BOUND = 0xc087'4910'd52d'3052ULL;

  // Normal range: 2^-53 <= |x| < 0x1.62e42fefa39f0p+9.
  if (LIBC_UNLIKELY(x_abs - TINY_BOUND >= OVERFLOW_BOUND - TINY_BOUND)) {
    // Exclude non-underflowing negative inputs in
    // (UNDERFLOW_BOUND, NEG_OVERFLOW_BOUND].
    if (LIBC_LIKELY(x_u < NEG_OVERFLOW_BOUND || x_u >= UNDERFLOW_BOUND))
      return set_exceptional(x);
  }

  // Now -0x1.74910d52d3052p+9 < x <= -0x1.0p-53 or
  //      0x1.0p-53 <= x < 0x1.62e42fefa39f0p+9.

  // Range reduction:
  // Let x = k * (log(2) / 2^N) + u
  // in which:
  //   k   = round(x * 2^N * log2(e))
  //   hi  = k >> N
  //   idx = k & (2^N - 1)
  // then:
  //   exp(x) = 2^hi * 2^(idx / 2^N) * exp(u).
  //
  // Using ROUND_BIAS, ROUND_SHIFT, and ROUND_MASK (see exp_utils.h), we compute
  // k_biased = fma(x, LOG2_E_SCALE, ROUND_BIAS) and extract in parallel:
  //   kd  = (k_biased & ROUND_MASK) - ROUND_SHIFT
  //   hi  = (int32_t)(k_bits >> 19) >> (13 + N)
  //   idx = (k_bits >> 32) & (2^N - 1).
  constexpr double LOG2_E_SCALE = EXP_SCALE * LOG2_E.hi;

  double k_biased = fputil::multiply_add(x, LOG2_E_SCALE, ROUND_BIAS);
  double kd = fputil::fp_and(k_biased, ROUND_MASK) - ROUND_SHIFT;
  uint64_t k_bits = cpp::bit_cast<uint64_t>(k_biased);

  int hi = static_cast<int32_t>(k_bits >> 19) >> (13 + EXP_N_BITS);
  unsigned idx = static_cast<unsigned>(k_bits >> 32) & EXP_MASK;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double t1 = fputil::multiply_add(kd, MLOG_2_EXP2_HI, x); // exact
  double u = fputil::multiply_add(kd, MLOG_2_EXP2_MID, t1);
  return exp2_eval(x, u, hi, idx);
#else // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // With FMA, we use the full 53-bit:
  //   MLOG_2_HI_53 = -2^-7 * log(2) = -LOG_2.hi * EXP_SCALE_INV  (lsb = 2^-60)
  // to compute:
  //   u = x - k * 2^-7 * log(2) ~ t1 + d
  // where:
  //   t1 = fma(kd, MLOG_2_HI_53, x)
  //   d  = kd * MLOG_2_LO_53         (|d| < 2^18 * 2^-62 = 2^-44).
  //
  // For k != 0, |x| >= 2^-8 * log(2) > 2^-9 so lsb(x) >= 2^-61, and since kd is
  // an integer, lsb(kd * MLOG_2_HI_53) >= 2^-60. Because |t1| < 2^-8, every
  // non-zero bit of x + kd * MLOG_2_HI_53 lies in [2^-61, 2^-9] (at most 53
  // bits), so:
  //   t1 = fma(kd, MLOG_2_HI_53, x)
  // is exact.
  //
  // In exp2_eval, P(u.hi) is evaluated at u.hi = t1 alone. Writing:
  //   exp(t1 + d) = exp(t1) * exp(d)
  //               ~ (1 + t1 + t1^2 * P(t1)) * (1 + d)
  //               ~ 1 + t1 + d * (1 + t1) + t1^2 * P(t1),
  // we fold the linear cross-term t1 * d into:
  //   u.lo = fma(t1, d, d),
  // bounding the omitted term:
  //   |t1^2 * P(t1) * d| < 2^-63.
  constexpr double MLOG_2_HI_53 = -LOG_2.hi * EXP_SCALE_INV;
  constexpr double MLOG_2_LO_53 = -LOG_2.lo * EXP_SCALE_INV;

  double t1 = fputil::multiply_add(kd, MLOG_2_HI_53, x); // exact
  double d = kd * MLOG_2_LO_53;
  double u_lo = fputil::multiply_add(t1, d, d);
  DoubleDouble u = {u_lo, t1};
#else  // !LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // Without FMA, we use a 35-bit-precision high part MLOG_2_EXP2_HI (with lsb =
  // 2^-42) so that:
  //   kd * MLOG_2_EXP2_HI
  // has at most 18 + 35 = 53 significant bits and is exact in double precision
  // for |kd| < 2^18, and:
  //   t1 = x + kd * MLOG_2_EXP2_HI
  // is exact by Sterbenz's lemma.
  //
  // We then split t1 at 2^-30 before adding t2 = kd * MLOG_2_EXP2_MID:
  //   u_hi  = (t1 + SPLIT_30) - SPLIT_30   (exact, lsb = 2^-30)
  //   t1_lo = t1 - u_hi                    (exact, |t1_lo| <= 2^-31)
  //   u_lo  = t1_lo + t2                   (|u_lo| < 2^-26).
  // This gives lsb(u_hi) >= 2^-30 so that m_hi * u_hi is exact in exp2_eval,
  // while keeping |u_lo| < 2^-26 so the rounding error of:
  //   u_lo = t1_lo + t2
  // is bounded by 2^(-26 - 53) = 2^-79.
  double t1 = fputil::multiply_add(kd, MLOG_2_EXP2_HI, x); // exact
  double t2 = kd * MLOG_2_EXP2_MID;
  double u_hi = (t1 + SPLIT_30) - SPLIT_30; // exact, lsb = 2^-30
  double t1_lo = t1 - u_hi;                 // exact
  double u_lo = t1_lo + t2;
  DoubleDouble u = {u_lo, u_hi};
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  return exp2_eval(x, u, hi, idx, EXP_ERR_D, &LOG2_E_192);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace double_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP_DOUBLE_EVAL_H
