//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for setpayloadf128.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADF128_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/float128.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

using LIBC_NAMESPACE::fputil::Float128;

LIBC_INLINE constexpr int setpayloadf128(Float128 *res, Float128 pl) {
  return static_cast<int>(fputil::setpayload</*IsSignaling=*/false>(*res, pl));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADF128_H
