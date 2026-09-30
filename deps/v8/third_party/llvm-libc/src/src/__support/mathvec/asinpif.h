//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD asinpi.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_ASINPIF_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_ASINPIF_H

#include "src/__support/CPP/simd.h"
#define LIBC_MATH_HAS_NO_ERRNO
#define LIBC_MATH_HAS_NO_EXCEPT
#define LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
#include "src/__support/math/asinpif.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE cpp::simd<float, N> asinpif(cpp::simd<float, N> x) {
  return cpp::map(x, [](float a) { return math::asinpif(a); });
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_ASINPIF_H
