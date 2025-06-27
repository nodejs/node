// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"

#include <limits>

#include "src/base/logging.h"

namespace v8 {
namespace base {
namespace bits {

int32_t SignedMulHigh32(int32_t lhs, int32_t rhs) {
  int64_t const value = static_cast<int64_t>(lhs) * static_cast<int64_t>(rhs);
  return base::bit_cast<int32_t, uint32_t>(base::bit_cast<uint64_t>(value) >>
                                           32u);
}

// The algorithm used is described in section 8.2 of
//   Hacker's Delight, by Henry S. Warren, Jr.
// It assumes that a right shift on a signed integer is an arithmetic shift.
int64_t SignedMulHigh64(int64_t u, int64_t v) {
  uint64_t u0 = u & 0xFFFFFFFF;
  int64_t u1 = u >> 32;
  uint64_t v0 = v & 0xFFFFFFFF;
  int64_t v1 = v >> 32;

  uint64_t w0 = u0 * v0;
  int64_t t = u1 * v0 + (w0 >> 32);
  int64_t w1 = t & 0xFFFFFFFF;
  int64_t w2 = t >> 32;
  w1 = u0 * v1 + w1;

  return u1 * v1 + w2 + (w1 >> 32);
}

// The algorithm used is described in section 8.2 of
//   Hacker's Delight, by Henry S. Warren, Jr.
uint64_t UnsignedMulHigh64(uint64_t u, uint64_t v) {
  uint64_t u0 = u & 0xFFFFFFFF;
  uint64_t u1 = u >> 32;
  uint64_t v0 = v & 0xFFFFFFFF;
  uint64_t v1 = v >> 32;

  uint64_t w0 = u0 * v0;
  uint64_t t = u1 * v0 + (w0 >> 32);
  uint64_t w1 = t & 0xFFFFFFFFLL;
  uint64_t w2 = t >> 32;
  w1 = u0 * v1 + w1;

  return u1 * v1 + w2 + (w1 >> 32);
}

uint32_t UnsignedMulHigh32(uint32_t lhs, uint32_t rhs) {
  uint64_t const value =
      static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs);
  return static_cast<uint32_t>(value >> 32u);
}

int32_t SignedMulHighAndAdd32(int32_t lhs, int32_t rhs, int32_t acc) {
  return base::bit_cast<int32_t>(
      base::bit_cast<uint32_t>(acc) +
      base::bit_cast<uint32_t>(SignedMulHigh32(lhs, rhs)));
}


int32_t SignedDiv32(int32_t lhs, int32_t rhs) {
  if (rhs == 0) return 0;
  if (rhs == -1) return lhs == std::numeric_limits<int32_t>::min() ? lhs : -lhs;
  return lhs / rhs;
}

int64_t SignedDiv64(int64_t lhs, int64_t rhs) {
  if (rhs == 0) return 0;
  if (rhs == -1) return lhs == std::numeric_limits<int64_t>::min() ? lhs : -lhs;
  return lhs / rhs;
}

int32_t SignedMod32(int32_t lhs, int32_t rhs) {
  if (rhs == 0 || rhs == -1) return 0;
  return lhs % rhs;
}

int64_t SignedMod64(int64_t lhs, int64_t rhs) {
  if (rhs == 0 || rhs == -1) return 0;
  return lhs % rhs;
}

int64_t SignedSaturatedAdd64(int64_t lhs, int64_t rhs) {
  using limits = std::numeric_limits<int64_t>;
  // Underflow if {lhs + rhs < min}. In that case, return {min}.
  if (rhs < 0 && lhs < limits::min() - rhs) return limits::min();
  // Overflow if {lhs + rhs > max}. In that case, return {max}.
  if (rhs >= 0 && lhs > limits::max() - rhs) return limits::max();
  return lhs + rhs;
}

int64_t SignedSaturatedSub64(int64_t lhs, int64_t rhs) {
  using limits = std::numeric_limits<int64_t>;
  // Underflow if {lhs - rhs < min}. In that case, return {min}.
  if (rhs > 0 && lhs < limits::min() + rhs) return limits::min();
  // Overflow if {lhs - rhs > max}. In that case, return {max}.
  if (rhs <= 0 && lhs > limits::max() + rhs) return limits::max();
  return lhs - rhs;
}

}  // namespace bits
}  // namespace base
}  // namespace v8
