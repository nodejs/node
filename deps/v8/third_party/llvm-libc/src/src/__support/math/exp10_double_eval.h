//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Double-precision implementation of exp10.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP10_DOUBLE_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP10_DOUBLE_EVAL_H

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

// log2(10) = 0x1.a934f0979a371p+1 + 0x1.7f2495fb7fa7bp-52
LIBC_INLINE_VAR constexpr double LOG2_10_HI = 0x1.a934f0979a371p+1;

// log(10) = 0x1.26bb1bbb55516p+1 - 0x1.f48ad494ea3e9p-53
LIBC_INLINE_VAR constexpr DoubleDouble LOG_10_DD = {-0x1.f48ad494ea3e9p-53,
                                                    0x1.26bb1bbb55516p+1};

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    defined(LIBC_MATH_HAS_SMALL_TABLES)
// Splitting -2^-3 * log10(2) ~ hi + lo with 36-bit-precision hi part so that
// kd * MLOG10_2_EXP2_HI is exact for |kd| < 2^14:
// > a = -2^-3 * log10(2);
// > hi = round(a, 36, RN); // -0x1.34413509f8p-5
// > lo = round(a - hi, D, RN); // 0x1.80433b83b532ap-47
LIBC_INLINE_VAR constexpr double MLOG10_2_EXP2_HI = -0x1.34413509f8p-5;
LIBC_INLINE_VAR constexpr double MLOG10_2_EXP2_MID = 0x1.80433b83b532ap-47;
#else
// Splitting -2^-7 * log10(2) ~ hi + lo with 35-bit-precision hi part so that
// kd * MLOG10_2_EXP2_HI is exact for |kd| < 2^18:
// > a = -2^-7 * log10(2);
// > hi = round(a, 35, RN); // -0x1.3441350ap-9
// > lo = round(a - hi, D, RN); // 0x1.0c0219dc1da99p-46
LIBC_INLINE_VAR constexpr double MLOG10_2_EXP2_HI = -0x1.3441350ap-9;
LIBC_INLINE_VAR constexpr double MLOG10_2_EXP2_MID = 0x1.0c0219dc1da99p-46;
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS && LIBC_MATH_HAS_SMALL_TABLES

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
// Splitting log(10) ~ hi + lo with 12-bit-precision hi part for exact non-FMA
// multiplication without Veltkamp splitting:
// > hi = round(log(10), 12, RN);
// > lo = round(log(10) - hi, D, RN);
LIBC_INLINE_VAR constexpr double LOG_10_HI_12 = 0x1.26cp+1;
LIBC_INLINE_VAR constexpr double LOG_10_LO_12 =
    (LOG_10_DD.hi - LOG_10_HI_12) + LOG_10_DD.lo;

// Fast-path Ziv error bound for exp10:
LIBC_INLINE_VAR constexpr double EXP10_ERR_D = 0x1.8p-64;

// Exact powers of 10 in double precision: 10^1, 10^2, ..., 10^22.
LIBC_INLINE_VAR constexpr double EXP10_EXACT[22] = {
    0x1.4p+3,              // 10^1
    0x1.9p+6,              // 10^2
    0x1.f4p+9,             // 10^3
    0x1.388p+13,           // 10^4
    0x1.86ap+16,           // 10^5
    0x1.e848p+19,          // 10^6
    0x1.312dp+23,          // 10^7
    0x1.7d784p+26,         // 10^8
    0x1.dcd65p+29,         // 10^9
    0x1.2a05f2p+33,        // 10^10
    0x1.74876e8p+36,       // 10^11
    0x1.d1a94a2p+39,       // 10^12
    0x1.2309ce54p+43,      // 10^13
    0x1.6bcc41e9p+46,      // 10^14
    0x1.c6bf52634p+49,     // 10^15
    0x1.1c37937e08p+53,    // 10^16
    0x1.6345785d8ap+56,    // 10^17
    0x1.bc16d674ec8p+59,   // 10^18
    0x1.158e460913d00p+63, // 10^19
    0x1.5af1d78b58c40p+66, // 10^20
    0x1.b1ae4d6e2ef50p+69, // 10^21
    0x1.0f0cf064dd592p+73, // 10^22
};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

LIBC_ALWAYS_INLINE double exp10(double x) {
  using FPBits = fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();
  uint64_t x_abs = xbits.abs().uintval();

  // Bound constants (with biased exponent and mantissa bits):
  // - Tiny bound: 10^x ~ 1 when |x| <= 2^-54 / log(10)
  //   > round(2^-54 / log(10), D, RD) = 0x1.bcb7b1526e50ep-56
  //   > round(2^-54 / log(10), D, RU) = 0x1.bcb7b1526e50fp-56
  constexpr uint64_t TINY_BOUND = 0x3c7b'cb7b'1526'e50fULL;
  // - Overflow bound: 10^x overflows when x >= log10(2^1024)
  //   > round(log10(2^1024), D, RU) = 0x1.34413509f79ffp+8
  constexpr uint64_t OVERFLOW_BOUND = 0x4073'4413'509f'79ffULL;
  // -0x1.34413509f79ffp+8:
  constexpr uint64_t NEG_OVERFLOW_BOUND = 0xc073'4413'509f'79ffULL;
  // - Underflow bound: 10^x underflows to 0 when x <= log10(2^-1075)
  //   > round(log10(2^-1075), D, RD) = -0x1.439b746e36b53p+8
  constexpr uint64_t UNDERFLOW_BOUND = 0xc074'39b7'46e3'6b53ULL;

  // Normal non-tiny range: TINY_BOUND <= |x| < OVERFLOW_BOUND.
  if (LIBC_UNLIKELY(x_abs - TINY_BOUND >= OVERFLOW_BOUND - TINY_BOUND)) {
    // Exclude non-underflowing negative inputs in
    // (UNDERFLOW_BOUND, NEG_OVERFLOW_BOUND].
    if (LIBC_LIKELY(x_u < NEG_OVERFLOW_BOUND || x_u >= UNDERFLOW_BOUND))
      return set_exceptional(x);
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  // Exact outputs when x = 1, 2, ..., 22 (must not raise FE_INEXACT).
  // 0x3ff0 = high 16 bits of 0x1.0p+0 (1.0)
  // 0x4036 = high 16 bits of 0x1.6p+4 (22.0)
  if (LIBC_UNLIKELY((x_u << 16) == 0ULL)) {
    uint32_t d = static_cast<uint32_t>(x_u >> 48) - 0x3ff0U;
    if (d <= 0x46U) {
      uint32_t scaled = (16U | (d & 0xfU)) << (d >> 4);
      if ((scaled & 0xfU) == 0U)
        return EXP10_EXACT[(scaled >> 4) - 1];
    }
  }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // Now -0x1.439b746e36b53p+8 < x < -0x1.bcb7b1526e50ep-56 or
  //     0x1.bcb7b1526e50ep-56 < x < 0x1.34413509f79ffp+8.

  // Range reduction:
  // Let x = k * (log10(2) / 2^N) + dx
  // in which:
  //   k   = round(x * 2^N * log2(10))
  //   hi  = k >> N
  //   idx = k & (2^N - 1)
  // then:
  //   10^x = 2^hi * 2^(idx / 2^N) * exp(dx * log(10)).
  //
  // Using ROUND_BIAS, ROUND_SHIFT, and ROUND_MASK (see exp_utils.h), we compute
  // k_biased = fma(x, LOG2_10_SCALE, ROUND_BIAS) and extract in parallel:
  //   kd  = (k_biased & ROUND_MASK) - ROUND_SHIFT
  //   hi  = (int32_t)(k_bits >> 19) >> (13 + N)
  //   idx = (k_bits >> 32) & (2^N - 1).
  constexpr double LOG2_10_SCALE = EXP_SCALE * LOG2_10_HI;

  double k_biased = fputil::multiply_add(x, LOG2_10_SCALE, ROUND_BIAS);
  double kd = fputil::fp_and(k_biased, ROUND_MASK) - ROUND_SHIFT;
  uint64_t k_bits = cpp::bit_cast<uint64_t>(k_biased);

  int hi = static_cast<int32_t>(k_bits >> 19) >> (13 + EXP_N_BITS);
  unsigned idx = static_cast<unsigned>(k_bits >> 32) & EXP_MASK;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double t1 = fputil::multiply_add(kd, MLOG10_2_EXP2_HI, x); // exact
  double dx = fputil::multiply_add(kd, MLOG10_2_EXP2_MID, t1);
  double u = dx * LOG_10_DD.hi;
  return exp2_eval(x, u, hi, idx);
#else // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // With FMA, we use the full 53-bit:
  //   MLOG10_2_HI_53 = -2^-7 * log10(2) = -0x1.34413509f79ffp-9 (lsb = 2^-61)
  // and pre-multiplied remainder:
  //   MLOG10_2_MID_53_LOG10 = (-2^-7 * log10(2) - MLOG10_2_HI_53) * log(10)
  //                         = 0x1.dc5b0a78f8364p-65
  // to compute:
  //   u = (x - k * 2^-7 * log10(2)) * log(10) ~ t1 * log(10) + corr
  // where:
  //   t1   = fma(kd, MLOG10_2_HI_53, x)   (exact for k != 0)
  //   corr = kd * MLOG10_2_MID_53_LOG10   (|corr| < 2^18 * 2^-64 = 2^-46).
  //
  // Writing:
  //   exp(u.hi + d) = exp(u.hi) * exp(d)
  //                 ~ (1 + u.hi + u.hi^2 * P(u.hi)) * (1 + d),
  // we fold the linear cross-term u.hi * corr into u.lo.
  constexpr double MLOG10_2_HI_53 = -0x1.34413509f79ffp-2 * EXP_SCALE_INV;
  constexpr double MLOG10_2_MID_53_LOG10 =
      0x1.dc5b0a78f8364p-58 * EXP_SCALE_INV;

  double t1 = fputil::multiply_add(kd, MLOG10_2_HI_53, x); // exact
  double corr = kd * MLOG10_2_MID_53_LOG10;
  DoubleDouble u = fputil::exact_mult(t1, LOG_10_DD.hi);
  double lo1 = fputil::multiply_add(t1, LOG_10_DD.lo, corr);
  double lo2 = fputil::multiply_add(u.hi, corr, lo1);
  u.lo += lo2;
  return exp2_eval(x, u, hi, idx, EXP10_ERR_D, &LOG2_10_192);
#else  // !LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  // Without FMA, we use a 35-bit-precision high part MLOG10_2_EXP2_HI (with
  // lsb = 2^-43) so that:
  //   dx1 = x + kd * MLOG10_2_EXP2_HI
  // is exact for |kd| < 2^18.
  //
  // We then split dx1 at 2^-18 before multiplying by the 12-bit LOG_10_HI_12
  // (with lsb = 2^-10):
  //   dx_hi = (dx1 + SPLIT_18) - SPLIT_18   (exact, lsb = 2^-18)
  //   dx_lo = (dx1 - dx_hi) + dx2           (|dx_lo| < 2^-18)
  //   u_hi  = dx_hi * LOG_10_HI_12          (exact, lsb = 2^-28 >= 2^-30)
  //   u_lo  = dx_lo * LOG_10_HI_12 + dx_sum * LOG_10_LO_12.
  double dx1 = fputil::multiply_add(kd, MLOG10_2_EXP2_HI, x); // exact
  double dx2 = kd * MLOG10_2_EXP2_MID;
  double dx_sum = dx1 + dx2;
  double dx_hi = (dx1 + SPLIT_18) - SPLIT_18;
  double dx_lo = (dx1 - dx_hi) + dx2;
  double u_hi = dx_hi * LOG_10_HI_12; // exact, lsb = 2^-28 >= 2^-30
  double u_lo = dx_lo * LOG_10_HI_12 + dx_sum * LOG_10_LO_12;
  return exp2_eval(x, {u_lo, u_hi}, hi, idx, EXP10_ERR_D, &LOG2_10_192);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace double_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP10_DOUBLE_EVAL_H
