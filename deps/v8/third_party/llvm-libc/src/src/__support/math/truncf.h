//===-- Implementation header for truncf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TRUNCF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TRUNCF_H

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR float truncf(float x) {
#if defined(__LIBC_USE_BUILTIN_CEIL_FLOOR_RINT_TRUNC) &&                       \
    !defined(LIBC_HAS_CONSTANT_EVALUATION)
  return __builtin_truncf(x);
#else
  return fputil::trunc(x);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TRUNCF_H
