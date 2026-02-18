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

#ifndef ABSL_EXTEND_INTERNAL_DEPENDENCIES_H_
#define ABSL_EXTEND_INTERNAL_DEPENDENCIES_H_

#include <type_traits>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace dependencies_internal {

// Dependencies is a meta-function which constructs a collection which
// recursively looks through types and either adds them to the list, or, if they
// have a `using deps = void(...);` takes those listed dependencies as well. For
// example, given:
//
// struct X {};
// struct Y {};
// struct A {};
// struct B { using deps = void(A, X); };
// struct C { using deps = void(A, Y); };
// struct D { using deps = void(B, C); };
//
// Dependencies<D>() will return a function pointer returning void and accepting
// arguments of types A, B, C, D, X, and Y in some unspecified order each
// exactly once, and no other types.
template <typename T>
auto GetDependencies(T*) -> typename T::deps*;
auto GetDependencies(void*) -> void (*)();

template <typename FnPtr, typename... Ts>
struct CombineDeps {};
template <typename... Ps, typename... Ts>
struct CombineDeps<void (*)(Ps...), Ts...> {
  using type = void (*)(Ps..., Ts...);
};

template <typename... Processed>
auto DependenciesImpl(void (*)(), void (*)(Processed...)) {
  return static_cast<void (*)(Processed...)>(nullptr);
}

template <typename T, typename... Ts, typename... Processed>
auto DependenciesImpl(void (*)(T, Ts...), void (*)(Processed...)) {
  if constexpr ((std::is_same_v<T, Processed> || ...)) {
    return DependenciesImpl(static_cast<void (*)(Ts...)>(nullptr),
                            static_cast<void (*)(Processed...)>(nullptr));
  } else {
    using deps = decltype(GetDependencies(static_cast<T*>(nullptr)));
    if constexpr (std ::is_same_v<deps, void (*)()>) {
      return DependenciesImpl(static_cast<void (*)(Ts...)>(nullptr),
                              static_cast<void (*)(T, Processed...)>(nullptr));
    } else {
      return DependenciesImpl(
          static_cast<typename CombineDeps<deps, Ts...>::type>(nullptr),
          static_cast<void (*)(T, Processed...)>(nullptr));
    }
  }
}

template <typename... Ts>
using Dependencies = decltype(DependenciesImpl(
    static_cast<void (*)(Ts...)>(nullptr), static_cast<void (*)()>(nullptr)));

}  // namespace dependencies_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_DEPENDENCIES_H_
