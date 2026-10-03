// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_MEMORY_SPAN_H_
#define INCLUDE_V8_MEMORY_SPAN_H_

#include <array>
#include <span>

#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

/**
 * Points to an unowned contiguous buffer holding a known number of elements.
 *
 * This is an alias for std::span.
 */
template <typename T>
using MemorySpan V8_DEPRECATE_SOON("Use std::span instead.") = std::span<T>;

/**
 * Helper function template to create an array of fixed length, initialized by
 * the provided initializer list, without explicitly specifying the array size,
 * e.g.
 *
 *   auto arr = v8::to_array<Local<String>>({v8_str("one"), v8_str("two")});
 *
 * This is an alias for std::to_array.
 */

template <typename T, std::size_t N>
V8_DEPRECATE_SOON("Use std::to_array instead.")
[[nodiscard]] constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
  return std::to_array(a);
}

template <typename T, std::size_t N>
V8_DEPRECATE_SOON("Use std::to_array instead.")
[[nodiscard]] constexpr std::array<std::remove_cv_t<T>, N> to_array(
    T (&&a)[N]) {
  return std::to_array(std::move(a));
}

}  // namespace v8

#endif  // INCLUDE_V8_MEMORY_SPAN_H_
