//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared copysignf80 function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_COPYSIGNF80_H
#define LLVM_LIBC_SHARED_MATH_COPYSIGNF80_H

#include "shared/libc_common.h"
#include "src/__support/math/copysignf80.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using math::copysignf80;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_MATH_COPYSIGNF80_H
