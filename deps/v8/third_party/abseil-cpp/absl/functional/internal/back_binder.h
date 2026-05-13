// Copyright 2026 The Abseil Authors.
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

// Implementation details for `absl::bind_back()`.

#ifndef ABSL_FUNCTIONAL_INTERNAL_BACK_BINDER_H_
#define ABSL_FUNCTIONAL_INTERNAL_BACK_BINDER_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/container/internal/compressed_tuple.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace functional_internal {

// Invoke the method, expanding the tuple of bound arguments.
template <class R, class Tuple, size_t... Idx, class... Args>
constexpr R ApplyBack(Tuple&& bound, std::index_sequence<Idx...>,
                      Args&&... free) {
  return std::invoke(std::forward<Tuple>(bound).template get<0>(),
                     std::forward<Args>(free)...,
                     std::forward<Tuple>(bound).template get<Idx + 1>()...);
}

template <class F, class... BoundArgs>
class BackBinder {
  using BoundArgsT = absl::container_internal::CompressedTuple<F, BoundArgs...>;
  using Idx = std::make_index_sequence<sizeof...(BoundArgs)>;

  BoundArgsT bound_args_;

 public:
  template <class... Ts>
  constexpr explicit BackBinder(std::in_place_t, Ts&&... ts)
      : bound_args_(std::forward<Ts>(ts)...) {}

  template <class... FreeArgs,
            class R = std::invoke_result_t<F&, FreeArgs&&..., BoundArgs&...>>
  constexpr R operator()(FreeArgs&&... free_args) & {
    return functional_internal::ApplyBack<R>(
        bound_args_, Idx(), std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<const F&, FreeArgs&&...,
                                           const BoundArgs&...>>
  constexpr R operator()(FreeArgs&&... free_args) const& {
    return functional_internal::ApplyBack<R>(
        bound_args_, Idx(), std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<F&&, FreeArgs&&..., BoundArgs&&...>>
  constexpr R operator()(FreeArgs&&... free_args) && {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::ApplyBack<R>(
        std::move(bound_args_), Idx(), std::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = std::invoke_result_t<const F&&, FreeArgs&&...,
                                           const BoundArgs&&...>>
  constexpr R operator()(FreeArgs&&... free_args) const&& {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::ApplyBack<R>(
        std::move(bound_args_), Idx(), std::forward<FreeArgs>(free_args)...);
  }
};

template <class F, class... BoundArgs>
using bind_back_t = BackBinder<std::decay_t<F>, std::decay_t<BoundArgs>...>;

}  // namespace functional_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_FUNCTIONAL_INTERNAL_BACK_BINDER_H_
