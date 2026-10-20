//===-- is_trivially_destructible type_traits -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_DESTRUCTIBLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_DESTRUCTIBLE_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_destructible.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_trivially_destructible
#if __has_builtin(__is_trivially_destructible)
template <typename T>
struct is_trivially_destructible
    : public bool_constant<__is_trivially_destructible(T)> {};
#else
template <typename T>
struct is_trivially_destructible
    : public bool_constant<cpp::is_destructible_v<T> &&__has_trivial_destructor(
          T)> {};
#endif // __has_builtin(__is_trivially_destructible)
template <typename T>
LIBC_INLINE_VAR constexpr bool is_trivially_destructible_v =
    is_trivially_destructible<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_DESTRUCTIBLE_H
