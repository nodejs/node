//===-- Implementation header for bf16addl ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_BF16ADDL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_BF16ADDL_H

#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/FPUtil/generic/add_sub.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE bfloat16 bf16addl(long double x, long double y) {
  return fputil::generic::add<bfloat16>(x, y);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_BF16ADDL_H
