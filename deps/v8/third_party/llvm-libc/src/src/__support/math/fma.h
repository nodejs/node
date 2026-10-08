//===-- Implementation header for fma ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FMA_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FMA_H

#include "src/__support/FPUtil/FMA.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE double fma(double x, double y, double z) {
  return fputil::fma<double>(x, y, z);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FMA_H
