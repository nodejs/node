//===-- has_unique_object_representations type_traits ------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_HAS_UNIQUE_OBJECT_REPRESENTATIONS_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_HAS_UNIQUE_OBJECT_REPRESENTATIONS_H

#include "src/__support/CPP/type_traits/integral_constant.h"
#include "src/__support/CPP/type_traits/remove_all_extents.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class T>
struct has_unique_object_representations
    : public integral_constant<bool, __has_unique_object_representations(
                                         remove_all_extents_t<T>)> {};

template <class T>
LIBC_INLINE_VAR constexpr bool has_unique_object_representations_v =
    has_unique_object_representations<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_HAS_UNIQUE_OBJECT_REPRESENTATIONS_H
