//===-- remove_extent type_traits -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_EXTENT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_EXTENT_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"
#include "stddef.h" // size_t

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// remove_extent
template <class T> struct remove_extent : cpp::type_identity<T> {};
template <class T> struct remove_extent<T[]> : cpp::type_identity<T> {};
template <class T, size_t N>
struct remove_extent<T[N]> : cpp::type_identity<T> {};
template <class T> using remove_extent_t = typename remove_extent<T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_EXTENT_H
