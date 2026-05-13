//===--- Round floating point to nearest integer on x86-64 ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEAREST_INTEGER_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEAREST_INTEGER_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if !defined(LIBC_TARGET_ARCH_IS_X86_64)
#error "Invalid include"
#endif

#if !defined(__SSE4_2__)
#error "SSE4.2 instruction set is not supported"
#endif

#include <immintrin.h>

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE float nearest_integer(float x) {
  __m128 xmm = _mm_set_ss(x); // NOLINT
  __m128 ymm =
      _mm_round_ss(xmm, xmm, _MM_ROUND_NEAREST | _MM_FROUND_NO_EXC); // NOLINT
  return ymm[0];
}

LIBC_INLINE double nearest_integer(double x) {
  __m128d xmm = _mm_set_sd(x); // NOLINT
  __m128d ymm =
      _mm_round_sd(xmm, xmm, _MM_ROUND_NEAREST | _MM_FROUND_NO_EXC); // NOLINT
  return ymm[0];
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEAREST_INTEGER_H
