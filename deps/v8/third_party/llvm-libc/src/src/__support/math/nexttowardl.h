//===-- Implementation header for nexttowardl -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_NEXTTOWARDL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_NEXTTOWARDL_H

#include "src/__support/FPUtil/ManipulationFunctions.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE long double nexttowardl(long double x, long double y) {
  // We can reuse the nextafter implementation because the internal nextafter is
  // templated on the types of the arguments.
  return fputil::nextafter(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_NEXTTOWARDL_H
