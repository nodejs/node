// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
#define V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_

// Do only include this header for implementing new Interface of the
// WasmFullDecoder.

#include "src/bit-vector.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmGlobal;
struct WasmException;

#if DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

// Return the evaluation of `condition` if validate==true, DCHECK
// and always return true otherwise.
#define VALIDATE(condition)       \
  (validate ? (condition) : [&] { \
    DCHECK(condition);            \
    return true;                  \
  }())

// Return the evaluation of `condition` if validate==true, DCHECK that it's
// false and always return false otherwise.
#define CHECK_ERROR(condition)    \
  (validate ? (condition) : [&] { \
    DCHECK(!(condition));         \
    return false;                 \
  }())

// Use this macro to check a condition if checked == true, and DCHECK the
// condition otherwise.
// TODO(clemensh): Rename all "checked" to "validate" and replace
// "CHECKED_COND" with "CHECK_ERROR".
#define CHECKED_COND(cond)   \
  (checked ? (cond) : ([&] { \
    DCHECK(cond);            \
    return true;             \
  })())

#define CHECK_PROTOTYPE_OPCODE(flag)                                           \
  if (this->module_ != nullptr && this->module_->is_asm_js()) {                \
    this->error("Opcode not supported for asmjs modules");                     \
  }                                                                            \
  if (!FLAG_experimental_wasm_##flag) {                                        \
    this->error("Invalid opcode (enable with --experimental-wasm-" #flag ")"); \
    break;                                                                     \
  }

#define OPCODE_ERROR(opcode, message)                                 \
  (this->errorf(this->pc_, "%s: %s", WasmOpcodes::OpcodeName(opcode), \
                (message)))

template <typename T>
Vector<T> vec2vec(std::vector<T>& vec) {
  return Vector<T>(vec.data(), vec.size());
}

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
struct ExceptionIndexOperand {
  uint32_t index;
  const WasmException* exception = nullptr;
  unsigned length;

  inline ExceptionIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<checked>(pc + 1, &length, "exception index");
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
      if (!CHECKED_COND(FLAG_experimental_wasm_mv)) {
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
      default:
        *result = kWasmStmt;
        return false;
    }
  }

  ValueType read_entry(unsigned index) {
    DCHECK_LT(index, arity);
    ValueType result;
    bool success = decode_local_type(types[index], &result);
    DCHECK(success);
    USE(success);
    return result;
  }
};

template <bool checked>
struct BreakDepthOperand {
  uint32_t depth;
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

  BranchTableIterator(Decoder* decoder,
                      const BranchTableOperand<checked>& operand)
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

// Operand for SIMD S8x16 shuffle operations.
template <bool checked>
struct Simd8x16ShuffleOperand {
  uint8_t shuffle[kSimd128Size];

  inline Simd8x16ShuffleOperand(Decoder* decoder, const byte* pc) {
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      shuffle[i] = decoder->read_u8<checked>(pc + 2 + i, "shuffle");
    }
  }
};

// An entry on the value stack.
template <typename Interface>
struct AbstractValue {
  const byte* pc;
  ValueType type;
  typename Interface::IValue interface_data;

  // Named constructors.
  static AbstractValue Unreachable(const byte* pc) {
    return {pc, kWasmVar, Interface::IValue::Unreachable()};
  }

  static AbstractValue New(const byte* pc, ValueType type) {
    return {pc, type, Interface::IValue::New()};
  }
};

template <typename Interface>
struct AbstractMerge {
  uint32_t arity;
  union {
    AbstractValue<Interface>* array;
    AbstractValue<Interface> first;
  } vals;  // Either multiple values or a single value.

  AbstractValue<Interface>& operator[](size_t i) {
    DCHECK_GT(arity, i);
    return arity == 1 ? vals.first : vals.array[i];
  }
};

enum ControlKind {
  kControlIf,
  kControlIfElse,
  kControlBlock,
  kControlLoop,
  kControlTry,
  kControlTryCatch
};

// An entry on the control stack (i.e. if, block, loop, or try).
template <typename Interface>
struct AbstractControl {
  const byte* pc;
  ControlKind kind;
  size_t stack_depth;  // stack height at the beginning of the construct.
  typename Interface::IControl interface_data;
  bool unreachable;  // The current block has been ended.

  // Values merged into the end of this control construct.
  AbstractMerge<Interface> merge;

  inline bool is_if() const { return is_onearmed_if() || is_if_else(); }
  inline bool is_onearmed_if() const { return kind == kControlIf; }
  inline bool is_if_else() const { return kind == kControlIfElse; }
  inline bool is_block() const { return kind == kControlBlock; }
  inline bool is_loop() const { return kind == kControlLoop; }
  inline bool is_try() const { return is_incomplete_try() || is_try_catch(); }
  inline bool is_incomplete_try() const { return kind == kControlTry; }
  inline bool is_try_catch() const { return kind == kControlTryCatch; }

  // Named constructors.
  static AbstractControl Block(const byte* pc, size_t stack_depth) {
    return {pc, kControlBlock, stack_depth, Interface::IControl::Block(), false,
            {}};
  }

  static AbstractControl If(const byte* pc, size_t stack_depth) {
    return {pc, kControlIf, stack_depth, Interface::IControl::If(), false, {}};
  }

  static AbstractControl Loop(const byte* pc, size_t stack_depth) {
    return {pc, kControlLoop, stack_depth, Interface::IControl::Loop(), false,
            {}};
  }

  static AbstractControl Try(const byte* pc, size_t stack_depth) {
    return {pc,    kControlTry, stack_depth, Interface::IControl::Try(),
            false, {}};
  }
};

// This is the list of callback functions that an interface for the
// WasmFullDecoder should implement.
// F(Name, args...)
#define INTERFACE_FUNCTIONS(F)                                                 \
  /* General: */                                                               \
  F(StartFunction)                                                             \
  F(StartFunctionBody, Control* block)                                         \
  F(FinishFunction)                                                            \
  /* Control: */                                                               \
  F(Block, Control* block)                                                     \
  F(Loop, Control* block)                                                      \
  F(Try, Control* block)                                                       \
  F(If, const Value& cond, Control* if_block)                                  \
  F(FallThruTo, Control* c)                                                    \
  F(PopControl, const Control& block)                                          \
  F(EndControl, Control* block)                                                \
  /* Instructions: */                                                          \
  F(UnOp, WasmOpcode opcode, FunctionSig*, const Value& value, Value* result)  \
  F(BinOp, WasmOpcode opcode, FunctionSig*, const Value& lhs,                  \
    const Value& rhs, Value* result)                                           \
  F(I32Const, Value* result, int32_t value)                                    \
  F(I64Const, Value* result, int64_t value)                                    \
  F(F32Const, Value* result, float value)                                      \
  F(F64Const, Value* result, double value)                                     \
  F(DoReturn, Vector<Value> values)                                            \
  F(GetLocal, Value* result, const LocalIndexOperand<validate>& operand)       \
  F(SetLocal, const Value& value, const LocalIndexOperand<validate>& operand)  \
  F(TeeLocal, const Value& value, Value* result,                               \
    const LocalIndexOperand<validate>& operand)                                \
  F(GetGlobal, Value* result, const GlobalIndexOperand<validate>& operand)     \
  F(SetGlobal, const Value& value,                                             \
    const GlobalIndexOperand<validate>& operand)                               \
  F(Unreachable)                                                               \
  F(Select, const Value& cond, const Value& fval, const Value& tval,           \
    Value* result)                                                             \
  F(BreakTo, Control* block)                                                   \
  F(BrIf, const Value& cond, Control* block)                                   \
  F(BrTable, const BranchTableOperand<validate>& operand, const Value& key)    \
  F(Else, Control* if_block)                                                   \
  F(LoadMem, ValueType type, MachineType mem_type,                             \
    const MemoryAccessOperand<validate>& operand, const Value& index,          \
    Value* result)                                                             \
  F(StoreMem, ValueType type, MachineType mem_type,                            \
    const MemoryAccessOperand<validate>& operand, const Value& index,          \
    const Value& value)                                                        \
  F(CurrentMemoryPages, Value* result)                                         \
  F(GrowMemory, const Value& value, Value* result)                             \
  F(CallDirect, const CallFunctionOperand<validate>& operand,                  \
    const Value args[], Value returns[])                                       \
  F(CallIndirect, const Value& index,                                          \
    const CallIndirectOperand<validate>& operand, const Value args[],          \
    Value returns[])                                                           \
  F(SimdOp, WasmOpcode opcode, Vector<Value> args, Value* result)              \
  F(SimdLaneOp, WasmOpcode opcode, const SimdLaneOperand<validate>& operand,   \
    const Vector<Value> inputs, Value* result)                                 \
  F(SimdShiftOp, WasmOpcode opcode, const SimdShiftOperand<validate>& operand, \
    const Value& input, Value* result)                                         \
  F(Simd8x16ShuffleOp, const Simd8x16ShuffleOperand<validate>& operand,        \
    const Value& input0, const Value& input1, Value* result)                   \
  F(Throw, const ExceptionIndexOperand<validate>&)                             \
  F(Catch, const ExceptionIndexOperand<validate>& operand, Control* block)     \
  F(AtomicOp, WasmOpcode opcode, Vector<Value> args, Value* result)

// Generic Wasm bytecode decoder with utilities for decoding operands,
// lengths, etc.
template <bool validate>
class WasmDecoder : public Decoder {
 public:
  WasmDecoder(const WasmModule* module, FunctionSig* sig, const byte* start,
              const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, end, buffer_offset),
        module_(module),
        sig_(sig),
        local_types_(nullptr) {}
  const WasmModule* module_;
  FunctionSig* sig_;

  ZoneVector<ValueType>* local_types_;

  size_t total_locals() const {
    return local_types_ == nullptr ? 0 : local_types_->size();
  }

  static bool DecodeLocals(Decoder* decoder, const FunctionSig* sig,
                           ZoneVector<ValueType>* type_list) {
    DCHECK_NOT_NULL(type_list);
    DCHECK_EQ(0, type_list->size());
    // Initialize from signature.
    if (sig != nullptr) {
      type_list->assign(sig->parameters().begin(), sig->parameters().end());
    }
    // Decode local declarations, if any.
    uint32_t entries = decoder->consume_u32v("local decls count");
    if (decoder->failed()) return false;

    TRACE("local decls count: %u\n", entries);
    while (entries-- > 0 && decoder->ok() && decoder->more()) {
      uint32_t count = decoder->consume_u32v("local count");
      if (decoder->failed()) return false;

      if ((count + type_list->size()) > kV8MaxWasmFunctionLocals) {
        decoder->error(decoder->pc() - 1, "local count too large");
        return false;
      }
      byte code = decoder->consume_u8("local type");
      if (decoder->failed()) return false;

      ValueType type;
      switch (code) {
        case kLocalI32:
          type = kWasmI32;
          break;
        case kLocalI64:
          type = kWasmI64;
          break;
        case kLocalF32:
          type = kWasmF32;
          break;
        case kLocalF64:
          type = kWasmF64;
          break;
        case kLocalS128:
          type = kWasmS128;
          break;
        default:
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
      }
      type_list->insert(type_list->end(), count, type);
    }
    DCHECK(decoder->ok());
    return true;
  }

  static BitVector* AnalyzeLoopAssignment(Decoder* decoder, const byte* pc,
                                          int locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    BitVector* assigned = new (zone) BitVector(locals_count, zone);
    int depth = 0;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && decoder->ok()) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      unsigned length = 1;
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          length = OpcodeLength(decoder, pc);
          depth++;
          break;
        case kExprSetLocal:  // fallthru
        case kExprTeeLocal: {
          LocalIndexOperand<validate> operand(decoder, pc);
          if (assigned->length() > 0 &&
              operand.index < static_cast<uint32_t>(assigned->length())) {
            // Unverified code might have an out-of-bounds index.
            assigned->Add(operand.index);
          }
          length = 1 + operand.length;
          break;
        }
        case kExprEnd:
          depth--;
          break;
        default:
          length = OpcodeLength(decoder, pc);
          break;
      }
      if (depth <= 0) break;
      pc += length;
    }
    return decoder->ok() ? assigned : nullptr;
  }

  inline bool Validate(const byte* pc, LocalIndexOperand<validate>& operand) {
    if (VALIDATE(operand.index < total_locals())) {
      if (local_types_) {
        operand.type = local_types_->at(operand.index);
      } else {
        operand.type = kWasmStmt;
      }
      return true;
    }
    errorf(pc + 1, "invalid local index: %u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc,
                       ExceptionIndexOperand<validate>& operand) {
    if (module_ != nullptr && operand.index < module_->exceptions.size()) {
      operand.exception = &module_->exceptions[operand.index];
      return true;
    }
    errorf(pc + 1, "Invalid exception index: %u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand<validate>& operand) {
    if (VALIDATE(module_ != nullptr &&
                 operand.index < module_->globals.size())) {
      operand.global = &module_->globals[operand.index];
      operand.type = operand.global->type;
      return true;
    }
    errorf(pc + 1, "invalid global index: %u", operand.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallFunctionOperand<validate>& operand) {
    if (VALIDATE(module_ != nullptr &&
                 operand.index < module_->functions.size())) {
      operand.sig = module_->functions[operand.index].sig;
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallFunctionOperand<validate>& operand) {
    if (Complete(pc, operand)) {
      return true;
    }
    errorf(pc + 1, "invalid function index: %u", operand.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectOperand<validate>& operand) {
    if (VALIDATE(module_ != nullptr &&
                 operand.index < module_->signatures.size())) {
      operand.sig = module_->signatures[operand.index];
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallIndirectOperand<validate>& operand) {
    if (CHECK_ERROR(module_ == nullptr || module_->function_tables.empty())) {
      error("function table has to exist to execute call_indirect");
      return false;
    }
    if (Complete(pc, operand)) {
      return true;
    }
    errorf(pc + 1, "invalid signature index: #%u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand<validate>& operand,
                       size_t control_depth) {
    if (VALIDATE(operand.depth < control_depth)) {
      return true;
    }
    errorf(pc + 1, "invalid break depth: %u", operand.depth);
    return false;
  }

  bool Validate(const byte* pc, BranchTableOperand<validate>& operand,
                size_t block_depth) {
    if (CHECK_ERROR(operand.table_count >= kV8MaxWasmFunctionSize)) {
      errorf(pc + 1, "invalid table count (> max function size): %u",
             operand.table_count);
      return false;
    }
    return checkAvailable(operand.table_count);
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdLaneOperand<validate>& operand) {
    uint8_t num_lanes = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane:
      case kExprF32x4ReplaceLane:
      case kExprI32x4ExtractLane:
      case kExprI32x4ReplaceLane:
        num_lanes = 4;
        break;
      case kExprI16x8ExtractLane:
      case kExprI16x8ReplaceLane:
        num_lanes = 8;
        break;
      case kExprI8x16ExtractLane:
      case kExprI8x16ReplaceLane:
        num_lanes = 16;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (CHECK_ERROR(operand.lane < 0 || operand.lane >= num_lanes)) {
      error(pc_ + 2, "invalid lane index");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdShiftOperand<validate>& operand) {
    uint8_t max_shift = 0;
    switch (opcode) {
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
        max_shift = 32;
        break;
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
        max_shift = 16;
        break;
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU:
        max_shift = 8;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (CHECK_ERROR(operand.shift < 0 || operand.shift >= max_shift)) {
      error(pc_ + 2, "invalid shift amount");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc,
                       Simd8x16ShuffleOperand<validate>& operand) {
    uint8_t max_lane = 0;
    for (uint32_t i = 0; i < kSimd128Size; ++i)
      max_lane = std::max(max_lane, operand.shuffle[i]);
    // Shuffle indices must be in [0..31] for a 16 lane shuffle.
    if (CHECK_ERROR(max_lane > 2 * kSimd128Size)) {
      error(pc_ + 2, "invalid shuffle mask");
      return false;
    } else {
      return true;
    }
  }

  static unsigned OpcodeLength(Decoder* decoder, const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessOperand<validate> operand(decoder, pc, UINT32_MAX);
        return 1 + operand.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprSetGlobal:
      case kExprGetGlobal: {
        GlobalIndexOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprCallFunction: {
        CallFunctionOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprTry:
      case kExprIf:  // fall through
      case kExprLoop:
      case kExprBlock: {
        BlockTypeOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprThrow:
      case kExprCatch: {
        ExceptionIndexOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprSetLocal:
      case kExprTeeLocal:
      case kExprGetLocal: {
        LocalIndexOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprBrTable: {
        BranchTableOperand<validate> operand(decoder, pc);
        BranchTableIterator<validate> iterator(decoder, operand);
        return 1 + iterator.length();
      }
      case kExprI32Const: {
        ImmI32Operand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprI64Const: {
        ImmI64Operand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprGrowMemory:
      case kExprMemorySize: {
        MemoryIndexOperand<validate> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kSimdPrefix: {
        byte simd_index = decoder->read_u8<validate>(pc + 1, "simd_index");
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kSimdPrefix << 8 | simd_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 2;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 3;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessOperand<validate> operand(decoder, pc + 1, UINT32_MAX);
            return 2 + operand.length;
          }
          // Shuffles require a byte per lane, or 16 immediate bytes.
          case kExprS8x16Shuffle:
            return 2 + kSimd128Size;
          default:
            decoder->error(pc, "invalid SIMD opcode");
            return 2;
        }
      }
      default:
        return 1;
    }
  }

  std::pair<uint32_t, uint32_t> StackEffect(const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // Handle "simple" opcodes with a fixed signature first.
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!sig) sig = WasmOpcodes::AsmjsSignature(opcode);
    if (sig) return {sig->parameter_count(), sig->return_count()};
    if (WasmOpcodes::IsPrefixOpcode(opcode)) {
      opcode = static_cast<WasmOpcode>(opcode << 8 | *(pc + 1));
    }

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprSelect:
        return {3, 1};
      case kExprS128StoreMem:
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
      case kExprS128LoadMem:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      case kExprTeeLocal:
      case kExprGrowMemory:
        return {1, 1};
      case kExprSetLocal:
      case kExprSetGlobal:
      case kExprDrop:
      case kExprBrIf:
      case kExprBrTable:
      case kExprIf:
        return {1, 0};
      case kExprGetLocal:
      case kExprGetGlobal:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF32Const:
      case kExprF64Const:
      case kExprMemorySize:
        return {0, 1};
      case kExprCallFunction: {
        CallFunctionOperand<validate> operand(this, pc);
        CHECK(Complete(pc, operand));
        return {operand.sig->parameter_count(), operand.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectOperand<validate> operand(this, pc);
        CHECK(Complete(pc, operand));
        // Indirect calls pop an additional argument for the table index.
        return {operand.sig->parameter_count() + 1,
                operand.sig->return_count()};
      }
      case kExprBr:
      case kExprBlock:
      case kExprLoop:
      case kExprEnd:
      case kExprElse:
      case kExprNop:
      case kExprReturn:
      case kExprUnreachable:
        return {0, 0};
      default:
        V8_Fatal(__FILE__, __LINE__, "unimplemented opcode: %x (%s)", opcode,
                 WasmOpcodes::OpcodeName(opcode));
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }
};

template <bool validate, typename Interface>
class WasmFullDecoder : public WasmDecoder<validate> {
  using Value = AbstractValue<Interface>;
  using Control = AbstractControl<Interface>;
  using MergeValues = AbstractMerge<Interface>;

  // All Value and Control types should be trivially copyable for
  // performance. We push and pop them, and store them in local variables.
  static_assert(IS_TRIVIALLY_COPYABLE(Value),
                "all Value<...> types should be trivially copyable");
  static_assert(IS_TRIVIALLY_COPYABLE(Control),
                "all Control<...> types should be trivially copyable");

 public:
  template <typename... InterfaceArgs>
  WasmFullDecoder(Zone* zone, const wasm::WasmModule* module,
                  const FunctionBody& body, InterfaceArgs&&... interface_args)
      : WasmDecoder<validate>(module, body.sig, body.start, body.end,
                              body.offset),
        zone_(zone),
        interface_(std::forward<InterfaceArgs>(interface_args)...),
        local_type_vec_(zone),
        stack_(zone),
        control_(zone),
        last_end_found_(false) {
    this->local_types_ = &local_type_vec_;
  }

  Interface& interface() { return interface_; }

  bool Decode() {
    DCHECK(stack_.empty());
    DCHECK(control_.empty());

    if (FLAG_wasm_code_fuzzer_gen_test) {
      PrintRawWasmCode(this->start_, this->end_);
    }
    base::ElapsedTimer decode_timer;
    if (FLAG_trace_wasm_decode_time) {
      decode_timer.Start();
    }

    if (this->end_ < this->pc_) {
      this->error("function body end < start");
      return false;
    }

    DCHECK_EQ(0, this->local_types_->size());
    WasmDecoder<validate>::DecodeLocals(this, this->sig_, this->local_types_);
    interface_.StartFunction(this);
    DecodeFunctionBody();
    if (!this->failed()) interface_.FinishFunction(this);

    if (this->failed()) return this->TraceFailed();

    if (!control_.empty()) {
      // Generate a better error message whether the unterminated control
      // structure is the function body block or an innner structure.
      if (control_.size() > 1) {
        this->error(control_.back().pc, "unterminated control structure");
      } else {
        this->error("function body must end with \"end\" opcode");
      }
      return TraceFailed();
    }

    if (!last_end_found_) {
      this->error("function body must end with \"end\" opcode");
      return false;
    }

    if (FLAG_trace_wasm_decode_time) {
      double ms = decode_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-decode %s (%0.3f ms)\n\n", this->ok() ? "ok" : "failed", ms);
    } else {
      TRACE("wasm-decode %s\n\n", this->ok() ? "ok" : "failed");
    }

    return true;
  }

  bool TraceFailed() {
    TRACE("wasm-error module+%-6d func+%d: %s\n\n", this->error_offset_,
          this->GetBufferRelativeOffset(this->error_offset_),
          this->error_msg_.c_str());
    return false;
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= this->end_) return "<end>";
    return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(*pc));
  }

  inline Zone* zone() const { return zone_; }

  inline uint32_t NumLocals() {
    return static_cast<uint32_t>(local_type_vec_.size());
  }

  inline ValueType GetLocalType(uint32_t index) {
    return local_type_vec_[index];
  }

  inline wasm::WasmCodePosition position() {
    int offset = static_cast<int>(this->pc_ - this->start_);
    DCHECK_EQ(this->pc_ - this->start_, offset);  // overflows cannot happen
    return offset;
  }

  inline uint32_t control_depth() const {
    return static_cast<uint32_t>(control_.size());
  }

  inline Control* control_at(uint32_t depth) {
    DCHECK_GT(control_.size(), depth);
    return &control_[control_.size() - depth - 1];
  }

  inline uint32_t stack_size() const {
    return static_cast<uint32_t>(stack_.size());
  }

  inline Value* stack_value(uint32_t depth) {
    DCHECK_GT(stack_.size(), depth);
    return &stack_[stack_.size() - depth - 1];
  }

  inline const Value& GetMergeValueFromStack(Control* c, size_t i) {
    DCHECK_GT(c->merge.arity, i);
    DCHECK_GE(stack_.size(), c->merge.arity);
    return stack_[stack_.size() - c->merge.arity + i];
  }

 private:
  static constexpr size_t kErrorMsgSize = 128;

  Zone* zone_;

  Interface interface_;

  ZoneVector<ValueType> local_type_vec_;  // types of local variables.
  ZoneVector<Value> stack_;               // stack of values.
  ZoneVector<Control> control_;           // stack of blocks, loops, and ifs.
  bool last_end_found_;

  bool CheckHasMemory() {
    if (VALIDATE(this->module_->has_memory)) return true;
    this->error(this->pc_ - 1, "memory instruction with no memory");
    return false;
  }

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n",
          reinterpret_cast<const void*>(this->start()),
          reinterpret_cast<const void*>(this->end()), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      auto* c = PushBlock();
      c->merge.arity = static_cast<uint32_t>(this->sig_->return_count());

      if (c->merge.arity == 1) {
        c->merge.vals.first = Value::New(this->pc_, this->sig_->GetReturn(0));
      } else if (c->merge.arity > 1) {
        c->merge.vals.array = zone_->NewArray<Value>(c->merge.arity);
        for (unsigned i = 0; i < c->merge.arity; i++) {
          c->merge.vals.array[i] =
              Value::New(this->pc_, this->sig_->GetReturn(i));
        }
      }
      interface_.StartFunctionBody(this, c);
    }

    while (this->pc_ < this->end_) {  // decoding loop.
      unsigned len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*this->pc_);
#if DEBUG
      if (FLAG_trace_wasm_decoder && !WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE("  @%-8d #%-20s|", startrel(this->pc_),
              WasmOpcodes::OpcodeName(opcode));
      }
#endif

      FunctionSig* sig = WasmOpcodes::Signature(opcode);
      if (sig) {
        BuildSimpleOperator(opcode, sig);
      } else {
        // Complex bytecode.
        switch (opcode) {
          case kExprNop:
            break;
          case kExprBlock: {
            BlockTypeOperand<validate> operand(this, this->pc_);
            auto* block = PushBlock();
            SetBlockType(block, operand);
            len = 1 + operand.length;
            interface_.Block(this, block);
            break;
          }
          case kExprRethrow: {
            // TODO(kschimpf): Implement.
            CHECK_PROTOTYPE_OPCODE(eh);
            OPCODE_ERROR(opcode, "not implemented yet");
            break;
          }
          case kExprThrow: {
            CHECK_PROTOTYPE_OPCODE(eh);
            ExceptionIndexOperand<true> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            if (operand.exception->sig->parameter_count() > 0) {
              // TODO(kschimpf): Fix to pull values off stack and build throw.
              OPCODE_ERROR(opcode, "can't handle exceptions with values yet");
              break;
            }
            interface_.Throw(this, operand);
            // TODO(titzer): Throw should end control, but currently we build a
            // (reachable) runtime call instead of connecting it directly to
            // end.
            //            EndControl();
            break;
          }
          case kExprTry: {
            CHECK_PROTOTYPE_OPCODE(eh);
            BlockTypeOperand<validate> operand(this, this->pc_);
            auto* try_block = PushTry();
            SetBlockType(try_block, operand);
            len = 1 + operand.length;
            interface_.Try(this, try_block);
            break;
          }
          case kExprCatch: {
            // TODO(kschimpf): Fix to use type signature of exception.
            CHECK_PROTOTYPE_OPCODE(eh);
            ExceptionIndexOperand<true> operand(this, this->pc_);
            len = 1 + operand.length;

            if (!this->Validate(this->pc_, operand)) break;

            if (CHECK_ERROR(control_.empty())) {
              this->error("catch does not match any try");
              break;
            }

            Control* c = &control_.back();
            if (CHECK_ERROR(!c->is_try())) {
              this->error("catch does not match any try");
              break;
            }

            if (CHECK_ERROR(c->is_try_catch())) {
              OPCODE_ERROR(opcode, "multiple catch blocks not implemented");
              break;
            }
            c->kind = kControlTryCatch;
            FallThruTo(c);
            stack_.resize(c->stack_depth);

            interface_.Catch(this, operand, c);
            break;
          }
          case kExprCatchAll: {
            // TODO(kschimpf): Implement.
            CHECK_PROTOTYPE_OPCODE(eh);
            OPCODE_ERROR(opcode, "not implemented yet");
            break;
          }
          case kExprLoop: {
            BlockTypeOperand<validate> operand(this, this->pc_);
            // The continue environment is the inner environment.
            auto* block = PushLoop();
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            interface_.Loop(this, block);
            break;
          }
          case kExprIf: {
            // Condition on top of stack. Split environments for branches.
            BlockTypeOperand<validate> operand(this, this->pc_);
            auto cond = Pop(0, kWasmI32);
            auto* if_block = PushIf();
            SetBlockType(if_block, operand);
            interface_.If(this, cond, if_block);
            len = 1 + operand.length;
            break;
          }
          case kExprElse: {
            if (CHECK_ERROR(control_.empty())) {
              this->error("else does not match any if");
              break;
            }
            Control* c = &control_.back();
            if (CHECK_ERROR(!c->is_if())) {
              this->error(this->pc_, "else does not match an if");
              break;
            }
            if (c->is_if_else()) {
              this->error(this->pc_, "else already present for if");
              break;
            }
            c->kind = kControlIfElse;
            FallThruTo(c);
            stack_.resize(c->stack_depth);
            interface_.Else(this, c);
            break;
          }
          case kExprEnd: {
            if (CHECK_ERROR(control_.empty())) {
              this->error("end does not match any if, try, or block");
              return;
            }
            Control* c = &control_.back();
            if (c->is_loop()) {
              // A loop just leaves the values on the stack.
              TypeCheckFallThru(c);
              if (c->unreachable) PushEndValues(c);
              PopControl(c);
              break;
            }
            if (c->is_onearmed_if()) {
              // End the true branch of a one-armed if.
              if (CHECK_ERROR(!c->unreachable &&
                              stack_.size() != c->stack_depth)) {
                this->error("end of if expected empty stack");
                stack_.resize(c->stack_depth);
              }
              if (CHECK_ERROR(c->merge.arity > 0)) {
                this->error("non-void one-armed if");
              }
            } else if (CHECK_ERROR(c->is_incomplete_try())) {
              this->error(this->pc_, "missing catch in try");
              break;
            }
            FallThruTo(c);
            PushEndValues(c);

            if (control_.size() == 1) {
              // If at the last (implicit) control, check we are at end.
              if (CHECK_ERROR(this->pc_ + 1 != this->end_)) {
                this->error(this->pc_ + 1, "trailing code after function end");
                break;
              }
              last_end_found_ = true;
              if (c->unreachable) {
                TypeCheckFallThru(c);
              } else {
                // The result of the block is the return value.
                TRACE("  @%-8d #xx:%-20s|", startrel(this->pc_),
                      "(implicit) return");
                DoReturn();
                TRACE("\n");
              }
            }
            PopControl(c);
            break;
          }
          case kExprSelect: {
            auto cond = Pop(2, kWasmI32);
            auto fval = Pop();
            auto tval = Pop(0, fval.type);
            auto* result = Push(tval.type == kWasmVar ? fval.type : tval.type);
            interface_.Select(this, cond, fval, tval, result);
            break;
          }
          case kExprBr: {
            BreakDepthOperand<validate> operand(this, this->pc_);
            if (VALIDATE(this->Validate(this->pc_, operand, control_.size()) &&
                         TypeCheckBreak(operand.depth))) {
              interface_.BreakTo(this, control_at(operand.depth));
            }
            len = 1 + operand.length;
            EndControl();
            break;
          }
          case kExprBrIf: {
            BreakDepthOperand<validate> operand(this, this->pc_);
            auto cond = Pop(0, kWasmI32);
            if (VALIDATE(this->ok() &&
                         this->Validate(this->pc_, operand, control_.size()) &&
                         TypeCheckBreak(operand.depth))) {
              interface_.BrIf(this, cond, control_at(operand.depth));
            }
            len = 1 + operand.length;
            break;
          }
          case kExprBrTable: {
            BranchTableOperand<validate> operand(this, this->pc_);
            BranchTableIterator<validate> iterator(this, operand);
            if (!this->Validate(this->pc_, operand, control_.size())) break;
            auto key = Pop(0, kWasmI32);
            MergeValues* merge = nullptr;
            while (iterator.has_next()) {
              const uint32_t i = iterator.cur_index();
              const byte* pos = iterator.pc();
              uint32_t target = iterator.next();
              if (CHECK_ERROR(target >= control_.size())) {
                this->error(pos, "improper branch in br_table");
                break;
              }
              // Check that label types match up.
              static MergeValues loop_dummy = {0, {nullptr}};
              Control* c = control_at(target);
              MergeValues* current = c->is_loop() ? &loop_dummy : &c->merge;
              if (i == 0) {
                merge = current;
              } else if (CHECK_ERROR(merge->arity != current->arity)) {
                this->errorf(pos,
                             "inconsistent arity in br_table target %d"
                             " (previous was %u, this one %u)",
                             i, merge->arity, current->arity);
              } else if (control_at(0)->unreachable) {
                for (uint32_t j = 0; VALIDATE(this->ok()) && j < merge->arity;
                     ++j) {
                  if (CHECK_ERROR((*merge)[j].type != (*current)[j].type)) {
                    this->errorf(pos,
                                 "type error in br_table target %d operand %d"
                                 " (previous expected %s, this one %s)",
                                 i, j, WasmOpcodes::TypeName((*merge)[j].type),
                                 WasmOpcodes::TypeName((*current)[j].type));
                  }
                }
              }
              bool valid = TypeCheckBreak(target);
              if (CHECK_ERROR(!valid)) break;
            }
            if (CHECK_ERROR(this->failed())) break;

            if (operand.table_count > 0) {
              interface_.BrTable(this, operand, key);
            } else {
              // Only a default target. Do the equivalent of br.
              BranchTableIterator<validate> iterator(this, operand);
              const byte* pos = iterator.pc();
              uint32_t target = iterator.next();
              if (CHECK_ERROR(target >= control_.size())) {
                this->error(pos, "improper branch in br_table");
                break;
              }
              interface_.BreakTo(this, control_at(target));
            }
            len = 1 + iterator.length();
            EndControl();
            break;
          }
          case kExprReturn: {
            DoReturn();
            break;
          }
          case kExprUnreachable: {
            interface_.Unreachable(this);
            EndControl();
            break;
          }
          case kExprI32Const: {
            ImmI32Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmI32);
            interface_.I32Const(this, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprI64Const: {
            ImmI64Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmI64);
            interface_.I64Const(this, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprF32Const: {
            ImmF32Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmF32);
            interface_.F32Const(this, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprF64Const: {
            ImmF64Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmF64);
            interface_.F64Const(this, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprGetLocal: {
            LocalIndexOperand<validate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto* value = Push(operand.type);
            interface_.GetLocal(this, value, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprSetLocal: {
            LocalIndexOperand<validate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto value = Pop(0, local_type_vec_[operand.index]);
            interface_.SetLocal(this, value, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprTeeLocal: {
            LocalIndexOperand<validate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto value = Pop(0, local_type_vec_[operand.index]);
            auto* result = Push(value.type);
            interface_.TeeLocal(this, value, result, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprDrop: {
            Pop();
            break;
          }
          case kExprGetGlobal: {
            GlobalIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            auto* result = Push(operand.type);
            interface_.GetGlobal(this, result, operand);
            break;
          }
          case kExprSetGlobal: {
            GlobalIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            if (CHECK_ERROR(!operand.global->mutability)) {
              this->errorf(this->pc_, "immutable global #%u cannot be assigned",
                           operand.index);
              break;
            }
            auto value = Pop(0, operand.type);
            interface_.SetGlobal(this, value, operand);
            break;
          }
          case kExprI32LoadMem8S:
            len = DecodeLoadMem(kWasmI32, MachineType::Int8());
            break;
          case kExprI32LoadMem8U:
            len = DecodeLoadMem(kWasmI32, MachineType::Uint8());
            break;
          case kExprI32LoadMem16S:
            len = DecodeLoadMem(kWasmI32, MachineType::Int16());
            break;
          case kExprI32LoadMem16U:
            len = DecodeLoadMem(kWasmI32, MachineType::Uint16());
            break;
          case kExprI32LoadMem:
            len = DecodeLoadMem(kWasmI32, MachineType::Int32());
            break;
          case kExprI64LoadMem8S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int8());
            break;
          case kExprI64LoadMem8U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint8());
            break;
          case kExprI64LoadMem16S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int16());
            break;
          case kExprI64LoadMem16U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint16());
            break;
          case kExprI64LoadMem32S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int32());
            break;
          case kExprI64LoadMem32U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint32());
            break;
          case kExprI64LoadMem:
            len = DecodeLoadMem(kWasmI64, MachineType::Int64());
            break;
          case kExprF32LoadMem:
            len = DecodeLoadMem(kWasmF32, MachineType::Float32());
            break;
          case kExprF64LoadMem:
            len = DecodeLoadMem(kWasmF64, MachineType::Float64());
            break;
          case kExprI32StoreMem8:
            len = DecodeStoreMem(kWasmI32, MachineType::Int8());
            break;
          case kExprI32StoreMem16:
            len = DecodeStoreMem(kWasmI32, MachineType::Int16());
            break;
          case kExprI32StoreMem:
            len = DecodeStoreMem(kWasmI32, MachineType::Int32());
            break;
          case kExprI64StoreMem8:
            len = DecodeStoreMem(kWasmI64, MachineType::Int8());
            break;
          case kExprI64StoreMem16:
            len = DecodeStoreMem(kWasmI64, MachineType::Int16());
            break;
          case kExprI64StoreMem32:
            len = DecodeStoreMem(kWasmI64, MachineType::Int32());
            break;
          case kExprI64StoreMem:
            len = DecodeStoreMem(kWasmI64, MachineType::Int64());
            break;
          case kExprF32StoreMem:
            len = DecodeStoreMem(kWasmF32, MachineType::Float32());
            break;
          case kExprF64StoreMem:
            len = DecodeStoreMem(kWasmF64, MachineType::Float64());
            break;
          case kExprGrowMemory: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            DCHECK_NOT_NULL(this->module_);
            if (CHECK_ERROR(!this->module_->is_wasm())) {
              this->error("grow_memory is not supported for asmjs modules");
              break;
            }
            auto value = Pop(0, kWasmI32);
            auto* result = Push(kWasmI32);
            interface_.GrowMemory(this, value, result);
            break;
          }
          case kExprMemorySize: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<validate> operand(this, this->pc_);
            auto* result = Push(kWasmI32);
            len = 1 + operand.length;
            interface_.CurrentMemoryPages(this, result);
            break;
          }
          case kExprCallFunction: {
            CallFunctionOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            // TODO(clemensh): Better memory management.
            std::vector<Value> args;
            PopArgs(operand.sig, &args);
            auto* returns = PushReturns(operand.sig);
            interface_.CallDirect(this, operand, args.data(), returns);
            break;
          }
          case kExprCallIndirect: {
            CallIndirectOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            auto index = Pop(0, kWasmI32);
            // TODO(clemensh): Better memory management.
            std::vector<Value> args;
            PopArgs(operand.sig, &args);
            auto* returns = PushReturns(operand.sig);
            interface_.CallIndirect(this, index, operand, args.data(), returns);
            break;
          }
          case kSimdPrefix: {
            CHECK_PROTOTYPE_OPCODE(simd);
            len++;
            byte simd_index =
                this->template read_u8<validate>(this->pc_ + 1, "simd index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
            TRACE("  @%-4d #%-20s|", startrel(this->pc_),
                  WasmOpcodes::OpcodeName(opcode));
            len += DecodeSimdOpcode(opcode);
            break;
          }
          case kAtomicPrefix: {
            CHECK_PROTOTYPE_OPCODE(threads);
            len++;
            byte atomic_index =
                this->template read_u8<validate>(this->pc_ + 1, "atomic index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | atomic_index);
            TRACE("  @%-4d #%-20s|", startrel(this->pc_),
                  WasmOpcodes::OpcodeName(opcode));
            len += DecodeAtomicOpcode(opcode);
            break;
          }
          default: {
            // Deal with special asmjs opcodes.
            if (this->module_ != nullptr && this->module_->is_asm_js()) {
              sig = WasmOpcodes::AsmjsSignature(opcode);
              if (sig) {
                BuildSimpleOperator(opcode, sig);
              }
            } else {
              this->error("Invalid opcode");
              return;
            }
          }
        }
      }

#if DEBUG
      if (FLAG_trace_wasm_decoder) {
        PrintF(" ");
        for (size_t i = 0; i < control_.size(); ++i) {
          Control* c = &control_[i];
          switch (c->kind) {
            case kControlIf:
              PrintF("I");
              break;
            case kControlBlock:
              PrintF("B");
              break;
            case kControlLoop:
              PrintF("L");
              break;
            case kControlTry:
              PrintF("T");
              break;
            default:
              break;
          }
          PrintF("%u", c->merge.arity);
          if (c->unreachable) PrintF("*");
        }
        PrintF(" | ");
        for (size_t i = 0; i < stack_.size(); ++i) {
          auto& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = static_cast<WasmOpcode>(opcode << 8 | *(val.pc + 1));
          }
          PrintF(" %c@%d:%s", WasmOpcodes::ShortNameOf(val.type),
                 static_cast<int>(val.pc - this->start_),
                 WasmOpcodes::OpcodeName(opcode));
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Operand<validate> operand(this, val.pc);
              PrintF("[%d]", operand.value);
              break;
            }
            case kExprGetLocal: {
              LocalIndexOperand<validate> operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            case kExprSetLocal:  // fallthru
            case kExprTeeLocal: {
              LocalIndexOperand<validate> operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            default:
              break;
          }
        }
        PrintF("\n");
      }
#endif
      this->pc_ += len;
    }  // end decode loop
    if (this->pc_ > this->end_ && this->ok()) this->error("Beyond end of code");
  }

  void EndControl() {
    DCHECK(!control_.empty());
    auto* current = &control_.back();
    stack_.resize(current->stack_depth);
    current->unreachable = true;
    interface_.EndControl(this, current);
  }

  void SetBlockType(Control* c, BlockTypeOperand<validate>& operand) {
    c->merge.arity = operand.arity;
    if (c->merge.arity == 1) {
      c->merge.vals.first = Value::New(this->pc_, operand.read_entry(0));
    } else if (c->merge.arity > 1) {
      c->merge.vals.array = zone_->NewArray<Value>(c->merge.arity);
      for (unsigned i = 0; i < c->merge.arity; i++) {
        c->merge.vals.array[i] = Value::New(this->pc_, operand.read_entry(i));
      }
    }
  }

  // TODO(clemensh): Better memory management.
  void PopArgs(FunctionSig* sig, std::vector<Value>* result) {
    DCHECK(result->empty());
    int count = static_cast<int>(sig->parameter_count());
    result->resize(count);
    for (int i = count - 1; i >= 0; --i) {
      (*result)[i] = Pop(i, sig->GetParam(i));
    }
  }

  ValueType GetReturnType(FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    return sig->return_count() == 0 ? kWasmStmt : sig->GetReturn();
  }

  Control* PushBlock() {
    control_.emplace_back(Control::Block(this->pc_, stack_.size()));
    return &control_.back();
  }

  Control* PushLoop() {
    control_.emplace_back(Control::Loop(this->pc_, stack_.size()));
    return &control_.back();
  }

  Control* PushIf() {
    control_.emplace_back(Control::If(this->pc_, stack_.size()));
    return &control_.back();
  }

  Control* PushTry() {
    control_.emplace_back(Control::Try(this->pc_, stack_.size()));
    // current_catch_ = static_cast<int32_t>(control_.size() - 1);
    return &control_.back();
  }

  void PopControl(Control* c) {
    DCHECK_EQ(c, &control_.back());
    interface_.PopControl(this, *c);
    control_.pop_back();
  }

  int DecodeLoadMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(
        this, this->pc_, ElementSizeLog2Of(mem_type.representation()));

    auto index = Pop(0, kWasmI32);
    auto* result = Push(type);
    interface_.LoadMem(this, type, mem_type, operand, index, result);
    return 1 + operand.length;
  }

  int DecodeStoreMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(
        this, this->pc_, ElementSizeLog2Of(mem_type.representation()));
    auto value = Pop(1, type);
    auto index = Pop(0, kWasmI32);
    interface_.StoreMem(this, type, mem_type, operand, index, value);
    return 1 + operand.length;
  }

  int DecodePrefixedLoadMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(
        this, this->pc_ + 1, ElementSizeLog2Of(mem_type.representation()));

    auto index = Pop(0, kWasmI32);
    auto* result = Push(type);
    interface_.LoadMem(this, type, mem_type, operand, index, result);
    return operand.length;
  }

  int DecodePrefixedStoreMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(
        this, this->pc_ + 1, ElementSizeLog2Of(mem_type.representation()));
    auto value = Pop(1, type);
    auto index = Pop(0, kWasmI32);
    interface_.StoreMem(this, type, mem_type, operand, index, value);
    return operand.length;
  }

  unsigned SimdExtractLane(WasmOpcode opcode, ValueType type) {
    SimdLaneOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, opcode, operand)) {
      Value inputs[] = {Pop(0, ValueType::kSimd128)};
      auto* result = Push(type);
      interface_.SimdLaneOp(this, opcode, operand, ArrayVector(inputs), result);
    }
    return operand.length;
  }

  unsigned SimdReplaceLane(WasmOpcode opcode, ValueType type) {
    SimdLaneOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, opcode, operand)) {
      Value inputs[2];
      inputs[1] = Pop(1, type);
      inputs[0] = Pop(0, ValueType::kSimd128);
      auto* result = Push(ValueType::kSimd128);
      interface_.SimdLaneOp(this, opcode, operand, ArrayVector(inputs), result);
    }
    return operand.length;
  }

  unsigned SimdShiftOp(WasmOpcode opcode) {
    SimdShiftOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, opcode, operand)) {
      auto input = Pop(0, ValueType::kSimd128);
      auto* result = Push(ValueType::kSimd128);
      interface_.SimdShiftOp(this, opcode, operand, input, result);
    }
    return operand.length;
  }

  unsigned Simd8x16ShuffleOp() {
    Simd8x16ShuffleOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, operand)) {
      auto input1 = Pop(1, ValueType::kSimd128);
      auto input0 = Pop(0, ValueType::kSimd128);
      auto* result = Push(ValueType::kSimd128);
      interface_.Simd8x16ShuffleOp(this, operand, input0, input1, result);
    }
    return 16;
  }

  unsigned DecodeSimdOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane: {
        len = SimdExtractLane(opcode, ValueType::kFloat32);
        break;
      }
      case kExprI32x4ExtractLane:
      case kExprI16x8ExtractLane:
      case kExprI8x16ExtractLane: {
        len = SimdExtractLane(opcode, ValueType::kWord32);
        break;
      }
      case kExprF32x4ReplaceLane: {
        len = SimdReplaceLane(opcode, ValueType::kFloat32);
        break;
      }
      case kExprI32x4ReplaceLane:
      case kExprI16x8ReplaceLane:
      case kExprI8x16ReplaceLane: {
        len = SimdReplaceLane(opcode, ValueType::kWord32);
        break;
      }
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU: {
        len = SimdShiftOp(opcode);
        break;
      }
      case kExprS8x16Shuffle: {
        len = Simd8x16ShuffleOp();
        break;
      }
      case kExprS128LoadMem:
        len = DecodePrefixedLoadMem(kWasmS128, MachineType::Simd128());
        break;
      case kExprS128StoreMem:
        len = DecodePrefixedStoreMem(kWasmS128, MachineType::Simd128());
        break;
      default: {
        FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (CHECK_ERROR(sig == nullptr)) {
          this->error("invalid simd opcode");
          break;
        }
        std::vector<Value> args;
        PopArgs(sig, &args);
        auto* result =
            sig->return_count() == 0 ? nullptr : Push(GetReturnType(sig));
        interface_.SimdOp(this, opcode, vec2vec(args), result);
      }
    }
    return len;
  }

  unsigned DecodeAtomicOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    FunctionSig* sig = WasmOpcodes::AtomicSignature(opcode);
    if (sig != nullptr) {
      // TODO(clemensh): Better memory management here.
      std::vector<Value> args(sig->parameter_count());
      for (int i = static_cast<int>(sig->parameter_count() - 1); i >= 0; --i) {
        args[i] = Pop(i, sig->GetParam(i));
      }
      auto* result = Push(GetReturnType(sig));
      interface_.AtomicOp(this, opcode, vec2vec(args), result);
    } else {
      this->error("invalid atomic opcode");
    }
    return len;
  }

  void DoReturn() {
    // TODO(clemensh): Optimize memory usage here (it will be mostly 0 or 1
    // returned values).
    int return_count = static_cast<int>(this->sig_->return_count());
    std::vector<Value> values(return_count);

    // Pop return values off the stack in reverse order.
    for (int i = return_count - 1; i >= 0; --i) {
      values[i] = Pop(i, this->sig_->GetReturn(i));
    }

    interface_.DoReturn(this, vec2vec(values));
    EndControl();
  }

  inline Value* Push(ValueType type) {
    DCHECK(type != kWasmStmt);
    stack_.push_back(Value::New(this->pc_, type));
    return &stack_.back();
  }

  void PushEndValues(Control* c) {
    DCHECK_EQ(c, &control_.back());
    stack_.resize(c->stack_depth);
    if (c->merge.arity == 1) {
      stack_.push_back(c->merge.vals.first);
    } else {
      for (unsigned i = 0; i < c->merge.arity; i++) {
        stack_.push_back(c->merge.vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + c->merge.arity, stack_.size());
  }

  Value* PushReturns(FunctionSig* sig) {
    size_t return_count = sig->return_count();
    if (return_count == 0) return nullptr;
    size_t old_size = stack_.size();
    for (size_t i = 0; i < return_count; ++i) {
      Push(sig->GetReturn(i));
    }
    return stack_.data() + old_size;
  }

  Value Pop(int index, ValueType expected) {
    auto val = Pop();
    if (CHECK_ERROR(val.type != expected && val.type != kWasmVar &&
                    expected != kWasmVar)) {
      this->errorf(val.pc, "%s[%d] expected type %s, found %s of type %s",
                   SafeOpcodeNameAt(this->pc_), index,
                   WasmOpcodes::TypeName(expected), SafeOpcodeNameAt(val.pc),
                   WasmOpcodes::TypeName(val.type));
    }
    return val;
  }

  Value Pop() {
    DCHECK(!control_.empty());
    size_t limit = control_.back().stack_depth;
    if (stack_.size() <= limit) {
      // Popping past the current control start in reachable code.
      if (CHECK_ERROR(!control_.back().unreachable)) {
        this->errorf(this->pc_, "%s found empty stack",
                     SafeOpcodeNameAt(this->pc_));
      }
      return Value::Unreachable(this->pc_);
    }
    auto val = stack_.back();
    stack_.pop_back();
    return val;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - this->start_); }

  bool TypeCheckBreak(unsigned depth) {
    DCHECK(validate);  // Only call this for validation.
    Control* c = control_at(depth);
    if (c->is_loop()) {
      // This is the inner loop block, which does not have a value.
      return true;
    }
    size_t expected = control_.back().stack_depth + c->merge.arity;
    if (stack_.size() < expected && !control_.back().unreachable) {
      this->errorf(
          this->pc_,
          "expected at least %u values on the stack for br to @%d, found %d",
          c->merge.arity, startrel(c->pc),
          static_cast<int>(stack_.size() - c->stack_depth));
      return false;
    }

    return TypeCheckMergeValues(c);
  }

  void FallThruTo(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!TypeCheckFallThru(c)) return;
    c->unreachable = false;

    interface_.FallThruTo(this, c);
  }

  bool TypeCheckMergeValues(Control* c) {
    // Typecheck the values left on the stack.
    size_t avail = stack_.size() - c->stack_depth;
    size_t start = avail >= c->merge.arity ? 0 : c->merge.arity - avail;
    for (size_t i = start; i < c->merge.arity; ++i) {
      auto& val = GetMergeValueFromStack(c, i);
      auto& old = c->merge[i];
      if (val.type != old.type && val.type != kWasmVar) {
        this->errorf(
            this->pc_, "type error in merge[%zu] (expected %s, got %s)", i,
            WasmOpcodes::TypeName(old.type), WasmOpcodes::TypeName(val.type));
        return false;
      }
    }

    return true;
  }

  bool TypeCheckFallThru(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!validate) return true;
    // Fallthru must match arity exactly.
    size_t expected = c->stack_depth + c->merge.arity;
    if (stack_.size() != expected &&
        (stack_.size() > expected || !c->unreachable)) {
      this->errorf(this->pc_,
                   "expected %u elements on the stack for fallthru to @%d",
                   c->merge.arity, startrel(c->pc));
      return false;
    }

    return TypeCheckMergeValues(c);
  }

  virtual void onFirstError() {
    this->end_ = this->pc_;  // Terminate decoding loop.
    TRACE(" !%s\n", this->error_msg_.c_str());
  }

  inline void BuildSimpleOperator(WasmOpcode opcode, FunctionSig* sig) {
    switch (sig->parameter_count()) {
      case 1: {
        auto val = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        interface_.UnOp(this, opcode, sig, val, ret);
        break;
      }
      case 2: {
        auto rval = Pop(1, sig->GetParam(1));
        auto lval = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        interface_.BinOp(this, opcode, sig, lval, rval, ret);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
};

template <bool decoder_validate, typename Interface>
class InterfaceTemplate {
 public:
  constexpr static bool validate = decoder_validate;
  using Decoder = WasmFullDecoder<validate, Interface>;
  using Control = AbstractControl<Interface>;
  using Value = AbstractValue<Interface>;
  using MergeValues = AbstractMerge<Interface>;
};

class EmptyInterface : public InterfaceTemplate<true, EmptyInterface> {
 public:
  struct IValue {
    static IValue Unreachable() { return {}; }
    static IValue New() { return {}; }
  };
  struct IControl {
    static IControl Block() { return {}; }
    static IControl If() { return {}; }
    static IControl Loop() { return {}; }
    static IControl Try() { return {}; }
  };

#define DEFINE_EMPTY_CALLBACK(name, ...) \
  void name(Decoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_FUNCTIONS(DEFINE_EMPTY_CALLBACK)
#undef DEFINE_EMPTY_CALLBACK
};

#undef CHECKED_COND
#undef VALIDATE
#undef CHECK_ERROR
#undef TRACE
#undef CHECK_PROTOTYPE_OPCODE
#undef PROTOTYPE_NOT_FUNCTIONAL

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
