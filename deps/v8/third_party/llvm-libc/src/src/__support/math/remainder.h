//===-- Implementation header for remainder ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_REMAINDER_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_REMAINDER_H

#include "src/__support/FPUtil/DivisionAndRemainderOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr double remainder(double x, double y) {
  int quotient{};
  return fputil::remquo(x, y, quotient);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_REMAINDER_H
