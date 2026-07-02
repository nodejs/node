//===-- Implementation header for isnanf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

#undef isnanf
LIBC_INLINE LIBC_CONSTEXPR int isnanf(float x) {
#if defined(__LIBC_USE_BUILTIN_ISNAN) && !defined(LIBC_USE_CONSTEXPR)
  return __builtin_isnan(x);
#else
  return fputil::FPBits<float>(x).is_nan();
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF_H
