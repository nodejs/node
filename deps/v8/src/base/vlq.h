// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_VLQ_H_
#define V8_BASE_VLQ_H_

#include <limits>
#include <vector>

#include "src/base/memory.h"

namespace v8 {
namespace base {

static constexpr uint32_t kContinueShift = 7;
static constexpr uint32_t kContinueBit = 1 << kContinueShift;
static constexpr uint32_t kDataMask = kContinueBit - 1;

// Encodes an unsigned value using variable-length encoding and stores it using
// the passed process_byte function. The function should return a pointer to
// the byte that was written, so that VLQEncodeUnsigned can mutate it after
// writing it.
template <typename Function>
inline typename std::enable_if<
    std::is_same<decltype(std::declval<Function>()(0)), byte*>::value,
    void>::type
VLQEncodeUnsigned(Function&& process_byte, uint32_t value) {
  byte* written_byte = process_byte(value);
  if (value <= kDataMask) {
    // Value fits in first byte, early return.
    return;
  }
  do {
    // Turn on continuation bit in the byte we just wrote.
    *written_byte |= kContinueBit;
    value >>= kContinueShift;
    written_byte = process_byte(value);
  } while (value > kDataMask);
}

// Encodes value using variable-length encoding and stores it using the passed
// process_byte function.
template <typename Function>
inline typename std::enable_if<
    std::is_same<decltype(std::declval<Function>()(0)), byte*>::value,
    void>::type
VLQEncode(Function&& process_byte, int32_t value) {
  // This wouldn't handle kMinInt correctly if it ever encountered it.
  DCHECK_NE(value, std::numeric_limits<int32_t>::min());
  bool is_negative = value < 0;
  // Encode sign in least significant bit.
  uint32_t bits = static_cast<uint32_t>((is_negative ? -value : value) << 1) |
                  static_cast<uint32_t>(is_negative);
  VLQEncodeUnsigned(std::forward<Function>(process_byte), bits);
}

// Wrapper of VLQEncode for std::vector backed storage containers.
template <typename A>
inline void VLQEncode(std::vector<byte, A>* data, int32_t value) {
  VLQEncode(
      [data](byte value) {
        data->push_back(value);
        return &data->back();
      },
      value);
}

// Wrapper of VLQEncodeUnsigned for std::vector backed storage containers.
template <typename A>
inline void VLQEncodeUnsigned(std::vector<byte, A>* data, uint32_t value) {
  VLQEncodeUnsigned(
      [data](byte value) {
        data->push_back(value);
        return &data->back();
      },
      value);
}

// Decodes a variable-length encoded unsigned value from bytes returned by
// successive calls to the given function.
template <typename GetNextFunction>
inline typename std::enable_if<
    std::is_same<decltype(std::declval<GetNextFunction>()()), byte>::value,
    uint32_t>::type
VLQDecodeUnsigned(GetNextFunction&& get_next) {
  byte cur_byte = get_next();
  // Single byte fast path; no need to mask.
  if (cur_byte <= kDataMask) {
    return cur_byte;
  }
  uint32_t bits = cur_byte & kDataMask;
  for (int shift = kContinueShift; shift <= 32; shift += kContinueShift) {
    byte cur_byte = get_next();
    bits |= (cur_byte & kDataMask) << shift;
    if (cur_byte <= kDataMask) break;
  }
  return bits;
}

// Decodes a variable-length encoded unsigned value stored in contiguous memory
// starting at data_start + index, updating index to where the next encoded
// value starts.
inline uint32_t VLQDecodeUnsigned(byte* data_start, int* index) {
  return VLQDecodeUnsigned([&] { return data_start[(*index)++]; });
}

// Decodes a variable-length encoded value stored in contiguous memory starting
// at data_start + index, updating index to where the next encoded value starts.
inline int32_t VLQDecode(byte* data_start, int* index) {
  uint32_t bits = VLQDecodeUnsigned(data_start, index);
  bool is_negative = (bits & 1) == 1;
  int32_t result = bits >> 1;
  return is_negative ? -result : result;
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_VLQ_H_
