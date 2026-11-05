//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Double-precision implementation of cosh.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COSH_DOUBLE_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COSH_DOUBLE_EVAL_H

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/sinhcosh_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace double_eval {

// Bound constants (with biased exponent and mantissa bits):
// - Tiny bound: 2^-27 = 0x1.0p-27
LIBC_INLINE_VAR constexpr uint64_t COSH_TINY_BOUND = 0x3e40'0000'0000'0000ULL;
// - Overflow bound:
//   > round(acosh(2^1023 * (2 - 2^-52)), D, RU) = 0x1.633ce8fb9f87ep+9
//   > round(acosh(2^1023 * (2 - 2^-52)), D, RD) = 0x1.633ce8fb9f87dp+9
//   > round(cosh(0x1.633ce8fb9f87dp+9), D, RN)  = 0x1.ffffffffffd3bp+1023
//   > round(cosh(0x1.633ce8fb9f87ep+9), D, RN)  = +inf
LIBC_INLINE_VAR constexpr uint64_t COSH_OVERFLOW_BOUND =
    0x4086'33ce'8fb9'f87eULL;

// Handle tiny (|x| <= 2^-27), overflow (|x| >= 0x1.633ce8fb9f87ep+9), inf, and
// nan for cosh(x):
LIBC_NOINLINE LIBC_INLINE double cosh_exceptional(double x) {
  using FPBits = fputil::FPBits<double>;
  FPBits xbits(x);

  // Tiny and subnormal range: |x| <= 2^-27.
  // For 0 < |x| <= 2^-27:
  //   0 < cosh(x) - 1 ~ x^2 / 2 < 2^-55 < 0.5 * ulp(1.0),
  // so cosh(x) rounds to 1.0 + 2^-52 when rounding mode is FE_UPWARD and to
  // 1.0 otherwise, with FE_INEXACT raised for all x != 0.
  if (xbits.abs().uintval() <= COSH_TINY_BOUND) {
    if (LIBC_UNLIKELY(x == 0.0))
      return 1.0;
    return fputil::round_result_slightly_up(1.0);
  }

  if (xbits.is_nan())
    return x + 1.0;

  if (xbits.is_inf())
    return FPBits::inf().get_val();

  fputil::set_errno_if_required(ERANGE);
  fputil::raise_overflow_except_if_required<double>();

#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  int rounding = fputil::quick_get_round();
  if (LIBC_UNLIKELY(rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO))
    return FPBits::max_normal().get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY

  return FPBits::inf().get_val();
}

LIBC_ALWAYS_INLINE double cosh(double x) {
  using FPBits = fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_abs = xbits.abs().uintval();

  if (LIBC_UNLIKELY(x_abs <= COSH_TINY_BOUND || x_abs >= COSH_OVERFLOW_BOUND))
    return cosh_exceptional(x);

  return exp_pm_eval(xbits.abs().get_val(), /*is_sinh=*/false);
}

} // namespace double_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COSH_DOUBLE_EVAL_H
