//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD exp10.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP10F_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP10F_H

#include "expf_utils.h"
#include "src/__support/CPP/simd.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE static cpp::simd<double, N> inline_exp10(cpp::simd<double, N> x) {
  constexpr cpp::simd<double, N> shift = 0x1.800000000ffc0p+46;
  constexpr cpp::simd<double, N> log2_10 = 0x1.a934f0979a371p+1;

  // Rounds n to be (x * log2(10)) to the nearest multiple of 1/64
  // While preparing z to be used as an index for the lookup in eval_exp
  cpp::simd<double, N> z = cpp::multiply_add(x, log2_10, shift);
  cpp::simd<double, N> n = z - shift;

  constexpr cpp::simd<double, N> log2_10_lo = 0x1.7f2495fb7fa6dp-53;
  constexpr cpp::simd<double, N> negative_ln2 = -0x1.62e42fefa39efp-1;

  // Using 10^x = e^(x * ln(10)) = e^(x * log2(10) * ln(2))
  // Reduces x into r as:
  // r = (x * log2(10) - n) * ln(2), with |r| <= ln2/128,
  // Computed as (n - x * log2(10)) * -ln(2) to avoid negating n.
  cpp::simd<double, N> r;
  r = cpp::multiply_add(-x, log2_10, n);
  r = cpp::multiply_add(-x, log2_10_lo, r);
  r = r * negative_ln2;

  return eval_exp(r, z);
}

template <size_t N>
LIBC_INLINE cpp::simd<float, N> exp10f(cpp::simd<float, N> x) {
  using FPBits = typename fputil::FPBits<float>;

  cpp::simd<bool, N> is_inf = x >= 0x1.344136p+5f;
  cpp::simd<bool, N> is_zero = x <= -0x1.693c6bp+5f;
  cpp::simd<bool, N> is_special = is_inf | is_zero;

  cpp::simd<float, N> special_res = is_inf ? FPBits::inf().get_val() : 0.0f;

  cpp::simd<double, N> x_d = cpp::simd_cast<double, float, N>(x);
  cpp::simd<double, N> y = inline_exp10(x_d);
  cpp::simd<float, N> ret = cpp::simd_cast<float, double, N>(y);

  return is_special ? special_res : ret;
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP10F_H
