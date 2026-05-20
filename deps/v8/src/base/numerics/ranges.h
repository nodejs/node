// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_RANGES_H_
#define V8_BASE_NUMERICS_RANGES_H_

#include <cmath>
#include <type_traits>

namespace v8::base {

template <typename T>
constexpr bool IsApproximatelyEqual(T lhs, T rhs, T tolerance) {
  static_assert(std::is_arithmetic_v<T>, "Argument must be arithmetic");
  return std::abs(rhs - lhs) <= tolerance;
}

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_RANGES_H_
