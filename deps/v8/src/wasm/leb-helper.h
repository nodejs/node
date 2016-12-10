// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_LEB_HELPER_H_
#define V8_WASM_LEB_HELPER_H_

namespace v8 {
namespace internal {
namespace wasm {

static const size_t kPaddedVarInt32Size = 5;
static const size_t kMaxVarInt32Size = 5;

class LEBHelper {
 public:
  // Write a 32-bit unsigned LEB to {dest}, updating {dest} to point after
  // the last uint8_t written. No safety checks.
  static void write_u32v(uint8_t** dest, uint32_t val) {
    while (val >= 0x80) {
      *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
      val >>= 7;
    }
    *((*dest)++) = static_cast<uint8_t>(val & 0x7F);
  }

  // Write a 32-bit signed LEB to {dest}, updating {dest} to point after
  // the last uint8_t written. No safety checks.
  static void write_i32v(uint8_t** dest, int32_t val) {
    if (val >= 0) {
      while (val >= 0x40) {  // prevent sign extension.
        *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
        val >>= 7;
      }
      *((*dest)++) = static_cast<uint8_t>(val & 0xFF);
    } else {
      while ((val >> 6) != -1) {
        *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
        val >>= 7;
      }
      *((*dest)++) = static_cast<uint8_t>(val & 0x7F);
    }
  }

  // Write a 64-bit unsigned LEB to {dest}, updating {dest} to point after
  // the last uint8_t written. No safety checks.
  static void write_u64v(uint8_t** dest, uint64_t val) {
    while (val >= 0x80) {
      *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
      val >>= 7;
    }
    *((*dest)++) = static_cast<uint8_t>(val & 0x7F);
  }

  // Write a 64-bit signed LEB to {dest}, updating {dest} to point after
  // the last uint8_t written. No safety checks.
  static void write_i64v(uint8_t** dest, int64_t val) {
    if (val >= 0) {
      while (val >= 0x40) {  // prevent sign extension.
        *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
        val >>= 7;
      }
      *((*dest)++) = static_cast<uint8_t>(val & 0xFF);
    } else {
      while ((val >> 6) != -1) {
        *((*dest)++) = static_cast<uint8_t>(0x80 | (val & 0x7F));
        val >>= 7;
      }
      *((*dest)++) = static_cast<uint8_t>(val & 0x7F);
    }
  }

  // TODO(titzer): move core logic for decoding LEBs from decoder.h to here.

  // Compute the size of {val} if emitted as an LEB32.
  static inline size_t sizeof_u32v(size_t val) {
    size_t size = 0;
    do {
      size++;
      val = val >> 7;
    } while (val > 0);
    return size;
  }

  // Compute the size of {val} if emitted as an LEB32.
  static inline size_t sizeof_i32v(int32_t val) {
    size_t size = 1;
    if (val >= 0) {
      while (val >= 0x40) {  // prevent sign extension.
        size++;
        val >>= 7;
      }
    } else {
      while ((val >> 6) != -1) {
        size++;
        val >>= 7;
      }
    }
    return size;
  }

  // Compute the size of {val} if emitted as an unsigned LEB64.
  static inline size_t sizeof_u64v(uint64_t val) {
    size_t size = 0;
    do {
      size++;
      val = val >> 7;
    } while (val > 0);
    return size;
  }

  // Compute the size of {val} if emitted as a signed LEB64.
  static inline size_t sizeof_i64v(int64_t val) {
    size_t size = 1;
    if (val >= 0) {
      while (val >= 0x40) {  // prevent sign extension.
        size++;
        val >>= 7;
      }
    } else {
      while ((val >> 6) != -1) {
        size++;
        val >>= 7;
      }
    }
    return size;
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_LEB_HELPER_H_
