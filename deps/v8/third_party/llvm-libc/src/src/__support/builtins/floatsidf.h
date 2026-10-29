//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __floatsidf implementation as
/// builtins::floatsidf so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATSIDF_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATSIDF_H

#include "hdr/stdint_proxy.h"
#include "src/__support/builtins/floatint_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// double <- int32_t conversion, round to nearest.
// Mirrors compiler-rt's __floatsidf.
LIBC_INLINE double floatsidf(int32_t x) { return floatint<double>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATSIDF_H
