//===-- Implementation header for fmabf16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FMABF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FMABF16_H

#include "src/__support/FPUtil/FMA.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE bfloat16 fmabf16(bfloat16 x, bfloat16 y, bfloat16 z) {
  return fputil::fma<bfloat16>(x, y, z);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FMABF16_H
