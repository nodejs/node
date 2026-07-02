//===-- Implementation header for roundevenf16 ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDEVENF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDEVENF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE LIBC_CONSTEXPR float16 roundevenf16(float16 x) {
#if defined(__LIBC_USE_BUILTIN_ROUNDEVEN) &&                                   \
    defined(LIBC_TARGET_CPU_HAS_FAST_FLOAT16_OPS) &&                           \
    !defined(LIBC_USE_CONSTEXPR)
  return fputil::cast<float16>(__builtin_roundevenf(x));
#else
  return fputil::round_using_specific_rounding_mode(x, FP_INT_TONEAREST);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDEVENF16_H
