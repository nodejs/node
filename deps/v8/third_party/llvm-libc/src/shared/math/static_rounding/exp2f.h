//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the shared statically-rounded exp2f(x) function
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_STATIC_ROUNDING_EXP2F_H
#define LLVM_LIBC_SHARED_MATH_STATIC_ROUNDING_EXP2F_H

#include "shared/libc_common.h"
#include "src/__support/math/exp2f_integer_eval.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {
namespace static_rounding {

using math::static_rounding::exp2f;

} // namespace static_rounding
} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_MATH_STATIC_ROUNDING_EXP2F_H
