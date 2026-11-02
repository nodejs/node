//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for getpayloadf128.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_GETPAYLOADF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_GETPAYLOADF128_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/float128.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

using LIBC_NAMESPACE::fputil::Float128;

LIBC_INLINE constexpr Float128 getpayloadf128(const Float128 *x) {
  return fputil::getpayload(*x);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_GETPAYLOADF128_H
