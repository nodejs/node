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

// Helpers for decoding different kinds of operands which follow bytecodes.
struct LocalIndexOperand {
  uint32_t index;
  ValueType type;
  unsigned length;

  inline LocalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "local index");
    type = kWasmStmt;
  }
};

struct ImmI32Operand {
  int32_t value;
  unsigned length;
  inline ImmI32Operand(Decoder* decoder, const byte* pc) {
    value = decoder->checked_read_i32v(pc, 1, &length, "immi32");
  }
};

struct ImmI64Operand {
  int64_t value;
  unsigned length;
  inline ImmI64Operand(Decoder* decoder, const byte* pc) {
    value = decoder->checked_read_i64v(pc, 1, &length, "immi64");
  }
};

struct ImmF32Operand {
  float value;
  unsigned length;
  inline ImmF32Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint32_t tmp = decoder->checked_read_u32(pc, 1, "immf32");
    memcpy(&value, &tmp, sizeof(value));
    length = 4;
  }
};

struct ImmF64Operand {
  double value;
  unsigned length;
  inline ImmF64Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->checked_read_u64(pc, 1, "immf64");
    memcpy(&value, &tmp, sizeof(value));
    length = 8;
  }
};

struct GlobalIndexOperand {
  uint32_t index;
  ValueType type;
  const WasmGlobal* global;
  unsigned length;

  inline GlobalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "global index");
    global = nullptr;
    type = kWasmStmt;
  }
};

struct BlockTypeOperand {
  uint32_t arity;
  const byte* types;  // pointer to encoded types for the block.
  unsigned length;

  inline BlockTypeOperand(Decoder* decoder, const byte* pc) {
    uint8_t val = decoder->checked_read_u8(pc, 1, "block type");
    ValueType type = kWasmStmt;
    length = 1;
    arity = 0;
    types = nullptr;
    if (decode_local_type(val, &type)) {
      arity = type == kWasmStmt ? 0 : 1;
      types = pc + 1;
    } else {
      // Handle multi-value blocks.
      if (!FLAG_wasm_mv_prototype) {
        decoder->error(pc, pc + 1, "invalid block arity > 1");
        return;
      }
      if (val != kMultivalBlock) {
        decoder->error(pc, pc + 1, "invalid block type");
        return;
      }
      // Decode and check the types vector of the block.
      unsigned len = 0;
      uint32_t count = decoder->checked_read_u32v(pc, 2, &len, "block arity");
      // {count} is encoded as {arity-2}, so that a {0} count here corresponds
      // to a block with 2 values. This makes invalid/redundant encodings
      // impossible.
      arity = count + 2;
      length = 1 + len + arity;
      types = pc + 1 + 1 + len;

      for (uint32_t i = 0; i < arity; i++) {
        uint32_t offset = 1 + 1 + len + i;
        val = decoder->checked_read_u8(pc, offset, "block type");
        decode_local_type(val, &type);
        if (type == kWasmStmt) {
          decoder->error(pc, pc + offset, "invalid block type");
          return;
        }
      }
    }
  }
  // Decode a byte representing a local type. Return {false} if the encoded
  // byte was invalid or {kMultivalBlock}.
  bool decode_local_type(uint8_t val, ValueType* result) {
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
struct BreakDepthOperand {
  uint32_t depth;
  Control* target;
  unsigned length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    depth = decoder->checked_read_u32v(pc, 1, &length, "break depth");
    target = nullptr;
  }
};

struct CallIndirectOperand {
  uint32_t table_index;
  uint32_t index;
  FunctionSig* sig;
  unsigned length;
  inline CallIndirectOperand(Decoder* decoder, const byte* pc) {
    unsigned len = 0;
    index = decoder->checked_read_u32v(pc, 1, &len, "signature index");
    table_index = decoder->checked_read_u8(pc, 1 + len, "table index");
    if (table_index != 0) {
      decoder->error(pc, pc + 1 + len, "expected table index 0, found %u",
                     table_index);
    }
    length = 1 + len;
    sig = nullptr;
  }
};

struct CallFunctionOperand {
  uint32_t index;
  FunctionSig* sig;
  unsigned length;
  inline CallFunctionOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    index = decoder->checked_read_u32v(pc, 1 + len1, &len2, "function index");
    length = len1 + len2;
    sig = nullptr;
  }
};

struct MemoryIndexOperand {
  uint32_t index;
  unsigned length;
  inline MemoryIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u8(pc, 1, "memory index");
    if (index != 0) {
      decoder->error(pc, pc + 1, "expected memory index 0, found %u", index);
    }
    length = 1;
  }
};

struct BranchTableOperand {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  inline BranchTableOperand(Decoder* decoder, const byte* pc) {
    DCHECK_EQ(kExprBrTable, decoder->checked_read_u8(pc, 0, "opcode"));
    start = pc + 1;
    unsigned len1 = 0;
    table_count = decoder->checked_read_u32v(pc, 1, &len1, "table count");
    if (table_count > (UINT_MAX / sizeof(uint32_t)) - 1 ||
        len1 > UINT_MAX - (table_count + 1) * sizeof(uint32_t)) {
      decoder->error(pc, "branch table size overflow");
    }
    table = pc + 1 + len1;
  }
};

// A helper to iterate over a branch table.
class BranchTableIterator {
 public:
  unsigned cur_index() { return index_; }
  bool has_next() { return decoder_->ok() && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    unsigned length = 0;
    uint32_t result =
        decoder_->checked_read_u32v(pc_, 0, &length, "branch table entry");
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

  BranchTableIterator(Decoder* decoder, BranchTableOperand& operand)
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

struct MemoryAccessOperand {
  uint32_t alignment;
  uint32_t offset;
  unsigned length;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc,
                             uint32_t max_alignment) {
    unsigned alignment_length;
    alignment =
        decoder->checked_read_u32v(pc, 1, &alignment_length, "alignment");
    if (max_alignment < alignment) {
      decoder->error(pc, pc + 1,
                     "invalid alignment; expected maximum alignment is %u, "
                     "actual alignment is %u",
                     max_alignment, alignment);
    }
    unsigned offset_length;
    offset = decoder->checked_read_u32v(pc, 1 + alignment_length,
                                        &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Operand for SIMD lane operations.
struct SimdLaneOperand {
  uint8_t lane;
  unsigned length;

  inline SimdLaneOperand(Decoder* decoder, const byte* pc) {
    lane = decoder->checked_read_u8(pc, 2, "lane");
    length = 1;
  }
};

// Operand for SIMD shift operations.
struct SimdShiftOperand {
  uint8_t shift;
  unsigned length;

  inline SimdShiftOperand(Decoder* decoder, const byte* pc) {
    shift = decoder->checked_read_u8(pc, 2, "shift");
    length = 1;
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
