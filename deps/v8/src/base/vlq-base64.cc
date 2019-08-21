// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <limits>

#include "src/base/logging.h"
#include "src/base/vlq-base64.h"

namespace v8 {
namespace base {

namespace {
constexpr int8_t kCharToDigit[] = {
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   0x3e, -1,   -1,   -1,   0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, -1,   -1,
    -1,   -1,   -1,   -1,   -1,   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, -1,   -1,   -1,   -1,   -1,
    -1,   0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, -1,   -1,   -1,   -1,   -1};

constexpr uint32_t kContinueShift = 5;
constexpr uint32_t kContinueMask = 1 << kContinueShift;
constexpr uint32_t kDataMask = kContinueMask - 1;

int8_t charToDigitDecode(uint8_t c) { return c < 128u ? kCharToDigit[c] : -1; }
}  // namespace

int8_t charToDigitDecodeForTesting(uint8_t c) { return charToDigitDecode(c); }

int32_t VLQBase64Decode(const char* start, size_t sz, size_t* pos) {
  uint32_t res = 0;
  uint64_t shift = 0;
  int32_t digit;

  do {
    if (*pos >= sz) {
      return std::numeric_limits<int32_t>::min();
    }
    digit = static_cast<int>(charToDigitDecode(start[*pos]));
    bool is_last_byte = (shift + kContinueShift >= 32);
    if (digit == -1 || (is_last_byte && (digit >> 2) != 0)) {
      return std::numeric_limits<int32_t>::min();
    }
    res += (digit & kDataMask) << shift;
    shift += kContinueShift;
    (*pos)++;
  } while (digit & kContinueMask);
  return (res & 1) ? -static_cast<int32_t>(res >> 1) : (res >> 1);
}
}  // namespace base
}  // namespace v8
