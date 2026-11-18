//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of cpp::function_ref.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/type_traits/enable_if.h"
#include "src/__support/CPP/type_traits/is_convertible.h"
#include "src/__support/CPP/type_traits/is_function.h"
#include "src/__support/CPP/type_traits/is_pointer.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/is_void.h"
#include "src/__support/CPP/type_traits/remove_cvref.h"
#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/CPP/utility/forward.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

/// A non-owning reference to a callable, similar to C++26 std::function_ref.
/// This class does not own the callable, so it is not in general safe to
/// store a function_ref.
template <typename Fn> class function_ref;

template <typename Ret, typename... Params> class function_ref<Ret(Params...)> {
  Ret (*callback)(intptr_t callable, Params... params);
  intptr_t callable;

  template <typename Callable>
  LIBC_INLINE static Ret callback_fn(intptr_t callable, Params... params) {
    auto &fn = *reinterpret_cast<Callable *>(callable);
    // Discard the result if Ret is void, and convert it implicitly otherwise.
    // Avoiding an explicit cast lets the compiler diagnose narrowing
    // conversions and discarded [[nodiscard]] results.
    if constexpr (cpp::is_void_v<Ret>)
      fn(cpp::forward<Params>(params)...);
    else
      return fn(cpp::forward<Params>(params)...);
  }

public:
  template <typename Callable>
  LIBC_INLINE function_ref(
      Callable &&callable,
      // This is not the copy-constructor.
      enable_if_t<!cpp::is_same_v<remove_cvref_t<Callable>, function_ref>> * =
          nullptr,
      // Functor must be callable and return a suitable type.
      enable_if_t<cpp::is_void_v<Ret> ||
                  cpp::is_convertible_v<
                      decltype(declval<Callable>()(declval<Params>()...)), Ret>>
          * = nullptr)
      : callback(callback_fn<cpp::remove_reference_t<Callable>>),
        callable(reinterpret_cast<intptr_t>(&callable)) {}

  // Function pointers are stored by value rather than by address so that
  // binding a temporary pointer (e.g. `function_ref<void()> f = &foo;`) does
  // not dangle. Overload resolution prefers this over the forwarding
  // constructor above because it is more specialized.
  template <typename Fn>
  LIBC_INLINE function_ref(
      Fn *fn, enable_if_t<cpp::is_function_v<Fn>> * = nullptr,
      // Function must be callable and return a suitable type.
      enable_if_t<cpp::is_void_v<Ret> ||
                  cpp::is_convertible_v<
                      decltype(declval<Fn *>()(declval<Params>()...)), Ret>> * =
          nullptr)
      : callback(callback_fn<Fn>), callable(reinterpret_cast<intptr_t>(fn)) {}

  // Assigning a callable (as opposed to another function_ref or a function
  // pointer) would most likely leave the function_ref pointing to a destroyed
  // temporary.
  template <typename T, enable_if_t<!cpp::is_same_v<T, function_ref> &&
                                    !cpp::is_pointer_v<T>> * = nullptr>
  function_ref &operator=(T) = delete;

  LIBC_INLINE Ret operator()(Params... params) const {
    return callback(callable, cpp::forward<Params>(params)...);
  }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_FUNCTIONAL_H
