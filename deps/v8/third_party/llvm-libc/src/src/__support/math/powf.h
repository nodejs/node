//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for powf.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    (defined(LIBC_MATH_HAS_SMALL_TABLES) ||                                    \
     defined(LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT))

#include "src/__support/math/powf_float_eval.h"
#define LIBC_MATH_POWF_IMPL float_eval

#else // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#include "src/__support/math/powf_double_eval.h"
#define LIBC_MATH_POWF_IMPL double_eval

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_MATH_POWF_IMPL::powf;

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_MATH_POWF_IMPL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H
