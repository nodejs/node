//===-- Implementation header for totalordermagbf16 -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TOTALORDERMAGBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TOTALORDERMAGBF16_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr int totalordermagbf16(const bfloat16 *x,
                                            const bfloat16 *y) {
  return static_cast<int>(fputil::totalordermag(*x, *y));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TOTALORDERMAGBF16_H
