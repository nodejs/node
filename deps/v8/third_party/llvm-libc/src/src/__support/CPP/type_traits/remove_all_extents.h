//===-- remove_all_extents type_traits --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_ALL_EXTENTS_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_ALL_EXTENTS_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// remove_all_extents
#if __has_builtin(__remove_all_extents)
template <typename T> using remove_all_extents_t = __remove_all_extents(T);
template <typename T>
struct remove_all_extents : cpp::type_identity<remove_all_extents_t<T>> {};
#else
template <typename T> struct remove_all_extents {
  using type = T;
};
template <typename T> struct remove_all_extents<T[]> {
  using type = typename remove_all_extents<T>::type;
};
template <typename T, size_t _Np> struct remove_all_extents<T[_Np]> {
  using type = typename remove_all_extents<T>::type;
};
template <typename T>
using remove_all_extents_t = typename remove_all_extents<T>::type;
#endif

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_ALL_EXTENTS_H
