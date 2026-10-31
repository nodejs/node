//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Integer-only implementation of exp2f.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_INTEGER_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_INTEGER_EVAL_H

#include "hdr/fenv_macros.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/types.h"
#include "src/__support/math/exp2f_integer_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace static_rounding {

LIBC_ALWAYS_INLINE uint32_t exp2f_bits(uint32_t x_u,
                                       [[maybe_unused]] int rounding) {
  using FPBits = fputil::FPBits<float>;

  bool is_neg = (x_u >> 31) != 0;
  uint32_t x_abs = x_u & 0x7fff'ffffU;
  uint32_t x_e = x_abs >> FPBits::FRACTION_LEN;

  // When |x| < 2^-24 (x_e <= 102) or |x| >= 128 (x_e >= 134):
  if (LIBC_UNLIKELY(x_e - 103U >= 31U)) {
    // |x| < 2^-24
    if (x_e <= 102U) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      if (LIBC_UNLIKELY(rounding != FE_TONEAREST)) {
        if (x_abs == 0)
          return 0x3f80'0000U; // 1.0f

        if (rounding == FE_UPWARD && !is_neg)
          return 0x3f80'0001U; // 0x1.000002p+0f = 1.0f + 2^-23f

        if ((rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO) && is_neg)
          return 0x3f7f'ffffU; // 0x1.fffffep-1f = 1.0f - 2^-24f

        return 0x3f80'0000U; // 1.0f
      }
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      // For FE_TONEAREST, 2^x rounds to 1.0f (0x3f80'0000U) or
      // 1.0f - 2^-24f (0x3f7f'ffffU) when
      // x < log2(1 - 2^-25) ~ -0x1.715476p-25f (0xb338'aa3bU).
      return 0x3f80'0000U - static_cast<uint32_t>(x_u > 0xb338'aa3bU);
    }

    // exp2(nan) = nan
    if (x_abs > 0x7f80'0000U)    // |x| > +inf (x is NaN)
      return x_u | 0x0040'0000U; // silence signaling NaN

    // x >= 128 or +inf:
    // exp2(x) = +inf     for FE_TONEAREST, FE_UPWARD
    //         = FLT_MAX  for FE_TOWARDZERO, FE_DOWNWARD
    if (!is_neg) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      if ((rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO) &&
          x_u < 0x7f80'0000U) // x < +inf
        return 0x7f7f'ffffU;  // 0x1.fffffep+127f (FLT_MAX)
#endif                        // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      return 0x7f80'0000U;    // +inf
    }

    // x <= -150.0f (0xc316'0000U) or -inf:
    // exp2(x) = 2^-149   for FE_UPWARD
    //         = +0.0f    otherwise
    if (x_u >= 0xc316'0000U) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      if (rounding == FE_UPWARD && x_u < 0xff80'0000U) // x > -inf
        return 0x0000'0001U; // 0x1.0p-149f (FLT_TRUE_MIN)
#endif                       // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
      return 0x0000'0000U;   // +0.0f
    }
  }

  // Range reduction:
  //   k = floor(x)
  //   x = k + u, with u in [0, 1)
  //   2^x = 2^(k + u) = 2^k * 2^u
  //
  // For all remaining inputs, x_e is in [103, 134], so shifting the 24-bit
  // significand of |x| places the binary point of |x| at bit 54.
  // On 32-bit targets, s = 134 - x_e is in [0, 31], avoiding undefined 32-bit
  // shift amounts.
#if defined(LIBC_TYPES_HAS_INT128) && !defined(LIBC_MATH_HAS_SMALL_TABLES)
  uint64_t m = (x_u & 0x007f'ffffU) | 0x0080'0000U;
  uint64_t u_bits = m << (x_e - 96U);
#else  // !defined(LIBC_TYPES_HAS_INT128) || defined(LIBC_MATH_HAS_SMALL_TABLES)
  uint32_t m_hi = ((x_u & 0x007f'ffffU) | 0x0080'0000U) << 6;
  uint32_t s = 134U - x_e;
  uint32_t u_lo = (m_hi << 1) << (31U - s);
  uint32_t u_hi = m_hi >> s;
  uint64_t u_bits = (static_cast<uint64_t>(u_hi) << 32) | u_lo;
#endif // defined(LIBC_TYPES_HAS_INT128) && !defined(LIBC_MATH_HAS_SMALL_TABLES)

  return exp2f_eval_bits(u_bits, is_neg, rounding);
}

// Statically rounded, no-except implementation of exp2f using integer-only
// arithmetic.
LIBC_ALWAYS_INLINE float exp2f(float x, [[maybe_unused]] int rounding) {
  return cpp::bit_cast<float>(exp2f_bits(cpp::bit_cast<uint32_t>(x), rounding));
}

} // namespace static_rounding

namespace integer_eval {

LIBC_ALWAYS_INLINE float exp2f(float x) {
  return static_rounding::exp2f(x, FE_TONEAREST);
}

} // namespace integer_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP2F_INTEGER_EVAL_H
