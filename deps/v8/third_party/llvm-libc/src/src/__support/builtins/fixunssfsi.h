//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __fixunssfsi implementation as
/// builtins::fixunssfsi so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFSI_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFSI_H

#include "hdr/stdint_proxy.h"
#include "src/__support/builtins/fixint_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncating float -> uint32_t conversion, saturating on overflow.
// Mirrors compiler-rt's __fixunssfsi.
LIBC_INLINE uint32_t fixunssfsi(float x) { return fixuint<uint32_t>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFSI_H
