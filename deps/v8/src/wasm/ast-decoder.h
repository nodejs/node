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
  int length;

  inline LocalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "local index");
    type = kAstStmt;
  }
};

struct ImmI8Operand {
  int8_t value;
  int length;
  inline ImmI8Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<int8_t>(decoder->checked_read_u8(pc, 1, "immi8"));
    length = 1;
  }
};

struct ImmI32Operand {
  int32_t value;
  int length;
  inline ImmI32Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<int32_t>(decoder->checked_read_u32(pc, 1, "immi32"));
    length = 4;
  }
};

struct ImmI64Operand {
  int64_t value;
  int length;
  inline ImmI64Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<int64_t>(decoder->checked_read_u64(pc, 1, "immi64"));
    length = 8;
  }
};

struct ImmF32Operand {
  float value;
  int length;
  inline ImmF32Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<float>(decoder->checked_read_u32(pc, 1, "immf32"));
    length = 4;
  }
};

struct ImmF64Operand {
  double value;
  int length;
  inline ImmF64Operand(Decoder* decoder, const byte* pc) {
    value = bit_cast<double>(decoder->checked_read_u64(pc, 1, "immf64"));
    length = 8;
  }
};

struct GlobalIndexOperand {
  uint32_t index;
  LocalType type;
  MachineType machine_type;
  int length;

  inline GlobalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "global index");
    type = kAstStmt;
    machine_type = MachineType::None();
  }
};

struct Block;
struct BreakDepthOperand {
  uint32_t depth;
  Block* target;
  int length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    depth = decoder->checked_read_u8(pc, 1, "break depth");
    length = 1;
    target = nullptr;
  }
};

struct BlockCountOperand {
  uint32_t count;
  int length;
  inline BlockCountOperand(Decoder* decoder, const byte* pc) {
    count = decoder->checked_read_u8(pc, 1, "block count");
    length = 1;
  }
};

struct SignatureIndexOperand {
  uint32_t index;
  FunctionSig* sig;
  int length;
  inline SignatureIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "signature index");
    sig = nullptr;
  }
};

struct FunctionIndexOperand {
  uint32_t index;
  FunctionSig* sig;
  int length;
  inline FunctionIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "function index");
    sig = nullptr;
  }
};

struct ImportIndexOperand {
  uint32_t index;
  FunctionSig* sig;
  int length;
  inline ImportIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->checked_read_u32v(pc, 1, &length, "import index");
    sig = nullptr;
  }
};

struct TableSwitchOperand {
  uint32_t case_count;
  uint32_t table_count;
  const byte* table;
  int length;
  inline TableSwitchOperand(Decoder* decoder, const byte* pc) {
    case_count = decoder->checked_read_u16(pc, 1, "expected #cases");
    table_count = decoder->checked_read_u16(pc, 3, "expected #entries");
    length = 4 + table_count * 2;

    if (decoder->check(pc, 5, table_count * 2, "expected <table entries>")) {
      table = pc + 5;
    } else {
      table = nullptr;
    }
  }
  inline uint16_t read_entry(Decoder* decoder, int i) {
    DCHECK(i >= 0 && static_cast<uint32_t>(i) < table_count);
    return table ? decoder->read_u16(table + i * sizeof(uint16_t)) : 0;
  }
};

struct MemoryAccessOperand {
  bool aligned;
  uint32_t offset;
  int length;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc) {
    byte bitfield = decoder->checked_read_u8(pc, 1, "memory access byte");
    aligned = MemoryAccess::AlignmentField::decode(bitfield);
    if (MemoryAccess::OffsetField::decode(bitfield)) {
      offset = decoder->checked_read_u32v(pc, 2, &length, "memory offset");
      length++;
    } else {
      offset = 0;
      length = 1;
    }
  }
};

typedef compiler::WasmGraphBuilder TFBuilder;
struct ModuleEnv;  // forward declaration of module interface.

// Interface the function environment during decoding, include the signature
// and number of locals.
struct FunctionEnv {
  ModuleEnv* module;             // module environment
  FunctionSig* sig;              // signature of this function
  uint32_t local_i32_count;      // number of int32 locals
  uint32_t local_i64_count;      // number of int64 locals
  uint32_t local_f32_count;      // number of float32 locals
  uint32_t local_f64_count;      // number of float64 locals
  uint32_t total_locals;         // sum of parameters and all locals

  uint32_t GetLocalCount() { return total_locals; }
  LocalType GetLocalType(uint32_t index) {
    if (index < static_cast<uint32_t>(sig->parameter_count())) {
      return sig->GetParam(index);
    }
    index -= static_cast<uint32_t>(sig->parameter_count());
    if (index < local_i32_count) return kAstI32;
    index -= local_i32_count;
    if (index < local_i64_count) return kAstI64;
    index -= local_i64_count;
    if (index < local_f32_count) return kAstF32;
    index -= local_f32_count;
    if (index < local_f64_count) return kAstF64;
    return kAstStmt;
  }

  void AddLocals(LocalType type, uint32_t count) {
    switch (type) {
      case kAstI32:
        local_i32_count += count;
        break;
      case kAstI64:
        local_i64_count += count;
        break;
      case kAstF32:
        local_f32_count += count;
        break;
      case kAstF64:
        local_f64_count += count;
        break;
      default:
        UNREACHABLE();
    }
    total_locals += count;
    DCHECK_EQ(total_locals,
              (sig->parameter_count() + local_i32_count + local_i64_count +
               local_f32_count + local_f64_count));
  }

  void SumLocals() {
    total_locals = static_cast<uint32_t>(sig->parameter_count()) +
                   local_i32_count + local_i64_count + local_f32_count +
                   local_f64_count;
  }
};

struct Tree;
typedef Result<Tree*> TreeResult;

std::ostream& operator<<(std::ostream& os, const Tree& tree);

TreeResult VerifyWasmCode(FunctionEnv* env, const byte* base, const byte* start,
                          const byte* end);
TreeResult BuildTFGraph(TFBuilder* builder, FunctionEnv* env, const byte* base,
                        const byte* start, const byte* end);

void PrintAst(FunctionEnv* env, const byte* start, const byte* end);

inline TreeResult VerifyWasmCode(FunctionEnv* env, const byte* start,
                                 const byte* end) {
  return VerifyWasmCode(env, nullptr, start, end);
}

inline TreeResult BuildTFGraph(TFBuilder* builder, FunctionEnv* env,
                               const byte* start, const byte* end) {
  return BuildTFGraph(builder, env, nullptr, start, end);
}

enum ReadUnsignedLEB128ErrorCode { kNoError, kInvalidLEB128, kMissingLEB128 };

ReadUnsignedLEB128ErrorCode ReadUnsignedLEB128Operand(const byte*, const byte*,
                                                      int*, uint32_t*);

BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, FunctionEnv* env,
                                           const byte* start, const byte* end);

// Computes the length of the opcode at the given address.
int OpcodeLength(const byte* pc, const byte* end);

// Computes the arity (number of sub-nodes) of the opcode at the given address.
int OpcodeArity(FunctionEnv* env, const byte* pc, const byte* end);
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_AST_DECODER_H_
