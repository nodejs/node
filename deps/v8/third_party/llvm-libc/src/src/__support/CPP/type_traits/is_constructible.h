//===------------------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file
// This file contains a free-standing implementation of is_constructible
// type trait.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTRUCTIBLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTRUCTIBLE_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

namespace is_contructible_detail {

#if LIBC_HAS_BUILTIN_IS_CONSTRUCTIBLE

template <typename T, typename... Args>
struct is_constructible_impl
    : public bool_constant<__is_constructible(T, Args...)> {};

#else
// Fallback SFINAE implementation for GCC 7 and older toolchains

template <typename T, typename... Args> struct is_constructible_impl {
private:
  template <typename T1, typename... Args1>
  LIBC_INLINE static auto test(int)
      -> decltype(T1(declval<Args1>()...), bool_constant<true>());

  template <typename, typename...>
  LIBC_INLINE static auto test(...) -> bool_constant<false>;

public:
  using type = decltype(test<T, Args...>(0));
};

#endif // LIBC_HAS_BUILTIN_IS_CONSTRUCTIBLE

} // namespace is_contructible_detail

template <typename T, typename... Args>
struct is_constructible
    : public is_contructible_detail::is_constructible_impl<T, Args...> {};

template <typename T, typename... Args>
LIBC_INLINE_VAR constexpr bool is_constructible_v =
    is_constructible<T, Args...>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTRUCTIBLE_H
