//===-- Implementation header for fmaximum_mag_numl -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_MAG_NUML_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_MAG_NUML_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr long double fmaximum_mag_numl(long double x,
                                                    long double y) {
  return fputil::fmaximum_mag_num(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FMAXIMUM_MAG_NUML_H
