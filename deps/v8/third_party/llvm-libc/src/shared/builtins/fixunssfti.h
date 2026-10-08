//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __fixunssfti implementation as
/// shared::fixunssfti so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_FIXUNSSFTI_H
#define LLVM_LIBC_SHARED_BUILTINS_FIXUNSSFTI_H

#include "src/__support/macros/properties/types.h"

#ifdef LIBC_TYPES_HAS_INT128

#include "shared/libc_common.h"
#include "src/__support/builtins/fixunssfti.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using builtins::fixunssfti;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_INT128

#endif // LLVM_LIBC_SHARED_BUILTINS_FIXUNSSFTI_H
