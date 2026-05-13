//===-- Shared fadd function ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_FADD_H
#define LLVM_LIBC_SHARED_MATH_FADD_H

#include "shared/libc_common.h"
#include "src/__support/math/fadd.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using math::fadd;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_MATH_FADD_H
