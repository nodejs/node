//===-- invoke type_traits --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_H

#include "src/__support/CPP/type_traits/always_false.h"
#include "src/__support/CPP/type_traits/decay.h"
#include "src/__support/CPP/type_traits/enable_if.h"
#include "src/__support/CPP/type_traits/is_base_of.h"
#include "src/__support/CPP/type_traits/is_pointer.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/utility/forward.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

namespace detail {

// Catch all function and functor types.
template <class FunctionPtrType> struct invoke_dispatcher {
  template <class T, class... Args,
            typename = cpp::enable_if_t<
                cpp::is_same_v<cpp::decay_t<T>, FunctionPtrType>>>
  LIBC_INLINE static decltype(auto) call(T &&fun, Args &&...args) {
    return cpp::forward<T>(fun)(cpp::forward<Args>(args)...);
  }
};

// Catch pointer to member function types.
template <class Class, class FunctionReturnType>
struct invoke_dispatcher<FunctionReturnType Class::*> {
  using FunctionPtrType = FunctionReturnType Class::*;

  template <class T, class... Args, class DecayT = cpp::decay_t<T>>
  LIBC_INLINE static decltype(auto) call(FunctionPtrType fun, T &&t1,
                                         Args &&...args) {
    if constexpr (cpp::is_base_of_v<Class, DecayT>) {
      // T is a (possibly cv ref) type.
      return (cpp::forward<T>(t1).*fun)(cpp::forward<Args>(args)...);
    } else if constexpr (cpp::is_pointer_v<T>) {
      // T is a pointer type.
      return (*cpp::forward<T>(t1).*fun)(cpp::forward<Args>(args)...);
    } else {
      static_assert(cpp::always_false<T>);
    }
  }
};

} // namespace detail
template <class Function, class... Args>
decltype(auto) invoke(Function &&fun, Args &&...args) {
  return detail::invoke_dispatcher<cpp::decay_t<Function>>::call(
      cpp::forward<Function>(fun), cpp::forward<Args>(args)...);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_H
