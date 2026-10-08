//===-- Implementation header for rintbf16 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_RINTBF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_RINTBF16_H

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/FPUtil/bfloat16.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr bfloat16 rintbf16(bfloat16 x) {
  return fputil::round_using_current_rounding_mode(x);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_RINTBF16_H
