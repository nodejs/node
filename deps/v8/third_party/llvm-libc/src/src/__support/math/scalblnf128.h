//===-- Implementation header for scalblnf128 -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SCALBLNF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SCALBLNF128_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "hdr/float_macros.h"
#include "src/__support/FPUtil/ManipulationFunctions.h"
#include "src/__support/macros/config.h"

#if FLT_RADIX != 2
#error "FLT_RADIX != 2 is not supported."
#endif

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr float128 scalblnf128(float128 x, long n) {
  return fputil::ldexp(x, n);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SCALBLNF128_H
