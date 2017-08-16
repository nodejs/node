// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
#define V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_

#include "src/wasm/decoder.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmGlobal;

// Use this macro to check a condition if checked == true, and DCHECK the
// condition otherwise.
#define CHECKED_COND(cond)   \
  (checked ? (cond) : ([&] { \
    DCHECK(cond);            \
    return true;             \
  })())

// Helpers for decoding different kinds of operands which follow bytecodes.
template <bool checked>
struct LocalIndexOperand {
  uint32_t index;
  ValueType type = kWasmStmt;
  unsigned length;

  inline LocalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<checked>(pc + 1, &length, "local index");
  }
};

template <bool checked>
struct ImmI32Operand {
  int32_t value;
  unsigned length;
  inline ImmI32Operand(Decoder* decoder, const byte* pc) {
    value = decoder->read_i32v<checked>(pc + 1, &length, "immi32");
  }
};

template <bool checked>
struct ImmI64Operand {
  int64_t value;
  unsigned length;
  inline ImmI64Operand(Decoder* decoder, const byte* pc) {
    value = decoder->read_i64v<checked>(pc + 1, &length, "immi64");
  }
};

template <bool checked>
struct ImmF32Operand {
  float value;
  unsigned length = 4;
  inline ImmF32Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint32_t tmp = decoder->read_u32<checked>(pc + 1, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <bool checked>
struct ImmF64Operand {
  double value;
  unsigned length = 8;
  inline ImmF64Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->read_u64<checked>(pc + 1, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <bool checked>
struct GlobalIndexOperand {
  uint32_t index;
  ValueType type = kWasmStmt;
  const WasmGlobal* global = nullptr;
  unsigned length;

  inline GlobalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<checked>(pc + 1, &length, "global index");
  }
};

template <bool checked>
struct BlockTypeOperand {
  uint32_t arity = 0;
  const byte* types = nullptr;  // pointer to encoded types for the block.
  unsigned length = 1;

  inline BlockTypeOperand(Decoder* decoder, const byte* pc) {
    uint8_t val = decoder->read_u8<checked>(pc + 1, "block type");
    ValueType type = kWasmStmt;
    if (decode_local_type(val, &type)) {
      arity = type == kWasmStmt ? 0 : 1;
      types = pc + 1;
    } else {
      // Handle multi-value blocks.
      if (!CHECKED_COND(FLAG_wasm_mv_prototype)) {
        decoder->error(pc + 1, "invalid block arity > 1");
        return;
      }
      if (!CHECKED_COND(val == kMultivalBlock)) {
        decoder->error(pc + 1, "invalid block type");
        return;
      }
      // Decode and check the types vector of the block.
      unsigned len = 0;
      uint32_t count = decoder->read_u32v<checked>(pc + 2, &len, "block arity");
      // {count} is encoded as {arity-2}, so that a {0} count here corresponds
      // to a block with 2 values. This makes invalid/redundant encodings
      // impossible.
      arity = count + 2;
      length = 1 + len + arity;
      types = pc + 1 + 1 + len;

      for (uint32_t i = 0; i < arity; i++) {
        uint32_t offset = 1 + 1 + len + i;
        val = decoder->read_u8<checked>(pc + offset, "block type");
        decode_local_type(val, &type);
        if (!CHECKED_COND(type != kWasmStmt)) {
          decoder->error(pc + offset, "invalid block type");
          return;
        }
      }
    }
  }

  // Decode a byte representing a local type. Return {false} if the encoded
  // byte was invalid or {kMultivalBlock}.
  inline bool decode_local_type(uint8_t val, ValueType* result) {
    switch (static_cast<ValueTypeCode>(val)) {
      case kLocalVoid:
        *result = kWasmStmt;
        return true;
      case kLocalI32:
        *result = kWasmI32;
        return true;
      case kLocalI64:
        *result = kWasmI64;
        return true;
      case kLocalF32:
        *result = kWasmF32;
        return true;
      case kLocalF64:
        *result = kWasmF64;
        return true;
      case kLocalS128:
        *result = kWasmS128;
        return true;
      case kLocalS1x4:
        *result = kWasmS1x4;
        return true;
      case kLocalS1x8:
        *result = kWasmS1x8;
        return true;
      case kLocalS1x16:
        *result = kWasmS1x16;
        return true;
      default:
        *result = kWasmStmt;
        return false;
    }
  }
  ValueType read_entry(unsigned index) {
    DCHECK_LT(index, arity);
    ValueType result;
    CHECK(decode_local_type(types[index], &result));
    return result;
  }
};

struct Control;
template <bool checked>
struct BreakDepthOperand {
  uint32_t depth;
  Control* target = nullptr;
  unsigned length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    depth = decoder->read_u32v<checked>(pc + 1, &length, "break depth");
  }
};

template <bool checked>
struct CallIndirectOperand {
  uint32_t table_index;
  uint32_t index;
  FunctionSig* sig = nullptr;
  unsigned length;
  inline CallIndirectOperand(Decoder* decoder, const byte* pc) {
    unsigned len = 0;
    index = decoder->read_u32v<checked>(pc + 1, &len, "signature index");
    table_index = decoder->read_u8<checked>(pc + 1 + len, "table index");
    if (!CHECKED_COND(table_index == 0)) {
      decoder->errorf(pc + 1 + len, "expected table index 0, found %u",
                      table_index);
    }
    length = 1 + len;
  }
};

template <bool checked>
struct CallFunctionOperand {
  uint32_t index;
  FunctionSig* sig = nullptr;
  unsigned length;
  inline CallFunctionOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<checked>(pc + 1, &length, "function index");
  }
};

template <bool checked>
struct MemoryIndexOperand {
  uint32_t index;
  unsigned length = 1;
  inline MemoryIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<checked>(pc + 1, "memory index");
    if (!CHECKED_COND(index == 0)) {
      decoder->errorf(pc + 1, "expected memory index 0, found %u", index);
    }
  }
};

template <bool checked>
struct BranchTableOperand {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  inline BranchTableOperand(Decoder* decoder, const byte* pc) {
    DCHECK_EQ(kExprBrTable, decoder->read_u8<checked>(pc, "opcode"));
    start = pc + 1;
    unsigned len = 0;
    table_count = decoder->read_u32v<checked>(pc + 1, &len, "table count");
    table = pc + 1 + len;
  }
};

// A helper to iterate over a branch table.
template <bool checked>
class BranchTableIterator {
 public:
  unsigned cur_index() { return index_; }
  bool has_next() { return decoder_->ok() && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    unsigned length;
    uint32_t result =
        decoder_->read_u32v<checked>(pc_, &length, "branch table entry");
    pc_ += length;
    return result;
  }
  // length, including the length of the {BranchTableOperand}, but not the
  // opcode.
  unsigned length() {
    while (has_next()) next();
    return static_cast<unsigned>(pc_ - start_);
  }
  const byte* pc() { return pc_; }

  BranchTableIterator(Decoder* decoder, BranchTableOperand<checked>& operand)
      : decoder_(decoder),
        start_(operand.start),
        pc_(operand.table),
        index_(0),
        table_count_(operand.table_count) {}

 private:
  Decoder* decoder_;
  const byte* start_;
  const byte* pc_;
  uint32_t index_;        // the current index.
  uint32_t table_count_;  // the count of entries, not including default.
};

template <bool checked>
struct MemoryAccessOperand {
  uint32_t alignment;
  uint32_t offset;
  unsigned length;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc,
                             uint32_t max_alignment) {
    unsigned alignment_length;
    alignment =
        decoder->read_u32v<checked>(pc + 1, &alignment_length, "alignment");
    if (!CHECKED_COND(alignment <= max_alignment)) {
      decoder->errorf(pc + 1,
                      "invalid alignment; expected maximum alignment is %u, "
                      "actual alignment is %u",
                      max_alignment, alignment);
    }
    unsigned offset_length;
    offset = decoder->read_u32v<checked>(pc + 1 + alignment_length,
                                         &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Operand for SIMD lane operations.
template <bool checked>
struct SimdLaneOperand {
  uint8_t lane;
  unsigned length = 1;

  inline SimdLaneOperand(Decoder* decoder, const byte* pc) {
    lane = decoder->read_u8<checked>(pc + 2, "lane");
  }
};

// Operand for SIMD shift operations.
template <bool checked>
struct SimdShiftOperand {
  uint8_t shift;
  unsigned length = 1;

  inline SimdShiftOperand(Decoder* decoder, const byte* pc) {
    shift = decoder->read_u8<checked>(pc + 2, "shift");
  }
};

// Operand for SIMD shuffle operations.
template <bool checked>
struct SimdShuffleOperand {
  uint8_t shuffle[16];
  unsigned lanes;

  inline SimdShuffleOperand(Decoder* decoder, const byte* pc, unsigned lanes_) {
    lanes = lanes_;
    for (unsigned i = 0; i < lanes; i++) {
      shuffle[i] = decoder->read_u8<checked>(pc + 2 + i, "shuffle");
    }
  }
};

#undef CHECKED_COND

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
