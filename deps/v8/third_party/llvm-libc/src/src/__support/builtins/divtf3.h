//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __divtf3 implementation as builtins::divtf3
/// so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_DIVTF3_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_DIVTF3_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "src/__support/FPUtil/generic/div.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Division at float128 precision; mirrors compiler-rt's __divtf3.
LIBC_INLINE float128 divtf3(float128 x, float128 y) {
  return fputil::generic::div<float128>(x, y);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_DIVTF3_H
