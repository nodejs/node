//===-- Implementation header for bf16fmaf128 -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_BF16FMAF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_BF16FMAF128_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "src/__support/FPUtil/FMA.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE bfloat16 bf16fmaf128(float128 x, float128 y, float128 z) {
  return fputil::fma<bfloat16>(x, y, z);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_BF16FMAF128_H
