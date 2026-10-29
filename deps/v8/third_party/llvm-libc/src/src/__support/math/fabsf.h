//===-- Implementation header for fabsf -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FABSF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FABSF_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr float fabsf(float x) {
#ifdef __LIBC_MISC_MATH_BASIC_OPS_OPT
  return __builtin_fabsf(x);
#else
  return fputil::abs(x);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FABSF_H
