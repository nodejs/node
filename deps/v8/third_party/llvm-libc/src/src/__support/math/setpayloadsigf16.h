//===-- Implementation header for setpayloadsigf16 --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE int setpayloadsigf16(float16 *res, float16 pl) {
  return static_cast<int>(fputil::setpayload</*IsSignaling=*/true>(*res, pl));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SETPAYLOADSIGF16_H
