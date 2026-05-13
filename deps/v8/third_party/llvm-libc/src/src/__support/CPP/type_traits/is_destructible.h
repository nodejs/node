//===-- is_destructible type_traits -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_DESTRUCTIBLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_DESTRUCTIBLE_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/is_function.h"
#include "src/__support/CPP/type_traits/is_reference.h"
#include "src/__support/CPP/type_traits/remove_all_extents.h"
#include "src/__support/CPP/type_traits/true_type.h"
#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_destructible
#if __has_builtin(__is_destructible) || defined(LIBC_COMPILER_IS_MSVC)
template <typename T>
struct is_destructible : bool_constant<__is_destructible(T)> {};
#else
//  if it's a   reference,              return true
//  if it's a   function,               return false
//  if it's     void,                   return false
//  if it's an  array of unknown bound, return false
//  Otherwise, return "declval<T&>().~T()" is well-formed
//    where T is remove_all_extents<T>::type
template <typename> struct __is_destructible_apply : cpp::type_identity<int> {};
template <typename T> struct __is_destructor_wellformed {
  template <typename T1>
  static cpp::true_type __test(
      typename __is_destructible_apply<decltype(declval<T1 &>().~T1())>::type);
  template <typename T1> static cpp::false_type __test(...);
  static const bool value = decltype(__test<T>(12))::value;
};
template <typename T, bool> struct __destructible_imp;
template <typename T>
struct __destructible_imp<T, false>
    : public bool_constant<
          __is_destructor_wellformed<cpp::remove_all_extents_t<T>>::value> {};
template <typename T>
struct __destructible_imp<T, true> : public cpp::true_type {};
template <typename T, bool> struct __destructible_false;
template <typename T>
struct __destructible_false<T, false>
    : public __destructible_imp<T, is_reference<T>::value> {};
template <typename T>
struct __destructible_false<T, true> : public cpp::false_type {};
template <typename T>
struct is_destructible : public __destructible_false<T, is_function<T>::value> {
};
template <typename T> struct is_destructible<T[]> : public false_type {};
template <> struct is_destructible<void> : public false_type {};
#endif
template <class T>
LIBC_INLINE_VAR constexpr bool is_destructible_v = is_destructible<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_DESTRUCTIBLE_H
