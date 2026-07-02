//===-- is_convertible type_traits ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONVERTIBLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONVERTIBLE_H

#include "src/__support/CPP/type_traits/is_void.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_convertible
namespace detail {
template <class T>
auto test_returnable(int)
    -> decltype(void(static_cast<T (*)()>(nullptr)), cpp::true_type{});
template <class> auto test_returnable(...) -> cpp::false_type;

template <class From, class To>
auto test_implicitly_convertible(int)
    -> decltype(void(cpp::declval<void (&)(To)>()(cpp::declval<From>())),
                cpp::true_type{});
template <class, class>
auto test_implicitly_convertible(...) -> cpp::false_type;
} // namespace detail

template <class From, class To>
struct is_convertible
    : cpp::bool_constant<
          (decltype(detail::test_returnable<To>(0))::value &&
           decltype(detail::test_implicitly_convertible<From, To>(0))::value) ||
          (cpp::is_void_v<From> && cpp::is_void_v<To>)> {};

template <class From, class To>
LIBC_INLINE_VAR constexpr bool is_convertible_v =
    is_convertible<From, To>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONVERTIBLE_H
