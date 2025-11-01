// Copyright 2025 The Abseil Authors
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
// File: resize_and_overwrite.h
// -----------------------------------------------------------------------------
//
// This file contains a polyfill for C++23's
// std::basic_string<CharT,Traits,Allocator>::resize_and_overwrite
//
// The polyfill takes the form of a free function:

// template<typename T, typename Op>
// void StringResizeAndOverwrite(T& str, typename T::size_type count, Op op);
//
// This avoids the cost of initializing a suitably-sized std::string when it is
// intended to be used as a char array, for example, to be populated by a
// C-style API.
//
// Example usage:
//
// std::string IntToString(int n) {
//   std::string result;
//   constexpr size_t kMaxIntChars = 10;
//   absl::StringResizeAndOverwrite(
//       result, kMaxIntChars, [n](char* buffer, size_t buffer_size) {
//         return snprintf(buffer, buffer_size, "%d", n);
//       });
//   return result;
//  }
//
// https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite.html
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p1072r10.html

#ifndef ABSL_STRINGS_RESIZE_AND_OVERWRITE_H_
#define ABSL_STRINGS_RESIZE_AND_OVERWRITE_H_

#include <cstddef>
#include <string>  // IWYU pragma: keep
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/internal/throw_delegate.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"

#if defined(__cpp_lib_string_resize_and_overwrite) && \
    __cpp_lib_string_resize_and_overwrite >= 202110L
#define ABSL_INTERNAL_HAS_RESIZE_AND_OVERWRITE 1
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace strings_internal {

#ifndef ABSL_INTERNAL_HAS_RESIZE_AND_OVERWRITE

inline size_t ProbeResizeAndOverwriteOp(char*, size_t) { return 0; }

// Prior to C++23, Google's libc++ backports resize_and_overwrite as
// __google_nonstandard_backport_resize_and_overwrite
template <typename T, typename = void>
struct has__google_nonstandard_backport_resize_and_overwrite : std::false_type {
};

template <typename T>
struct has__google_nonstandard_backport_resize_and_overwrite<
    T,
    std::void_t<
        decltype(std::declval<T&>()
                     .__google_nonstandard_backport_resize_and_overwrite(
                         std::declval<size_t>(), ProbeResizeAndOverwriteOp))>>
    : std::true_type {};

// Prior to C++23, the version of libstdc++ that shipped with GCC >= 14
// has __resize_and_overwrite.
template <typename T, typename = void>
struct has__resize_and_overwrite : std::false_type {};

template <typename T>
struct has__resize_and_overwrite<
    T, std::void_t<decltype(std::declval<T&>().__resize_and_overwrite(
           std::declval<size_t>(), ProbeResizeAndOverwriteOp))>>
    : std::true_type {};

// libc++ used  __resize_default_init to achieve uninitialized string resizes
// before removing it September 2025, in favor of resize_and_overwrite.
// https://github.com/llvm/llvm-project/commit/92f5d8df361bb1bb6dea88f86faeedfd295ab970
template <typename T, typename = void>
struct has__resize_default_init : std::false_type {};

template <typename T>
struct has__resize_default_init<
    T, std::void_t<decltype(std::declval<T&>().__resize_default_init(42))>>
    : std::true_type {};

// Prior to C++23, some versions of MSVC have _Resize_and_overwrite.
template <typename T, typename = void>
struct has_Resize_and_overwrite : std::false_type {};

template <typename T>
struct has_Resize_and_overwrite<
    T, std::void_t<decltype(std::declval<T&>()._Resize_and_overwrite(
           std::declval<size_t>(), ProbeResizeAndOverwriteOp))>>
    : std::true_type {};

#endif  // ifndef ABSL_INTERNAL_HAS_RESIZE_AND_OVERWRITE

// A less-efficient fallback implementation that uses resize().
template <typename T, typename Op>
void StringResizeAndOverwriteFallback(T& str, typename T::size_type n, Op op) {
  if (ABSL_PREDICT_FALSE(n > str.max_size())) {
    absl::base_internal::ThrowStdLengthError("absl::StringResizeAndOverwrite");
  }
#ifdef ABSL_HAVE_MEMORY_SANITIZER
  auto old_size = str.size();
#endif
  str.resize(n);
#ifdef ABSL_HAVE_MEMORY_SANITIZER
  if (old_size < n) {
    ABSL_ANNOTATE_MEMORY_IS_UNINITIALIZED(str.data() + old_size, n - old_size);
  }
#endif
  auto new_size = std::move(op)(str.data(), n);
  ABSL_HARDENING_ASSERT(new_size >= 0 && new_size <= n);
  ABSL_HARDENING_ASSERT(str.data()[n] == typename T::value_type{});
  str.erase(static_cast<typename T::size_type>(new_size));
}

}  // namespace strings_internal

// Resizes `str` to contain at most `n` characters, using the user-provided
// operation `op` to modify the possibly indeterminate contents. `op` must
// return the finalized length of `str`.
//
// Invalidates all iterators, pointers, and references into `str`, regardless
// of whether reallocation occurs.
//
// `op(value_type* buf, size_t buf_size)` is allowed to write `value_type{}` to
// `buf[buf_size]`, which facilitiates interoperation with functions that write
// a trailing NUL. Please note that this requirement is more strict than
// `basic_string::resize_and_overwrite()`, which allows writing an abitrary
// value to `buf[buf_size]`.
template <typename T, typename Op>
void StringResizeAndOverwrite(T& str, typename T::size_type n, Op op) {
#ifdef ABSL_INTERNAL_HAS_RESIZE_AND_OVERWRITE
  str.resize_and_overwrite(n, std::move(op));
#else
  if constexpr (strings_internal::
                    has__google_nonstandard_backport_resize_and_overwrite<
                        T>::value) {
    str.__google_nonstandard_backport_resize_and_overwrite(n, std::move(op));
  } else if constexpr (strings_internal::has__resize_and_overwrite<T>::value) {
    str.__resize_and_overwrite(n, std::move(op));
  } else if constexpr (strings_internal::has__resize_default_init<T>::value) {
    str.__resize_default_init(n);
    str.__resize_default_init(
        static_cast<typename T::size_type>(std::move(op)(str.data(), n)));
  } else if constexpr (strings_internal::has_Resize_and_overwrite<T>::value) {
    str._Resize_and_overwrite(n, std::move(op));
  } else {
    strings_internal::StringResizeAndOverwriteFallback(str, n, op);
  }
#endif
#if defined(ABSL_HAVE_MEMORY_SANITIZER)
  auto shadow = __msan_test_shadow(str.data(), str.size());
  ABSL_ASSERT(shadow == -1);
#endif
}

ABSL_NAMESPACE_END
}  // namespace absl

#undef ABSL_INTERNAL_HAS_RESIZE_AND_OVERWRITE

#endif  // ABSL_STRINGS_RESIZE_AND_OVERWRITE_H_
