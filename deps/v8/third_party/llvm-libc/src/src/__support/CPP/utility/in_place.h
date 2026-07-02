//===-- in_place utility ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_IN_PLACE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_IN_PLACE_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE, LIBC_INLINE_VAR
#include "src/__support/macros/config.h"

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// in_place
struct in_place_t {
  LIBC_INLINE explicit in_place_t() = default;
};
LIBC_INLINE_VAR constexpr in_place_t in_place{};

template <class T> struct in_place_type_t {
  LIBC_INLINE explicit in_place_type_t() = default;
};
template <class T> LIBC_INLINE_VAR constexpr in_place_type_t<T> in_place_type{};

template <size_t IDX> struct in_place_index_t {
  LIBC_INLINE explicit in_place_index_t() = default;
};
template <size_t IDX>
LIBC_INLINE_VAR constexpr in_place_index_t<IDX> in_place_index{};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_IN_PLACE_H
