//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the fminf80 function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FMINF80_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FMINF80_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/float80.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_NAMESPACE::fputil::Float80;

LIBC_INLINE constexpr Float80 fminf80(Float80 x, Float80 y) {
  return fputil::fmin(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FMINF80_H
