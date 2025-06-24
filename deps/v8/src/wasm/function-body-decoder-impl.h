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

#include <optional>

#include "src/base/bounds.h"
#include "src/base/small-vector.h"
#include "src/base/strings.h"
#include "src/base/v8-fallthrough.h"
#include "src/strings/unicode.h"
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
struct WasmTag;

#define TRACE(...)                                        \
  do {                                                    \
    if (v8_flags.trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST_FORMAT "  @%-8d #%-30s|"

// Return the evaluation of {condition} if {ValidationTag::validate} is true,
// DCHECK that it is true and always return true otherwise.
// Note that this needs to be a macro, because the "likely" annotation does not
// survive inlining.
#ifdef DEBUG
#define VALIDATE(condition)                       \
  (ValidationTag::validate ? V8_LIKELY(condition) \
                           : ValidateAssumeTrue(condition, #condition))

V8_INLINE bool ValidateAssumeTrue(bool condition, const char* message) {
  DCHECK_WITH_MSG(condition, message);
  return true;
}
#else
#define VALIDATE(condition) (!ValidationTag::validate || V8_LIKELY(condition))
#endif

#define CHECK_PROTOTYPE_OPCODE(feat)                                         \
  DCHECK(this->module_->origin == kWasmOrigin);                              \
  if (!VALIDATE(this->enabled_.has_##feat())) {                              \
    this->DecodeError(                                                       \
        "Invalid opcode 0x%02x (enable with --experimental-wasm-" #feat ")", \
        opcode);                                                             \
    return 0;                                                                \
  }                                                                          \
  this->detected_->Add(kFeature_##feat);

static constexpr LoadType GetLoadType(WasmOpcode opcode) {
  // Hard-code the list of load types. The opcodes are highly unlikely to
  // ever change, and we have some checks here to guard against that.
  static_assert(sizeof(LoadType) == sizeof(uint8_t), "LoadType is compact");
  constexpr uint8_t kMinOpcode = kExprI32LoadMem;
  constexpr uint8_t kMaxOpcode = kExprI64LoadMem32U;
  constexpr LoadType kLoadTypes[] = {
      LoadType::kI32Load,    LoadType::kI64Load,    LoadType::kF32Load,
      LoadType::kF64Load,    LoadType::kI32Load8S,  LoadType::kI32Load8U,
      LoadType::kI32Load16S, LoadType::kI32Load16U, LoadType::kI64Load8S,
      LoadType::kI64Load8U,  LoadType::kI64Load16S, LoadType::kI64Load16U,
      LoadType::kI64Load32S, LoadType::kI64Load32U};
  static_assert(arraysize(kLoadTypes) == kMaxOpcode - kMinOpcode + 1);
  DCHECK_LE(kMinOpcode, opcode);
  DCHECK_GE(kMaxOpcode, opcode);
  return kLoadTypes[opcode - kMinOpcode];
}

static constexpr StoreType GetStoreType(WasmOpcode opcode) {
  // Hard-code the list of store types. The opcodes are highly unlikely to
  // ever change, and we have some checks here to guard against that.
  static_assert(sizeof(StoreType) == sizeof(uint8_t), "StoreType is compact");
  constexpr uint8_t kMinOpcode = kExprI32StoreMem;
  constexpr uint8_t kMaxOpcode = kExprI64StoreMem32;
  constexpr StoreType kStoreTypes[] = {
      StoreType::kI32Store,  StoreType::kI64Store,   StoreType::kF32Store,
      StoreType::kF64Store,  StoreType::kI32Store8,  StoreType::kI32Store16,
      StoreType::kI64Store8, StoreType::kI64Store16, StoreType::kI64Store32};
  static_assert(arraysize(kStoreTypes) == kMaxOpcode - kMinOpcode + 1);
  DCHECK_LE(kMinOpcode, opcode);
  DCHECK_GE(kMaxOpcode, opcode);
  return kStoreTypes[opcode - kMinOpcode];
}

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

// Decoder error with explicit PC and optional format arguments.
// Depending on the validation tag and the number of arguments, this forwards to
// a V8_NOINLINE and V8_PRESERVE_MOST method of the decoder.
template <typename ValidationTag, typename... Args>
V8_INLINE void DecodeError(Decoder* decoder, const byte* pc, const char* str,
                           Args&&... args) {
  // Decode errors can only happen if we are validating; the compiler should
  // know this e.g. from the VALIDATE macro, but this assumption tells it again
  // that this path is impossible.
  V8_ASSUME(ValidationTag::validate);
  if constexpr (!ValidationTag::full_validation) {
    decoder->MarkError();
  } else if constexpr (sizeof...(Args) == 0) {
    decoder->error(pc, str);
  } else {
    decoder->errorf(pc, str, std::forward<Args>(args)...);
  }
}

// Decoder error without explicit PC and with optional format arguments.
// Depending on the validation tag and the number of arguments, this forwards to
// a V8_NOINLINE and V8_PRESERVE_MOST method of the decoder.
template <typename ValidationTag, typename... Args>
V8_INLINE void DecodeError(Decoder* decoder, const char* str, Args&&... args) {
  // Decode errors can only happen if we are validating; the compiler should
  // know this e.g. from the VALIDATE macro, but this assumption tells it again
  // that this path is impossible.
  V8_ASSUME(ValidationTag::validate);
  if constexpr (!ValidationTag::full_validation) {
    decoder->MarkError();
  } else if constexpr (sizeof...(Args) == 0) {
    decoder->error(str);
  } else {
    decoder->errorf(str, std::forward<Args>(args)...);
  }
}

namespace value_type_reader {

template <typename ValidationTag>
std::pair<HeapType, uint32_t> read_heap_type(Decoder* decoder, const byte* pc,
                                             const WasmFeatures& enabled) {
  auto [heap_index, length] =
      decoder->read_i33v<ValidationTag>(pc, "heap type");
  if (heap_index < 0) {
    int64_t min_1_byte_leb128 = -64;
    if (!VALIDATE(heap_index >= min_1_byte_leb128)) {
      DecodeError<ValidationTag>(decoder, pc, "Unknown heap type %" PRId64,
                                 heap_index);
      return {HeapType(HeapType::kBottom), length};
    }
    uint8_t uint_7_mask = 0x7F;
    uint8_t code = static_cast<ValueTypeCode>(heap_index) & uint_7_mask;
    switch (code) {
      case kEqRefCode:
      case kI31RefCode:
      case kStructRefCode:
      case kArrayRefCode:
      case kAnyRefCode:
      case kNoneCode:
      case kNoExternCode:
      case kNoFuncCode:
        if (!VALIDATE(enabled.has_gc())) {
          DecodeError<ValidationTag>(
              decoder, pc,
              "invalid heap type '%s', enable with --experimental-wasm-gc",
              HeapType::from_code(code).name().c_str());
        }
        V8_FALLTHROUGH;
      case kExternRefCode:
      case kFuncRefCode:
        return {HeapType::from_code(code), length};
      case kStringRefCode:
      case kStringViewWtf8Code:
      case kStringViewWtf16Code:
      case kStringViewIterCode:
        if (!VALIDATE(enabled.has_stringref())) {
          DecodeError<ValidationTag>(decoder, pc,
                                     "invalid heap type '%s', enable with "
                                     "--experimental-wasm-stringref",
                                     HeapType::from_code(code).name().c_str());
        }
        return {HeapType::from_code(code), length};
      default:
        DecodeError<ValidationTag>(decoder, pc, "Unknown heap type %" PRId64,
                                   heap_index);
        return {HeapType(HeapType::kBottom), length};
    }
  } else {
    if (!VALIDATE(enabled.has_typed_funcref())) {
      DecodeError<ValidationTag>(decoder, pc,
                                 "Invalid indexed heap type, enable with "
                                 "--experimental-wasm-typed-funcref");
    }
    uint32_t type_index = static_cast<uint32_t>(heap_index);
    if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
      DecodeError<ValidationTag>(
          decoder, pc,
          "Type index %u is greater than the maximum number %zu "
          "of type definitions supported by V8",
          type_index, kV8MaxWasmTypes);
      return {HeapType(HeapType::kBottom), length};
    }
    return {HeapType(type_index), length};
  }
}

// Read a value type starting at address {pc} using {decoder}.
// No bytes are consumed.
// Returns the read value type and the number of bytes read (a.k.a. length).
template <typename ValidationTag>
std::pair<ValueType, uint32_t> read_value_type(Decoder* decoder, const byte* pc,
                                               const WasmFeatures& enabled) {
  byte val = decoder->read_u8<ValidationTag>(pc, "value type opcode");
  if (!VALIDATE(decoder->ok())) {
    return {kWasmBottom, 0};
  }
  ValueTypeCode code = static_cast<ValueTypeCode>(val);
  switch (code) {
    case kEqRefCode:
    case kI31RefCode:
    case kStructRefCode:
    case kArrayRefCode:
    case kAnyRefCode:
    case kNoneCode:
    case kNoExternCode:
    case kNoFuncCode:
      if (!VALIDATE(enabled.has_gc())) {
        DecodeError<ValidationTag>(
            decoder, pc,
            "invalid value type '%sref', enable with --experimental-wasm-gc",
            HeapType::from_code(code).name().c_str());
        return {kWasmBottom, 0};
      }
      V8_FALLTHROUGH;
    case kExternRefCode:
    case kFuncRefCode:
      return {ValueType::RefNull(HeapType::from_code(code)), 1};
    case kStringRefCode:
    case kStringViewWtf8Code:
    case kStringViewWtf16Code:
    case kStringViewIterCode: {
      if (!VALIDATE(enabled.has_stringref())) {
        DecodeError<ValidationTag>(decoder, pc,
                                   "invalid value type '%sref', enable with "
                                   "--experimental-wasm-stringref",
                                   HeapType::from_code(code).name().c_str());
        return {kWasmBottom, 0};
      }
      return {ValueType::RefNull(HeapType::from_code(code)), 1};
    }
    case kI32Code:
      return {kWasmI32, 1};
    case kI64Code:
      return {kWasmI64, 1};
    case kF32Code:
      return {kWasmF32, 1};
    case kF64Code:
      return {kWasmF64, 1};
    case kRefCode:
    case kRefNullCode: {
      Nullability nullability = code == kRefNullCode ? kNullable : kNonNullable;
      if (!VALIDATE(enabled.has_typed_funcref())) {
        DecodeError<ValidationTag>(
            decoder, pc,
            "Invalid type '(ref%s <heaptype>)', enable with "
            "--experimental-wasm-typed-funcref",
            nullability == kNullable ? " null" : "");
        return {kWasmBottom, 0};
      }
      auto [heap_type, length] =
          read_heap_type<ValidationTag>(decoder, pc + 1, enabled);
      ValueType type = heap_type.is_bottom()
                           ? kWasmBottom
                           : ValueType::RefMaybeNull(heap_type, nullability);
      return {type, length + 1};
    }
    case kS128Code: {
      if (!VALIDATE(CheckHardwareSupportsSimd())) {
        DecodeError<ValidationTag>(decoder, pc, "Wasm SIMD unsupported");
        return {kWasmBottom, 0};
      }
      return {kWasmS128, 1};
    }
    // Although these codes are included in ValueTypeCode, they technically
    // do not correspond to value types and are only used in specific
    // contexts. The caller of this function is responsible for handling them.
    case kVoidCode:
    case kI8Code:
    case kI16Code:
      // Fall through to the error reporting below.
      break;
  }
  // Anything that doesn't match an enumeration value is an invalid type code.
  if constexpr (!ValidationTag::validate) UNREACHABLE();
  DecodeError<ValidationTag>(decoder, pc, "invalid value type 0x%x", code);
  return {kWasmBottom, 0};
}

template <typename ValidationTag>
bool ValidateHeapType(Decoder* decoder, const byte* pc,
                      const WasmModule* module, HeapType type) {
  if (!type.is_index()) return true;
  // A {nullptr} module is accepted if we are not validating anyway (e.g. for
  // opcode length computation).
  if (!ValidationTag::validate && module == nullptr) return true;
  if (!VALIDATE(type.ref_index() < module->types.size())) {
    DecodeError<ValidationTag>(decoder, pc, "Type index %u is out of bounds",
                               type.ref_index());
    return false;
  }
  return true;
}

template <typename ValidationTag>
bool ValidateValueType(Decoder* decoder, const byte* pc,
                       const WasmModule* module, ValueType type) {
  if (V8_LIKELY(!type.is_object_reference())) return true;
  return ValidateHeapType<ValidationTag>(decoder, pc, module, type.heap_type());
}

}  // namespace value_type_reader

enum DecodingMode { kFunctionBody, kConstantExpression };

// Helpers for decoding different kinds of immediates which follow bytecodes.
struct ImmI32Immediate {
  int32_t value;
  uint32_t length;

  template <typename ValidationTag>
  ImmI32Immediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    std::tie(value, length) = decoder->read_i32v<ValidationTag>(pc, "immi32");
  }
};

struct ImmI64Immediate {
  int64_t value;
  uint32_t length;

  template <typename ValidationTag>
  ImmI64Immediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    std::tie(value, length) = decoder->read_i64v<ValidationTag>(pc, "immi64");
  }
};

struct ImmF32Immediate {
  float value;
  uint32_t length = 4;

  template <typename ValidationTag>
  ImmF32Immediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    // We can't use base::bit_cast here because calling any helper function
    // that returns a float would potentially flip NaN bits per C++ semantics,
    // so we have to inline the memcpy call directly.
    uint32_t tmp = decoder->read_u32<ValidationTag>(pc, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

struct ImmF64Immediate {
  double value;
  uint32_t length = 8;

  template <typename ValidationTag>
  ImmF64Immediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    // Avoid base::bit_cast because it might not preserve the signalling bit
    // of a NaN.
    uint64_t tmp = decoder->read_u64<ValidationTag>(pc, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

struct MemoryIndexImmediate {
  uint8_t index = 0;
  uint32_t length = 1;

  template <typename ValidationTag>
  MemoryIndexImmediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    index = decoder->read_u8<ValidationTag>(pc, "memory index");
  }
};

// Parent class for all Immediates which read a u32v index value in their
// constructor.
struct IndexImmediate {
  uint32_t index;
  uint32_t length;

  template <typename ValidationTag>
  IndexImmediate(Decoder* decoder, const byte* pc, const char* name,
                 ValidationTag = {}) {
    std::tie(index, length) = decoder->read_u32v<ValidationTag>(pc, name);
  }
};

struct TagIndexImmediate : public IndexImmediate {
  const WasmTag* tag = nullptr;

  template <typename ValidationTag>
  TagIndexImmediate(Decoder* decoder, const byte* pc,
                    ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "tag index", validate) {}
};

struct GlobalIndexImmediate : public IndexImmediate {
  const WasmGlobal* global = nullptr;

  template <typename ValidationTag>
  GlobalIndexImmediate(Decoder* decoder, const byte* pc,
                       ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "global index", validate) {}
};

struct SigIndexImmediate : public IndexImmediate {
  const FunctionSig* sig = nullptr;

  template <typename ValidationTag>
  SigIndexImmediate(Decoder* decoder, const byte* pc,
                    ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "signature index", validate) {}
};

struct StructIndexImmediate : public IndexImmediate {
  const StructType* struct_type = nullptr;

  template <typename ValidationTag>
  StructIndexImmediate(Decoder* decoder, const byte* pc,
                       ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "struct index", validate) {}
};

struct ArrayIndexImmediate : public IndexImmediate {
  const ArrayType* array_type = nullptr;

  template <typename ValidationTag>
  ArrayIndexImmediate(Decoder* decoder, const byte* pc,
                      ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "array index", validate) {}
};

struct CallFunctionImmediate : public IndexImmediate {
  const FunctionSig* sig = nullptr;

  template <typename ValidationTag>
  CallFunctionImmediate(Decoder* decoder, const byte* pc,
                        ValidationTag validate = {})
      : IndexImmediate(decoder, pc, "function index", validate) {}
};

struct SelectTypeImmediate {
  uint32_t length;
  ValueType type;

  template <typename ValidationTag>
  SelectTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                      const byte* pc, ValidationTag = {}) {
    uint8_t num_types;
    std::tie(num_types, length) =
        decoder->read_u32v<ValidationTag>(pc, "number of select types");
    if (!VALIDATE(num_types == 1)) {
      DecodeError<ValidationTag>(
          decoder, pc,
          "Invalid number of types. Select accepts exactly one type");
      return;
    }
    uint32_t type_length;
    std::tie(type, type_length) =
        value_type_reader::read_value_type<ValidationTag>(decoder, pc + length,
                                                          enabled);
    length += type_length;
  }
};

struct BlockTypeImmediate {
  uint32_t length = 1;
  // After decoding, either {sig_index} is set XOR {sig} points to
  // {single_return_sig_storage}.
  uint32_t sig_index;
  FunctionSig sig{0, 0, single_return_sig_storage};
  // Internal field, potentially pointed to by {sig}. Do not access directly.
  ValueType single_return_sig_storage[1];

  // Do not copy or move, as {sig} might point to {single_return_sig_storage} so
  // this cannot trivially be copied. If needed, define those operators later.
  BlockTypeImmediate(const BlockTypeImmediate&) = delete;
  BlockTypeImmediate(BlockTypeImmediate&&) = delete;
  BlockTypeImmediate& operator=(const BlockTypeImmediate&) = delete;
  BlockTypeImmediate& operator=(BlockTypeImmediate&&) = delete;

  template <typename ValidationTag>
  BlockTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                     const byte* pc, ValidationTag = {}) {
    int64_t block_type;
    std::tie(block_type, length) =
        decoder->read_i33v<ValidationTag>(pc, "block type");
    if (block_type < 0) {
      // All valid negative types are 1 byte in length, so we check against the
      // minimum 1-byte LEB128 value.
      constexpr int64_t min_1_byte_leb128 = -64;
      if (!VALIDATE(block_type >= min_1_byte_leb128)) {
        DecodeError<ValidationTag>(decoder, pc, "invalid block type %" PRId64,
                                   block_type);
        return;
      }
      if (static_cast<ValueTypeCode>(block_type & 0x7F) != kVoidCode) {
        sig = FunctionSig{1, 0, single_return_sig_storage};
        std::tie(single_return_sig_storage[0], length) =
            value_type_reader::read_value_type<ValidationTag>(decoder, pc,
                                                              enabled);
      }
    } else {
      sig = FunctionSig{0, 0, nullptr};
      sig_index = static_cast<uint32_t>(block_type);
    }
  }

  uint32_t in_arity() const {
    return static_cast<uint32_t>(sig.parameter_count());
  }
  uint32_t out_arity() const {
    return static_cast<uint32_t>(sig.return_count());
  }
  ValueType in_type(uint32_t index) { return sig.GetParam(index); }
  ValueType out_type(uint32_t index) { return sig.GetReturn(index); }
};

struct BranchDepthImmediate {
  uint32_t depth;
  uint32_t length;

  template <typename ValidationTag>
  BranchDepthImmediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    std::tie(depth, length) =
        decoder->read_u32v<ValidationTag>(pc, "branch depth");
  }
};

struct FieldImmediate {
  StructIndexImmediate struct_imm;
  IndexImmediate field_imm;
  uint32_t length;

  template <typename ValidationTag>
  FieldImmediate(Decoder* decoder, const byte* pc, ValidationTag validate = {})
      : struct_imm(decoder, pc, validate),
        field_imm(decoder, pc + struct_imm.length, "field index", validate),
        length(struct_imm.length + field_imm.length) {}
};

struct CallIndirectImmediate {
  IndexImmediate sig_imm;
  IndexImmediate table_imm;
  uint32_t length;
  const FunctionSig* sig = nullptr;

  template <typename ValidationTag>
  CallIndirectImmediate(Decoder* decoder, const byte* pc,
                        ValidationTag validate = {})
      : sig_imm(decoder, pc, "singature index", validate),
        table_imm(decoder, pc + sig_imm.length, "table index", validate),
        length(sig_imm.length + table_imm.length) {}
};

struct BranchTableImmediate {
  uint32_t table_count;
  const byte* start;
  const byte* table;

  template <typename ValidationTag>
  BranchTableImmediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    start = pc;
    uint32_t len;
    std::tie(table_count, len) =
        decoder->read_u32v<ValidationTag>(pc, "table count");
    table = pc + len;
  }
};

// A helper to iterate over a branch table.
template <typename ValidationTag>
class BranchTableIterator {
 public:
  uint32_t cur_index() { return index_; }
  bool has_next() { return VALIDATE(decoder_->ok()) && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    auto [result, length] =
        decoder_->read_u32v<ValidationTag>(pc_, "branch table entry");
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

  BranchTableIterator(Decoder* decoder, const BranchTableImmediate& imm)
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

struct MemoryAccessImmediate {
  uint32_t alignment;
  uint64_t offset;
  uint32_t length = 0;

  template <typename ValidationTag>
  V8_INLINE MemoryAccessImmediate(Decoder* decoder, const byte* pc,
                                  uint32_t max_alignment, bool is_memory64,
                                  ValidationTag = {}) {
    // Check for the fast path (two single-byte LEBs).
    const bool two_bytes = !ValidationTag::validate || decoder->end() - pc >= 2;
    const bool use_fast_path = two_bytes && !((pc[0] | pc[1]) & 0x80);
    if (V8_LIKELY(use_fast_path)) {
      alignment = pc[0];
      offset = pc[1];
      length = 2;
    } else {
      ConstructSlow<ValidationTag>(decoder, pc, max_alignment, is_memory64);
    }
    if (!VALIDATE(alignment <= max_alignment)) {
      DecodeError<ValidationTag>(
          decoder, pc,
          "invalid alignment; expected maximum alignment is %u, "
          "actual alignment is %u",
          max_alignment, alignment);
    }
  }

 private:
  template <typename ValidationTag>
  V8_NOINLINE V8_PRESERVE_MOST void ConstructSlow(Decoder* decoder,
                                                  const byte* pc,
                                                  uint32_t max_alignment,
                                                  bool is_memory64) {
    uint32_t alignment_length;
    std::tie(alignment, alignment_length) =
        decoder->read_u32v<ValidationTag>(pc, "alignment");
    uint32_t offset_length;
    if (is_memory64) {
      std::tie(offset, offset_length) =
          decoder->read_u64v<ValidationTag>(pc + alignment_length, "offset");
    } else {
      std::tie(offset, offset_length) =
          decoder->read_u32v<ValidationTag>(pc + alignment_length, "offset");
    }
    length = alignment_length + offset_length;
  }
};

// Immediate for SIMD lane operations.
struct SimdLaneImmediate {
  uint8_t lane;
  uint32_t length = 1;

  template <typename ValidationTag>
  SimdLaneImmediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    lane = decoder->read_u8<ValidationTag>(pc, "lane");
  }
};

// Immediate for SIMD S8x16 shuffle operations.
struct Simd128Immediate {
  uint8_t value[kSimd128Size] = {0};

  template <typename ValidationTag>
  Simd128Immediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      value[i] = decoder->read_u8<ValidationTag>(pc + i, "value");
    }
  }
};

struct MemoryInitImmediate {
  IndexImmediate data_segment;
  MemoryIndexImmediate memory;
  uint32_t length;

  template <typename ValidationTag>
  MemoryInitImmediate(Decoder* decoder, const byte* pc,
                      ValidationTag validate = {})
      : data_segment(decoder, pc, "data segment index", validate),
        memory(decoder, pc + data_segment.length, validate),
        length(data_segment.length + memory.length) {}
};

struct MemoryCopyImmediate {
  MemoryIndexImmediate memory_src;
  MemoryIndexImmediate memory_dst;
  uint32_t length;

  template <typename ValidationTag>
  MemoryCopyImmediate(Decoder* decoder, const byte* pc,
                      ValidationTag validate = {})
      : memory_src(decoder, pc, validate),
        memory_dst(decoder, pc + memory_src.length, validate),
        length(memory_src.length + memory_dst.length) {}
};

struct TableInitImmediate {
  IndexImmediate element_segment;
  IndexImmediate table;
  uint32_t length;

  template <typename ValidationTag>
  TableInitImmediate(Decoder* decoder, const byte* pc,
                     ValidationTag validate = {})
      : element_segment(decoder, pc, "element segment index", validate),
        table(decoder, pc + element_segment.length, "table index", validate),
        length(element_segment.length + table.length) {}
};

struct TableCopyImmediate {
  IndexImmediate table_dst;
  IndexImmediate table_src;
  uint32_t length;

  template <typename ValidationTag>
  TableCopyImmediate(Decoder* decoder, const byte* pc,
                     ValidationTag validate = {})
      : table_dst(decoder, pc, "table index", validate),
        table_src(decoder, pc + table_dst.length, "table index", validate),
        length(table_src.length + table_dst.length) {}
};

struct HeapTypeImmediate {
  uint32_t length;
  HeapType type{kBottom};

  template <typename ValidationTag>
  HeapTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                    const byte* pc, ValidationTag = {}) {
    std::tie(type, length) =
        value_type_reader::read_heap_type<ValidationTag>(decoder, pc, enabled);
  }
};

struct StringConstImmediate {
  uint32_t index;
  uint32_t length;

  template <typename ValidationTag>
  StringConstImmediate(Decoder* decoder, const byte* pc, ValidationTag = {}) {
    std::tie(index, length) =
        decoder->read_u32v<ValidationTag>(pc, "stringref literal index");
  }
};

template <bool full_validation>
struct PcForErrors {
  static_assert(full_validation == false);
  explicit PcForErrors(const byte* /* pc */) {}

  const byte* pc() const { return nullptr; }
};

template <>
struct PcForErrors<true> {
  const byte* pc_for_errors = nullptr;

  explicit PcForErrors(const byte* pc) : pc_for_errors(pc) {}

  const byte* pc() const { return pc_for_errors; }
};

// An entry on the value stack.
template <typename ValidationTag>
struct ValueBase : public PcForErrors<ValidationTag::full_validation> {
  ValueType type = kWasmVoid;

  ValueBase(const byte* pc, ValueType type)
      : PcForErrors<ValidationTag::full_validation>(pc), type(type) {}
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
template <typename Value, typename ValidationTag>
struct ControlBase : public PcForErrors<ValidationTag::full_validation> {
  ControlKind kind = kControlBlock;
  Reachability reachability = kReachable;
  uint32_t stack_depth = 0;  // Stack height at the beginning of the construct.
  uint32_t init_stack_depth = 0;  // Height of "locals initialization" stack
                                  // at the beginning of the construct.
  int32_t previous_catch = -1;  // Depth of the innermost catch containing this
                                // 'try'.

  // Values merged into the start or end of this control construct.
  Merge<Value> start_merge;
  Merge<Value> end_merge;

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ControlBase);

  ControlBase(Zone* /* unused in the base class */, ControlKind kind,
              uint32_t stack_depth, uint32_t init_stack_depth,
              const uint8_t* pc, Reachability reachability)
      : PcForErrors<ValidationTag::full_validation>(pc),
        kind(kind),
        reachability(reachability),
        stack_depth(stack_depth),
        init_stack_depth(init_stack_depth),
        start_merge(reachability == kReachable) {}

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
  F(TraceInstruction, uint32_t value)  \
  F(StartFunction)                     \
  F(StartFunctionBody, Control* block) \
  F(FinishFunction)                    \
  F(OnFirstError)                      \
  F(NextInstruction, WasmOpcode)

#define INTERFACE_CONSTANT_FUNCTIONS(F) /*       force 80 columns           */ \
  F(I32Const, Value* result, int32_t value)                                    \
  F(I64Const, Value* result, int64_t value)                                    \
  F(F32Const, Value* result, float value)                                      \
  F(F64Const, Value* result, double value)                                     \
  F(S128Const, const Simd128Immediate& imm, Value* result)                     \
  F(GlobalGet, Value* result, const GlobalIndexImmediate& imm)                 \
  F(DoReturn, uint32_t drop_values)                                            \
  F(UnOp, WasmOpcode opcode, const Value& value, Value* result)                \
  F(BinOp, WasmOpcode opcode, const Value& lhs, const Value& rhs,              \
    Value* result)                                                             \
  F(RefNull, ValueType type, Value* result)                                    \
  F(RefFunc, uint32_t function_index, Value* result)                           \
  F(StructNew, const StructIndexImmediate& imm, const Value& rtt,              \
    const Value args[], Value* result)                                         \
  F(StructNewDefault, const StructIndexImmediate& imm, const Value& rtt,       \
    Value* result)                                                             \
  F(ArrayNew, const ArrayIndexImmediate& imm, const Value& length,             \
    const Value& initial_value, const Value& rtt, Value* result)               \
  F(ArrayNewDefault, const ArrayIndexImmediate& imm, const Value& length,      \
    const Value& rtt, Value* result)                                           \
  F(ArrayNewFixed, const ArrayIndexImmediate& imm,                             \
    const base::Vector<Value>& elements, const Value& rtt, Value* result)      \
  F(ArrayNewSegment, const ArrayIndexImmediate& array_imm,                     \
    const IndexImmediate& data_segment, const Value& offset,                   \
    const Value& length, const Value& rtt, Value* result)                      \
  F(I31New, const Value& input, Value* result)                                 \
  F(RttCanon, uint32_t type_index, Value* result)                              \
  F(StringConst, const StringConstImmediate& imm, Value* result)

#define INTERFACE_NON_CONSTANT_FUNCTIONS(F) /*       force 80 columns       */ \
  /* Control: */                                                               \
  F(Block, Control* block)                                                     \
  F(Loop, Control* block)                                                      \
  F(Try, Control* block)                                                       \
  F(If, const Value& cond, Control* if_block)                                  \
  F(FallThruTo, Control* c)                                                    \
  F(PopControl, Control* block)                                                \
  /* Instructions: */                                                          \
  F(RefAsNonNull, const Value& arg, Value* result)                             \
  F(Drop)                                                                      \
  F(LocalGet, Value* result, const IndexImmediate& imm)                        \
  F(LocalSet, const Value& value, const IndexImmediate& imm)                   \
  F(LocalTee, const Value& value, Value* result, const IndexImmediate& imm)    \
  F(GlobalSet, const Value& value, const GlobalIndexImmediate& imm)            \
  F(TableGet, const Value& index, Value* result, const IndexImmediate& imm)    \
  F(TableSet, const Value& index, const Value& value,                          \
    const IndexImmediate& imm)                                                 \
  F(Trap, TrapReason reason)                                                   \
  F(NopForTestingUnsupportedInLiftoff)                                         \
  F(Forward, const Value& from, Value* to)                                     \
  F(Select, const Value& cond, const Value& fval, const Value& tval,           \
    Value* result)                                                             \
  F(BrOrRet, uint32_t depth, uint32_t drop_values)                             \
  F(BrIf, const Value& cond, uint32_t depth)                                   \
  F(BrTable, const BranchTableImmediate& imm, const Value& key)                \
  F(Else, Control* if_block)                                                   \
  F(LoadMem, LoadType type, const MemoryAccessImmediate& imm,                  \
    const Value& index, Value* result)                                         \
  F(LoadTransform, LoadType type, LoadTransformationKind transform,            \
    const MemoryAccessImmediate& imm, const Value& index, Value* result)       \
  F(LoadLane, LoadType type, const Value& value, const Value& index,           \
    const MemoryAccessImmediate& imm, const uint8_t laneidx, Value* result)    \
  F(StoreMem, StoreType type, const MemoryAccessImmediate& imm,                \
    const Value& index, const Value& value)                                    \
  F(StoreLane, StoreType type, const MemoryAccessImmediate& imm,               \
    const Value& index, const Value& value, const uint8_t laneidx)             \
  F(CurrentMemoryPages, Value* result)                                         \
  F(MemoryGrow, const Value& value, Value* result)                             \
  F(CallDirect, const CallFunctionImmediate& imm, const Value args[],          \
    Value returns[])                                                           \
  F(CallIndirect, const Value& index, const CallIndirectImmediate& imm,        \
    const Value args[], Value returns[])                                       \
  F(CallRef, const Value& func_ref, const FunctionSig* sig,                    \
    uint32_t sig_index, const Value args[], const Value returns[])             \
  F(ReturnCallRef, const Value& func_ref, const FunctionSig* sig,              \
    uint32_t sig_index, const Value args[])                                    \
  F(ReturnCall, const CallFunctionImmediate& imm, const Value args[])          \
  F(ReturnCallIndirect, const Value& index, const CallIndirectImmediate& imm,  \
    const Value args[])                                                        \
  F(BrOnNull, const Value& ref_object, uint32_t depth,                         \
    bool pass_null_along_branch, Value* result_on_fallthrough)                 \
  F(BrOnNonNull, const Value& ref_object, Value* result, uint32_t depth,       \
    bool drop_null_on_fallthrough)                                             \
  F(SimdOp, WasmOpcode opcode, base::Vector<Value> args, Value* result)        \
  F(SimdLaneOp, WasmOpcode opcode, const SimdLaneImmediate& imm,               \
    const base::Vector<Value> inputs, Value* result)                           \
  F(Simd8x16ShuffleOp, const Simd128Immediate& imm, const Value& input0,       \
    const Value& input1, Value* result)                                        \
  F(Throw, const TagIndexImmediate& imm, const base::Vector<Value>& args)      \
  F(Rethrow, Control* block)                                                   \
  F(CatchException, const TagIndexImmediate& imm, Control* block,              \
    base::Vector<Value> caught_values)                                         \
  F(Delegate, uint32_t depth, Control* block)                                  \
  F(CatchAll, Control* block)                                                  \
  F(AtomicOp, WasmOpcode opcode, base::Vector<Value> args,                     \
    const MemoryAccessImmediate& imm, Value* result)                           \
  F(AtomicFence)                                                               \
  F(MemoryInit, const MemoryInitImmediate& imm, const Value& dst,              \
    const Value& src, const Value& size)                                       \
  F(DataDrop, const IndexImmediate& imm)                                       \
  F(MemoryCopy, const MemoryCopyImmediate& imm, const Value& dst,              \
    const Value& src, const Value& size)                                       \
  F(MemoryFill, const MemoryIndexImmediate& imm, const Value& dst,             \
    const Value& value, const Value& size)                                     \
  F(TableInit, const TableInitImmediate& imm, base::Vector<Value> args)        \
  F(ElemDrop, const IndexImmediate& imm)                                       \
  F(TableCopy, const TableCopyImmediate& imm, base::Vector<Value> args)        \
  F(TableGrow, const IndexImmediate& imm, const Value& value,                  \
    const Value& delta, Value* result)                                         \
  F(TableSize, const IndexImmediate& imm, Value* result)                       \
  F(TableFill, const IndexImmediate& imm, const Value& start,                  \
    const Value& value, const Value& count)                                    \
  F(StructGet, const Value& struct_object, const FieldImmediate& field,        \
    bool is_signed, Value* result)                                             \
  F(StructSet, const Value& struct_object, const FieldImmediate& field,        \
    const Value& field_value)                                                  \
  F(ArrayGet, const Value& array_obj, const ArrayIndexImmediate& imm,          \
    const Value& index, bool is_signed, Value* result)                         \
  F(ArraySet, const Value& array_obj, const ArrayIndexImmediate& imm,          \
    const Value& index, const Value& value)                                    \
  F(ArrayLen, const Value& array_obj, Value* result)                           \
  F(ArrayCopy, const Value& src, const Value& src_index, const Value& dst,     \
    const Value& dst_index, const Value& length)                               \
  F(ArrayFill, const ArrayIndexImmediate& imm, const Value& array,             \
    const Value& index, const Value& value, const Value& length)               \
  F(I31GetS, const Value& input, Value* result)                                \
  F(I31GetU, const Value& input, Value* result)                                \
  F(RefTest, const Value& obj, const Value& rtt, Value* result,                \
    bool null_succeeds)                                                        \
  F(RefTestAbstract, const Value& obj, HeapType type, Value* result,           \
    bool null_succeeds)                                                        \
  F(RefCast, const Value& obj, const Value& rtt, Value* result,                \
    bool null_succeeds)                                                        \
  F(RefCastAbstract, const Value& obj, HeapType type, Value* result,           \
    bool null_succeeds)                                                        \
  F(AssertNullTypecheck, const Value& obj, Value* result)                      \
  F(AssertNotNullTypecheck, const Value& obj, Value* result)                   \
  F(BrOnCast, const Value& obj, const Value& rtt, Value* result_on_branch,     \
    uint32_t depth, bool null_succeeds)                                        \
  F(BrOnCastFail, const Value& obj, const Value& rtt,                          \
    Value* result_on_fallthrough, uint32_t depth, bool null_succeeds)          \
  F(BrOnCastAbstract, const Value& obj, HeapType type,                         \
    Value* result_on_branch, uint32_t depth, bool null_succeeds)               \
  F(BrOnCastFailAbstract, const Value& obj, HeapType type,                     \
    Value* result_on_fallthrough, uint32_t depth, bool null_succeeds)          \
  F(RefIsStruct, const Value& object, Value* result)                           \
  F(RefIsEq, const Value& object, Value* result)                               \
  F(RefIsI31, const Value& object, Value* result)                              \
  F(RefIsArray, const Value& object, Value* result)                            \
  F(RefAsStruct, const Value& object, Value* result)                           \
  F(RefAsI31, const Value& object, Value* result)                              \
  F(RefAsArray, const Value& object, Value* result)                            \
  F(BrOnStruct, const Value& object, Value* value_on_branch,                   \
    uint32_t br_depth, bool null_succeeds)                                     \
  F(BrOnI31, const Value& object, Value* value_on_branch, uint32_t br_depth,   \
    bool null_succeeds)                                                        \
  F(BrOnArray, const Value& object, Value* value_on_branch, uint32_t br_depth, \
    bool null_succeeds)                                                        \
  F(BrOnNonStruct, const Value& object, Value* value_on_fallthrough,           \
    uint32_t br_depth, bool null_succeeds)                                     \
  F(BrOnNonI31, const Value& object, Value* value_on_fallthrough,              \
    uint32_t br_depth, bool null_succeeds)                                     \
  F(BrOnNonArray, const Value& object, Value* value_on_fallthrough,            \
    uint32_t br_depth, bool null_succeeds)                                     \
  F(StringNewWtf8, const MemoryIndexImmediate& memory,                         \
    const unibrow::Utf8Variant variant, const Value& offset,                   \
    const Value& size, Value* result)                                          \
  F(StringNewWtf8Array, const unibrow::Utf8Variant variant,                    \
    const Value& array, const Value& start, const Value& end, Value* result)   \
  F(StringNewWtf16, const MemoryIndexImmediate& memory, const Value& offset,   \
    const Value& size, Value* result)                                          \
  F(StringNewWtf16Array, const Value& array, const Value& start,               \
    const Value& end, Value* result)                                           \
  F(StringMeasureWtf8, const unibrow::Utf8Variant variant, const Value& str,   \
    Value* result)                                                             \
  F(StringMeasureWtf16, const Value& str, Value* result)                       \
  F(StringEncodeWtf8, const MemoryIndexImmediate& memory,                      \
    const unibrow::Utf8Variant variant, const Value& str,                      \
    const Value& address, Value* result)                                       \
  F(StringEncodeWtf8Array, const unibrow::Utf8Variant variant,                 \
    const Value& str, const Value& array, const Value& start, Value* result)   \
  F(StringEncodeWtf16, const MemoryIndexImmediate& memory, const Value& str,   \
    const Value& address, Value* result)                                       \
  F(StringEncodeWtf16Array, const Value& str, const Value& array,              \
    const Value& start, Value* result)                                         \
  F(StringConcat, const Value& head, const Value& tail, Value* result)         \
  F(StringEq, const Value& a, const Value& b, Value* result)                   \
  F(StringIsUSVSequence, const Value& str, Value* result)                      \
  F(StringAsWtf8, const Value& str, Value* result)                             \
  F(StringViewWtf8Advance, const Value& view, const Value& pos,                \
    const Value& bytes, Value* result)                                         \
  F(StringViewWtf8Encode, const MemoryIndexImmediate& memory,                  \
    const unibrow::Utf8Variant variant, const Value& view, const Value& addr,  \
    const Value& pos, const Value& bytes, Value* next_pos,                     \
    Value* bytes_written)                                                      \
  F(StringViewWtf8Slice, const Value& view, const Value& start,                \
    const Value& end, Value* result)                                           \
  F(StringAsWtf16, const Value& str, Value* result)                            \
  F(StringViewWtf16GetCodeUnit, const Value& view, const Value& pos,           \
    Value* result)                                                             \
  F(StringViewWtf16Encode, const MemoryIndexImmediate& memory,                 \
    const Value& view, const Value& addr, const Value& pos,                    \
    const Value& codeunits, Value* result)                                     \
  F(StringViewWtf16Slice, const Value& view, const Value& start,               \
    const Value& end, Value* result)                                           \
  F(StringAsIter, const Value& str, Value* result)                             \
  F(StringViewIterNext, const Value& view, Value* result)                      \
  F(StringViewIterAdvance, const Value& view, const Value& codepoints,         \
    Value* result)                                                             \
  F(StringViewIterRewind, const Value& view, const Value& codepoints,          \
    Value* result)                                                             \
  F(StringViewIterSlice, const Value& view, const Value& codepoints,           \
    Value* result)                                                             \
  F(StringCompare, const Value& lhs, const Value& rhs, Value* result)          \
  F(StringFromCodePoint, const Value& code_point, Value* result)               \
  F(StringHash, const Value& string, Value* result)

// This is a global constant invalid instruction trace, to be pointed at by
// the current instruction trace pointer in the default case
const std::pair<uint32_t, uint32_t> invalid_instruction_trace = {0, 0};

// A fast vector implementation, without implicit bounds checks (see
// https://crbug.com/1358853).
template <typename T>
class FastZoneVector {
 public:
  FastZoneVector() = default;
  explicit FastZoneVector(int initial_size, Zone* zone) {
    Grow(initial_size, zone);
  }

#ifdef DEBUG
  ~FastZoneVector() {
    // Check that {Reset} was called on this vector.
    DCHECK_NULL(begin_);
  }
#endif

  void Reset(Zone* zone) {
    if (begin_ == nullptr) return;
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (T* ptr = begin_; ptr != end_; ++ptr) {
        ptr->~T();
      }
    }
    zone->DeleteArray(begin_, capacity_end_ - begin_);
    begin_ = nullptr;
    end_ = nullptr;
    capacity_end_ = nullptr;
  }

  T* begin() const { return begin_; }
  T* end() const { return end_; }

  T& front() {
    DCHECK(!empty());
    return begin_[0];
  }

  T& back() {
    DCHECK(!empty());
    return end_[-1];
  }

  uint32_t size() const { return static_cast<uint32_t>(end_ - begin_); }

  bool empty() const { return begin_ == end_; }

  T& operator[](uint32_t index) {
    DCHECK_GE(size(), index);
    return begin_[index];
  }

  void shrink_to(uint32_t new_size) {
    DCHECK_GE(size(), new_size);
    end_ = begin_ + new_size;
  }

  void pop(uint32_t num = 1) {
    DCHECK_GE(size(), num);
    for (T* new_end = end_ - num; end_ != new_end;) {
      --end_;
      end_->~T();
    }
  }

  void push(T value) {
    DCHECK_GT(capacity_end_, end_);
    *end_ = std::move(value);
    ++end_;
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    DCHECK_GT(capacity_end_, end_);
    new (end_) T{std::forward<Args>(args)...};
    ++end_;
  }

  V8_INLINE void EnsureMoreCapacity(int slots_needed, Zone* zone) {
    if (V8_LIKELY(capacity_end_ - end_ >= slots_needed)) return;
    Grow(slots_needed, zone);
  }

 private:
  V8_NOINLINE V8_PRESERVE_MOST void Grow(int slots_needed, Zone* zone) {
    size_t new_capacity = std::max(
        size_t{8}, base::bits::RoundUpToPowerOfTwo(size() + slots_needed));
    CHECK_GE(kMaxUInt32, new_capacity);
    DCHECK_LT(capacity_end_ - begin_, new_capacity);
    T* new_begin = zone->template NewArray<T>(new_capacity);
    if (begin_) {
      for (T *ptr = begin_, *new_ptr = new_begin; ptr != end_;
           ++ptr, ++new_ptr) {
        new (new_ptr) T{std::move(*ptr)};
        ptr->~T();
      }
      zone->DeleteArray(begin_, capacity_end_ - begin_);
    }
    end_ = new_begin + (end_ - begin_);
    begin_ = new_begin;
    capacity_end_ = new_begin + new_capacity;
  }

  // The array is zone-allocated inside {EnsureMoreCapacity}.
  T* begin_ = nullptr;
  T* end_ = nullptr;
  T* capacity_end_ = nullptr;
};

// Generic Wasm bytecode decoder with utilities for decoding immediates,
// lengths, etc.
template <typename ValidationTag, DecodingMode decoding_mode = kFunctionBody>
class WasmDecoder : public Decoder {
  // {full_validation} implies {validate}.
  static_assert(!ValidationTag::full_validation || ValidationTag::validate);

 public:
  WasmDecoder(Zone* zone, const WasmModule* module, WasmFeatures enabled,
              WasmFeatures* detected, const FunctionSig* sig, const byte* start,
              const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, end, buffer_offset),
        zone_(zone),
        module_(module),
        enabled_(enabled),
        detected_(detected),
        sig_(sig) {
    current_inst_trace_ = &invalid_instruction_trace;
    if (V8_UNLIKELY(module_ && !module_->inst_traces.empty())) {
      auto last_trace = module_->inst_traces.end() - 1;
      auto first_inst_trace =
          std::lower_bound(module_->inst_traces.begin(), last_trace,
                           std::make_pair(buffer_offset, 0),
                           [](const std::pair<uint32_t, uint32_t>& a,
                              const std::pair<uint32_t, uint32_t>& b) {
                             return a.first < b.first;
                           });
      if (V8_UNLIKELY(first_inst_trace != last_trace)) {
        current_inst_trace_ = &*first_inst_trace;
      }
    }
  }

  Zone* zone() const { return zone_; }

  uint32_t num_locals() const { return num_locals_; }

  ValueType local_type(uint32_t index) const {
    DCHECK_GE(num_locals_, index);
    return local_types_[index];
  }

  // Decodes local definitions in the current decoder.
  // The decoded locals will be appended to {this->local_types_}.
  // The decoder's pc is not advanced.
  // The total length of decoded locals is returned.
  uint32_t DecodeLocals(const byte* pc) {
    DCHECK_NULL(local_types_);
    DCHECK_EQ(0, num_locals_);

    // In a first step, count the number of locals and store the decoded
    // entries.
    num_locals_ = static_cast<uint32_t>(this->sig_->parameter_count());

    // Decode local declarations, if any.
    auto [entries, entries_length] =
        read_u32v<ValidationTag>(pc, "local decls count");

    if (!VALIDATE(ok())) {
      DecodeError(pc, "invalid local decls count");
      return 0;
    }
    TRACE("local decls count: %u\n", entries);

    // Do an early validity check, to avoid allocating too much memory below.
    // Every entry needs at least two bytes (count plus type); if that many are
    // not available any more, flag that as an error.
    if (available_bytes() / 2 < entries) {
      DecodeError(pc, "local decls count bigger than remaining function size");
      return 0;
    }

    struct DecodedLocalEntry {
      uint32_t count;
      ValueType type;
    };
    base::SmallVector<DecodedLocalEntry, 8> decoded_locals(entries);
    uint32_t total_length = entries_length;
    for (uint32_t entry = 0; entry < entries; ++entry) {
      if (!VALIDATE(more())) {
        DecodeError(end(),
                    "expected more local decls but reached end of input");
        return 0;
      }

      auto [count, count_length] =
          read_u32v<ValidationTag>(pc + total_length, "local count");
      if (!VALIDATE(ok())) {
        DecodeError(pc + total_length, "invalid local count");
        return 0;
      }
      DCHECK_LE(num_locals_, kV8MaxWasmFunctionLocals);
      if (!VALIDATE(count <= kV8MaxWasmFunctionLocals - num_locals_)) {
        DecodeError(pc + total_length, "local count too large");
        return 0;
      }
      total_length += count_length;

      auto [type, type_length] =
          value_type_reader::read_value_type<ValidationTag>(
              this, pc + total_length, enabled_);
      ValidateValueType(pc + total_length, type);
      if (!VALIDATE(ok())) return 0;
      total_length += type_length;

      num_locals_ += count;
      decoded_locals[entry] = DecodedLocalEntry{count, type};
    }
    DCHECK(ok());

    if (num_locals_ > 0) {
      // Now build the array of local types from the parsed entries.
      local_types_ = zone_->NewArray<ValueType>(num_locals_);
      ValueType* locals_ptr = local_types_;

      if (sig_->parameter_count() > 0) {
        std::copy(sig_->parameters().begin(), sig_->parameters().end(),
                  locals_ptr);
        locals_ptr += sig_->parameter_count();
      }

      for (auto& entry : decoded_locals) {
        std::fill_n(locals_ptr, entry.count, entry.type);
        locals_ptr += entry.count;
      }
      DCHECK_EQ(locals_ptr, local_types_ + num_locals_);
    }
    return total_length;
  }

  // Shorthand that forwards to the {DecodeError} functions above, passing our
  // {ValidationTag}.
  template <typename... Args>
  V8_INLINE void DecodeError(Args... args) {
    wasm::DecodeError<ValidationTag>(this, std::forward<Args>(args)...);
  }

  // Returns a BitVector of length {locals_count + 1} representing the set of
  // variables that are assigned in the loop starting at {pc}. The additional
  // position at the end of the vector represents possible assignments to
  // the instance cache.
  static BitVector* AnalyzeLoopAssignment(WasmDecoder* decoder, const byte* pc,
                                          uint32_t locals_count, Zone* zone,
                                          bool* loop_is_innermost = nullptr) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;
    // The number of locals_count is augmented by 1 so that the 'locals_count'
    // index can be used to track the instance cache.
    BitVector* assigned = zone->New<BitVector>(locals_count + 1, zone);
    int depth = -1;  // We will increment the depth to 0 when we decode the
                     // starting 'loop' opcode.
    if (loop_is_innermost) *loop_is_innermost = true;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && VALIDATE(decoder->ok())) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      switch (opcode) {
        case kExprLoop:
          if (loop_is_innermost && depth >= 0) *loop_is_innermost = false;
          V8_FALLTHROUGH;
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          depth++;
          break;
        case kExprLocalSet:
        case kExprLocalTee: {
          IndexImmediate imm(decoder, pc + 1, "local index", validate);
          // Unverified code might have an out-of-bounds index.
          if (imm.index < locals_count) assigned->Add(imm.index);
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

  bool Validate(const byte* pc, TagIndexImmediate& imm) {
    size_t num_tags = module_->tags.size();
    if (!VALIDATE(imm.index < num_tags)) {
      DecodeError(pc, "Invalid tag index: %u", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_tags);
    imm.tag = &module_->tags[imm.index];
    return true;
  }

  bool Validate(const byte* pc, GlobalIndexImmediate& imm) {
    // We compare with the current size of the globals vector. This is important
    // if we are decoding a constant expression in the global section.
    size_t num_globals = module_->globals.size();
    if (!VALIDATE(imm.index < num_globals)) {
      DecodeError(pc, "Invalid global index: %u", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_globals);
    imm.global = &module_->globals[imm.index];

    if constexpr (decoding_mode == kConstantExpression) {
      if (!VALIDATE(!imm.global->mutability)) {
        this->DecodeError(pc,
                          "mutable globals cannot be used in constant "
                          "expressions");
        return false;
      }
      if (!VALIDATE(imm.global->imported || this->enabled_.has_gc())) {
        this->DecodeError(
            pc, "non-imported globals cannot be used in constant expressions");
        return false;
      }
    }

    return true;
  }

  bool Validate(const byte* pc, SigIndexImmediate& imm) {
    if (!VALIDATE(module_->has_signature(imm.index))) {
      DecodeError(pc, "invalid signature index: %u", imm.index);
      return false;
    }
    imm.sig = module_->signature(imm.index);
    return true;
  }

  bool Validate(const byte* pc, StructIndexImmediate& imm) {
    if (!VALIDATE(module_->has_struct(imm.index))) {
      DecodeError(pc, "invalid struct index: %u", imm.index);
      return false;
    }
    imm.struct_type = module_->struct_type(imm.index);
    return true;
  }

  bool Validate(const byte* pc, FieldImmediate& imm) {
    if (!Validate(pc, imm.struct_imm)) return false;
    if (!VALIDATE(imm.field_imm.index <
                  imm.struct_imm.struct_type->field_count())) {
      DecodeError(pc + imm.struct_imm.length, "invalid field index: %u",
                  imm.field_imm.index);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, ArrayIndexImmediate& imm) {
    if (!VALIDATE(module_->has_array(imm.index))) {
      DecodeError(pc, "invalid array index: %u", imm.index);
      return false;
    }
    imm.array_type = module_->array_type(imm.index);
    return true;
  }

  bool CanReturnCall(const FunctionSig* target_sig) {
    if (sig_->return_count() != target_sig->return_count()) return false;
    auto target_sig_it = target_sig->returns().begin();
    for (ValueType ret_type : sig_->returns()) {
      if (!IsSubtypeOf(*target_sig_it++, ret_type, this->module_)) return false;
    }
    return true;
  }

  bool Validate(const byte* pc, CallFunctionImmediate& imm) {
    size_t num_functions = module_->functions.size();
    if (!VALIDATE(imm.index < num_functions)) {
      DecodeError(pc, "function index #%u is out of bounds", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_functions);
    imm.sig = module_->functions[imm.index].sig;
    return true;
  }

  bool Validate(const byte* pc, CallIndirectImmediate& imm) {
    if (!ValidateSignature(pc, imm.sig_imm)) return false;
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
    ValueType immediate_type = ValueType::Ref(imm.sig_imm.index);
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

  bool Validate(const byte* pc, BranchDepthImmediate& imm,
                size_t control_depth) {
    if (!VALIDATE(imm.depth < control_depth)) {
      DecodeError(pc, "invalid branch depth: %u", imm.depth);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BranchTableImmediate& imm, size_t block_depth) {
    if (!VALIDATE(imm.table_count <= kV8MaxWasmFunctionBrTableSize)) {
      DecodeError(pc, "invalid table count (> max br_table size): %u",
                  imm.table_count);
      return false;
    }
    return checkAvailable(imm.table_count);
  }

  bool Validate(const byte* pc, WasmOpcode opcode, SimdLaneImmediate& imm) {
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

  bool Validate(const byte* pc, Simd128Immediate& imm) {
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

  bool Validate(const byte* pc, BlockTypeImmediate& imm) {
    if (imm.sig.all().begin() == nullptr) {
      // Then use {sig_index} to initialize the signature.
      if (!VALIDATE(module_->has_signature(imm.sig_index))) {
        DecodeError(pc, "block type index %u is not a signature definition",
                    imm.sig_index);
        return false;
      }
      imm.sig = *module_->signature(imm.sig_index);
    } else {
      // Then it's an MVP immediate with 0 parameters and 0-1 returns.
      DCHECK_EQ(0, imm.sig.parameter_count());
      DCHECK_GE(1, imm.sig.return_count());
      if (imm.sig.return_count()) {
        if (!ValidateValueType(pc, imm.sig.GetReturn(0))) return false;
      }
    }
    return true;
  }

  bool Validate(const byte* pc, MemoryIndexImmediate& imm) {
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

  bool Validate(const byte* pc, MemoryAccessImmediate& imm) {
    if (!VALIDATE(this->module_->has_memory)) {
      this->DecodeError(pc, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, MemoryInitImmediate& imm) {
    return ValidateDataSegment(pc, imm.data_segment) &&
           Validate(pc + imm.data_segment.length, imm.memory);
  }

  bool Validate(const byte* pc, MemoryCopyImmediate& imm) {
    return Validate(pc, imm.memory_src) &&
           Validate(pc + imm.memory_src.length, imm.memory_dst);
  }

  bool Validate(const byte* pc, TableInitImmediate& imm) {
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

  bool Validate(const byte* pc, TableCopyImmediate& imm) {
    if (!ValidateTable(pc, imm.table_src)) return false;
    if (!ValidateTable(pc + imm.table_src.length, imm.table_dst)) return false;
    size_t num_tables = module_->tables.size();
    V8_ASSUME(imm.table_src.index < num_tables);
    V8_ASSUME(imm.table_dst.index < num_tables);
    ValueType src_type = module_->tables[imm.table_src.index].type;
    if (!VALIDATE(IsSubtypeOf(
            src_type, module_->tables[imm.table_dst.index].type, module_))) {
      DecodeError(pc, "table %u is not a super-type of %s", imm.table_dst.index,
                  src_type.name().c_str());
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, StringConstImmediate& imm) {
    if (!VALIDATE(imm.index < module_->stringref_literals.size())) {
      DecodeError(pc, "Invalid string literal index: %u", imm.index);
      return false;
    }
    return true;
  }

  // The following Validate* functions all validate an IndexImmediate, albeit
  // differently according to context.
  bool ValidateTable(const byte* pc, IndexImmediate& imm) {
    if (imm.index > 0 || imm.length > 1) {
      this->detected_->Add(kFeature_reftypes);
    }
    size_t num_tables = module_->tables.size();
    if (!VALIDATE(imm.index < num_tables)) {
      DecodeError(pc, "invalid table index: %u", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_tables);
    return true;
  }

  bool ValidateElementSegment(const byte* pc, IndexImmediate& imm) {
    size_t num_elem_segments = module_->elem_segments.size();
    if (!VALIDATE(imm.index < num_elem_segments)) {
      DecodeError(pc, "invalid element segment index: %u", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_elem_segments);
    return true;
  }

  bool ValidateLocal(const byte* pc, IndexImmediate& imm) {
    if (!VALIDATE(imm.index < num_locals())) {
      DecodeError(pc, "invalid local index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateType(const byte* pc, IndexImmediate& imm) {
    if (!VALIDATE(module_->has_type(imm.index))) {
      DecodeError(pc, "invalid type index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateSignature(const byte* pc, IndexImmediate& imm) {
    if (!VALIDATE(module_->has_signature(imm.index))) {
      DecodeError(pc, "invalid signature index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateFunction(const byte* pc, IndexImmediate& imm) {
    size_t num_functions = module_->functions.size();
    if (!VALIDATE(imm.index < num_functions)) {
      DecodeError(pc, "function index #%u is out of bounds", imm.index);
      return false;
    }
    V8_ASSUME(imm.index < num_functions);
    if (decoding_mode == kFunctionBody &&
        !VALIDATE(module_->functions[imm.index].declared)) {
      DecodeError(pc, "undeclared reference to function #%u", imm.index);
      return false;
    }
    return true;
  }

  bool ValidateDataSegment(const byte* pc, IndexImmediate& imm) {
    if (!VALIDATE(imm.index < module_->num_declared_data_segments)) {
      DecodeError(pc, "invalid data segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, SelectTypeImmediate& imm) {
    return ValidateValueType(pc, imm.type);
  }

  bool Validate(const byte* pc, HeapTypeImmediate& imm) {
    return ValidateHeapType(pc, imm.type);
  }

  bool ValidateValueType(const byte* pc, ValueType type) {
    return value_type_reader::ValidateValueType<ValidationTag>(this, pc,
                                                               module_, type);
  }

  bool ValidateHeapType(const byte* pc, HeapType type) {
    return value_type_reader::ValidateHeapType<ValidationTag>(this, pc, module_,
                                                              type);
  }

  // Returns the length of the opcode under {pc}.
  template <typename... ImmediateObservers>
  static uint32_t OpcodeLength(WasmDecoder* decoder, const byte* pc,
                               ImmediateObservers&... ios) {
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
        BlockTypeImmediate imm(WasmFeatures::All(), decoder, pc + 1, validate);
        (ios.BlockType(imm), ...);
        return 1 + imm.length;
      }
      case kExprRethrow:
      case kExprBr:
      case kExprBrIf:
      case kExprBrOnNull:
      case kExprBrOnNonNull:
      case kExprDelegate: {
        BranchDepthImmediate imm(decoder, pc + 1, validate);
        (ios.BranchDepth(imm), ...);
        return 1 + imm.length;
      }
      case kExprBrTable: {
        BranchTableImmediate imm(decoder, pc + 1, validate);
        (ios.BranchTable(imm), ...);
        BranchTableIterator<ValidationTag> iterator(decoder, imm);
        return 1 + iterator.length();
      }
      case kExprThrow:
      case kExprCatch: {
        TagIndexImmediate imm(decoder, pc + 1, validate);
        (ios.TagIndex(imm), ...);
        return 1 + imm.length;
      }

      /********** Misc opcodes **********/
      case kExprCallFunction:
      case kExprReturnCall: {
        CallFunctionImmediate imm(decoder, pc + 1, validate);
        (ios.FunctionIndex(imm), ...);
        return 1 + imm.length;
      }
      case kExprCallIndirect:
      case kExprReturnCallIndirect: {
        CallIndirectImmediate imm(decoder, pc + 1, validate);
        (ios.CallIndirect(imm), ...);
        return 1 + imm.length;
      }
      case kExprCallRef:
      case kExprReturnCallRef: {
        SigIndexImmediate imm(decoder, pc + 1, validate);
        (ios.TypeIndex(imm), ...);
        return 1 + imm.length;
      }
      case kExprDrop:
      case kExprSelect:
      case kExprCatchAll:
        return 1;
      case kExprSelectWithType: {
        SelectTypeImmediate imm(WasmFeatures::All(), decoder, pc + 1, validate);
        (ios.SelectType(imm), ...);
        return 1 + imm.length;
      }

      case kExprLocalGet:
      case kExprLocalSet:
      case kExprLocalTee: {
        IndexImmediate imm(decoder, pc + 1, "local index", validate);
        (ios.LocalIndex(imm), ...);
        return 1 + imm.length;
      }
      case kExprGlobalGet:
      case kExprGlobalSet: {
        GlobalIndexImmediate imm(decoder, pc + 1, validate);
        (ios.GlobalIndex(imm), ...);
        return 1 + imm.length;
      }
      case kExprTableGet:
      case kExprTableSet: {
        IndexImmediate imm(decoder, pc + 1, "table index", validate);
        (ios.TableIndex(imm), ...);
        return 1 + imm.length;
      }
      case kExprI32Const: {
        ImmI32Immediate imm(decoder, pc + 1, validate);
        (ios.I32Const(imm), ...);
        return 1 + imm.length;
      }
      case kExprI64Const: {
        ImmI64Immediate imm(decoder, pc + 1, validate);
        (ios.I64Const(imm), ...);
        return 1 + imm.length;
      }
      case kExprF32Const:
        if (sizeof...(ios) > 0) {
          ImmF32Immediate imm(decoder, pc + 1, validate);
          (ios.F32Const(imm), ...);
        }
        return 5;
      case kExprF64Const:
        if (sizeof...(ios) > 0) {
          ImmF64Immediate imm(decoder, pc + 1, validate);
          (ios.F64Const(imm), ...);
        }
        return 9;
      case kExprRefNull: {
        HeapTypeImmediate imm(WasmFeatures::All(), decoder, pc + 1, validate);
        (ios.HeapType(imm), ...);
        return 1 + imm.length;
      }
      case kExprRefIsNull:
      case kExprRefAsNonNull:
        return 1;
      case kExprRefFunc: {
        IndexImmediate imm(decoder, pc + 1, "function index", validate);
        (ios.FunctionIndex(imm), ...);
        return 1 + imm.length;
      }

#define DECLARE_OPCODE_CASE(name, ...) case kExpr##name:
        // clang-format off
      /********** Simple and memory opcodes **********/
      FOREACH_SIMPLE_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_SIMPLE_PROTOTYPE_OPCODE(DECLARE_OPCODE_CASE)
        return 1;
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE) {
        MemoryAccessImmediate imm(decoder, pc + 1, UINT32_MAX,
                                  kConservativelyAssumeMemory64, validate);
        (ios.MemoryAccess(imm), ...);
        return 1 + imm.length;
      }
      // clang-format on
      case kExprMemoryGrow:
      case kExprMemorySize: {
        MemoryIndexImmediate imm(decoder, pc + 1, validate);
        (ios.MemoryIndex(imm), ...);
        return 1 + imm.length;
      }

      /********** Prefixed opcodes **********/
      case kNumericPrefix: {
        uint32_t length;
        std::tie(opcode, length) =
            decoder->read_prefixed_opcode<ValidationTag>(pc);
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
            MemoryInitImmediate imm(decoder, pc + length, validate);
            (ios.MemoryInit(imm), ...);
            return length + imm.length;
          }
          case kExprDataDrop: {
            IndexImmediate imm(decoder, pc + length, "data segment index",
                               validate);
            (ios.DataSegmentIndex(imm), ...);
            return length + imm.length;
          }
          case kExprMemoryCopy: {
            MemoryCopyImmediate imm(decoder, pc + length, validate);
            (ios.MemoryCopy(imm), ...);
            return length + imm.length;
          }
          case kExprMemoryFill: {
            MemoryIndexImmediate imm(decoder, pc + length, validate);
            (ios.MemoryIndex(imm), ...);
            return length + imm.length;
          }
          case kExprTableInit: {
            TableInitImmediate imm(decoder, pc + length, validate);
            (ios.TableInit(imm), ...);
            return length + imm.length;
          }
          case kExprElemDrop: {
            IndexImmediate imm(decoder, pc + length, "element segment index",
                               validate);
            (ios.ElemSegmentIndex(imm), ...);
            return length + imm.length;
          }
          case kExprTableCopy: {
            TableCopyImmediate imm(decoder, pc + length, validate);
            (ios.TableCopy(imm), ...);
            return length + imm.length;
          }
          case kExprTableGrow:
          case kExprTableSize:
          case kExprTableFill: {
            IndexImmediate imm(decoder, pc + length, "table index", validate);
            (ios.TableIndex(imm), ...);
            return length + imm.length;
          }
          default:
            // This path is only possible if we are validating.
            V8_ASSUME(ValidationTag::validate);
            decoder->DecodeError(pc, "invalid numeric opcode");
            return length;
        }
      }
      case kSimdPrefix: {
        uint32_t length;
        std::tie(opcode, length) =
            decoder->read_prefixed_opcode<ValidationTag>(pc);
        switch (opcode) {
          // clang-format off
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return length;
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
        if (sizeof...(ios) > 0) {
              SimdLaneImmediate lane_imm(decoder, pc + length, validate);
             (ios.SimdLane(lane_imm), ...);
            }
            return length + 1;
          FOREACH_SIMD_MEM_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate imm(decoder, pc + length, UINT32_MAX,
                                      kConservativelyAssumeMemory64, validate);
            (ios.MemoryAccess(imm), ...);
            return length + imm.length;
          }
          FOREACH_SIMD_MEM_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate imm(
                decoder, pc + length, UINT32_MAX,
                kConservativelyAssumeMemory64, validate);
        if (sizeof...(ios) > 0) {
              SimdLaneImmediate lane_imm(decoder,
                                         pc + length + imm.length, validate);
             (ios.MemoryAccess(imm), ...);
             (ios.SimdLane(lane_imm), ...);
            }
            // 1 more byte for lane index immediate.
            return length + imm.length + 1;
          }
          // clang-format on
          // Shuffles require a byte per lane, or 16 immediate bytes.
          case kExprS128Const:
          case kExprI8x16Shuffle:
            if (sizeof...(ios) > 0) {
              Simd128Immediate imm(decoder, pc + length, validate);
              (ios.S128Const(imm), ...);
            }
            return length + kSimd128Size;
          default:
            // This path is only possible if we are validating.
            V8_ASSUME(ValidationTag::validate);
            decoder->DecodeError(pc, "invalid SIMD opcode");
            return length;
        }
      }
      case kAtomicPrefix: {
        uint32_t length;
        std::tie(opcode, length) =
            decoder->read_prefixed_opcode<ValidationTag>(pc, "atomic_index");
        switch (opcode) {
          FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE) {
            MemoryAccessImmediate imm(decoder, pc + length, UINT32_MAX,
                                      kConservativelyAssumeMemory64, validate);
            (ios.MemoryAccess(imm), ...);
            return length + imm.length;
          }
          FOREACH_ATOMIC_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE) {
            // One unused zero-byte.
            return length + 1;
          }
          default:
            // This path is only possible if we are validating.
            V8_ASSUME(ValidationTag::validate);
            decoder->DecodeError(pc, "invalid Atomics opcode");
            return length;
        }
      }
      case kGCPrefix: {
        uint32_t length;
        std::tie(opcode, length) =
            decoder->read_prefixed_opcode<ValidationTag>(pc, "gc_index");
        switch (opcode) {
          case kExprStructNew:
          case kExprStructNewDefault: {
            StructIndexImmediate imm(decoder, pc + length, validate);
            (ios.TypeIndex(imm), ...);
            return length + imm.length;
          }
          case kExprStructGet:
          case kExprStructGetS:
          case kExprStructGetU:
          case kExprStructSet: {
            FieldImmediate imm(decoder, pc + length, validate);
            (ios.Field(imm), ...);
            return length + imm.length;
          }
          case kExprArrayNew:
          case kExprArrayNewDefault:
          case kExprArrayGet:
          case kExprArrayGetS:
          case kExprArrayGetU:
          case kExprArraySet: {
            ArrayIndexImmediate imm(decoder, pc + length, validate);
            (ios.TypeIndex(imm), ...);
            return length + imm.length;
          }
          case kExprArrayNewFixed: {
            ArrayIndexImmediate array_imm(decoder, pc + length, validate);
            IndexImmediate length_imm(decoder, pc + length + array_imm.length,
                                      "array length", validate);
            (ios.TypeIndex(array_imm), ...);
            (ios.Length(length_imm), ...);
            return length + array_imm.length + length_imm.length;
          }
          case kExprArrayCopy: {
            ArrayIndexImmediate dst_imm(decoder, pc + length, validate);
            ArrayIndexImmediate src_imm(decoder, pc + length + dst_imm.length,
                                        validate);
            (ios.ArrayCopy(dst_imm, src_imm), ...);
            return length + dst_imm.length + src_imm.length;
          }
          case kExprArrayFill: {
            ArrayIndexImmediate imm(decoder, pc + length, validate);
            (ios.TypeIndex(imm), ...);
            return length + imm.length;
          }
          case kExprArrayNewData:
          case kExprArrayNewElem: {
            ArrayIndexImmediate array_imm(decoder, pc + length, validate);
            IndexImmediate data_imm(decoder, pc + length + array_imm.length,
                                    "segment index", validate);
            (ios.TypeIndex(array_imm), ...);
            (ios.DataSegmentIndex(data_imm), ...);
            return length + array_imm.length + data_imm.length;
          }
          case kExprBrOnArray:
          case kExprBrOnStruct:
          case kExprBrOnI31:
          case kExprBrOnNonArray:
          case kExprBrOnNonStruct:
          case kExprBrOnNonI31: {
            BranchDepthImmediate imm(decoder, pc + length, validate);
            (ios.BranchDepth(imm), ...);
            return length + imm.length;
          }
          case kExprRefCast:
          case kExprRefCastNull:
          case kExprRefTest:
          case kExprRefTestNull: {
            HeapTypeImmediate imm(WasmFeatures::All(), decoder, pc + length,
                                  validate);
            (ios.HeapType(imm), ...);
            return length + imm.length;
          }
          case kExprRefTestDeprecated:
          case kExprRefCastDeprecated:
          case kExprRefCastNop: {
            IndexImmediate imm(decoder, pc + length, "type index", validate);
            (ios.TypeIndex(imm), ...);
            return length + imm.length;
          }
          case kExprBrOnCast:
          case kExprBrOnCastNull:
          case kExprBrOnCastFail:
          case kExprBrOnCastFailNull: {
            BranchDepthImmediate branch(decoder, pc + length, validate);
            HeapTypeImmediate imm(WasmFeatures::All(), decoder,
                                  pc + length + branch.length, validate);
            (ios.BranchDepth(branch), ...);
            (ios.HeapType(imm), ...);
            return length + branch.length + imm.length;
          }
          case kExprBrOnCastFailDeprecated:
          case kExprBrOnCastDeprecated: {
            BranchDepthImmediate branch(decoder, pc + length, validate);
            IndexImmediate index(decoder, pc + length + branch.length,
                                 "type index", validate);
            (ios.BranchDepth(branch), ...);
            (ios.TypeIndex(index), ...);
            return length + branch.length + index.length;
          }
          case kExprI31New:
          case kExprI31GetS:
          case kExprI31GetU:
          case kExprRefAsArray:
          case kExprRefAsStruct:
          case kExprRefAsI31:
          case kExprRefIsArray:
          case kExprRefIsStruct:
          case kExprRefIsI31:
          case kExprExternInternalize:
          case kExprExternExternalize:
          case kExprArrayLen:
            return length;
          case kExprStringNewUtf8:
          case kExprStringNewUtf8Try:
          case kExprStringNewLossyUtf8:
          case kExprStringNewWtf8:
          case kExprStringEncodeUtf8:
          case kExprStringEncodeLossyUtf8:
          case kExprStringEncodeWtf8:
          case kExprStringViewWtf8EncodeUtf8:
          case kExprStringViewWtf8EncodeLossyUtf8:
          case kExprStringViewWtf8EncodeWtf8:
          case kExprStringNewWtf16:
          case kExprStringEncodeWtf16:
          case kExprStringViewWtf16Encode: {
            MemoryIndexImmediate imm(decoder, pc + length, validate);
            (ios.MemoryIndex(imm), ...);
            return length + imm.length;
          }
          case kExprStringConst: {
            StringConstImmediate imm(decoder, pc + length, validate);
            (ios.StringConst(imm), ...);
            return length + imm.length;
          }
          case kExprStringMeasureUtf8:
          case kExprStringMeasureWtf8:
          case kExprStringNewUtf8Array:
          case kExprStringNewUtf8ArrayTry:
          case kExprStringNewLossyUtf8Array:
          case kExprStringNewWtf8Array:
          case kExprStringEncodeUtf8Array:
          case kExprStringEncodeLossyUtf8Array:
          case kExprStringEncodeWtf8Array:
          case kExprStringMeasureWtf16:
          case kExprStringConcat:
          case kExprStringEq:
          case kExprStringIsUSVSequence:
          case kExprStringAsWtf8:
          case kExprStringViewWtf8Advance:
          case kExprStringViewWtf8Slice:
          case kExprStringAsWtf16:
          case kExprStringViewWtf16Length:
          case kExprStringViewWtf16GetCodeUnit:
          case kExprStringViewWtf16Slice:
          case kExprStringAsIter:
          case kExprStringViewIterNext:
          case kExprStringViewIterAdvance:
          case kExprStringViewIterRewind:
          case kExprStringViewIterSlice:
          case kExprStringNewWtf16Array:
          case kExprStringEncodeWtf16Array:
          case kExprStringCompare:
          case kExprStringFromCodePoint:
          case kExprStringHash:
            return length;
          default:
            // This path is only possible if we are validating.
            V8_ASSUME(ValidationTag::validate);
            decoder->DecodeError(pc, "invalid gc opcode");
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
    if (ValidationTag::validate) {
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

#define DECLARE_OPCODE_CASE(name, ...) case kExpr##name:
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
        CallFunctionImmediate imm(this, pc + 1, validate);
        CHECK(Validate(pc + 1, imm));
        return {imm.sig->parameter_count(), imm.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectImmediate imm(this, pc + 1, validate);
        CHECK(Validate(pc + 1, imm));
        // Indirect calls pop an additional argument for the table index.
        return {imm.sig->parameter_count() + 1,
                imm.sig->return_count()};
      }
      case kExprThrow: {
        TagIndexImmediate imm(this, pc + 1, validate);
        CHECK(Validate(pc + 1, imm));
        DCHECK_EQ(0, imm.tag->sig->return_count());
        return {imm.tag->sig->parameter_count(), 0};
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
      case kNumericPrefix:
      case kAtomicPrefix:
      case kSimdPrefix: {
        opcode = this->read_prefixed_opcode<ValidationTag>(pc).first;
        switch (opcode) {
          FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(DECLARE_OPCODE_CASE)
            return {1, 1};
          FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(DECLARE_OPCODE_CASE)
          FOREACH_SIMD_MASK_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return {2, 1};
          FOREACH_SIMD_CONST_OPCODE(DECLARE_OPCODE_CASE)
            return {0, 1};
          // Special case numeric opcodes without fixed signature.
          case kExprMemoryInit:
          case kExprMemoryCopy:
          case kExprMemoryFill:
            return {3, 0};
          case kExprTableGrow:
            return {2, 1};
          case kExprTableFill:
            return {3, 0};
          default: {
            sig = WasmOpcodes::Signature(opcode);
            DCHECK_NOT_NULL(sig);
            return {sig->parameter_count(), sig->return_count()};
          }
        }
      }
      case kGCPrefix: {
        opcode = this->read_prefixed_opcode<ValidationTag>(pc).first;
        switch (opcode) {
          case kExprStructGet:
          case kExprStructGetS:
          case kExprStructGetU:
          case kExprI31New:
          case kExprI31GetS:
          case kExprI31GetU:
          case kExprArrayNewDefault:
          case kExprArrayLen:
          case kExprRefTest:
          case kExprRefTestNull:
          case kExprRefTestDeprecated:
          case kExprRefCast:
          case kExprRefCastNull:
          case kExprRefCastDeprecated:
          case kExprRefCastNop:
          case kExprBrOnCast:
          case kExprBrOnCastNull:
          case kExprBrOnCastFail:
          case kExprBrOnCastFailNull:
          case kExprBrOnCastFailDeprecated:
          case kExprBrOnCastDeprecated:
            return {1, 1};
          case kExprStructSet:
            return {2, 0};
          case kExprArrayNew:
          case kExprArrayNewData:
          case kExprArrayNewElem:
          case kExprArrayGet:
          case kExprArrayGetS:
          case kExprArrayGetU:
            return {2, 1};
          case kExprArraySet:
            return {3, 0};
          case kExprArrayCopy:
            return {5, 0};
          case kExprArrayFill:
            return {4, 0};
          case kExprStructNewDefault:
            return {0, 1};
          case kExprStructNew: {
            StructIndexImmediate imm(this, pc + 2, validate);
            CHECK(Validate(pc + 2, imm));
            return {imm.struct_type->field_count(), 1};
          }
          case kExprArrayNewFixed: {
            ArrayIndexImmediate array_imm(this, pc + 2, validate);
            IndexImmediate length_imm(this, pc + 2 + array_imm.length,
                                                "array length", validate);
            return {length_imm.index, 1};
          }
          case kExprStringConst:
            return { 0, 1 };
          case kExprStringMeasureUtf8:
          case kExprStringMeasureWtf8:
          case kExprStringMeasureWtf16:
          case kExprStringIsUSVSequence:
          case kExprStringAsWtf8:
          case kExprStringAsWtf16:
          case kExprStringAsIter:
          case kExprStringViewWtf16Length:
          case kExprStringViewIterNext:
          case kExprStringFromCodePoint:
          case kExprStringHash:
            return { 1, 1 };
          case kExprStringNewUtf8:
          case kExprStringNewUtf8Try:
          case kExprStringNewLossyUtf8:
          case kExprStringNewWtf8:
          case kExprStringNewWtf16:
          case kExprStringConcat:
          case kExprStringEq:
          case kExprStringViewWtf16GetCodeUnit:
          case kExprStringViewIterAdvance:
          case kExprStringViewIterRewind:
          case kExprStringViewIterSlice:
          case kExprStringCompare:
            return { 2, 1 };
          case kExprStringNewUtf8Array:
          case kExprStringNewUtf8ArrayTry:
          case kExprStringNewLossyUtf8Array:
          case kExprStringNewWtf8Array:
          case kExprStringNewWtf16Array:
          case kExprStringEncodeUtf8:
          case kExprStringEncodeLossyUtf8:
          case kExprStringEncodeWtf8:
          case kExprStringEncodeUtf8Array:
          case kExprStringEncodeLossyUtf8Array:
          case kExprStringEncodeWtf8Array:
          case kExprStringEncodeWtf16:
          case kExprStringEncodeWtf16Array:
          case kExprStringViewWtf8Advance:
          case kExprStringViewWtf8Slice:
          case kExprStringViewWtf16Slice:
            return { 3, 1 };
          case kExprStringViewWtf16Encode:
            return { 4, 1 };
          case kExprStringViewWtf8EncodeUtf8:
          case kExprStringViewWtf8EncodeLossyUtf8:
          case kExprStringViewWtf8EncodeWtf8:
            return { 4, 2 };
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

  static constexpr ValidationTag validate = {};

  Zone* const zone_;

  ValueType* local_types_ = nullptr;
  uint32_t num_locals_ = 0;

  const WasmModule* module_;
  const WasmFeatures enabled_;
  WasmFeatures* detected_;
  const FunctionSig* sig_;
  const std::pair<uint32_t, uint32_t>* current_inst_trace_;
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

template <typename ValidationTag, typename Interface,
          DecodingMode decoding_mode = kFunctionBody>
class WasmFullDecoder : public WasmDecoder<ValidationTag, decoding_mode> {
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
      : WasmDecoder<ValidationTag, decoding_mode>(
            zone, module, enabled, detected, body.sig, body.start, body.end,
            body.offset),
        interface_(std::forward<InterfaceArgs>(interface_args)...),
        stack_(16, zone),
        control_(16, zone) {}

  ~WasmFullDecoder() {
    control_.Reset(this->zone_);
    stack_.Reset(this->zone_);
    locals_initializers_stack_.Reset(this->zone_);
  }

  Interface& interface() { return interface_; }

  void Decode() {
    DCHECK(stack_.empty());
    DCHECK(control_.empty());
    DCHECK_LE(this->pc_, this->end_);
    DCHECK_EQ(this->num_locals(), 0);

    locals_offset_ = this->pc_offset();
    uint32_t locals_length = this->DecodeLocals(this->pc());
    if (!VALIDATE(this->ok())) return TraceFailed();
    this->consume_bytes(locals_length);
    int non_defaultable = 0;
    uint32_t params_count =
        static_cast<uint32_t>(this->sig_->parameter_count());
    for (uint32_t index = params_count; index < this->num_locals(); index++) {
      if (!this->local_type(index).is_defaultable()) non_defaultable++;
    }
    this->InitializeInitializedLocalsTracking(non_defaultable);

    // Cannot use CALL_INTERFACE_* macros because control is empty.
    interface().StartFunction(this);
    DecodeFunctionBody();
    // Decoding can fail even without validation, e.g. due to missing Liftoff
    // support.
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

    DCHECK(stack_.empty());
    TRACE("wasm-decode ok\n\n");
  }

  void TraceFailed() {
    if (this->error_.offset()) {
      TRACE("wasm-error module+%-6d func+%d: %s\n\n", this->error_.offset(),
            this->GetBufferRelativeOffset(this->error_.offset()),
            this->error_.message().c_str());
    } else {
      TRACE("wasm-error: %s\n\n", this->error_.message().c_str());
    }
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (!pc) return "<null>";
    if (pc >= this->end_) return "<end>";
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
      return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(opcode));
    }
    opcode = this->template read_prefixed_opcode<Decoder::FullValidationTag>(pc)
                 .first;
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
    return control_.end() - 1 - depth;
  }

  uint32_t stack_size() const { return stack_.size(); }

  Value* stack_value(uint32_t depth) const {
    DCHECK_LT(0, depth);
    DCHECK_GE(stack_.size(), depth);
    return stack_.end() - depth;
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
    return this->pc_offset() - locals_offset_;
  }

  bool is_local_initialized(uint32_t local_index) {
    DCHECK_GT(this->num_locals_, local_index);
    if (!has_nondefaultable_locals_) return true;
    return initialized_locals_[local_index];
  }

  void set_local_initialized(uint32_t local_index) {
    DCHECK_GT(this->num_locals_, local_index);
    if (!has_nondefaultable_locals_) return;
    // This implicitly covers defaultable locals too (which are always
    // initialized).
    if (is_local_initialized(local_index)) return;
    initialized_locals_[local_index] = true;
    locals_initializers_stack_.push(local_index);
  }

  uint32_t locals_initialization_stack_depth() const {
    return static_cast<uint32_t>(locals_initializers_stack_.size());
  }

  void RollbackLocalsInitialization(Control* c) {
    if (!has_nondefaultable_locals_) return;
    uint32_t previous_stack_height = c->init_stack_depth;
    while (locals_initializers_stack_.size() > previous_stack_height) {
      uint32_t local_index = locals_initializers_stack_.back();
      locals_initializers_stack_.pop();
      initialized_locals_[local_index] = false;
    }
  }

  void InitializeInitializedLocalsTracking(int non_defaultable_locals) {
    has_nondefaultable_locals_ = non_defaultable_locals > 0;
    if (!has_nondefaultable_locals_) return;
    initialized_locals_ =
        this->zone_->template NewArray<bool>(this->num_locals_);
    // Parameters are always initialized.
    const size_t num_params = this->sig_->parameter_count();
    std::fill_n(initialized_locals_, num_params, true);
    // Locals are initialized if they are defaultable.
    for (size_t i = num_params; i < this->num_locals_; i++) {
      initialized_locals_[i] = this->local_types_[i].is_defaultable();
    }
    DCHECK(locals_initializers_stack_.empty());
    locals_initializers_stack_.EnsureMoreCapacity(non_defaultable_locals,
                                                  this->zone_);
  }

  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n", this->start(),
          this->end(), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      DCHECK(control_.empty());
      constexpr uint32_t kStackDepth = 0;
      constexpr uint32_t kInitStackDepth = 0;
      control_.EnsureMoreCapacity(1, this->zone_);
      control_.emplace_back(this->zone_, kControlBlock, kStackDepth,
                            kInitStackDepth, this->pc_, kReachable);
      Control* c = &control_.back();
      if constexpr (decoding_mode == kFunctionBody) {
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

    if (V8_LIKELY(this->current_inst_trace_->first == 0)) {
      // Decode the function body.
      while (this->pc_ < this->end_) {
        // Most operations only grow the stack by at least one element (unary
        // and binary operations, local.get, constants, ...). Thus check that
        // there is enough space for those operations centrally, and avoid any
        // bounds checks in those operations.
        stack_.EnsureMoreCapacity(1, this->zone_);
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

    } else {
      // Decode the function body.
      while (this->pc_ < this->end_) {
        DCHECK(this->current_inst_trace_->first == 0 ||
               this->current_inst_trace_->first >= this->pc_offset());
        if (V8_UNLIKELY(this->current_inst_trace_->first ==
                        this->pc_offset())) {
          TRACE("Emit trace at 0x%x with ID[0x%x]\n", this->pc_offset(),
                this->current_inst_trace_->second);
          CALL_INTERFACE_IF_OK_AND_REACHABLE(TraceInstruction,
                                             this->current_inst_trace_->second);
          this->current_inst_trace_++;
        }

        // Most operations only grow the stack by at least one element (unary
        // and binary operations, local.get, constants, ...). Thus check that
        // there is enough space for those operations centrally, and avoid any
        // bounds checks in those operations.
        stack_.EnsureMoreCapacity(1, this->zone_);
        uint8_t first_byte = *this->pc_;
        WasmOpcode opcode = static_cast<WasmOpcode>(first_byte);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(NextInstruction, opcode);
        OpcodeHandler handler = GetOpcodeHandler(first_byte);
        int len = (*handler)(this, opcode);
        this->pc_ += len;
      }
    }

    // Even without validation, compilation could fail because of unsupported
    // Liftoff operations. In that case, {pc_} did not necessarily advance until
    // {end_}. Thus do not wrap the next check in {VALIDATE}.
    if (this->pc_ != this->end_) {
      this->DecodeError("Beyond end of code");
    }
  }

 private:
  uint32_t locals_offset_ = 0;
  Interface interface_;

  // The value stack, stored as individual pointers for maximum performance.
  FastZoneVector<Value> stack_;

  // Indicates whether the local with the given index is currently initialized.
  // Entries for defaultable locals are meaningless; we have a byte for each
  // local because we expect that the effort required to densify this bit
  // vector would more than offset the memory savings.
  bool* initialized_locals_;
  // Keeps track of initializing assignments to non-defaultable locals that
  // happened, so they can be discarded at the end of the current block.
  // Contains no duplicates, so the size of this stack is bounded (and pre-
  // allocated) to the number of non-defaultable locals in the function.
  FastZoneVector<uint32_t> locals_initializers_stack_;

  // Control stack (blocks, loops, ifs, ...).
  FastZoneVector<Control> control_;

  // Controls whether code should be generated for the current block (basically
  // a cache for {ok() && control_.back().reachable()}).
  bool current_code_reachable_and_ok_ = true;

  // Performance optimization: bail out of any functions dealing with non-
  // defaultable locals early when there are no such locals anyway.
  bool has_nondefaultable_locals_ = true;

  // Depth of the current try block.
  int32_t current_catch_ = -1;

  static Value UnreachableValue(const uint8_t* pc) {
    return Value{pc, kWasmBottom};
  }

  bool CheckSimdFeatureFlagOpcode(WasmOpcode opcode) {
    if (!v8_flags.experimental_wasm_relaxed_simd &&
        WasmOpcodes::IsRelaxedSimdOpcode(opcode)) {
      this->DecodeError(
          "simd opcode not available, enable with --experimental-relaxed-simd");
      return false;
    }

    return true;
  }

  V8_INLINE MemoryAccessImmediate
  MakeMemoryAccessImmediate(uint32_t pc_offset, uint32_t max_alignment) {
    return MemoryAccessImmediate(this, this->pc_ + pc_offset, max_alignment,
                                 this->module_->is_memory64, validate);
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
      if (!v8_flags.trace_wasm_decoder) return;
      AppendStackState();
      PrintF("%.*s\n", len_, buffer_);
    }

    // Appends a formatted string.
    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {
      if (!v8_flags.trace_wasm_decoder) return;
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
      DCHECK(v8_flags.trace_wasm_decoder);
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
            Append("E");
            break;
          case kControlTryCatch:
            Append("C");
            break;
          case kControlTryCatchAll:
            Append("A");
            break;
        }
        if (c.start_merge.arity) Append("%u-", c.start_merge.arity);
        Append("%u", c.end_merge.arity);
        if (!c.reachable()) Append("%c", c.unreachable() ? '*' : '#');
      }
      Append(" | ");
      for (uint32_t i = 0; i < decoder_->stack_.size(); ++i) {
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
    if (!VALIDATE(v8_flags.enable_testing_opcode_in_wasm)) {
      this->DecodeError("Invalid opcode 0x%x", opcode);
      return 0;
    }
    CALL_INTERFACE_IF_OK_AND_REACHABLE(NopForTestingUnsupportedInLiftoff);
    // Return {0} if we failed, to not advance the pc past the end.
    if (this->failed()) {
      DCHECK_EQ(this->pc_, this->end_);
      return 0;
    }
    return 1;
  }

#define BUILD_SIMPLE_OPCODE(op, _, sig, ...) \
  DECODE(op) { return BuildSimpleOperator_##sig(kExpr##op); }
  FOREACH_SIMPLE_NON_CONST_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE

#define BUILD_SIMPLE_OPCODE(op, _, sig, ...)                \
  DECODE(op) {                                              \
    if constexpr (decoding_mode == kConstantExpression) {   \
      if (!VALIDATE(this->enabled_.has_extended_const())) { \
        NonConstError(this, kExpr##op);                     \
        return 0;                                           \
      }                                                     \
    }                                                       \
    return BuildSimpleOperator_##sig(kExpr##op);            \
  }
  FOREACH_SIMPLE_EXTENDED_CONST_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE

  DECODE(Block) {
    BlockTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(&imm.sig);
    Control* block = PushControl(kControlBlock, args.length());
    SetBlockType(block, imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Block, block);
    DropArgs(&imm.sig);
    PushMergeValues(block, &block->start_merge);
    return 1 + imm.length;
  }

  DECODE(Rethrow) {
    this->detected_->Add(kFeature_eh);
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
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
    this->detected_->Add(kFeature_eh);
    TagIndexImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(imm.tag->ToFunctionSig());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Throw, imm, base::VectorOf(args));
    DropArgs(imm.tag->ToFunctionSig());
    EndControl();
    return 1 + imm.length;
  }

  DECODE(Try) {
    this->detected_->Add(kFeature_eh);
    BlockTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(&imm.sig);
    Control* try_block = PushControl(kControlTry, args.length());
    SetBlockType(try_block, imm, args.begin());
    try_block->previous_catch = current_catch_;
    current_catch_ = static_cast<int>(control_depth() - 1);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Try, try_block);
    DropArgs(&imm.sig);
    PushMergeValues(try_block, &try_block->start_merge);
    return 1 + imm.length;
  }

  DECODE(Catch) {
    this->detected_->Add(kFeature_eh);
    TagIndexImmediate imm(this, this->pc_ + 1, validate);
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
    stack_.shrink_to(c->stack_depth);
    c->reachability = control_at(1)->innerReachability();
    RollbackLocalsInitialization(c);
    const WasmTagSig* sig = imm.tag->sig;
    stack_.EnsureMoreCapacity(static_cast<int>(sig->parameter_count()),
                              this->zone_);
    for (ValueType type : sig->parameters()) Push(CreateValue(type));
    base::Vector<Value> values(stack_.begin() + c->stack_depth,
                               sig->parameter_count());
    current_catch_ = c->previous_catch;  // Pop try scope.
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(CatchException, imm, c, values);
    current_code_reachable_and_ok_ = VALIDATE(this->ok()) && c->reachable();
    return 1 + imm.length;
  }

  DECODE(Delegate) {
    this->detected_->Add(kFeature_eh);
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
    // -1 because the current try block is not included in the count.
    if (!this->Validate(this->pc_ + 1, imm, control_depth() - 1)) return 0;
    Control* c = &control_.back();
    if (!VALIDATE(c->is_incomplete_try())) {
      this->DecodeError("delegate does not match a try");
      return 0;
    }
    // +1 because the current try block is not included in the count.
    uint32_t target_depth = imm.depth + 1;
    while (target_depth < control_depth() - 1 &&
           (!control_at(target_depth)->is_try() ||
            control_at(target_depth)->is_try_catch() ||
            control_at(target_depth)->is_try_catchall())) {
      target_depth++;
    }
    FallThrough();
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(Delegate, target_depth, c);
    current_catch_ = c->previous_catch;
    EndControl();
    PopControl();
    return 1 + imm.length;
  }

  DECODE(CatchAll) {
    this->detected_->Add(kFeature_eh);
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
    RollbackLocalsInitialization(c);
    current_catch_ = c->previous_catch;  // Pop try scope.
    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(CatchAll, c);
    stack_.shrink_to(c->stack_depth);
    current_code_reachable_and_ok_ = VALIDATE(this->ok()) && c->reachable();
    return 1;
  }

  DECODE(BrOnNull) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Value ref_object = Peek(0);
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
      case kRefNull: {
        Value result = CreateValue(ValueType::Ref(ref_object.type.heap_type()));
        // The result of br_on_null has the same value as the argument (but a
        // non-nullable type).
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          CALL_INTERFACE(BrOnNull, ref_object, imm.depth, false, &result);
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
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm, control_.size())) return 0;
    Value ref_object = Peek(0);
    if (!VALIDATE(ref_object.type.is_object_reference() ||
                  ref_object.type.is_bottom())) {
      PopTypeError(
          0, ref_object,
          "subtype of ((ref null any), (ref null extern) or (ref null func))");
      return 0;
    }
    Drop(ref_object);
    // Typechecking the branch and creating the branch merges requires the
    // non-null value on the stack, so we push it temporarily.
    Push(CreateValue(ref_object.type.AsNonNull()));
    // The {value_on_branch} parameter we pass to the interface must be
    // pointer-identical to the object on the stack.
    Value* value_on_branch = stack_value(1);
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
          CALL_INTERFACE(Forward, ref_object, value_on_branch);
          CALL_INTERFACE(BrOrRet, imm.depth, 0);
          // We know that the following code is not reachable, but according
          // to the spec it technically is. Set it to spec-only reachable.
          SetSucceedingCodeDynamicallyUnreachable();
          c->br_merge()->reached = true;
        }
        break;
      case kRefNull: {
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          CALL_INTERFACE(BrOnNonNull, ref_object, value_on_branch, imm.depth,
                         true);
          c->br_merge()->reached = true;
        }
        break;
      }
      default:
        PopTypeError(0, ref_object, "object reference");
        return 0;
    }
    // If we stay in the branch, {ref_object} is null. Drop it from the stack.
    Drop(1);
    return 1 + imm.length;
  }

  DECODE(Loop) {
    BlockTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ArgVector args = PeekArgs(&imm.sig);
    Control* block = PushControl(kControlLoop, args.length());
    SetBlockType(&control_.back(), imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Loop, block);
    DropArgs(&imm.sig);
    PushMergeValues(block, &block->start_merge);
    return 1 + imm.length;
  }

  DECODE(If) {
    BlockTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value cond = Peek(0, 0, kWasmI32);
    ArgVector args = PeekArgs(&imm.sig, 1);
    if (!VALIDATE(this->ok())) return 0;
    Control* if_block = PushControl(kControlIf, 1 + args.length());
    SetBlockType(if_block, imm, args.begin());
    CALL_INTERFACE_IF_OK_AND_REACHABLE(If, cond, if_block);
    Drop(cond);
    DropArgs(&imm.sig);
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
    RollbackLocalsInitialization(c);
    PushMergeValues(c, &c->start_merge);
    c->reachability = control_at(1)->innerReachability();
    current_code_reachable_and_ok_ = VALIDATE(this->ok()) && c->reachable();
    return 1;
  }

  DECODE(End) {
    DCHECK(!control_.empty());
    if constexpr (decoding_mode == kFunctionBody) {
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
            VALIDATE(this->ok()) && control_.back().reachable();
        CALL_INTERFACE_IF_OK_AND_REACHABLE(Rethrow, c);
        EndControl();
        PopControl();
        return 1;
      }
      if (c->is_onearmed_if()) {
        if (!VALIDATE(TypeCheckOneArmedIf(c))) return 0;
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
      control_.pop();
      return 1;
    }

    if (!VALIDATE(TypeCheckFallThru())) return 0;
    PopControl();
    return 1;
  }

  DECODE(Select) {
    Value cond = Peek(0, 2, kWasmI32);
    Value fval = Peek(1);
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
    this->detected_->Add(kFeature_reftypes);
    SelectTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    this->Validate(this->pc_ + 1, imm);
    if (!VALIDATE(this->ok())) return 0;
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
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
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
    BranchDepthImmediate imm(this, this->pc_ + 1, validate);
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
    BranchTableImmediate imm(this, this->pc_ + 1, validate);
    BranchTableIterator<ValidationTag> iterator(this, imm);
    Value key = Peek(0, 0, kWasmI32);
    if (!VALIDATE(this->ok())) return 0;
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

      if (ValidationTag::validate) {
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
    ImmI32Immediate imm(this, this->pc_ + 1, validate);
    Value value = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(I32Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(I64Const) {
    ImmI64Immediate imm(this, this->pc_ + 1, validate);
    Value value = CreateValue(kWasmI64);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(I64Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(F32Const) {
    ImmF32Immediate imm(this, this->pc_ + 1, validate);
    Value value = CreateValue(kWasmF32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(F32Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(F64Const) {
    ImmF64Immediate imm(this, this->pc_ + 1, validate);
    Value value = CreateValue(kWasmF64);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(F64Const, &value, imm.value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefNull) {
    this->detected_->Add(kFeature_reftypes);
    HeapTypeImmediate imm(this->enabled_, this, this->pc_ + 1, validate);
    this->Validate(this->pc_ + 1, imm);
    if (!VALIDATE(this->ok())) return 0;
    ValueType type = ValueType::RefNull(imm.type);
    Value value = CreateValue(type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefNull, type, &value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefIsNull) {
    this->detected_->Add(kFeature_reftypes);
    Value value = Peek(0);
    Value result = CreateValue(kWasmI32);
    switch (value.type.kind()) {
      case kRefNull:
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
        if (ValidationTag::validate) {
          PopTypeError(0, value, "reference type");
          return 0;
        }
        UNREACHABLE();
    }
  }

  DECODE(RefFunc) {
    this->detected_->Add(kFeature_reftypes);
    IndexImmediate imm(this, this->pc_ + 1, "function index", validate);
    if (!this->ValidateFunction(this->pc_ + 1, imm)) return 0;
    HeapType heap_type(this->enabled_.has_typed_funcref()
                           ? this->module_->functions[imm.index].sig_index
                           : HeapType::kFunc);
    Value value = CreateValue(ValueType::Ref(heap_type));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(RefFunc, imm.index, &value);
    Push(value);
    return 1 + imm.length;
  }

  DECODE(RefAsNonNull) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    Value value = Peek(0);
    switch (value.type.kind()) {
      case kBottom:
        // We are in unreachable code. Forward the bottom value.
      case kRef:
        // A non-nullable value can remain as-is.
        return 1;
      case kRefNull: {
        Value result = CreateValue(ValueType::Ref(value.type.heap_type()));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RefAsNonNull, value, &result);
        Drop(value);
        Push(result);
        return 1;
      }
      default:
        if (ValidationTag::validate) {
          PopTypeError(0, value, "reference type");
        }
        return 0;
    }
  }

  V8_INLINE DECODE(LocalGet) {
    IndexImmediate imm(this, this->pc_ + 1, "local index", validate);
    if (!this->ValidateLocal(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(this->is_local_initialized(imm.index))) {
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
    IndexImmediate imm(this, this->pc_ + 1, "local index", validate);
    if (!this->ValidateLocal(this->pc_ + 1, imm)) return 0;
    Value value = Peek(0, 0, this->local_type(imm.index));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(LocalSet, value, imm);
    Drop(value);
    this->set_local_initialized(imm.index);
    return 1 + imm.length;
  }

  DECODE(LocalTee) {
    IndexImmediate imm(this, this->pc_ + 1, "local index", validate);
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
    Peek(0);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(Drop);
    Drop(1);
    return 1;
  }

  DECODE(GlobalGet) {
    GlobalIndexImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value result = CreateValue(imm.global->type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(GlobalGet, &result, imm);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(GlobalSet) {
    GlobalIndexImmediate imm(this, this->pc_ + 1, validate);
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
    this->detected_->Add(kFeature_reftypes);
    IndexImmediate imm(this, this->pc_ + 1, "table index", validate);
    if (!this->ValidateTable(this->pc_ + 1, imm)) return 0;
    Value index = Peek(0, 0, kWasmI32);
    Value result = CreateValue(this->module_->tables[imm.index].type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(TableGet, index, &result, imm);
    Drop(index);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(TableSet) {
    this->detected_->Add(kFeature_reftypes);
    IndexImmediate imm(this, this->pc_ + 1, "table index", validate);
    if (!this->ValidateTable(this->pc_ + 1, imm)) return 0;
    Value value = Peek(0, 1, this->module_->tables[imm.index].type);
    Value index = Peek(1, 0, kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(TableSet, index, value, imm);
    Drop(2);
    return 1 + imm.length;
  }

  DECODE(LoadMem) { return DecodeLoadMem(GetLoadType(opcode)); }

  DECODE(StoreMem) { return DecodeStoreMem(GetStoreType(opcode)); }

  DECODE(MemoryGrow) {
    MemoryIndexImmediate imm(this, this->pc_ + 1, validate);
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
    MemoryIndexImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    ValueType result_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value result = CreateValue(result_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CurrentMemoryPages, &result);
    Push(result);
    return 1 + imm.length;
  }

  DECODE(CallFunction) {
    CallFunctionImmediate imm(this, this->pc_ + 1, validate);
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
    CallIndirectImmediate imm(this, this->pc_ + 1, validate);
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
    CallFunctionImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    if (!VALIDATE(this->CanReturnCall(imm.sig))) {
      this->DecodeError("%s: %s", WasmOpcodes::OpcodeName(kExprReturnCall),
                        "tail call type error");
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
    CallIndirectImmediate imm(this, this->pc_ + 1, validate);
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
    SigIndexImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value func_ref = Peek(0, 0, ValueType::RefNull(imm.index));
    ArgVector args = PeekArgs(imm.sig, 1);
    ReturnVector returns = CreateReturnValues(imm.sig);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(CallRef, func_ref, imm.sig, imm.index,
                                       args.begin(), returns.begin());
    Drop(func_ref);
    DropArgs(imm.sig);
    PushReturns(returns);
    return 1 + imm.length;
  }

  DECODE(ReturnCallRef) {
    CHECK_PROTOTYPE_OPCODE(typed_funcref);
    CHECK_PROTOTYPE_OPCODE(return_call);
    SigIndexImmediate imm(this, this->pc_ + 1, validate);
    if (!this->Validate(this->pc_ + 1, imm)) return 0;
    Value func_ref = Peek(0, 0, ValueType::RefNull(imm.index));
    ArgVector args = PeekArgs(imm.sig, 1);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(ReturnCallRef, func_ref, imm.sig,
                                       imm.index, args.begin());
    Drop(func_ref);
    DropArgs(imm.sig);
    EndControl();
    return 1 + imm.length;
  }

  DECODE(Numeric) {
    auto [full_opcode, opcode_length] =
        this->template read_prefixed_opcode<ValidationTag>(this->pc_,
                                                           "numeric index");
    if (full_opcode == kExprTableGrow || full_opcode == kExprTableSize ||
        full_opcode == kExprTableFill) {
      this->detected_->Add(kFeature_reftypes);
    }
    trace_msg->AppendOpcode(full_opcode);
    return DecodeNumericOpcode(full_opcode, opcode_length);
  }

  DECODE(Simd) {
    this->detected_->Add(kFeature_simd);
    if (!CheckHardwareSupportsSimd()) {
      if (v8_flags.correctness_fuzzer_suppressions) {
        FATAL("Aborting on missing Wasm SIMD support");
      }
      this->DecodeError("Wasm SIMD unsupported");
      return 0;
    }
    auto [full_opcode, opcode_length] =
        this->template read_prefixed_opcode<ValidationTag>(this->pc_);
    if (!VALIDATE(this->ok())) return 0;
    trace_msg->AppendOpcode(full_opcode);
    if (!CheckSimdFeatureFlagOpcode(full_opcode)) {
      return 0;
    }
    return DecodeSimdOpcode(full_opcode, opcode_length);
  }

  DECODE(Atomic) {
    this->detected_->Add(kFeature_threads);
    auto [full_opcode, opcode_length] =
        this->template read_prefixed_opcode<ValidationTag>(this->pc_,
                                                           "atomic index");
    trace_msg->AppendOpcode(full_opcode);
    return DecodeAtomicOpcode(full_opcode, opcode_length);
  }

  DECODE(GC) {
    auto [full_opcode, opcode_length] =
        this->template read_prefixed_opcode<ValidationTag>(this->pc_,
                                                           "gc index");
    trace_msg->AppendOpcode(full_opcode);
    // If we are validating we could have read an illegal opcode. Handle that
    // separately.
    if (!VALIDATE(full_opcode != 0)) {
      DCHECK(this->failed());
      return 0;
    } else if (full_opcode >= kExprStringNewUtf8) {
      CHECK_PROTOTYPE_OPCODE(stringref);
      return DecodeStringRefOpcode(full_opcode, opcode_length);
    } else {
      CHECK_PROTOTYPE_OPCODE(gc);
      return DecodeGCOpcode(full_opcode, opcode_length);
    }
  }

#define SIMPLE_PROTOTYPE_CASE(name, ...) \
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
    decoder->DecodeError("opcode %s is not allowed in constant expressions",
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
#define DECODE_IMPL2(opcode, name)                        \
  if (idx == opcode) {                                    \
    if constexpr (decoding_mode == kConstantExpression) { \
      return &WasmFullDecoder::NonConstError;             \
    } else {                                              \
      return &WasmFullDecoder::Decode##name;              \
    }                                                     \
  }
#define DECODE_IMPL_CONST(opcode) DECODE_IMPL_CONST2(kExpr##opcode, opcode)
#define DECODE_IMPL_CONST2(opcode, name) \
  if (idx == opcode) return &WasmFullDecoder::Decode##name

  static constexpr OpcodeHandler GetOpcodeHandlerTableEntry(size_t idx) {
    DECODE_IMPL(Nop);
#define BUILD_SIMPLE_OPCODE(op, ...) DECODE_IMPL(op);
    FOREACH_SIMPLE_NON_CONST_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE
#define BUILD_SIMPLE_EXTENDED_CONST_OPCODE(op, ...) DECODE_IMPL_CONST(op);
    FOREACH_SIMPLE_EXTENDED_CONST_OPCODE(BUILD_SIMPLE_EXTENDED_CONST_OPCODE)
#undef BUILD_SIMPLE_EXTENDED_CONST_OPCODE
    DECODE_IMPL(Block);
    DECODE_IMPL(Rethrow);
    DECODE_IMPL(Throw);
    DECODE_IMPL(Try);
    DECODE_IMPL(Catch);
    DECODE_IMPL(Delegate);
    DECODE_IMPL(CatchAll);
    DECODE_IMPL(BrOnNull);
    DECODE_IMPL(BrOnNonNull);
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
#define SIMPLE_PROTOTYPE_CASE(name, ...) DECODE_IMPL(name);
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
    stack_.shrink_to(current->stack_depth);
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
  void SetBlockType(Control* c, BlockTypeImmediate& imm, Value* args) {
    const byte* pc = this->pc_;
    InitMerge(&c->end_merge, imm.out_arity(), [pc, &imm](uint32_t i) {
      return Value{pc, imm.out_type(i)};
    });
    InitMerge(&c->start_merge, imm.in_arity(), [&imm, args](uint32_t i) {
      // The merge needs to be instantiated with Values of the correct
      // type, even if the actual Value is bottom/unreachable or has
      // a subtype of the static type.
      // So we copy-construct a new Value, and update its type.
      Value value = args[i];
      value.type = imm.in_type(i);
      return value;
    });
  }

  // In reachable code, check if there are at least {count} values on the stack.
  // In unreachable code, if there are less than {count} values on the stack,
  // insert a number of unreachable values underneath the current values equal
  // to the difference, and return that number.
  V8_INLINE int EnsureStackArguments(int count) {
    uint32_t limit = control_.back().stack_depth;
    if (V8_LIKELY(stack_.size() >= count + limit)) return 0;
    return EnsureStackArguments_Slow(count, limit);
  }

  V8_NOINLINE V8_PRESERVE_MOST int EnsureStackArguments_Slow(int count,
                                                             uint32_t limit) {
    if (!VALIDATE(control_.back().unreachable())) {
      NotEnoughArgumentsError(count, stack_.size() - limit);
    }
    // Silently create unreachable values out of thin air underneath the
    // existing stack values. To do so, we have to move existing stack values
    // upwards in the stack, then instantiate the new Values as
    // {UnreachableValue}.
    int current_values = stack_.size() - limit;
    int additional_values = count - current_values;
    DCHECK_GT(additional_values, 0);
    stack_.EnsureMoreCapacity(additional_values, this->zone_);
    Value unreachable_value = UnreachableValue(this->pc_);
    for (int i = 0; i < additional_values; ++i) stack_.push(unreachable_value);
    if (current_values > 0) {
      // Move the current values up to the end of the stack, and create
      // unreachable values below.
      Value* stack_base = stack_value(current_values + additional_values);
      for (int i = current_values - 1; i >= 0; i--) {
        stack_base[additional_values + i] = stack_base[i];
      }
      for (int i = 0; i < additional_values; i++) {
        stack_base[i] = UnreachableValue(this->pc_);
      }
    }
    return additional_values;
  }

  // Peeks arguments as required by signature.
  V8_INLINE ArgVector PeekArgs(const FunctionSig* sig, int depth = 0) {
    return PeekArgs(
        base::VectorOf(sig->parameters().begin(), sig->parameter_count()),
        depth);
  }

  // Drops a number of stack elements equal to the {sig}'s parameter count (0 if
  // {sig} is null), or all of them if less are present.
  V8_INLINE void DropArgs(const FunctionSig* sig) {
    int count = static_cast<int>(sig->parameter_count());
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

  V8_INLINE ArgVector PeekArgs(base::Vector<const ValueType> arg_types,
                               int depth = 0) {
    int size = static_cast<int>(arg_types.size());
    if (size == 0) return {};
    EnsureStackArguments(size + depth);
    ArgVector args(stack_value(size + depth), arg_types.size());
    for (int i = 0; i < size; i++) {
      ValidateArgType(args, i, arg_types[i]);
    }
    return args;
  }

  // TODO(jkummerow): Consider refactoring control stack management so
  // that {drop_values} is never needed. That would require decoupling
  // creation of the Control object from setting of its stack depth.
  Control* PushControl(ControlKind kind, uint32_t drop_values) {
    DCHECK(!control_.empty());
    Reachability reachability = control_.back().innerReachability();
    // In unreachable code, we may run out of stack.
    uint32_t stack_depth =
        stack_.size() >= drop_values ? stack_.size() - drop_values : 0;
    stack_depth = std::max(stack_depth, control_.back().stack_depth);
    uint32_t init_stack_depth = this->locals_initialization_stack_depth();
    control_.EnsureMoreCapacity(1, this->zone_);
    control_.emplace_back(this->zone_, kind, stack_depth, init_stack_depth,
                          this->pc_, reachability);
    current_code_reachable_and_ok_ =
        VALIDATE(this->ok()) && reachability == kReachable;
    return &control_.back();
  }

  void PopControl() {
    // This cannot be the outermost control block.
    DCHECK_LT(1, control_.size());
    Control* c = &control_.back();
    DCHECK_LE(c->stack_depth, stack_.size());

    CALL_INTERFACE_IF_OK_AND_PARENT_REACHABLE(PopControl, c);

    // - In non-unreachable code, a loop just leaves the values on the stack.
    // - In unreachable code, it is not guaranteed that we have Values of the
    //   correct types on the stack, so we have to make sure we do. Their values
    //   do not matter, so we might as well push the (uninitialized) values of
    //   the loop's end merge.
    if (!c->is_loop() || c->unreachable()) {
      PushMergeValues(c, &c->end_merge);
    }
    RollbackLocalsInitialization(c);

    bool parent_reached =
        c->reachable() || c->end_merge.reached || c->is_onearmed_if();
    control_.pop();
    // If the parent block was reachable before, but the popped control does not
    // return to here, this block becomes "spec only reachable".
    if (!parent_reached) SetSucceedingCodeDynamicallyUnreachable();
    current_code_reachable_and_ok_ =
        VALIDATE(this->ok()) && control_.back().reachable();
  }

  int DecodeLoadMem(LoadType type, int prefix_len = 1) {
    MemoryAccessImmediate imm =
        MakeMemoryAccessImmediate(prefix_len, type.size_log_2());
    if (!this->Validate(this->pc_ + prefix_len, imm)) return 0;
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(0, 0, index_type);
    Value result = CreateValue(type.value_type());
    if (V8_LIKELY(!CheckStaticallyOutOfBounds(type.size(), imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadMem, type, imm, index, &result);
    }
    Drop(index);
    Push(result);
    return prefix_len + imm.length;
  }

  int DecodeLoadTransformMem(LoadType type, LoadTransformationKind transform,
                             uint32_t opcode_length) {
    // Load extends always load 64-bits.
    uint32_t max_alignment =
        transform == LoadTransformationKind::kExtend ? 3 : type.size_log_2();
    MemoryAccessImmediate imm =
        MakeMemoryAccessImmediate(opcode_length, max_alignment);
    if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(0, 0, index_type);
    Value result = CreateValue(kWasmS128);
    uintptr_t op_size =
        transform == LoadTransformationKind::kExtend ? 8 : type.size();
    if (V8_LIKELY(!CheckStaticallyOutOfBounds(op_size, imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadTransform, type, transform, imm,
                                         index, &result);
    }
    Drop(index);
    Push(result);
    return opcode_length + imm.length;
  }

  int DecodeLoadLane(WasmOpcode opcode, LoadType type, uint32_t opcode_length) {
    MemoryAccessImmediate mem_imm =
        MakeMemoryAccessImmediate(opcode_length, type.size_log_2());
    if (!this->Validate(this->pc_ + opcode_length, mem_imm)) return 0;
    SimdLaneImmediate lane_imm(this, this->pc_ + opcode_length + mem_imm.length,
                               validate);
    if (!this->Validate(this->pc_ + opcode_length, opcode, lane_imm)) return 0;
    Value v128 = Peek(0, 1, kWasmS128);
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(1, 0, index_type);

    Value result = CreateValue(kWasmS128);
    if (V8_LIKELY(!CheckStaticallyOutOfBounds(type.size(), mem_imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(LoadLane, type, v128, index, mem_imm,
                                         lane_imm.lane, &result);
    }
    Drop(2);
    Push(result);
    return opcode_length + mem_imm.length + lane_imm.length;
  }

  int DecodeStoreLane(WasmOpcode opcode, StoreType type,
                      uint32_t opcode_length) {
    MemoryAccessImmediate mem_imm =
        MakeMemoryAccessImmediate(opcode_length, type.size_log_2());
    if (!this->Validate(this->pc_ + opcode_length, mem_imm)) return 0;
    SimdLaneImmediate lane_imm(this, this->pc_ + opcode_length + mem_imm.length,
                               validate);
    if (!this->Validate(this->pc_ + opcode_length, opcode, lane_imm)) return 0;
    Value v128 = Peek(0, 1, kWasmS128);
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(1, 0, index_type);

    if (V8_LIKELY(!CheckStaticallyOutOfBounds(type.size(), mem_imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(StoreLane, type, mem_imm, index, v128,
                                         lane_imm.lane);
    }
    Drop(2);
    return opcode_length + mem_imm.length + lane_imm.length;
  }

  bool CheckStaticallyOutOfBounds(uintptr_t size, uintptr_t offset) {
    const bool statically_oob = !base::IsInBounds<uintptr_t>(
        offset, size, this->module_->max_memory_size);
    if (V8_UNLIKELY(statically_oob)) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(Trap, TrapReason::kTrapMemOutOfBounds);
      SetSucceedingCodeDynamicallyUnreachable();
    }
    return statically_oob;
  }

  int DecodeStoreMem(StoreType store, int prefix_len = 1) {
    MemoryAccessImmediate imm =
        MakeMemoryAccessImmediate(prefix_len, store.size_log_2());
    if (!this->Validate(this->pc_ + prefix_len, imm)) return 0;
    Value value = Peek(0, 1, store.value_type());
    ValueType index_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value index = Peek(1, 0, index_type);
    if (V8_LIKELY(!CheckStaticallyOutOfBounds(store.size(), imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(StoreMem, store, imm, index, value);
    }
    Drop(2);
    return prefix_len + imm.length;
  }

  uint32_t SimdConstOp(uint32_t opcode_length) {
    Simd128Immediate imm(this, this->pc_ + opcode_length, validate);
    Value result = CreateValue(kWasmS128);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(S128Const, imm, &result);
    Push(result);
    return opcode_length + kSimd128Size;
  }

  uint32_t SimdExtractLane(WasmOpcode opcode, ValueType type,
                           uint32_t opcode_length) {
    SimdLaneImmediate imm(this, this->pc_ + opcode_length, validate);
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
    SimdLaneImmediate imm(this, this->pc_ + opcode_length, validate);
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
    Simd128Immediate imm(this, this->pc_ + opcode_length, validate);
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
    if constexpr (decoding_mode == kConstantExpression) {
      // Currently, only s128.const is allowed in constant expressions.
      if (opcode != kExprS128Const) {
        this->DecodeError("opcode %s is not allowed in constant expressions",
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

  // Returns true if type checking will always fail, either because the types
  // are unrelated or because the target_type is one of the null sentinels and
  // conversion to null does not succeed.
  bool TypeCheckAlwaysFails(Value obj, HeapType expected_type,
                            bool null_succeeds) {
    bool types_unrelated =
        !IsSubtypeOf(ValueType::Ref(expected_type), obj.type, this->module_) &&
        !IsSubtypeOf(obj.type, ValueType::RefNull(expected_type),
                     this->module_);
    // For "unrelated" types the check can still succeed for the null value on
    // instructions treating null as a successful check.
    return (types_unrelated && (!null_succeeds || !obj.type.is_nullable())) ||
           (!null_succeeds &&
            (expected_type.representation() == HeapType::kNone ||
             expected_type.representation() == HeapType::kNoFunc ||
             expected_type.representation() == HeapType::kNoExtern));
  }
  bool TypeCheckAlwaysFails(Value obj, Value rtt) {
    // All old casts / checks treat null as failure.
    const bool kNullSucceeds = false;
    return TypeCheckAlwaysFails(obj, HeapType(rtt.type.ref_index()),
                                kNullSucceeds);
  }

  // Checks if {obj} is a subtype of type, thus checking will always
  // succeed.
  bool TypeCheckAlwaysSucceeds(Value obj, HeapType type) {
    return IsSubtypeOf(obj.type, ValueType::RefNull(type), this->module_);
  }
  bool TypeCheckAlwaysSucceeds(Value obj, Value rtt) {
    return TypeCheckAlwaysSucceeds(obj, HeapType(rtt.type.ref_index()));
  }

#define NON_CONST_ONLY                                                    \
  if constexpr (decoding_mode == kConstantExpression) {                   \
    this->DecodeError("opcode %s is not allowed in constant expressions", \
                      this->SafeOpcodeNameAt(this->pc()));                \
    return 0;                                                             \
  }

  int DecodeGCOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    // Bigger GC opcodes are handled via {DecodeStringRefOpcode}, so we can
    // assume here that opcodes are within [0xfb00, 0xfbff].
    // This assumption might help the big switch below.
    V8_ASSUME(opcode >> 8 == kGCPrefix);
    switch (opcode) {
      case kExprStructNew: {
        StructIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType rtt_type = ValueType::Rtt(imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        ArgVector args = PeekArgs(imm.struct_type, 1);
        Value value = CreateValue(ValueType::Ref(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructNew, imm, rtt, args.begin(),
                                           &value);
        Drop(rtt);
        DropArgs(imm.struct_type);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprStructNewDefault: {
        StructIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (ValidationTag::validate) {
          for (uint32_t i = 0; i < imm.struct_type->field_count(); i++) {
            ValueType ftype = imm.struct_type->field(i);
            if (!VALIDATE(ftype.is_defaultable())) {
              this->DecodeError(
                  "%s: struct type %d has field %d of non-defaultable type %s",
                  WasmOpcodes::OpcodeName(opcode), imm.index, i,
                  ftype.name().c_str());
              return 0;
            }
          }
        }
        ValueType rtt_type = ValueType::Rtt(imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        Value value = CreateValue(ValueType::Ref(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructNewDefault, imm, rtt, &value);
        Drop(rtt);
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprStructGet: {
        NON_CONST_ONLY
        FieldImmediate field(this, this->pc_ + opcode_length, validate);
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
            Peek(0, 0, ValueType::RefNull(field.struct_imm.index));
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
        FieldImmediate field(this, this->pc_ + opcode_length, validate);
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
            Peek(0, 0, ValueType::RefNull(field.struct_imm.index));
        Value value = CreateValue(field_type.Unpacked());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructGet, struct_obj, field,
                                           opcode == kExprStructGetS, &value);
        Drop(struct_obj);
        Push(value);
        return opcode_length + field.length;
      }
      case kExprStructSet: {
        NON_CONST_ONLY
        FieldImmediate field(this, this->pc_ + opcode_length, validate);
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
            Peek(1, 0, ValueType::RefNull(field.struct_imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StructSet, struct_obj, field,
                                           field_value);
        Drop(2);
        return opcode_length + field.length;
      }
      case kExprArrayNew: {
        ArrayIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType rtt_type = ValueType::Rtt(imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        Value length = Peek(1, 1, kWasmI32);
        Value initial_value =
            Peek(2, 0, imm.array_type->element_type().Unpacked());
        Value value = CreateValue(ValueType::Ref(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNew, imm, length, initial_value,
                                           rtt, &value);
        Drop(3);  // rtt, length, initial_value.
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayNewDefault: {
        ArrayIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(imm.array_type->element_type().is_defaultable())) {
          this->DecodeError(
              "%s: array type %d has non-defaultable element type %s",
              WasmOpcodes::OpcodeName(opcode), imm.index,
              imm.array_type->element_type().name().c_str());
          return 0;
        }
        ValueType rtt_type = ValueType::Rtt(imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        Value length = Peek(1, 0, kWasmI32);
        Value value = CreateValue(ValueType::Ref(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewDefault, imm, length, rtt,
                                           &value);
        Drop(2);  // rtt, length
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayNewData: {
        NON_CONST_ONLY
        ArrayIndexImmediate array_imm(this, this->pc_ + opcode_length,
                                      validate);
        if (!this->Validate(this->pc_ + opcode_length, array_imm)) return 0;
        ValueType element_type = array_imm.array_type->element_type();
        if (element_type.is_reference()) {
          this->DecodeError(
              "array.new_data can only be used with numeric-type arrays, found "
              "array type #%d instead",
              array_imm.index);
          return 0;
        }
#if V8_TARGET_BIG_ENDIAN
        // Byte sequences in data segments are interpreted as little endian for
        // the purposes of this instruction. This means that those will have to
        // be transformed in big endian architectures. TODO(7748): Implement.
        if (element_type.value_kind_size() > 1) {
          UNIMPLEMENTED();
        }
#endif
        const byte* data_index_pc =
            this->pc_ + opcode_length + array_imm.length;
        IndexImmediate data_segment(this, data_index_pc, "data segment",
                                    validate);
        if (!this->ValidateDataSegment(data_index_pc, data_segment)) return 0;

        ValueType rtt_type = ValueType::Rtt(array_imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, array_imm.index, &rtt);
        Push(rtt);

        Value length = Peek(1, 1, kWasmI32);
        Value offset = Peek(2, 0, kWasmI32);

        Value array = CreateValue(ValueType::Ref(array_imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewSegment, array_imm,
                                           data_segment, offset, length, rtt,
                                           &array);
        Drop(3);  // rtt, length, offset
        Push(array);
        return opcode_length + array_imm.length + data_segment.length;
      }
      case kExprArrayNewElem: {
        NON_CONST_ONLY
        ArrayIndexImmediate array_imm(this, this->pc_ + opcode_length,
                                      validate);
        if (!this->Validate(this->pc_ + opcode_length, array_imm)) return 0;
        ValueType element_type = array_imm.array_type->element_type();
        if (element_type.is_numeric()) {
          this->DecodeError(
              "array.new_elem can only be used with reference-type arrays, "
              "found array type #%d instead",
              array_imm.index);
          return 0;
        }
        const byte* elem_index_pc =
            this->pc_ + opcode_length + array_imm.length;
        IndexImmediate elem_segment(this, elem_index_pc, "data segment",
                                    validate);
        if (!this->ValidateElementSegment(elem_index_pc, elem_segment)) {
          return 0;
        }

        ValueType rtt_type = ValueType::Rtt(array_imm.index);
        Value rtt = CreateValue(rtt_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, array_imm.index, &rtt);
        Push(rtt);
        Value array = CreateValue(ValueType::Ref(array_imm.index));
        ValueType elem_segment_type =
            this->module_->elem_segments[elem_segment.index].type;
        if (V8_UNLIKELY(
                !IsSubtypeOf(elem_segment_type, element_type, this->module_))) {
          this->DecodeError(
              "array.new_elem: segment type %s is not a subtype of array "
              "element type %s",
              elem_segment_type.name().c_str(), element_type.name().c_str());
          return 0;
        }

        Value length = Peek(1, 1, kWasmI32);
        Value offset = Peek(2, 0, kWasmI32);

        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewSegment, array_imm,
                                           elem_segment, offset, length, rtt,
                                           &array);
        Drop(3);  // rtt, length, offset
        Push(array);
        return opcode_length + array_imm.length + elem_segment.length;
      }
      case kExprArrayGetS:
      case kExprArrayGetU: {
        NON_CONST_ONLY
        ArrayIndexImmediate imm(this, this->pc_ + opcode_length, validate);
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
        Value array_obj = Peek(1, 0, ValueType::RefNull(imm.index));
        Value value = CreateValue(imm.array_type->element_type().Unpacked());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayGet, array_obj, imm, index,
                                           opcode == kExprArrayGetS, &value);
        Drop(2);  // index, array_obj
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArrayGet: {
        NON_CONST_ONLY
        ArrayIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(!imm.array_type->element_type().is_packed())) {
          this->DecodeError(
              "array.get: Immediate array type %d has packed type %s. Use "
              "array.get_s or array.get_u instead.",
              imm.index, imm.array_type->element_type().name().c_str());
          return 0;
        }
        Value index = Peek(0, 1, kWasmI32);
        Value array_obj = Peek(1, 0, ValueType::RefNull(imm.index));
        Value value = CreateValue(imm.array_type->element_type());
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayGet, array_obj, imm, index,
                                           true, &value);
        Drop(2);  // index, array_obj
        Push(value);
        return opcode_length + imm.length;
      }
      case kExprArraySet: {
        NON_CONST_ONLY
        ArrayIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        if (!VALIDATE(imm.array_type->mutability())) {
          this->DecodeError("array.set: immediate array type %d is immutable",
                            imm.index);
          return 0;
        }
        Value value = Peek(0, 2, imm.array_type->element_type().Unpacked());
        Value index = Peek(1, 1, kWasmI32);
        Value array_obj = Peek(2, 0, ValueType::RefNull(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArraySet, array_obj, imm, index,
                                           value);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprArrayLen: {
        NON_CONST_ONLY
        Value array_obj = Peek(0, 0, kWasmArrayRef);
        Value value = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayLen, array_obj, &value);
        Drop(array_obj);
        Push(value);
        return opcode_length;
      }
      case kExprArrayCopy: {
        NON_CONST_ONLY
        ArrayIndexImmediate dst_imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, dst_imm)) return 0;
        if (!VALIDATE(dst_imm.array_type->mutability())) {
          this->DecodeError(
              "array.copy: immediate destination array type #%d is immutable",
              dst_imm.index);
          return 0;
        }
        ArrayIndexImmediate src_imm(
            this, this->pc_ + opcode_length + dst_imm.length, validate);
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
        Value dst = Peek(4, 0, ValueType::RefNull(dst_imm.index));
        Value dst_index = Peek(3, 1, kWasmI32);
        Value src = Peek(2, 2, ValueType::RefNull(src_imm.index));
        Value src_index = Peek(1, 3, kWasmI32);
        Value length = Peek(0, 4, kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayCopy, dst, dst_index, src,
                                           src_index, length);
        Drop(5);
        return opcode_length + dst_imm.length + src_imm.length;
      }
      case kExprArrayFill: {
        NON_CONST_ONLY
        ArrayIndexImmediate array_imm(this, this->pc_ + opcode_length,
                                      validate);
        if (!this->Validate(this->pc_ + opcode_length, array_imm)) return 0;
        if (!VALIDATE(array_imm.array_type->mutability())) {
          this->DecodeError("array.init: immediate array type #%d is immutable",
                            array_imm.index);
          return 0;
        }

        Value array = Peek(3, 0, ValueType::RefNull(array_imm.index));
        Value offset = Peek(2, 1, kWasmI32);
        Value value = Peek(1, 2, array_imm.array_type->element_type());
        Value length = Peek(0, 3, kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayFill, array_imm, array, offset,
                                           value, length);
        Drop(4);
        return opcode_length + array_imm.length;
      }
      case kExprArrayNewFixed: {
        ArrayIndexImmediate array_imm(this, this->pc_ + opcode_length,
                                      validate);
        if (!this->Validate(this->pc_ + opcode_length, array_imm)) return 0;
        IndexImmediate length_imm(this,
                                  this->pc_ + opcode_length + array_imm.length,
                                  "array.new_fixed length", validate);
        uint32_t elem_count = length_imm.index;
        if (!VALIDATE(elem_count <= kV8MaxWasmArrayNewFixedLength)) {
          this->DecodeError(
              "Requested length %u for array.new_fixed too large, maximum is "
              "%zu",
              length_imm.index, kV8MaxWasmArrayNewFixedLength);
          return 0;
        }
        Value rtt = CreateValue(ValueType::Rtt(array_imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, array_imm.index, &rtt);
        Push(rtt);
        ValueType element_type = array_imm.array_type->element_type();
        std::vector<ValueType> element_types(elem_count,
                                             element_type.Unpacked());
        FunctionSig element_sig(0, elem_count, element_types.data());
        ArgVector elements = PeekArgs(&element_sig, 1);
        Value result = CreateValue(ValueType::Ref(array_imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ArrayNewFixed, array_imm, elements,
                                           rtt, &result);
        Drop(elem_count + 1);
        Push(result);
        return opcode_length + array_imm.length + length_imm.length;
      }
      case kExprI31New: {
        Value input = Peek(0, 0, kWasmI32);
        Value value = CreateValue(ValueType::Ref(HeapType::kI31));
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
      case kExprRefCast:
      case kExprRefCastNull: {
        NON_CONST_ONLY
        HeapTypeImmediate imm(this->enabled_, this, this->pc_ + opcode_length,
                              validate);
        this->Validate(this->pc_ + opcode_length, imm);
        if (!VALIDATE(this->ok())) return 0;
        opcode_length += imm.length;

        std::optional<Value> rtt;
        HeapType target_type = imm.type;
        if (imm.type.is_index()) {
          rtt = CreateValue(ValueType::Rtt(imm.type.ref_index()));
          CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.type.ref_index(),
                                             &rtt.value());
          Push(rtt.value());
        }

        Value obj = Peek(rtt.has_value() ? 1 : 0);
        if (!VALIDATE((obj.type.is_object_reference() &&
                       IsSameTypeHierarchy(obj.type.heap_type(), target_type,
                                           this->module_)) ||
                      obj.type.is_bottom())) {
          this->DecodeError(
              obj.pc(),
              "Invalid types for %s: %s of type %s has to "
              "be in the same reference type hierarchy as (ref %s)",
              WasmOpcodes::OpcodeName(opcode), SafeOpcodeNameAt(obj.pc()),
              obj.type.name().c_str(), target_type.name().c_str());
          return 0;
        }

        bool null_succeeds = opcode == kExprRefCastNull;
        Value value = CreateValue(ValueType::RefMaybeNull(
            target_type, null_succeeds ? kNullable : kNonNullable));
        if (current_code_reachable_and_ok_) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, target_type))) {
            // Drop the rtt from the stack, then forward the object value to the
            // result.
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);
            }
            if (obj.type.is_nullable() && !null_succeeds) {
              CALL_INTERFACE(AssertNotNullTypecheck, obj, &value);
            } else {
              CALL_INTERFACE(Forward, obj, &value);
            }
          } else if (V8_UNLIKELY(TypeCheckAlwaysFails(obj, target_type,
                                                      null_succeeds))) {
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);
            }
            // Unrelated types. The only way this will not trap is if the object
            // is null.
            if (obj.type.is_nullable() && null_succeeds) {
              // Drop rtt from the stack, then assert that obj is null.
              CALL_INTERFACE(AssertNullTypecheck, obj, &value);
            } else {
              CALL_INTERFACE(Trap, TrapReason::kTrapIllegalCast);
              // We know that the following code is not reachable, but according
              // to the spec it technically is. Set it to spec-only reachable.
              SetSucceedingCodeDynamicallyUnreachable();
            }
          } else {
            if (rtt.has_value()) {
              CALL_INTERFACE(RefCast, obj, rtt.value(), &value, null_succeeds);
            } else {
              CALL_INTERFACE(RefCastAbstract, obj, target_type, &value,
                             null_succeeds);
            }
          }
        }
        Drop(1 + rtt.has_value());
        Push(value);
        return opcode_length;
      }
      case kExprRefTestNull:
      case kExprRefTest: {
        NON_CONST_ONLY
        HeapTypeImmediate imm(this->enabled_, this, this->pc_ + opcode_length,
                              validate);
        this->Validate(this->pc_ + opcode_length, imm);
        if (!VALIDATE(this->ok())) return 0;
        opcode_length += imm.length;

        std::optional<Value> rtt;
        HeapType target_type = imm.type;
        if (imm.type.is_index()) {
          rtt = CreateValue(ValueType::Rtt(imm.type.ref_index()));
          CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.type.ref_index(),
                                             &rtt.value());
          Push(rtt.value());
        }

        Value obj = Peek(rtt.has_value() ? 1 : 0);
        Value value = CreateValue(kWasmI32);

        if (!VALIDATE((obj.type.is_object_reference() &&
                       IsSameTypeHierarchy(obj.type.heap_type(), target_type,
                                           this->module_)) ||
                      obj.type.is_bottom())) {
          this->DecodeError(
              obj.pc(),
              "Invalid types for %s: %s of type %s has to "
              "be in the same reference type hierarchy as (ref %s)",
              WasmOpcodes::OpcodeName(opcode), SafeOpcodeNameAt(obj.pc()),
              obj.type.name().c_str(), target_type.name().c_str());
          return 0;
        }
        bool null_succeeds = opcode == kExprRefTestNull;
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, target_type))) {
            if (rtt.has_value()) {
              // Drop rtt.
              CALL_INTERFACE(Drop);
            }
            // Type checking can still fail for null.
            if (obj.type.is_nullable() && !null_succeeds) {
              // We abuse ref.as_non_null, which isn't otherwise used as a unary
              // operator, as a sentinel for the negation of ref.is_null.
              CALL_INTERFACE(UnOp, kExprRefAsNonNull, obj, &value);
            } else {
              CALL_INTERFACE(Drop);
              CALL_INTERFACE(I32Const, &value, 1);
            }
          } else if (V8_UNLIKELY(TypeCheckAlwaysFails(obj, target_type,
                                                      null_succeeds))) {
            if (rtt.has_value()) {
              // Drop rtt.
              CALL_INTERFACE(Drop);
            }
            CALL_INTERFACE(Drop);
            CALL_INTERFACE(I32Const, &value, 0);
          } else {
            if (rtt.has_value()) {
              // RTT => Cast to concrete (index) type.
              CALL_INTERFACE(RefTest, obj, rtt.value(), &value, null_succeeds);
            } else {
              // No RTT => Cast to abstract (non-index) types.
              CALL_INTERFACE(RefTestAbstract, obj, target_type, &value,
                             null_succeeds);
            }
          }
        }
        Drop(1 + rtt.has_value());
        Push(value);
        return opcode_length;
      }
      case kExprRefTestDeprecated: {
        NON_CONST_ONLY
        IndexImmediate imm(this, this->pc_ + opcode_length, "type index",
                           validate);
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        opcode_length += imm.length;
        Value rtt = CreateValue(ValueType::Rtt(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        Value obj = Peek(1);
        Value value = CreateValue(kWasmI32);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmStructRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmArrayRef, this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj,
                       "subtype of (ref null func), (ref null struct) or (ref "
                       "null array)");
          return 0;
        }
        if (current_code_reachable_and_ok_) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, rtt))) {
            // Drop rtt.
            CALL_INTERFACE(Drop);
            // Type checking can still fail for null.
            if (obj.type.is_nullable()) {
              // We abuse ref.as_non_null, which isn't otherwise used as a unary
              // operator, as a sentinel for the negation of ref.is_null.
              CALL_INTERFACE(UnOp, kExprRefAsNonNull, obj, &value);
            } else {
              CALL_INTERFACE(Drop);
              CALL_INTERFACE(I32Const, &value, 1);
            }
          } else if (V8_UNLIKELY(TypeCheckAlwaysFails(obj, rtt))) {
            CALL_INTERFACE(Drop);
            CALL_INTERFACE(Drop);
            CALL_INTERFACE(I32Const, &value, 0);
          } else {
            CALL_INTERFACE(RefTest, obj, rtt, &value, /*null_succeeds*/ false);
          }
        }
        Drop(2);
        Push(value);
        return opcode_length;
      }
      case kExprRefCastNop: {
        NON_CONST_ONLY
        // Temporary non-standard instruction, for performance experiments.
        if (!VALIDATE(this->enabled_.has_ref_cast_nop())) {
          this->DecodeError(
              "Invalid opcode 0xfb4c (enable with "
              "--experimental-wasm-ref-cast-nop)");
          return 0;
        }
        HeapTypeImmediate imm(this->enabled_, this, this->pc_ + opcode_length,
                              validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        opcode_length += imm.length;
        HeapType target_type = imm.type;
        Value obj = Peek(0);
        if (!VALIDATE((obj.type.is_object_reference() &&
                       IsSameTypeHierarchy(obj.type.heap_type(), target_type,
                                           this->module_)) ||
                      obj.type.is_bottom())) {
          this->DecodeError(
              obj.pc(),
              "Invalid types for %s: %s of type %s has to "
              "be in the same reference type hierarchy as (ref %s)",
              WasmOpcodes::OpcodeName(opcode), SafeOpcodeNameAt(obj.pc()),
              obj.type.name().c_str(), target_type.name().c_str());
          return 0;
        }
        Value value = CreateValue(ValueType::Ref(target_type));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(Forward, obj, &value);
        Drop(obj);
        Push(value);
        return opcode_length;
      }
      case kExprRefCastDeprecated: {
        NON_CONST_ONLY
        IndexImmediate imm(this, this->pc_ + opcode_length, "type index",
                           validate);
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        opcode_length += imm.length;
        Value rtt = CreateValue(ValueType::Rtt(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Push(rtt);
        Value obj = Peek(1);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmStructRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmArrayRef, this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj,
                       "subtype of (ref null func), (ref null struct) or (ref "
                       "null array)");
          return 0;
        }
        // If either value is bottom, we emit the most specific type possible.
        DCHECK(!rtt.type.is_bottom());
        Value value = CreateValue(ValueType::RefMaybeNull(
            imm.index,
            obj.type.is_bottom() ? kNonNullable : obj.type.nullability()));
        if (current_code_reachable_and_ok_) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, rtt))) {
            // Drop the rtt from the stack, then forward the object value to the
            // result.
            CALL_INTERFACE(Drop);
            CALL_INTERFACE(Forward, obj, &value);
          } else if (V8_UNLIKELY(TypeCheckAlwaysFails(obj, rtt))) {
            // Unrelated types. The only way this will not trap is if the object
            // is null.
            if (obj.type.is_nullable()) {
              // Drop rtt from the stack, then assert that obj is null.
              CALL_INTERFACE(Drop);
              CALL_INTERFACE(AssertNullTypecheck, obj, &value);
            } else {
              CALL_INTERFACE(Trap, TrapReason::kTrapIllegalCast);
              // We know that the following code is not reachable, but according
              // to the spec it technically is. Set it to spec-only reachable.
              SetSucceedingCodeDynamicallyUnreachable();
            }
          } else {
            bool null_succeeds = true;
            CALL_INTERFACE(RefCast, obj, rtt, &value, null_succeeds);
          }
        }
        Drop(2);
        Push(value);
        return opcode_length;
      }
      case kExprBrOnCast:
      case kExprBrOnCastNull: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        uint32_t pc_offset = opcode_length + branch_depth.length;

        HeapTypeImmediate imm(this->enabled_, this, this->pc_ + pc_offset,
                              validate);
        this->Validate(this->pc_ + opcode_length, imm);
        if (!VALIDATE(this->ok())) return 0;
        pc_offset += imm.length;

        std::optional<Value> rtt;
        HeapType target_type = imm.type;
        if (imm.type.is_index()) {
          rtt = CreateValue(ValueType::Rtt(imm.type.ref_index()));
          CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.type.ref_index(),
                                             &rtt.value());
          // Differently to other instructions we don't push the RTT yet.
        }

        Value obj = Peek(0);

        if (!VALIDATE((obj.type.is_object_reference() &&
                       IsSameTypeHierarchy(obj.type.heap_type(), target_type,
                                           this->module_)) ||
                      obj.type.is_bottom())) {
          this->DecodeError(
              obj.pc(),
              "Invalid types for %s: %s of type %s has to "
              "be in the same reference type hierarchy as (ref %s)",
              WasmOpcodes::OpcodeName(opcode), SafeOpcodeNameAt(obj.pc()),
              obj.type.name().c_str(), target_type.name().c_str());
          return 0;
        }

        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError("%s must target a branch of arity at least 1",
                            WasmOpcodes::OpcodeName(opcode));
          return 0;
        }
        // Attention: contrary to most other instructions, we modify the
        // stack before calling the interface function. This makes it
        // significantly more convenient to pass around the values that
        // will be on the stack when the branch is taken.
        // TODO(jkummerow): Reconsider this choice.
        Drop(obj);
        bool null_succeeds = opcode == kExprBrOnCastNull;
        Push(CreateValue(ValueType::RefMaybeNull(
            target_type, null_succeeds ? kNullable : kNonNullable)));
        // The {value_on_branch} parameter we pass to the interface must
        // be pointer-identical to the object on the stack.
        Value* value_on_branch = stack_value(1);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, target_type))) {
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);  // rtt
            }
            // The branch will still not be taken on null if not
            // {null_succeeds}.
            if (obj.type.is_nullable() && !null_succeeds) {
              CALL_INTERFACE(BrOnNonNull, obj, value_on_branch,
                             branch_depth.depth, false);
            } else {
              CALL_INTERFACE(Forward, obj, value_on_branch);
              CALL_INTERFACE(BrOrRet, branch_depth.depth, 0);
              // We know that the following code is not reachable, but according
              // to the spec it technically is. Set it to spec-only reachable.
              SetSucceedingCodeDynamicallyUnreachable();
            }
            c->br_merge()->reached = true;
          } else if (V8_LIKELY(!TypeCheckAlwaysFails(obj, target_type,
                                                     null_succeeds))) {
            if (rtt.has_value()) {
              CALL_INTERFACE(BrOnCast, obj, rtt.value(), value_on_branch,
                             branch_depth.depth, null_succeeds);
            } else {
              CALL_INTERFACE(BrOnCastAbstract, obj, target_type,
                             value_on_branch, branch_depth.depth,
                             null_succeeds);
            }
            c->br_merge()->reached = true;
          } else {
            // Otherwise the types are unrelated. Do not branch.
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);  // rtt
            }
          }
        }

        Drop(1);    // value_on_branch
        Push(obj);  // Restore stack state on fallthrough.
        if (current_code_reachable_and_ok_ && null_succeeds) {
          // As null branches, the type on fallthrough will be the non-null
          // variant of the input type.
          // Note that this is handled differently for br_on_cast_fail for which
          // the Forward is handled by TurboFan.
          // TODO(mliedtke): This currently deviates from the spec and is
          // discussed at
          // https://github.com/WebAssembly/gc/issues/342#issuecomment-1354505307.
          stack_value(1)->type = obj.type.AsNonNull();
          CALL_INTERFACE(Forward, obj, stack_value(1));
        }
        return pc_offset;
      }
      case kExprBrOnCastDeprecated: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        uint32_t pc_offset = opcode_length + branch_depth.length;
        IndexImmediate imm(this, this->pc_ + pc_offset, "type index", validate);
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        pc_offset += imm.length;
        Value rtt = CreateValue(ValueType::Rtt(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        // Don't bother pushing the rtt, as we'd drop it again immediately
        // anyway.
        Value obj = Peek(0);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmStructRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmArrayRef, this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj,
                       "subtype of (ref null func), (ref null struct) or (ref "
                       "null array)");
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
        Drop(obj);
        Push(CreateValue(ValueType::Ref(imm.index)));
        // The {value_on_branch} parameter we pass to the interface must
        // be pointer-identical to the object on the stack.
        Value* value_on_branch = stack_value(1);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, rtt))) {
            CALL_INTERFACE(Drop);  // rtt
            // The branch will still not be taken on null.
            if (obj.type.is_nullable()) {
              CALL_INTERFACE(BrOnNonNull, obj, value_on_branch,
                             branch_depth.depth, false);
            } else {
              CALL_INTERFACE(Forward, obj, value_on_branch);
              CALL_INTERFACE(BrOrRet, branch_depth.depth, 0);
              // We know that the following code is not reachable, but according
              // to the spec it technically is. Set it to spec-only reachable.
              SetSucceedingCodeDynamicallyUnreachable();
            }
            c->br_merge()->reached = true;
          } else if (V8_LIKELY(!TypeCheckAlwaysFails(obj, rtt))) {
            bool null_succeeds = false;
            CALL_INTERFACE(BrOnCast, obj, rtt, value_on_branch,
                           branch_depth.depth, null_succeeds);
            c->br_merge()->reached = true;
          } else {
            // Otherwise the types are unrelated. Do not branch.
            CALL_INTERFACE(Drop);  // rtt
          }
        }

        Drop(1);    // value_on_branch
        Push(obj);  // Restore stack state on fallthrough.
        return pc_offset;
      }
      case kExprBrOnCastFail:
      case kExprBrOnCastFailNull: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        uint32_t pc_offset = opcode_length + branch_depth.length;
        HeapTypeImmediate imm(this->enabled_, this, this->pc_ + pc_offset,
                              validate);
        this->Validate(this->pc_ + opcode_length, imm);
        if (!VALIDATE(this->ok())) return 0;
        pc_offset += imm.length;

        std::optional<Value> rtt;
        HeapType target_type = imm.type;
        if (imm.type.is_index()) {
          rtt = CreateValue(ValueType::Rtt(imm.type.ref_index()));
          CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.type.ref_index(),
                                             &rtt.value());
          // Differently to other instructions we don't push the RTT yet.
        }

        Value obj = Peek(0);

        if (!VALIDATE((obj.type.is_object_reference() &&
                       IsSameTypeHierarchy(obj.type.heap_type(), target_type,
                                           this->module_)) ||
                      obj.type.is_bottom())) {
          this->DecodeError(
              obj.pc(),
              "Invalid types for %s: %s of type %s has to "
              "be in the same reference type hierarchy as (ref %s)",
              WasmOpcodes::OpcodeName(opcode), SafeOpcodeNameAt(obj.pc()),
              obj.type.name().c_str(), target_type.name().c_str());
          return 0;
        }

        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError("%s must target a branch of arity at least 1",
                            WasmOpcodes::OpcodeName(opcode));
          return 0;
        }

        bool null_succeeds = opcode == kExprBrOnCastFailNull;
        if (null_succeeds) {
          // If null is treated as a successful cast, then the branch type is
          // guaranteed to be non-null.
          Drop(obj);
          Push(CreateValue(obj.type.AsNonNull()));
        }
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;

        Value result_on_fallthrough = CreateValue(ValueType::RefMaybeNull(
            target_type, (obj.type.is_bottom() || !null_succeeds)
                             ? kNonNullable
                             : obj.type.nullability()));
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          // This logic ensures that code generation can assume that functions
          // can only be cast between compatible types.
          if (V8_UNLIKELY(
                  TypeCheckAlwaysFails(obj, target_type, null_succeeds))) {
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);  // rtt
            }
            // The types are incompatible (i.e. neither of the two types is a
            // subtype of the other). Always branch.
            CALL_INTERFACE(Forward, obj, stack_value(1));
            CALL_INTERFACE(BrOrRet, branch_depth.depth, 0);
            // We know that the following code is not reachable, but according
            // to the spec it technically is. Set it to spec-only reachable.
            SetSucceedingCodeDynamicallyUnreachable();
            c->br_merge()->reached = true;
          } else if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, target_type))) {
            if (rtt.has_value()) {
              CALL_INTERFACE(Drop);  // rtt
            }
            // The branch can still be taken on null.
            if (obj.type.is_nullable() && !null_succeeds) {
              CALL_INTERFACE(BrOnNull, obj, branch_depth.depth, true,
                             &result_on_fallthrough);
              c->br_merge()->reached = true;
            }
            // Otherwise, the type check always succeeds. Do not branch. Also,
            // the object is already on the stack; do not manipulate the stack.
          } else {
            if (rtt.has_value()) {
              CALL_INTERFACE(BrOnCastFail, obj, rtt.value(),
                             &result_on_fallthrough, branch_depth.depth,
                             null_succeeds);
            } else {
              CALL_INTERFACE(BrOnCastFailAbstract, obj, target_type,
                             &result_on_fallthrough, branch_depth.depth,
                             null_succeeds);
            }
            c->br_merge()->reached = true;
          }
        }
        // Make sure the correct value is on the stack state on fallthrough.
        Drop(obj);
        Push(result_on_fallthrough);
        return pc_offset;
      }
      case kExprBrOnCastFailDeprecated: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
        if (!this->Validate(this->pc_ + opcode_length, branch_depth,
                            control_.size())) {
          return 0;
        }
        uint32_t pc_offset = opcode_length + branch_depth.length;
        IndexImmediate imm(this, this->pc_ + pc_offset, "type index", validate);
        if (!this->ValidateType(this->pc_ + opcode_length, imm)) return 0;
        pc_offset += imm.length;
        Value rtt = CreateValue(ValueType::Rtt(imm.index));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(RttCanon, imm.index, &rtt);
        Value obj = Peek(0);
        if (!VALIDATE(IsSubtypeOf(obj.type, kWasmFuncRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmStructRef, this->module_) ||
                      IsSubtypeOf(obj.type, kWasmArrayRef, this->module_) ||
                      obj.type.is_bottom())) {
          PopTypeError(0, obj,
                       "subtype of (ref null func), (ref null struct) or (ref "
                       "null array)");
          return 0;
        }
        Control* c = control_at(branch_depth.depth);
        if (c->br_merge()->arity == 0) {
          this->DecodeError(
              "br_on_cast_fail must target a branch of arity at least 1");
          return 0;
        }

        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        Value result_on_fallthrough = CreateValue(ValueType::Ref(imm.index));
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          // This logic ensures that code generation can assume that functions
          // can only be cast to function types, and data objects to data types.
          if (V8_UNLIKELY(TypeCheckAlwaysFails(obj, rtt))) {
            // Drop {rtt} in the interface.
            CALL_INTERFACE(Drop);
            // Otherwise the types are unrelated. Always branch.
            CALL_INTERFACE(BrOrRet, branch_depth.depth, 0);
            // We know that the following code is not reachable, but according
            // to the spec it technically is. Set it to spec-only reachable.
            SetSucceedingCodeDynamicallyUnreachable();
            c->br_merge()->reached = true;
          } else if (V8_UNLIKELY(TypeCheckAlwaysSucceeds(obj, rtt))) {
            // Drop {rtt} in the interface.
            CALL_INTERFACE(Drop);
            // The branch can still be taken on null.
            if (obj.type.is_nullable()) {
              CALL_INTERFACE(BrOnNull, obj, branch_depth.depth, true,
                             &result_on_fallthrough);
              c->br_merge()->reached = true;
            }
            // Otherwise, the type check always succeeds. Do not branch. Also,
            // the object is already on the stack; do not manipulate the stack.
          } else {
            bool null_succeeds = false;
            CALL_INTERFACE(BrOnCastFail, obj, rtt, &result_on_fallthrough,
                           branch_depth.depth, null_succeeds);
            c->br_merge()->reached = true;
          }
        }
        // Make sure the correct value is on the stack state on fallthrough.
        Drop(obj);
        Push(result_on_fallthrough);
        return pc_offset;
      }
#define ABSTRACT_TYPE_CHECK(h_type)                                            \
  case kExprRefIs##h_type: {                                                   \
    NON_CONST_ONLY                                                             \
    Value arg = Peek(0, 0, kWasmAnyRef);                                       \
    if (!VALIDATE(this->ok())) return 0;                                       \
    Value result = CreateValue(kWasmI32);                                      \
    if (V8_LIKELY(current_code_reachable_and_ok_)) {                           \
      if (IsHeapSubtypeOf(arg.type.heap_type(), HeapType(HeapType::k##h_type), \
                          this->module_)) {                                    \
        if (arg.type.is_nullable()) {                                          \
          /* We abuse ref.as_non_null, which isn't otherwise used as a unary   \
           * operator, as a sentinel for the negation of ref.is_null. */       \
          CALL_INTERFACE(UnOp, kExprRefAsNonNull, arg, &result);               \
        } else {                                                               \
          CALL_INTERFACE(Drop);                                                \
          CALL_INTERFACE(I32Const, &result, 1);                                \
        }                                                                      \
      } else if (!IsHeapSubtypeOf(HeapType(HeapType::k##h_type),               \
                                  arg.type.heap_type(), this->module_)) {      \
        CALL_INTERFACE(Drop);                                                  \
        CALL_INTERFACE(I32Const, &result, 0);                                  \
      } else {                                                                 \
        CALL_INTERFACE(RefIs##h_type, arg, &result);                           \
      }                                                                        \
    }                                                                          \
    Drop(arg);                                                                 \
    Push(result);                                                              \
    return opcode_length;                                                      \
  }
        ABSTRACT_TYPE_CHECK(Struct)
        ABSTRACT_TYPE_CHECK(I31)
        ABSTRACT_TYPE_CHECK(Array)
#undef ABSTRACT_TYPE_CHECK

#define ABSTRACT_TYPE_CAST(h_type)                                             \
  case kExprRefAs##h_type: {                                                   \
    NON_CONST_ONLY                                                             \
    Value arg = Peek(0, 0, kWasmAnyRef);                                       \
    ValueType non_nullable_abstract_type =                                     \
        ValueType::Ref(HeapType::k##h_type);                                   \
    Value result = CreateValue(non_nullable_abstract_type);                    \
    if (V8_LIKELY(current_code_reachable_and_ok_)) {                           \
      if (IsHeapSubtypeOf(arg.type.heap_type(), HeapType(HeapType::k##h_type), \
                          this->module_)) {                                    \
        if (arg.type.is_nullable()) {                                          \
          CALL_INTERFACE(RefAsNonNull, arg, &result);                          \
        } else {                                                               \
          CALL_INTERFACE(Forward, arg, &result);                               \
        }                                                                      \
      } else if (!IsHeapSubtypeOf(HeapType(HeapType::k##h_type),               \
                                  arg.type.heap_type(), this->module_)) {      \
        CALL_INTERFACE(Trap, TrapReason::kTrapIllegalCast);                    \
        /* We know that the following code is not reachable, but according */  \
        /* to the spec it technically is. Set it to spec-only reachable. */    \
        SetSucceedingCodeDynamicallyUnreachable();                             \
      } else {                                                                 \
        CALL_INTERFACE(RefAs##h_type, arg, &result);                           \
      }                                                                        \
    }                                                                          \
    Drop(arg);                                                                 \
    Push(result);                                                              \
    return opcode_length;                                                      \
  }
        ABSTRACT_TYPE_CAST(Struct)
        ABSTRACT_TYPE_CAST(I31)
        ABSTRACT_TYPE_CAST(Array)
#undef ABSTRACT_TYPE_CAST

      case kExprBrOnStruct:
      case kExprBrOnArray:
      case kExprBrOnI31: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
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
            opcode == kExprBrOnStruct  ? HeapType::kStruct
            : opcode == kExprBrOnArray ? HeapType::kArray
                                       : HeapType::kI31;
        Value result_on_branch = CreateValue(ValueType::Ref(heap_type));
        Push(result_on_branch);
        if (!VALIDATE(TypeCheckBranch<true>(c, 0))) return 0;
        // The {value_on_branch} parameter we pass to the interface must be
        // pointer-identical to the object on the stack, so we can't reuse
        // {result_on_branch} which was passed-by-value to {Push}.
        Value* value_on_branch = stack_value(1);
        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          bool null_succeeds = false;
          if (opcode == kExprBrOnStruct) {
            CALL_INTERFACE(BrOnStruct, obj, value_on_branch, branch_depth.depth,
                           null_succeeds);
          } else if (opcode == kExprBrOnArray) {
            CALL_INTERFACE(BrOnArray, obj, value_on_branch, branch_depth.depth,
                           null_succeeds);
          } else {
            CALL_INTERFACE(BrOnI31, obj, value_on_branch, branch_depth.depth,
                           null_succeeds);
          }
          c->br_merge()->reached = true;
        }
        Drop(result_on_branch);
        Push(obj);  // Restore stack state on fallthrough.
        return opcode_length + branch_depth.length;
      }
      case kExprBrOnNonStruct:
      case kExprBrOnNonArray:
      case kExprBrOnNonI31: {
        NON_CONST_ONLY
        BranchDepthImmediate branch_depth(this, this->pc_ + opcode_length,
                                          validate);
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
            opcode == kExprBrOnNonStruct  ? HeapType::kStruct
            : opcode == kExprBrOnNonArray ? HeapType::kArray
                                          : HeapType::kI31;
        Value value_on_fallthrough = CreateValue(ValueType::Ref(heap_type));

        if (V8_LIKELY(current_code_reachable_and_ok_)) {
          bool null_succeeds = false;
          if (opcode == kExprBrOnNonStruct) {
            CALL_INTERFACE(BrOnNonStruct, obj, &value_on_fallthrough,
                           branch_depth.depth, null_succeeds);
          } else if (opcode == kExprBrOnNonArray) {
            CALL_INTERFACE(BrOnNonArray, obj, &value_on_fallthrough,
                           branch_depth.depth, null_succeeds);
          } else {
            CALL_INTERFACE(BrOnNonI31, obj, &value_on_fallthrough,
                           branch_depth.depth, null_succeeds);
          }
          c->br_merge()->reached = true;
        }
        Drop(obj);
        Push(value_on_fallthrough);
        return opcode_length + branch_depth.length;
      }
      case kExprExternInternalize: {
        Value extern_val = Peek(0, 0, kWasmExternRef);
        ValueType intern_type = ValueType::RefMaybeNull(
            HeapType::kAny, Nullability(extern_val.type.is_nullable()));
        Value intern_val = CreateValue(intern_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, kExprExternInternalize,
                                           extern_val, &intern_val);
        Drop(extern_val);
        Push(intern_val);
        return opcode_length;
      }
      case kExprExternExternalize: {
        Value val = Peek(0, 0, kWasmAnyRef);
        ValueType extern_type = ValueType::RefMaybeNull(
            HeapType::kExtern, Nullability(val.type.is_nullable()));
        Value extern_val = CreateValue(extern_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, kExprExternExternalize, val,
                                           &extern_val);
        Drop(val);
        Push(extern_val);
        return opcode_length;
      }
      default:
        this->DecodeError("invalid gc opcode: %x", opcode);
        return 0;
    }
  }

  enum class WasmArrayAccess { kRead, kWrite };

  int DecodeStringNewWtf8(unibrow::Utf8Variant variant,
                          uint32_t opcode_length) {
    NON_CONST_ONLY
    bool null_on_invalid = variant == unibrow::Utf8Variant::kUtf8NoTrap;
    MemoryIndexImmediate memory(this, this->pc_ + opcode_length, validate);
    if (!this->Validate(this->pc_ + opcode_length, memory)) return 0;
    ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value offset = Peek(1, 0, addr_type);
    Value size = Peek(0, 1, kWasmI32);
    Value result = CreateValue(ValueType::RefMaybeNull(
        HeapType::kString, null_on_invalid ? kNullable : kNonNullable));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringNewWtf8, memory, variant, offset,
                                       size, &result);
    Drop(2);
    Push(result);
    return opcode_length + memory.length;
  }

  int DecodeStringMeasureWtf8(unibrow::Utf8Variant variant,
                              uint32_t opcode_length) {
    NON_CONST_ONLY
    Value str = Peek(0, 0, kWasmStringRef);
    Value result = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringMeasureWtf8, variant, str,
                                       &result);
    Drop(str);
    Push(result);
    return opcode_length;
  }

  int DecodeStringEncodeWtf8(unibrow::Utf8Variant variant,
                             uint32_t opcode_length) {
    NON_CONST_ONLY
    MemoryIndexImmediate memory(this, this->pc_ + opcode_length, validate);
    if (!this->Validate(this->pc_ + opcode_length, memory)) return 0;
    ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value str = Peek(1, 0, kWasmStringRef);
    Value addr = Peek(0, 1, addr_type);
    Value result = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringEncodeWtf8, memory, variant, str,
                                       addr, &result);
    Drop(2);
    Push(result);
    return opcode_length + memory.length;
  }

  int DecodeStringViewWtf8Encode(unibrow::Utf8Variant variant,
                                 uint32_t opcode_length) {
    NON_CONST_ONLY
    MemoryIndexImmediate memory(this, this->pc_ + opcode_length, validate);
    if (!this->Validate(this->pc_ + opcode_length, memory)) return 0;
    ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    Value view = Peek(3, 0, kWasmStringViewWtf8);
    Value addr = Peek(2, 1, addr_type);
    Value pos = Peek(1, 2, kWasmI32);
    Value bytes = Peek(0, 3, kWasmI32);
    Value next_pos = CreateValue(kWasmI32);
    Value bytes_out = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf8Encode, memory, variant,
                                       view, addr, pos, bytes, &next_pos,
                                       &bytes_out);
    Drop(4);
    Push(next_pos);
    Push(bytes_out);
    return opcode_length + memory.length;
  }

  int DecodeStringNewWtf8Array(unibrow::Utf8Variant variant,
                               uint32_t opcode_length) {
    NON_CONST_ONLY
    Value array = PeekPackedArray(2, 0, kWasmI8, WasmArrayAccess::kRead);
    Value start = Peek(1, 1, kWasmI32);
    Value end = Peek(0, 2, kWasmI32);
    bool null_on_invalid = variant == unibrow::Utf8Variant::kUtf8NoTrap;
    Value result = CreateValue(ValueType::RefMaybeNull(
        HeapType::kString, null_on_invalid ? kNullable : kNonNullable));
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringNewWtf8Array, variant, array,
                                       start, end, &result);
    Drop(3);
    Push(result);
    return opcode_length;
  }

  int DecodeStringEncodeWtf8Array(unibrow::Utf8Variant variant,
                                  uint32_t opcode_length) {
    NON_CONST_ONLY
    Value str = Peek(2, 0, kWasmStringRef);
    Value array = PeekPackedArray(1, 1, kWasmI8, WasmArrayAccess::kWrite);
    Value start = Peek(0, 2, kWasmI32);
    Value result = CreateValue(kWasmI32);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(StringEncodeWtf8Array, variant, str,
                                       array, start, &result);
    Drop(3);
    Push(result);
    return opcode_length;
  }

  int DecodeStringRefOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    // Fast check for out-of-range opcodes (only allow 0xfbXX).
    // This might help the big switch below.
    if (!VALIDATE((opcode >> 8) == kGCPrefix)) {
      this->DecodeError("invalid stringref opcode: %x", opcode);
      return 0;
    }

    switch (opcode) {
      case kExprStringNewUtf8:
        return DecodeStringNewWtf8(unibrow::Utf8Variant::kUtf8, opcode_length);
      case kExprStringNewUtf8Try:
        return DecodeStringNewWtf8(unibrow::Utf8Variant::kUtf8NoTrap,
                                   opcode_length);
      case kExprStringNewLossyUtf8:
        return DecodeStringNewWtf8(unibrow::Utf8Variant::kLossyUtf8,
                                   opcode_length);
      case kExprStringNewWtf8:
        return DecodeStringNewWtf8(unibrow::Utf8Variant::kWtf8, opcode_length);
      case kExprStringNewWtf16: {
        NON_CONST_ONLY
        MemoryIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value offset = Peek(1, 0, addr_type);
        Value size = Peek(0, 1, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringNewWtf16, imm, offset, size,
                                           &result);
        Drop(2);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprStringConst: {
        StringConstImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringConst, imm, &result);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprStringMeasureUtf8:
        return DecodeStringMeasureWtf8(unibrow::Utf8Variant::kUtf8,
                                       opcode_length);
      case kExprStringMeasureWtf8:
        return DecodeStringMeasureWtf8(unibrow::Utf8Variant::kWtf8,
                                       opcode_length);
      case kExprStringMeasureWtf16: {
        NON_CONST_ONLY
        Value str = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringMeasureWtf16, str, &result);
        Drop(str);
        Push(result);
        return opcode_length;
      }
      case kExprStringEncodeUtf8:
        return DecodeStringEncodeWtf8(unibrow::Utf8Variant::kUtf8,
                                      opcode_length);
      case kExprStringEncodeLossyUtf8:
        return DecodeStringEncodeWtf8(unibrow::Utf8Variant::kLossyUtf8,
                                      opcode_length);
      case kExprStringEncodeWtf8:
        return DecodeStringEncodeWtf8(unibrow::Utf8Variant::kWtf8,
                                      opcode_length);
      case kExprStringEncodeWtf16: {
        NON_CONST_ONLY
        MemoryIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value str = Peek(1, 0, kWasmStringRef);
        Value addr = Peek(0, 1, addr_type);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringEncodeWtf16, imm, str, addr,
                                           &result);
        Drop(2);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprStringConcat: {
        NON_CONST_ONLY
        Value head = Peek(1, 0, kWasmStringRef);
        Value tail = Peek(0, 1, kWasmStringRef);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringConcat, head, tail, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringEq: {
        NON_CONST_ONLY
        Value a = Peek(1, 0, kWasmStringRef);
        Value b = Peek(0, 1, kWasmStringRef);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringEq, a, b, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringIsUSVSequence: {
        NON_CONST_ONLY
        Value str = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringIsUSVSequence, str, &result);
        Drop(1);
        Push(result);
        return opcode_length;
      }
      case kExprStringAsWtf8: {
        NON_CONST_ONLY
        Value str = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(ValueType::Ref(HeapType::kStringViewWtf8));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringAsWtf8, str, &result);
        Drop(str);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewWtf8Advance: {
        NON_CONST_ONLY
        Value view = Peek(2, 0, kWasmStringViewWtf8);
        Value pos = Peek(1, 1, kWasmI32);
        Value bytes = Peek(0, 2, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf8Advance, view, pos,
                                           bytes, &result);
        Drop(3);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewWtf8EncodeUtf8:
        return DecodeStringViewWtf8Encode(unibrow::Utf8Variant::kUtf8,
                                          opcode_length);
      case kExprStringViewWtf8EncodeLossyUtf8:
        return DecodeStringViewWtf8Encode(unibrow::Utf8Variant::kLossyUtf8,
                                          opcode_length);
      case kExprStringViewWtf8EncodeWtf8:
        return DecodeStringViewWtf8Encode(unibrow::Utf8Variant::kWtf8,
                                          opcode_length);
      case kExprStringViewWtf8Slice: {
        NON_CONST_ONLY
        Value view = Peek(2, 0, kWasmStringViewWtf8);
        Value start = Peek(1, 1, kWasmI32);
        Value end = Peek(0, 2, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf8Slice, view, start,
                                           end, &result);
        Drop(3);
        Push(result);
        return opcode_length;
      }
      case kExprStringAsWtf16: {
        NON_CONST_ONLY
        Value str = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(ValueType::Ref(HeapType::kStringViewWtf16));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringAsWtf16, str, &result);
        Drop(str);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewWtf16Length: {
        NON_CONST_ONLY
        Value view = Peek(0, 0, kWasmStringViewWtf16);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringMeasureWtf16, view, &result);
        Drop(view);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewWtf16GetCodeUnit: {
        NON_CONST_ONLY
        Value view = Peek(1, 0, kWasmStringViewWtf16);
        Value pos = Peek(0, 1, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf16GetCodeUnit, view,
                                           pos, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewWtf16Encode: {
        NON_CONST_ONLY
        MemoryIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType addr_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value view = Peek(3, 0, kWasmStringViewWtf16);
        Value addr = Peek(2, 1, addr_type);
        Value pos = Peek(1, 2, kWasmI32);
        Value codeunits = Peek(0, 3, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf16Encode, imm, view,
                                           addr, pos, codeunits, &result);
        Drop(4);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprStringViewWtf16Slice: {
        NON_CONST_ONLY
        Value view = Peek(2, 0, kWasmStringViewWtf16);
        Value start = Peek(1, 1, kWasmI32);
        Value end = Peek(0, 2, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewWtf16Slice, view, start,
                                           end, &result);
        Drop(3);
        Push(result);
        return opcode_length;
      }
      case kExprStringAsIter: {
        NON_CONST_ONLY
        Value str = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(ValueType::Ref(HeapType::kStringViewIter));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringAsIter, str, &result);
        Drop(str);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewIterNext: {
        NON_CONST_ONLY
        Value view = Peek(0, 0, kWasmStringViewIter);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewIterNext, view, &result);
        Drop(view);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewIterAdvance: {
        NON_CONST_ONLY
        Value view = Peek(1, 0, kWasmStringViewIter);
        Value codepoints = Peek(0, 1, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewIterAdvance, view,
                                           codepoints, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewIterRewind: {
        NON_CONST_ONLY
        Value view = Peek(1, 0, kWasmStringViewIter);
        Value codepoints = Peek(0, 1, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewIterRewind, view,
                                           codepoints, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringViewIterSlice: {
        NON_CONST_ONLY
        Value view = Peek(1, 0, kWasmStringViewIter);
        Value codepoints = Peek(0, 1, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringViewIterSlice, view,
                                           codepoints, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringNewUtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringNewWtf8Array(unibrow::Utf8Variant::kUtf8,
                                        opcode_length);
      case kExprStringNewUtf8ArrayTry:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringNewWtf8Array(unibrow::Utf8Variant::kUtf8NoTrap,
                                        opcode_length);
      case kExprStringNewLossyUtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringNewWtf8Array(unibrow::Utf8Variant::kLossyUtf8,
                                        opcode_length);
      case kExprStringNewWtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringNewWtf8Array(unibrow::Utf8Variant::kWtf8,
                                        opcode_length);
      case kExprStringNewWtf16Array: {
        CHECK_PROTOTYPE_OPCODE(gc);
        NON_CONST_ONLY
        Value array = PeekPackedArray(2, 0, kWasmI16, WasmArrayAccess::kRead);
        Value start = Peek(1, 1, kWasmI32);
        Value end = Peek(0, 2, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringNewWtf16Array, array, start,
                                           end, &result);
        Drop(3);
        Push(result);
        return opcode_length;
      }
      case kExprStringEncodeUtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringEncodeWtf8Array(unibrow::Utf8Variant::kUtf8,
                                           opcode_length);
      case kExprStringEncodeLossyUtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringEncodeWtf8Array(unibrow::Utf8Variant::kLossyUtf8,
                                           opcode_length);
      case kExprStringEncodeWtf8Array:
        CHECK_PROTOTYPE_OPCODE(gc);
        return DecodeStringEncodeWtf8Array(unibrow::Utf8Variant::kWtf8,
                                           opcode_length);
      case kExprStringEncodeWtf16Array: {
        CHECK_PROTOTYPE_OPCODE(gc);
        NON_CONST_ONLY
        Value str = Peek(2, 0, kWasmStringRef);
        Value array = PeekPackedArray(1, 1, kWasmI16, WasmArrayAccess::kWrite);
        Value start = Peek(0, 2, kWasmI32);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringEncodeWtf16Array, str, array,
                                           start, &result);
        Drop(3);
        Push(result);
        return opcode_length;
      }
      case kExprStringCompare: {
        NON_CONST_ONLY
        Value lhs = Peek(1, 0, kWasmStringRef);
        Value rhs = Peek(0, 1, kWasmStringRef);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringCompare, lhs, rhs, &result);
        Drop(2);
        Push(result);
        return opcode_length;
      }
      case kExprStringFromCodePoint: {
        NON_CONST_ONLY
        Value code_point = Peek(0, 0, kWasmI32);
        Value result = CreateValue(ValueType::Ref(HeapType::kString));
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringFromCodePoint, code_point,
                                           &result);
        Drop(1);
        Push(result);
        return opcode_length;
      }
      case kExprStringHash: {
        NON_CONST_ONLY
        Value string = Peek(0, 0, kWasmStringRef);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(StringHash, string, &result);
        Drop(1);
        Push(result);
        return opcode_length;
      }
      default:
        this->DecodeError("invalid stringref opcode: %x", opcode);
        return 0;
    }
  }
#undef NON_CONST_ONLY

  uint32_t DecodeAtomicOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    // Fast check for out-of-range opcodes (only allow 0xfeXX).
    if (!VALIDATE((opcode >> 8) == kAtomicPrefix)) {
      this->DecodeError("invalid atomic opcode: 0x%x", opcode);
      return 0;
    }

    MachineType memtype;
    switch (opcode) {
#define CASE_ATOMIC_STORE_OP(Name, Type)          \
  case kExpr##Name: {                             \
    memtype = MachineType::Type();                \
    break; /* to generic mem access code below */ \
  }
      ATOMIC_STORE_OP_LIST(CASE_ATOMIC_STORE_OP)
#undef CASE_ATOMIC_OP
#define CASE_ATOMIC_OP(Name, Type)                \
  case kExpr##Name: {                             \
    memtype = MachineType::Type();                \
    break; /* to generic mem access code below */ \
  }
      ATOMIC_OP_LIST(CASE_ATOMIC_OP)
#undef CASE_ATOMIC_OP
      case kExprAtomicFence: {
        byte zero = this->template read_u8<ValidationTag>(
            this->pc_ + opcode_length, "zero");
        if (!VALIDATE(zero == 0)) {
          this->DecodeError(this->pc_ + opcode_length,
                            "invalid atomic operand");
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(AtomicFence);
        return 1 + opcode_length;
      }
      default:
        // This path is only possible if we are validating.
        V8_ASSUME(ValidationTag::validate);
        this->DecodeError("invalid atomic opcode: 0x%x", opcode);
        return 0;
    }

    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    V8_ASSUME(sig != nullptr);

    MemoryAccessImmediate imm = MakeMemoryAccessImmediate(
        opcode_length, ElementSizeLog2Of(memtype.representation()));
    if (!this->Validate(this->pc_ + opcode_length, imm)) return false;

    int parameter_count = static_cast<int>(sig->parameter_count());
    DCHECK_LE(1, parameter_count);
    DCHECK_EQ(kWasmI32, sig->GetParam(0));
    EnsureStackArguments(parameter_count);
    ArgVector args(stack_value(parameter_count), parameter_count);
    ValueType mem_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
    ValidateArgType(args, 0, mem_type);
    for (int i = 1; i < parameter_count; i++) {
      ValidateArgType(args, i, sig->GetParam(i));
    }

    base::Optional<Value> result;
    if (sig->return_count()) {
      DCHECK_EQ(1, sig->return_count());
      result = CreateValue(sig->GetReturn());
    }

    if (V8_LIKELY(!CheckStaticallyOutOfBounds(memtype.MemSize(), imm.offset))) {
      CALL_INTERFACE_IF_OK_AND_REACHABLE(
          AtomicOp, opcode, base::VectorOf(args), imm,
          result.has_value() ? &result.value() : nullptr);
    }
    DropArgs(sig);
    if (result.has_value()) Push(result.value());
    return opcode_length + imm.length;
  }

  unsigned DecodeNumericOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    // Fast check for out-of-range opcodes (only allow 0xfcXX).
    // This avoids a dynamic check in signature lookup, and might also help the
    // big switch below.
    if (!VALIDATE((opcode >> 8) == kNumericPrefix)) {
      this->DecodeError("invalid numeric opcode: 0x%x", opcode);
      return 0;
    }

    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
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
        MemoryInitImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType mem_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value size = Peek(0, 2, kWasmI32);
        Value offset = Peek(1, 1, kWasmI32);
        Value dst = Peek(2, 0, mem_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryInit, imm, dst, offset, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprDataDrop: {
        IndexImmediate imm(this, this->pc_ + opcode_length,
                           "data segment index", validate);
        if (!this->ValidateDataSegment(this->pc_ + opcode_length, imm)) {
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(DataDrop, imm);
        return opcode_length + imm.length;
      }
      case kExprMemoryCopy: {
        MemoryCopyImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType mem_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value size = Peek(0, 2, mem_type);
        Value src = Peek(1, 1, mem_type);
        Value dst = Peek(2, 0, mem_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryCopy, imm, dst, src, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprMemoryFill: {
        MemoryIndexImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ValueType mem_type = this->module_->is_memory64 ? kWasmI64 : kWasmI32;
        Value size = Peek(0, 2, mem_type);
        Value value = Peek(1, 1, kWasmI32);
        Value dst = Peek(2, 0, mem_type);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(MemoryFill, imm, dst, value, size);
        Drop(3);
        return opcode_length + imm.length;
      }
      case kExprTableInit: {
        TableInitImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ArgVector args = PeekArgs(sig);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableInit, imm,
                                           base::VectorOf(args));
        DropArgs(sig);
        return opcode_length + imm.length;
      }
      case kExprElemDrop: {
        IndexImmediate imm(this, this->pc_ + opcode_length,
                           "element segment index", validate);
        if (!this->ValidateElementSegment(this->pc_ + opcode_length, imm)) {
          return 0;
        }
        CALL_INTERFACE_IF_OK_AND_REACHABLE(ElemDrop, imm);
        return opcode_length + imm.length;
      }
      case kExprTableCopy: {
        TableCopyImmediate imm(this, this->pc_ + opcode_length, validate);
        if (!this->Validate(this->pc_ + opcode_length, imm)) return 0;
        ArgVector args = PeekArgs(sig);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableCopy, imm,
                                           base::VectorOf(args));
        DropArgs(sig);
        return opcode_length + imm.length;
      }
      case kExprTableGrow: {
        IndexImmediate imm(this, this->pc_ + opcode_length, "table index",
                           validate);
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value delta = Peek(0, 1, kWasmI32);
        Value value = Peek(1, 0, this->module_->tables[imm.index].type);
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableGrow, imm, value, delta,
                                           &result);
        Drop(2);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprTableSize: {
        IndexImmediate imm(this, this->pc_ + opcode_length, "table index",
                           validate);
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value result = CreateValue(kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableSize, imm, &result);
        Push(result);
        return opcode_length + imm.length;
      }
      case kExprTableFill: {
        IndexImmediate imm(this, this->pc_ + opcode_length, "table index",
                           validate);
        if (!this->ValidateTable(this->pc_ + opcode_length, imm)) return 0;
        Value count = Peek(0, 2, kWasmI32);
        Value value = Peek(1, 1, this->module_->tables[imm.index].type);
        Value start = Peek(2, 0, kWasmI32);
        CALL_INTERFACE_IF_OK_AND_REACHABLE(TableFill, imm, start, value, count);
        Drop(3);
        return opcode_length + imm.length;
      }
      default:
        this->DecodeError("invalid numeric opcode: 0x%x", opcode);
        return 0;
    }
  }

  V8_INLINE Value CreateValue(ValueType type) { return Value{this->pc_, type}; }

  V8_INLINE void Push(Value value) {
    DCHECK_NE(kWasmVoid, value.type);
    // {stack_.EnsureMoreCapacity} should have been called before, either in the
    // central decoding loop, or individually if more than one element is
    // pushed.
    stack_.push(value);
  }

  void PushMergeValues(Control* c, Merge<Value>* merge) {
    if constexpr (decoding_mode == kConstantExpression) return;
    DCHECK_EQ(c, &control_.back());
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    stack_.shrink_to(c->stack_depth);
    if (merge->arity == 1) {
      // {stack_.EnsureMoreCapacity} should have been called before in the
      // central decoding loop.
      stack_.push(merge->vals.first);
    } else {
      stack_.EnsureMoreCapacity(merge->arity, this->zone_);
      for (uint32_t i = 0; i < merge->arity; i++) {
        stack_.push(merge->vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + merge->arity, stack_.size());
  }

  V8_INLINE ReturnVector CreateReturnValues(const FunctionSig* sig) {
    size_t return_count = sig->return_count();
    ReturnVector values(return_count);
    std::transform(sig->returns().begin(), sig->returns().end(), values.begin(),
                   [this](ValueType type) { return CreateValue(type); });
    return values;
  }
  V8_INLINE void PushReturns(ReturnVector values) {
    stack_.EnsureMoreCapacity(static_cast<int>(values.size()), this->zone_);
    for (Value& value : values) Push(value);
  }

  // We do not inline these functions because doing so causes a large binary
  // size increase. Not inlining them should not create a performance
  // degradation, because their invocations are guarded by V8_LIKELY.
  V8_NOINLINE V8_PRESERVE_MOST void PopTypeError(int index, Value val,
                                                 const char* expected) {
    this->DecodeError(val.pc(), "%s[%d] expected %s, found %s of type %s",
                      SafeOpcodeNameAt(this->pc_), index, expected,
                      SafeOpcodeNameAt(val.pc()), val.type.name().c_str());
  }

  V8_NOINLINE V8_PRESERVE_MOST void PopTypeError(int index, Value val,
                                                 std::string expected) {
    PopTypeError(index, val, expected.c_str());
  }

  V8_NOINLINE V8_PRESERVE_MOST void PopTypeError(int index, Value val,
                                                 ValueType expected) {
    PopTypeError(index, val, ("type " + expected.name()).c_str());
  }

  V8_NOINLINE V8_PRESERVE_MOST void NotEnoughArgumentsError(int needed,
                                                            int actual) {
    DCHECK_LT(0, needed);
    DCHECK_LE(0, actual);
    DCHECK_LT(actual, needed);
    this->DecodeError(
        "not enough arguments on the stack for %s (need %d, got %d)",
        SafeOpcodeNameAt(this->pc_), needed, actual);
  }

  V8_INLINE Value Peek(int depth, int index, ValueType expected) {
    Value val = Peek(depth);
    if (!VALIDATE(IsSubtypeOf(val.type, expected, this->module_) ||
                  val.type == kWasmBottom || expected == kWasmBottom)) {
      PopTypeError(index, val, expected);
    }
    return val;
  }

  V8_INLINE Value Peek(int depth) {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (V8_UNLIKELY(stack_.size() <= limit + depth)) {
      // Peeking past the current control start in reachable code.
      if (!VALIDATE(decoding_mode == kFunctionBody &&
                    control_.back().unreachable())) {
        NotEnoughArgumentsError(depth + 1, stack_.size() - limit);
      }
      return UnreachableValue(this->pc_);
    }
    DCHECK_LT(depth, stack_.size());
    return *(stack_.end() - depth - 1);
  }

  Value PeekPackedArray(uint32_t stack_depth, uint32_t operand_index,
                        ValueType expected_element_type,
                        WasmArrayAccess access) {
    Value array = Peek(stack_depth);
    if (array.type.is_bottom()) {
      // We are in a polymorphic stack. Leave the stack as it is.
      DCHECK(!current_code_reachable_and_ok_);
      return array;
    }
    if (VALIDATE(array.type.is_object_reference() && array.type.has_index())) {
      uint32_t ref_index = array.type.ref_index();
      if (VALIDATE(this->module_->has_array(ref_index))) {
        const ArrayType* array_type = this->module_->array_type(ref_index);
        if (VALIDATE(array_type->element_type() == expected_element_type &&
                     (access == WasmArrayAccess::kRead ||
                      array_type->mutability()))) {
          return array;
        }
      }
    }
    PopTypeError(operand_index, array,
                 (std::string("array of ") +
                  (access == WasmArrayAccess::kWrite ? "mutable " : "") +
                  expected_element_type.name())
                     .c_str());
    return array;
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
    if (V8_UNLIKELY(stack_.size() < limit + count)) {
      // Pop what we can.
      count = std::min(count, static_cast<int>(stack_.size() - limit));
    }
    stack_.pop(count);
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
    constexpr const char* merge_description =
        merge_type == kBranchMerge     ? "branch"
        : merge_type == kReturnMerge   ? "return"
        : merge_type == kInitExprMerge ? "constant expression"
                                       : "fallthrough";
    uint32_t arity = merge->arity;
    uint32_t actual = stack_.size() - control_.back().stack_depth;
    // Here we have to check for !unreachable(), because we need to typecheck as
    // if the current code is reachable even if it is spec-only reachable.
    if (V8_LIKELY(decoding_mode == kConstantExpression ||
                  !control_.back().unreachable())) {
      if (V8_UNLIKELY(strict_count ? actual != drop_values + arity
                                   : actual < drop_values + arity)) {
        this->DecodeError("expected %u elements on the stack for %s, found %u",
                          arity, merge_description,
                          actual >= drop_values ? actual - drop_values : 0);
        return false;
      }
      // Typecheck the topmost {merge->arity} values on the stack.
      Value* stack_values = stack_.end() - (arity + drop_values);
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
    if constexpr (push_branch_values) {
      uint32_t inserted_value_count =
          static_cast<uint32_t>(EnsureStackArguments(drop_values + arity));
      if (inserted_value_count > 0) {
        // stack_.EnsureMoreCapacity() may have inserted unreachable values into
        // the bottom of the stack. If so, mark them with the correct type. If
        // drop values were also inserted, disregard them, as they will be
        // dropped anyway.
        Value* stack_base = stack_value(drop_values + arity);
        for (uint32_t i = 0; i < std::min(arity, inserted_value_count); i++) {
          if (stack_base[i].type == kWasmBottom) {
            stack_base[i].type = (*merge)[i].type;
          }
        }
      }
    }
    return VALIDATE(this->ok());
  }

  template <StackElementsCountMode strict_count, MergeType merge_type>
  bool DoReturn() {
    if (!VALIDATE((TypeCheckStackAgainstMerge<strict_count, false, merge_type>(
            0, &control_.front().end_merge)))) {
      return false;
    }
    DCHECK_IMPLIES(current_code_reachable_and_ok_,
                   stack_.size() >= this->sig_->return_count());
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
    if (sig->parameter_count() == 1) {
      // All current simple unary operators have exactly 1 return value.
      DCHECK_EQ(1, sig->return_count());
      return BuildSimpleOperator(opcode, sig->GetReturn(0), sig->GetParam(0));
    } else {
      DCHECK_EQ(2, sig->parameter_count());
      ValueType ret = sig->return_count() == 0 ? kWasmVoid : sig->GetReturn(0);
      return BuildSimpleOperator(opcode, ret, sig->GetParam(0),
                                 sig->GetParam(1));
    }
  }

  int BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                          ValueType arg_type) {
    DCHECK_NE(kWasmVoid, return_type);
    Value val = Peek(0, 0, arg_type);
    Value ret = CreateValue(return_type);
    CALL_INTERFACE_IF_OK_AND_REACHABLE(UnOp, opcode, val, &ret);
    Drop(val);
    Push(ret);
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

  static constexpr ValidationTag validate = {};
};

class EmptyInterface {
 public:
  using ValidationTag = Decoder::FullValidationTag;
  static constexpr DecodingMode decoding_mode = kFunctionBody;
  using Value = ValueBase<ValidationTag>;
  using Control = ControlBase<Value, ValidationTag>;
  using FullDecoder = WasmFullDecoder<ValidationTag, EmptyInterface>;

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
