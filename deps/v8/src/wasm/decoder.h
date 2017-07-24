// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#include <cstdarg>
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
#define TRACE_IF(cond, ...)                                     \
  do {                                                          \
    if (FLAG_trace_wasm_decoder && (cond)) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#define TRACE_IF(...)
#endif

// A helper utility to decode bytes, integers, fields, varints, etc, from
// a buffer of bytes.
class Decoder {
 public:
  Decoder(const byte* start, const byte* end)
      : start_(start), pc_(start), end_(end), error_pc_(nullptr) {}
  Decoder(const byte* start, const byte* pc, const byte* end)
      : start_(start), pc_(pc), end_(end), error_pc_(nullptr) {}

  virtual ~Decoder() {}

  inline bool check(const byte* pc, unsigned length, const char* msg) {
    DCHECK_LE(start_, pc);
    if (V8_UNLIKELY(pc + length > end_)) {
      error(pc, msg);
      return false;
    }
    return true;
  }

  // Reads an 8-bit unsigned integer.
  template <bool checked>
  inline uint8_t read_u8(const byte* pc, const char* msg = "expected 1 byte") {
    return read_little_endian<uint8_t, checked>(pc, msg);
  }

  // Reads a 16-bit unsigned integer (little endian).
  template <bool checked>
  inline uint16_t read_u16(const byte* pc,
                           const char* msg = "expected 2 bytes") {
    return read_little_endian<uint16_t, checked>(pc, msg);
  }

  // Reads a 32-bit unsigned integer (little endian).
  template <bool checked>
  inline uint32_t read_u32(const byte* pc,
                           const char* msg = "expected 4 bytes") {
    return read_little_endian<uint32_t, checked>(pc, msg);
  }

  // Reads a 64-bit unsigned integer (little endian).
  template <bool checked>
  inline uint64_t read_u64(const byte* pc,
                           const char* msg = "expected 8 bytes") {
    return read_little_endian<uint64_t, checked>(pc, msg);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <bool checked>
  uint32_t read_u32v(const byte* pc, unsigned* length,
                     const char* name = "LEB32") {
    return read_leb<uint32_t, checked, false, false>(pc, length, name);
  }

  // Reads a variable-length signed integer (little endian).
  template <bool checked>
  int32_t read_i32v(const byte* pc, unsigned* length,
                    const char* name = "signed LEB32") {
    return read_leb<int32_t, checked, false, false>(pc, length, name);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <bool checked>
  uint64_t read_u64v(const byte* pc, unsigned* length,
                     const char* name = "LEB64") {
    return read_leb<uint64_t, checked, false, false>(pc, length, name);
  }

  // Reads a variable-length signed integer (little endian).
  template <bool checked>
  int64_t read_i64v(const byte* pc, unsigned* length,
                    const char* name = "signed LEB64") {
    return read_leb<int64_t, checked, false, false>(pc, length, name);
  }

  // Reads a 8-bit unsigned integer (byte) and advances {pc_}.
  uint8_t consume_u8(const char* name = "uint8_t") {
    return consume_little_endian<uint8_t>(name);
  }

  // Reads a 16-bit unsigned integer (little endian) and advances {pc_}.
  uint16_t consume_u16(const char* name = "uint16_t") {
    return consume_little_endian<uint16_t>(name);
  }

  // Reads a single 32-bit unsigned integer (little endian) and advances {pc_}.
  uint32_t consume_u32(const char* name = "uint32_t") {
    return consume_little_endian<uint32_t>(name);
  }

  // Reads a LEB128 variable-length unsigned 32-bit integer and advances {pc_}.
  uint32_t consume_u32v(const char* name = nullptr) {
    unsigned length = 0;
    return read_leb<uint32_t, true, true, true>(pc_, &length, name);
  }

  // Reads a LEB128 variable-length signed 32-bit integer and advances {pc_}.
  int32_t consume_i32v(const char* name = nullptr) {
    unsigned length = 0;
    return read_leb<int32_t, true, true, true>(pc_, &length, name);
  }

  // Consume {size} bytes and send them to the bit bucket, advancing {pc_}.
  void consume_bytes(uint32_t size, const char* name = "skip") {
    // Only trace if the name is not null.
    TRACE_IF(name, "  +%d  %-20s: %d bytes\n", static_cast<int>(pc_ - start_),
             name, size);
    if (checkAvailable(size)) {
      pc_ += size;
    } else {
      pc_ = end_;
    }
  }

  // Check that at least {size} bytes exist between {pc_} and {end_}.
  bool checkAvailable(int size) {
    intptr_t pc_overflow_value = std::numeric_limits<intptr_t>::max() - size;
    if (size < 0 || (intptr_t)pc_ > pc_overflow_value) {
      errorf(pc_, "reading %d bytes would underflow/overflow", size);
      return false;
    } else if (pc_ < start_ || end_ < (pc_ + size)) {
      errorf(pc_, "expected %d bytes, fell off end", size);
      return false;
    } else {
      return true;
    }
  }

  void error(const char* msg) { errorf(pc_, "%s", msg); }

  void error(const byte* pc, const char* msg) { errorf(pc, "%s", msg); }

  // Sets internal error state.
  void PRINTF_FORMAT(3, 4) errorf(const byte* pc, const char* format, ...) {
    // Only report the first error.
    if (!ok()) return;
#if DEBUG
    if (FLAG_wasm_break_on_decoder_error) {
      base::OS::DebugBreak();
    }
#endif
    constexpr int kMaxErrorMsg = 256;
    EmbeddedVector<char, kMaxErrorMsg> buffer;
    va_list arguments;
    va_start(arguments, format);
    int len = VSNPrintF(buffer, format, arguments);
    CHECK_LT(0, len);
    va_end(arguments);
    error_msg_.assign(buffer.start(), len);
    error_pc_ = pc;
    onFirstError();
  }

  // Behavior triggered on first error, overridden in subclasses.
  virtual void onFirstError() {}

  // Debugging helper to print a bytes range as hex bytes.
  void traceByteRange(const byte* start, const byte* end) {
    DCHECK_LE(start, end);
    for (const byte* p = start; p < end; ++p) TRACE("%02x ", *p);
  }

  // Debugging helper to print bytes up to the end.
  void traceOffEnd() {
    traceByteRange(pc_, end_);
    TRACE("<end>\n");
  }

  // Converts the given value to a {Result}, copying the error if necessary.
  template <typename T, typename U = typename std::remove_reference<T>::type>
  Result<U> toResult(T&& val) {
    Result<U> result(std::forward<T>(val));
    if (failed()) {
      // The error message must not be empty, otherwise Result::failed() will be
      // false.
      DCHECK(!error_msg_.empty());
      TRACE("Result error: %s\n", error_msg_.c_str());
      DCHECK_GE(error_pc_, start_);
      result.error_offset = static_cast<uint32_t>(error_pc_ - start_);
      result.error_msg = std::move(error_msg_);
    }
    return result;
  }

  // Resets the boundaries of this decoder.
  void Reset(const byte* start, const byte* end) {
    start_ = start;
    pc_ = start;
    end_ = end;
    error_pc_ = nullptr;
    error_msg_.clear();
  }

  bool ok() const { return error_msg_.empty(); }
  bool failed() const { return !ok(); }
  bool more() const { return pc_ < end_; }

  const byte* start() const { return start_; }
  const byte* pc() const { return pc_; }
  uint32_t pc_offset() const { return static_cast<uint32_t>(pc_ - start_); }
  const byte* end() const { return end_; }

 protected:
  const byte* start_;
  const byte* pc_;
  const byte* end_;
  const byte* error_pc_;
  std::string error_msg_;

 private:
  template <typename IntType, bool checked>
  inline IntType read_little_endian(const byte* pc, const char* msg) {
    if (!checked) {
      DCHECK(check(pc, sizeof(IntType), msg));
    } else if (!check(pc, sizeof(IntType), msg)) {
      return IntType{0};
    }
    return ReadLittleEndianValue<IntType>(pc);
  }

  template <typename IntType>
  inline IntType consume_little_endian(const char* name) {
    TRACE("  +%d  %-20s: ", static_cast<int>(pc_ - start_), name);
    if (!checkAvailable(sizeof(IntType))) {
      traceOffEnd();
      pc_ = end_;
      return IntType{0};
    }
    IntType val = read_little_endian<IntType, false>(pc_, name);
    traceByteRange(pc_, pc_ + sizeof(IntType));
    TRACE("= %d\n", val);
    pc_ += sizeof(IntType);
    return val;
  }

  template <typename IntType, bool checked, bool advance_pc, bool trace>
  inline IntType read_leb(const byte* pc, unsigned* length,
                          const char* name = "varint") {
    DCHECK_IMPLIES(advance_pc, pc == pc_);
    constexpr bool is_signed = std::is_signed<IntType>::value;
    TRACE_IF(trace, "  +%d  %-20s: ", static_cast<int>(pc - start_), name);
    constexpr int kMaxLength = (sizeof(IntType) * 8 + 6) / 7;
    const byte* ptr = pc;
    const byte* end = Min(end_, ptr + kMaxLength);
    // The end variable is only used if checked == true. MSVC recognizes this.
    USE(end);
    int shift = 0;
    byte b = 0;
    IntType result = 0;
    do {
      if (checked && V8_UNLIKELY(ptr >= end)) {
        TRACE_IF(trace,
                 ptr == pc + kMaxLength ? "<length overflow> " : "<end> ");
        errorf(ptr, "expected %s", name);
        result = 0;
        break;
      }
      DCHECK_GT(end, ptr);
      b = *ptr++;
      TRACE_IF(trace, "%02x ", b);
      result = result | ((static_cast<IntType>(b) & 0x7F) << shift);
      shift += 7;
    } while (b & 0x80);
    DCHECK_LE(ptr - pc, kMaxLength);
    *length = static_cast<unsigned>(ptr - pc);
    if (advance_pc) pc_ = ptr;
    if (*length == kMaxLength) {
      // A signed-LEB128 must sign-extend the final byte, excluding its
      // most-significant bit; e.g. for a 32-bit LEB128:
      //   kExtraBits = 4  (== 32 - (5-1) * 7)
      // For unsigned values, the extra bits must be all zero.
      // For signed values, the extra bits *plus* the most significant bit must
      // either be 0, or all ones.
      constexpr int kExtraBits = (sizeof(IntType) * 8) - ((kMaxLength - 1) * 7);
      constexpr int kSignExtBits = kExtraBits - (is_signed ? 1 : 0);
      const byte checked_bits = b & (0xFF << kSignExtBits);
      constexpr byte kSignExtendedExtraBits = 0x7f & (0xFF << kSignExtBits);
      bool valid_extra_bits =
          checked_bits == 0 ||
          (is_signed && checked_bits == kSignExtendedExtraBits);
      if (!checked) {
        DCHECK(valid_extra_bits);
      } else if (!valid_extra_bits) {
        error(ptr, "extra bits in varint");
        result = 0;
      }
    }
    if (is_signed && *length < kMaxLength) {
      int sign_ext_shift = 8 * sizeof(IntType) - shift;
      // Perform sign extension.
      result = (result << sign_ext_shift) >> sign_ext_shift;
    }
    if (trace && is_signed) {
      TRACE("= %" PRIi64 "\n", static_cast<int64_t>(result));
    } else if (trace) {
      TRACE("= %" PRIu64 "\n", static_cast<uint64_t>(result));
    }
    return result;
  }
};

#undef TRACE
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_DECODER_H_
