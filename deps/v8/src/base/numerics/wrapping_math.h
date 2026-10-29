// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_WRAPPING_MATH_H_
#define V8_BASE_NUMERICS_WRAPPING_MATH_H_

#include <type_traits>

namespace v8::base {

// Returns `a + b` with overflow defined to wrap around, i.e. modulo 2^N where N
// is the bit width of `T`.
template <typename T>
inline constexpr T WrappingAdd(T a, T b) {
  static_assert(std::is_integral_v<T>);
  // Unsigned arithmetic wraps, so convert to the corresponding unsigned type.
  // Note that, if `T` is smaller than `int`, e.g. `int16_t`, the values are
  // promoted to `int`, which brings us back to undefined overflow. This is fine
  // here because the sum of any two `int16_t`s fits in `int`, but `WrappingMul`
  // will need a more complex implementation.
  using Unsigned = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<Unsigned>(a) + static_cast<Unsigned>(b));
}

// Returns `a - b` with overflow defined to wrap around, i.e. modulo 2^N where N
// is the bit width of `T`.
template <typename T>
inline constexpr T WrappingSub(T a, T b) {
  static_assert(std::is_integral_v<T>);
  // Unsigned arithmetic wraps, so convert to the corresponding unsigned type.
  // Note that, if `T` is smaller than `int`, e.g. `int16_t`, the values are
  // promoted to `int`, which brings us back to undefined overflow. This is fine
  // here because the difference of any two `int16_t`s fits in `int`, but
  // `WrappingMul` will need a more complex implementation.
  using Unsigned = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<Unsigned>(a) - static_cast<Unsigned>(b));
}

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_WRAPPING_MATH_H_
