// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"

#include <limits>

#include "src/base/logging.h"
#include "src/base/safe_math.h"

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


int64_t FromCheckedNumeric(const internal::CheckedNumeric<int64_t> value) {
  if (value.IsValid())
    return value.ValueUnsafe();

  // We could return max/min but we don't really expose what the maximum delta
  // is. Instead, return max/(-max), which is something that clients can reason
  // about.
  // TODO(rvargas) crbug.com/332611: don't use internal values.
  int64_t limit = std::numeric_limits<int64_t>::max();
  if (value.validity() == internal::RANGE_UNDERFLOW)
    limit = -limit;
  return value.ValueOrDefault(limit);
}


int64_t SignedSaturatedAdd64(int64_t lhs, int64_t rhs) {
  internal::CheckedNumeric<int64_t> rv(lhs);
  rv += rhs;
  return FromCheckedNumeric(rv);
}


int64_t SignedSaturatedSub64(int64_t lhs, int64_t rhs) {
  internal::CheckedNumeric<int64_t> rv(lhs);
  rv -= rhs;
  return FromCheckedNumeric(rv);
}

}  // namespace bits
}  // namespace base
}  // namespace v8
