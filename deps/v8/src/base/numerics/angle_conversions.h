// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_ANGLE_CONVERSIONS_H_
#define V8_BASE_NUMERICS_ANGLE_CONVERSIONS_H_

#include <concepts>
#include <numbers>

namespace v8::base {

template <typename T>
  requires std::floating_point<T>
constexpr T DegToRad(T deg) {
  return deg * std::numbers::pi_v<T> / 180;
}

template <typename T>
  requires std::floating_point<T>
constexpr T RadToDeg(T rad) {
  return rad * 180 / std::numbers::pi_v<T>;
}

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_ANGLE_CONVERSIONS_H_
