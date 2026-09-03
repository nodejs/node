//===-- Implementation header for copysignbf16 ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNBF16_H

#include "src/__support/FPUtil/ManipulationFunctions.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr bfloat16 copysignbf16(bfloat16 x, bfloat16 y) {
  return fputil::copysign(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COPYSIGNBF16_H
