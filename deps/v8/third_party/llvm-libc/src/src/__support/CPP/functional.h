//===-- Self contained functional header ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/type_traits/enable_if.h"
#include "src/__support/CPP/type_traits/is_convertible.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/is_void.h"
#include "src/__support/CPP/type_traits/remove_cvref.h"
#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/CPP/utility/forward.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

/// A function type adapted from LLVM's function_ref.
/// This class does not own the callable, so it is not in general safe to
/// store a function.
template <typename Fn> class function;

template <typename Ret, typename... Params> class function<Ret(Params...)> {
  Ret (*callback)(intptr_t callable, Params... params) = nullptr;
  intptr_t callable;

  template <typename Callable>
  LIBC_INLINE static Ret callback_fn(intptr_t callable, Params... params) {
    return (*reinterpret_cast<Callable *>(callable))(
        cpp::forward<Params>(params)...);
  }

public:
  LIBC_INLINE function() = default;
  LIBC_INLINE function(decltype(nullptr)) {}
  LIBC_INLINE ~function() = default;

  template <typename Callable>
  LIBC_INLINE function(
      Callable &&callable,
      // This is not the copy-constructor.
      enable_if_t<!cpp::is_same_v<remove_cvref_t<Callable>, function>> * =
          nullptr,
      // Functor must be callable and return a suitable type.
      enable_if_t<cpp::is_void_v<Ret> ||
                  cpp::is_convertible_v<
                      decltype(declval<Callable>()(declval<Params>()...)), Ret>>
          * = nullptr)
      : callback(callback_fn<cpp::remove_reference_t<Callable>>),
        callable(reinterpret_cast<intptr_t>(&callable)) {}

  LIBC_INLINE Ret operator()(Params... params) const {
    return callback(callable, cpp::forward<Params>(params)...);
  }

  LIBC_INLINE explicit operator bool() const { return callback; }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H
