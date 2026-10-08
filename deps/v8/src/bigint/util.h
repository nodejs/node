// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_UTIL_H_
#define V8_BIGINT_UTIL_H_

// "Generic" helper functions (not specific to BigInts).

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>  // For _BitScanReverse.
#endif

namespace v8 {
namespace bigint {

// Rounds up x to a multiple of y.
inline constexpr int RoundUp(int x, int y) { return (x + y - 1) & -y; }

inline constexpr int CountTrailingZeros(uint32_t value) {
#if __GNUC__ || __clang__
  return value == 0 ? 32 : __builtin_ctz(value);
#elif _MSC_VER
  unsigned long index = 0;  // NOLINT(runtime/int).
  return _BitScanForward(&index, value) ? index : 32;
#else
#error Unsupported compiler.
#endif
}

inline constexpr bool IsPowerOfTwo(int value) {
  return value > 0 && (value & (value - 1)) == 0;
}

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_UTIL_H_
