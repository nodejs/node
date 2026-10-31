//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Float-only implementation of sinf.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SINF_FLOAT_EVAL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SINF_FLOAT_EVAL_H

#include "sincosf_float_eval.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace float_eval {

LIBC_INLINE float sinf(float x) {
  return sincosf_float_eval::sincosf_eval</*IS_SIN*/ true>(x);
}

} // namespace float_eval
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SINF_FLOAT_EVAL_H
