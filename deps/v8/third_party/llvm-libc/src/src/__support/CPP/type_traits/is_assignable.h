//===------------------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file
// This file contains a free-standing implementation of is_assignable
// type trait.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ASSIGNABLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ASSIGNABLE_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

namespace is_assignable_detail {

#if LIBC_HAS_BUILTIN_IS_ASSIGNABLE

template <typename T, typename U>
struct is_assignable_impl : public bool_constant<__is_assignable(T, U)> {};

#else
// Fallback SFINAE implementation for GCC 7 and older toolchains

template <typename T, typename U> struct is_assignable_impl {
private:
  template <typename T1, typename U1>
  LIBC_INLINE static auto test(int)
      -> decltype(declval<T1>() = declval<U1>(), bool_constant<true>());

  template <typename, typename>
  LIBC_INLINE static auto test(...) -> bool_constant<false>;

public:
  using type = decltype(test<T, U>(0));
};

#endif // LIBC_HAS_BUILTIN_IS_ASSIGNABLE

} // namespace is_assignable_detail

// is_assignable
template <typename T, typename U>
struct is_assignable : public is_assignable_detail::is_assignable_impl<T, U> {};

template <typename T, typename U>
LIBC_INLINE_VAR constexpr bool is_assignable_v = is_assignable<T, U>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ASSIGNABLE_H
