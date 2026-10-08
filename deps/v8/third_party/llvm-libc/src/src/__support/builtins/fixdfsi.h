//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __fixdfsi implementation as
/// builtins::fixdfsi so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXDFSI_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXDFSI_H

#include "hdr/stdint_proxy.h"
#include "src/__support/builtins/fixint_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncating double -> int32_t conversion, saturating on overflow.
// Mirrors compiler-rt's __fixdfsi.
LIBC_INLINE int32_t fixdfsi(double x) { return fixint<int32_t>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXDFSI_H
