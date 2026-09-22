//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared floating-point comparison results for compiler-rt's soft-float
/// compare builtins.  These mirror compiler-rt's GCC-compatible __le<f>2 /
/// __ge<f>2 / __unord<f>2 (quiet: they raise no FP exceptions), reusing
/// LLVM-libc's IEEE comparison predicates, so they can be reused by
/// compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_CMP_HELPER_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_CMP_HELPER_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/comparison_operations.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// GCC soft-float compare, "LE" convention: -1 if a<b, 0 if a==b, 1 if a>b, and
// 1 when unordered (either operand is NaN).  Quiet -- raises no FP exceptions,
// matching compiler-rt's __le<f>2 (which __eq/__ne/__lt/__cmp alias).  NaN is
// handled up front so the ordering predicates are only reached for non-NaN
// inputs, where they never signal.
template <typename T> LIBC_INLINE int cmp_le(T a, T b) {
  using FPBits = fputil::FPBits<T>;
  if (FPBits(a).is_nan() || FPBits(b).is_nan())
    return 1; // LE_UNORDERED
  if (fputil::equals(a, b))
    return 0;                              // LE_EQUAL
  return fputil::less_than(a, b) ? -1 : 1; // LE_LESS : LE_GREATER
}

// Same as cmp_le, but unordered yields -1; matches compiler-rt's __ge<f>2
// (which __gt aliases).
template <typename T> LIBC_INLINE int cmp_ge(T a, T b) {
  using FPBits = fputil::FPBits<T>;
  if (FPBits(a).is_nan() || FPBits(b).is_nan())
    return -1; // GE_UNORDERED
  if (fputil::equals(a, b))
    return 0;                                 // GE_EQUAL
  return fputil::greater_than(a, b) ? 1 : -1; // GE_GREATER : GE_LESS
}

// 1 iff either operand is NaN, else 0; matches compiler-rt's __unord<f>2.
template <typename T> LIBC_INLINE int cmp_unord(T a, T b) {
  using FPBits = fputil::FPBits<T>;
  return FPBits(a).is_nan() || FPBits(b).is_nan();
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_CMP_HELPER_H
