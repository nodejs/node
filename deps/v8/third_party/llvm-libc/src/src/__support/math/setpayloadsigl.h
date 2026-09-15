//===-- Implementation header for setpayloadsigl ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGL_H

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE int setpayloadsigl(long double *res, long double pl) {
  return static_cast<int>(fputil::setpayload</*IsSignaling=*/true>(*res, pl));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGL_H
