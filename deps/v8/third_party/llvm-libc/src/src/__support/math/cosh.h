//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for cosh.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_COSH_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_COSH_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

#include "src/__support/math/cosh_double_eval.h"
#define LIBC_MATH_COSH_IMPL double_eval

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_MATH_COSH_IMPL::cosh;

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_MATH_COSH_IMPL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_COSH_H
