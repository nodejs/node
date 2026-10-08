//===-- Implementation header for iscanonicall ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ISCANONICALL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ISCANONICALL_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr int iscanonicall(long double x) {
  long double temp{};
  return fputil::canonicalize(temp, x) == 0;
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ISCANONICALL_H
