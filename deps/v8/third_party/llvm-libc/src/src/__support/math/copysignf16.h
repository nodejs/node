//===-- Implementation header for copysignf16 -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "src/__support/FPUtil/ManipulationFunctions.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR float16 copysignf16(float16 x, float16 y) {
#if defined(__LIBC_MISC_MATH_BASIC_OPS_OPT) && !defined(LIBC_USE_CONSTEXPR)
  return __builtin_copysignf16(x, y);
#else
  return fputil::copysign(x, y);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF16_H
