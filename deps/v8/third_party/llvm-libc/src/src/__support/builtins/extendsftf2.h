//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __extendsftf2 implementation as
/// builtins::extendsftf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDSFTF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDSFTF2_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_NATIVE_FLOAT128

#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Extend float to float128; mirrors compiler-rt's __extendsftf2.
LIBC_INLINE float128 extendsftf2(float x) { return fpconvert<float128>(x); }

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_NATIVE_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDSFTF2_H
