//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for log2f.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

#ifndef LIBC_MATH_LOG2F_IMPL
#define LIBC_MATH_LOG2F_IMPL double_eval
#endif // !LIBC_MATH_LOG2F_IMPL

#if LIBC_MATH_IMPL_IS(LIBC_MATH_LOG2F_IMPL, double_eval)
#include "src/__support/math/log2f_double_eval.h"
#else
#error "Unsupported LIBC_MATH_LOG2F_IMPL"
#endif // LIBC_MATH_IMPL_IS

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_MATH_LOG2F_IMPL::log2f;

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_MATH_LOG2F_IMPL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG2F_H
