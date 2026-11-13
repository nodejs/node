// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DECODER_H_
#define V8_WASM_DECODER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdarg>
#include <memory>

#include "src/base/bits.h"
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

struct WasmFunction;

class ITracer {
 public:
  static constexpr ITracer* NoTrace = nullptr;

  // Hooks for extracting byte offsets of things.
  virtual void TypeOffset(uint32_t offset) = 0;
  virtual void ImportOffset(uint32_t offset) = 0;
  virtual void ImportsDone(const WasmModule* module) = 0;
  virtual void TableOffset(uint32_t offset) = 0;
  virtual void MemoryOffset(uint32_t offset) = 0;
  virtual void TagOffset(uint32_t offset) = 0;
  virtual void GlobalOffset(uint32_t offset) = 0;
  virtual void StartOffset(uint32_t offset) = 0;
  virtual void ElementOffset(uint32_t offset) = 0;
  virtual void DataOffset(uint32_t offset) = 0;
  virtual void StringOffset(uint32_t offset) = 0;
  virtual void RecGroupOffset(uint32_t offset, uint32_t group_size) = 0;

  // Hooks for annotated hex dumps.
  virtual void Bytes(const uint8_t* start, uint32_t count) = 0;

  virtual void Description(const char* desc) = 0;
  virtual void Description(const char* desc, size_t length) = 0;
  virtual void Description(uint32_t number) = 0;
  virtual void Description(uint64_t number) = 0;
  virtual void Description(ValueType type) = 0;
  virtual void Description(HeapType type) = 0;
  virtual void Description(const FunctionSig* sig) = 0;

  virtual void NextLine() = 0;
  virtual void NextLineIfFull() = 0;
  virtual void NextLineIfNonEmpty() = 0;

  virtual void InitializerExpression(const uint8_t* start, const uint8_t* end,
                                     ValueType expected_type) = 0;
  virtual void FunctionBody(const WasmFunction* func, const uint8_t* start) = 0;
  virtual void FunctionName(uint32_t func_index) = 0;
  virtual void NameSection(const uint8_t* start, const uint8_t* end,
                           uint32_t offset) = 0;

  virtual ~ITracer() = default;
};

// A helper utility to decode bytes, integers, fields, varints, etc, from
// a buffer of bytes.
class Decoder {
 public:
  // Don't run validation, assume valid input.
  static constexpr struct NoValidationTag {
    static constexpr bool validate = false;
  } kNoValidation = {};
  // Run full validation with error message and location.
  static constexpr struct FullValidationTag {
    static constexpr bool validate = true;
  } kFullValidation = {};

  struct NoName {
    constexpr NoName(const char*) {}  // NOLINT(runtime/explicit)
    operator const char*() const { UNREACHABLE(); }
  };
  // Pass a {NoName} if we know statically that we do not use it anyway (we are
  // not tracing (in release mode) and not running full validation).
#ifdef DEBUG
  template <typename ValidationTag>
  using Name = const char*;
#else
  template <typename ValidationTag>
  using Name = std::conditional_t<ValidationTag::validate, const char*, NoName>;
#endif

  enum TraceFlag : bool { kTrace = true, kNoTrace = false };

  Decoder(const uint8_t* start, const uint8_t* end, uint32_t buffer_offset = 0)
      : Decoder(start, start, end, buffer_offset) {}
  explicit Decoder(const base::Vector<const uint8_t> bytes,
                   uint32_t buffer_offset = 0)
      : Decoder(bytes.begin(), bytes.end(), buffer_offset) {}
  Decoder(const uint8_t* start, const uint8_t* pc, const uint8_t* end,
          uint32_t buffer_offset = 0)
      : start_(start), pc_(pc), end_(end), buffer_offset_(buffer_offset) {
    DCHECK_LE(start, pc);
    DCHECK_LE(pc, end);
    DCHECK_EQ(static_cast<uint32_t>(end - start), end - start);
  }

  virtual ~Decoder() = default;

  // Reads an 8-bit unsigned integer.
  template <typename ValidationTag>
  uint8_t read_u8(const uint8_t* pc,
                  Name<ValidationTag> msg = "expected 1 byte") {
    return read_little_endian<uint8_t, ValidationTag>(pc, msg);
  }

  // Reads a 32-bit unsigned integer (little endian).
  template <typename ValidationTag>
  uint32_t read_u32(const uint8_t* pc,
                    Name<ValidationTag> msg = "expected 4 bytes") {
    return read_little_endian<uint32_t, ValidationTag>(pc, msg);
  }

  // Reads a 64-bit unsigned integer (little endian).
  template <typename ValidationTag>
  uint64_t read_u64(const uint8_t* pc,
                    Name<ValidationTag> msg = "expected 8 bytes") {
    return read_little_endian<uint64_t, ValidationTag>(pc, msg);
  }

  // Reads a variable-length unsigned integer (little endian). Returns the read
  // value and the number of bytes read.
  template <typename ValidationTag>
  std::pair<uint32_t, uint32_t> read_u32v(const uint8_t* pc,
                                          Name<ValidationTag> name = "LEB32") {
    return read_leb<uint32_t, ValidationTag, kNoTrace>(pc, name);
  }

  // Reads a variable-length signed integer (little endian). Returns the read
  // value and the number of bytes read.
  template <typename ValidationTag>
  std::pair<int32_t, uint32_t> read_i32v(
      const uint8_t* pc, Name<ValidationTag> name = "signed LEB32") {
    return read_leb<int32_t, ValidationTag, kNoTrace>(pc, name);
  }

  // Reads a variable-length unsigned integer (little endian). Returns the read
  // value and the number of bytes read.
  template <typename ValidationTag>
  std::pair<uint64_t, uint32_t> read_u64v(const uint8_t* pc,
                                          Name<ValidationTag> name = "LEB64") {
    return read_leb<uint64_t, ValidationTag, kNoTrace>(pc, name);
  }

  // Reads a variable-length signed integer (little endian). Returns the read
  // value and the number of bytes read.
  template <typename ValidationTag>
  std::pair<int64_t, uint32_t> read_i64v(
      const uint8_t* pc, Name<ValidationTag> name = "signed LEB64") {
    return read_leb<int64_t, ValidationTag, kNoTrace>(pc, name);
  }

  // Reads a variable-length 33-bit signed integer (little endian). Returns the
  // read value and the number of bytes read.
  template <typename ValidationTag>
  std::pair<int64_t, uint32_t> read_i33v(
      const uint8_t* pc, Name<ValidationTag> name = "signed LEB33") {
    return read_leb<int64_t, ValidationTag, kNoTrace, 33>(pc, name);
  }

  // Reads a prefixed-opcode, possibly with variable-length index.
  // Returns the read opcode and the number of bytes that make up this opcode,
  // *including* the prefix byte. For most opcodes, it will be 2.
  template <typename ValidationTag>
  V8_INLINE std::pair<WasmOpcode, uint32_t> read_prefixed_opcode(
      const uint8_t* pc) {
    DCHECK_LE(start_, pc);
    DCHECK_LT(pc, end_);
    DCHECK(WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(*pc)));
    static_assert(kGCPrefix == 0xfb && kNumericPrefix == 0xfc &&
                  kSimdPrefix == 0xfd && kAtomicPrefix == 0xfe);
    // Helps callers prove the fast-path result is non-zero (>= 0xfb00).
    V8_ASSUME(*pc >= kGCPrefix);
    // Inline fast paths for 1-byte (0..0x7f) and 2-byte (0..0xfff) opcode
    // indices, keeping multi-byte LEB / validation-error handling out of line.
    // Valid Wasm function bodies and constant expressions always end with
    // kExprEnd, so at least 3 bytes (prefix, b1, and kExprEnd or b2) remain at
    // any valid prefixed opcode.
    if (V8_LIKELY(!ValidationTag::validate || end_ - pc >= 3)) {
      DCHECK_LE(2, end_ - pc);
      uint32_t b1 = pc[1];
      if (V8_LIKELY(b1 < 0x80)) {
        return {static_cast<WasmOpcode>((static_cast<uint32_t>(*pc) << 8) | b1),
                2};
      }
      DCHECK_LE(3, end_ - pc);
      uint32_t b2 = pc[2];
      if (V8_LIKELY(b2 <= 0x1f)) {
        uint32_t index = (b1 & 0x7f) | (b2 << 7);
        int shift = b2 >= 2 ? 12 : 8;
        return {static_cast<WasmOpcode>((static_cast<uint32_t>(*pc) << shift) |
                                        index),
                3};
      }
    }
    return read_prefixed_opcode_slowpath<ValidationTag>(pc);
  }

  // Reads a 8-bit unsigned integer (byte) and advances {pc_}.
  uint8_t consume_u8(const char* name = "uint8_t") {
    return consume_little_endian<uint8_t, kTrace>(name);
  }
  uint8_t consume_u8(const char* name, ITracer* tracer) {
    if (tracer) {
      tracer->Bytes(pc_, sizeof(uint8_t));
      tracer->Description(name);
    }
    return consume_little_endian<uint8_t, kNoTrace>(name);
  }

  // Reads a single 32-bit unsigned integer (little endian) and advances {pc_}.
  uint32_t consume_u32(const char* name, ITracer* tracer) {
    if (tracer) {
      tracer->Bytes(pc_, sizeof(uint32_t));
      tracer->Description(name);
    }
    return consume_little_endian<uint32_t, kNoTrace>(name);
  }

  // Reads a LEB128 variable-length unsigned 32-bit integer and advances {pc_}.
  uint32_t consume_u32v(const char* name = "var_uint32") {
    auto [result, length] =
        read_leb<uint32_t, FullValidationTag, kTrace>(pc_, name);
    pc_ += length;
    return result;
  }
  uint32_t consume_u32v(const char* name, ITracer* tracer) {
    auto [result, length] =
        read_leb<uint32_t, FullValidationTag, kNoTrace>(pc_, name);
    if (tracer) {
      tracer->Bytes(pc_, length);
      tracer->Description(name);
    }
    pc_ += length;
    return result;
  }

  // Reads a LEB128 variable-length signed 32-bit integer and advances {pc_}.
  int32_t consume_i32v(const char* name = "var_int32") {
    auto [result, length] =
        read_leb<int32_t, FullValidationTag, kTrace>(pc_, name);
    pc_ += length;
    return result;
  }

  // Reads a LEB128 variable-length unsigned 64-bit integer and advances {pc_}.
  uint64_t consume_u64v(const char* name, ITracer* tracer) {
    auto [result, length] =
        read_leb<uint64_t, FullValidationTag, kNoTrace>(pc_, name);
    if (tracer) {
      tracer->Bytes(pc_, length);
      tracer->Description(name);
    }
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
  void consume_bytes(uint32_t size, const char* name, ITracer* tracer) {
    if (tracer) {
      tracer->Bytes(pc_, size);
      tracer->Description(name);
    }
    consume_bytes(size, nullptr);
  }

  uint32_t available_bytes() const {
    DCHECK_LE(pc_, end_);
    DCHECK_GE(kMaxUInt32, end_ - pc_);
    return static_cast<uint32_t>(end_ - pc_);
  }

  // Check that at least {size} bytes exist between {pc_} and {end_}.
  bool checkAvailable(uint32_t size) {
    if (V8_UNLIKELY(size > available_bytes())) {
      errorf(pc_, "expected %u bytes, fell off end", size);
      return false;
    }
    return true;
  }

  // Do not inline error methods. This has measurable impact on validation time,
  // see https://crbug.com/910432.
  void V8_NOINLINE V8_PRESERVE_MOST error(const char* msg) {
    errorf(pc_offset(), "%s", msg);
  }
  void V8_NOINLINE V8_PRESERVE_MOST error(const uint8_t* pc, const char* msg) {
    errorf(pc_offset(pc), "%s", msg);
  }
  void V8_NOINLINE V8_PRESERVE_MOST error(uint32_t offset, const char* msg) {
    errorf(offset, "%s", msg);
  }

  template <typename... Args>
  void V8_NOINLINE V8_PRESERVE_MOST errorf(const char* format, Args... args) {
    errorf(pc_offset(), format, args...);
  }

  template <typename... Args>
  void V8_NOINLINE V8_PRESERVE_MOST errorf(const uint8_t* pc,
                                           const char* format, Args... args) {
    errorf(pc_offset(pc), format, args...);
  }

  template <typename... Args>
  void V8_NOINLINE V8_PRESERVE_MOST errorf(uint32_t offset, const char* format,
                                           Args... args) {
    static_assert(
        sizeof...(Args) > 0,
        "Use error instead of errorf if the format string has no placeholders");
    verrorf(offset, format, args...);
  }

  // Behavior triggered on first error, overridden in subclasses.
  virtual void onFirstError() {}

  // Debugging helper to print a bytes range as hex bytes.
  void traceByteRange(const uint8_t* start, const uint8_t* end) {
    DCHECK_LE(start, end);
    for (const uint8_t* p = start; p < end; ++p) TRACE("%02x ", *p);
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
  void Reset(const uint8_t* start, const uint8_t* end,
             uint32_t buffer_offset = 0) {
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

  bool ok() const { return !failed(); }
  bool failed() const { return V8_UNLIKELY(error_.has_error()); }
  bool more() const { return pc_ < end_; }
  const WasmError& error() const { return error_; }

  const uint8_t* start() const { return start_; }
  const uint8_t* pc() const { return pc_; }
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
  const uint8_t* end() const { return end_; }
  void set_end(const uint8_t* end) { end_ = end; }

  // Check if the uint8_t at {offset} from the current pc equals {expected}.
  bool lookahead(int offset, uint8_t expected) {
    DCHECK_LE(pc_, end_);
    return end_ - pc_ > offset && pc_[offset] == expected;
  }

 protected:
  const uint8_t* start_;
  const uint8_t* pc_;
  const uint8_t* end_;
  // The offset of the current buffer in the module. Needed for streaming.
  uint32_t buffer_offset_;
  WasmError error_;

 private:
  void V8_NOINLINE PRINTF_FORMAT(3, 4)
      verrorf(uint32_t offset, const char* format, ...) {
    // Only report the first error.
    if (!ok()) return;
    constexpr int kMaxErrorMsg = 256;
    std::array<char, kMaxErrorMsg> buffer;
    va_list args;
    va_start(args, format);
    int len = base::VSNPrintF(buffer, format, args);
    va_end(args);
    CHECK_LT(0, len);
    error_ = {offset, {buffer.data(), static_cast<size_t>(len)}};
    onFirstError();
  }

  template <typename IntType, typename ValidationTag>
  IntType read_little_endian(const uint8_t* pc, Name<ValidationTag> msg) {
    DCHECK_LE(start_, pc);

    if (!ValidationTag::validate) {
      DCHECK_LE(pc, end_);
      DCHECK_LE(sizeof(IntType), end_ - pc);
    } else if (V8_UNLIKELY(ptrdiff_t{sizeof(IntType)} > end_ - pc)) {
      error(pc, msg);
      return 0;
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
    IntType val = read_little_endian<IntType, NoValidationTag>(pc_, name);
    traceByteRange(pc_, pc_ + sizeof(IntType));
    TRACE_IF(trace, "= %d\n", val);
    pc_ += sizeof(IntType);
    return val;
  }

  // The implementation of LEB-decoding; returns the value and the number of
  // bytes read.
  template <typename IntType, typename ValidationTag, TraceFlag trace,
            size_t size_in_bits = 8 * sizeof(IntType)>
  V8_INLINE std::pair<IntType, uint32_t> read_leb(
      const uint8_t* pc, Name<ValidationTag> name = "varint") {
    static_assert(size_in_bits <= 8 * sizeof(IntType),
                  "leb does not fit in type");
    TRACE_IF(trace, "  +%u  %-20s: ", pc_offset(),
             implicit_cast<const char*>(name));
    // Fast path for single-byte integers.
    if (V8_LIKELY((!ValidationTag::validate || pc < end_) && !(*pc & 0x80))) {
      TRACE_IF(trace, "%02x ", *pc);
      IntType result = *pc;
      if (std::is_signed_v<IntType>) {
        // Perform sign extension.
        constexpr int sign_ext_shift = int{8 * sizeof(IntType)} - 7;
        result = (result << sign_ext_shift) >> sign_ext_shift;
        TRACE_IF(trace, "= %" PRIi64 "\n", static_cast<int64_t>(result));
      } else {
        TRACE_IF(trace, "= %" PRIu64 "\n", static_cast<uint64_t>(result));
      }
      return {result, 1};
    }
    // Multi-byte unsigned 32-bit LEBs (indices, counts) and 33-bit signed LEBs
    // (WasmGC heap/block type indices) are almost always 2 bytes; unsigned
    // 64-bit LEBs (memory offsets) are almost always <= 4 bytes. Try a narrow
    // SWAR load first for those, while sending general signed constants
    // (i32.const, i64.const) straight to the 8-byte SWAR load.
    using InitialSwarWord = std::conditional_t<
        (!std::is_signed_v<IntType> && sizeof(IntType) == 4) ||
            size_in_bits == 33,
        uint16_t,
        std::conditional_t<std::is_signed_v<IntType>, uint64_t, uint32_t>>;
    std::pair<IntType, uint32_t> decoded;
    if (trace && V8_UNLIKELY(v8_flags.trace_wasm_decoder)) {
      decoded =
          read_leb_byte_by_byte<IntType, ValidationTag, trace, size_in_bits>(
              pc, name);
    } else {
      decoded = read_leb_swar<IntType, ValidationTag, size_in_bits,
                              InitialSwarWord>(pc, name);
    }
    // Single-byte LEBs already returned above, so multi-byte lengths are >= 2
    // on success or 0 on validation failure.
    V8_ASSUME(decoded.second <= (size_in_bits + 6) / 7);
    V8_ASSUME(ValidationTag::validate ? decoded.second != 1
                                      : decoded.second >= 2);
    return decoded;
  }

  template <size_t size_in_bits, bool is_signed>
  static constexpr V8_INLINE bool is_valid_leb_last_byte(uint8_t byte) {
    // The final (kMaxLength-th) byte of an N-bit LEB128 encodes only the
    // remaining {kUsedBits} bits; e.g. for a 32-bit LEB128 (kMaxLength == 5):
    //   kUsedBits = 4  (== 32 - (5 - 1) * 7)
    // - For unsigned values, bit 7 (continuation bit) and bits [kUsedBits .. 6]
    //   must all be 0, i.e. {byte < (1U << kUsedBits)}.
    // - For signed values, bit 7 must be 0 and the unused bits [kUsedBits .. 6]
    //   must sign-extend the highest used payload bit (bit kUsedBits - 1).
    //   In {u ^ (u << 1)}:
    //   * bit 8 equals bit 7 of {byte} (since bit 8 of {u} is 0), so masking
    //     with 0x100U checks that the continuation bit is 0;
    //   * each bit i in [kUsedBits .. 6] is 0 iff bit i of {byte} equals bit
    //     (i - 1), so masking with (0x7fU & (0xffU << kUsedBits)) checks that
    //     all unused payload bits equal bit (kUsedBits - 1).
    constexpr int kMaxLength = (size_in_bits + 6) / 7;
    constexpr int kUsedBits = size_in_bits - ((kMaxLength - 1) * 7);
    if constexpr (!is_signed) {
      return byte < (1U << kUsedBits);
    } else {
      constexpr uint32_t kInvalidMask = 0x100U | (0x7fU & (0xffU << kUsedBits));
      uint32_t u = byte;
      return ((u ^ (u << 1)) & kInvalidMask) == 0;
    }
  }

  template <size_t size_in_bits, bool is_signed, bool validate,
            typename SwarWord>
  static constexpr V8_INLINE bool can_decode_leb_swar(SwarWord word,
                                                      SwarWord terminators) {
    constexpr int kMaxLength = (size_in_bits + 6) / 7;
    if constexpr (int{sizeof(SwarWord)} < kMaxLength) {
      // Narrow SWAR load (2B for 32/33-bit LEB, or 4B/8B for 64-bit LEB): any
      // terminator in {word} fits in the target type without overflowing.
      return terminators != 0;
    }
    // If any of bytes 0..3 terminates, no extra-bits check is needed:
    // substitute 0 (always valid) so both length-overflow (byte 4 bit 7) and
    // extra-bits validation collapse into a single branchless check.
    uint8_t last_byte = static_cast<uint32_t>(terminators) != 0
                            ? 0
                            : static_cast<uint8_t>(word >> 32);
    bool valid = is_valid_leb_last_byte<size_in_bits, is_signed>(last_byte);
    if constexpr (!validate) {
      DCHECK(valid);
      return true;
    }
    return valid;
  }

  // Packs the low 7 payload bits of each byte in {word} into a contiguous
  // integer via pairwise SWAR stages:
  //   bytes:    [.ddddddd .ccccccc .bbbbbbb .aaaaaaa]
  //   pairs:    [..dddddd dccccccc ..bbbbbb baaaaaaa]  (14-bit halves)
  //   gathered: [....dddd dddccccc ccbbbbbb baaaaaaa]  (28 contiguous bits)
  template <size_t size_in_bits, typename SwarWord>
  static constexpr V8_INLINE uint64_t gather_leb_bytes(SwarWord word) {
    uint32_t low = static_cast<uint32_t>(word);
    uint32_t pairs = (low & 0x007f007fU) | ((low >> 1) & 0x3f803f80U);
    if constexpr (sizeof(SwarWord) == 2) {
      return pairs;
    }
    uint64_t gathered = (pairs & 0x00003fffU) | ((pairs >> 2) & 0x0fffc000U);
    if constexpr (sizeof(SwarWord) == 4) {
      return gathered;
    } else if constexpr (size_in_bits <= 33) {
      // Place byte 4's payload (bits 32..38 of {word}) at bits 28..34 via
      // {word >> 4}. For 32-bit types (u32v/i32v), mask only the 4 used bits
      // (28..31) so the mask fits in a 32-bit immediate on x64.
      constexpr uint64_t kByte4Mask =
          (size_in_bits == 32 ? 0x0fULL : 0x7fULL) << 28;
      return gathered | ((word >> 4) & kByte4Mask);
    } else {
      return gathered |
             (gather_leb_bytes<size_in_bits>(static_cast<uint32_t>(word >> 32))
              << 28);
    }
  }

  // Multi-byte LEB decoding is split into V8_PRESERVE_MOST functions chained
  // via V8_MUSTTAIL:
  // 1. {read_leb_swar}: branchless SWAR decode over a {SwarWord} load
  //    (starting with {uint16_t} for 2B u32v/i33v or {uint32_t} for <= 4B u64v
  //    to avoid callee-saved register spills on the common short cases, then
  //    chaining to {uint64_t} for up to 8 bytes).
  // 2. {read_leb_byte_by_byte}: cold byte-by-byte loop for < 8-byte buffer
  //    tails near {end_}, 9..10-byte 64-bit LEBs, --trace-wasm-decoder, and
  //    error reporting. Isolating {errorf}/{error} here prevents their
  //    argument-register setup from forcing callee-saved register spills in
  //    {read_leb_swar}.
  template <typename IntType, typename ValidationTag,
            size_t size_in_bits = 8 * sizeof(IntType),
            typename SwarWord = uint64_t>
  V8_NOINLINE V8_PRESERVE_MOST std::pair<IntType, uint32_t> read_leb_swar(
      const uint8_t* pc, Name<ValidationTag> name) {
    constexpr int kSwarWidth = sizeof(SwarWord);
    static_assert(kSwarWidth == 2 || kSwarWidth == 4 || kSwarWidth == 8);
    constexpr bool is_signed = std::is_signed_v<IntType>;

    // In NoValidationTag mode, reaching {read_leb_swar} implies
    // (pc[0] & 0x80) != 0, so at least 2 bytes are guaranteed in bounds; wider
    // loads (4 or 8 bytes) must still check {end_ - pc >= kSwarWidth} so a
    // shorter LEB near {end_} never reads past the buffer.
    constexpr bool kMustCheckBounds =
        ValidationTag::validate || kSwarWidth > 2;
    if (!kMustCheckBounds || V8_LIKELY(end_ - pc >= kSwarWidth)) {
      DCHECK_LE(kSwarWidth, end_ - pc);
      SwarWord word = base::ReadLittleEndianValue<SwarWord>(
          reinterpret_cast<Address>(pc));
      DCHECK_NE(0, word & 0x80);
      // Byte 0 is guaranteed to have its continuation bit set, so only check
      // bytes 1..kSwarWidth-1 for a terminating byte (MSB == 0). For
      // SwarWord == uint16_t, only bit 15 can be set in {terminators}, so
      // Clang folds {length} to the constant 2 without emitting {tzcnt}.
      SwarWord terminators =
          ~word & static_cast<SwarWord>(0x8080808080808000ULL);
      if (V8_LIKELY((can_decode_leb_swar<size_in_bits, is_signed,
                                         ValidationTag::validate>(
              word, terminators)))) {
        DCHECK_NE(0, terminators);
        uint32_t length =
            (base::bits::CountTrailingZerosNonZero(terminators) >> 3) + 1;
        if constexpr (!is_signed) {
          // Zero out trailing bytes past the first terminator before gathering.
          word &= terminators ^ (terminators - 1);
        }
        uint64_t gathered = gather_leb_bytes<size_in_bits>(word);
        if constexpr (is_signed) {
          // For signed LEBs, {word} was gathered unmasked in parallel with
          // {ctz}; a single 64-bit shift pair both discards trailing bytes
          // above {7 * length} bits and sign-extends bit (7 * length - 1).
          uint32_t shift = 64 - 7 * length;
          return {static_cast<IntType>(
                      static_cast<int64_t>(gathered << shift) >> shift),
                  length};
        }
        return {static_cast<IntType>(gathered), length};
      }
    }
    if constexpr (kSwarWidth < 8) {
      V8_MUSTTAIL return read_leb_swar<IntType, ValidationTag, size_in_bits,
                                       uint64_t>(pc, name);
    } else {
      V8_MUSTTAIL return read_leb_byte_by_byte<IntType, ValidationTag, kNoTrace,
                                               size_in_bits>(pc, name);
    }
  }

  // Cold fallback for LEBs near {end_} (< 8 bytes left in buffer), 9..10-byte
  // 64-bit LEBs, --trace-wasm-decoder, and malformed LEBs (reporting the exact
  // error offset and message).
  template <typename IntType, typename ValidationTag, TraceFlag trace,
            size_t size_in_bits = 8 * sizeof(IntType)>
  V8_NOINLINE V8_PRESERVE_MOST std::pair<IntType, uint32_t>
  read_leb_byte_by_byte(const uint8_t* pc, Name<ValidationTag> name) {
    constexpr bool is_signed = std::is_signed_v<IntType>;
    constexpr int kMaxLength = (size_in_bits + 6) / 7;
    using Unsigned = std::make_unsigned_t<IntType>;
    Unsigned result = 0;
    for (int byte_index = 0; byte_index < kMaxLength; ++byte_index) {
      const int shift = byte_index * 7;
      const bool is_last_byte = byte_index == kMaxLength - 1;
      const bool at_end = ValidationTag::validate && pc >= end_;
      uint8_t b = 0;
      if (V8_LIKELY(!at_end)) {
        DCHECK_LT(pc, end_);
        b = *pc;
        TRACE_IF(trace, "%02x ", b);
        result |= static_cast<Unsigned>(b & 0x7f) << shift;
      }
      if (!is_last_byte && (b & 0x80)) {
        ++pc;
        continue;
      }
      if constexpr (ValidationTag::validate) {
        if (V8_UNLIKELY(at_end || (b & 0x80))) {
          TRACE_IF(trace, at_end ? "<end> " : "<length overflow> ");
          errorf(pc, "%s while decoding %s",
                 at_end ? "reached end" : "length overflow", name);
          return {0, 0};
        }
      }
      if (is_last_byte) {
        const bool valid_extra_bits =
            is_valid_leb_last_byte<size_in_bits, is_signed>(b);
        if constexpr (!ValidationTag::validate) {
          DCHECK(valid_extra_bits);
        } else if (V8_UNLIKELY(!valid_extra_bits)) {
          error(pc, "extra bits in varint");
          return {0, 0};
        }
      }
      if constexpr (is_signed) {
        // Clamps to 0 when the final byte already reaches the MSB of IntType
        // (e.g. 5th byte of int32_t or 10th byte of int64_t).
        const int sign_ext_shift =
            std::max(0, int{8 * sizeof(IntType)} - shift - 7);
        const IntType signed_result =
            static_cast<IntType>(result << sign_ext_shift) >> sign_ext_shift;
        TRACE_IF(trace, "= %" PRIi64 "\n", static_cast<int64_t>(signed_result));
        return {signed_result, static_cast<uint32_t>(byte_index + 1)};
      } else {
        TRACE_IF(trace, "= %" PRIu64 "\n", static_cast<uint64_t>(result));
        return {static_cast<IntType>(result),
                static_cast<uint32_t>(byte_index + 1)};
      }
    }
    UNREACHABLE();
  }

  template <typename ValidationTag>
  V8_NOINLINE V8_PRESERVE_MOST std::pair<WasmOpcode, uint32_t>
  read_prefixed_opcode_slowpath(const uint8_t* pc) {
    // Prefixed opcodes all use LEB128 encoding.
    uint32_t index;
    uint32_t index_length;
    std::tie(index, index_length) =
        read_u32v<ValidationTag>(pc + 1, "prefixed opcode index");
    // Only support opcodes that go up to 0xFFF (when decoded). Anything
    // bigger will need more than 2 bytes, and the '<< 12' below will be wrong.
    if constexpr (ValidationTag::validate) {
      if (V8_UNLIKELY(index_length == 0 || index > 0xfff)) {
        if (index_length != 0) {
          errorf(pc, "Invalid prefixed opcode %u", index);
        }
        // On validation failure we return "unreachable" (opcode 0).
        static_assert(kExprUnreachable == 0);
        return {kExprUnreachable, 0};
      }
    } else {
      DCHECK_GT(index_length, 0);
      DCHECK_LE(index, 0xfff);
    }
    int shift = index > 0xff ? 12 : 8;
    return {
        static_cast<WasmOpcode>((static_cast<uint32_t>(*pc) << shift) | index),
        index_length + 1};
  }
};

#undef TRACE
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_DECODER_H_
