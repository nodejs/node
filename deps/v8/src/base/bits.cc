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
  DCHECK_LE(value, uint32_t{1} << 31);
  if (value) --value;
// Use computation based on leading zeros if we have compiler support for that.
#if V8_HAS_BUILTIN_CLZ || V8_CC_MSVC
  return 1u << (32 - CountLeadingZeros(value));
#else
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  return value + 1;
#endif
}

uint64_t RoundUpToPowerOfTwo64(uint64_t value) {
  DCHECK_LE(value, uint64_t{1} << 63);
  if (value) --value;
// Use computation based on leading zeros if we have compiler support for that.
#if V8_HAS_BUILTIN_CLZ
  return uint64_t{1} << (64 - CountLeadingZeros(value));
#else
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;
  return value + 1;
#endif
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
  if (rhs == -1) return lhs == std::numeric_limits<int32_t>::min() ? lhs : -lhs;
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

bool SignedMulOverflow32(int32_t lhs, int32_t rhs, int32_t* val) {
  internal::CheckedNumeric<int32_t> rv(lhs);
  rv *= rhs;
  int32_t limit = std::numeric_limits<int32_t>::max();
  *val = rv.ValueOrDefault(limit);
  return !rv.IsValid();
}

bool SignedMulOverflow64(int64_t lhs, int64_t rhs, int64_t* val) {
  internal::CheckedNumeric<int64_t> rv(lhs);
  rv *= rhs;
  int64_t limit = std::numeric_limits<int64_t>::max();
  *val = rv.ValueOrDefault(limit);
  return !rv.IsValid();
}

}  // namespace bits
}  // namespace base
}  // namespace v8
