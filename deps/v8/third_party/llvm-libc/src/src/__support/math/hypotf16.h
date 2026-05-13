//===-- Implementation header for hypotf16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF16_H

#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/common.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/types.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

// For targets where conversion from float to float16 has to be
// emulated, fputil::hypot<float16> is faster
LIBC_INLINE float16 hypotf16(float16 x, float16 y) {
  using FloatBits = fputil::FPBits<float>;
  using FPBits = fputil::FPBits<float16>;

  FPBits x_abs = FPBits(x).abs();
  FPBits y_abs = FPBits(y).abs();

  bool x_abs_larger = x_abs.uintval() >= y_abs.uintval();

  FPBits a_bits = x_abs_larger ? x_abs : y_abs;
  FPBits b_bits = x_abs_larger ? y_abs : x_abs;

  uint16_t a_u = a_bits.uintval();
  uint16_t b_u = b_bits.uintval();

  // Note: replacing `a_u >= FPBits::EXP_MASK` with `a_bits.is_inf_or_nan()`
  // generates extra exponent bit masking instructions on x86-64.
  if (LIBC_UNLIKELY(a_u >= FPBits::EXP_MASK)) {
    // x or y is inf or nan
    if (a_bits.is_signaling_nan() || b_bits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits::quiet_nan().get_val();
    }
    if (a_bits.is_inf() || b_bits.is_inf())
      return FPBits::inf().get_val();
    return a_bits.get_val();
  }

  float af = fputil::cast<float>(a_bits.get_val());
  float bf = fputil::cast<float>(b_bits.get_val());

  // Compiler runtime basic operations for float16 might not be correctly
  // rounded for all rounding modes.
  if (LIBC_UNLIKELY(a_u - b_u >=
                    static_cast<uint16_t>((FPBits::FRACTION_LEN + 2)
                                          << FPBits::FRACTION_LEN)))
    return fputil::cast<float16>(af + bf);

  // These squares are exact.
  float a_sq = af * af;
  float sum_sq = fputil::multiply_add(bf, bf, a_sq);

  FloatBits result(fputil::sqrt<float>(sum_sq));
  uint32_t r_u = result.uintval();

  // If any of the sticky bits of the result are non-zero, except the LSB, then
  // the rounded result is correct.
  if (LIBC_UNLIKELY(((r_u + 1) & 0x0000'0FFE) == 0)) {
    float r_d = result.get_val();

    // Perform rounding correction.
    float sum_sq_lo = fputil::multiply_add(bf, bf, a_sq - sum_sq);
    float err = sum_sq_lo - fputil::multiply_add(r_d, r_d, -sum_sq);

    if (err > 0) {
      r_u |= 1;
    } else if ((err < 0) && (r_u & 1) == 0) {
      r_u -= 1;
    } else if ((r_u & 0x0000'1FFF) == 0) {
      // The rounded result is exact.
      fputil::clear_except_if_required(FE_INEXACT);
    }
    return fputil::cast<float16>(FloatBits(r_u).get_val());
  }

  return fputil::cast<float16>(result.get_val());
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_HYPOTF16_H
