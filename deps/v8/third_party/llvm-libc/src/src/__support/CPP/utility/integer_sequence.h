//===-- integer_sequence utility --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_INTEGER_SEQUENCE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_INTEGER_SEQUENCE_H

#include "src/__support/CPP/type_traits/is_integral.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// integer_sequence
template <typename T, T... Ints> struct integer_sequence {
  static_assert(cpp::is_integral_v<T>);
  template <T Next> using append = integer_sequence<T, Ints..., Next>;
};

namespace detail {
template <typename T, int N> struct make_integer_sequence {
  using type =
      typename make_integer_sequence<T, N - 1>::type::template append<N>;
};
template <typename T> struct make_integer_sequence<T, -1> {
  using type = integer_sequence<T>;
};
} // namespace detail

template <typename T, int N>
using make_integer_sequence =
    typename detail::make_integer_sequence<T, N - 1>::type;

// index sequence
template <size_t... Ints>
using index_sequence = integer_sequence<size_t, Ints...>;
template <int N>
using make_index_sequence =
    typename detail::make_integer_sequence<size_t, N - 1>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_INTEGER_SEQUENCE_H
