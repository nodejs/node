//===-- Check exceptions for exp functions ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_MATH_CHECK_EXP_H
#define LLVM_LIBC_SHARED_MATH_CHECK_EXP_H

#include "shared/libc_common.h"
#include "src/__support/math/check/exp_exceptions.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {
namespace check {

using math::check::exp_exceptions;

} // namespace check
} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_MATH_CHECK_EXP_H
