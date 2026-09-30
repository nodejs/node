//===-- Implementation header for sqrtl -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SQRTL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SQRTL_H

#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"

#ifndef LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR long double sqrtl(long double x) {
  return fputil::sqrt<long double>(x);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SQRTL_H
