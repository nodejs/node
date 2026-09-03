//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __muldf3 implementation as builtins::muldf3
/// so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULDF3_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULDF3_H

#include "src/__support/FPUtil/generic/mul.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Multiplication at double precision; mirrors compiler-rt's __muldf3.
LIBC_INLINE double muldf3(double x, double y) {
  return fputil::generic::mul<double>(x, y);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_MULDF3_H
