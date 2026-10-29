//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __subsf3 implementation as builtins::subsf3
/// so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_SUBSF3_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_SUBSF3_H

#include "src/__support/FPUtil/generic/add_sub.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Subtraction at float precision; mirrors compiler-rt's __subsf3.
LIBC_INLINE float subsf3(float x, float y) {
  return fputil::generic::sub<float>(x, y);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_SUBSF3_H
