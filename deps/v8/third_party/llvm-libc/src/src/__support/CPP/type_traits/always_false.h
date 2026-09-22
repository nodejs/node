//===-- convenient static_assert(false) helper ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ALWAYS_FALSE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ALWAYS_FALSE_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// This is technically not part of the standard but it come often enough that
// it's convenient to have around.
//
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2593r0.html#valid-workaround
//
// This will be fixed in C++23 according to [CWG
// 2518](https://cplusplus.github.io/CWG/issues/2518.html).

// Usage `static_assert(cpp::always_false<T>, "error message");`
template <typename...> LIBC_INLINE_VAR constexpr bool always_false = false;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ALWAYS_FALSE_H
