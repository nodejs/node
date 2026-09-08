//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __fixunssfdi implementation as
/// builtins::fixunssfdi so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFDI_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFDI_H

#include "hdr/stdint_proxy.h"
#include "src/__support/builtins/fixint_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncating float -> uint64_t conversion, saturating on overflow.
// Mirrors compiler-rt's __fixunssfdi.
LIBC_INLINE uint64_t fixunssfdi(float x) { return fixuint<uint64_t>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXUNSSFDI_H
