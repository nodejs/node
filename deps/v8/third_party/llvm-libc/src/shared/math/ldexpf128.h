//===-- Shared ldexpf128 function -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_LDEXPF128_H
#define LLVM_LIBC_SHARED_MATH_LDEXPF128_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "shared/libc_common.h"
#include "src/__support/math/ldexpf128.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using math::ldexpf128;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SHARED_MATH_LDEXPF128_H
