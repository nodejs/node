//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __floatunsisf implementation as
/// shared::floatunsisf so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_FLOATUNSISF_H
#define LLVM_LIBC_SHARED_BUILTINS_FLOATUNSISF_H

#include "shared/libc_common.h"
#include "src/__support/builtins/floatunsisf.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using builtins::floatunsisf;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_BUILTINS_FLOATUNSISF_H
