//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for expf.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/cpu_features.h"

#ifndef LIBC_MATH_EXPF_IMPL
#if defined(LIBC_TARGET_ARCH_IS_GPU) &&                                        \
    !defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                              \
    (defined(LIBC_MATH_HAS_SMALL_TABLES) ||                                    \
     defined(LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT)) &&                     \
    defined(LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY) &&                        \
    defined(LIBC_MATH_HAS_NO_EXCEPT) && defined(LIBC_MATH_HAS_NO_ERRNO)
#define LIBC_MATH_EXPF_IMPL float_eval_cr_rn
#elif defined(LIBC_TARGET_CPU_HAS_FPU_FLOAT) &&                                \
    defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    (defined(LIBC_MATH_HAS_SMALL_TABLES) ||                                    \
     defined(LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT))
#define LIBC_MATH_EXPF_IMPL float_eval
#elif !defined(LIBC_TARGET_CPU_HAS_FPU_DOUBLE) &&                              \
    defined(LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY) &&                        \
    defined(LIBC_MATH_HAS_NO_EXCEPT) && defined(LIBC_MATH_HAS_NO_ERRNO)
#define LIBC_MATH_EXPF_IMPL integer_eval
#else
#define LIBC_MATH_EXPF_IMPL double_eval
#endif
#endif // !LIBC_MATH_EXPF_IMPL

#if LIBC_MATH_IMPL_IS(LIBC_MATH_EXPF_IMPL, float_eval) ||                      \
    LIBC_MATH_IMPL_IS(LIBC_MATH_EXPF_IMPL, float_eval_cr_rn)
#include "src/__support/math/expf_float_eval.h"
#elif LIBC_MATH_IMPL_IS(LIBC_MATH_EXPF_IMPL, integer_eval)
#include "src/__support/math/expf_integer_eval.h"
#elif LIBC_MATH_IMPL_IS(LIBC_MATH_EXPF_IMPL, double_eval)
#include "src/__support/math/expf_double_eval.h"
#else
#error "Unsupported LIBC_MATH_EXPF_IMPL"
#endif // LIBC_MATH_IMPL_IS

namespace LIBC_NAMESPACE_DECL {
namespace math {

using LIBC_MATH_EXPF_IMPL::expf;

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_MATH_EXPF_IMPL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXPF_H
