// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#include <cinttypes>
#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/base/memory.h"
#include "src/codegen/signature.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#define TRACE_IF(cond, ...)                                     \
  do {                                                          \
    if (FLAG_trace_wasm_decoder && (cond)) PrintF(__VA_ARGS__); \
  } while (false)

// A {DecodeResult} only stores the failure / success status, but no data.
using DecodeResult = VoidResult;

// A helper utility to decode bytes, integers, fields, varints, etc, from
// a buffer of bytes.
class Decoder {
 public:
  enum ValidateFlag : bool { kValidate = true, kNoValidate = false };

  enum AdvancePCFlag : bool { kAdvancePc = true, kNoAdvancePc = false };

  enum TraceFlag : bool { kTrace = true, kNoTrace = false };

  Decoder(const byte* start, const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, start, end, buffer_offset) {}
  explicit Decoder(const Vector<const byte> bytes, uint32_t buffer_offset = 0)
      : Decoder(bytes.begin(), bytes.begin() + bytes.length(), buffer_offset) {}
  Decoder(const byte* start, const byte* pc, const byte* end,
          uint32_t buffer_offset = 0)
      : start_(start), pc_(pc), end_(end), buffer_offset_(buffer_offset) {
    DCHECK_LE(start, pc);
    DCHECK_LE(pc, end);
    DCHECK_EQ(static_cast<uint32_t>(end - start), end - start);
  }

  virtual ~Decoder() = default;

  inline bool validate_size(const byte* pc, uint32_t length, const char* msg) {
    DCHECK_LE(start_, pc);
    if (V8_UNLIKELY(pc > end_ || length > static_cast<uint32_t>(end_ - pc))) {
      error(pc, msg);
      return false;
    }
    return true;
  }

  // Reads an 8-bit unsigned integer.
  template <ValidateFlag validate>
  inline uint8_t read_u8(const byte* pc, const char* msg = "expected 1 byte") {
    return read_little_endian<uint8_t, validate>(pc, msg);
  }

  // Reads a 16-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  inline uint16_t read_u16(const byte* pc,
                           const char* msg = "expected 2 bytes") {
    return read_little_endian<uint16_t, validate>(pc, msg);
  }

  // Reads a 32-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  inline uint32_t read_u32(const byte* pc,
                           const char* msg = "expected 4 bytes") {
    return read_little_endian<uint32_t, validate>(pc, msg);
  }

  // Reads a 64-bit unsigned integer (little endian).
  template <ValidateFlag validate>
  inline uint64_t read_u64(const byte* pc,
                           const char* msg = "expected 8 bytes") {
    return read_little_endian<uint64_t, validate>(pc, msg);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <ValidateFlag validate>
  uint32_t read_u32v(const byte* pc, uint32_t* length,
                     const char* name = "LEB32") {
    return read_leb<uint32_t, validate, kNoAdvancePc, kNoTrace>(pc, length,
                                                                name);
  }

  // Reads a variable-length signed integer (little endian).
  template <ValidateFlag validate>
  int32_t read_i32v(const byte* pc, uint32_t* length,
                    const char* name = "signed LEB32") {
    return read_leb<int32_t, validate, kNoAdvancePc, kNoTrace>(pc, length,
                                                               name);
  }

  // Reads a variable-length unsigned integer (little endian).
  template <ValidateFlag validate>
  uint64_t read_u64v(const byte* pc, uint32_t* length,
                     const char* name = "LEB64") {
    return read_leb<uint64_t, validate, kNoAdvancePc, kNoTrace>(pc, length,
                                                                name);
  }

  // Reads a variable-length signed integer (little endian).
  template <ValidateFlag validate>
  int64_t read_i64v(const byte* pc, uint32_t* length,
                    const char* name = "signed LEB64") {
    return read_leb<int64_t, validate, kNoAdvancePc, kNoTrace>(pc, length,
                                                               name);
  }

  // Reads a prefixed-opcode, possibly with variable-length index.
  // The length param is set to the number of bytes this index is encoded with.
  // For most cases (non variable-length), it will be 1.
  template <ValidateFlag validate>
  WasmOpcode read_prefixed_opcode(const byte* pc, uint32_t* length = nullptr,
                                  const char* name = "prefixed opcode") {
    uint32_t unused_length;
    if (length == nullptr) {
      length = &unused_length;
    }
    DCHECK(WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(*pc)));
    uint32_t index;
    if (*pc == WasmOpcode::kSimdPrefix) {
      // SIMD opcodes can be multiple bytes (when LEB128 encoded).
      index = read_u32v<validate>(pc + 1, length, "prefixed opcode index");
      // Only support SIMD opcodes that go up to 0xFF (when decoded). Anything
      // bigger will need 1 more byte, and the '<< 8' below will be wrong.
      if (validate && V8_UNLIKELY(index > 0xff)) {
        errorf(pc, "Invalid SIMD opcode %d", index);
      }
    } else {
      if (!validate || validate_size(pc, 2, "expected 2 bytes")) {
        DCHECK(validate_size(pc, 2, "expected 2 bytes"));
        index = *(pc + 1);
        *length = 1;
      } else {
        // If kValidate and size validation fails.
        index = 0;
        *length = 0;
      }
    }
    return static_cast<WasmOpcode>((*pc) << 8 | index);
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
    uint32_t length = 0;
    return read_leb<uint32_t, kValidate, kAdvancePc, kTrace>(pc_, &length,
                                                             name);
  }

  // Reads a LEB128 variable-length signed 32-bit integer and advances {pc_}.
  int32_t consume_i32v(const char* name = nullptr) {
    uint32_t length = 0;
    return read_leb<int32_t, kValidate, kAdvancePc, kTrace>(pc_, &length, name);
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

  // Check that at least {size} bytes exist between {pc_} and {end_}.
  bool checkAvailable(uint32_t size) {
    DCHECK_LE(pc_, end_);
    if (V8_UNLIKELY(size > static_cast<uint32_t>(end_ - pc_))) {
      errorf(pc_, "expected %u bytes, fell off end", size);
      return false;
    }
    return true;
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
  template <typename T, typename U = typename std::remove_reference<T>::type>
  Result<U> toResult(T&& val) {
    if (failed()) {
      TRACE("Result error: %s\n", error_.message().c_str());
      return Result<U>{error_};
    }
    return Result<U>{std::forward<T>(val)};
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

  void Reset(Vector<const uint8_t> bytes, uint32_t buffer_offset = 0) {
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
    EmbeddedVector<char, kMaxErrorMsg> buffer;
    int len = VSNPrintF(buffer, format, args);
    CHECK_LT(0, len);
    error_ = {offset, {buffer.begin(), static_cast<size_t>(len)}};
    onFirstError();
  }

  template <typename IntType, bool validate>
  inline IntType read_little_endian(const byte* pc, const char* msg) {
    if (!validate) {
      DCHECK(validate_size(pc, sizeof(IntType), msg));
    } else if (!validate_size(pc, sizeof(IntType), msg)) {
      return IntType{0};
    }
    return base::ReadLittleEndianValue<IntType>(reinterpret_cast<Address>(pc));
  }

  template <typename IntType>
  inline IntType consume_little_endian(const char* name) {
    TRACE("  +%u  %-20s: ", pc_offset(), name);
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

  template <typename IntType, ValidateFlag validate, AdvancePCFlag advance_pc,
            TraceFlag trace>
  inline IntType read_leb(const byte* pc, uint32_t* length,
                          const char* name = "varint") {
    DCHECK_IMPLIES(advance_pc, pc == pc_);
    TRACE_IF(trace, "  +%u  %-20s: ", pc_offset(), name);
    return read_leb_tail<IntType, validate, advance_pc, trace, 0>(pc, length,
                                                                  name, 0);
  }

  template <typename IntType, ValidateFlag validate, AdvancePCFlag advance_pc,
            TraceFlag trace, int byte_index>
  IntType read_leb_tail(const byte* pc, uint32_t* length, const char* name,
                        IntType result) {
    constexpr bool is_signed = std::is_signed<IntType>::value;
    constexpr int kMaxLength = (sizeof(IntType) * 8 + 6) / 7;
    static_assert(byte_index < kMaxLength, "invalid template instantiation");
    constexpr int shift = byte_index * 7;
    constexpr bool is_last_byte = byte_index == kMaxLength - 1;
    const bool at_end = validate && pc >= end_;
    byte b = 0;
    if (!at_end) {
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
      return read_leb_tail<IntType, validate, advance_pc, trace,
                           next_byte_index>(pc + 1, length, name, result);
    }
    if (advance_pc) pc_ = pc + (at_end ? 0 : 1);
    *length = byte_index + (at_end ? 0 : 1);
    if (validate && (at_end || (b & 0x80))) {
      TRACE_IF(trace, at_end ? "<end> " : "<length overflow> ");
      errorf(pc, "expected %s", name);
      result = 0;
    }
    if (is_last_byte) {
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
      if (!validate) {
        DCHECK(valid_extra_bits);
      } else if (!valid_extra_bits) {
        error(pc, "extra bits in varint");
        result = 0;
      }
    }
    constexpr int sign_ext_shift =
        is_signed ? Max(0, int{8 * sizeof(IntType)} - shift - 7) : 0;
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
