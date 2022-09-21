// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#include <cinttypes>
#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/base/memory.h"
#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE(...)                                        \
  do {                                                    \
    if (v8_flags.trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#define TRACE_IF(cond, ...)                                         \
  do {                                                              \
    if (v8_flags.trace_wasm_decoder && (cond)) PrintF(__VA_ARGS__); \
  } while (false)

// A {DecodeResult} only stores the failure / success status, but no data.
using DecodeResult = VoidResult;

// A helper utility to decode bytes, integers, fields, varints, etc, from
// a buffer of bytes.
class Decoder {
 public:
  // {ValidateFlag} can be used in a boolean manner ({if (!validate) ...}).
  enum ValidateFlag : int8_t {
    kNoValidation = 0,   // Don't run validation, assume valid input.
    kBooleanValidation,  // Run validation but only store a generic error.
    kFullValidation      // Run full validation with error message and location.
  };

  enum TraceFlag : bool { kTrace = true, kNoTrace = false };

  Decoder(const byte* start, const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, start, end, buffer_offset) {}
  explicit Decoder(const base::Vector<const byte> bytes,
                   uint32_t buffer_offset = 0)
      : Decoder(bytes.begin(), bytes.begin() + bytes.length(), buffer_offset) {}
  Decoder(const byte* start, const byte* pc, const byte* end,
          uint32_t buffer_offset = 0)
      : start_(start), pc_(pc), end_(end), buffer_offset_(buffer_offset) {
    DCHECK_LE(start, pc);
    DCHECK_LE(pc, end);
    DCHECK_EQ(static_cast<uint32_t>(end - start), end - start);
  }

  virtual ~Decoder() = default;

  // Ensures there are at least {length} bytes left to read, starting at {pc}.
  bool validate_size(const byte* pc, uint32_t length, const char* msg) {
    DCHECK_LE(start_, pc);
    if (V8_UNLIKELY(pc > end_ || length > static_cast<uint32_t>(end_ - pc))) {
      error(pc, msg);
      return false;
    }
    return true;
  }

  // Reads an 8-bit unsigned integer.
  template <ValidateFlag validate>
  uint8_t read_u8(const byte* pc, const char* msg = "expected 1 byte") {
    return read_little_endian<uint8_t, validate>(pc, msg);
  }

  // Reads a 16-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  uint16_t read_u16(const byte* pc, const char* msg = "expected 2 bytes") {
    return read_little_endian<uint16_t, validate>(pc, msg);
  }

  // Reads a 32-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  uint32_t read_u32(const byte* pc, const char* msg = "expected 4 bytes") {
    return read_little_endian<uint32_t, validate>(pc, msg);
  }

  // Reads a 64-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  uint64_t read_u64(const byte* pc, const char* msg = "expected 8 bytes") {
    return read_little_endian<uint64_t, validate>(pc, msg);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <ValidateFlag validate>
  uint32_t read_u32v(const byte* pc, uint32_t* length,
                     const char* name = "LEB32") {
    return read_leb<uint32_t, validate, kNoTrace>(pc, length, name);
  }

  // Reads a variable-length signed integer (little endian).
  template <ValidateFlag validate>
  int32_t read_i32v(const byte* pc, uint32_t* length,
                    const char* name = "signed LEB32") {
    return read_leb<int32_t, validate, kNoTrace>(pc, length, name);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <ValidateFlag validate>
  uint64_t read_u64v(const byte* pc, uint32_t* length,
                     const char* name = "LEB64") {
    return read_leb<uint64_t, validate, kNoTrace>(pc, length, name);
  }

  // Reads a variable-length signed integer (little endian).
  template <ValidateFlag validate>
  int64_t read_i64v(const byte* pc, uint32_t* length,
                    const char* name = "signed LEB64") {
    return read_leb<int64_t, validate, kNoTrace>(pc, length, name);
  }

  // Reads a variable-length 33-bit signed integer (little endian).
  template <ValidateFlag validate>
  int64_t read_i33v(const byte* pc, uint32_t* length,
                    const char* name = "signed LEB33") {
    return read_leb<int64_t, validate, kNoTrace, 33>(pc, length, name);
  }

  // Convenient overload for callers who don't care about length.
  template <ValidateFlag validate>
  WasmOpcode read_prefixed_opcode(const byte* pc) {
    uint32_t len;
    return read_prefixed_opcode<validate>(pc, &len);
  }

  // Reads a prefixed-opcode, possibly with variable-length index.
  // `length` is set to the number of bytes that make up this opcode,
  // *including* the prefix byte. For most opcodes, it will be 2.
  template <ValidateFlag validate>
  WasmOpcode read_prefixed_opcode(const byte* pc, uint32_t* length,
                                  const char* name = "prefixed opcode") {
    uint32_t index;

    // Prefixed opcodes all use LEB128 encoding.
    index = read_u32v<validate>(pc + 1, length, "prefixed opcode index");
    *length += 1;  // Prefix byte.
    // Only support opcodes that go up to 0xFFF (when decoded). Anything
    // bigger will need more than 2 bytes, and the '<< 12' below will be wrong.
    if (validate && V8_UNLIKELY(index > 0xfff)) {
      errorf(pc, "Invalid prefixed opcode %d", index);
      // If size validation fails.
      index = 0;
      *length = 0;
    }

    if (index > 0xff) return static_cast<WasmOpcode>((*pc) << 12 | index);

    return static_cast<WasmOpcode>((*pc) << 8 | index);
  }

  // Reads a 8-bit unsigned integer (byte) and advances {pc_}.
  uint8_t consume_u8(const char* name = "uint8_t") {
    return consume_little_endian<uint8_t, kTrace>(name);
  }
  template <class Tracer>
  uint8_t consume_u8(const char* name, Tracer& tracer) {
    tracer.Bytes(pc_, sizeof(uint8_t));
    tracer.Description(name);
    return consume_little_endian<uint8_t, kNoTrace>(name);
  }

  // Reads a 16-bit unsigned integer (little endian) and advances {pc_}.
  uint16_t consume_u16(const char* name = "uint16_t") {
    return consume_little_endian<uint16_t, kTrace>(name);
  }

  // Reads a single 32-bit unsigned integer (little endian) and advances {pc_}.
  template <class Tracer>
  uint32_t consume_u32(const char* name, Tracer& tracer) {
    tracer.Bytes(pc_, sizeof(uint32_t));
    tracer.Description(name);
    return consume_little_endian<uint32_t, kNoTrace>(name);
  }

  // Reads a LEB128 variable-length unsigned 32-bit integer and advances {pc_}.
  uint32_t consume_u32v(const char* name = "var_uint32") {
    uint32_t length = 0;
    uint32_t result =
        read_leb<uint32_t, kFullValidation, kTrace>(pc_, &length, name);
    pc_ += length;
    return result;
  }
  template <class Tracer>
  uint32_t consume_u32v(const char* name, Tracer& tracer) {
    uint32_t length = 0;
    uint32_t result =
        read_leb<uint32_t, kFullValidation, kNoTrace>(pc_, &length, name);
    tracer.Bytes(pc_, length);
    tracer.Description(name);
    pc_ += length;
    return result;
  }

  // Reads a LEB128 variable-length signed 32-bit integer and advances {pc_}.
  int32_t consume_i32v(const char* name = "var_int32") {
    uint32_t length = 0;
    int32_t result =
        read_leb<int32_t, kFullValidation, kTrace>(pc_, &length, name);
    pc_ += length;
    return result;
  }

  // Reads a LEB128 variable-length unsigned 64-bit integer and advances {pc_}.
  template <class Tracer>
  uint64_t consume_u64v(const char* name, Tracer& tracer) {
    uint32_t length = 0;
    uint64_t result =
        read_leb<uint64_t, kFullValidation, kNoTrace>(pc_, &length, name);
    tracer.Bytes(pc_, length);
    tracer.Description(name);
    pc_ += length;
    return result;
  }

  // Reads a LEB128 variable-length signed 64-bit integer and advances {pc_}.
  int64_t consume_i64v(const char* name = "var_int64") {
    uint32_t length = 0;
    int64_t result =
        read_leb<int64_t, kFullValidation, kTrace>(pc_, &length, name);
    pc_ += length;
    return result;
  }

  // Consume {size} bytes and send them to the bit bucket, advancing {pc_}.
  void consume_bytes(uint32_t size, const char* name = "skip") {
    // Only trace if the name is not null.
    TRACE_IF(name, "  +%u  %-20s: %u bytes\n", pc_offset(), name, size);
    if (checkAvailable(size)) {
      pc_ += size;
    } else {
      pc_ = end_;
    }
  }
  template <class Tracer>
  void consume_bytes(uint32_t size, const char* name, Tracer& tracer) {
    tracer.Bytes(pc_, size);
    tracer.Description(name);
    consume_bytes(size, nullptr);
  }

  // Check that at least {size} bytes exist between {pc_} and {end_}.
  bool checkAvailable(uint32_t size) {
    DCHECK_LE(pc_, end_);
    if (V8_UNLIKELY(size > static_cast<uint32_t>(end_ - pc_))) {
      errorf(pc_, "expected %u bytes, fell off end", size);
      return false;
    }
    return true;
  }

  // Use this for "boolean validation", i.e. if the error message is not used
  // anyway.
  void V8_NOINLINE MarkError() {
    if (!ok()) return;
    error_ = {0, "validation failed"};
    onFirstError();
  }

  // Do not inline error methods. This has measurable impact on validation time,
  // see https://crbug.com/910432.
  void V8_NOINLINE error(const char* msg) { errorf(pc_offset(), "%s", msg); }
  void V8_NOINLINE error(const uint8_t* pc, const char* msg) {
    errorf(pc_offset(pc), "%s", msg);
  }
  void V8_NOINLINE error(uint32_t offset, const char* msg) {
    errorf(offset, "%s", msg);
  }

  void V8_NOINLINE PRINTF_FORMAT(2, 3) errorf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    verrorf(pc_offset(), format, args);
    va_end(args);
  }

  void V8_NOINLINE PRINTF_FORMAT(3, 4)
      errorf(uint32_t offset, const char* format, ...) {
    va_list args;
    va_start(args, format);
    verrorf(offset, format, args);
    va_end(args);
  }

  void V8_NOINLINE PRINTF_FORMAT(3, 4)
      errorf(const uint8_t* pc, const char* format, ...) {
    va_list args;
    va_start(args, format);
    verrorf(pc_offset(pc), format, args);
    va_end(args);
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
  template <typename T, typename R = std::decay_t<T>>
  Result<R> toResult(T&& val) {
    if (failed()) {
      TRACE("Result error: %s\n", error_.message().c_str());
      return Result<R>{error_};
    }
    return Result<R>{std::forward<T>(val)};
  }

  // Resets the boundaries of this decoder.
  void Reset(const byte* start, const byte* end, uint32_t buffer_offset = 0) {
    DCHECK_LE(start, end);
    DCHECK_EQ(static_cast<uint32_t>(end - start), end - start);
    start_ = start;
    pc_ = start;
    end_ = end;
    buffer_offset_ = buffer_offset;
    error_ = {};
  }

  void Reset(base::Vector<const uint8_t> bytes, uint32_t buffer_offset = 0) {
    Reset(bytes.begin(), bytes.end(), buffer_offset);
  }

  bool ok() const { return error_.empty(); }
  bool failed() const { return !ok(); }
  bool more() const { return pc_ < end_; }
  const WasmError& error() const { return error_; }

  const byte* start() const { return start_; }
  const byte* pc() const { return pc_; }
  uint32_t V8_INLINE position() const {
    return static_cast<uint32_t>(pc_ - start_);
  }
  // This needs to be inlined for performance (see https://crbug.com/910432).
  uint32_t V8_INLINE pc_offset(const uint8_t* pc) const {
    DCHECK_LE(start_, pc);
    DCHECK_GE(kMaxUInt32 - buffer_offset_, pc - start_);
    return static_cast<uint32_t>(pc - start_) + buffer_offset_;
  }
  uint32_t pc_offset() const { return pc_offset(pc_); }
  uint32_t buffer_offset() const { return buffer_offset_; }
  // Takes an offset relative to the module start and returns an offset relative
  // to the current buffer of the decoder.
  uint32_t GetBufferRelativeOffset(uint32_t offset) const {
    DCHECK_LE(buffer_offset_, offset);
    return offset - buffer_offset_;
  }
  const byte* end() const { return end_; }
  void set_end(const byte* end) { end_ = end; }

  // Check if the byte at {offset} from the current pc equals {expected}.
  bool lookahead(int offset, byte expected) {
    DCHECK_LE(pc_, end_);
    return end_ - pc_ > offset && pc_[offset] == expected;
  }

 protected:
  const byte* start_;
  const byte* pc_;
  const byte* end_;
  // The offset of the current buffer in the module. Needed for streaming.
  uint32_t buffer_offset_;
  WasmError error_;

 private:
  void verrorf(uint32_t offset, const char* format, va_list args) {
    // Only report the first error.
    if (!ok()) return;
    constexpr int kMaxErrorMsg = 256;
    base::EmbeddedVector<char, kMaxErrorMsg> buffer;
    int len = base::VSNPrintF(buffer, format, args);
    CHECK_LT(0, len);
    error_ = {offset, {buffer.begin(), static_cast<size_t>(len)}};
    onFirstError();
  }

  template <typename IntType, ValidateFlag validate>
  IntType read_little_endian(const byte* pc, const char* msg) {
    if (!validate) {
      DCHECK(validate_size(pc, sizeof(IntType), msg));
    } else if (!validate_size(pc, sizeof(IntType), msg)) {
      return IntType{0};
    }
    return base::ReadLittleEndianValue<IntType>(reinterpret_cast<Address>(pc));
  }

  template <typename IntType, TraceFlag trace>
  IntType consume_little_endian(const char* name) {
    TRACE_IF(trace, "  +%u  %-20s: ", pc_offset(), name);
    if (!checkAvailable(sizeof(IntType))) {
      traceOffEnd();
      pc_ = end_;
      return IntType{0};
    }
    IntType val = read_little_endian<IntType, kNoValidation>(pc_, name);
    traceByteRange(pc_, pc_ + sizeof(IntType));
    TRACE_IF(trace, "= %d\n", val);
    pc_ += sizeof(IntType);
    return val;
  }

  template <typename IntType, ValidateFlag validate, TraceFlag trace,
            size_t size_in_bits = 8 * sizeof(IntType)>
  V8_INLINE IntType read_leb(const byte* pc, uint32_t* length,
                             const char* name = "varint") {
    static_assert(size_in_bits <= 8 * sizeof(IntType),
                  "leb does not fit in type");
    TRACE_IF(trace, "  +%u  %-20s: ", pc_offset(), name);
    // Fast path for single-byte integers.
    if ((!validate || V8_LIKELY(pc < end_)) && !(*pc & 0x80)) {
      TRACE_IF(trace, "%02x ", *pc);
      *length = 1;
      IntType result = *pc;
      if (std::is_signed<IntType>::value) {
        // Perform sign extension.
        constexpr int sign_ext_shift = int{8 * sizeof(IntType)} - 7;
        result = (result << sign_ext_shift) >> sign_ext_shift;
        TRACE_IF(trace, "= %" PRIi64 "\n", static_cast<int64_t>(result));
      } else {
        TRACE_IF(trace, "= %" PRIu64 "\n", static_cast<uint64_t>(result));
      }
      return result;
    }
    return read_leb_slowpath<IntType, validate, trace, size_in_bits>(pc, length,
                                                                     name);
  }

  template <typename IntType, ValidateFlag validate, TraceFlag trace,
            size_t size_in_bits = 8 * sizeof(IntType)>
  V8_NOINLINE IntType read_leb_slowpath(const byte* pc, uint32_t* length,
                                        const char* name) {
    // Create an unrolled LEB decoding function per integer type.
    return read_leb_tail<IntType, validate, trace, size_in_bits, 0>(pc, length,
                                                                    name, 0);
  }

  template <typename IntType, ValidateFlag validate, TraceFlag trace,
            size_t size_in_bits, int byte_index>
  V8_INLINE IntType read_leb_tail(const byte* pc, uint32_t* length,
                                  const char* name, IntType result) {
    constexpr bool is_signed = std::is_signed<IntType>::value;
    constexpr int kMaxLength = (size_in_bits + 6) / 7;
    static_assert(byte_index < kMaxLength, "invalid template instantiation");
    constexpr int shift = byte_index * 7;
    constexpr bool is_last_byte = byte_index == kMaxLength - 1;
    const bool at_end = validate && pc >= end_;
    byte b = 0;
    if (V8_LIKELY(!at_end)) {
      DCHECK_LT(pc, end_);
      b = *pc;
      TRACE_IF(trace, "%02x ", b);
      using Unsigned = typename std::make_unsigned<IntType>::type;
      result = result |
               (static_cast<Unsigned>(static_cast<IntType>(b) & 0x7f) << shift);
    }
    if (!is_last_byte && (b & 0x80)) {
      // Make sure that we only instantiate the template for valid byte indexes.
      // Compilers are not smart enough to figure out statically that the
      // following call is unreachable if is_last_byte is false.
      constexpr int next_byte_index = byte_index + (is_last_byte ? 0 : 1);
      return read_leb_tail<IntType, validate, trace, size_in_bits,
                           next_byte_index>(pc + 1, length, name, result);
    }
    *length = byte_index + (at_end ? 0 : 1);
    if (validate && V8_UNLIKELY(at_end || (b & 0x80))) {
      TRACE_IF(trace, at_end ? "<end> " : "<length overflow> ");
      if (validate == kFullValidation) {
        errorf(pc, "expected %s", name);
      } else {
        MarkError();
      }
      result = 0;
      *length = 0;
    }
    if (is_last_byte) {
      // A signed-LEB128 must sign-extend the final byte, excluding its
      // most-significant bit; e.g. for a 32-bit LEB128:
      //   kExtraBits = 4  (== 32 - (5-1) * 7)
      // For unsigned values, the extra bits must be all zero.
      // For signed values, the extra bits *plus* the most significant bit must
      // either be 0, or all ones.
      constexpr int kExtraBits = size_in_bits - ((kMaxLength - 1) * 7);
      constexpr int kSignExtBits = kExtraBits - (is_signed ? 1 : 0);
      const byte checked_bits = b & (0xFF << kSignExtBits);
      constexpr byte kSignExtendedExtraBits = 0x7f & (0xFF << kSignExtBits);
      const bool valid_extra_bits =
          checked_bits == 0 ||
          (is_signed && checked_bits == kSignExtendedExtraBits);
      if (!validate) {
        DCHECK(valid_extra_bits);
      } else if (V8_UNLIKELY(!valid_extra_bits)) {
        if (validate == kFullValidation) {
          error(pc, "extra bits in varint");
        } else {
          MarkError();
        }
        result = 0;
        *length = 0;
      }
    }
    constexpr int sign_ext_shift =
        is_signed ? std::max(0, int{8 * sizeof(IntType)} - shift - 7) : 0;
    // Perform sign extension.
    result = (result << sign_ext_shift) >> sign_ext_shift;
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
