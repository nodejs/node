//===-- Implementation header for ceill -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_CEILL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_CEILL_H

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

// TODO(issue#185232): Mark as constexpr once the refactor is done.
LIBC_INLINE long double ceill(long double x) { return fputil::ceil(x); }

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_CEILL_H
