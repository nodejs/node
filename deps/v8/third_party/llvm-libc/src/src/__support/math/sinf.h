//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation selector for sinf.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINF_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/cpu_features.h"

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    (defined(LIBC_MATH_HAS_SMALL_TABLES) ||                                    \
     defined(LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT)) &&                     \
    defined(LIBC_TARGET_CPU_HAS_FMA_FLOAT)
#include "sinf_float_eval.h"
#define LIBC_MATH_SINF_IMPL float_eval
#else
#include "sinf_double_eval.h"
#define LIBC_MATH_SINF_IMPL double_eval
#endif

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_MATH_SINF_IMPL::sinf;

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_MATH_SINF_IMPL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINF_H
