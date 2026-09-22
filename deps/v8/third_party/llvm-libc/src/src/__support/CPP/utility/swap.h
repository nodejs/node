//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the swap utility.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_SWAP_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_SWAP_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/utility/move.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class T> LIBC_INLINE constexpr void swap(T &a, T &b) {
  T temp = cpp::move(a);
  a = cpp::move(b);
  b = cpp::move(temp);
}

template <class T, size_t N>
LIBC_INLINE constexpr void swap(T (&a)[N], T (&b)[N]) {
  for (size_t i = 0; i < N; ++i)
    swap(a[i], b[i]);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_SWAP_H
