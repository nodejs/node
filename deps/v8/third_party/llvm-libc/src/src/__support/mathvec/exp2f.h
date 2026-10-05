//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD exp2.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP2F_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP2F_H

#include "expf_utils.h"
#include "src/__support/CPP/simd.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE static cpp::simd<double, N> inline_exp2(cpp::simd<double, N> x) {
  constexpr cpp::simd<double, N> shift = 0x1.800000000ffc0p+46;

  // Rounds n to be x to the nearest multiple of 1/64
  // While preparing z to be used as an index for the lookup in eval_exp
  cpp::simd<double, N> z = x + shift;
  cpp::simd<double, N> n = z - shift;

  // ln(2), with a bias of +768 FP64 ULP, which is enough to remove the
  // hard to round cases when casting back to FP32.
  constexpr cpp::simd<double, N> biased_ln2 = 0x1.62e42fefa3cefp-1;

  // Using 2^x = e^(x * ln2),
  // Reduces x into r as:
  // r = (x - n) * ln(2), with |r| <= ln2/128.
  // then computes 2^x = 2^n * exp(r).
  cpp::simd<double, N> r = (x - n) * biased_ln2;
  return eval_exp(r, z);
}

template <size_t N>
LIBC_INLINE cpp::simd<float, N> exp2f(cpp::simd<float, N> x) {
  using FPBits = typename fputil::FPBits<float>;

  cpp::simd<bool, N> is_inf = x >= 0x1p7;
  cpp::simd<bool, N> is_zero = x <= -0x1.2cp7f;
  cpp::simd<bool, N> is_special = is_inf | is_zero;

  cpp::simd<float, N> special_res = is_inf ? FPBits::inf().get_val() : 0.0f;

  cpp::simd<double, N> x_d = cpp::simd_cast<double, float, N>(x);
  cpp::simd<double, N> y = inline_exp2(x_d);
  cpp::simd<float, N> ret = cpp::simd_cast<float, double, N>(y);

  return is_special ? special_res : ret;
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP2F_H
