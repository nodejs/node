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
static constexpr uint32_t kContinueMask = 1 << kContinueShift;
static constexpr uint32_t kDataMask = kContinueMask - 1;

// Encodes an unsigned value using variable-length encoding and stores it using
// the passed process_byte function.
inline void VLQEncodeUnsigned(const std::function<void(byte)>& process_byte,
                              uint32_t value) {
  bool has_next;
  do {
    byte cur_byte = value & kDataMask;
    value >>= kContinueShift;
    has_next = value != 0;
    // The most significant bit is set when we are not done with the value yet.
    cur_byte |= static_cast<uint32_t>(has_next) << kContinueShift;
    process_byte(cur_byte);
  } while (has_next);
}

// Encodes value using variable-length encoding and stores it using the passed
// process_byte function.
inline void VLQEncode(const std::function<void(byte)>& process_byte,
                      int32_t value) {
  // This wouldn't handle kMinInt correctly if it ever encountered it.
  DCHECK_NE(value, std::numeric_limits<int32_t>::min());
  bool is_negative = value < 0;
  // Encode sign in least significant bit.
  uint32_t bits = static_cast<uint32_t>((is_negative ? -value : value) << 1) |
                  static_cast<uint32_t>(is_negative);
  VLQEncodeUnsigned(process_byte, bits);
}

// Wrapper of VLQEncode for std::vector backed storage containers.
template <typename A>
inline void VLQEncode(std::vector<byte, A>* data, int32_t value) {
  VLQEncode([data](byte value) { data->push_back(value); }, value);
}

// Wrapper of VLQEncodeUnsigned for std::vector backed storage containers.
template <typename A>
inline void VLQEncodeUnsigned(std::vector<byte, A>* data, uint32_t value) {
  VLQEncodeUnsigned([data](byte value) { data->push_back(value); }, value);
}

// Decodes a variable-length encoded unsigned value stored in contiguous memory
// starting at data_start + index, updating index to where the next encoded
// value starts.
inline uint32_t VLQDecodeUnsigned(byte* data_start, int* index) {
  uint32_t bits = 0;
  for (int shift = 0; true; shift += kContinueShift) {
    byte cur_byte = data_start[(*index)++];
    bits += (cur_byte & kDataMask) << shift;
    if ((cur_byte & kContinueMask) == 0) break;
  }
  return bits;
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
