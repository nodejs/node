//===-- Implementation header for roundf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDF_H

#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

// For the following targets, clang will generate rounding instructions
// by default:
//   - x86-64 with sse4.1 or after
//   - ARM v8
//   - RISC-V

// Notes: gcc does not generate the instructions for x86-64 by default.

// Notes: for x86-64, if `-ffp-model=strict` is set, `__builtin_round` will
// generate a callback to `round` and it does not look like there is any way for
// us to detect that just from the pre-defined macros.  The only way to really
// detect the call back is to compile and link with nostdlib.
// This also affects `__builtin_elementwise_round`, making it behave identical
// to `__builtin_round`.

// Notes: `__builtin_elementwise_round` is slightly better than
// `__builtin_round` in that it is not defined for x86-64 pre-SSE4.1, but it
// still generate callback for ARM version < 8, and for x86-64 with
// `-ffp-model=strict`.

// Notes: `__builtin_roundf` expansion for x86-64 using SSE4.1 rounding
// instruction by clang is only correct for the default rounding mode.
// See https://github.com/llvm/llvm-project/issues/140252
// So we will only use `__builtin_round` with clang on x86-64 if we assume
// default rounding mode (FE_TONEAREST) only.

LIBC_INLINE LIBC_CONSTEXPR float roundf(float x) {
#if __has_builtin(__builtin_roundf) && !defined(LIBC_USE_CONSTEXPR) &&         \
    (defined(__LIBC_USE_BUILTIN_ROUND) ||                                      \
     (defined(LIBC_COMPILER_IS_CLANG) &&                                       \
      defined(LIBC_TARGET_CPU_HAS_FPU_FLOAT) &&                                \
      (!defined(__ARM_ARCH) || (__ARM_ARCH >= 8)) &&                           \
      (!defined(LIBC_TARGET_ARCH_IS_X86) ||                                    \
       defined(LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY))))
  return __builtin_roundf(x);
#else
  return fputil::round(x);
#endif
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ROUNDF_H
