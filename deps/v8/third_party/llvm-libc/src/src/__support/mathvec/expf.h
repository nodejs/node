//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD exp.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXPF_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXPF_H

#include "expf_utils.h"
#include "src/__support/CPP/simd.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE static cpp::simd<double, N> inline_exp(cpp::simd<double, N> x) {
  constexpr cpp::simd<double, N> shift = 0x1.800000000ffc0p+46;

  // inv_ln2 = round(1/log(2), D, RN);
  constexpr cpp::simd<double, N> inv_ln2 = 0x1.71547652b82fep+0;
  cpp::simd<double, N> z = cpp::multiply_add(x, inv_ln2, shift);
  cpp::simd<double, N> n = z - shift;

  // ln2_hi = round(log(2), D, RN);
  // ln2_lo = round(log(2) - ln2_hi, D, RN);
  constexpr cpp::simd<double, N> ln2_hi = 0x1.62e42fefa39efp-1;
  constexpr cpp::simd<double, N> ln2_lo = 0x1.abc9e3b39803fp-56;

  cpp::simd<double, N> r = x;
  r = cpp::multiply_add(-n, ln2_hi, r);
  r = cpp::multiply_add(-n, ln2_lo, r);

  return eval_exp(r, z);
}

template <size_t N>
LIBC_INLINE cpp::simd<float, N> expf(cpp::simd<float, N> x) {
  using FPBits = typename fputil::FPBits<float>;

  cpp::simd<bool, N> is_inf = x >= 0x1.62e43p+6f;
  cpp::simd<bool, N> is_zero = x <= -0x1.9fe36ap+6f;
  cpp::simd<bool, N> is_special = is_inf | is_zero;

  cpp::simd<float, N> special_res = is_inf ? FPBits::inf().get_val() : 0.0f;

  cpp::simd<double, N> x_d = cpp::simd_cast<double, float, N>(x);
  cpp::simd<double, N> y = inline_exp(x_d);
  cpp::simd<float, N> ret = cpp::simd_cast<float, double, N>(y);

#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  cpp::simd<bool, N> is_hard_to_round = (x == FPBits(0xc169'12cdU).get_val());
  ret = is_hard_to_round ? 0x1.fa6635bp-22f : ret;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  return is_special ? special_res : ret;
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXPF_H
