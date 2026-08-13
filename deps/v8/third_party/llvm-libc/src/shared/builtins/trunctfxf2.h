//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __trunctfxf2 implementation as
/// shared::trunctfxf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_TRUNCTFXF2_H
#define LLVM_LIBC_SHARED_BUILTINS_TRUNCTFXF2_H

#include "include/llvm-libc-types/float128.h"
#include "src/__support/macros/properties/types.h"

#if defined(LIBC_TYPES_HAS_FLOAT128) &&                                        \
    defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)

#include "shared/libc_common.h"
#include "src/__support/builtins/trunctfxf2.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using builtins::trunctfxf2;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128 && LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

#endif // LLVM_LIBC_SHARED_BUILTINS_TRUNCTFXF2_H
