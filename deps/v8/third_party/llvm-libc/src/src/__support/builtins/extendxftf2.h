//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __extendxftf2 implementation as
/// builtins::extendxftf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDXFTF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDXFTF2_H

#include "include/llvm-libc-types/float128.h"
#include "src/__support/macros/properties/types.h"

#if defined(LIBC_TYPES_HAS_FLOAT128) &&                                        \
    defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)

#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Extend long double to float128; mirrors compiler-rt's __extendxftf2.
LIBC_INLINE float128 extendxftf2(long double x) {
  return fpconvert<float128>(x);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128 && LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDXFTF2_H
