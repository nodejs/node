//===-- is_base_of type_traits ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_BASE_OF_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_BASE_OF_H

#include "src/__support/CPP/type_traits/add_rvalue_reference.h"
#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/is_class.h"
#include "src/__support/CPP/type_traits/remove_all_extents.h"
#include "src/__support/CPP/type_traits/true_type.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_base_of
namespace detail {
template <typename B> cpp::true_type __test_ptr_conv(const volatile B *);
template <typename> cpp::false_type __test_ptr_conv(const volatile void *);

template <typename B, typename D>
auto is_base_of(int) -> decltype(__test_ptr_conv<B>(static_cast<D *>(nullptr)));

template <typename, typename>
auto is_base_of(...) -> cpp::true_type; // private or ambiguous base

} // namespace detail

template <typename Base, typename Derived>
struct is_base_of
    : cpp::bool_constant<
          cpp::is_class_v<Base> &&
          cpp::is_class_v<Derived> &&decltype(detail::is_base_of<Base, Derived>(
              0))::value> {};
template <typename Base, typename Derived>
LIBC_INLINE_VAR constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_BASE_OF_H
