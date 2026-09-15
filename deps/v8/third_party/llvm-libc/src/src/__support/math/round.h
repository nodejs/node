//===-- Implementation header for round -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ROUND_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ROUND_H

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR double round(double x) {
#if defined(__LIBC_USE_BUILTIN_ROUND) && !defined(LIBC_USE_CONSTEXPR)
  return __builtin_round(x);
#else
  return fputil::round(x);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ROUND_H
