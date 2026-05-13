//===-- A self contained equivalent of <algorithm> --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This file is minimalist on purpose but can receive a few more function if
// they prove useful.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_ALGORITHM_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_ALGORITHM_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class T = void> struct plus {};
template <class T = void> struct multiplies {};
template <class T = void> struct bit_and {};
template <class T = void> struct bit_or {};
template <class T = void> struct bit_xor {};

template <class T> LIBC_INLINE constexpr const T &max(const T &a, const T &b) {
  return (a < b) ? b : a;
}

template <class T> LIBC_INLINE constexpr const T &min(const T &a, const T &b) {
  return (a < b) ? a : b;
}

template <class T> LIBC_INLINE constexpr T abs(T a) { return a < 0 ? -a : a; }

template <class InputIt, class UnaryPred>
LIBC_INLINE constexpr InputIt find_if_not(InputIt first, InputIt last,
                                          UnaryPred q) {
  for (; first != last; ++first)
    if (!q(*first))
      return first;

  return last;
}

template <class InputIt, class UnaryPred>
LIBC_INLINE constexpr bool all_of(InputIt first, InputIt last, UnaryPred p) {
  return find_if_not(first, last, p) == last;
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_ALGORITHM_H
