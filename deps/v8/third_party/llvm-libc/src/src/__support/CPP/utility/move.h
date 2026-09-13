//===-- move utility --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_MOVE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_MOVE_H

#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// move
template <class T>
LIBC_INLINE constexpr cpp::remove_reference_t<T> &&move(T &&t) {
  return static_cast<typename cpp::remove_reference_t<T> &&>(t);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_MOVE_H
