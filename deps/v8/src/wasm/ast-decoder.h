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
    value = decoder->checked_read_i32v(pc, 1, &length, "immi32");
  }
};

struct ImmI64Operand {
  int64_t value;
  int length;
  inline ImmI64Operand(Decoder* decoder, const byte* pc) {
    value = decoder->checked_read_i64v(pc, 1, &length, "immi64");
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

struct Control;
struct BreakDepthOperand {
  uint32_t arity;
  uint32_t depth;
  Control* target;
  int length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    int len1 = 0;
    int len2 = 0;
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
  int length;
  inline CallIndirectOperand(Decoder* decoder, const byte* pc) {
    int len1 = 0;
    int len2 = 0;
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
  int length;
  inline CallFunctionOperand(Decoder* decoder, const byte* pc) {
    int len1 = 0;
    int len2 = 0;
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
  int length;
  inline CallImportOperand(Decoder* decoder, const byte* pc) {
    int len1 = 0;
    int len2 = 0;
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
  int length;
  inline BranchTableOperand(Decoder* decoder, const byte* pc) {
    int len1 = 0;
    int len2 = 0;
    arity = decoder->checked_read_u32v(pc, 1, &len1, "argument count");
    table_count =
        decoder->checked_read_u32v(pc, 1 + len1, &len2, "table count");
    length = len1 + len2 + (table_count + 1) * sizeof(uint32_t);

    uint32_t table_start = 1 + len1 + len2;
    if (decoder->check(pc, table_start, (table_count + 1) * sizeof(uint32_t),
                       "expected <table entries>")) {
      table = pc + table_start;
    } else {
      table = nullptr;
    }
  }
  inline uint32_t read_entry(Decoder* decoder, int i) {
    DCHECK(i >= 0 && static_cast<uint32_t>(i) <= table_count);
    return table ? decoder->read_u32(table + i * sizeof(uint32_t)) : 0;
  }
};

struct MemoryAccessOperand {
  uint32_t alignment;
  uint32_t offset;
  int length;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc) {
    int alignment_length;
    alignment =
        decoder->checked_read_u32v(pc, 1, &alignment_length, "alignment");
    int offset_length;
    offset = decoder->checked_read_u32v(pc, 1 + alignment_length,
                                        &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

struct ReturnArityOperand {
  uint32_t arity;
  int length;

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

struct Tree;
typedef Result<Tree*> TreeResult;

std::ostream& operator<<(std::ostream& os, const Tree& tree);

TreeResult VerifyWasmCode(base::AccountingAllocator* allocator,
                          FunctionBody& body);
TreeResult BuildTFGraph(base::AccountingAllocator* allocator,
                        TFBuilder* builder, FunctionBody& body);
void PrintAst(base::AccountingAllocator* allocator, FunctionBody& body);

// A simplified form of AST printing, e.g. from a debugger.
void PrintAstForDebugging(const byte* start, const byte* end);

inline TreeResult VerifyWasmCode(base::AccountingAllocator* allocator,
                                 ModuleEnv* module, FunctionSig* sig,
                                 const byte* start, const byte* end) {
  FunctionBody body = {module, sig, nullptr, start, end};
  return VerifyWasmCode(allocator, body);
}

inline TreeResult BuildTFGraph(base::AccountingAllocator* allocator,
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
int OpcodeLength(const byte* pc, const byte* end);

// Computes the arity (number of sub-nodes) of the opcode at the given address.
int OpcodeArity(const byte* pc, const byte* end);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_AST_DECODER_H_
