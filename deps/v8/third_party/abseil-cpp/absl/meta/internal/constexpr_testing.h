// Copyright 2025 The Abseil Authors.
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

#ifndef ABSL_META_INTERNAL_CONSTEXPR_TESTING_H_
#define ABSL_META_INTERNAL_CONSTEXPR_TESTING_H_

#include <type_traits>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace meta_internal {

// HasConstexprEvaluation([] { ... }) will evaluate to `true` if the
// lambda can be evaluated in a constant expression and `false`
// otherwise.
// The return type of the lambda is not relevant, as long as the whole
// evaluation works in a constant expression.
template <typename F>
constexpr bool HasConstexprEvaluation(F f);

/// Implementation details below ///

namespace internal_constexpr_evaluation {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
#endif
// This will give a constexpr instance of `F`.
// This works for captureless lambdas because they have no state and the copy
// constructor does not look at the input reference.
template <typename F>
constexpr F default_instance = default_instance<F>;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

template <typename F>
constexpr std::integral_constant<bool, (default_instance<F>(), true)> Tester(
    int) {
  return {};
}

template <typename S>
constexpr std::false_type Tester(char) {
  return {};
}

}  // namespace internal_constexpr_evaluation

template <typename F>
constexpr bool HasConstexprEvaluation(F) {
  return internal_constexpr_evaluation::Tester<F>(0);
}

}  // namespace meta_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_META_INTERNAL_CONSTEXPR_TESTING_H_
