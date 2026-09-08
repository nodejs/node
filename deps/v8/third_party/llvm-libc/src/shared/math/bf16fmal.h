//===-- Shared bf16fmal function --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_BF16FMAL_H
#define LLVM_LIBC_SHARED_MATH_BF16FMAL_H

#include "shared/libc_common.h"
#include "src/__support/macros/properties/types.h"

#ifndef LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE

#include "src/__support/math/bf16fmal.h"

namespace LIBC_NAMESPACE_DECL {

namespace shared {

using math::bf16fmal;

} // namespace shared

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE

#endif // LLVM_LIBC_SHARED_MATH_BF16FMAL_H
