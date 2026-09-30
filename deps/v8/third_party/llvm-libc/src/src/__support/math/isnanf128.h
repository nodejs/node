//===-- Implementation header for isnanf128 ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF128_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/float128.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_NAMESPACE::fputil::Float128;

LIBC_INLINE LIBC_CONSTEXPR int isnanf128(Float128 x) {
  return fputil::FPBits<Float128>(x).is_nan();
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ISNANF128_H
