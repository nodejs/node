//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __fixsfdi implementation as
/// builtins::fixsfdi so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXSFDI_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXSFDI_H

#include "hdr/stdint_proxy.h"
#include "src/__support/builtins/fixint_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncating float -> int64_t conversion, saturating on overflow.
// Mirrors compiler-rt's __fixsfdi.
LIBC_INLINE int64_t fixsfdi(float x) { return fixint<int64_t>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXSFDI_H
