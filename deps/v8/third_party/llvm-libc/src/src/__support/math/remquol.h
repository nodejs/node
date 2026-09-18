//===-- Implementation header for remquol -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_REMQUOL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_REMQUOL_H

#include "src/__support/FPUtil/DivisionAndRemainderOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr long double remquol(long double x, long double y,
                                          int *exp) {
  return fputil::remquo(x, y, *exp);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_REMQUOL_H
