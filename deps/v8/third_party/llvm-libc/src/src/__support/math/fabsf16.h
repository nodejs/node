//===-- Implementation header for fabsf16 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_FABSF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_FABSF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr float16 fabsf16(float16 x) {
  if (cpp::is_constant_evaluated())
    return fputil::abs(x);
  // For x86, GCC generates better code from the generic implementation.
  // https://godbolt.org/z/K9orM4hTa
#if defined(__LIBC_MISC_MATH_BASIC_OPS_OPT) &&                                 \
    !(defined(LIBC_TARGET_ARCH_IS_X86) && defined(LIBC_COMPILER_IS_GCC))
  return __builtin_fabsf16(x);
#else
  return fputil::abs(x);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_FABSF16_H
