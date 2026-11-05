// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implementation details for `absl::bind_front()`.

#ifndef ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_
#define ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/container/internal/compressed_tuple.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace functional_internal {

// Invoke the method, expanding the tuple of bound arguments.
template <class R, class Tuple, size_t... Idx, class... Args>
R Apply(Tuple&& bound, std::index_sequence<Idx...>, Args&&... free) {
  return std::invoke(std::forward<Tuple>(bound).template get<Idx>()...,
                     std::forward<Args>(free)...);
}

template <class F, class... BoundArgs>
class FrontBinder {
  using BoundArgsT = absl::container_internal::CompressedTuple<F, BoundArgs...>;
  using Idx = std::make_index_sequence<sizeof...(BoundArgs) + 1>;

  BoundArgsT bound_args_;

 public:
  template <class... Ts>
  constexpr explicit FrontBinder(std::in_place_t, Ts&&... ts)
      : bound_args_(std::forward<Ts>(ts)...) {}

  template <class... FreeArgs,
            class R = std::invoke_result_t<F&, BoundArgs&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) & {
    return functional_internal::Apply<R>(bound_args_, Idx(),
                                         std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<const F&, const BoundArgs&...,
                                           FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) const& {
    return functional_internal::Apply<R>(bound_args_, Idx(),
                                         std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<F&&, BoundArgs&&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) && {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::Apply<R>(std::move(bound_args_), Idx(),
                                         std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<const F&&, const BoundArgs&&...,
                                           FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) const&& {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::Apply<R>(std::move(bound_args_), Idx(),
                                         std::forward<FreeArgs>(free_args)...);
  }
};

template <class F, class... BoundArgs>
using bind_front_t = FrontBinder<std::decay_t<F>, std::decay_t<BoundArgs>...>;

// Stateless invoker for the compile-time constant target `ConstFn` of
// `absl::bind_front<ConstFn>(args...)` (C++26 [func.bind.front]).
//
// `operator()` has a trailing `decltype(std::invoke(ConstFn, ...))` return type
// rather than `decltype(auto)` so that an invalid call is a substitution
// failure instead of a hard error. Deducing a `decltype(auto)` return type
// instantiates the body, so merely checking whether a call is valid would fail
// to compile. That would break `std::is_invocable` and overload resolution,
// including among the call operators of a `bind_front` result wrapping the
// invoker: e.g. calling such a result as an rvalue fails if it holds a
// move-only bound argument, because its `const&` overload is also considered.
//
// The return type is `decltype(std::invoke(ConstFn, ...))` rather than
// `std::invoke_result_t<decltype(ConstFn), ...>` because for C++20 class-type
// non-type template parameters, `decltype(ConstFn)` is the unqualified type `F`
// (which `std::invoke_result_t` treats as an rvalue `F&&`), whereas the
// template parameter expression `ConstFn` is a `const F` lvalue as specified by
// [func.bind.front].
template <auto ConstFn>
struct ConstFnInvoker {
  template <class... FreeArgs>
  constexpr auto operator()(FreeArgs&&... free_args) const noexcept(
      noexcept(std::invoke(ConstFn, std::forward<FreeArgs>(free_args)...)))
      -> decltype(std::invoke(ConstFn, std::forward<FreeArgs>(free_args)...)) {
    return std::invoke(ConstFn, std::forward<FreeArgs>(free_args)...);
  }
};

}  // namespace functional_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_
