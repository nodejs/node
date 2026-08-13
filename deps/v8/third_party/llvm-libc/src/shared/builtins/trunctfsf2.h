//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __trunctfsf2 implementation as
/// shared::trunctfsf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_TRUNCTFSF2_H
#define LLVM_LIBC_SHARED_BUILTINS_TRUNCTFSF2_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "shared/libc_common.h"
#include "src/__support/builtins/trunctfsf2.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using builtins::trunctfsf2;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SHARED_BUILTINS_TRUNCTFSF2_H
