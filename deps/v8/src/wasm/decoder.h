// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#include <memory>

#include "src/base/compiler-specific.h"
#include "src/flags.h"
#include "src/signature.h"
#include "src/utils.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone-containers.h"

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

  inline bool check(const byte* base, unsigned offset, unsigned length,
                    const char* msg) {
    DCHECK_GE(base, start_);
    if ((base + offset + length) > limit_) {
      error(base, base + offset, "%s", msg);
      return false;
    }
    return true;
  }

  // Reads a single 8-bit byte, reporting an error if out of bounds.
  inline uint8_t checked_read_u8(const byte* base, unsigned offset,
                                 const char* msg = "expected 1 byte") {
    return check(base, offset, 1, msg) ? base[offset] : 0;
  }

  // Reads 16-bit word, reporting an error if out of bounds.
  inline uint16_t checked_read_u16(const byte* base, unsigned offset,
                                   const char* msg = "expected 2 bytes") {
    return check(base, offset, 2, msg) ? read_u16(base + offset) : 0;
  }

  // Reads 32-bit word, reporting an error if out of bounds.
  inline uint32_t checked_read_u32(const byte* base, unsigned offset,
                                   const char* msg = "expected 4 bytes") {
    return check(base, offset, 4, msg) ? read_u32(base + offset) : 0;
  }

  // Reads 64-bit word, reporting an error if out of bounds.
  inline uint64_t checked_read_u64(const byte* base, unsigned offset,
                                   const char* msg = "expected 8 bytes") {
    return check(base, offset, 8, msg) ? read_u64(base + offset) : 0;
  }

  // Reads a variable-length unsigned integer (little endian).
  uint32_t checked_read_u32v(const byte* base, unsigned offset,
                             unsigned* length,
                             const char* msg = "expected LEB32") {
    return checked_read_leb<uint32_t, false>(base, offset, length, msg);
  }

  // Reads a variable-length signed integer (little endian).
  int32_t checked_read_i32v(const byte* base, unsigned offset, unsigned* length,
                            const char* msg = "expected SLEB32") {
    uint32_t result =
        checked_read_leb<uint32_t, true>(base, offset, length, msg);
    if (*length == 5) return bit_cast<int32_t>(result);
    if (*length > 0) {
      int shift = 32 - 7 * *length;
      // Perform sign extension.
      return bit_cast<int32_t>(result << shift) >> shift;
    }
    return 0;
  }

  // Reads a variable-length unsigned integer (little endian).
  uint64_t checked_read_u64v(const byte* base, unsigned offset,
                             unsigned* length,
                             const char* msg = "expected LEB64") {
    return checked_read_leb<uint64_t, false>(base, offset, length, msg);
  }

  // Reads a variable-length signed integer (little endian).
  int64_t checked_read_i64v(const byte* base, unsigned offset, unsigned* length,
                            const char* msg = "expected SLEB64") {
    uint64_t result =
        checked_read_leb<uint64_t, true>(base, offset, length, msg);
    if (*length == 10) return bit_cast<int64_t>(result);
    if (*length > 0) {
      int shift = 64 - 7 * *length;
      // Perform sign extension.
      return bit_cast<int64_t>(result << shift) >> shift;
    }
    return 0;
  }

  // Reads a single 16-bit unsigned integer (little endian).
  inline uint16_t read_u16(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 2) <= end_);
    return ReadLittleEndianValue<uint16_t>(ptr);
  }

  // Reads a single 32-bit unsigned integer (little endian).
  inline uint32_t read_u32(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 4) <= end_);
    return ReadLittleEndianValue<uint32_t>(ptr);
  }

  // Reads a single 64-bit unsigned integer (little endian).
  inline uint64_t read_u64(const byte* ptr) {
    DCHECK(ptr >= start_ && (ptr + 8) <= end_);
    return ReadLittleEndianValue<uint64_t>(ptr);
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
  uint32_t consume_u32v(const char* name = nullptr) {
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

      int length = static_cast<int>(pc_ - pos);
      if (pc_ == end && (b & 0x80)) {
        error(pc_ - 1, "varint too large");
      } else if (length == 0) {
        error(pc_, "varint of length 0");
      } else {
        TRACE("= %u\n", result);
      }
      return result;
    }
    return traceOffEnd<uint32_t>();
  }

  // Consume {size} bytes and send them to the bit bucket, advancing {pc_}.
  void consume_bytes(int size) {
    TRACE("  +%d  %-20s: %d bytes\n", static_cast<int>(pc_ - start_), "skip",
          size);
    if (checkAvailable(size)) {
      pc_ += size;
    } else {
      pc_ = limit_;
    }
  }

  // Consume {size} bytes and send them to the bit bucket, advancing {pc_}.
  void consume_bytes(uint32_t size, const char* name = "skip") {
    TRACE("  +%d  %-20s: %d bytes\n", static_cast<int>(pc_ - start_), name,
          size);
    if (checkAvailable(size)) {
      pc_ += size;
    } else {
      pc_ = limit_;
    }
  }

  // Check that at least {size} bytes exist between {pc_} and {limit_}.
  bool checkAvailable(int size) {
    intptr_t pc_overflow_value = std::numeric_limits<intptr_t>::max() - size;
    if (size < 0 || (intptr_t)pc_ > pc_overflow_value) {
      error(pc_, nullptr, "reading %d bytes would underflow/overflow", size);
      return false;
    } else if (pc_ < start_ || limit_ < (pc_ + size)) {
      error(pc_, nullptr, "expected %d bytes, fell off end", size);
      return false;
    } else {
      return true;
    }
  }

  void error(const char* msg) { error(pc_, nullptr, "%s", msg); }

  void error(const byte* pc, const char* msg) { error(pc, nullptr, "%s", msg); }

  // Sets internal error state.
  void PRINTF_FORMAT(4, 5)
      error(const byte* pc, const byte* pt, const char* format, ...) {
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
      error_msg_.reset(buffer);
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
      TRACE("Result error: %s\n", error_msg_.get());
      result.error_code = kError;
      result.start = start_;
      result.error_pc = error_pc_;
      result.error_pt = error_pt_;
      // transfer ownership of the error to the result.
      result.error_msg.reset(error_msg_.release());
    } else {
      result.error_code = kSuccess;
    }
    result.val = std::move(val);
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
    error_msg_.reset();
  }

  bool ok() const { return error_pc_ == nullptr; }
  bool failed() const { return !!error_msg_; }
  bool more() const { return pc_ < limit_; }

  const byte* start() { return start_; }
  const byte* pc() { return pc_; }
  uint32_t pc_offset() { return static_cast<uint32_t>(pc_ - start_); }

 protected:
  const byte* start_;
  const byte* pc_;
  const byte* limit_;
  const byte* end_;
  const byte* error_pc_;
  const byte* error_pt_;
  std::unique_ptr<char[]> error_msg_;

 private:
  template <typename IntType, bool is_signed>
  IntType checked_read_leb(const byte* base, unsigned offset, unsigned* length,
                           const char* msg) {
    if (!check(base, offset, 1, msg)) {
      *length = 0;
      return 0;
    }

    const int kMaxLength = (sizeof(IntType) * 8 + 6) / 7;
    const byte* ptr = base + offset;
    const byte* end = ptr + kMaxLength;
    if (end > limit_) end = limit_;
    int shift = 0;
    byte b = 0;
    IntType result = 0;
    while (ptr < end) {
      b = *ptr++;
      result = result | (static_cast<IntType>(b & 0x7F) << shift);
      if ((b & 0x80) == 0) break;
      shift += 7;
    }
    DCHECK_LE(ptr - (base + offset), kMaxLength);
    *length = static_cast<unsigned>(ptr - (base + offset));
    if (ptr == end) {
      // Check there are no bits set beyond the bitwidth of {IntType}.
      const int kExtraBits = (1 + kMaxLength * 7) - (sizeof(IntType) * 8);
      const byte kExtraBitsMask =
          static_cast<byte>((0xFF << (8 - kExtraBits)) & 0xFF);
      int extra_bits_value;
      if (is_signed) {
        // A signed-LEB128 must sign-extend the final byte, excluding its
        // most-signifcant bit. e.g. for a 32-bit LEB128:
        //   kExtraBits = 4
        //   kExtraBitsMask = 0xf0
        // If b is 0x0f, the value is negative, so extra_bits_value is 0x70.
        // If b is 0x03, the value is positive, so extra_bits_value is 0x00.
        extra_bits_value = (static_cast<int8_t>(b << kExtraBits) >> 8) &
                           kExtraBitsMask & ~0x80;
      } else {
        extra_bits_value = 0;
      }
      if (*length == kMaxLength && (b & kExtraBitsMask) != extra_bits_value) {
        error(base, ptr, "extra bits in varint");
        return 0;
      }
      if ((b & 0x80) != 0) {
        error(base, ptr, "%s", msg);
        return 0;
      }
    }
    return result;
  }
};

#undef TRACE
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_DECODER_H_
