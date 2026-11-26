//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the ddivf128 function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_DDIVF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_DDIVF128_H

#include "src/__support/FPUtil/float128.h"
#include "src/__support/FPUtil/generic/div.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_NAMESPACE::fputil::Float128;

LIBC_INLINE constexpr double ddivf128(Float128 x, Float128 y) {
  return fputil::generic::div<double>(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_DDIVF128_H
