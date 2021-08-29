// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
#define V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_

// Do only include this header for implementing new Interface of the
// WasmFullDecoder.

#include <inttypes.h>

#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/wrappers.h"
#include "src/base/small-vector.h"
#include "src/base/strings.h"
#include "src/utils/bit-vector.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmGlobal;
struct WasmException;

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST_FORMAT "  @%-8d #%-30s|"

// Return the evaluation of `condition` if validate==true, DCHECK that it's
// true and always return true otherwise.
#define VALIDATE(condition)                \
  (validate ? V8_LIKELY(condition) : [&] { \
    DCHECK(condition);                     \
    return true;                           \
  }())

#define CHECK_PROTOTYPE_OPCODE(feat)                                         \
  DCHECK(this->module_->origin == kWasmOrigin);                              \
  if (!VALIDATE(this->enabled_.has_##feat())) {                              \
    this->DecodeError(                                                       \
        "Invalid opcode 0x%02x (enable with --experimental-wasm-" #feat ")", \
        opcode);                                                             \
    return 0;                                                                \
  }                                                                          \
  this->detected_->Add(kFeature_##feat);

#define ATOMIC_OP_LIST(V)                \
  V(AtomicNotify, Uint32)                \
  V(I32AtomicWait, Uint32)               \
  V(I64AtomicWait, Uint64)               \
  V(I32AtomicLoad, Uint32)               \
  V(I64AtomicLoad, Uint64)               \
  V(I32AtomicLoad8U, Uint8)              \
  V(I32AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad8U, Uint8)              \
  V(I64AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad32U, Uint32)            \
  V(I32AtomicAdd, Uint32)                \
  V(I32AtomicAdd8U, Uint8)               \
  V(I32AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd, Uint64)                \
  V(I64AtomicAdd8U, Uint8)               \
  V(I64AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd32U, Uint32)             \
  V(I32AtomicSub, Uint32)                \
  V(I64AtomicSub, Uint64)                \
  V(I32AtomicSub8U, Uint8)               \
  V(I32AtomicSub16U, Uint16)             \
  V(I64AtomicSub8U, Uint8)               \
  V(I64AtomicSub16U, Uint16)             \
  V(I64AtomicSub32U, Uint32)             \
  V(I32AtomicAnd, Uint32)                \
  V(I64AtomicAnd, Uint64)                \
  V(I32AtomicAnd8U, Uint8)               \
  V(I32AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd8U, Uint8)               \
  V(I64AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd32U, Uint32)             \
  V(I32AtomicOr, Uint32)                 \
  V(I64AtomicOr, Uint64)                 \
  V(I32AtomicOr8U, Uint8)                \
  V(I32AtomicOr16U, Uint16)              \
  V(I64AtomicOr8U, Uint8)                \
  V(I64AtomicOr16U, Uint16)              \
  V(I64AtomicOr32U, Uint32)              \
  V(I32AtomicXor, Uint32)                \
  V(I64AtomicXor, Uint64)                \
  V(I32AtomicXor8U, Uint8)               \
  V(I32AtomicXor16U, Uint16)             \
  V(I64AtomicXor8U, Uint8)               \
  V(I64AtomicXor16U, Uint16)             \
  V(I64AtomicXor32U, Uint32)             \
  V(I32AtomicExchange, Uint32)           \
  V(I64AtomicExchange, Uint64)           \
  V(I32AtomicExchange8U, Uint8)          \
  V(I32AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange8U, Uint8)          \
  V(I64AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange32U, Uint32)        \
  V(I32AtomicCompareExchange, Uint32)    \
  V(I64AtomicCompareExchange, Uint64)    \
  V(I32AtomicCompareExchange8U, Uint8)   \
  V(I32AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange8U, Uint8)   \
  V(I64AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange32U, Uint32)

#define ATOMIC_STORE_OP_LIST(V) \
  V(I32AtomicStore, Uint32)     \
  V(I64AtomicStore, Uint64)     \
  V(I32AtomicStore8U, Uint8)    \
  V(I32AtomicStore16U, Uint16)  \
  V(I64AtomicStore8U, Uint8)    \
  V(I64AtomicStore16U, Uint16)  \
  V(I64AtomicStore32U, Uint32)

// Decoder error with explicit PC and format arguments.
template <Decoder::ValidateFlag validate, typename... Args>
void DecodeError(Decoder* decoder, const byte* pc, const char* str,
                 Args&&... args) {
  CHECK(validate == Decoder::kFullValidation ||
        validate == Decoder::kBooleanValidation);
  STATIC_ASSERT(sizeof...(Args) > 0);
  if (validate == Decoder::kBooleanValidation) {
    decoder->MarkError();
  } else {
    decoder->errorf(pc, str, std::forward<Args>(args)...);
  }
}

// Decoder error with explicit PC and no format arguments.
template <Decoder::ValidateFlag validate>
void DecodeError(Decoder* decoder, const byte* pc, const char* str) {
  CHECK(validate == Decoder::kFullValidation ||
        validate == Decoder::kBooleanValidation);
  if (validate == Decoder::kBooleanValidation) {
    decoder->MarkError();
  } else {
    decoder->error(pc, str);
  }
}

// Decoder error without explicit PC, but with format arguments.
template <Decoder::ValidateFlag validate, typename... Args>
void DecodeError(Decoder* decoder, const char* str, Args&&... args) {
  CHECK(validate == Decoder::kFullValidation ||
        validate == Decoder::kBooleanValidation);
  STATIC_ASSERT(sizeof...(Args) > 0);
  if (validate == Decoder::kBooleanValidation) {
    decoder->MarkError();
  } else {
    decoder->errorf(str, std::forward<Args>(args)...);
  }
}

// Decoder error without explicit PC and without format arguments.
template <Decoder::ValidateFlag validate>
void DecodeError(Decoder* decoder, const char* str) {
  CHECK(validate == Decoder::kFullValidation ||
        validate == Decoder::kBooleanValidation);
  if (validate == Decoder::kBooleanValidation) {
    decoder->MarkError();
  } else {
    decoder->error(str);
  }
}

namespace value_type_reader {

V8_INLINE WasmFeature feature_for_heap_type(HeapType heap_type) {
  switch (heap_type.representation()) {
    case HeapType::kFunc:
    case HeapType::kExtern:
      return WasmFeature::kFeature_reftypes;
    case HeapType::kEq:
    case HeapType::kI31:
    case HeapType::kData:
    case HeapType::kAny:
      return WasmFeature::kFeature_gc;
    case HeapType::kBottom:
      UNREACHABLE();
  }
}

// If {module} is not null, the read index will be checked against the module's
// type capacity.
template <Decoder::ValidateFlag validate>
HeapType read_heap_type(Decoder* decoder, const byte* pc,
                        uint32_t* const length, const WasmModule* module,
                        const WasmFeatures& enabled) {
  int64_t heap_index = decoder->read_i33v<validate>(pc, length, "heap type");
  if (heap_index < 0) {
    int64_t min_1_byte_leb128 = -64;
    if (!VALIDATE(heap_index >= min_1_byte_leb128)) {
      DecodeError<validate>(decoder, pc, "Unknown heap type %" PRId64,
                            heap_index);
      return HeapType(HeapType::kBottom);
    }
    uint8_t uint_7_mask = 0x7F;
    uint8_t code = static_cast<ValueTypeCode>(heap_index) & uint_7_mask;
    switch (code) {
      case kFuncRefCode:
      case kEqRefCode:
      case kExternRefCode:
      case kI31RefCode:
      case kDataRefCode:
      case kAnyRefCode: {
        HeapType result = HeapType::from_code(code);
        if (!VALIDATE(enabled.contains(feature_for_heap_type(result)))) {
          DecodeError<validate>(
              decoder, pc,
              "invalid heap type '%s', enable with --experimental-wasm-%s",
              result.name().c_str(),
              WasmFeatures::name_for_feature(feature_for_heap_type(result)));
          return HeapType(HeapType::kBottom);
        }
        return result;
      }
      default:
        DecodeError<validate>(decoder, pc, "Unknown heap type %" PRId64,
                              heap_index);
        return HeapType(HeapType::kBottom);
    }
    UNREACHABLE();
  } else {
    if (!VALIDATE(enabled.has_typed_funcref())) {
      DecodeError<validate>(decoder, pc,
                            "Invalid indexed heap type, enable with "
                            "--experimental-wasm-typed-funcref");
      return HeapType(HeapType::kBottom);
    }
    uint32_t type_index = static_cast<uint32_t>(heap_index);
    if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
      DecodeError<validate>(
          decoder, pc,
          "Type index %u is greater than the maximum number %zu "
          "of type definitions supported by V8",
          type_index, kV8MaxWasmTypes);
      return HeapType(HeapType::kBottom);
    }
    // We use capacity over size so this works mid-DecodeTypeSection.
    if (!VALIDATE(module == nullptr || type_index < module->types.capacity())) {
      DecodeError<validate>(decoder, pc, "Type index %u is out of bounds",
                            type_index);
      return HeapType(HeapType::kBottom);
    }
    return HeapType(type_index);
  }
}

// Read a value type starting at address {pc} using {decoder}.
// No bytes are consumed.
// The length of the read value type is written in {length}.
// Registers an error for an invalid type only if {validate} is not
// kNoValidate.
template <Decoder::ValidateFlag validate>
ValueType read_value_type(Decoder* decoder, const byte* pc,
                          uint32_t* const length, const WasmModule* module,
                          const WasmFeatures& enabled) {
  *length = 1;
  byte val = decoder->read_u8<validate>(pc, "value type opcode");
  if (decoder->failed()) {
    *length = 0;
    return kWasmBottom;
  }
  ValueTypeCode code = static_cast<ValueTypeCode>(val);
  switch (code) {
    case kFuncRefCode:
    case kEqRefCode:
    case kExternRefCode:
    case kI31RefCode:
    case kDataRefCode:
    case kAnyRefCode: {
      HeapType heap_type = HeapType::from_code(code);
      Nullability nullability = code == kI31RefCode || code == kDataRefCode
                                    ? kNonNullable
                                    : kNullable;
      ValueType result = ValueType::Ref(heap_type, nullability);
      if (!VALIDATE(enabled.contains(feature_for_heap_type(heap_type)))) {
        DecodeError<validate>(
            decoder, pc,
            "invalid value type '%s', enable with --experimental-wasm-%s",
            result.name().c_str(),
            WasmFeatures::name_for_feature(feature_for_heap_type(heap_type)));
        return kWasmBottom;
      }
      return result;
    }
    case kI32Code:
      return kWasmI32;
    case kI64Code:
      return kWasmI64;
    case kF32Code:
      return kWasmF32;
    case kF64Code:
      return kWasmF64;
    case kRefCode:
    case kOptRefCode: {
      Nullability nullability = code == kOptRefCode ? kNullable : kNonNullable;
      if (!VALIDATE(enabled.has_typed_funcref())) {
        DecodeError<validate>(decoder, pc,
                              "Invalid type '(ref%s <heaptype>)', enable with "
                              "--experimental-wasm-typed-funcref",
                              nullability == kNullable ? " null" : "");
        return kWasmBottom;
      }
      HeapType heap_type =
          read_heap_type<validate>(decoder, pc + 1, length, module, enabled);
      *length += 1;
      return heap_type.is_bottom() ? kWasmBottom
                                   : ValueType::Ref(heap_type, nullability);
    }
    case kRttWithDepthCode: {
      if (!VALIDATE(enabled.has_gc())) {
        DecodeError<validate>(
            decoder, pc,
            "invalid value type 'rtt', enable with --experimental-wasm-gc");
        return kWasmBottom;
      }
      uint32_t depth = decoder->read_u32v<validate>(pc + 1, length, "depth");
      *length += 1;
      if (!VALIDATE(depth <= kV8MaxRttSubtypingDepth)) {
        DecodeError<validate>(
            decoder, pc,
            "subtyping depth %u is greater than the maximum depth "
            "%u supported by V8",
            depth, kV8MaxRttSubtypingDepth);
        return kWasmBottom;
      }
      uint32_t type_index_length;
      uint32_t type_index =
          decoder->read_u32v<validate>(pc + *length, &type_index_length);
      *length += type_index_length;
      if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
        DecodeError<validate>(
            decoder, pc,
            "Type index %u is greater than the maximum number %zu "
            "of type definitions supported by V8",
            type_index, kV8MaxWasmTypes);
        return kWasmBottom;
      }
      // We use capacity over size so this works mid-DecodeTypeSection.
      if (!VALIDATE(module == nullptr ||
                    type_index < module->types.capacity())) {
        DecodeError<validate>(decoder, pc, "Type index %u is out of bounds",
                              type_index);
        return kWasmBottom;
      }
      return ValueType::Rtt(type_index, depth);
    }
    case kRttCode: {
      if (!VALIDATE(enabled.has_gc())) {
        DecodeError<validate>(
            decoder, pc,
            "invalid value type 'rtt', enable with --experimental-wasm-gc");
        return kWasmBottom;
      }
      uint32_t type_index = decoder->read_u32v<validate>(pc + 1, length);
      *length += 1;
      if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
        DecodeError<validate>(
            decoder, pc,
            "Type index %u is greater than the maximum number %zu "
            "of type definitions supported by V8",
            type_index, kV8MaxWasmTypes);
        return kWasmBottom;
      }
      // We use capacity over size so this works mid-DecodeTypeSection.
      if (!VALIDATE(module == nullptr ||
                    type_index < module->types.capacity())) {
        DecodeError<validate>(decoder, pc, "Type index %u is out of bounds",
                              type_index);
        return kWasmBottom;
      }
      return ValueType::Rtt(type_index);
    }
    case kS128Code: {
      if (!VALIDATE(enabled.has_simd())) {
        DecodeError<validate>(
            decoder, pc,
            "invalid value type 's128', enable with --experimental-wasm-simd");
        return kWasmBottom;
      }
      return kWasmS128;
    }
    // Although these codes are included in ValueTypeCode, they technically
    // do not correspond to value types and are only used in specific
    // contexts. The caller of this function is responsible for handling them.
    case kVoidCode:
    case kI8Code:
    case kI16Code:
      if (validate) {
        DecodeError<validate>(decoder, pc, "invalid value type 0x%x", code);
      }
      return kWasmBottom;
  }
  // Anything that doesn't match an enumeration value is an invalid type code.
  if (validate) {
    DecodeError<validate>(decoder, pc, "invalid value type 0x%x", code);
  }
  return kWasmBottom;
}
}  // namespace value_type_reader

enum DecodingMode { kFunctionBody, kInitExpression };

// Helpers for decoding different kinds of immediates which follow bytecodes.
template <Decoder::ValidateFlag validate>
struct ImmI32Immediate {
  int32_t value;
  uint32_t length;
  ImmI32Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i32v<validate>(pc, &length, "immi32");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI64Immediate {
  int64_t value;
  uint32_t length;
  ImmI64Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i64v<validate>(pc, &length, "immi64");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF32Immediate {
  float value;
  uint32_t length = 4;
  ImmF32Immediate(Decoder* decoder, const byte* pc) {
    // We can't use bit_cast here because calling any helper function that
    // returns a float would potentially flip NaN bits per C++ semantics, so we
    // have to inline the memcpy call directly.
    uint32_t tmp = decoder->read_u32<validate>(pc, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF64Immediate {
  double value;
  uint32_t length = 8;
  ImmF64Immediate(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->read_u64<validate>(pc, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

// This is different than IndexImmediate because {index} is a byte.
template <Decoder::ValidateFlag validate>
struct MemoryIndexImmediate {
  uint8_t index = 0;
  uint32_t length = 1;
  MemoryIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<validate>(pc, "memory index");
  }
};

// Parent class for all Immediates which read a u32v index value in their
// constructor.
template <Decoder::ValidateFlag validate>
struct IndexImmediate {
  uint32_t index;
  uint32_t length;

  IndexImmediate(Decoder* decoder, const byte* pc, const char* name) {
    index = decoder->read_u32v<validate>(pc, &length, name);
  }
};

template <Decoder::ValidateFlag validate>
struct ExceptionIndexImmediate : public IndexImmediate<validate> {
  const WasmException* exception = nullptr;

  ExceptionIndexImmediate(Decoder* decoder, const byte* pc)
      : IndexImmediate<validate>(decoder, pc, "exception index") {}
};

template <Decoder::ValidateFlag validate>
struct GlobalIndexImmediate : public IndexImmediate<validate> {
  const WasmGlobal* global = nullptr;

  GlobalIndexImmediate(Decoder* decoder, const byte* pc)
      : IndexImmediate<validate>(decoder, pc, "global index") {}
};

template <Decoder::ValidateFlag validate>
struct StructIndexImmediate : public IndexImmediate<validate> {
  const StructType* struct_type = nullptr;

  StructIndexImmediate(Decoder* decoder, const byte* pc)
      : IndexImmediate<validate>(decoder, pc, "struct index") {}
};

template <Decoder::ValidateFlag validate>
struct ArrayIndexImmediate : public IndexImmediate<validate> {
  const ArrayType* array_type = nullptr;

  ArrayIndexImmediate(Decoder* decoder, const byte* pc)
      : IndexImmediate<validate>(decoder, pc, "array index") {}
};
template <Decoder::ValidateFlag validate>
struct CallFunctionImmediate : public IndexImmediate<validate> {
  const FunctionSig* sig = nullptr;

  CallFunctionImmediate(Decoder* decoder, const byte* pc)
      : IndexImmediate<validate>(decoder, pc, "function index") {}
};

template <Decoder::ValidateFlag validate>
struct SelectTypeImmediate {
  uint32_t length;
  ValueType type;

  SelectTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                      const byte* pc, const WasmModule* module) {
    uint8_t num_types =
        decoder->read_u32v<validate>(pc, &length, "number of select types");
    if (!VALIDATE(num_types == 1)) {
      DecodeError<validate>(
          decoder, pc + 1,
          "Invalid number of types. Select accepts exactly one type");
      return;
    }
    uint32_t type_length;
    type = value_type_reader::read_value_type<validate>(
        decoder, pc + length, &type_length, module, enabled);
    length += type_length;
  }
};

template <Decoder::ValidateFlag validate>
struct BlockTypeImmediate {
  uint32_t length = 1;
  ValueType type = kWasmVoid;
  uint32_t sig_index = 0;
  const FunctionSig* sig = nullptr;

  BlockTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                     const byte* pc, const WasmModule* module) {
    int64_t block_type =
        decoder->read_i33v<validate>(pc, &length, "block type");
    if (block_type < 0) {
      // All valid negative types are 1 byte in length, so we check against the
      // minimum 1-byte LEB128 value.
      constexpr int64_t min_1_byte_leb128 = -64;
      if (!VALIDATE(block_type >= min_1_byte_leb128)) {
        DecodeError<validate>(decoder, pc, "invalid block type %" PRId64,
                              block_type);
        return;
      }
      if (static_cast<ValueTypeCode>(block_type & 0x7F) == kVoidCode) return;
      type = value_type_reader::read_value_type<validate>(decoder, pc, &length,
                                                          module, enabled);
    } else {
      type = kWasmBottom;
      sig_index = static_cast<uint32_t>(block_type);
    }
  }

  uint32_t in_arity() const {
    if (type != kWasmBottom) return 0;
    return static_cast<uint32_t>(sig->parameter_count());
  }
  uint32_t out_arity() const {
    if (type == kWasmVoid) return 0;
    if (type != kWasmBottom) return 1;
    return static_cast<uint32_t>(sig->return_count());
  }
  ValueType in_type(uint32_t index) {
    DCHECK_EQ(kWasmBottom, type);
    return sig->GetParam(index);
  }
  ValueType out_type(uint32_t index) {
    if (type == kWasmBottom) return sig->GetReturn(index);
    DCHECK_NE(kWasmVoid, type);
    DCHECK_EQ(0, index);
    return type;
  }
};

template <Decoder::ValidateFlag validate>
struct BranchDepthImmediate {
  uint32_t depth;
  uint32_t length;
  BranchDepthImmediate(Decoder* decoder, const byte* pc) {
    depth = decoder->read_u32v<validate>(pc, &length, "branch depth");
  }
};

template <Decoder::ValidateFlag validate>
struct FieldImmediate {
  StructIndexImmediate<validate> struct_imm;
  IndexImmediate<validate> field_imm;
  uint32_t length;
  FieldImmediate(Decoder* decoder, const byte* pc)
      : struct_imm(decoder, pc),
        field_imm(decoder, pc + struct_imm.length, "field index"),
        length(struct_imm.length + field_imm.length) {}
};

template <Decoder::ValidateFlag validate>
struct CallIndirectImmediate {
  IndexImmediate<validate> sig_imm;
  IndexImmediate<validate> table_imm;
  uint32_t length;
  const FunctionSig* sig = nullptr;
  CallIndirectImmediate(Decoder* decoder, const byte* pc)
      : sig_imm(decoder, pc, "singature index"),
        table_imm(decoder, pc + sig_imm.length, "table index"),
        length(sig_imm.length + table_imm.length) {}
};

template <Decoder::ValidateFlag validate>
struct BranchTableImmediate {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  BranchTableImmediate(Decoder* decoder, const byte* pc) {
    start = pc;
    uint32_t len = 0;
    table_count = decoder->read_u32v<validate>(pc, &len, "table count");
    table = pc + len;
  }
};

// A helper to iterate over a branch table.
template <Decoder::ValidateFlag validate>
class BranchTableIterator {
 public:
  uint32_t cur_index() { return index_; }
  bool has_next() { return VALIDATE(decoder_->ok()) && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    uint32_t length;
    uint32_t result =
        decoder_->read_u32v<validate>(pc_, &length, "branch table entry");
    pc_ += length;
    return result;
  }
  // length, including the length of the {BranchTableImmediate}, but not the
  // opcode.
  uint32_t length() {
    while (has_next()) next();
    return static_cast<uint32_t>(pc_ - start_);
  }
  const byte* pc() { return pc_; }

  BranchTableIterator(Decoder* decoder,
                      const BranchTableImmediate<validate>& imm)
      : decoder_(decoder),
        start_(imm.start),
        pc_(imm.table),
        table_count_(imm.table_count) {}

 private:
  Decoder* const decoder_;
  const byte* start_;
  const byte* pc_;
  uint32_t index_ = 0;          // the current index.
  const uint32_t table_count_;  // the count of entries, not including default.
};

template <Decoder::ValidateFlag validate,
          DecodingMode decoding_mode = kFunctionBody>
class WasmDecoder;

template <Decoder::ValidateFlag validate>
struct MemoryAccessImmediate {
  uint32_t alignment;
  uint64_t offset;
  uint32_t length = 0;
  MemoryAccessImmediate(Decoder* decoder, const byte* pc,
                        uint32_t max_alignment, bool is_memory64) {
    uint32_t alignment_length;
    alignment =
        decoder->read_u32v<validate>(pc, &alignment_length, "alignment");
    if (!VALIDATE(alignment <= max_alignment)) {
      DecodeError<validate>(
          decoder, pc,
          "invalid alignment; expected maximum alignment is %u, "
          "actual alignment is %u",
          max_alignment, alignment);
    }
    uint32_t offset_length;
    offset = is_memory64 ? decoder->read_u64v<validate>(
                               pc + alignment_length, &offset_length, "offset")
                         : decoder->read_u32v<validate>(
                               pc + alignment_length, &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Immediate for SIMD lane operations.
template <Decoder::ValidateFlag validate>
struct SimdLaneImmediate {
  uint8_t lane;
  uint32_t length = 1;

  SimdLaneImmediate(Decoder* decoder, const byte* pc) {
    lane = decoder->read_u8<validate>(pc, "lane");
  }
};

// Immediate for SIMD S8x16 shuffle operations.
template <Decoder::ValidateFlag validate>
struct Simd128Immediate {
  uint8_t value[kSimd128Size] = {0};

  Simd128Immediate(Decoder* decoder, const byte* pc) {
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      value[i] = decoder->read_u8<validate>(pc + i, "value");
    }
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryInitImmediate {
  IndexImmediate<validate> data_segment;
  MemoryIndexImmediate<validate> memory;
  uint32_t length;

  MemoryInitImmediate(Decoder* decoder, const byte* pc)
      : data_segment(decoder, pc, "data segment index"),
        memory(decoder, pc + data_segment.length),
        length(data_segment.length + memory.length) {}
};

template <Decoder::ValidateFlag validate>
struct MemoryCopyImmediate {
  MemoryIndexImmediate<validate> memory_src;
  MemoryIndexImmediate<validate> memory_dst;
  uint32_t length;

  MemoryCopyImmediate(Decoder* decoder, const byte* pc)
      : memory_src(decoder, pc),
        memory_dst(decoder, pc + memory_src.length),
        length(memory_src.length + memory_dst.length) {}
};

template <Decoder::ValidateFlag validate>
struct TableInitImmediate {
  IndexImmediate<validate> element_segment;
  IndexImmediate<validate> table;
  uint32_t length;

  TableInitImmediate(Decoder* decoder, const byte* pc)
      : element_segment(decoder, pc, "element segment index"),
        table(decoder, pc + element_segment.length, "table index"),
        length(element_segment.length + table.length) {}
};

template <Decoder::ValidateFlag validate>
struct TableCopyImmediate {
  IndexImmediate<validate> table_dst;
  IndexImmediate<validate> table_src;
  uint32_t length;

  TableCopyImmediate(Decoder* decoder, const byte* pc)
      : table_dst(decoder, pc, "table index"),
        table_src(decoder, pc + table_dst.length, "table index"),
        length(table_src.length + table_dst.length) {}
};

template <Decoder::ValidateFlag validate>
struct HeapTypeImmediate {
  uint32_t length = 1;
  HeapType type;
  HeapTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                    const byte* pc, const WasmModule* module)
      : type(value_type_reader::read_heap_type<validate>(decoder, pc, &length,
                                                         module, enabled)) {}
};

template <Decoder::ValidateFlag validate>
struct PcForErrors {
  PcForErrors(const byte* /* pc */) {}

  const byte* pc() const { return nullptr; }
};

template <>
struct PcForErrors<Decoder::kFullValidation> {
  const byte* pc_for_errors = nullptr;

  PcForErrors(const byte* pc) : pc_for_errors(pc) {}

  const byte* pc() const { return pc_for_errors; }
};

// An entry on the value stack.
template <Decoder::ValidateFlag validate>
struct ValueBase : public PcForErrors<validate> {
  ValueType type = kWasmVoid;

  ValueBase(const byte* pc, ValueType type)
      : PcForErrors<validate>(pc), type(type) {}
};

template <typename Value>
struct Merge {
  uint32_t arity = 0;
  union {  // Either multiple values or a single value.
    Value* array;
    Value first;
  } vals = {nullptr};  // Initialize {array} with {nullptr}.

  // Tracks whether this merge was ever reached. Uses precise reachability, like
  // Reachability::kReachable.
  bool reached;

  explicit Merge(bool reached = false) : reached(reached) {}

  Value& operator[](uint32_t i) {
    DCHECK_GT(arity, i);
    return arity == 1 ? vals.first : vals.array[i];
  }
};

enum ControlKind : uint8_t {
  kControlIf,
  kControlIfElse,
  kControlBlock,
  kControlLoop,
  kControlLet,
  kControlTry,
  kControlTryCatch,
  kControlTryCatchAll,
};

enum Reachability : uint8_t {
  // reachable code.
  kReachable,
  // reachable code in unreachable block (implies normal validation).
  kSpecOnlyReachable,
  // code unreachable in its own block (implies polymorphic validation).
  kUnreachable
};

// An entry on the control stack (i.e. if, block, loop, or try).
template <typename Value, Decoder::ValidateFlag validate>
struct ControlBase : public PcForErrors<validate> {
  ControlKind kind = kControlBlock;
  uint32_t locals_count = 0;  // Additional locals introduced in this 'let'.
  uint32_t stack_depth = 0;   // Stack height at the beginning of the construct.
  uint32_t init_stack_depth = 0;  // Height of "locals initialization" stack
                                  // at the beginning of the construct.
  int32_t previous_catch = -1;  // Depth of the innermost catch containing this
                                // 'try'.
  Reachability reachability = kReachable;

  // Values merged into the start or end of this control construct.
  Merge<Value> start_merge;
  Merge<Value> end_merge;

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ControlBase);

  ControlBase(ControlKind kind, uint32_t locals_count, uint32_t stack_depth,
              uint32_t init_stack_depth, const uint8_t* pc,
              Reachability reachability)
      : PcForErrors<validate>(pc),
        kind(kind),
        locals_count(locals_count),
        stack_depth(stack_depth),
        init_stack_depth(init_stack_depth),
        reachability(reachability),
        start_merge(reachability == kReachable) {
    DCHECK(kind == kControlLet || locals_count == 0);
  }

  // Check whether the current block is reachable.
  bool reachable() const { return reachability == kReachable; }

  // Check whether the rest of the block is unreachable.
  // Note that this is different from {!reachable()}, as there is also the
  // "indirect unreachable state", for which both {reachable()} and
  // {unreachable()} return false.
  bool unreachable() const { return reachability == kUnreachable; }

  // Return the reachability of new control structs started in this block.
  Reachability innerReachability() const {
    return reachability == kReachable ? kReachable : kSpecOnlyReachable;
  }

  bool is_if() const { return is_onearmed_if() || is_if_else(); }
  bool is_onearmed_if() const { return kind == kControlIf; }
  bool is_if_else() const { return kind == kControlIfElse; }
  bool is_block() const { return kind == kControlBlock; }
  bool is_let() const { return kind == kControlLet; }
  bool is_loop() const { return kind == kControlLoop; }
  bool is_incomplete_try() const { return kind == kControlTry; }
  bool is_try_catch() const { return kind == kControlTryCatch; }
  bool is_try_catchall() const { return kind == kControlTryCatchAll; }
  bool is_try() const {
    return is_incomplete_try() || is_try_catch() || is_try_catchall();
  }

  Merge<Value>* br_merge() {
    return is_loop() ? &this->start_merge : &this->end_merge;
  }
};

// This is the list of callback functions that an interface for the
// WasmFullDecoder should implement.
// F(Name, args...)
#define INTERFACE_FUNCTIONS(F)    \
  INTERFACE_META_FUNCTIONS(F)     \
  INTERFACE_CONSTANT_FUNCTIONS(F) \
  INTERFACE_NON_CONSTANT_FUNCTIONS(F)

#define INTERFACE_META_FUNCTIONS(F)    \
  F(StartFunction)                     \
  F(StartFunctionBody, Control* block) \
  F(FinishFunction)                    \
  F(OnFirstError)                      \
  F(NextInstruction, WasmOpcode)       \
  F(Forward, const Value& from, Value* to)

#define INTERFACE_CONSTANT_FUNCTIONS(F)                                   \
  F(I32Const, Value* result, int32_t value)                               \
  F(I64Const, Value* result, int64_t value)                               \
  F(F32Const, Value* result, float value)                                 \
  F(F64Const, Value* result, double value)                                \
  F(S128Const, Simd128Immediate<validate>& imm, Value* result)            \
  F(RefNull, ValueType type, Value* result)                               \
  F(RefFunc, uint32_t function_index, Value* result)                      \
  F(GlobalGet, Value* result, const GlobalIndexImmediate<validate>& imm)  \
  F(StructNewWithRtt, const StructIndexImmediate<validate>& imm,          \
    const Value& rtt, const Value args[], Value* result)                  \
  F(ArrayInit, const ArrayIndexImmediate<validate>& imm,                  \
    const base::Vector<Value>& elements, const Value& rtt, Value* result) \
  F(RttCanon, uint32_t type_index, Value* result)                         \
  F(RttSub, uint32_t type_index, const Value& parent, Value* result,      \
    WasmRttSubMode mode)                                                  \
  F(DoReturn, uint32_t drop_values)

#define INTERFACE_NON_CONSTANT_FUNCTIONS(F)                                   \
  /* Control: */                                                              \
  F(Block, Control* block)                                                    \
  F(Loop, Control* block)                                                     \
  F(Try, Control* block)                                                      \
  F(If, const Value& cond, Control* if_block)                                 \
  F(FallThruTo, Control* c)                                                   \
  F(PopControl, Control* block)                                               \
  /* Instructions: */                                                         \
  F(UnOp, WasmOpcode opcode, const Value& value, Value* result)               \
  F(BinOp, WasmOpcode opcode, const Value& lhs, const Value& rhs,             \
    Value* result)                                                            \
  F(RefAsNonNull, const Value& arg, Value* result)                            \
  F(Drop)                                                                     \
  F(LocalGet, Value* result, const IndexImmediate<validate>& imm)             \
  F(LocalSet, const Value& value, const IndexImmediate<validate>& imm)        \
  F(LocalTee, const Value& value, Value* result,                              \
    const IndexImmediate<validate>& imm)                                      \
  F(AllocateLocals, base::Vector<Value> local_values)                         \
  F(DeallocateLocals, uint32_t count)                                         \
  F(GlobalSet, const Value& value, const GlobalIndexImmediate<validate>& imm) \
  F(TableGet, const Value& index, Value* result,                              \
    const IndexImmediate<validate>& imm)                                      \
  F(TableSet, const Value& index, const Value& value,                         \
    const IndexImmediate<validate>& imm)                                      \
  F(Trap, TrapReason reason)                                                  \
  F(NopForTestingUnsupportedInLiftoff)                                        \
  F(Select, const Value& cond, const Value& fval, const Value& tval,          \
    Value* result)                                                            \
  F(BrOrRet, uint32_t depth, uint32_t drop_values)                            \
  F(BrIf, const Value& cond, uint32_t depth)                                  \
  F(BrTable, const BranchTableImmediate<validate>& imm, const Value& key)     \
  F(Else, Control* if_block)                                                  \
  F(LoadMem, LoadType type, const MemoryAccessImmediate<validate>& imm,       \
    const Value& index, Value* result)                                        \
  F(LoadTransform, LoadType type, LoadTransformationKind transform,           \
    const MemoryAccessImmediate<validate>& imm, const Value& index,           \
    Value* result)                                                            \
  F(LoadLane, LoadType type, const Value& value, const Value& index,          \
    const MemoryAccessImmediate<validate>& imm, const uint8_t laneidx,        \
    Value* result)                                                            \
  F(StoreMem, StoreType type, const MemoryAccessImmediate<validate>& imm,     \
    const Value& index, const Value& value)                                   \
  F(StoreLane, StoreType type, const MemoryAccessImmediate<validate>& imm,    \
    const Value& index, const Value& value, const uint8_t laneidx)            \
  F(CurrentMemoryPages, Value* result)                                        \
  F(MemoryGrow, const Value& value, Value* result)                            \
  F(CallDirect, const CallFunctionImmediate<validate>& imm,                   \
    const Value args[], Value returns[])                                      \
  F(CallIndirect, const Value& index,                                         \
    const CallIndirectImmediate<validate>& imm, const Value args[],           \
    Value returns[])                                                          \
  F(CallRef, const Value& func_ref, const FunctionSig* sig,                   \
    uint32_t sig_index, const Value args[], const Value returns[])            \
  F(ReturnCallRef, const Value& func_ref, const FunctionSig* sig,             \
    uint32_t sig_index, const Value args[])                                   \
  F(ReturnCall, const CallFunctionImmediate<validate>& imm,                   \
    const Value args[])                                                       \
  F(ReturnCallIndirect, const Value& index,                                   \
    const CallIndirectImmediate<validate>& imm, const Value args[])           \
  F(BrOnNull, const Value& ref_object, uint32_t depth)                        \
  F(BrOnNonNull, const Value& ref_object, uint32_t depth)                     \
  F(SimdOp, WasmOpcode opcode, base::Vector<Value> args, Value* result)       \
  F(SimdLaneOp, WasmOpcode opcode, const SimdLaneImmediate<validate>& imm,    \
    const base::Vector<Value> inputs, Value* result)                          \
  F(S128Const, const Simd128Immediate<validate>& imm, Value* result)          \
  F(Simd8x16ShuffleOp, const Simd128Immediate<validate>& imm,                 \
    const Value& input0, const Value& input1, Value* result)                  \
  F(Throw, const ExceptionIndexImmediate<validate>& imm,                      \
    const base::Vector<Value>& args)                                          \
  F(Rethrow, Control* block)                                                  \
  F(CatchException, const ExceptionIndexImmediate<validate>& imm,             \
    Control* block, base::Vector<Value> caught_values)                        \
  F(Delegate, uint32_t depth, Control* block)                                 \
  F(CatchAll, Control* block)                                                 \
  F(AtomicOp, WasmOpcode opcode, base::Vector<Value> args,                    \
    const MemoryAccessImmediate<validate>& imm, Value* result)                \
  F(AtomicFence)                                                              \
  F(MemoryInit, const MemoryInitImmediate<validate>& imm, const Value& dst,   \
    const Value& src, const Value& size)                                      \
  F(DataDrop, const IndexImmediate<validate>& imm)                            \
  F(MemoryCopy, const MemoryCopyImmediate<validate>& imm, const Value& dst,   \
    const Value& src, const Value& size)                                      \
  F(MemoryFill, const MemoryIndexImmediate<validate>& imm, const Value& dst,  \
    const Value& value, const Value& size)                                    \
  F(TableInit, const TableInitImmediate<validate>& imm,                       \
    base::Vector<Value> args)                                                 \
  F(ElemDrop, const IndexImmediate<validate>& imm)                            \
  F(TableCopy, const TableCopyImmediate<validate>& imm,                       \
    base::Vector<Value> args)                                                 \
  F(TableGrow, const IndexImmediate<validate>& imm, const Value& value,       \
    const Value& delta, Value* result)                                        \
  F(TableSize, const IndexImmediate<validate>& imm, Value* result)            \
  F(TableFill, const IndexImmediate<validate>& imm, const Value& start,       \
    const Value& value, const Value& count)                                   \
  F(StructNewDefault, const StructIndexImmediate<validate>& imm,              \
    const Value& rtt, Value* result)                                          \
  F(StructGet, const Value& struct_object,                                    \
    const FieldImmediate<validate>& field, bool is_signed, Value* result)     \
  F(StructSet, const Value& struct_object,                                    \
    const FieldImmediate<validate>& field, const Value& field_value)          \
  F(ArrayNewWithRtt, const ArrayIndexImmediate<validate>& imm,                \
    const Value& length, const Value& initial_value, const Value& rtt,        \
    Value* result)                                                            \
  F(ArrayNewDefault, const ArrayIndexImmediate<validate>& imm,                \
    const Value& length, const Value& rtt, Value* result)                     \
  F(ArrayGet, const Value& array_obj,                                         \
    const ArrayIndexImmediate<validate>& imm, const Value& index,             \
    bool is_signed, Value* result)                                            \
  F(ArraySet, const Value& array_obj,                                         \
    const ArrayIndexImmediate<validate>& imm, const Value& index,             \
    const Value& value)                                                       \
  F(ArrayLen, const Value& array_obj, Value* result)                          \
  F(ArrayCopy, const Value& src, const Value& src_index, const Value& dst,    \
    const Value& dst_index, const Value& length)                              \
  F(I31New, const Value& input, Value* result)                                \
  F(I31GetS, const Value& input, Value* result)                               \
  F(I31GetU, const Value& input, Value* result)                               \
  F(RefTest, const Value& obj, const Value& rtt, Value* result)               \
  F(RefCast, const Value& obj, const Value& rtt, Value* result)               \
  F(AssertNull, const Value& obj, Value* result)                              \
  F(BrOnCast, const Value& obj, const Value& rtt, Value* result_on_branch,    \
    uint32_t depth)                                                           \
  F(BrOnCastFail, const Value& obj, const Value& rtt,                         \
    Value* result_on_fallthrough, uint32_t depth)                             \
  F(RefIsFunc, const Value& object, Value* result)                            \
  F(RefIsData, const Value& object, Value* result)                            \
  F(RefIsI31, const Value& object, Value* result)                             \
  F(RefAsFunc, const Value& object, Value* result)                            \
  F(RefAsData, const Value& object, Value* result)                            \
  F(RefAsI31, const Value& object, Value* result)                             \
  F(BrOnFunc, const Value& object, Value* value_on_branch, uint32_t br_depth) \
  F(BrOnData, const Value& object, Value* value_on_branch, uint32_t br_depth) \
  F(BrOnI31, const Value& object, Value* value_on_branch, uint32_t br_depth)  \
  F(BrOnNonFunc, const Value& object, Value* value_on_fallthrough,            \
    uint32_t br_depth)                                                        \
  F(BrOnNonData, const Value& object, Value* value_on_fallthrough,            \
    uint32_t br_depth)                                                        \
  F(BrOnNonI31, const Value& object, Value* value_on_fallthrough,             \
    uint32_t br_depth)

// Generic Wasm bytecode decoder with utilities for decoding immediates,
// lengths, etc.
template <Decoder::ValidateFlag validate, DecodingMode decoding_mode>
class WasmDecoder : public Decoder {
 public:
  WasmDecoder(Zone* zone, const WasmModule* module, const WasmFeatures& enabled,
              WasmFeatures* detected, const FunctionSig* sig, const byte* start,
              const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, end, buffer_offset),
        local_types_(zone),
        initialized_locals_(zone),
        locals_initializers_stack_(zone),
        module_(module),
        enabled_(enabled),
        detected_(detected),
        sig_(sig) {}

  Zone* zone() const { return local_types_.get_allocator().zone(); }

  uint32_t num_locals() const {
    DCHECK_EQ(num_locals_, local_types_.size());
    return num_locals_;
  }

  ValueType local_type(uint32_t index) const { return local_types_[index]; }

  void InitializeLocalsFromSig() {
    DCHECK_NOT_NULL(sig_);
    DCHECK_EQ(0, this->local_types_.size());
    local_types_.assign(sig_->parameters().begin(), sig_->parameters().end());
    num_locals_ = static_cast<uint32_t>(sig_->parameters().size());
  }

  // Decodes local definitions in the current decoder.
  // Returns the number of newly defined locals, or -1 if decoding failed.
  // Writes the total length of decoded locals in {total_length}.
  // If {insert_position} is defined, the decoded locals will be inserted into
  // the {this->local_types_}. The decoder's pc is not advanced.
  int DecodeLocals(const byte* pc, uint32_t* total_length,
                   const base::Optional<uint32_t> insert_position) {
    uint32_t length;
    *total_length = 0;
    int total_count = 0;

    // The 'else' value is useless, we pass it for convenience.
    auto insert_iterator = insert_position.has_value()
                               ? local_types_.begin() + insert_position.value()
                               : local_types_.begin();

    // Decode local declarations, if any.
    uint32_t entries = read_u32v<validate>(pc, &length, "local decls count");
    if (!VALIDATE(ok())) {
      DecodeError(pc + *total_length, "invalid local decls count");
      return -1;
    }
    *total_length += length;
    TRACE("local decls count: %u\n", entries);

    while (entries-- > 0) {
      if (!VALIDATE(more())) {
        DecodeError(end(),
                    "expected more local decls but reached end of input");
        return -1;
      }

      uint32_t count =
          read_u32v<validate>(pc + *total_length, &length, "local count");
      if (!VALIDATE(ok())) {
        DecodeError(pc + *total_length, "invalid local count");
        return -1;
      }
      DCHECK_LE(local_types_.size(), kV8MaxWasmFunctionLocals);
      if (!VALIDATE(count <= kV8MaxWasmFunctionLocals - local_types_.size())) {
        DecodeError(pc + *total_length, "local count too large");
        return -1;
      }
      *total_length += length;

      ValueType type = value_type_reader::read_value_type<validate>(
          this, pc + *total_length, &length, this->module_, enabled_);
      if (!VALIDATE(type != kWasmBottom)) return -1;
      *total_length += length;
      total_count += count;

      if (insert_position.has_value()) {
        // Move the insertion iterator to the end of the newly inserted locals.
        insert_iterator =
            local_types_.insert(insert_iterator, count, type) + count;
        num_locals_ += count;
      }
    }

    DCHECK(ok());
    return total_count;
  }

  // Shorthand that forwards to the {DecodeError} functions above, passing our
  // {validate} flag.
  template <typename... Args>
  void DecodeError(Args... args) {
    wasm::DecodeError<validate>(this, std::forward<Args>(args)...);
  }

  // Returns a BitVector of length {locals_count + 1} representing the set of
  // variables that are assigned in the loop starting at {pc}. The additional
  // position at the end of the vector represents possible assignments to
  // the instance cache.
  static BitVector* AnalyzeLoopAssignment(WasmDecoder* decoder, const byte* pc,
                                          uint32_t locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;
    // The number of locals_count is augmented by 1 so that the 'locals_count'
    // index can be used to track the instance cache.
    BitVector* assigned = zone->New<BitVector>(locals_count + 1, zone);
    int depth = -1;  // We will increment the depth to 0 when we decode the
                     // starting 'loop' opcode.
    // Since 'let' can add additional locals at the beginning of the locals
    // index space, we need to track this offset for every depth up to the
    // current depth.
    base::SmallVector<uint32_t, 8> local_offsets(8);
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && VALIDATE(decoder->ok())) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          depth++;
          local_offsets.resize_no_init(depth + 1);
          // No additional locals.
          local_offsets[depth] = depth > 0 ? local_offsets[depth - 1] : 0;
          break;
        case kExprLet: {
          depth++;
          local_offsets.resize_no_init(depth + 1);
          BlockTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc + 1,
                                           nullptr);
          uint32_t locals_length;
          int new_locals_count = decoder->DecodeLocals(
              pc + 1 + imm.length, &locals_length, base::Optional<uint32_t>());
          local_offsets[depth] = local_offsets[depth - 1] + new_locals_count;
          break;
        }
        case kExprLocalSet:
        case kExprLocalTee: {
          IndexImmediate<validate> imm(decoder, pc + 1, "local index");
          // Unverified code might have an out-of-bounds index.
          if (imm.index >= local_offsets[depth] &&
              imm.index - local_offsets[depth] < locals_count) {
            assigned->Add(imm.index - local_offsets[depth]);
          }
          break;
        }
        case kExprMemoryGrow:
        case kExprCallFunction:
        case kExprCallIndirect:
        case kExprCallRef:
          // Add instance cache to the assigned set.
          assigned->Add(locals_count);
          break;
        case kExprEnd:
          depth--;
          break;
        default:
          break;
      }
      if (depth < 0) break;
      pc += OpcodeLength(decoder, pc);
    }
    return VALIDATE(decoder->ok()) ? assigned : nullptr;
  }

  bool Validate(const byte* pc, ExceptionIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->exceptions.size())) {
      DecodeError(pc, "Invalid exception index: %u", imm.index);
      return false;
    }
    imm.exception = &module_->exceptions[imm.index];
    return true;
  }

  bool Validate(const byte* pc, GlobalIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->globals.size())) {
      DecodeError(pc, "Invalid global index: %u", imm.index);
      return false;
    }
    imm.global = &module_->globals[imm.index];

    if (decoding_mode == kInitExpression) {
      if (!VALIDATE(!imm.global->mutability)) {
        this->DecodeError(pc,
                          "mutable globals cannot be used in initializer "
                          "expressions");
        return false;
      }
      if (!VALIDATE(imm.global->imported || this->enabled_.has_gc())) {
        this->DecodeError(
            pc,
            "non-imported globals cannot be used in initializer expressions");
        return false;
      }
    }

    return true;
  }

  bool Validate(const byte* pc, StructIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_struct(imm.index))) {
      DecodeError(pc, "invalid struct index: %u", imm.index);
      return false;
    }
    imm.struct_type = module_->struct_type(imm.index);
    return true;
  }

  bool Validate(const byte* pc, FieldImmediate<validate>& imm) {
    if (!Validate(pc, imm.struct_imm)) return false;
    if (!VALIDATE(imm.field_imm.index <
                  imm.struct_imm.struct_type->field_count())) {
      DecodeError(pc + imm.struct_imm.length, "invalid field index: %u",
                  imm.field_imm.index);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, ArrayIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_array(imm.index))) {
      DecodeError(pc, "invalid array index: %u", imm.index);
      return false;
    }
    imm.array_type = module_->array_type(imm.index);
    return true;
  }

  bool CanReturnCall(const FunctionSig* target_sig) {
    if (target_sig == nullptr) return false;
    size_t num_returns = sig_->return_count();
    if (num_returns != target_sig->return_count()) return false;
    for (size_t i = 0; i < num_returns; ++i) {
      if (sig_->GetReturn(i) != target_sig->GetReturn(i)) return false;
    }
    return true;
  }

  bool Validate(const byte* pc, CallFunctionImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->functions.size())) {
      DecodeError(pc, "function index #%u is out of bounds", imm.index);
      return false;
    }
    imm.sig = module_->functions[imm.index].sig;
    return true;
  }

  bool Validate(const byte* pc, CallIndirectImmediate<validate>& imm) {
    if (!ValidateSignature(pc, imm.sig_imm)) return false;
    // call_indirect is not behind the reftypes feature, so we have to impose
    // the older format if reftypes is not enabled.
    if (!VALIDATE((imm.table_imm.index == 0 && imm.table_imm.length == 1) ||
                  this->enabled_.has_reftypes())) {
      DecodeError(pc + imm.sig_imm.length, "expected table index 0, found %u",
                  imm.table_imm.index);
    }
    if (!ValidateTable(pc + imm.sig_imm.length, imm.table_imm)) {
      return false;
    }
    ValueType table_type = module_->tables[imm.table_imm.index].type;
    if (!VALIDATE(IsSubtypeOf(table_type, kWasmFuncRef, module_))) {
      DecodeError(
          pc, "call_indirect: immediate table #%u is not of a function type",
          imm.table_imm.index);
      return false;
    }

    // Check that the dynamic signature for this call is a subtype of the static
    // type of the table the function is defined in.
    ValueType immediate_type = ValueType::Ref(imm.sig_imm.index, kNonNullable);
    if (!VALIDATE(IsSubtypeOf(immediate_type, table_type, module_))) {
      DecodeError(pc,
                  "call_indirect: Immediate signature #%u is not a subtype of "
                  "immediate table #%u",
                  imm.sig_imm.index, imm.table_imm.index);
      return false;
    }

    imm.sig = module_->signature(imm.sig_imm.index);
    return true;
  }

  bool Validate(const byte* pc, BranchDepthImmediate<validate>& imm,
                size_t control_depth) {
    if (!VALIDATE(imm.depth < control_depth)) {
      DecodeError(pc, "invalid branch depth: %u", imm.depth);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BranchTableImmediate<validate>& imm,
                size_t block_depth) {
    if (!VALIDATE(imm.table_count <= kV8MaxWasmFunctionBrTableSize)) {
      DecodeError(pc, "invalid table count (> max br_table size): %u",
                  imm.table_count);
      return false;
    }
    return checkAvailable(imm.table_count);
  }

  bool Validate(const byte* pc, WasmOpcode opcode,
                SimdLaneImmediate<validate>& imm) {
    uint8_t num_lanes = 0;
    switch (opcode) {
      case kExprF64x2ExtractLane:
      case kExprF64x2ReplaceLane:
      case kExprI64x2ExtractLane:
      case kExprI64x2ReplaceLane:
      case kExprS128Load64Lane:
      case kExprS128Store64Lane:
        num_lanes = 2;
        break;
      case kExprF32x4ExtractLane:
      case kExprF32x4ReplaceLane:
      case kExprI32x4ExtractLane:
      case kExprI32x4ReplaceLane:
      case kExprS128Load32Lane:
      case kExprS128Store32Lane:
        num_lanes = 4;
        break;
      case kExprI16x8ExtractLaneS:
      case kExprI16x8ExtractLaneU:
      case kExprI16x8ReplaceLane:
      case kExprS128Load16Lane:
      case kExprS128Store16Lane:
        num_lanes = 8;
        break;
      case kExprI8x16ExtractLaneS:
      case kExprI8x16ExtractLaneU:
      case kExprI8x16ReplaceLane:
      case kExprS128Load8Lane:
      case kExprS128Store8Lane:
        num_lanes = 16;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (!VALIDATE(imm.lane >= 0 && imm.lane < num_lanes)) {
      DecodeError(pc, "invalid lane index");
      return false;
    } else {
      return true;
    }
  }

  bool Validate(const byte* pc, Simd128Immediate<validate>& imm) {
    uint8_t max_lane = 0;
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      max_lane = std::max(max_lane, imm.value[i]);
    }
    // Shuffle indices must be in [0..31] for a 16 lane shuffle.
    if (!VALIDATE(max_lane < 2 * kSimd128Size)) {
      DecodeError(pc, "invalid shuffle mask");
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BlockTypeImmediate<validate>& imm) {
    if (imm.type != kWasmBottom) return true;
    if (!VALIDATE(module_->has_signature(imm.sig_index))) {
      DecodeError(pc, "block type index %u is not a signature definition",
                  imm.sig_index);
      return false;
    }
    imm.sig = module_->signature(imm.sig_index);
    return true;
  }

  bool Validate(const byte* pc, MemoryIndexImmediate<validate>& imm) {
    if (!VALIDATE(this->module_->has_memory)) {
      this->DecodeError(pc, "memory instruction with no memory");
      return false;
    }
    if (!VALIDATE(imm.index == uint8_t{0})) {
      DecodeError(pc, "expected memory index 0, found %u", imm.index);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, MemoryAccessImmediate<validate>& imm) {
    if (!VALIDATE(this->module_->has_memory)) {
      this->DecodeError(pc, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, MemoryInitImmediate<validate>& imm) {
    return ValidateDataSegment(pc, imm.data_segment) &&
           Validate(pc + imm.data_segment.length, imm.memory);
  }

  bool Validate(const byte* pc, MemoryCopyImmediate<validate>& imm) {
    return Validate(pc, imm.memory_src) &&
           Validate(pc + imm.memory_src.length, imm.memory_dst);
  }

  bool Validate(const byte* pc, TableInitImmediate<validate>& imm) {
    if (!ValidateElementSegment(pc, imm.element_segment)) return false;
    if (!ValidateTable(pc + imm.element_segment.length, imm.table)) {
      return false;
    }
    ValueType elem_type =
        module_->elem_segments[imm.element_segment.index].type;
    if (!VALIDATE(IsSubtypeOf(elem_type, module_->tables[imm.table.index].type,
                              module_))) {
      DecodeError(pc, "table %u is not a super-type of %s", imm.table.index,
                  elem_type.name().c_str());
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, TableCopyImmediate<validate>& imm) {
    if (!ValidateTable(pc, imm.table_src)) return false;
    if (!ValidateTable(pc + imm.table_src.length, imm.table_dst)) return false;
    ValueType src_type = module_->tables[imm.table_src.index].type;
    if (!VALIDATE(IsSubtypeOf(
            src_type, module_->tables[imm.table_dst.index].type, module_))) {
      DecodeError(pc, "table %u is not a super-type of %s", imm.table_dst.index,
                  src_type.name().c_str());
      return false;
    }
    return true;
  }

  // The following Validate* functions all validate an IndexImmediate, albeit
  // differently according to context.
  bool ValidateTable(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->tables.size())) {
      DecodeError(pc, "invalid table index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateElementSegment(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->elem_segments.size())) {
      DecodeError(pc, "invalid element segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateLocal(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < num_locals())) {
      DecodeError(pc, "invalid local index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateType(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_type(imm.index))) {
      DecodeError(pc, "invalid type index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateSignature(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_signature(imm.index))) {
      DecodeError(pc, "invalid signature index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateFunction(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->functions.size())) {
      DecodeError(pc, "function index #%u is out of bounds", imm.index);
      return false;
    }
    if (decoding_mode == kFunctionBody &&
        !VALIDATE(module_->functions[imm.index].declared)) {
      DecodeError(pc, "undeclared reference to function #%u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateDataSegment(const byte* pc, IndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->num_declared_data_segments)) {
      DecodeError(pc, "invalid data segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  // Returns the length of the opcode under {pc}.
  static uint32_t OpcodeLength(WasmDecoder* decoder, const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // We don't have information about the module here, so we just assume that
    // memory64 is enabled when parsing memory access immediates. This is
    // backwards-compatible; decode errors will be detected at another time when
    // actually decoding that opcode.
    constexpr bool kConservativelyAssumeMemory64 = true;
    switch (opcode) {
      /********** Control opcodes **********/
      case kExprUnreachable:
      case kExprNop:
      case kExprNopForTestingUnsupportedInLiftoff:
      case kExprElse:
      case kExprEnd:
      case kExprReturn:
        return 1;
      case kExprTry:
      case kExprIf:
      case kExprLoop:
      case kExprBlock: {
        BlockTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc + 1,
                                         nullptr);
        return 1 + imm.length;
      }
      case kExprRethrow:
      case kExprBr:
      case kExprBrIf:
      case kExprBrOnNull:
      case kExprBrOnNonNull:
      case kExprDelegate: {
        BranchDepthImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprBrTable: {
        BranchTableImmediate<validate> imm(decoder, pc + 1);
        BranchTableIterator<validate> iterator(decoder, imm);
        return 1 + iterator.length();
      }
      case kExprThrow:
      case kExprCatch: {
        ExceptionIndexImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprLet: {
        BlockTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc + 1,
                                         nullptr);
        uint32_t locals_length;
        int new_locals_count = decoder->DecodeLocals(
            pc + 1 + imm.length, &locals_length, base::Optional<uint32_t>());
        return 1 + imm.length + ((new_locals_count >= 0) ? locals_length : 0);
      }

      /********** Misc opcodes **********/
      case kExprCallFunction:
      case kExprReturnCall: {
        CallFunctionImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprCallIndirect:
      case kExprReturnCallIndirect: {
        CallIndirectImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprCallRef:
      case kExprReturnCallRef:
      case kExprDrop:
      case kExprSelect:
      case kExprCatchAll:
        return 1;
      case kExprSelectWithType: {
        SelectTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc + 1,
                                          nullptr);
        return 1 + imm.length;
      }

      case kExprLocalGet:
      case kExprLocalSet:
      case kExprLocalTee: {
        IndexImmediate<validate> imm(decoder, pc + 1, "local index");
        return 1 + imm.length;
      }
      case kExprGlobalGet:
      case kExprGlobalSet: {
        GlobalIndexImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprTableGet:
      case kExprTableSet: {
        IndexImmediate<validate> imm(decoder, pc + 1, "table index");
        return 1 + imm.length;
      }
      case kExprI32Const: {
        ImmI32Immediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprI64Const: {
        ImmI64Immediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kExprRefNull: {
        HeapTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc + 1,
                                        nullptr);
        return 1 + imm.length;
      }
      case kExprRefIsNull: {
        return 1;
      }
      case kExprRefFunc: {
        IndexImmediate<validate> imm(decoder, pc + 1, "function index");
        return 1 + imm.length;
      }
      case kExprRefAsNonNull:
        return 1;

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
        // clang-format off
      /********** Simple and memory opcodes **********/
      FOREACH_SIMPLE_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_SIMPLE_PROTOTYPE_OPCODE(DECLARE_OPCODE_CASE)
        return 1;
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE) {
        MemoryAccessImmediate<validate> imm(decoder, pc + 1, UINT32_MAX,
                                            kConservativelyAssumeMemory64);
        return 1 + imm.length;
      }
      // clang-format on
      case kExprMemoryGrow:
      case kExprMemorySize: {
        MemoryIndexImmediate<validate> imm(decoder, pc + 1);
        return 1 + imm.length;
      }

      /********** Prefixed opcodes **********/
      case kNumericPrefix: {
        uint32_t length = 0;
        opcode = decoder->read_prefixed_opcode<validate>(pc, &length);
        switch (opcode) {
          case kExprI32SConvertSatF32:
          case kExprI32UConvertSatF32:
          case kExprI32SConvertSatF64:
          case kExprI32UConvertSatF64:
          case kExprI64SConvertSatF32:
          case kExprI64UConvertSatF32:
          case kExprI64SConvertSatF64:
          case kExprI64UConvertSatF64:
            return length;
          case kExprMemoryInit: {
            MemoryInitImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprDataDrop: {
            IndexImmediate<validate> imm(decoder, pc + length,
                                         "data segment index");
            return length + imm.length;
          }
          case kExprMemoryCopy: {
            MemoryCopyImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprMemoryFill: {
            MemoryIndexImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprTableInit: {
            TableInitImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprElemDrop: {
            IndexImmediate<validate> imm(decoder, pc + length,
                                         "element segment index");
            return length + imm.length;
          }
          case kExprTableCopy: {
            TableCopyImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprTableGrow:
          case kExprTableSize:
          case kExprTableFill: {
            IndexImmediate<validate> imm(decoder, pc + length, "table index");
            return length + imm.length;
          }
          default:
            if (validate) {
              decoder->DecodeError(pc, "invalid numeric opcode");
            }
            return length;
        }
      }
      case kSimdPrefix: {
        uint32_t length = 0;
        opcode = decoder->read_prefixed_opcode<validate>(pc, &length);
        switch (opcode) {
          // clang-format off
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return length;
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return length + 1;
          FOREACH_SIMD_MEM_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate<validate> imm(decoder, pc + length,
                                                UINT32_MAX,
                                                kConservativelyAssumeMemory64);
            return length + imm.length;
          }
          FOREACH_SIMD_MEM_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate<validate> imm(
                decoder, pc + length, UINT32_MAX,
                kConservativelyAssumeMemory64);
            // 1 more byte for lane index immediate.
            return length + imm.length + 1;
          }
          // clang-format on
          // Shuffles require a byte per lane, or 16 immediate bytes.
          case kExprS128Const:
          case kExprI8x16Shuffle:
            return length + kSimd128Size;
          default:
            if (validate) {
              decoder->DecodeError(pc, "invalid SIMD opcode");
            }
            return length;
        }
      }
      case kAtomicPrefix: {
        uint32_t length = 0;
        opcode = decoder->read_prefixed_opcode<validate>(pc, &length,
                                                         "atomic_index");
        switch (opcode) {
          FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate<validate> imm(decoder, pc + length,
                                                UINT32_MAX,
                                                kConservativelyAssumeMemory64);
            return length + imm.length;
          }
          FOREACH_ATOMIC_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE) {
            return length + 1;
          }
          default:
            if (validate) {
              decoder->DecodeError(pc, "invalid Atomics opcode");
            }
            return length;
        }
      }
      case kGCPrefix: {
        uint32_t length = 0;
        opcode =
            decoder->read_prefixed_opcode<validate>(pc, &length, "gc_index");
        switch (opcode) {
          case kExprStructNewWithRtt:
          case kExprStructNewDefault: {
            StructIndexImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprStructGet:
          case kExprStructGetS:
          case kExprStructGetU:
          case kExprStructSet: {
            FieldImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprArrayNewWithRtt:
          case kExprArrayNewDefault:
          case kExprArrayGet:
          case kExprArrayGetS:
          case kExprArrayGetU:
          case kExprArraySet:
          case kExprArrayLen: {
            ArrayIndexImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprArrayCopy: {
            ArrayIndexImmediate<validate> dst_imm(decoder, pc + length);
            ArrayIndexImmediate<validate> src_imm(decoder,
                                                  pc + length + dst_imm.length);
            return length + dst_imm.length + src_imm.length;
          }
          case kExprBrOnCast:
          case kExprBrOnCastFail:
          case kExprBrOnData:
          case kExprBrOnFunc:
          case kExprBrOnI31: {
            BranchDepthImmediate<validate> imm(decoder, pc + length);
            return length + imm.length;
          }
          case kExprRttCanon:
          case kExprRttSub:
          case kExprRttFreshSub: {
            IndexImmediate<validate> imm(decoder, pc + length, "type index");
            return length + imm.length;
          }
          case kExprI31New:
          case kExprI31GetS:
          case kExprI31GetU:
          case kExprRefAsData:
          case kExprRefAsFunc:
          case kExprRefAsI31:
          case kExprRefIsData:
          case kExprRefIsFunc:
          case kExprRefIsI31:
          case kExprRefTest:
          case kExprRefCast:
            return length;
          default:
            // This is unreachable except for malformed modules.
            if (validate) {
              decoder->DecodeError(pc, "invalid gc opcode");
            }
            return length;
        }
      }

        // clang-format off
      /********** Asmjs opcodes **********/
      FOREACH_ASMJS_COMPAT_OPCODE(DECLARE_OPCODE_CASE)
        return 1;

      // Prefixed opcodes (already handled, included here for completeness of
      // switch)
      FOREACH_SIMD_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_NUMERIC_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_ATOMIC_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_GC_OPCODE(DECLARE_OPCODE_CASE)
        UNREACHABLE();
        // clang-format on
#undef DECLARE_OPCODE_CASE
    }
    // Invalid modules will reach this point.
    if (validate) {
      decoder->DecodeError(pc, "invalid opcode");
    }
    return 1;
  }

  // TODO(clemensb): This is only used by the interpreter; move there.
  V8_EXPORT_PRIVATE std::pair<uint32_t, uint32_t> StackEffect(const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // Handle "simple" opcodes with a fixed signature first.
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!sig) sig = WasmOpcodes::AsmjsSignature(opcode);
    if (sig) return {sig->parameter_count(), sig->return_count()};

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprSelect:
      case kExprSelectWithType:
        return {3, 1};
      case kExprTableSet:
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      case kExprTableGet:
      case kExprLocalTee:
      case kExprMemoryGrow:
      case kExprRefAsNonNull:
      case kExprBrOnNull:
      case kExprRefIsNull:
        return {1, 1};
      case kExprLocalSet:
      case kExprGlobalSet:
      case kExprDrop:
      case kExprBrIf:
      case kExprBrTable:
      case kExprIf:
      case kExprBrOnNonNull:
        return {1, 0};
      case kExprLocalGet:
      case kExprGlobalGet:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF32Const:
      case kExprF64Const:
      case kExprRefNull:
      case kExprRefFunc:
      case kExprMemorySize:
        return {0, 1};
      case kExprCallFunction: {
        CallFunctionImmediate<validate> imm(this, pc + 1);
        CHECK(Validate(pc + 1, imm));
        return {imm.sig->parameter_count(), imm.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<validate> imm(this, pc + 1);
        CHECK(Validate(pc + 1, imm));
        // Indirect calls pop an additional argument for the table index.
        return {imm.sig->parameter_count() + 1,
                imm.sig->return_count()};
      }
      case kExprThrow: {
        ExceptionIndexImmediate<validate> imm(this, pc + 1);
        CHECK(Validate(pc + 1, imm));
        DCHECK_EQ(0, imm.exception->sig->return_count());
        return {imm.exception->sig->parameter_count(), 0};
      }
      case kExprBr:
      case kExprBlock:
      case kExprLoop:
      case kExprEnd:
      case kExprElse:
      case kExprTry:
      case kExprCatch:
      case kExprCatchAll:
      case kExprDelegate:
      case kExprRethrow:
      case kExprNop:
      case kExprNopForTestingUnsupportedInLiftoff:
      case kExprReturn:
      case kExprReturnCall:
      case kExprReturnCallIndirect:
      case kExprUnreachable:
        return {0, 0};
      case kExprLet:
        // TODO(7748): Implement
        return {0, 0};
      case kNumericPrefix:
      case kAtomicPrefix:
      case kSimdPrefix: {
        opcode = this->read_prefixed_opcode<validate>(pc);
        switch (opcode) {
          FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(DECLARE_OPCODE_CASE)
            return {1, 1};
          FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(DECLARE_OPCODE_CASE)
          FOREACH_SIMD_MASK_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return {2, 1};
          FOREACH_SIMD_CONST_OPCODE(DECLARE_OPCODE_CASE)
            return {0, 1};
          default: {
            sig = WasmOpcodes::Signature(opcode);
            if (sig) {
              return {sig->parameter_count(), sig->return_count()};
            } else {
              UNREACHABLE();
            }
          }
        }
      }
      case kGCPrefix: {
        opcode = this->read_prefixed_opcode<validate>(pc);
        switch (opcode) {
          case kExprStructNewDefault:
          case kExprStructGet:
          case kExprStructGetS:
          case kExprStructGetU:
          case kExprI31New:
          case kExprI31GetS:
          case kExprI31GetU:
          case kExprArrayLen:
          case kExprRttSub:
          case kExprRttFreshSub:
            return {1, 1};
          case kExprStructSet:
            return {2, 0};
          case kExprArrayNewDefault:
          case kExprArrayGet:
          case kExprArrayGetS:
          case kExprArrayGetU:
          case kExprRefTest:
          case kExprRefCast:
          case kExprBrOnCast:
          case kExprBrOnCastFail:
            return {2, 1};
          case kExprArraySet:
            return {3, 0};
          case kExprArrayCopy:
            return {5, 0};
          case kExprRttCanon:
            return {0, 1};
          case kExprArrayNewWithRtt:
            return {3, 1};
          case kExprStructNewWithRtt: {
            StructIndexImmediate<validate> imm(this, pc + 2);
            CHECK(Validate(pc + 2, imm));
            return {imm.struct_type->field_count() + 1, 1};
          }
          default:
            UNREACHABLE();
        }
      }
      default:
        FATAL("unimplemented opcode: %x (%s)", opcode,
              WasmOpcodes::OpcodeName(opcode));
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }

  bool is_local_initialized(uint32_t local_index) {
    return initialized_locals_[local_index];
  }

  void set_local_initialized(uint32_t local_index) {
    if (!enabled_.has_nn_locals()) return;
    // This implicitly covers defaultable locals too (which are always
    // initialized).
    if (is_local_initialized(local_index)) return;
    initialized_locals_[local_index] = true;
    locals_initializers_stack_.push_back(local_index);
  }

  uint32_t locals_initialization_stack_depth() const {
    return static_cast<uint32_t>(locals_initializers_stack_.size());
  }

  void RollbackLocalsInitialization(uint32_t previous_stack_height) {
    if (!enabled_.has_nn_locals()) return;
    while (locals_initializers_stack_.size() > previous_stack_height) {
      uint32_t local_index = locals_initializers_stack_.back();
      locals_initializers_stack_.pop_back();
      initialized_locals_[local_index] = false;
    }
  }

  void InitializeInitializedLocalsTracking(int non_defaultable_locals) {
    initialized_locals_.assign(num_locals_, false);
    // Parameters count as initialized...
    const uint32_t num_params = static_cast<uint32_t>(sig_->parameter_count());
    for (uint32_t i = 0; i < num_params; i++) {
      initialized_locals_[i] = true;
    }
    // ...and so do defaultable locals.
    for (uint32_t i = num_params; i < num_locals_; i++) {
      if (local_types_[i].is_defaultable()) initialized_locals_[i] = true;
    }
    if (non_defaultable_locals == 0) return;
    locals_initializers_stack_.reserve(non_defaultable_locals);
  }

  // The {Zone} is implicitly stored in the {ZoneAllocator} which is part of
  // this {ZoneVector}. Hence save one field and just get it from there if
  // needed (see {zone()} accessor below).
  ZoneVector<ValueType> local_types_;

  // Cached value, for speed (yes, it's measurably faster to load this value
  // than to load the start and end pointer from a vector, subtract and shift).
  uint32_t num_locals_ = 0;

  // Indicates whether the local with the given index is currently initialized.
  // Entries for defaultable locals are meaningless; we have a bit for each
  // local because we expect that the effort required to densify this bit
  // vector would more than offset the memory savings.
  ZoneVector<bool> initialized_locals_;
  // Keeps track of initializing assignments to non-defaultable locals that
  // happened, so they can be discarded at the end of the current block.
  // Contains no duplicates, so the size of this stack is bounded (and pre-
  // allocated) to the number of non-defaultable locals in the function.
  ZoneVector<uint32_t> locals_initializers_stack_;

  const WasmModule* module_;
  const WasmFeatures enabled_;
  WasmFeatures* detected_;
  const FunctionSig* sig_;
};

// Only call this in contexts where {current_code_reachable_and_ok_} is known to
// hold.
#define CALL_INTERFACE(name, ...)                         \
  do {                                                    \
    DCHECK(!control_.empty());                            \
    DCHECK(current_code_reachable_and_ok_);               \
    DCHECK_EQ(current_code_reachable_and_ok_,             \
              this->ok() && control_.back().reachable()); \
    interface_.name(this, ##__VA_ARGS__);                 \
  } while (false)
#define CALL_INTERFACE_IF_OK_AND_REACHABLE(name, ...)     \
  do {                                                    \
    DCHECK(!control_.empty());                            \
    DCHECK_EQ(current_code_reachable_and_ok_,             \
              this->ok() && control_.back().reachable()); \
    if (V8_LIKELY(current_code_reachable_and_ok_)) {      \
      interface_.name(this, ##__VA_ARGS__);               \
    }                                                     \
  } while (false)
#define CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(name, ...)    \
  do {                                                          \
    DCHECK(!control_.empty());                                  \
    if (VALIDATE(this->ok()) &&                                 \
        (control_.size() == 1 || control_at(1)->reachable())) { \
      interface_.name(this, ##__VA_ARGS__);                     \
    }                                                           \
  } while (false)

template <Decoder::ValidateFlag validate, typename Interface,
          DecodingMode decoding_mode = kFunctionBody>
class WasmFullDecoder : public WasmDecoder<validate, decoding_mode> {
  using Value = typename Interface::Value;
  using Control = typename Interface::Control;
  using ArgVector = base::Vector<Value>;
  using ReturnVector = base::SmallVector<Value, 2>;

  // All Value types should be trivially copyable for performance. We push, pop,
  // and store them in local variables.
  ASSERT_TRIVIALLY_COPYABLE(Value);

 public:
  template <typename... InterfaceArgs>
  WasmFullDecoder(Zone* zone, const WasmModule* module,
                  const WasmFeatures& enabled, WasmFeatures* detected,
                  const FunctionBody& body, InterfaceArgs&&... interface_args)
      : WasmDecoder<validate, decoding_mode>(zone, module, enabled, detected,
                                             body.sig, body.start, body.end,
                                             body.offset),
        interface_(std::forward<InterfaceArgs>(interface_args)...),
        control_(zone) {}

  Interface& interface() { return interface_; }

  bool Decode() {
    DCHECK_EQ(stack_end_, stack_);
    DCHECK(control_.empty());
    DCHECK_LE(this->pc_, this->end_);
    DCHECK_EQ(this->num_locals(), 0);

    this->InitializeLocalsFromSig();
    uint32_t params_count = static_cast<uint32_t>(this->num_locals());
    uint32_t locals_length;
    this->DecodeLocals(this->pc(), &locals_length, params_count);
    if (this->failed()) return TraceFailed();
    this->consume_bytes(locals_length);
    int non_defaultable = 0;
    for (uint32_t index = params_count; index < this->num_locals(); index++) {
      if (!VALIDATE(this->enabled_.has_nn_locals() ||
                    this->local_type(index).is_defaultable())) {
        this->DecodeError(
            "Cannot define function-level local of non-defaultable type %s",
            this->local_type(index).name().c_str());
        return this->TraceFailed();
      }
      if (!this->local_type(index).is_defaultable()) non_defaultable++;
    }
    this->InitializeInitializedLocalsTracking(non_defaultable);

    // Cannot use CALL_INTERFACE_* macros because control is empty.
    interface().StartFunction(this);
    DecodeFunctionBody();
    if (this->failed()) return TraceFailed();

    if (!VALIDATE(control_.empty())) {
      if (control_.size() > 1) {
        this->DecodeError(control_.back().pc(),
                          "unterminated control structure");
      } else {
        this->DecodeError("function body must end with \"end\" opcode");
      }
      return TraceFailed();
    }
    // Cannot use CALL_INTERFACE_* macros because control is empty.
    interface().FinishFunction(this);
    if (this->failed()) return TraceFailed();

    TRACE("wasm-decode ok\n\n");
    return true;
  }

  bool TraceFailed() {
    if (this->error_.offset()) {
      TRACE("wasm-error module+%-6d func+%d: %s\n\n", this->error_.offset(),
            this->GetBufferRelativeOffset(this->error_.offset()),
            this->error_.message().c_str());
    } else {
      TRACE("wasm-error: %s\n\n", this->error_.message().c_str());
    }
    return false;
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (!pc) return "<null>";
    if (pc >= this->end_) return "<end>";
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
      return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(opcode));
    }
    opcode = this->template read_prefixed_opcode<Decoder::kFullValidation>(pc);
    return WasmOpcodes::OpcodeName(opcode);
  }

  WasmCodePosition position() const {
    int offset = static_cast<int>(this->pc_ - this->start_);
    DCHECK_EQ(this->pc_ - this->start_, offset);  // overflows cannot happen
    return offset;
  }

  uint32_t control_depth() const {
    return static_cast<uint32_t>(control_.size());
  }

  Control* control_at(uint32_t depth) {
    DCHECK_GT(control_.size(), depth);
    return &control_.back() - depth;
  }

  uint32_t stack_size() const {
    DCHECK_GE(stack_end_, stack_);
    DCHECK_GE(kMaxUInt32, stack_end_ - stack_);
    return static_cast<uint32_t>(stack_end_ - stack_);
  }

  Value* stack_value(uint32_t depth) const {
    DCHECK_LT(0, depth);
    DCHECK_GE(stack_size(), depth);
    return stack_end_ - depth;
  }

  int32_t current_catch() const { return current_catch_; }

  uint32_t control_depth_of_current_catch() const {
    return control_depth() - 1 - current_catch();
  }

  void SetSucceedingCodeDynamicallyUnreachable() {
    Control* current = &control_.back();
    if (current->reachable()) {
      current->reachability = kSpecOnlyReachable;
      current_code_reachable_and_ok_ = false;
    }
  }

  uint32_t pc_relative_offset() const {
    return this->pc_offset() - first_instruction_offset;
  }

  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n", this->start(),
          this->end(), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      DCHECK(control_.empty());
      constexpr uint32_t kLocalsCount = 0;
      constexpr uint32_t kStackDepth = 0;
      constexpr uint32_t kInitStackDepth = 0;
      control_.emplace_back(kControlBlock, kLocalsCount, kStackDepth,
                            kInitStackDepth, this->pc_, kReachable);
      Control* c = &control_.back();
      if (decoding_mode == kFunctionBody) {
        InitMerge(&c->start_merge, 0, [](uint32_t) -> Value { UNREACHABLE(); });
        InitMerge(&c->end_merge,
                  static_cast<uint32_t>(this->sig_->return_count()),
                  [&](uint32_t i) {
                    return Value{this->pc_, this->sig_->GetReturn(i)};
                  });
      } else {
        DCHECK_EQ(this->sig_->parameter_count(), 0);
        DCHECK_EQ(this->sig_->return_count(), 1);
        c->start_merge.arity = 0;
        c->end_merge.arity = 1;
        c->end_merge.vals.first = Value{this->pc_, this->sig_->GetReturn(0)};
      }
      CALL_INTERFACE_IF_OK_AND_REACHABLE(StartFunctionBody, c);
    }

    first_instruction_offset = this->pc_offset();
    // Decode the function body.
    while (this->pc_ < this->end_) {
      // Most operations only grow the stack by at least one element (unary and
      // binary operations, local.get, constants, ...). Thus check that there is
      // enough space for those operations centrally, and avoid any bounds
      // checks in those operations.
      EnsureStackSpace(1);
      uint8_t first_byte = *this->pc_;
      WasmOpcode opcode = static_cast<WasmOpcode>(first_byte);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(NextInstruction, opcode);
      int len;
      // Allowing two of the most common decoding functions to get inlined
      // appears to be the sweet spot.
      // Handling _all_ opcodes via a giant switch-statement has been tried
      // and found to be slower than calling through the handler table.
      if (opcode == kExprLocalGet) {
        len = WasmFullDecoder::DecodeLocalGet(this, opcode);
      } else if (opcode == kExprI32Const) {
        len = WasmFullDecoder::DecodeI32Const(this, opcode);
      } else {
        OpcodeHandler handler = GetOpcodeHandler(first_byte);
        len = (*handler)(this, opcode);
      }
      this->pc_ += len;
    }

    if (!VALIDATE(this->pc_ == this->end_)) {
      this->DecodeError("Beyond end of code");
    }
  }

 private:
  uint32_t first_instruction_offset = 0;
  Interface interface_;

  // The value stack, stored as individual pointers for maximum performance.
  Value* stack_ = nullptr;
  Value* stack_end_ = nullptr;
  Value* stack_capacity_end_ = nullptr;
  ASSERT_TRIVIALLY_COPYABLE(Value);

  // stack of blocks, loops, and ifs.
  ZoneVector<Control> control_;

  // Controls whether code should be generated for the current block (basically
  // a cache for {ok() && control_.back().reachable()}).
  bool current_code_reachable_and_ok_ = true;

  // Depth of the current try block.
  int32_t current_catch_ = -1;

  static Value UnreachableValue(const uint8_t* pc) {
    return Value{pc, kWasmBottom};
  }

  bool CheckSimdFeatureFlagOpcode(WasmOpcode opcode) {
    if (!FLAG_experimental_wasm_relaxed_simd &&
        WasmOpcodes::IsRelaxedSimdOpcode(opcode)) {
      this->DecodeError(
          "simd opcode not available, enable with --experimental-relaxed-simd");
      return false;
    }

    return true;
  }

  MemoryAccessImmediate<validate> MakeMemoryAccessImmediate(
      uint32_t pc_offset, uint32_t max_alignment) {
    return MemoryAccessImmediate<validate>(
        this, this->pc_ + pc_offset, max_alignment, this->module_->is_memory64);
  }

#ifdef DEBUG
  class TraceLine {
   public:
    explicit TraceLine(WasmFullDecoder* decoder) : decoder_(decoder) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*decoder->pc());
      if (!WasmOpcodes::IsPrefixOpcode(opcode)) AppendOpcode(opcode);
    }

    void AppendOpcode(WasmOpcode opcode) {
      DCHECK(!WasmOpcodes::IsPrefixOpcode(opcode));
      Append(TRACE_INST_FORMAT, decoder_->startrel(decoder_->pc_),
             WasmOpcodes::OpcodeName(opcode));
    }

    ~TraceLine() {
      if (!FLAG_trace_wasm_decoder) return;
      AppendStackState();
      PrintF("%.*s\n", len_, buffer_);
    }

    // Appends a formatted string.
    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {
      if (!FLAG_trace_wasm_decoder) return;
      va_list va_args;
      va_start(va_args, format);
      size_t remaining_len = kMaxLen - len_;
      base::Vector<char> remaining_msg_space(buffer_ + len_, remaining_len);
      int len = base::VSNPrintF(remaining_msg_space, format, va_args);
      va_end(va_args);
      len_ += len < 0 ? remaining_len : len;
    }

   private:
    void AppendStackState() {
      DCHECK(FLAG_trace_wasm_decoder);
      Append(" ");
      for (Control& c : decoder_->control_) {
        switch (c.kind) {
          case kControlIf:
            Append("I");
            break;
          case kControlBlock:
            Append("B");
            break;
          case kControlLoop:
            Append("L");
            break;
          case kControlTry:
            Append("T");
            break;
          case kControlIfElse:
          case kControlTryCatch:
          case kControlTryCatchAll:
          case kControlLet:  // TODO(7748): Implement
            break;
        }
        if (c.start_merge.arity) Append("%u-", c.start_merge.arity);
        Append("%u", c.end_merge.arity);
        if (!c.reachable()) Append("%c", c.unreachable() ? '*' : '#');
      }
      Append(" | ");
      for (size_t i = 0; i < decoder_->stack_size(); ++i) {
        Value& val = decoder_->stack_[i];
        Append(" %c", val.type.short_name());
      }
    }

    static constexpr int kMaxLen = 512;

    char buffer_[kMaxLen];
    int len_ = 0;
    WasmFullDecoder* const decoder_;
  };
#else
  class TraceLine {
   public:
    explicit TraceLine(WasmFullDecoder*) {}

    void AppendOpcode(WasmOpcode) {}

    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {}
  };
#endif

#define DECODE(name)                                                     \
  static int Decode##name(WasmFullDecoder* decoder, WasmOpcode opcode) { \
    TraceLine trace_msg(decoder);                                        \
    return decoder->Decode##name##Impl(&trace_msg, opcode);              \
  }                                                                      \
  V8_INLINE int Decode##name##Impl(TraceLine* trace_msg, WasmOpcode opcode)

  DECODE(Nop) { return 1; }

  DECODE(NopForTestingUnsupportedInLiftoff) {
    if (!VALIDATE(FLAG_enable_testing_opcode_in_wasm)) {
      this->DecodeError("Invalid opcode 0x%x", opcode);
      return 0;
    }
    CALL_INTERFACE_IF_OK_AND_REACHABLE(NopForTestingUnsupportedInLiftoff);
    return 1;
  }

#define BUILD_SIMPLE_OPCODE(op, _, sig) \
  DECODE(op) { return BuildSimpleOperator_##sig(kExpr##op); }
  FOREACH_SIMPLE_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE

  DECODE(Block) {
    BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                     this->module_);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.sig);
    Control* block = PushControl(kControlBlock, 0, args.length());
    SetBlockType(block, imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Block, block);
    DropArgs(imm.sig);
    PushMergeValues(block, &block->start_merge);
    return 1 + imm.length;
  }

  DECODE(Rethrow) {
    CHECK_PROTOTYPE_OPCODE(eh);
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Control* c = control_at(imm.depth);
    if (!VALIDATE(c->is_try_catchall() || c->is_try_catch())) {
      this->error("rethrow not targeting catch or catch-all");
      return 0;
    }
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Rethrow, c);
    EndControl();
    return 1 + imm.length;
  }

  DECODE(Throw) {
    CHECK_PROTOTYPE_OPCODE(eh);
    ExceptionIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.exception->ToFunctionSig());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Throw, imm, base::VectorOf(args));
    DropArgs(imm.exception->ToFunctionSig());
    EndControl();
    return 1 + imm.length;
  }

  DECODE(Try) {
    CHECK_PROTOTYPE_OPCODE(eh);
    BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                     this->module_);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.sig);
    Control* try_block = PushControl(kControlTry, 0, args.length());
    SetBlockType(try_block, imm, args.begin());
    try_block->previous_catch = current_catch_;
    current_catch_ = static_cast<int>(control_depth() - 1);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Try, try_block);
    DropArgs(imm.sig);
    PushMergeValues(try_block, &try_block->start_merge);
    return 1 + imm.length;
  }

  DECODE(Catch) {
    CHECK_PROTOTYPE_OPCODE(eh);
    ExceptionIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    DCHECK(!control_.empty());
    Control* c = &control_.back();
    if (!VALIDATE(c->is_try())) {
      this->DecodeError("catch does not match a try");
      return 0;
    }
    if (!VALIDATE(!c->is_try_catchall())) {
      this->DecodeError("catch after catch-all for try");
      return 0;
    }
    FallThrough();
    c->kind = kControlTryCatch;
    // TODO(jkummerow): Consider moving the stack manipulation after the
    // INTERFACE call for consistency.
    DCHECK_LE(stack_ + c->stack_depth, stack_end_);
    stack_end_ = stack_ + c->stack_depth;
    c->reachability = control_at(1)->innerReachability();
    const WasmExceptionSig* sig = imm.exception->sig;
    EnsureStackSpace(static_cast<int>(sig->parameter_count()));
    for (size_t i = 0, e = sig->parameter_count(); i < e; ++i) {
      Push(CreateValue(sig->GetParam(i)));
    }
    base::Vector<Value> values(stack_ + c->stack_depth, sig->parameter_count());
    current_catch_ = c->previous_catch;  // Pop try scope.
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(CatchException, imm, c, values);
    current_code_reachable_and_ok_ = this->ok() && c->reachable();
    return 1 + imm.length;
  }

  DECODE(Delegate) {
    CHECK_PROTOTYPE_OPCODE(eh);
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    // -1 because the current try block is not included in the count.
    if (!this->Validate(this->pc_ + 1, imm, control_depth() - 1)) return 0;
    Control* c = &control_.back();
    if (!VALIDATE(c->is_incomplete_try())) {
      this->DecodeError("delegate does not match a try");
      return 0;
    }
    // +1 because the current try block is not included in the count.
    Control* target = control_at(imm.depth + 1);
    if (imm.depth + 1 < control_depth() - 1 && !target->is_try()) {
      this->DecodeError(
          "delegate target must be a try block or the function block");
      return 0;
    }
    if (target->is_try_catch() || target->is_try_catchall()) {
      this->DecodeError(
          "cannot delegate inside the catch handler of the target");
      return 0;
    }
    FallThrough();
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(Delegate, imm.depth + 1, c);
    current_catch_ = c->previous_catch;
    EndControl();
    PopControl();
    return 1 + imm.length;
  }

  DECODE(CatchAll) {
    CHECK_PROTOTYPE_OPCODE(eh);
    DCHECK(!control_.empty());
    Control* c = &control_.back();
    if (!VALIDATE(c->is_try())) {
      this->DecodeError("catch-all does not match a try");
      return 0;
    }
    if (!VALIDATE(!c->is_try_catchall())) {
      this->error("catch-all already present for try");
      return 0;
    }
    FallThrough();
    c->kind = kControlTryCatchAll;
    c->reachability = control_at(1)->innerReachability();
    current_catch_ = c->previous_catch;  // Pop try scope.
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(CatchAll, c);
    stack_end_ = stack_ + c->stack_depth;
    current_code_reachable_and_ok_ = this->ok() && c->reachable();
    return 1;
  }

  DECODE(BrOnNull) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Value ref_object = Peek(0, 0);
    Control* c = control_at(imm.depth);
    if (!VALIDATE(TypeCheckBranch<true>(c, 1))) return 0;
    switch (ref_object.type.kind()) {
      case kBottom:
        // We are in a polymorphic stack. Leave the stack as it is.
        DCHECK(!current_code_reachable_and_ok_);
        break;
      case kRef:
        // For a non-nullable value, we won't take the branch, and can leave
        // the stack as it is.
        break;
      case kOptRef: {
          Value result = CreateValue(
              ValueType::Ref(ref_object.type.heap_type(), kNonNullable));
          // The result of br_on_null has the same value as the argument (but a
          // non-nullable type).
          if (V8_LIKELY(current_code_reachable_and_ok_)) {
            CALL_INTERFACE(BrOnNull, ref_object, imm.depth);
            CALL_INTERFACE(Forward, ref_object, &result);
            c->br_merge()->reached = true;
          }
          // In unreachable code, we still have to push a value of the correct
          // type onto the stack.
          Drop(ref_object);
          Push(result);
        break;
      }
      default:
        PopTypeError(0, ref_object, "object reference");
        return 0;
    }
    return 1 + imm.length;
  }

  DECODE(BrOnNonNull) {
    CHECK_PROTOTYPE_OPCODE(gc);
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Value ref_object = Peek(0, 0, kWasmAnyRef);
    Drop(ref_object);
    // Typechecking the branch and creating the branch merges requires the
    // non-null value on the stack, so we push it temporarily.
    Value result = CreateValue(ref_object.type.AsNonNull());
    Push(result);
    Control* c = control_at(imm.depth);
    if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
    switch (ref_object.type.kind()) {
      case kBottom:
        // We are in unreachable code. Do nothing.
        DCHECK(!current_code_reachable_and_ok_);
        break;
      case kRef:
        // For a non-nullable value, we always take the branch.
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          CALL_INTERFACE(Forward, ref_object, stack_value(1));
          CALL_INTERFACE(BrOrRet, imm.depth, 0);
          c->br_merge()->reached = true;
        }
        break;
      case kOptRef: {
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          CALL_INTERFACE(Forward, ref_object, stack_value(1));
          CALL_INTERFACE(BrOnNonNull, ref_object, imm.depth);
          c->br_merge()->reached = true;
        }
        break;
      }
      default:
        PopTypeError(0, ref_object, "object reference");
        return 0;
    }
    // If we stay in the branch, {ref_object} is null. Drop it from the stack.
    Drop(result);
    return 1 + imm.length;
  }

  DECODE(Let) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                     this->module_);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    // Temporarily add the let-defined values to the beginning of the function
    // locals.
    uint32_t locals_length;
    int new_locals_count =
        this->DecodeLocals(this->pc() + 1 + imm.length, &locals_length, 0);
    if (new_locals_count < 0) {
      return 0;
    }
    ArgVector let_local_values =
        PeekArgs(static_cast<uint32_t>(imm.in_arity()),
                 base::VectorOf(this->local_types_.data(), new_locals_count));
    ArgVector args = PeekArgs(imm.sig, new_locals_count);
    Control* let_block = PushControl(kControlLet, new_locals_count,
                                     let_local_values.length() + args.length());
    SetBlockType(let_block, imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Block, let_block);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(AllocateLocals,
                                       base::VectorOf(let_local_values));
    Drop(new_locals_count);  // Drop {let_local_values}.
    DropArgs(imm.sig);       // Drop {args}.
    PushMergeValues(let_block, &let_block->start_merge);
    return 1 + imm.length + locals_length;
  }

  DECODE(Loop) {
    BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                     this->module_);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.sig);
    Control* block = PushControl(kControlLoop, 0, args.length());
    SetBlockType(&control_.back(), imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Loop, block);
    DropArgs(imm.sig);
    PushMergeValues(block, &block->start_merge);
    return 1 + imm.length;
  }

  DECODE(If) {
    BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                     this->module_);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value cond = Peek(0, 0, kWasmI32);
    ArgVector args = PeekArgs(imm.sig, 1);
    if (!VALIDATE(this->ok())) return 0;
    Control* if_block = PushControl(kControlIf, 0, 1 + args.length());
    SetBlockType(if_block, imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(If, cond, if_block);
    Drop(cond);
    DropArgs(imm.sig);  // Drop {args}.
    PushMergeValues(if_block, &if_block->start_merge);
    return 1 + imm.length;
  }

  DECODE(Else) {
    DCHECK(!control_.empty());
    Control* c = &control_.back();
    if (!VALIDATE(c->is_if())) {
      this->DecodeError("else does not match an if");
      return 0;
    }
    if (!VALIDATE(c->is_onearmed_if())) {
      this->DecodeError("else already present for if");
      return 0;
    }
    if (!VALIDATE(TypeCheckFallThru())) return 0;
    c->kind = kControlIfElse;
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(Else, c);
    if (c->reachable()) c->end_merge.reached = true;
    PushMergeValues(c, &c->start_merge);
    c->reachability = control_at(1)->innerReachability();
    current_code_reachable_and_ok_ = this->ok() && c->reachable();
    return 1;
  }

  DECODE(End) {
    DCHECK(!control_.empty());
    if (decoding_mode == kFunctionBody) {
      Control* c = &control_.back();
      if (c->is_incomplete_try()) {
        // Catch-less try, fall through to the implicit catch-all.
        c->kind = kControlTryCatch;
        current_catch_ = c->previous_catch;  // Pop try scope.
      }
      if (c->is_try_catch()) {
        // Emulate catch-all + re-throw.
        FallThrough();
        c->reachability = control_at(1)->innerReachability();
        CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(CatchAll, c);
        current_code_reachable_and_ok_ =
            this->ok() && control_.back().reachable();
        CALL_INTERFACE_IF_OK_AND_REACHABLE(Rethrow, c);
        EndControl();
        PopControl();
        return 1;
      }
      if (c->is_onearmed_if()) {
        if (!VALIDATE(TypeCheckOneArmedIf(c))) return 0;
      }

      if (c->is_let()) {
        CALL_INTERFACE_IF_OK_AND_REACHABLE(DeallocateLocals, c->locals_count);
        this->local_types_.erase(this->local_types_.begin(),
                                 this->local_types_.begin() + c->locals_count);
        this->num_locals_ -= c->locals_count;
      }
    }

    if (control_.size() == 1) {
      // We need to call this first because the interface might set
      // {this->end_}, making the next check pass.
      DoReturn<kStrictCounting, decoding_mode == kFunctionBody
                                    ? kFallthroughMerge
                                    : kInitExprMerge>();
      // If at the last (implicit) control, check we are at end.
      if (!VALIDATE(this->pc_ + 1 == this->end_)) {
        this->DecodeError(this->pc_ + 1, "trailing code after function end");
        return 0;
      }
      // The result of the block is the return value.
      trace_msg->Append("\n" TRACE_INST_FORMAT, startrel(this->pc_),
                        "(implicit) return");
      control_.clear();
      return 1;
    }

    if (!VALIDATE(TypeCheckFallThru())) return 0;
    PopControl();
    return 1;
  }

  DECODE(Select) {
    Value cond = Peek(0, 2, kWasmI32);
    Value fval = Peek(1, 1);
    Value tval = Peek(2, 0, fval.type);
    ValueType type = tval.type == kWasmBottom ? fval.type : tval.type;
    if (!VALIDATE(!type.is_reference())) {
      this->DecodeError(
          "select without type is only valid for value type inputs");
      return 0;
    }
    Value result = CreateValue(type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Select, cond, fval, tval, &result);
    Drop(3);
    Push(result);
    return 1;
  }

  DECODE(SelectWithType) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    SelectTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                      this->module_);
    if (this->failed()) return 0;
    Value cond = Peek(0, 2, kWasmI32);
    Value fval = Peek(1, 1, imm.type);
    Value tval = Peek(2, 0, imm.type);
    Value result = CreateValue(imm.type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Select, cond, fval, tval, &result);
    Drop(3);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(Br) {
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Control* c = control_at(imm.depth);
    if (!VALIDATE(TypeCheckBranch<false>(c, 0))) return 0;
    if (V8_LIKELY(current_code_reachable_and_ok_)) {
      CALL_INTERFACE(BrOrRet, imm.depth, 0);
      c->br_merge()->reached = true;
    }
    EndControl();
    return 1 + imm.length;
  }

  DECODE(BrIf) {
    BranchDepthImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Value cond = Peek(0, 0, kWasmI32);
    Control* c = control_at(imm.depth);
    if (!VALIDATE(TypeCheckBranch<true>(c, 1))) return 0;
    if (V8_LIKELY(current_code_reachable_and_ok_)) {
      CALL_INTERFACE(BrIf, cond, imm.depth);
      c->br_merge()->reached = true;
    }
    Drop(cond);
    return 1 + imm.length;
  }

  DECODE(BrTable) {
    BranchTableImmediate<validate> imm(this, this->pc_ + 1);
    BranchTableIterator<validate> iterator(this, imm);
    Value key = Peek(0, 0, kWasmI32);
    if (this->failed()) return 0;
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;

    // Cache the branch targets during the iteration, so that we can set
    // all branch targets as reachable after the {CALL_INTERFACE} call.
    std::vector<bool> br_targets(control_.size());

    uint32_t arity = 0;

    while (iterator.has_next()) {
      const uint32_t index = iterator.cur_index();
      const byte* pos = iterator.pc();
      const uint32_t target = iterator.next();
      if (!VALIDATE(target < control_depth())) {
        this->DecodeError(pos, "invalid branch depth: %u", target);
        return 0;
      }
      // Avoid redundant branch target checks.
      if (br_targets[target]) continue;
      br_targets[target] = true;

      if (validate) {
        if (index == 0) {
          arity = control_at(target)->br_merge()->arity;
        } else if (!VALIDATE(control_at(target)->br_merge()->arity == arity)) {
          this->DecodeError(
              pos, "br_table: label arity inconsistent with previous arity %d",
              arity);
          return 0;
        }
        if (!VALIDATE(TypeCheckBranch<false>(control_at(target), 1))) return 0;
      }
    }

    if (V8_LIKELY(current_code_reachable_and_ok_)) {
      CALL_INTERFACE(BrTable, imm, key);

      for (uint32_t i = 0; i < control_depth(); ++i) {
        control_at(i)->br_merge()->reached |= br_targets[i];
      }
    }
    Drop(key);
    EndControl();
    return 1 + iterator.length();
  }

  DECODE(Return) {
    return DoReturn<kNonStrictCounting, kReturnMerge>() ? 1 : 0;
  }

  DECODE(Unreachable) {
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Trap, TrapReason::kTrapUnreachable);
    EndControl();
    return 1;
  }

  DECODE(I32Const) {
    ImmI32Immediate<validate> imm(this, this->pc_ + 1);
    Value value = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(I32Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(I64Const) {
    ImmI64Immediate<validate> imm(this, this->pc_ + 1);
    Value value = CreateValue(kWasmI64);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(I64Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(F32Const) {
    ImmF32Immediate<validate> imm(this, this->pc_ + 1);
    Value value = CreateValue(kWasmF32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(F32Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(F64Const) {
    ImmF64Immediate<validate> imm(this, this->pc_ + 1);
    Value value = CreateValue(kWasmF64);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(F64Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefNull) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    HeapTypeImmediate<validate> imm(this->enabled_, this, this->pc_ + 1,
                                    this->module_);
    if (!VALIDATE(this->ok())) return 0;
    ValueType type = ValueType::Ref(imm.type, kNullable);
    Value value = CreateValue(type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefNull, type, &value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefIsNull) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    Value value = Peek(0, 0);
    Value result = CreateValue(kWasmI32);
    switch (value.type.kind()) {
      case kOptRef:
        CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, kExprRefIsNull, value,
                                           &result);
        Drop(value);
        Push(result);
        return 1;
      case kBottom:
        // We are in unreachable code, the return value does not matter.
      case kRef:
        // For non-nullable references, the result is always false.
        CALL_INTERFACE_IF_OK_AND_REACHABLE(Drop);
        Drop(value);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(I32Const, &result, 0);
        Push(result);
        return 1;
      default:
        if (validate) {
          PopTypeError(0, value, "reference type");
          return 0;
        }
        UNREACHABLE();
    }
  }

  DECODE(RefFunc) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    IndexImmediate<validate> imm(this, this->pc_ + 1, "function index");
    if (!this->ValidateFunction(this->pc_ + 1, imm)) return 0;
    HeapType heap_type(this->enabled_.has_typed_funcref()
                           ? this->module_->functions[imm.index].sig_index
                           : HeapType::kFunc);
    Value value = CreateValue(ValueType::Ref(heap_type, kNonNullable));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefFunc, imm.index, &value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefAsNonNull) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    Value value = Peek(0, 0);
    switch (value.type.kind()) {
      case kBottom:
        // We are in unreachable code. Forward the bottom value.
      case kRef:
        // A non-nullable value can remain as-is.
        return 1;
      case kOptRef: {
        Value result =
            CreateValue(ValueType::Ref(value.type.heap_type(), kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RefAsNonNull, value, &result);
        Drop(value);
        Push(result);
        return 1;
      }
      default:
        if (validate) {
          PopTypeError(0, value, "reference type");
        }
        return 0;
    }
  }

  V8_INLINE DECODE(LocalGet) {
    IndexImmediate<validate> imm(this, this->pc_ + 1, "local index");
    if (!this->ValidateLocal(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(!this->enabled_.has_nn_locals() ||
                  this->is_local_initialized(imm.index))) {
      this->DecodeError(this->pc_, "uninitialized non-defaultable local: %u",
                        imm.index);
      return 0;
    }
    Value value = CreateValue(this->local_type(imm.index));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LocalGet, &value, imm);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(LocalSet) {
    IndexImmediate<validate> imm(this, this->pc_ + 1, "local index");
    if (!this->ValidateLocal(this->pc_ + 1, imm)) return 0;
    Value value = Peek(0, 0, this->local_type(imm.index));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LocalSet, value, imm);
    Drop(value);
    this->set_local_initialized(imm.index);
    return 1 + imm.length;
  }

  DECODE(LocalTee) {
    IndexImmediate<validate> imm(this, this->pc_ + 1, "local index");
    if (!this->ValidateLocal(this->pc_ + 1, imm)) return 0;
    ValueType local_type = this->local_type(imm.index);
    Value value = Peek(0, 0, local_type);
    Value result = CreateValue(local_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LocalTee, value, &result, imm);
    Drop(value);
    Push(result);
    this->set_local_initialized(imm.index);
    return 1 + imm.length;
  }

  DECODE(Drop) {
    Peek(0, 0);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Drop);
    Drop(1);
    return 1;
  }

  DECODE(GlobalGet) {
    GlobalIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value result = CreateValue(imm.global->type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(GlobalGet, &result, imm);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(GlobalSet) {
    GlobalIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(imm.global->mutability)) {
      this->DecodeError("immutable global #%u cannot be assigned", imm.index);
      return 0;
    }
    Value value = Peek(0, 0, imm.global->type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(GlobalSet, value, imm);
    Drop(value);
    return 1 + imm.length;
  }

  DECODE(TableGet) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    IndexImmediate<validate> imm(this, this->pc_ + 1, "table index");
    if (!this->ValidateTable(this->pc_ + 1, imm)) return 0;
    Value index = Peek(0, 0, kWasmI32);
    Value result = CreateValue(this->module_->tables[imm.index].type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(TableGet, index, &result, imm);
    Drop(index);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(TableSet) {
    CHECK_PROTOTYPE_OPCODE(reftypes);
    IndexImmediate<validate> imm(this, this->pc_ + 1, "table index");
    if (!this->ValidateTable(this->pc_ + 1, imm)) return 0;
    Value value = Peek(0, 1, this->module_->tables[imm.index].type);
    Value index = Peek(1, 0, kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(TableSet, index, value, imm);
    Drop(2);
    return 1 + imm.length;
  }

  DECODE(LoadMem) {
    // Hard-code the list of load types. The opcodes are highly unlikely to
    // ever change, and we have some checks here to guard against that.
    static_assert(sizeof(LoadType) == sizeof(uint8_t), "LoadType is compact");
    static constexpr uint8_t kMinOpcode = kExprI32LoadMem;
    static constexpr uint8_t kMaxOpcode = kExprI64LoadMem32U;
    static constexpr LoadType kLoadTypes[] = {
        LoadType::kI32Load,    LoadType::kI64Load,    LoadType::kF32Load,
        LoadType::kF64Load,    LoadType::kI32Load8S,  LoadType::kI32Load8U,
        LoadType::kI32Load16S, LoadType::kI32Load16U, LoadType::kI64Load8S,
        LoadType::kI64Load8U,  LoadType::kI64Load16S, LoadType::kI64Load16U,
        LoadType::kI64Load32S, LoadType::kI64Load32U};
    STATIC_ASSERT(arraysize(kLoadTypes) == kMaxOpcode - kMinOpcode + 1);
    DCHECK_LE(kMinOpcode, opcode);
    DCHECK_GE(kMaxOpcode, opcode);
    return DecodeLoadMem(kLoadTypes[opcode - kMinOpcode]);
  }

  DECODE(StoreMem) {
    // Hard-code the list of store types. The opcodes are highly unlikely to
    // ever change, and we have some checks here to guard against that.
    static_assert(sizeof(StoreType) == sizeof(uint8_t), "StoreType is compact");
    static constexpr uint8_t kMinOpcode = kExprI32StoreMem;
    static constexpr uint8_t kMaxOpcode = kExprI64StoreMem32;
    static constexpr StoreType kStoreTypes[] = {
        StoreType::kI32Store,  StoreType::kI64Store,   StoreType::kF32Store,
        StoreType::kF64Store,  StoreType::kI32Store8,  StoreType::kI32Store16,
        StoreType::kI64Store8, StoreType::kI64Store16, StoreType::kI64Store32};
    STATIC_ASSERT(arraysize(kStoreTypes) == kMaxOpcode - kMinOpcode + 1);
    DCHECK_LE(kMinOpcode, opcode);
    DCHECK_GE(kMaxOpcode, opcode);
    return DecodeStoreMem(kStoreTypes[opcode - kMinOpcode]);
  }

  DECODE(MemoryGrow) {
    MemoryIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    // This opcode will not be emitted by the asm translator.
    DCHECK_EQ(kWasmOrigin, this->module_->origin);
    ValueType mem_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value value = Peek(0, 0, mem_type);
    Value result = CreateValue(mem_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryGrow, value, &result);
    Drop(value);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(MemorySize) {
    MemoryIndexImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ValueType result_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value result = CreateValue(result_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CurrentMemoryPages, &result);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(CallFunction) {
    CallFunctionImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.sig);
    ReturnVector returns = CreateReturnValues(imm.sig);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CallDirect, imm, args.begin(),
                                       returns.begin());
    DropArgs(imm.sig);
    PushReturns(returns);
    return 1 + imm.length;
  }

  DECODE(CallIndirect) {
    CallIndirectImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value index =
        Peek(0, static_cast<int>(imm.sig->parameter_count()), kWasmI32);
    ArgVector args = PeekArgs(imm.sig, 1);
    ReturnVector returns = CreateReturnValues(imm.sig);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CallIndirect, index, imm, args.begin(),
                                       returns.begin());
    Drop(index);
    DropArgs(imm.sig);
    PushReturns(returns);
    return 1 + imm.length;
  }

  DECODE(ReturnCall) {
    CHECK_PROTOTYPE_OPCODE(return_call);
    CallFunctionImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(this->CanReturnCall(imm.sig))) {
      this->DecodeError("%s: %s", WasmOpcodes::OpcodeName(kExprReturnCall),
                        "tail call return types mismatch");
      return 0;
    }
    ArgVector args = PeekArgs(imm.sig);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(ReturnCall, imm, args.begin());
    DropArgs(imm.sig);
    EndControl();
    return 1 + imm.length;
  }

  DECODE(ReturnCallIndirect) {
    CHECK_PROTOTYPE_OPCODE(return_call);
    CallIndirectImmediate<validate> imm(this, this->pc_ + 1);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(this->CanReturnCall(imm.sig))) {
      this->DecodeError("%s: %s",
                        WasmOpcodes::OpcodeName(kExprReturnCallIndirect),
                        "tail call return types mismatch");
      return 0;
    }
    Value index = Peek(0, 0, kWasmI32);
    ArgVector args = PeekArgs(imm.sig, 1);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(ReturnCallIndirect, index, imm,
                                       args.begin());
    Drop(index);
    DropArgs(imm.sig);
    EndControl();
    return 1 + imm.length;
  }

  DECODE(CallRef) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    Value func_ref = Peek(0, 0);
    ValueType func_type = func_ref.type;
    if (func_type == kWasmBottom) {
      // We are in unreachable code, maintain the polymorphic stack.
      return 1;
    }
    if (!VALIDATE(func_type.is_object_reference() && func_type.has_index() &&
                  this->module_->has_signature(func_type.ref_index()))) {
      PopTypeError(0, func_ref, "function reference");
      return 0;
    }
    const FunctionSig* sig = this->module_->signature(func_type.ref_index());
    ArgVector args = PeekArgs(sig, 1);
    ReturnVector returns = CreateReturnValues(sig);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CallRef, func_ref, sig,
                                       func_type.ref_index(), args.begin(),
                                       returns.begin());
    Drop(func_ref);
    DropArgs(sig);
    PushReturns(returns);
    return 1;
  }

  DECODE(ReturnCallRef) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    CHECK_PROTOTYPE_OPCODE(return_call);
    Value func_ref = Peek(0, 0);
    ValueType func_type = func_ref.type;
    if (func_type == kWasmBottom) {
      // We are in unreachable code, maintain the polymorphic stack.
      return 1;
    }
    if (!VALIDATE(func_type.is_object_reference() && func_type.has_index() &&
                  this->module_->has_signature(func_type.ref_index()))) {
      PopTypeError(0, func_ref, "function reference");
      return 0;
    }
    const FunctionSig* sig = this->module_->signature(func_type.ref_index());
    ArgVector args = PeekArgs(sig, 1);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(ReturnCallRef, func_ref, sig,
                                       func_type.ref_index(), args.begin());
    Drop(func_ref);
    DropArgs(sig);
    EndControl();
    return 1;
  }

  DECODE(Numeric) {
    uint32_t opcode_length = 0;
    WasmOpcode full_opcode = this->template read_prefixed_opcode<validate>(
        this->pc_, &opcode_length, "numeric index");
    if (full_opcode == kExprTableGrow || full_opcode == kExprTableSize ||
        full_opcode == kExprTableFill) {
      CHECK_PROTOTYPE_OPCODE(reftypes);
    }
    trace_msg->AppendOpcode(full_opcode);
    return DecodeNumericOpcode(full_opcode, opcode_length);
  }

  DECODE(Simd) {
    CHECK_PROTOTYPE_OPCODE(simd);
    if (!CheckHardwareSupportsSimd()) {
      if (FLAG_correctness_fuzzer_suppressions) {
        FATAL("Aborting on missing Wasm SIMD support");
      }
      this->DecodeError("Wasm SIMD unsupported");
      return 0;
    }
    uint32_t opcode_length = 0;
    WasmOpcode full_opcode = this->template read_prefixed_opcode<validate>(
        this->pc_, &opcode_length);
    if (!VALIDATE(this->ok())) return 0;
    trace_msg->AppendOpcode(full_opcode);
    if (!CheckSimdFeatureFlagOpcode(full_opcode)) {
      return 0;
    }
    return DecodeSimdOpcode(full_opcode, opcode_length);
  }

  DECODE(Atomic) {
    CHECK_PROTOTYPE_OPCODE(threads);
    uint32_t opcode_length = 0;
    WasmOpcode full_opcode = this->template read_prefixed_opcode<validate>(
        this->pc_, &opcode_length, "atomic index");
    trace_msg->AppendOpcode(full_opcode);
    return DecodeAtomicOpcode(full_opcode, opcode_length);
  }

  DECODE(GC) {
    CHECK_PROTOTYPE_OPCODE(gc);
    uint32_t opcode_length = 0;
    WasmOpcode full_opcode = this->template read_prefixed_opcode<validate>(
        this->pc_, &opcode_length, "gc index");
    trace_msg->AppendOpcode(full_opcode);
    return DecodeGCOpcode(full_opcode, opcode_length);
  }

#define SIMPLE_PROTOTYPE_CASE(name, opc, sig) \
  DECODE(name) { return BuildSimplePrototypeOperator(opcode); }
  FOREACH_SIMPLE_PROTOTYPE_OPCODE(SIMPLE_PROTOTYPE_CASE)
#undef SIMPLE_PROTOTYPE_CASE

  DECODE(UnknownOrAsmJs) {
    // Deal with special asmjs opcodes.
    if (!VALIDATE(is_asmjs_module(this->module_))) {
      this->DecodeError("Invalid opcode 0x%x", opcode);
      return 0;
    }
    const FunctionSig* sig = WasmOpcodes::AsmjsSignature(opcode);
    DCHECK_NOT_NULL(sig);
    return BuildSimpleOperator(opcode, sig);
  }

#undef DECODE

  static int NonConstError(WasmFullDecoder* decoder, WasmOpcode opcode) {
    decoder->DecodeError("opcode %s is not allowed in init. expressions",
                         WasmOpcodes::OpcodeName(opcode));
    return 0;
  }

  using OpcodeHandler = int (*)(WasmFullDecoder*, WasmOpcode);

  // Ideally we would use template specialization for the different opcodes, but
  // GCC does not allow to specialize templates in class scope
  // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282), and specializing
  // outside the class is not allowed for non-specialized classes.
  // Hence just list all implementations explicitly here, which also gives more
  // freedom to use the same implementation for different opcodes.
#define DECODE_IMPL(opcode) DECODE_IMPL2(kExpr##opcode, opcode)
#define DECODE_IMPL2(opcode, name)            \
  if (idx == opcode) {                        \
    if (decoding_mode == kInitExpression) {   \
      return &WasmFullDecoder::NonConstError; \
    } else {                                  \
      return &WasmFullDecoder::Decode##name;  \
    }                                         \
  }
#define DECODE_IMPL_CONST(opcode) DECODE_IMPL_CONST2(kExpr##opcode, opcode)
#define DECODE_IMPL_CONST2(opcode, name) \
  if (idx == opcode) return &WasmFullDecoder::Decode##name

  static constexpr OpcodeHandler GetOpcodeHandlerTableEntry(size_t idx) {
    DECODE_IMPL(Nop);
#define BUILD_SIMPLE_OPCODE(op, _, sig) DECODE_IMPL(op);
    FOREACH_SIMPLE_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE
    DECODE_IMPL(Block);
    DECODE_IMPL(Rethrow);
    DECODE_IMPL(Throw);
    DECODE_IMPL(Try);
    DECODE_IMPL(Catch);
    DECODE_IMPL(Delegate);
    DECODE_IMPL(CatchAll);
    DECODE_IMPL(BrOnNull);
    DECODE_IMPL(BrOnNonNull);
    DECODE_IMPL(Let);
    DECODE_IMPL(Loop);
    DECODE_IMPL(If);
    DECODE_IMPL(Else);
    DECODE_IMPL_CONST(End);
    DECODE_IMPL(Select);
    DECODE_IMPL(SelectWithType);
    DECODE_IMPL(Br);
    DECODE_IMPL(BrIf);
    DECODE_IMPL(BrTable);
    DECODE_IMPL(Return);
    DECODE_IMPL(Unreachable);
    DECODE_IMPL(NopForTestingUnsupportedInLiftoff);
    DECODE_IMPL_CONST(I32Const);
    DECODE_IMPL_CONST(I64Const);
    DECODE_IMPL_CONST(F32Const);
    DECODE_IMPL_CONST(F64Const);
    DECODE_IMPL_CONST(RefNull);
    DECODE_IMPL(RefIsNull);
    DECODE_IMPL_CONST(RefFunc);
    DECODE_IMPL(RefAsNonNull);
    DECODE_IMPL(LocalGet);
    DECODE_IMPL(LocalSet);
    DECODE_IMPL(LocalTee);
    DECODE_IMPL(Drop);
    DECODE_IMPL_CONST(GlobalGet);
    DECODE_IMPL(GlobalSet);
    DECODE_IMPL(TableGet);
    DECODE_IMPL(TableSet);
#define DECODE_LOAD_MEM(op, ...) DECODE_IMPL2(kExpr##op, LoadMem);
    FOREACH_LOAD_MEM_OPCODE(DECODE_LOAD_MEM)
#undef DECODE_LOAD_MEM
#define DECODE_STORE_MEM(op, ...) DECODE_IMPL2(kExpr##op, StoreMem);
    FOREACH_STORE_MEM_OPCODE(DECODE_STORE_MEM)
#undef DECODE_LOAD_MEM
    DECODE_IMPL(MemoryGrow);
    DECODE_IMPL(MemorySize);
    DECODE_IMPL(CallFunction);
    DECODE_IMPL(CallIndirect);
    DECODE_IMPL(ReturnCall);
    DECODE_IMPL(ReturnCallIndirect);
    DECODE_IMPL(CallRef);
    DECODE_IMPL(ReturnCallRef);
    DECODE_IMPL2(kNumericPrefix, Numeric);
    DECODE_IMPL_CONST2(kSimdPrefix, Simd);
    DECODE_IMPL2(kAtomicPrefix, Atomic);
    DECODE_IMPL_CONST2(kGCPrefix, GC);
#define SIMPLE_PROTOTYPE_CASE(name, opc, sig) DECODE_IMPL(name);
    FOREACH_SIMPLE_PROTOTYPE_OPCODE(SIMPLE_PROTOTYPE_CASE)
#undef SIMPLE_PROTOTYPE_CASE
    return &WasmFullDecoder::DecodeUnknownOrAsmJs;
  }

#undef DECODE_IMPL
#undef DECODE_IMPL2

  OpcodeHandler GetOpcodeHandler(uint8_t opcode) {
    static constexpr std::array<OpcodeHandler, 256> kOpcodeHandlers =
        base::make_array<256>(GetOpcodeHandlerTableEntry);
    return kOpcodeHandlers[opcode];
  }

  void EndControl() {
    DCHECK(!control_.empty());
    Control* current = &control_.back();
    DCHECK_LE(stack_ + current->stack_depth, stack_end_);
    stack_end_ = stack_ + current->stack_depth;
    current->reachability = kUnreachable;
    current_code_reachable_and_ok_ = false;
  }

  template <typename func>
  void InitMerge(Merge<Value>* merge, uint32_t arity, func get_val) {
    merge->arity = arity;
    if (arity == 1) {
      merge->vals.first = get_val(0);
    } else if (arity > 1) {
      merge->vals.array = this->zone()->template NewArray<Value>(arity);
      for (uint32_t i = 0; i < arity; i++) {
        merge->vals.array[i] = get_val(i);
      }
    }
  }

  // Initializes start- and end-merges of {c} with values according to the
  // in- and out-types of {c} respectively.
  void SetBlockType(Control* c, BlockTypeImmediate<validate>& imm,
                    Value* args) {
    const byte* pc = this->pc_;
    InitMerge(&c->end_merge, imm.out_arity(), [pc, &imm](uint32_t i) {
      return Value{pc, imm.out_type(i)};
    });
    InitMerge(&c->start_merge, imm.in_arity(),
#ifdef DEBUG
              [this, pc, &imm, args](uint32_t i) {
#else
              [pc, &imm, args](uint32_t i) {
#endif
                // The merge needs to be instantiated with Values of the correct
                // type even in the presence of bottom values (i.e. in
                // unreachable code). Since bottom Values will never be used for
                // code generation, we can safely instantiate new ones in that
                // case.
                DCHECK_IMPLIES(current_code_reachable_and_ok_,
                               args[i].type != kWasmBottom);
                // Warning: Do not use a ternary operator here, as gcc bugs out
                // (as of version 10.2.1).
                if (args[i].type != kWasmBottom) {
                  return args[i];
                } else {
                  return Value{pc, imm.in_type(i)};
                }
              });
  }

  // In reachable code, check if there are at least {count} values on the stack.
  // In unreachable code, if there are less than {count} values on the stack,
  // insert a number of unreachable values underneath the current values equal
  // to the difference, and return that number.
  V8_INLINE int EnsureStackArguments(int count) {
    uint32_t limit = control_.back().stack_depth;
    if (V8_LIKELY(stack_size() >= count + limit)) return 0;
    return EnsureStackArguments_Slow(count, limit);
  }

  V8_NOINLINE int EnsureStackArguments_Slow(int count, uint32_t limit) {
    if (!VALIDATE(control_.back().unreachable())) {
      int index = count - stack_size() - 1;
      NotEnoughArgumentsError(index);
    }
    // Silently create unreachable values out of thin air underneath the
    // existing stack values. To do so, we have to move existing stack values
    // upwards in the stack, then instantiate the new Values as
    // {UnreachableValue}.
    int current_values = stack_size() - limit;
    int additional_values = count - current_values;
    DCHECK_GT(additional_values, 0);
    EnsureStackSpace(additional_values);
    stack_end_ += additional_values;
    Value* stack_base = stack_value(current_values + additional_values);
    for (int i = current_values - 1; i >= 0; i--) {
      stack_base[additional_values + i] = stack_base[i];
    }
    for (int i = 0; i < additional_values; i++) {
      stack_base[i] = UnreachableValue(this->pc_);
    }
    return additional_values;
  }

  // Peeks arguments as required by signature.
  V8_INLINE ArgVector PeekArgs(const FunctionSig* sig, int depth = 0) {
    int count = sig ? static_cast<int>(sig->parameter_count()) : 0;
    if (count == 0) return {};
    EnsureStackArguments(depth + count);
    ArgVector args(stack_value(depth + count), count);
    for (int i = 0; i < count; i++) {
      ValidateArgType(args, i, sig->GetParam(i));
    }
    return args;
  }
  // Drops a number of stack elements equal to the {sig}'s parameter count (0 if
  // {sig} is null), or all of them if less are present.
  V8_INLINE void DropArgs(const FunctionSig* sig) {
    int count = sig ? static_cast<int>(sig->parameter_count()) : 0;
    Drop(count);
  }

  V8_INLINE ArgVector PeekArgs(const StructType* type, int depth = 0) {
    int count = static_cast<int>(type->field_count());
    if (count == 0) return {};
    EnsureStackArguments(depth + count);
    ArgVector args(stack_value(depth + count), count);
    for (int i = 0; i < count; i++) {
      ValidateArgType(args, i, type->field(i).Unpacked());
    }
    return args;
  }
  // Drops a number of stack elements equal to the struct's field count, or all
  // of them if less are present.
  V8_INLINE void DropArgs(const StructType* type) {
    Drop(static_cast<int>(type->field_count()));
  }

  V8_INLINE ArgVector PeekArgs(uint32_t base_index,
                               base::Vector<ValueType> arg_types) {
    int size = static_cast<int>(arg_types.size());
    EnsureStackArguments(size);
    ArgVector args(stack_value(size), arg_types.size());
    for (int i = 0; i < size; i++) {
      ValidateArgType(args, i, arg_types[i]);
    }
    return args;
  }

  ValueType GetReturnType(const FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    return sig->return_count() == 0 ? kWasmVoid : sig->GetReturn();
  }

  // TODO(jkummerow): Consider refactoring control stack management so
  // that {drop_values} is never needed. That would require decoupling
  // creation of the Control object from setting of its stack depth.
  Control* PushControl(ControlKind kind, uint32_t locals_count = 0,
                       uint32_t drop_values = 0) {
    DCHECK(!control_.empty());
    Reachability reachability = control_.back().innerReachability();
    // In unreachable code, we may run out of stack.
    uint32_t stack_depth =
        stack_size() >= drop_values ? stack_size() - drop_values : 0;
    stack_depth = std::max(stack_depth, control_.back().stack_depth);
    uint32_t init_stack_depth = this->locals_initialization_stack_depth();
    control_.emplace_back(kind, locals_count, stack_depth, init_stack_depth,
                          this->pc_, reachability);
    current_code_reachable_and_ok_ = this->ok() && reachability == kReachable;
    return &control_.back();
  }

  void PopControl() {
    // This cannot be the outermost control block.
    DCHECK_LT(1, control_.size());
    Control* c = &control_.back();
    DCHECK_LE(stack_ + c->stack_depth, stack_end_);

    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(PopControl, c);

    // - In non-unreachable code, a loop just leaves the values on the stack.
    // - In unreachable code, it is not guaranteed that we have Values of the
    //   correct types on the stack, so we have to make sure we do. Their values
    //   do not matter, so we might as well push the (uninitialized) values of
    //   the loop's end merge.
    if (!c->is_loop() || c->unreachable()) {
      PushMergeValues(c, &c->end_merge);
    }
    this->RollbackLocalsInitialization(c->init_stack_depth);

    bool parent_reached =
        c->reachable() || c->end_merge.reached || c->is_onearmed_if();
    control_.pop_back();
    // If the parent block was reachable before, but the popped control does not
    // return to here, this block becomes "spec only reachable".
    if (!parent_reached) SetSucceedingCodeDynamicallyUnreachable();
    current_code_reachable_and_ok_ = this->ok() && control_.back().reachable();
  }

  int DecodeLoadMem(LoadType type, int prefix_len = 1) {
    MemoryAccessImmediate<validate> imm =
        MakeMemoryAccessImmediate(prefix_len, type.size_log_2());
    if (!this->Validate(this->pc_ + prefix_len, imm)) return 0;
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(0, 0, index_type);
    Value result = CreateValue(type.value_type());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadMem, type, imm, index, &result);
    Drop(index);
    Push(result);
    return prefix_len + imm.length;
  }

  int DecodeLoadTransformMem(LoadType type, LoadTransformationKind transform,
                             uint32_t opcode_length) {
    // Load extends always load 64-bits.
    uint32_t max_alignment =
        transform == LoadTransformationKind::kExtend ? 3 : type.size_log_2();
    MemoryAccessImmediate<validate> imm =
        MakeMemoryAccessImmediate(opcode_length, max_alignment);
    if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(0, 0, index_type);
    Value result = CreateValue(kWasmS128);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadTransform, type, transform, imm,
                                       index, &result);
    Drop(index);
    Push(result);
    return opcode_length + imm.length;
  }

  int DecodeLoadLane(WasmOpcode opcode, LoadType type, uint32_t opcode_length) {
    MemoryAccessImmediate<validate> mem_imm =
        MakeMemoryAccessImmediate(opcode_length, type.size_log_2());
    if (!this->Validate(this->pc_ + opcode_length, mem_imm)) return 0;
    SimdLaneImmediate<validate> lane_imm(
        this, this->pc_ + opcode_length + mem_imm.length);
    if (!this->Validate(this->pc_ + opcode_length, opcode, lane_imm)) return 0;
    Value v128 = Peek(0, 1, kWasmS128);
    Value index = Peek(1, 0, kWasmI32);

    Value result = CreateValue(kWasmS128);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadLane, type, v128, index, mem_imm,
                                       lane_imm.lane, &result);
    Drop(2);
    Push(result);
    return opcode_length + mem_imm.length + lane_imm.length;
  }

  int DecodeStoreLane(WasmOpcode opcode, StoreType type,
                      uint32_t opcode_length) {
    MemoryAccessImmediate<validate> mem_imm =
        MakeMemoryAccessImmediate(opcode_length, type.size_log_2());
    if (!this->Validate(this->pc_ + opcode_length, mem_imm)) return 0;
    SimdLaneImmediate<validate> lane_imm(
        this, this->pc_ + opcode_length + mem_imm.length);
    if (!this->Validate(this->pc_ + opcode_length, opcode, lane_imm)) return 0;
    Value v128 = Peek(0, 1, kWasmS128);
    Value index = Peek(1, 0, kWasmI32);

    CALL_INTERFACE_IF_OK_AND_REACHABLE(StoreLane, type, mem_imm, index, v128,
                                       lane_imm.lane);
    Drop(2);
    return opcode_length + mem_imm.length + lane_imm.length;
  }

  int DecodeStoreMem(StoreType store, int prefix_len = 1) {
    MemoryAccessImmediate<validate> imm =
        MakeMemoryAccessImmediate(prefix_len, store.size_log_2());
    if (!this->Validate(this->pc_ + prefix_len, imm)) return 0;
    Value value = Peek(0, 1, store.value_type());
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(1, 0, index_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StoreMem, store, imm, index, value);
    Drop(2);
    return prefix_len + imm.length;
  }

  uint32_t SimdConstOp(uint32_t opcode_length) {
    Simd128Immediate<validate> imm(this, this->pc_ + opcode_length);
    Value result = CreateValue(kWasmS128);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(S128Const, imm, &result);
    Push(result);
    return opcode_length + kSimd128Size;
  }

  uint32_t SimdExtractLane(WasmOpcode opcode, ValueType type,
                           uint32_t opcode_length) {
    SimdLaneImmediate<validate> imm(this, this->pc_ + opcode_length);
    if (this->Validate(this->pc_ + opcode_length, opcode, imm)) {
      Value inputs[] = {Peek(0, 0, kWasmS128)};
      Value result = CreateValue(type);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(SimdLaneOp, opcode, imm,
                                         base::ArrayVector(inputs), &result);
      Drop(1);
      Push(result);
    }
    return opcode_length + imm.length;
  }

  uint32_t SimdReplaceLane(WasmOpcode opcode, ValueType type,
                           uint32_t opcode_length) {
    SimdLaneImmediate<validate> imm(this, this->pc_ + opcode_length);
    if (this->Validate(this->pc_ + opcode_length, opcode, imm)) {
      Value inputs[2] = {Peek(1, 0, kWasmS128), Peek(0, 1, type)};
      Value result = CreateValue(kWasmS128);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(SimdLaneOp, opcode, imm,
                                         base::ArrayVector(inputs), &result);
      Drop(2);
      Push(result);
    }
    return opcode_length + imm.length;
  }

  uint32_t Simd8x16ShuffleOp(uint32_t opcode_length) {
    Simd128Immediate<validate> imm(this, this->pc_ + opcode_length);
    if (this->Validate(this->pc_ + opcode_length, imm)) {
      Value input1 = Peek(0, 1, kWasmS128);
      Value input0 = Peek(1, 0, kWasmS128);
      Value result = CreateValue(kWasmS128);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(Simd8x16ShuffleOp, imm, input0, input1,
                                         &result);
      Drop(2);
      Push(result);
    }
    return opcode_length + 16;
  }

  uint32_t DecodeSimdOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    if (decoding_mode == kInitExpression) {
      // Currently, only s128.const is allowed in initializer expressions.
      if (opcode != kExprS128Const) {
        this->DecodeError("opcode %s is not allowed in init. expressions",
                          this->SafeOpcodeNameAt(this->pc()));
        return 0;
      }
      return SimdConstOp(opcode_length);
    }
    // opcode_length is the number of bytes that this SIMD-specific opcode takes
    // up in the LEB128 encoded form.
    switch (opcode) {
      case kExprF64x2ExtractLane:
        return SimdExtractLane(opcode, kWasmF64, opcode_length);
      case kExprF32x4ExtractLane:
        return SimdExtractLane(opcode, kWasmF32, opcode_length);
      case kExprI64x2ExtractLane:
        return SimdExtractLane(opcode, kWasmI64, opcode_length);
      case kExprI32x4ExtractLane:
      case kExprI16x8ExtractLaneS:
      case kExprI16x8ExtractLaneU:
      case kExprI8x16ExtractLaneS:
      case kExprI8x16ExtractLaneU:
        return SimdExtractLane(opcode, kWasmI32, opcode_length);
      case kExprF64x2ReplaceLane:
        return SimdReplaceLane(opcode, kWasmF64, opcode_length);
      case kExprF32x4ReplaceLane:
        return SimdReplaceLane(opcode, kWasmF32, opcode_length);
      case kExprI64x2ReplaceLane:
        return SimdReplaceLane(opcode, kWasmI64, opcode_length);
      case kExprI32x4ReplaceLane:
      case kExprI16x8ReplaceLane:
      case kExprI8x16ReplaceLane:
        return SimdReplaceLane(opcode, kWasmI32, opcode_length);
      case kExprI8x16Shuffle:
        return Simd8x16ShuffleOp(opcode_length);
      case kExprS128LoadMem:
        return DecodeLoadMem(LoadType::kS128Load, opcode_length);
      case kExprS128StoreMem:
        return DecodeStoreMem(StoreType::kS128Store, opcode_length);
      case kExprS128Load32Zero:
        return DecodeLoadTransformMem(LoadType::kI32Load,
                                      LoadTransformationKind::kZeroExtend,
                                      opcode_length);
      case kExprS128Load64Zero:
        return DecodeLoadTransformMem(LoadType::kI64Load,
                                      LoadTransformationKind::kZeroExtend,
                                      opcode_length);
      case kExprS128Load8Splat:
        return DecodeLoadTransformMem(LoadType::kI32Load8S,
                                      LoadTransformationKind::kSplat,
                                      opcode_length);
      case kExprS128Load16Splat:
        return DecodeLoadTransformMem(LoadType::kI32Load16S,
                                      LoadTransformationKind::kSplat,
                                      opcode_length);
      case kExprS128Load32Splat:
        return DecodeLoadTransformMem(
            LoadType::kI32Load, LoadTransformationKind::kSplat, opcode_length);
      case kExprS128Load64Splat:
        return DecodeLoadTransformMem(
            LoadType::kI64Load, LoadTransformationKind::kSplat, opcode_length);
      case kExprS128Load8x8S:
        return DecodeLoadTransformMem(LoadType::kI32Load8S,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load8x8U:
        return DecodeLoadTransformMem(LoadType::kI32Load8U,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load16x4S:
        return DecodeLoadTransformMem(LoadType::kI32Load16S,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load16x4U:
        return DecodeLoadTransformMem(LoadType::kI32Load16U,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load32x2S:
        return DecodeLoadTransformMem(LoadType::kI64Load32S,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load32x2U:
        return DecodeLoadTransformMem(LoadType::kI64Load32U,
                                      LoadTransformationKind::kExtend,
                                      opcode_length);
      case kExprS128Load8Lane: {
        return DecodeLoadLane(opcode, LoadType::kI32Load8S, opcode_length);
      }
      case kExprS128Load16Lane: {
        return DecodeLoadLane(opcode, LoadType::kI32Load16S, opcode_length);
      }
      case kExprS128Load32Lane: {
        return DecodeLoadLane(opcode, LoadType::kI32Load, opcode_length);
      }
      case kExprS128Load64Lane: {
        return DecodeLoadLane(opcode, LoadType::kI64Load, opcode_length);
      }
      case kExprS128Store8Lane: {
        return DecodeStoreLane(opcode, StoreType::kI32Store8, opcode_length);
      }
      case kExprS128Store16Lane: {
        return DecodeStoreLane(opcode, StoreType::kI32Store16, opcode_length);
      }
      case kExprS128Store32Lane: {
        return DecodeStoreLane(opcode, StoreType::kI32Store, opcode_length);
      }
      case kExprS128Store64Lane: {
        return DecodeStoreLane(opcode, StoreType::kI64Store, opcode_length);
      }
      case kExprS128Const:
        return SimdConstOp(opcode_length);
      default: {
        const FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (!VALIDATE(sig != nullptr)) {
          this->DecodeError("invalid simd opcode");
          return 0;
        }
        ArgVector args = PeekArgs(sig);
        if (sig->return_count() == 0) {
          CALL_INTERFACE_IF_OK_AND_REACHABLE(SimdOp, opcode,
                                             base::VectorOf(args), nullptr);
          DropArgs(sig);
        } else {
          ReturnVector results = CreateReturnValues(sig);
          CALL_INTERFACE_IF_OK_AND_REACHABLE(
              SimdOp, opcode, base::VectorOf(args), results.begin());
          DropArgs(sig);
          PushReturns(results);
        }
        return opcode_length;
      }
    }
  }

  bool ObjectRelatedWithRtt(Value obj, Value rtt) {
    return IsSubtypeOf(ValueType::Ref(rtt.type.ref_index(), kNonNullable),
                       obj.type, this->module_) ||
           IsSubtypeOf(obj.type,
                       ValueType::Ref(rtt.type.ref_index(), kNullable),
                       this->module_);
  }

#define NON_CONST_ONLY                                                 \
  if (decoding_mode == kInitExpression) {                              \
    this->DecodeError("opcode %s is not allowed in init. expressions", \
                      this->SafeOpcodeNameAt(this->pc()));             \
    return 0;                                                          \
  }

  int DecodeGCOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    switch (opcode) {
      case kExprStructNewWithRtt: {
        StructIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value rtt = Peek(0, imm.struct_type->field_count());
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(imm.struct_type->field_count(), rtt, "rtt");
          return 0;
        }
        // TODO(7748): Drop this check if {imm} is dropped from the proposal
        // à la https://github.com/WebAssembly/function-references/pull/31.
        if (!VALIDATE(
                rtt.type.is_bottom() ||
                (rtt.type.ref_index() == imm.index && rtt.type.has_depth()))) {
          PopTypeError(imm.struct_type->field_count(), rtt,
                       "rtt with depth for type " + std::to_string(imm.index));
          return 0;
        }
        ArgVector args = PeekArgs(imm.struct_type, 1);
        Value value = CreateValue(ValueType::Ref(imm.index, kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructNewWithRtt, imm, rtt,
                                           args.begin(), &value);
        Drop(rtt);
        DropArgs(imm.struct_type);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprStructNewDefault: {
        NON_CONST_ONLY
        StructIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (validate) {
          for (uint32_t i = 0; i < imm.struct_type->field_count(); i++) {
            ValueType ftype = imm.struct_type->field(i);
            if (!VALIDATE(ftype.is_defaultable())) {
              this->DecodeError(
                  "struct.new_default_with_rtt: immediate struct type %d has "
                  "field %d of non-defaultable type %s",
                  imm.index, i, ftype.name().c_str());
              return 0;
            }
          }
        }
        Value rtt = Peek(0, 0);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(0, rtt, "rtt");
          return 0;
        }
        // TODO(7748): Drop this check if {imm} is dropped from the proposal
        // à la https://github.com/WebAssembly/function-references/pull/31.
        if (!VALIDATE(
                rtt.type.is_bottom() ||
                (rtt.type.ref_index() == imm.index && rtt.type.has_depth()))) {
          PopTypeError(0, rtt,
                       "rtt with depth for type " + std::to_string(imm.index));
          return 0;
        }
        Value value = CreateValue(ValueType::Ref(imm.index, kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructNewDefault, imm, rtt, &value);
        Drop(rtt);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprStructGet: {
        NON_CONST_ONLY
        FieldImmediate<validate> field(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, field)) return 0;
        ValueType field_type =
            field.struct_imm.struct_type->field(field.field_imm.index);
        if (!VALIDATE(!field_type.is_packed())) {
          this->DecodeError(
              "struct.get: Immediate field %d of type %d has packed type %s. "
              "Use struct.get_s or struct.get_u instead.",
              field.field_imm.index, field.struct_imm.index,
              field_type.name().c_str());
          return 0;
        }
        Value struct_obj =
            Peek(0, 0, ValueType::Ref(field.struct_imm.index, kNullable));
        Value value = CreateValue(field_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructGet, struct_obj, field, true,
                                           &value);
        Drop(struct_obj);
        Push(value);
        return opcode_length + field.length;
      }
      case kExprStructGetU:
      case kExprStructGetS: {
        NON_CONST_ONLY
        FieldImmediate<validate> field(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, field)) return 0;
        ValueType field_type =
            field.struct_imm.struct_type->field(field.field_imm.index);
        if (!VALIDATE(field_type.is_packed())) {
          this->DecodeError(
              "%s: Immediate field %d of type %d has non-packed type %s. Use "
              "struct.get instead.",
              WasmOpcodes::OpcodeName(opcode), field.field_imm.index,
              field.struct_imm.index, field_type.name().c_str());
          return 0;
        }
        Value struct_obj =
            Peek(0, 0, ValueType::Ref(field.struct_imm.index, kNullable));
        Value value = CreateValue(field_type.Unpacked());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructGet, struct_obj, field,
                                           opcode == kExprStructGetS, &value);
        Drop(struct_obj);
        Push(value);
        return opcode_length + field.length;
      }
      case kExprStructSet: {
        NON_CONST_ONLY
        FieldImmediate<validate> field(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, field)) return 0;
        const StructType* struct_type = field.struct_imm.struct_type;
        if (!VALIDATE(struct_type->mutability(field.field_imm.index))) {
          this->DecodeError("struct.set: Field %d of type %d is immutable.",
                            field.field_imm.index, field.struct_imm.index);
          return 0;
        }
        Value field_value =
            Peek(0, 1, struct_type->field(field.field_imm.index).Unpacked());
        Value struct_obj =
            Peek(1, 0, ValueType::Ref(field.struct_imm.index, kNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructSet, struct_obj, field,
                                           field_value);
        Drop(2);
        return opcode_length + field.length;
      }
      case kExprArrayNewWithRtt: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value rtt = Peek(0, 2);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(2, rtt, "rtt");
          return 0;
        }
        // TODO(7748): Drop this check if {imm} is dropped from the proposal
        // à la https://github.com/WebAssembly/function-references/pull/31.
        if (!VALIDATE(
                rtt.type.is_bottom() ||
                (rtt.type.ref_index() == imm.index && rtt.type.has_depth()))) {
          PopTypeError(2, rtt,
                       "rtt with depth for type " + std::to_string(imm.index));
          return 0;
        }
        Value length = Peek(1, 1, kWasmI32);
        Value initial_value =
            Peek(2, 0, imm.array_type->element_type().Unpacked());
        Value value = CreateValue(ValueType::Ref(imm.index, kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewWithRtt, imm, length,
                                           initial_value, rtt, &value);
        Drop(3);  // rtt, length, initial_value.
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayNewDefault: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(imm.array_type->element_type().is_defaultable())) {
          this->DecodeError(
              "array.new_default_with_rtt: immediate array type %d has "
              "non-defaultable element type %s",
              imm.index, imm.array_type->element_type().name().c_str());
          return 0;
        }
        Value rtt = Peek(0, 1);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(1, rtt, "rtt");
          return 0;
        }
        // TODO(7748): Drop this check if {imm} is dropped from the proposal
        // à la https://github.com/WebAssembly/function-references/pull/31.
        if (!VALIDATE(
                rtt.type.is_bottom() ||
                (rtt.type.ref_index() == imm.index && rtt.type.has_depth()))) {
          PopTypeError(1, rtt,
                       "rtt with depth for type " + std::to_string(imm.index));
          return 0;
        }
        Value length = Peek(1, 0, kWasmI32);
        Value value = CreateValue(ValueType::Ref(imm.index, kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewDefault, imm, length, rtt,
                                           &value);
        Drop(2);  // rtt, length
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayGetS:
      case kExprArrayGetU: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(imm.array_type->element_type().is_packed())) {
          this->DecodeError(
              "%s: Immediate array type %d has non-packed type %s. Use "
              "array.get instead.",
              WasmOpcodes::OpcodeName(opcode), imm.index,
              imm.array_type->element_type().name().c_str());
          return 0;
        }
        Value index = Peek(0, 1, kWasmI32);
        Value array_obj = Peek(1, 0, ValueType::Ref(imm.index, kNullable));
        Value value = CreateValue(imm.array_type->element_type().Unpacked());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayGet, array_obj, imm, index,
                                           opcode == kExprArrayGetS, &value);
        Drop(2);  // index, array_obj
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayGet: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(!imm.array_type->element_type().is_packed())) {
          this->DecodeError(
              "array.get: Immediate array type %d has packed type %s. Use "
              "array.get_s or array.get_u instead.",
              imm.index, imm.array_type->element_type().name().c_str());
          return 0;
        }
        Value index = Peek(0, 1, kWasmI32);
        Value array_obj = Peek(1, 0, ValueType::Ref(imm.index, kNullable));
        Value value = CreateValue(imm.array_type->element_type());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayGet, array_obj, imm, index,
                                           true, &value);
        Drop(2);  // index, array_obj
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArraySet: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(imm.array_type->mutability())) {
          this->DecodeError("array.set: immediate array type %d is immutable",
                            imm.index);
          return 0;
        }
        Value value = Peek(0, 2, imm.array_type->element_type().Unpacked());
        Value index = Peek(1, 1, kWasmI32);
        Value array_obj = Peek(2, 0, ValueType::Ref(imm.index, kNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArraySet, array_obj, imm, index,
                                           value);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprArrayLen: {
        NON_CONST_ONLY
        ArrayIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value array_obj = Peek(0, 0, ValueType::Ref(imm.index, kNullable));
        Value value = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayLen, array_obj, &value);
        Drop(array_obj);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayCopy: {
        NON_CONST_ONLY
        CHECK_PROTOTYPE_OPCODE(gc_experiments);
        ArrayIndexImmediate<validate> dst_imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, dst_imm)) return 0;
        if (!VALIDATE(dst_imm.array_type->mutability())) {
          this->DecodeError(
              "array.copy: immediate destination array type #%d is immutable",
              dst_imm.index);
          return 0;
        }
        ArrayIndexImmediate<validate> src_imm(
            this, this->pc_ + opcode_length + dst_imm.length);
        if (!this->Validate(this->pc_ + opcode_length + dst_imm.length,
                            src_imm)) {
          return 0;
        }
        if (!IsSubtypeOf(src_imm.array_type->element_type(),
                         dst_imm.array_type->element_type(), this->module_)) {
          this->DecodeError(
              "array.copy: source array's #%d element type is not a subtype of "
              "destination array's #%d element type",
              src_imm.index, dst_imm.index);
          return 0;
        }
        // [dst, dst_index, src, src_index, length]
        Value dst = Peek(4, 0, ValueType::Ref(dst_imm.index, kNullable));
        Value dst_index = Peek(3, 1, kWasmI32);
        Value src = Peek(2, 2, ValueType::Ref(src_imm.index, kNullable));
        Value src_index = Peek(1, 3, kWasmI32);
        Value length = Peek(0, 4, kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayCopy, dst, dst_index, src,
                                           src_index, length);
        Drop(5);
        return opcode_length + dst_imm.length + src_imm.length;
      }
      case kExprArrayInit: {
        CHECK_PROTOTYPE_OPCODE(gc_experiments);
        if (decoding_mode != kInitExpression) {
          this->DecodeError("array.init is only allowed in init. expressions");
          return 0;
        }
        ArrayIndexImmediate<validate> array_imm(this,
                                                this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, array_imm)) return 0;
        IndexImmediate<validate> length_imm(
            this, this->pc_ + opcode_length + array_imm.length,
            "array.init length");
        uint32_t elem_count = length_imm.index;
        if (!VALIDATE(elem_count <= kV8MaxWasmArrayInitLength)) {
          this->DecodeError(
              "Requested length %u for array.init too large, maximum is %zu",
              length_imm.index, kV8MaxWasmArrayInitLength);
          return 0;
        }
        ValueType element_type = array_imm.array_type->element_type();
        std::vector<ValueType> element_types(elem_count,
                                             element_type.Unpacked());
        FunctionSig element_sig(0, elem_count, element_types.data());
        ArgVector elements = PeekArgs(&element_sig, 1);
        Value rtt = Peek(0, elem_count, ValueType::Rtt(array_imm.index));
        Value result =
            CreateValue(ValueType::Ref(array_imm.index, kNonNullable));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayInit, array_imm, elements, rtt,
                                           &result);
        Drop(elem_count + 1);
        Push(result);
        return opcode_length + array_imm.length + length_imm.length;
      }
      case kExprI31New: {
        NON_CONST_ONLY
        Value input = Peek(0, 0, kWasmI32);
        Value value = CreateValue(kWasmI31Ref);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(I31New, input, &value);
        Drop(input);
        Push(value);
        return opcode_length;
      }
      case kExprI31GetS: {
        NON_CONST_ONLY
        Value i31 = Peek(0, 0, kWasmI31Ref);
        Value value = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(I31GetS, i31, &value);
        Drop(i31);
        Push(value);
        return opcode_length;
      }
      case kExprI31GetU: {
        NON_CONST_ONLY
        Value i31 = Peek(0, 0, kWasmI31Ref);
        Value value = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(I31GetU, i31, &value);
        Drop(i31);
        Push(value);
        return opcode_length;
      }
      case kExprRttCanon: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "type index");
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        Value value = CreateValue(ValueType::Rtt(imm.index, 0));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &value);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprRttFreshSub:
        CHECK_PROTOTYPE_OPCODE(gc_experiments);
        V8_FALLTHROUGH;
      case kExprRttSub: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "type index");
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        Value parent = Peek(0, 0);
        if (parent.type.is_bottom()) {
          DCHECK(!current_code_reachable_and_ok_);
          // Just leave the unreachable/bottom value on the stack.
        } else {
          if (!VALIDATE(parent.type.is_rtt() &&
                        IsHeapSubtypeOf(imm.index, parent.type.ref_index(),
                                        this->module_))) {
            PopTypeError(
                0, parent,
                "rtt for a supertype of type " + std::to_string(imm.index));
            return 0;
          }
          Value value = parent.type.has_depth()
                            ? CreateValue(ValueType::Rtt(
                                  imm.index, parent.type.depth() + 1))
                            : CreateValue(ValueType::Rtt(imm.index));

          WasmRttSubMode mode = opcode == kExprRttSub
                                    ? WasmRttSubMode::kCanonicalize
                                    : WasmRttSubMode::kFresh;
          CALL_INTERFACE_IF_OK_AND_REACHABLE(RttSub, imm.index, parent, &value,
                                             mode);
          Drop(parent);
          Push(value);
        }
        return opcode_length + imm.length;
      }
      case kExprRefTest: {
        NON_CONST_ONLY
        // "Tests whether {obj}'s runtime type is a runtime subtype of {rtt}."
        Value rtt = Peek(0, 1);
        Value obj = Peek(1, 0);
        Value value = CreateValue(kWasmI32);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(1, rtt, "rtt");
          return 0;
        }
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type,
                                  ValueType::Ref(HeapType::kData, kNullable),
                                  this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj, "subtype of (ref null func) or (ref null data)");
          return 0;
        }
        if (current_code_reachable_and_ok_) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_LIKELY(ObjectRelatedWithRtt(obj, rtt))) {
            CALL_INTERFACE(RefTest, obj, rtt, &value);
          } else {
            // Unrelated types. Will always fail.
            CALL_INTERFACE(I32Const, &value, 0);
          }
        }
        Drop(2);
        Push(value);
        return opcode_length;
      }
      case kExprRefCast: {
        NON_CONST_ONLY
        Value rtt = Peek(0, 1);
        Value obj = Peek(1, 0);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(1, rtt, "rtt");
          return 0;
        }
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type,
                                  ValueType::Ref(HeapType::kData, kNullable),
                                  this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj, "subtype of (ref null func) or (ref null data)");
          return 0;
        }
        // If either value is bottom, we emit the most specific type possible.
        Value value =
            CreateValue(rtt.type.is_bottom()
                            ? kWasmBottom
                            : ValueType::Ref(rtt.type.ref_index(),
                                             obj.type.is_bottom()
                                                 ? kNonNullable
                                                 : obj.type.nullability()));
        if (current_code_reachable_and_ok_) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_LIKELY(ObjectRelatedWithRtt(obj, rtt))) {
            CALL_INTERFACE(RefCast, obj, rtt, &value);
          } else {
            // Unrelated types. The only way this will not trap is if the object
            // is null.
            if (obj.type.is_nullable()) {
              // Drop rtt from the stack, then assert that obj is null.
              CALL_INTERFACE(Drop);
              CALL_INTERFACE(AssertNull, obj, &value);
            } else {
              CALL_INTERFACE(Trap, TrapReason::kTrapIllegalCast);
              EndControl();
            }
          }
        }
        Drop(2);
        Push(value);
        return opcode_length;
      }
      case kExprBrOnCast: {
        NON_CONST_ONLY
        BranchDepthImmediate<validate> branch_depth(this,
                                                    this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        Value rtt = Peek(0, 1);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(1, rtt, "rtt");
          return 0;
        }
        Value obj = Peek(1, 0);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type,
                                  ValueType::Ref(HeapType::kData, kNullable),
                                  this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj, "subtype of (ref null func) or (ref null data)");
          return 0;
        }
        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError(
              "br_on_cast must target a branch of arity at least 1");
          return 0;
        }
        // Attention: contrary to most other instructions, we modify the
        // stack before calling the interface function. This makes it
        // significantly more convenient to pass around the values that
        // will be on the stack when the branch is taken.
        // TODO(jkummerow): Reconsider this choice.
        Drop(2);  // {obj} and {rtt}.
        Value result_on_branch = CreateValue(
            rtt.type.is_bottom()
                ? kWasmBottom
                : ValueType::Ref(rtt.type.ref_index(), kNonNullable));
        Push(result_on_branch);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        // This logic ensures that code generation can assume that functions
        // can only be cast to function types, and data objects to data types.
        if (V8_LIKELY(ObjectRelatedWithRtt(obj, rtt))) {
          // The {value_on_branch} parameter we pass to the interface must
          // be pointer-identical to the object on the stack, so we can't
          // reuse {result_on_branch} which was passed-by-value to {Push}.
          Value* value_on_branch = stack_value(1);
          if (V8_LIKELY(current_code_reachable_and_ok_)) {
            CALL_INTERFACE(BrOnCast, obj, rtt, value_on_branch,
                           branch_depth.depth);
            c->br_merge()->reached = true;
          }
        }
        // Otherwise the types are unrelated. Do not branch.
        Drop(result_on_branch);
        Push(obj);  // Restore stack state on fallthrough.
        return opcode_length + branch_depth.length;
      }
      case kExprBrOnCastFail: {
        NON_CONST_ONLY
        BranchDepthImmediate<validate> branch_depth(this,
                                                    this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        Value rtt = Peek(0, 1);
        if (!VALIDATE(rtt.type.is_rtt() || rtt.type.is_bottom())) {
          PopTypeError(1, rtt, "rtt");
          return 0;
        }
        Value obj = Peek(1, 0);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type,
                                  ValueType::Ref(HeapType::kData, kNullable),
                                  this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj, "subtype of (ref null func) or (ref null data)");
          return 0;
        }
        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError(
              "br_on_cast_fail must target a branch of arity at least 1");
          return 0;
        }
        // Attention: contrary to most other instructions, we modify the stack
        // before calling the interface function. This makes it significantly
        // more convenient to pass around the values that will be on the stack
        // when the branch is taken. In this case, we leave {obj} on the stack
        // to type check the branch.
        // TODO(jkummerow): Reconsider this choice.
        Drop(rtt);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        Value result_on_fallthrough = CreateValue(
            rtt.type.is_bottom()
                ? kWasmBottom
                : ValueType::Ref(rtt.type.ref_index(), kNonNullable));
        // This logic ensures that code generation can assume that functions
        // can only be cast to function types, and data objects to data types.
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          if (V8_LIKELY(ObjectRelatedWithRtt(obj, rtt))) {
            CALL_INTERFACE(BrOnCastFail, obj, rtt, &result_on_fallthrough,
                           branch_depth.depth);
          } else {
            // Drop {rtt} in the interface.
            CALL_INTERFACE(Drop);
            // Otherwise the types are unrelated. Always branch.
            CALL_INTERFACE(BrOrRet, branch_depth.depth, 0);
            // We know that the following code is not reachable, but according
            // to the spec it technically is. Set it to spec-only reachable.
            SetSucceedingCodeDynamicallyUnreachable();
          }
          c->br_merge()->reached = true;
        }
        // Make sure the correct value is on the stack state on fallthrough.
        Drop(obj);
        Push(result_on_fallthrough);
        return opcode_length + branch_depth.length;
      }
#define ABSTRACT_TYPE_CHECK(heap_type)                                  \
  case kExprRefIs##heap_type: {                                         \
    NON_CONST_ONLY                                                      \
    Value arg = Peek(0, 0, kWasmAnyRef);                                \
    Value result = CreateValue(kWasmI32);                               \
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefIs##heap_type, arg, &result); \
    Drop(arg);                                                          \
    Push(result);                                                       \
    return opcode_length;                                               \
  }

        ABSTRACT_TYPE_CHECK(Data)
        ABSTRACT_TYPE_CHECK(Func)
        ABSTRACT_TYPE_CHECK(I31)
#undef ABSTRACT_TYPE_CHECK

#define ABSTRACT_TYPE_CAST(heap_type)                                      \
  case kExprRefAs##heap_type: {                                            \
    NON_CONST_ONLY                                                         \
    Value arg = Peek(0, 0, kWasmAnyRef);                                   \
    Value result =                                                         \
        CreateValue(ValueType::Ref(HeapType::k##heap_type, kNonNullable)); \
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefAs##heap_type, arg, &result);    \
    Drop(arg);                                                             \
    Push(result);                                                          \
    return opcode_length;                                                  \
  }

        ABSTRACT_TYPE_CAST(Data)
        ABSTRACT_TYPE_CAST(Func)
        ABSTRACT_TYPE_CAST(I31)
#undef ABSTRACT_TYPE_CAST

      case kExprBrOnData:
      case kExprBrOnFunc:
      case kExprBrOnI31: {
        NON_CONST_ONLY
        BranchDepthImmediate<validate> branch_depth(this,
                                                    this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }

        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError("%s must target a branch of arity at least 1",
                            SafeOpcodeNameAt(this->pc_));
          return 0;
        }

        // Attention: contrary to most other instructions, we modify the
        // stack before calling the interface function. This makes it
        // significantly more convenient to pass around the values that
        // will be on the stack when the branch is taken.
        // TODO(jkummerow): Reconsider this choice.
        Value obj = Peek(0, 0, kWasmAnyRef);
        Drop(obj);
        HeapType::Representation heap_type =
            opcode == kExprBrOnFunc
                ? HeapType::kFunc
                : opcode == kExprBrOnData ? HeapType::kData : HeapType::kI31;
        Value result_on_branch =
            CreateValue(ValueType::Ref(heap_type, kNonNullable));
        Push(result_on_branch);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        // The {value_on_branch} parameter we pass to the interface must be
        // pointer-identical to the object on the stack, so we can't reuse
        // {result_on_branch} which was passed-by-value to {Push}.
        Value* value_on_branch = stack_value(1);
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          if (opcode == kExprBrOnFunc) {
            CALL_INTERFACE(BrOnFunc, obj, value_on_branch, branch_depth.depth);
          } else if (opcode == kExprBrOnData) {
            CALL_INTERFACE(BrOnData, obj, value_on_branch, branch_depth.depth);
          } else {
            CALL_INTERFACE(BrOnI31, obj, value_on_branch, branch_depth.depth);
          }
          c->br_merge()->reached = true;
        }
        Drop(result_on_branch);
        Push(obj);  // Restore stack state on fallthrough.
        return opcode_length + branch_depth.length;
      }
      case kExprBrOnNonData:
      case kExprBrOnNonFunc:
      case kExprBrOnNonI31: {
        NON_CONST_ONLY
        BranchDepthImmediate<validate> branch_depth(this,
                                                    this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }

        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError("%s must target a branch of arity at least 1",
                            SafeOpcodeNameAt(this->pc_));
          return 0;
        }
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;

        Value obj = Peek(0, 0, kWasmAnyRef);
        HeapType::Representation heap_type =
            opcode == kExprBrOnNonFunc
                ? HeapType::kFunc
                : opcode == kExprBrOnNonData ? HeapType::kData : HeapType::kI31;
        Value value_on_fallthrough =
            CreateValue(ValueType::Ref(heap_type, kNonNullable));

        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          if (opcode == kExprBrOnNonFunc) {
            CALL_INTERFACE(BrOnNonFunc, obj, &value_on_fallthrough,
                           branch_depth.depth);
          } else if (opcode == kExprBrOnNonData) {
            CALL_INTERFACE(BrOnNonData, obj, &value_on_fallthrough,
                           branch_depth.depth);
          } else {
            CALL_INTERFACE(BrOnNonI31, obj, &value_on_fallthrough,
                           branch_depth.depth);
          }
          c->br_merge()->reached = true;
        }
        Drop(obj);
        Push(value_on_fallthrough);
        return opcode_length + branch_depth.length;
      }
      default:
        this->DecodeError("invalid gc opcode");
        return 0;
    }
  }
#undef NON_CONST_ONLY

  uint32_t DecodeAtomicOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    ValueType ret_type;
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!VALIDATE(sig != nullptr)) {
      this->DecodeError("invalid atomic opcode");
      return 0;
    }
    MachineType memtype;
    switch (opcode) {
#define CASE_ATOMIC_STORE_OP(Name, Type)          \
  case kExpr##Name: {                             \
    memtype = MachineType::Type();                \
    ret_type = kWasmVoid;                         \
    break; /* to generic mem access code below */ \
  }
      ATOMIC_STORE_OP_LIST(CASE_ATOMIC_STORE_OP)
#undef CASE_ATOMIC_OP
#define CASE_ATOMIC_OP(Name, Type)                \
  case kExpr##Name: {                             \
    memtype = MachineType::Type();                \
    ret_type = GetReturnType(sig);                \
    break; /* to generic mem access code below */ \
  }
      ATOMIC_OP_LIST(CASE_ATOMIC_OP)
#undef CASE_ATOMIC_OP
      case kExprAtomicFence: {
        byte zero =
            this->template read_u8<validate>(this->pc_ + opcode_length, "zero");
        if (!VALIDATE(zero == 0)) {
          this->DecodeError(this->pc_ + opcode_length,
                            "invalid atomic operand");
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(AtomicFence);
        return 1 + opcode_length;
      }
      default:
        this->DecodeError("invalid atomic opcode");
        return 0;
    }

    MemoryAccessImmediate<validate> imm = MakeMemoryAccessImmediate(
        opcode_length, ElementSizeLog2Of(memtype.representation()));
    if (!this->Validate(this->pc_ + opcode_length, imm)) return false;

    // TODO(10949): Fix this for memory64 (index type should be kWasmI64
    // then).
    CHECK(!this->module_->is_memory64);
    ArgVector args = PeekArgs(sig);
    if (ret_type == kWasmVoid) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(AtomicOp, opcode, base::VectorOf(args),
                                         imm, nullptr);
      DropArgs(sig);
    } else {
      Value result = CreateValue(GetReturnType(sig));
      CALL_INTERFACE_IF_OK_AND_REACHABLE(AtomicOp, opcode, base::VectorOf(args),
                                         imm, &result);
      DropArgs(sig);
      Push(result);
    }
    return opcode_length + imm.length;
  }

  unsigned DecodeNumericOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!VALIDATE(sig != nullptr)) {
      this->DecodeError("invalid numeric opcode");
      return 0;
    }
    switch (opcode) {
      case kExprI32SConvertSatF32:
      case kExprI32UConvertSatF32:
      case kExprI32SConvertSatF64:
      case kExprI32UConvertSatF64:
      case kExprI64SConvertSatF32:
      case kExprI64UConvertSatF32:
      case kExprI64SConvertSatF64:
      case kExprI64UConvertSatF64: {
        BuildSimpleOperator(opcode, sig);
        return opcode_length;
      }
      case kExprMemoryInit: {
        MemoryInitImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value size = Peek(0, 2, sig->GetParam(2));
        Value src = Peek(1, 1, sig->GetParam(1));
        Value dst = Peek(2, 0, sig->GetParam(0));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryInit, imm, dst, src, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprDataDrop: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "data segment index");
        if (!this->ValidateDataSegment(this->pc_ + opcode_length, imm)) {
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(DataDrop, imm);
        return opcode_length + imm.length;
      }
      case kExprMemoryCopy: {
        MemoryCopyImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value size = Peek(0, 2, sig->GetParam(2));
        Value src = Peek(1, 1, sig->GetParam(1));
        Value dst = Peek(2, 0, sig->GetParam(0));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryCopy, imm, dst, src, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprMemoryFill: {
        MemoryIndexImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value size = Peek(0, 2, sig->GetParam(2));
        Value value = Peek(1, 1, sig->GetParam(1));
        Value dst = Peek(2, 0, sig->GetParam(0));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryFill, imm, dst, value, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprTableInit: {
        TableInitImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ArgVector args = PeekArgs(sig);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableInit, imm,
                                           base::VectorOf(args));
        DropArgs(sig);
        return opcode_length + imm.length;
      }
      case kExprElemDrop: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "element segment index");
        if (!this->ValidateElementSegment(this->pc_ + opcode_length, imm)) {
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ElemDrop, imm);
        return opcode_length + imm.length;
      }
      case kExprTableCopy: {
        TableCopyImmediate<validate> imm(this, this->pc_ + opcode_length);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ArgVector args = PeekArgs(sig);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableCopy, imm,
                                           base::VectorOf(args));
        DropArgs(sig);
        return opcode_length + imm.length;
      }
      case kExprTableGrow: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "table index");
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value delta = Peek(0, 1, sig->GetParam(1));
        Value value = Peek(1, 0, this->module_->tables[imm.index].type);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableGrow, imm, value, delta,
                                           &result);
        Drop(2);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprTableSize: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "table index");
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableSize, imm, &result);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprTableFill: {
        IndexImmediate<validate> imm(this, this->pc_ + opcode_length,
                                     "table index");
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value count = Peek(0, 2, sig->GetParam(2));
        Value value = Peek(1, 1, this->module_->tables[imm.index].type);
        Value start = Peek(2, 0, sig->GetParam(0));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableFill, imm, start, value, count);
        Drop(3);
        return opcode_length + imm.length;
      }
      default:
        this->DecodeError("invalid numeric opcode");
        return 0;
    }
  }

  V8_INLINE void EnsureStackSpace(int slots_needed) {
    if (V8_LIKELY(stack_capacity_end_ - stack_end_ >= slots_needed)) return;
    GrowStackSpace(slots_needed);
  }

  V8_NOINLINE void GrowStackSpace(int slots_needed) {
    size_t new_stack_capacity =
        std::max(size_t{8},
                 base::bits::RoundUpToPowerOfTwo(stack_size() + slots_needed));
    Value* new_stack =
        this->zone()->template NewArray<Value>(new_stack_capacity);
    if (stack_) {
      std::copy(stack_, stack_end_, new_stack);
      this->zone()->DeleteArray(stack_, stack_capacity_end_ - stack_);
    }
    stack_end_ = new_stack + (stack_end_ - stack_);
    stack_ = new_stack;
    stack_capacity_end_ = new_stack + new_stack_capacity;
  }

  V8_INLINE Value CreateValue(ValueType type) { return Value{this->pc_, type}; }
  V8_INLINE void Push(Value value) {
    DCHECK_NE(kWasmVoid, value.type);
    // {EnsureStackSpace} should have been called before, either in the central
    // decoding loop, or individually if more than one element is pushed.
    DCHECK_GT(stack_capacity_end_, stack_end_);
    *stack_end_ = value;
    ++stack_end_;
  }

  void PushMergeValues(Control* c, Merge<Value>* merge) {
    if (decoding_mode == kInitExpression) return;
    DCHECK_EQ(c, &control_.back());
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    DCHECK_LE(stack_ + c->stack_depth, stack_end_);
    stack_end_ = stack_ + c->stack_depth;
    if (merge->arity == 1) {
      // {EnsureStackSpace} should have been called before in the central
      // decoding loop.
      DCHECK_GT(stack_capacity_end_, stack_end_);
      *stack_end_++ = merge->vals.first;
    } else {
      EnsureStackSpace(merge->arity);
      for (uint32_t i = 0; i < merge->arity; i++) {
        *stack_end_++ = merge->vals.array[i];
      }
    }
    DCHECK_EQ(c->stack_depth + merge->arity, stack_size());
  }

  V8_INLINE ReturnVector CreateReturnValues(const FunctionSig* sig) {
    size_t return_count = sig->return_count();
    ReturnVector values(return_count);
    for (size_t i = 0; i < return_count; ++i) {
      values[i] = CreateValue(sig->GetReturn(i));
    }
    return values;
  }
  V8_INLINE void PushReturns(ReturnVector values) {
    EnsureStackSpace(static_cast<int>(values.size()));
    for (Value& value : values) Push(value);
  }

  // We do not inline these functions because doing so causes a large binary
  // size increase. Not inlining them should not create a performance
  // degradation, because their invocations are guarded by V8_LIKELY.
  V8_NOINLINE void PopTypeError(int index, Value val, const char* expected) {
    this->DecodeError(val.pc(), "%s[%d] expected %s, found %s of type %s",
                      SafeOpcodeNameAt(this->pc_), index, expected,
                      SafeOpcodeNameAt(val.pc()), val.type.name().c_str());
  }

  V8_NOINLINE void PopTypeError(int index, Value val, std::string expected) {
    PopTypeError(index, val, expected.c_str());
  }

  V8_NOINLINE void PopTypeError(int index, Value val, ValueType expected) {
    PopTypeError(index, val, ("type " + expected.name()).c_str());
  }

  V8_NOINLINE void NotEnoughArgumentsError(int index) {
    this->DecodeError(
        "not enough arguments on the stack for %s, expected %d more",
        SafeOpcodeNameAt(this->pc_), index + 1);
  }

  V8_INLINE Value Peek(int depth, int index, ValueType expected) {
    Value val = Peek(depth, index);
    if (!VALIDATE(IsSubtypeOf(val.type, expected, this->module_) ||
                  val.type == kWasmBottom || expected == kWasmBottom)) {
      PopTypeError(index, val, expected);
    }
    return val;
  }

  V8_INLINE Value Peek(int depth, int index) {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (V8_UNLIKELY(stack_size() <= limit + depth)) {
      // Peeking past the current control start in reachable code.
      if (!VALIDATE(decoding_mode == kFunctionBody &&
                    control_.back().unreachable())) {
        NotEnoughArgumentsError(index);
      }
      return UnreachableValue(this->pc_);
    }
    DCHECK_LE(stack_, stack_end_ - depth - 1);
    return *(stack_end_ - depth - 1);
  }

  V8_INLINE void ValidateArgType(ArgVector args, int index,
                                 ValueType expected) {
    Value val = args[index];
    if (!VALIDATE(IsSubtypeOf(val.type, expected, this->module_) ||
                  val.type == kWasmBottom || expected == kWasmBottom)) {
      PopTypeError(index, val, expected);
    }
  }

  // Drop the top {count} stack elements, or all of them if less than {count}
  // are present.
  V8_INLINE void Drop(int count = 1) {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (V8_UNLIKELY(stack_size() < limit + count)) {
      // Pop what we can.
      count = std::min(count, static_cast<int>(stack_size() - limit));
    }
    DCHECK_LE(stack_, stack_end_ - count);
    stack_end_ -= count;
  }
  // Drop the top stack element if present. Takes a Value input for more
  // descriptive call sites.
  V8_INLINE void Drop(const Value& /* unused */) { Drop(1); }

  enum StackElementsCountMode : bool {
    kNonStrictCounting = false,
    kStrictCounting = true
  };

  enum MergeType {
    kBranchMerge,
    kReturnMerge,
    kFallthroughMerge,
    kInitExprMerge
  };

  // - If the current code is reachable, check if the current stack values are
  //   compatible with {merge} based on their number and types. Disregard the
  //   first {drop_values} on the stack. If {strict_count}, check that
  //   #(stack elements) == {merge->arity}, otherwise
  //   #(stack elements) >= {merge->arity}.
  // - If the current code is unreachable, check if any values that may exist on
  //   top of the stack are compatible with {merge}. If {push_branch_values},
  //   push back to the stack values based on the type of {merge} (this is
  //   needed for conditional branches due to their typing rules, and
  //   fallthroughs so that the outer control finds the expected values on the
  //   stack). TODO(manoskouk): We expect the unreachable-code behavior to
  //   change, either due to relaxation of dead code verification, or the
  //   introduction of subtyping.
  template <StackElementsCountMode strict_count, bool push_branch_values,
            MergeType merge_type>
  bool TypeCheckStackAgainstMerge(uint32_t drop_values, Merge<Value>* merge) {
    static_assert(validate, "Call this function only within VALIDATE");
    constexpr const char* merge_description =
        merge_type == kBranchMerge
            ? "branch"
            : merge_type == kReturnMerge
                  ? "return"
                  : merge_type == kInitExprMerge ? "init. expression"
                                                 : "fallthru";
    uint32_t arity = merge->arity;
    uint32_t actual = stack_size() - control_.back().stack_depth;
    // Here we have to check for !unreachable(), because we need to typecheck as
    // if the current code is reachable even if it is spec-only reachable.
    if (V8_LIKELY(decoding_mode == kInitExpression ||
                  !control_.back().unreachable())) {
      if (V8_UNLIKELY(strict_count ? actual != drop_values + arity
                                   : actual < drop_values + arity)) {
        this->DecodeError("expected %u elements on the stack for %s, found %u",
                          arity, merge_description,
                          actual >= drop_values ? actual - drop_values : 0);
        return false;
      }
      // Typecheck the topmost {merge->arity} values on the stack.
      Value* stack_values = stack_end_ - (arity + drop_values);
      for (uint32_t i = 0; i < arity; ++i) {
        Value& val = stack_values[i];
        Value& old = (*merge)[i];
        if (!IsSubtypeOf(val.type, old.type, this->module_)) {
          this->DecodeError("type error in %s[%u] (expected %s, got %s)",
                            merge_description, i, old.type.name().c_str(),
                            val.type.name().c_str());
          return false;
        }
      }
      return true;
    }
    // Unreachable code validation starts here.
    if (V8_UNLIKELY(strict_count && actual > drop_values + arity)) {
      this->DecodeError("expected %u elements on the stack for %s, found %u",
                        arity, merge_description,
                        actual >= drop_values ? actual - drop_values : 0);
      return false;
    }
    // TODO(manoskouk): Use similar code as above if we keep unreachable checks.
    for (int i = arity - 1, depth = drop_values; i >= 0; --i, ++depth) {
      Peek(depth, i, (*merge)[i].type);
    }
    if (push_branch_values) {
      uint32_t inserted_value_count =
          static_cast<uint32_t>(EnsureStackArguments(drop_values + arity));
      if (inserted_value_count > 0) {
        // EnsureStackSpace may have inserted unreachable values into the bottom
        // of the stack. If so, mark them with the correct type. If drop values
        // were also inserted, disregard them, as they will be dropped anyway.
        Value* stack_base = stack_value(drop_values + arity);
        for (uint32_t i = 0; i < std::min(arity, inserted_value_count); i++) {
          if (stack_base[i].type == kWasmBottom) {
            stack_base[i].type = (*merge)[i].type;
          }
        }
      }
    }
    return this->ok();
  }

  template <StackElementsCountMode strict_count, MergeType merge_type>
  bool DoReturn() {
    if (!VALIDATE((TypeCheckStackAgainstMerge<strict_count, false, merge_type>(
            0, &control_.front().end_merge)))) {
      return false;
    }
    DCHECK_IMPLIES(current_code_reachable_and_ok_,
                   stack_size() >= this->sig_->return_count());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(DoReturn, 0);
    EndControl();
    return true;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - this->start_); }

  void FallThrough() {
    Control* c = &control_.back();
    DCHECK_NE(c->kind, kControlLoop);
    if (!VALIDATE(TypeCheckFallThru())) return;
    CALL_INTERFACE_IF_OK_AND_REACHABLE(FallThruTo, c);
    if (c->reachable()) c->end_merge.reached = true;
  }

  bool TypeCheckOneArmedIf(Control* c) {
    static_assert(validate, "Call this function only within VALIDATE");
    DCHECK(c->is_onearmed_if());
    if (c->end_merge.arity != c->start_merge.arity) {
      this->DecodeError(c->pc(),
                        "start-arity and end-arity of one-armed if must match");
      return false;
    }
    for (uint32_t i = 0; i < c->start_merge.arity; ++i) {
      Value& start = c->start_merge[i];
      Value& end = c->end_merge[i];
      if (!IsSubtypeOf(start.type, end.type, this->module_)) {
        this->DecodeError("type error in merge[%u] (expected %s, got %s)", i,
                          end.type.name().c_str(), start.type.name().c_str());
        return false;
      }
    }
    return true;
  }

  bool TypeCheckFallThru() {
    static_assert(validate, "Call this function only within VALIDATE");
    return TypeCheckStackAgainstMerge<kStrictCounting, true, kFallthroughMerge>(
        0, &control_.back().end_merge);
  }

  // If the current code is reachable, check if the current stack values are
  // compatible with a jump to {c}, based on their number and types.
  // Otherwise, we have a polymorphic stack: check if any values that may exist
  // on top of the stack are compatible with {c}. If {push_branch_values},
  // push back to the stack values based on the type of {c} (this is needed for
  // conditional branches due to their typing rules, and fallthroughs so that
  // the outer control finds enough values on the stack).
  // {drop_values} is the number of stack values that will be dropped before the
  // branch is taken. This is currently 1 for for br (condition), br_table
  // (index) and br_on_null (reference), and 0 for all other branches.
  template <bool push_branch_values>
  bool TypeCheckBranch(Control* c, uint32_t drop_values) {
    static_assert(validate, "Call this function only within VALIDATE");
    return TypeCheckStackAgainstMerge<kNonStrictCounting, push_branch_values,
                                      kBranchMerge>(drop_values, c->br_merge());
  }

  void onFirstError() override {
    this->end_ = this->pc_;  // Terminate decoding loop.
    this->current_code_reachable_and_ok_ = false;
    TRACE(" !%s\n", this->error_.message().c_str());
    // Cannot use CALL_INTERFACE_* macros because we emitted an error.
    interface().OnFirstError(this);
  }

  int BuildSimplePrototypeOperator(WasmOpcode opcode) {
    if (opcode == kExprRefEq) {
      CHECK_PROTOTYPE_OPCODE(gc);
    }
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    return BuildSimpleOperator(opcode, sig);
  }

  int BuildSimpleOperator(WasmOpcode opcode, const FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    ValueType ret = sig->return_count() == 0 ? kWasmVoid : sig->GetReturn(0);
    if (sig->parameter_count() == 1) {
      return BuildSimpleOperator(opcode, ret, sig->GetParam(0));
    } else {
      DCHECK_EQ(2, sig->parameter_count());
      return BuildSimpleOperator(opcode, ret, sig->GetParam(0),
                                 sig->GetParam(1));
    }
  }

  int BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                          ValueType arg_type) {
    Value val = Peek(0, 0, arg_type);
    if (return_type == kWasmVoid) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, opcode, val, nullptr);
      Drop(val);
    } else {
      Value ret = CreateValue(return_type);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, opcode, val, &ret);
      Drop(val);
      Push(ret);
    }
    return 1;
  }

  int BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                          ValueType lhs_type, ValueType rhs_type) {
    Value rval = Peek(0, 1, rhs_type);
    Value lval = Peek(1, 0, lhs_type);
    if (return_type == kWasmVoid) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(BinOp, opcode, lval, rval, nullptr);
      Drop(2);
    } else {
      Value ret = CreateValue(return_type);
      CALL_INTERFACE_IF_OK_AND_REACHABLE(BinOp, opcode, lval, rval, &ret);
      Drop(2);
      Push(ret);
    }
    return 1;
  }

#define DEFINE_SIMPLE_SIG_OPERATOR(sig, ...)         \
  int BuildSimpleOperator_##sig(WasmOpcode opcode) { \
    return BuildSimpleOperator(opcode, __VA_ARGS__); \
  }
  FOREACH_SIGNATURE(DEFINE_SIMPLE_SIG_OPERATOR)
#undef DEFINE_SIMPLE_SIG_OPERATOR
};

class EmptyInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kFullValidation;
  static constexpr DecodingMode decoding_mode = kFunctionBody;
  using Value = ValueBase<validate>;
  using Control = ControlBase<Value, validate>;
  using FullDecoder = WasmFullDecoder<validate, EmptyInterface>;

#define DEFINE_EMPTY_CALLBACK(name, ...) \
  void name(FullDecoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_FUNCTIONS(DEFINE_EMPTY_CALLBACK)
#undef DEFINE_EMPTY_CALLBACK
};

#undef CALL_INTERFACE_IF_OK_AND_REACHABLE
#undef CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE
#undef TRACE
#undef TRACE_INST_FORMAT
#undef VALIDATE
#undef CHECK_PROTOTYPE_OPCODE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
