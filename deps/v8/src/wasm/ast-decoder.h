// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_AST_DECODER_H_
#define V8_WASM_AST_DECODER_H_

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

// Helpers for decoding different kinds of operands which follow bytecodes.
struct LocalIndexOperand {
  uint32_t index;
  LocalType type;
  unsigned length;

  inline LocalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "local index");
    type = kAstStmt;
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
    value = bit_cast<float>(decoder->checked_read_u32(pc, 1, "immf32"));
    length = 4;
  }
};

struct ImmF64Operand {
  double value;
  unsigned length;
  inline ImmF64Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<double>(decoder->checked_read_u64(pc, 1, "immf64"));
    length = 8;
  }
};

struct GlobalIndexOperand {
  uint32_t index;
  LocalType type;
  unsigned length;

  inline GlobalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "global index");
    type = kAstStmt;
  }
};

struct Control;
struct BreakDepthOperand {
  uint32_t arity;
  uint32_t depth;
  Control* target;
  unsigned length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    depth = decoder->checked_read_u32v(pc, 1 + len1, &len2, "break depth");
    length = len1 + len2;
    target = nullptr;
  }
};

struct CallIndirectOperand {
  uint32_t arity;
  uint32_t index;
  FunctionSig* sig;
  unsigned length;
  inline CallIndirectOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    index = decoder->checked_read_u32v(pc, 1 + len1, &len2, "signature index");
    length = len1 + len2;
    sig = nullptr;
  }
};

struct CallFunctionOperand {
  uint32_t arity;
  uint32_t index;
  FunctionSig* sig;
  unsigned length;
  inline CallFunctionOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    index = decoder->checked_read_u32v(pc, 1 + len1, &len2, "function index");
    length = len1 + len2;
    sig = nullptr;
  }
};

struct CallImportOperand {
  uint32_t arity;
  uint32_t index;
  FunctionSig* sig;
  unsigned length;
  inline CallImportOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    index = decoder->checked_read_u32v(pc, 1 + len1, &len2, "import index");
    length = len1 + len2;
    sig = nullptr;
  }
};

struct BranchTableOperand {
  uint32_t arity;
  uint32_t table_count;
  const byte* table;
  unsigned length;
  inline BranchTableOperand(Decoder* decoder, const byte* pc) {
    unsigned len1 = 0;
    unsigned len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    table_count =
        decoder->checked_read_u32v(pc, 1 + len1, &len2, "table count");
    if (table_count > (UINT_MAX / sizeof(uint32_t)) - 1 ||
        len1 + len2 > UINT_MAX - (table_count + 1) * sizeof(uint32_t)) {
      decoder->error(pc, "branch table size overflow");
    }
    length = len1 + len2 + (table_count + 1) * sizeof(uint32_t);

    uint32_t table_start = 1 + len1 + len2;
    if (decoder->check(pc, table_start, (table_count + 1) * sizeof(uint32_t),
                       "expected <table entries>")) {
      table = pc + table_start;
    } else {
      table = nullptr;
    }
  }
  inline uint32_t read_entry(Decoder* decoder, unsigned i) {
    DCHECK(i <= table_count);
    return table ? decoder->read_u32(table + i * sizeof(uint32_t)) : 0;
  }
};

struct MemoryAccessOperand {
  uint32_t alignment;
  uint32_t offset;
  unsigned length;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc) {
    unsigned alignment_length;
    alignment =
        decoder->checked_read_u32v(pc, 1, &alignment_length, "alignment");
    unsigned offset_length;
    offset = decoder->checked_read_u32v(pc, 1 + alignment_length,
                                        &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

struct ReturnArityOperand {
  uint32_t arity;
  unsigned length;

  inline ReturnArityOperand(Decoder* decoder, const byte* pc) {
    arity = decoder->checked_read_u32v(pc, 1, &length, "return count");
  }
};

typedef compiler::WasmGraphBuilder TFBuilder;
struct ModuleEnv;  // forward declaration of module interface.

// All of the various data structures necessary to decode a function body.
struct FunctionBody {
  ModuleEnv* module;  // module environment
  FunctionSig* sig;   // function signature
  const byte* base;   // base of the module bytes, for error reporting
  const byte* start;  // start of the function body
  const byte* end;    // end of the function body
};

static inline FunctionBody FunctionBodyForTesting(const byte* start,
                                                  const byte* end) {
  return {nullptr, nullptr, start, start, end};
}

struct DecodeStruct {
  int unused;
};
typedef Result<DecodeStruct*> DecodeResult;
inline std::ostream& operator<<(std::ostream& os, const DecodeStruct& tree) {
  return os;
}

DecodeResult VerifyWasmCode(base::AccountingAllocator* allocator,
                            FunctionBody& body);
DecodeResult BuildTFGraph(base::AccountingAllocator* allocator,
                          TFBuilder* builder, FunctionBody& body);
bool PrintAst(base::AccountingAllocator* allocator, const FunctionBody& body,
              std::ostream& os,
              std::vector<std::tuple<uint32_t, int, int>>* offset_table);

// A simplified form of AST printing, e.g. from a debugger.
void PrintAstForDebugging(const byte* start, const byte* end);

inline DecodeResult VerifyWasmCode(base::AccountingAllocator* allocator,
                                   ModuleEnv* module, FunctionSig* sig,
                                   const byte* start, const byte* end) {
  FunctionBody body = {module, sig, nullptr, start, end};
  return VerifyWasmCode(allocator, body);
}

inline DecodeResult BuildTFGraph(base::AccountingAllocator* allocator,
                                 TFBuilder* builder, ModuleEnv* module,
                                 FunctionSig* sig, const byte* start,
                                 const byte* end) {
  FunctionBody body = {module, sig, nullptr, start, end};
  return BuildTFGraph(allocator, builder, body);
}

struct AstLocalDecls {
  // The size of the encoded declarations.
  uint32_t decls_encoded_size;  // size of encoded declarations

  // Total number of locals.
  uint32_t total_local_count;

  // List of {local type, count} pairs.
  ZoneVector<std::pair<LocalType, uint32_t>> local_types;

  // Constructor initializes the vector.
  explicit AstLocalDecls(Zone* zone)
      : decls_encoded_size(0), total_local_count(0), local_types(zone) {}
};

bool DecodeLocalDecls(AstLocalDecls& decls, const byte* start, const byte* end);
BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, size_t num_locals,
                                           const byte* start, const byte* end);

// Computes the length of the opcode at the given address.
unsigned OpcodeLength(const byte* pc, const byte* end);

// Computes the arity (number of sub-nodes) of the opcode at the given address.
unsigned OpcodeArity(const byte* pc, const byte* end);

// A simple forward iterator for bytecodes.
class BytecodeIterator : public Decoder {
 public:
  // If one wants to iterate over the bytecode without looking at {pc_offset()}.
  class iterator {
   public:
    inline iterator& operator++() {
      DCHECK_LT(ptr_, end_);
      ptr_ += OpcodeLength(ptr_, end_);
      return *this;
    }
    inline WasmOpcode operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<WasmOpcode>(*ptr_);
    }
    inline bool operator==(const iterator& that) {
      return this->ptr_ == that.ptr_;
    }
    inline bool operator!=(const iterator& that) {
      return this->ptr_ != that.ptr_;
    }

   private:
    friend class BytecodeIterator;
    const byte* ptr_;
    const byte* end_;
    iterator(const byte* ptr, const byte* end) : ptr_(ptr), end_(end) {}
  };

  // Create a new {BytecodeIterator}. If the {decls} pointer is non-null,
  // assume the bytecode starts with local declarations and decode them.
  // Otherwise, do not decode local decls.
  BytecodeIterator(const byte* start, const byte* end,
                   AstLocalDecls* decls = nullptr);

  inline iterator begin() const { return iterator(pc_, end_); }
  inline iterator end() const { return iterator(end_, end_); }

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

#endif  // V8_WASM_AST_DECODER_H_
