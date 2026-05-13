//===-- Implementation header for copysignf ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF_H

#include "src/__support/FPUtil/ManipulationFunctions.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR float copysignf(float x, float y) {
#if defined(__LIBC_MISC_MATH_BASIC_OPS_OPT) &&                                 \
    !defined(LIBC_HAS_CONSTANT_EVALUATION)
  return __builtin_copysignf(x, y);
#else
  return fputil::copysign(x, y);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNF_H
