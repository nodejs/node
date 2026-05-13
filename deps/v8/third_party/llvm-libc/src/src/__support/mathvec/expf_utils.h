//===-- Common utils for exp function ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP_UTILS_H

#include "src/__support/CPP/simd.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/mathvec/common_constants.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE static cpp::simd<double, N> exp_lookup(cpp::simd<uint64_t, N> u) {
  cpp::simd<uint64_t, N> index = u & cpp::simd<uint64_t, N>(0x3f);
  cpp::simd<uint64_t, N> mantissa =
      cpp::gather<cpp::simd<uint64_t, N>>(true, index, EXP_MANTISSA);
  cpp::simd<uint64_t, N> exponent = (u >> 6) << 52;
  cpp::simd<uint64_t, N> result = mantissa | exponent;
  return cpp::bit_cast<cpp::simd<double, N>>(result);
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_EXP_UTILS_H
