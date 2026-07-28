// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: bind_back.h
// -----------------------------------------------------------------------------
//
// `absl::bind_back()` returns a functor by binding a number of arguments to
// the back of a provided (usually more generic) functor. Unlike `std::bind`,
// it does not require the use of argument placeholders. The simpler syntax of
// `absl::bind_back()` allows you to avoid known misuses with `std::bind()`.
//
// `absl::bind_back()` is meant as a drop-in replacement for C++23's
// `std::bind_back()`, which similarly resolves these issues with
// `std::bind()`. Both `bind_back()` alternatives, unlike `std::bind()`, allow
// partial function application. (See
// https://en.wikipedia.org/wiki/Partial_application).

#ifndef ABSL_FUNCTIONAL_BIND_BACK_H_
#define ABSL_FUNCTIONAL_BIND_BACK_H_

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#if defined(__cpp_lib_bind_back) && __cpp_lib_bind_back >= 202202L
#include <functional>  // For std::bind_back.
#endif

#include <utility>

#include "absl/base/config.h"
#include "absl/functional/internal/back_binder.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// bind_back()
//
// Binds the last N arguments of an invocable object and stores them by value.
//
// Like `std::bind()`, `absl::bind_back()` is implicitly convertible to
// `std::function`.  In particular, it may be used as a simpler replacement for
// `std::bind()` in most cases, as it does not require placeholders to be
// specified. More importantly, it provides more reliable correctness guarantees
// than `std::bind()`; while `std::bind()` will silently ignore passing more
// parameters than expected, for example, `absl::bind_back()` will report such
// mis-uses as errors. In C++23, `absl::bind_back` is replaced by
// `std::bind_back`.
//
#if defined(__cpp_lib_bind_back) && __cpp_lib_bind_back >= 202202L
using std::bind_back;
#else
template <class F, class... BoundArgs>
constexpr functional_internal::bind_back_t<F, BoundArgs...> bind_back(
    F&& func, BoundArgs&&... args) {
  return functional_internal::bind_back_t<F, BoundArgs...>(
      std::in_place, std::forward<F>(func), std::forward<BoundArgs>(args)...);
}
#endif

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_FUNCTIONAL_BIND_BACK_H_
