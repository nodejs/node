//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __truncdfsf2 implementation as
/// builtins::truncdfsf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCDFSF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCDFSF2_H

#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncate double to float; mirrors compiler-rt's __truncdfsf2.
LIBC_INLINE float truncdfsf2(double x) { return fpconvert<float>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCDFSF2_H
