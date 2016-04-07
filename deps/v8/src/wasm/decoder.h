// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#include "src/base/smart-pointers.h"
#include "src/flags.h"
#include "src/signature.h"
#include "src/wasm/wasm-result.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

#if DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM)
#define UNALIGNED_ACCESS_OK 1
#else
#define UNALIGNED_ACCESS_OK 0
#endif

// A helper utility to decode bytes, integers, fields, varints, etc, from
// a buffer of bytes.
class Decoder {
 public:
  Decoder(const byte* start, const byte* end)
      : start_(start),
        pc_(start),
        limit_(end),
        end_(end),
        error_pc_(nullptr),
        error_pt_(nullptr) {}

  virtual ~Decoder() {}

  inline bool check(const byte* base, int offset, int length, const char* msg) {
    DCHECK_GE(base, start_);
    if ((base + offset + length) > limit_) {
      error(base, base + offset, msg);
      return false;
    }
    return true;
  }

  // Reads a single 8-bit byte, reporting an error if out of bounds.
  inline uint8_t checked_read_u8(const byte* base, int offset,
                                 const char* msg = "expected 1 byte") {
    return check(base, offset, 1, msg) ? base[offset] : 0;
  }

  // Reads 16-bit word, reporting an error if out of bounds.
  inline uint16_t checked_read_u16(const byte* base, int offset,
                                   const char* msg = "expected 2 bytes") {
    return check(base, offset, 2, msg) ? read_u16(base + offset) : 0;
  }

  // Reads 32-bit word, reporting an error if out of bounds.
  inline uint32_t checked_read_u32(const byte* base, int offset,
                                   const char* msg = "expected 4 bytes") {
    return check(base, offset, 4, msg) ? read_u32(base + offset) : 0;
  }

  // Reads 64-bit word, reporting an error if out of bounds.
  inline uint64_t checked_read_u64(const byte* base, int offset,
                                   const char* msg = "expected 8 bytes") {
    return check(base, offset, 8, msg) ? read_u64(base + offset) : 0;
  }

  uint32_t checked_read_u32v(const byte* base, int offset, int* length,
                             const char* msg = "expected LEB128") {
    if (!check(base, offset, 1, msg)) {
      *length = 0;
      return 0;
    }

    const ptrdiff_t kMaxDiff = 5;  // maximum 5 bytes.
    const byte* ptr = base + offset;
    const byte* end = ptr + kMaxDiff;
    if (end > limit_) end = limit_;
    int shift = 0;
    byte b = 0;
    uint32_t result = 0;
    while (ptr < end) {
      b = *ptr++;
      result = result | ((b & 0x7F) << shift);
      if ((b & 0x80) == 0) break;
      shift += 7;
    }
    DCHECK_LE(ptr - (base + offset), kMaxDiff);
    *length = static_cast<int>(ptr - (base + offset));
    if (ptr == end && (b & 0x80)) {
      error(base, ptr, msg);
      return 0;
    }
    return result;
  }

  // Reads a single 16-bit unsigned integer (little endian).
  inline uint16_t read_u16(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 2) <= end_);
#if V8_TARGET_LITTLE_ENDIAN && UNALIGNED_ACCESS_OK
    return *reinterpret_cast<const uint16_t*>(ptr);
#else
    uint16_t b0 = ptr[0];
    uint16_t b1 = ptr[1];
    return (b1 << 8) | b0;
#endif
  }

  // Reads a single 32-bit unsigned integer (little endian).
  inline uint32_t read_u32(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 4) <= end_);
#if V8_TARGET_LITTLE_ENDIAN && UNALIGNED_ACCESS_OK
    return *reinterpret_cast<const uint32_t*>(ptr);
#else
    uint32_t b0 = ptr[0];
    uint32_t b1 = ptr[1];
    uint32_t b2 = ptr[2];
    uint32_t b3 = ptr[3];
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
#endif
  }

  // Reads a single 64-bit unsigned integer (little endian).
  inline uint64_t read_u64(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 8) <= end_);
#if V8_TARGET_LITTLE_ENDIAN && UNALIGNED_ACCESS_OK
    return *reinterpret_cast<const uint64_t*>(ptr);
#else
    uint32_t b0 = ptr[0];
    uint32_t b1 = ptr[1];
    uint32_t b2 = ptr[2];
    uint32_t b3 = ptr[3];
    uint32_t low = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    uint32_t b4 = ptr[4];
    uint32_t b5 = ptr[5];
    uint32_t b6 = ptr[6];
    uint32_t b7 = ptr[7];
    uint64_t high = (b7 << 24) | (b6 << 16) | (b5 << 8) | b4;
    return (high << 32) | low;
#endif
  }

  // Reads a 8-bit unsigned integer (byte) and advances {pc_}.
  uint8_t consume_u8(const char* name = nullptr) {
    TRACE("  +%d  %-20s: ", static_cast<int>(pc_ - start_),
          name ? name : "uint8_t");
    if (checkAvailable(1)) {
      byte val = *(pc_++);
      TRACE("%02x = %d\n", val, val);
      return val;
    }
    return traceOffEnd<uint8_t>();
  }

  // Reads a 16-bit unsigned integer (little endian) and advances {pc_}.
  uint16_t consume_u16(const char* name = nullptr) {
    TRACE("  +%d  %-20s: ", static_cast<int>(pc_ - start_),
          name ? name : "uint16_t");
    if (checkAvailable(2)) {
      uint16_t val = read_u16(pc_);
      TRACE("%02x %02x = %d\n", pc_[0], pc_[1], val);
      pc_ += 2;
      return val;
    }
    return traceOffEnd<uint16_t>();
  }

  // Reads a single 32-bit unsigned integer (little endian) and advances {pc_}.
  uint32_t consume_u32(const char* name = nullptr) {
    TRACE("  +%d  %-20s: ", static_cast<int>(pc_ - start_),
          name ? name : "uint32_t");
    if (checkAvailable(4)) {
      uint32_t val = read_u32(pc_);
      TRACE("%02x %02x %02x %02x = %u\n", pc_[0], pc_[1], pc_[2], pc_[3], val);
      pc_ += 4;
      return val;
    }
    return traceOffEnd<uint32_t>();
  }

  // Reads a LEB128 variable-length 32-bit integer and advances {pc_}.
  uint32_t consume_u32v(int* length, const char* name = nullptr) {
    TRACE("  +%d  %-20s: ", static_cast<int>(pc_ - start_),
          name ? name : "varint");

    if (checkAvailable(1)) {
      const byte* pos = pc_;
      const byte* end = pc_ + 5;
      if (end > limit_) end = limit_;

      uint32_t result = 0;
      int shift = 0;
      byte b = 0;
      while (pc_ < end) {
        b = *pc_++;
        TRACE("%02x ", b);
        result = result | ((b & 0x7F) << shift);
        if ((b & 0x80) == 0) break;
        shift += 7;
      }

      *length = static_cast<int>(pc_ - pos);
      if (pc_ == end && (b & 0x80)) {
        error(pc_ - 1, "varint too large");
      } else {
        TRACE("= %u\n", result);
      }
      return result;
    }
    return traceOffEnd<uint32_t>();
  }

  // Check that at least {size} bytes exist between {pc_} and {limit_}.
  bool checkAvailable(int size) {
    if (pc_ < start_ || (pc_ + size) > limit_) {
      error(pc_, nullptr, "expected %d bytes, fell off end", size);
      return false;
    } else {
      return true;
    }
  }

  bool RangeOk(const byte* pc, int length) {
    if (pc < start_ || pc_ >= limit_) return false;
    if ((pc + length) >= limit_) return false;
    return true;
  }

  void error(const char* msg) { error(pc_, nullptr, msg); }

  void error(const byte* pc, const char* msg) { error(pc, nullptr, msg); }

  // Sets internal error state.
  void error(const byte* pc, const byte* pt, const char* format, ...) {
    if (ok()) {
#if DEBUG
      if (FLAG_wasm_break_on_decoder_error) {
        base::OS::DebugBreak();
      }
#endif
      const int kMaxErrorMsg = 256;
      char* buffer = new char[kMaxErrorMsg];
      va_list arguments;
      va_start(arguments, format);
      base::OS::VSNPrintF(buffer, kMaxErrorMsg - 1, format, arguments);
      va_end(arguments);
      error_msg_.Reset(buffer);
      error_pc_ = pc;
      error_pt_ = pt;
      onFirstError();
    }
  }

  // Behavior triggered on first error, overridden in subclasses.
  virtual void onFirstError() {}

  // Debugging helper to print bytes up to the end.
  template <typename T>
  T traceOffEnd() {
    T t = 0;
    for (const byte* ptr = pc_; ptr < limit_; ptr++) {
      TRACE("%02x ", *ptr);
    }
    TRACE("<end>\n");
    pc_ = limit_;
    return t;
  }

  // Converts the given value to a {Result}, copying the error if necessary.
  template <typename T>
  Result<T> toResult(T val) {
    Result<T> result;
    if (error_pc_) {
      result.error_code = kError;
      result.start = start_;
      result.error_pc = error_pc_;
      result.error_pt = error_pt_;
      result.error_msg = error_msg_;
      error_msg_.Reset(nullptr);
    } else {
      result.error_code = kSuccess;
    }
    result.val = val;
    return result;
  }

  // Resets the boundaries of this decoder.
  void Reset(const byte* start, const byte* end) {
    start_ = start;
    pc_ = start;
    limit_ = end;
    end_ = end;
    error_pc_ = nullptr;
    error_pt_ = nullptr;
    error_msg_.Reset(nullptr);
  }

  bool ok() const { return error_pc_ == nullptr; }
  bool failed() const { return error_pc_ != nullptr; }

 protected:
  const byte* start_;
  const byte* pc_;
  const byte* limit_;
  const byte* end_;
  const byte* error_pc_;
  const byte* error_pt_;
  base::SmartArrayPointer<char> error_msg_;
};

#undef TRACE
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_DECODER_H_
