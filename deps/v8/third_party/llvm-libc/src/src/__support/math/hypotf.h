//===-- Implementation header for hypotf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float hypotf(float x, float y) {
  using DoubleBits = fputil::FPBits<double>;
  using FPBits = fputil::FPBits<float>;
  using fputil::DoubleDouble;

  uint32_t x_a = FPBits(x).uintval() & 0x7fff'ffff;
  uint32_t y_a = FPBits(y).uintval() & 0x7fff'ffff;

  // Note: replacing `x_a >= FPBits::EXP_MASK` with `x_bits.is_inf_or_nan()`
  // generates extra exponent bit masking instructions on x86-64.
  if (LIBC_UNLIKELY(x_a >= FPBits::EXP_MASK || y_a >= FPBits::EXP_MASK)) {
    // x or y is inf or nan
    FPBits x_bits(x);
    FPBits y_bits(y);
    if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    if (x_bits.is_inf() || y_bits.is_inf())
      return FPBits::inf().get_val();
    return x + y;
  }

  double xd = static_cast<double>(x);
  double yd = static_cast<double>(y);

  // x^2 and y^2 are exact in double precision.
  double x_sq = xd * xd;

  double sum_sq;
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  sum_sq = fputil::multiply_add(yd, yd, x_sq);
#else
  double y_sq = yd * yd;
  sum_sq = x_sq + y_sq;
#endif

  // Take sqrt in double precision.
  DoubleBits result(fputil::sqrt<double>(sum_sq));
  double r = result.get_val();
  float r_f = static_cast<float>(r);

  // If any of the sticky bits of the result are non-zero, except the LSB, then
  // the rounded result is correct.
  uint64_t r_u = result.uintval();
  uint32_t r_u32 = static_cast<uint32_t>(r_u);

  if (LIBC_UNLIKELY(((r_u32 + 1) & 0x0FFF'FFFE) == 0)) {
    // Almost all the sticky bits of the results are non-zero, extra checks are
    // needed to make sure rounding is correct.

    // Perform a quick check to see if the result rounded to float is already
    // correct.  Majority of hard-to-round cases fall in this case.  If not, we
    // will need to perform more expensive computations to get the correct error
    // terms.
    double r_d = static_cast<double>(r_f);
    bool y_a_smaller = y_a < x_a;

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    // Compute the missing y_sq variable for FMA code path.
    double y_sq = yd * yd;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

    double a = y_a_smaller ? x_sq : y_sq;
    double b = y_a_smaller ? y_sq : x_sq;
    double e = b - fputil::multiply_add(r_d, r_d, -a);
    if (e == 0.0)
      return r_f;

    // Rounding correction is needed.
    // The errors come from two parts:
    // - rounding errors from sqrt(sum_sq) -> D(sum_sq)
    // - rounding errors from x_sq + y_sq -> sum_sq
    // We use FastTwoSum algorithm to compute those errors and then combine.
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    double sum_sq_lo = (b - (sum_sq - a));
    double err = sum_sq_lo - fputil::multiply_add(r, r, -sum_sq);
#else
    fputil::DoubleDouble r_sq = fputil::exact_mult(r, r);
    double sum_sq_lo = (b - (sum_sq - a));
    double err = (sum_sq - r_sq.hi) + (sum_sq_lo - r_sq.lo);
#endif

    if (err > 0) {
      r_u |= 1;
    } else if ((err < 0) && ((r_u32 & 0x0FFF'FFFF) == 0)) {
      r_u -= 1;
    }

    return static_cast<float>(DoubleBits(r_u).get_val());
  }

  return r_f;
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF_H
