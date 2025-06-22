// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_INTEGRAL_CONSTANT_LIKE_H_
#define V8_BASE_NUMERICS_INTEGRAL_CONSTANT_LIKE_H_

#include <concepts>
#include <type_traits>

namespace v8::base {

// Exposition-only concept from [span.syn]
template <typename T>
concept IntegralConstantLike =
    std::is_integral_v<decltype(T::value)> &&
    !std::is_same_v<bool, std::remove_const_t<decltype(T::value)>> &&
    std::convertible_to<T, decltype(T::value)> &&
    std::equality_comparable_with<T, decltype(T::value)> &&
    std::bool_constant<T() == T::value>::value &&
    std::bool_constant<static_cast<decltype(T::value)>(T()) == T::value>::value;

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_INTEGRAL_CONSTANT_LIKE_H_
