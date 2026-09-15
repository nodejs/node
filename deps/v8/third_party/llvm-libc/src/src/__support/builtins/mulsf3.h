//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __mulsf3 implementation as builtins::mulsf3
/// so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULSF3_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULSF3_H

#include "src/__support/FPUtil/generic/mul.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Multiplication at float precision; mirrors compiler-rt's __mulsf3.
LIBC_INLINE float mulsf3(float x, float y) {
  return fputil::generic::mul<float>(x, y);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULSF3_H
