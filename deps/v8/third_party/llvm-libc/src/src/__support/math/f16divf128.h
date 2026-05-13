//===-- Implementation header for f16divf128 --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_F16DIVF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_F16DIVF128_H

#include "include/llvm-libc-macros/float16-macros.h"
#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT16
#ifdef LIBC_TYPES_HAS_FLOAT128

#include "src/__support/FPUtil/generic/div.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float16 f16divf128(float128 x, float128 y) {
  return fputil::generic::div<float16>(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128
#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_F16DIVF128_H
