//===-- Implementation header for fmaximum_numf128 --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_NUMF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_NUMF128_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/float128.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_NAMESPACE::fputil::Float128;

LIBC_INLINE constexpr Float128 fmaximum_numf128(Float128 x, Float128 y) {
  return fputil::fmaximum_num(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_NUMF128_H
