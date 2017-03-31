// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_H_
#define V8_WASM_FUNCTION_BODY_DECODER_H_

#include <iterator>

#include "src/base/compiler-specific.h"
#include "src/base/iterator.h"
#include "src/globals.h"
#include "src/signature.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

class BitVector;  // forward declaration

namespace compiler {  // external declarations from compiler.
class WasmGraphBuilder;
}

namespace wasm {

const uint32_t kMaxNumWasmLocals = 8000000;
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

struct ImmI8Operand {
  int8_t value;
  unsigned length;
  inline ImmI8Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<int8_t>(decoder->checked_read_u8(pc, 1, "immi8"));
    length = 1;
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

typedef compiler::WasmGraphBuilder TFBuilder;
struct WasmModule;  // forward declaration of module interface.

// All of the various data structures necessary to decode a function body.
struct FunctionBody {
  FunctionSig* sig;   // function signature
  const byte* base;   // base of the module bytes, for error reporting
  const byte* start;  // start of the function body
  const byte* end;    // end of the function body
};

static inline FunctionBody FunctionBodyForTesting(const byte* start,
                                                  const byte* end) {
  return {nullptr, start, start, end};
}

struct DecodeStruct {
  int unused;
};
typedef Result<DecodeStruct*> DecodeResult;
inline std::ostream& operator<<(std::ostream& os, const DecodeStruct& tree) {
  return os;
}

V8_EXPORT_PRIVATE DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                                              const wasm::WasmModule* module,
                                              FunctionBody& body);
DecodeResult BuildTFGraph(AccountingAllocator* allocator, TFBuilder* builder,
                          FunctionBody& body);
bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const wasm::WasmModule* module);

// A simplified form of AST printing, e.g. from a debugger.
void PrintRawWasmCode(const byte* start, const byte* end);

inline DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                                   const WasmModule* module, FunctionSig* sig,
                                   const byte* start, const byte* end) {
  FunctionBody body = {sig, nullptr, start, end};
  return VerifyWasmCode(allocator, module, body);
}

inline DecodeResult BuildTFGraph(AccountingAllocator* allocator,
                                 TFBuilder* builder, FunctionSig* sig,
                                 const byte* start, const byte* end) {
  FunctionBody body = {sig, nullptr, start, end};
  return BuildTFGraph(allocator, builder, body);
}

struct BodyLocalDecls {
  // The size of the encoded declarations.
  uint32_t encoded_size;  // size of encoded declarations

  ZoneVector<ValueType> type_list;

  // Constructor initializes the vector.
  explicit BodyLocalDecls(Zone* zone) : encoded_size(0), type_list(zone) {}
};

V8_EXPORT_PRIVATE bool DecodeLocalDecls(BodyLocalDecls* decls,
                                        const byte* start, const byte* end);
V8_EXPORT_PRIVATE BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone,
                                                             size_t num_locals,
                                                             const byte* start,
                                                             const byte* end);

// Computes the length of the opcode at the given address.
V8_EXPORT_PRIVATE unsigned OpcodeLength(const byte* pc, const byte* end);

// A simple forward iterator for bytecodes.
class V8_EXPORT_PRIVATE BytecodeIterator : public NON_EXPORTED_BASE(Decoder) {
  // Base class for both iterators defined below.
  class iterator_base {
   public:
    inline iterator_base& operator++() {
      DCHECK_LT(ptr_, end_);
      ptr_ += OpcodeLength(ptr_, end_);
      return *this;
    }
    inline bool operator==(const iterator_base& that) {
      return this->ptr_ == that.ptr_;
    }
    inline bool operator!=(const iterator_base& that) {
      return this->ptr_ != that.ptr_;
    }

   protected:
    const byte* ptr_;
    const byte* end_;
    iterator_base(const byte* ptr, const byte* end) : ptr_(ptr), end_(end) {}
  };

 public:
  // If one wants to iterate over the bytecode without looking at {pc_offset()}.
  class opcode_iterator
      : public iterator_base,
        public std::iterator<std::input_iterator_tag, WasmOpcode> {
   public:
    inline WasmOpcode operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<WasmOpcode>(*ptr_);
    }

   private:
    friend class BytecodeIterator;
    opcode_iterator(const byte* ptr, const byte* end)
        : iterator_base(ptr, end) {}
  };
  // If one wants to iterate over the instruction offsets without looking at
  // opcodes.
  class offset_iterator
      : public iterator_base,
        public std::iterator<std::input_iterator_tag, uint32_t> {
   public:
    inline uint32_t operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<uint32_t>(ptr_ - start_);
    }

   private:
    const byte* start_;
    friend class BytecodeIterator;
    offset_iterator(const byte* start, const byte* ptr, const byte* end)
        : iterator_base(ptr, end), start_(start) {}
  };

  // Create a new {BytecodeIterator}. If the {decls} pointer is non-null,
  // assume the bytecode starts with local declarations and decode them.
  // Otherwise, do not decode local decls.
  BytecodeIterator(const byte* start, const byte* end,
                   BodyLocalDecls* decls = nullptr);

  base::iterator_range<opcode_iterator> opcodes() {
    return base::iterator_range<opcode_iterator>(opcode_iterator(pc_, end_),
                                                 opcode_iterator(end_, end_));
  }

  base::iterator_range<offset_iterator> offsets() {
    return base::iterator_range<offset_iterator>(
        offset_iterator(start_, pc_, end_),
        offset_iterator(start_, end_, end_));
  }

  WasmOpcode current() {
    return static_cast<WasmOpcode>(
        checked_read_u8(pc_, 0, "expected bytecode"));
  }

  void next() {
    if (pc_ < end_) {
      pc_ += OpcodeLength(pc_, end_);
      if (pc_ >= end_) pc_ = end_;
    }
  }

  bool has_next() { return pc_ < end_; }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_H_
