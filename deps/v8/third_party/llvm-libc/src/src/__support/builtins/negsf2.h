//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __negsf2 implementation as builtins::negsf2
/// so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_NEGSF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_NEGSF2_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Single-precision negation
// Mirrors compiler-rt's __negsf2.
LIBC_INLINE float negsf2(float x) {
  using FPBits = fputil::FPBits<float>;

  FPBits bits(x);
  bits.set_sign(bits.sign().negate());
  return bits.get_val();
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_NEGSF2_H
