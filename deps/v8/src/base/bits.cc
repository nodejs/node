// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"

#include <limits>

#include "src/base/logging.h"

namespace v8 {
namespace base {
namespace bits {

uint32_t RoundUpToPowerOfTwo32(uint32_t value) {
  DCHECK_LE(value, 0x80000000u);
  value = value - 1;
  value = value | (value >> 1);
  value = value | (value >> 2);
  value = value | (value >> 4);
  value = value | (value >> 8);
  value = value | (value >> 16);
  return value + 1;
}


int32_t SignedMulHigh32(int32_t lhs, int32_t rhs) {
  int64_t const value = static_cast<int64_t>(lhs) * static_cast<int64_t>(rhs);
  return bit_cast<int32_t, uint32_t>(bit_cast<uint64_t>(value) >> 32u);
}


int32_t SignedMulHighAndAdd32(int32_t lhs, int32_t rhs, int32_t acc) {
  return bit_cast<int32_t>(bit_cast<uint32_t>(acc) +
                           bit_cast<uint32_t>(SignedMulHigh32(lhs, rhs)));
}


int32_t SignedDiv32(int32_t lhs, int32_t rhs) {
  if (rhs == 0) return 0;
  if (rhs == -1) return -lhs;
  return lhs / rhs;
}


int32_t SignedMod32(int32_t lhs, int32_t rhs) {
  if (rhs == 0 || rhs == -1) return 0;
  return lhs % rhs;
}

}  // namespace bits
}  // namespace base
}  // namespace v8
