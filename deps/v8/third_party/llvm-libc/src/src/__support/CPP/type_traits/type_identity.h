//===-- type_identity type_traits -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_TYPE_IDENTITY_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_TYPE_IDENTITY_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// type_identity
template <typename T> struct type_identity {
  using type = T;
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_TYPE_IDENTITY_H
