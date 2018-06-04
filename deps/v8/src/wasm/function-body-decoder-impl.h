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

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST_FORMAT "  @%-8d #%-20s|"

// Return the evaluation of `condition` if validate==true, DCHECK that it's
// true and always return true otherwise.
#define VALIDATE(condition)       \
  (validate ? (condition) : [&] { \
    DCHECK(condition);            \
    return true;                  \
  }())

#define RET_ON_PROTOTYPE_OPCODE(flag)                                          \
  DCHECK(!this->module_ || !this->module_->is_asm_js());                       \
  if (!FLAG_experimental_wasm_##flag) {                                        \
    this->error("Invalid opcode (enable with --experimental-wasm-" #flag ")"); \
  }

#define CHECK_PROTOTYPE_OPCODE(flag)                                           \
  DCHECK(!this->module_ || !this->module_->is_asm_js());                       \
  if (!FLAG_experimental_wasm_##flag) {                                        \
    this->error("Invalid opcode (enable with --experimental-wasm-" #flag ")"); \
    break;                                                                     \
  }

#define OPCODE_ERROR(opcode, message)                                 \
  (this->errorf(this->pc_, "%s: %s", WasmOpcodes::OpcodeName(opcode), \
                (message)))

#define ATOMIC_OP_LIST(V)                \
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

template <typename T, typename Allocator>
Vector<T> vec2vec(std::vector<T, Allocator>& vec) {
  return Vector<T>(vec.data(), vec.size());
}


// Helpers for decoding different kinds of operands which follow bytecodes.
template <Decoder::ValidateFlag validate>
struct LocalIndexOperand {
  uint32_t index;
  ValueType type = kWasmStmt;
  unsigned length;

  inline LocalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "local index");
  }
};

template <Decoder::ValidateFlag validate>
struct ExceptionIndexOperand {
  uint32_t index;
  const WasmException* exception = nullptr;
  unsigned length;

  inline ExceptionIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "exception index");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI32Operand {
  int32_t value;
  unsigned length;
  inline ImmI32Operand(Decoder* decoder, const byte* pc) {
    value = decoder->read_i32v<validate>(pc + 1, &length, "immi32");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI64Operand {
  int64_t value;
  unsigned length;
  inline ImmI64Operand(Decoder* decoder, const byte* pc) {
    value = decoder->read_i64v<validate>(pc + 1, &length, "immi64");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF32Operand {
  float value;
  unsigned length = 4;
  inline ImmF32Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint32_t tmp = decoder->read_u32<validate>(pc + 1, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF64Operand {
  double value;
  unsigned length = 8;
  inline ImmF64Operand(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->read_u64<validate>(pc + 1, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct GlobalIndexOperand {
  uint32_t index;
  ValueType type = kWasmStmt;
  const WasmGlobal* global = nullptr;
  unsigned length;

  inline GlobalIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "global index");
  }
};

template <Decoder::ValidateFlag validate>
struct BlockTypeOperand {
  unsigned length = 1;
  ValueType type = kWasmStmt;
  uint32_t sig_index = 0;
  FunctionSig* sig = nullptr;

  inline BlockTypeOperand(Decoder* decoder, const byte* pc) {
    uint8_t val = decoder->read_u8<validate>(pc + 1, "block type");
    if (!decode_local_type(val, &type)) {
      // Handle multi-value blocks.
      if (!VALIDATE(FLAG_experimental_wasm_mv)) {
        decoder->error(pc + 1, "invalid block type");
        return;
      }
      int32_t index =
          decoder->read_i32v<validate>(pc + 1, &length, "block arity");
      if (!VALIDATE(length > 0 && index >= 0)) {
        decoder->error(pc + 1, "invalid block type index");
        return;
      }
      sig_index = static_cast<uint32_t>(index);
    }
  }

  // Decode a byte representing a local type. Return {false} if the encoded
  // byte was invalid or the start of a type index.
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
      case kLocalAnyRef:
        *result = kWasmAnyRef;
        return true;
      default:
        *result = kWasmVar;
        return false;
    }
  }

  uint32_t in_arity() const {
    if (type != kWasmVar) return 0;
    return static_cast<uint32_t>(sig->parameter_count());
  }
  uint32_t out_arity() const {
    if (type == kWasmStmt) return 0;
    if (type != kWasmVar) return 1;
    return static_cast<uint32_t>(sig->return_count());
  }
  ValueType in_type(uint32_t index) {
    DCHECK_EQ(kWasmVar, type);
    return sig->GetParam(index);
  }
  ValueType out_type(uint32_t index) {
    if (type == kWasmVar) return sig->GetReturn(index);
    DCHECK_NE(kWasmStmt, type);
    DCHECK_EQ(0, index);
    return type;
  }
};

template <Decoder::ValidateFlag validate>
struct BreakDepthOperand {
  uint32_t depth;
  unsigned length;
  inline BreakDepthOperand(Decoder* decoder, const byte* pc) {
    depth = decoder->read_u32v<validate>(pc + 1, &length, "break depth");
  }
};

template <Decoder::ValidateFlag validate>
struct CallIndirectOperand {
  uint32_t table_index;
  uint32_t sig_index;
  FunctionSig* sig = nullptr;
  unsigned length = 0;
  inline CallIndirectOperand(Decoder* decoder, const byte* pc) {
    unsigned len = 0;
    sig_index = decoder->read_u32v<validate>(pc + 1, &len, "signature index");
    if (!VALIDATE(decoder->ok())) return;
    table_index = decoder->read_u8<validate>(pc + 1 + len, "table index");
    if (!VALIDATE(table_index == 0)) {
      decoder->errorf(pc + 1 + len, "expected table index 0, found %u",
                      table_index);
    }
    length = 1 + len;
  }
};

template <Decoder::ValidateFlag validate>
struct CallFunctionOperand {
  uint32_t index;
  FunctionSig* sig = nullptr;
  unsigned length;
  inline CallFunctionOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "function index");
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryIndexOperand {
  uint32_t index;
  unsigned length = 1;
  inline MemoryIndexOperand(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<validate>(pc + 1, "memory index");
    if (!VALIDATE(index == 0)) {
      decoder->errorf(pc + 1, "expected memory index 0, found %u", index);
    }
  }
};

template <Decoder::ValidateFlag validate>
struct BranchTableOperand {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  inline BranchTableOperand(Decoder* decoder, const byte* pc) {
    DCHECK_EQ(kExprBrTable, decoder->read_u8<validate>(pc, "opcode"));
    start = pc + 1;
    unsigned len = 0;
    table_count = decoder->read_u32v<validate>(pc + 1, &len, "table count");
    table = pc + 1 + len;
  }
};

// A helper to iterate over a branch table.
template <Decoder::ValidateFlag validate>
class BranchTableIterator {
 public:
  unsigned cur_index() { return index_; }
  bool has_next() { return decoder_->ok() && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    unsigned length;
    uint32_t result =
        decoder_->read_u32v<validate>(pc_, &length, "branch table entry");
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
                      const BranchTableOperand<validate>& operand)
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

template <Decoder::ValidateFlag validate>
struct MemoryAccessOperand {
  uint32_t alignment;
  uint32_t offset;
  unsigned length = 0;
  inline MemoryAccessOperand(Decoder* decoder, const byte* pc,
                             uint32_t max_alignment) {
    unsigned alignment_length;
    alignment =
        decoder->read_u32v<validate>(pc + 1, &alignment_length, "alignment");
    if (!VALIDATE(alignment <= max_alignment)) {
      decoder->errorf(pc + 1,
                      "invalid alignment; expected maximum alignment is %u, "
                      "actual alignment is %u",
                      max_alignment, alignment);
    }
    if (!VALIDATE(decoder->ok())) return;
    unsigned offset_length;
    offset = decoder->read_u32v<validate>(pc + 1 + alignment_length,
                                          &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Operand for SIMD lane operations.
template <Decoder::ValidateFlag validate>
struct SimdLaneOperand {
  uint8_t lane;
  unsigned length = 1;

  inline SimdLaneOperand(Decoder* decoder, const byte* pc) {
    lane = decoder->read_u8<validate>(pc + 2, "lane");
  }
};

// Operand for SIMD shift operations.
template <Decoder::ValidateFlag validate>
struct SimdShiftOperand {
  uint8_t shift;
  unsigned length = 1;

  inline SimdShiftOperand(Decoder* decoder, const byte* pc) {
    shift = decoder->read_u8<validate>(pc + 2, "shift");
  }
};

// Operand for SIMD S8x16 shuffle operations.
template <Decoder::ValidateFlag validate>
struct Simd8x16ShuffleOperand {
  uint8_t shuffle[kSimd128Size] = {0};

  inline Simd8x16ShuffleOperand(Decoder* decoder, const byte* pc) {
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      shuffle[i] = decoder->read_u8<validate>(pc + 2 + i, "shuffle");
      if (!VALIDATE(decoder->ok())) return;
    }
  }
};

// An entry on the value stack.
struct ValueBase {
  const byte* pc;
  ValueType type;

  // Named constructors.
  static ValueBase Unreachable(const byte* pc) { return {pc, kWasmVar}; }

  static ValueBase New(const byte* pc, ValueType type) { return {pc, type}; }
};

template <typename Value>
struct Merge {
  uint32_t arity;
  union {
    Value* array;
    Value first;
  } vals;  // Either multiple values or a single value.

  // Tracks whether this merge was ever reached. Uses precise reachability, like
  // Reachability::kReachable.
  bool reached;

  Merge(bool reached = false) : reached(reached) {}

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
  kControlTryCatch
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
template <typename Value>
struct ControlBase {
  ControlKind kind;
  uint32_t stack_depth;  // stack height at the beginning of the construct.
  const byte* pc;
  Reachability reachability = kReachable;

  // Values merged into the start or end of this control construct.
  Merge<Value> start_merge;
  Merge<Value> end_merge;

  ControlBase() = default;
  ControlBase(ControlKind kind, uint32_t stack_depth, const byte* pc)
      : kind(kind), stack_depth(stack_depth), pc(pc) {}

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
  bool is_try() const { return is_incomplete_try() || is_try_catch(); }
  bool is_incomplete_try() const { return kind == kControlTry; }
  bool is_try_catch() const { return kind == kControlTryCatch; }

  inline Merge<Value>* br_merge() {
    return is_loop() ? &this->start_merge : &this->end_merge;
  }

  // Named constructors.
  static ControlBase Block(const byte* pc, uint32_t stack_depth) {
    return {kControlBlock, stack_depth, pc};
  }

  static ControlBase If(const byte* pc, uint32_t stack_depth) {
    return {kControlIf, stack_depth, pc};
  }

  static ControlBase Loop(const byte* pc, uint32_t stack_depth) {
    return {kControlLoop, stack_depth, pc};
  }

  static ControlBase Try(const byte* pc, uint32_t stack_depth) {
    return {kControlTry, stack_depth, pc};
  }
};

#define CONCRETE_NAMED_CONSTRUCTOR(concrete_type, abstract_type, name) \
  template <typename... Args>                                          \
  static concrete_type name(Args&&... args) {                          \
    concrete_type val;                                                 \
    static_cast<abstract_type&>(val) =                                 \
        abstract_type::name(std::forward<Args>(args)...);              \
    return val;                                                        \
  }

// Provide the default named constructors, which default-initialize the
// ConcreteType and the initialize the fields of ValueBase correctly.
// Use like this:
// struct Value : public ValueWithNamedConstructors<Value> { int new_field; };
template <typename ConcreteType>
struct ValueWithNamedConstructors : public ValueBase {
  // Named constructors.
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ValueBase, Unreachable)
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ValueBase, New)
};

// Provide the default named constructors, which default-initialize the
// ConcreteType and the initialize the fields of ControlBase correctly.
// Use like this:
// struct Control : public ControlWithNamedConstructors<Control, Value> {
//   int my_uninitialized_field;
//   char* other_field = nullptr;
// };
template <typename ConcreteType, typename Value>
struct ControlWithNamedConstructors : public ControlBase<Value> {
  // Named constructors.
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ControlBase<Value>, Block)
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ControlBase<Value>, If)
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ControlBase<Value>, Loop)
  CONCRETE_NAMED_CONSTRUCTOR(ConcreteType, ControlBase<Value>, Try)
};

// This is the list of callback functions that an interface for the
// WasmFullDecoder should implement.
// F(Name, args...)
#define INTERFACE_FUNCTIONS(F)                                                 \
  /* General: */                                                               \
  F(StartFunction)                                                             \
  F(StartFunctionBody, Control* block)                                         \
  F(FinishFunction)                                                            \
  F(OnFirstError)                                                              \
  F(NextInstruction, WasmOpcode)                                               \
  /* Control: */                                                               \
  F(Block, Control* block)                                                     \
  F(Loop, Control* block)                                                      \
  F(Try, Control* block)                                                       \
  F(If, const Value& cond, Control* if_block)                                  \
  F(FallThruTo, Control* c)                                                    \
  F(PopControl, Control* block)                                                \
  F(EndControl, Control* block)                                                \
  /* Instructions: */                                                          \
  F(UnOp, WasmOpcode opcode, FunctionSig*, const Value& value, Value* result)  \
  F(BinOp, WasmOpcode opcode, FunctionSig*, const Value& lhs,                  \
    const Value& rhs, Value* result)                                           \
  F(I32Const, Value* result, int32_t value)                                    \
  F(I64Const, Value* result, int64_t value)                                    \
  F(F32Const, Value* result, float value)                                      \
  F(F64Const, Value* result, double value)                                     \
  F(RefNull, Value* result)                                                    \
  F(Drop, const Value& value)                                                  \
  F(DoReturn, Vector<Value> values, bool implicit)                             \
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
  F(Br, Control* target)                                                       \
  F(BrIf, const Value& cond, Control* target)                                  \
  F(BrTable, const BranchTableOperand<validate>& operand, const Value& key)    \
  F(Else, Control* if_block)                                                   \
  F(LoadMem, LoadType type, const MemoryAccessOperand<validate>& operand,      \
    const Value& index, Value* result)                                         \
  F(StoreMem, StoreType type, const MemoryAccessOperand<validate>& operand,    \
    const Value& index, const Value& value)                                    \
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
  F(Throw, const ExceptionIndexOperand<validate>&, Control* block,             \
    const Vector<Value>& args)                                                 \
  F(CatchException, const ExceptionIndexOperand<validate>& operand,            \
    Control* block, Vector<Value> caught_values)                               \
  F(AtomicOp, WasmOpcode opcode, Vector<Value> args,                           \
    const MemoryAccessOperand<validate>& operand, Value* result)

// Generic Wasm bytecode decoder with utilities for decoding operands,
// lengths, etc.
template <Decoder::ValidateFlag validate>
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

  uint32_t total_locals() const {
    return local_types_ == nullptr
               ? 0
               : static_cast<uint32_t>(local_types_->size());
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

      DCHECK_LE(type_list->size(), kV8MaxWasmFunctionLocals);
      if (count > kV8MaxWasmFunctionLocals - type_list->size()) {
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
        case kLocalAnyRef:
          if (FLAG_experimental_wasm_anyref) {
            type = kWasmAnyRef;
            break;
          }
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
        case kLocalS128:
          if (FLAG_experimental_wasm_simd) {
            type = kWasmS128;
            break;
          }
          V8_FALLTHROUGH;
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
                                          uint32_t locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    // The number of locals_count is augmented by 2 so that 'locals_count - 2'
    // can be used to track mem_size, and 'locals_count - 1' to track mem_start.
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
          LocalIndexOperand<Decoder::kValidate> operand(decoder, pc);
          if (assigned->length() > 0 &&
              operand.index < static_cast<uint32_t>(assigned->length())) {
            // Unverified code might have an out-of-bounds index.
            assigned->Add(operand.index);
          }
          length = 1 + operand.length;
          break;
        }
        case kExprGrowMemory:
        case kExprCallFunction:
        case kExprCallIndirect:
          // Add instance cache nodes to the assigned set.
          // TODO(titzer): make this more clear.
          assigned->Add(locals_count - 1);
          length = OpcodeLength(decoder, pc);
          break;
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

  inline bool Validate(const byte* pc,
                       LocalIndexOperand<Decoder::kValidate>& operand) {
    if (!VALIDATE(operand.index < total_locals())) {
      errorf(pc + 1, "invalid local index: %u", operand.index);
      return false;
    }
    operand.type = local_types_ ? local_types_->at(operand.index) : kWasmStmt;
    return true;
  }

  inline bool Validate(const byte* pc,
                       ExceptionIndexOperand<validate>& operand) {
    if (!VALIDATE(module_ != nullptr &&
                  operand.index < module_->exceptions.size())) {
      errorf(pc + 1, "Invalid exception index: %u", operand.index);
      return false;
    }
    operand.exception = &module_->exceptions[operand.index];
    return true;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand<validate>& operand) {
    if (!VALIDATE(module_ != nullptr &&
                  operand.index < module_->globals.size())) {
      errorf(pc + 1, "invalid global index: %u", operand.index);
      return false;
    }
    operand.global = &module_->globals[operand.index];
    operand.type = operand.global->type;
    return true;
  }

  inline bool Complete(const byte* pc, CallFunctionOperand<validate>& operand) {
    if (!VALIDATE(module_ != nullptr &&
                  operand.index < module_->functions.size())) {
      return false;
    }
    operand.sig = module_->functions[operand.index].sig;
    return true;
  }

  inline bool Validate(const byte* pc, CallFunctionOperand<validate>& operand) {
    if (Complete(pc, operand)) {
      return true;
    }
    errorf(pc + 1, "invalid function index: %u", operand.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectOperand<validate>& operand) {
    if (!VALIDATE(module_ != nullptr &&
                  operand.sig_index < module_->signatures.size())) {
      return false;
    }
    operand.sig = module_->signatures[operand.sig_index];
    return true;
  }

  inline bool Validate(const byte* pc, CallIndirectOperand<validate>& operand) {
    if (!VALIDATE(module_ != nullptr && !module_->function_tables.empty())) {
      error("function table has to exist to execute call_indirect");
      return false;
    }
    if (!Complete(pc, operand)) {
      errorf(pc + 1, "invalid signature index: #%u", operand.sig_index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand<validate>& operand,
                       size_t control_depth) {
    if (!VALIDATE(operand.depth < control_depth)) {
      errorf(pc + 1, "invalid break depth: %u", operand.depth);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BranchTableOperand<validate>& operand,
                size_t block_depth) {
    if (!VALIDATE(operand.table_count < kV8MaxWasmFunctionSize)) {
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
    if (!VALIDATE(operand.lane >= 0 && operand.lane < num_lanes)) {
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
    if (!VALIDATE(operand.shift >= 0 && operand.shift < max_shift)) {
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
    if (!VALIDATE(max_lane <= 2 * kSimd128Size)) {
      error(pc_ + 2, "invalid shuffle mask");
      return false;
    }
    return true;
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
        LocalIndexOperand<Decoder::kValidate> operand(decoder, pc);
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
      case kExprRefNull: {
        return 1;
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
      case kNumericPrefix:
        return 2;
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
      case kAtomicPrefix: {
        byte atomic_index = decoder->read_u8<validate>(pc + 1, "atomic_index");
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kAtomicPrefix << 8 | atomic_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessOperand<validate> operand(decoder, pc + 1, UINT32_MAX);
            return 2 + operand.length;
          }
          default:
            decoder->error(pc, "invalid Atomics opcode");
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

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprSelect:
        return {3, 1};
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
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
      case kExprRefNull:
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
      case kNumericPrefix:
      case kAtomicPrefix:
      case kSimdPrefix: {
        opcode = static_cast<WasmOpcode>(opcode << 8 | *(pc + 1));
        switch (opcode) {
          case kExprI32AtomicStore:
          case kExprI32AtomicStore8U:
          case kExprI32AtomicStore16U:
          case kExprS128StoreMem:
            return {2, 0};
          default: {
            sig = WasmOpcodes::Signature(opcode);
            if (sig) {
              return {sig->parameter_count(), sig->return_count()};
            }
          }
        }
        V8_FALLTHROUGH;
      }
      default:
        V8_Fatal(__FILE__, __LINE__, "unimplemented opcode: %x (%s)", opcode,
                 WasmOpcodes::OpcodeName(opcode));
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }
};

#define CALL_INTERFACE(name, ...) interface_.name(this, ##__VA_ARGS__)
#define CALL_INTERFACE_IF_REACHABLE(name, ...)       \
  do {                                               \
    DCHECK(!control_.empty());                       \
    if (this->ok() && control_.back().reachable()) { \
      interface_.name(this, ##__VA_ARGS__);          \
    }                                                \
  } while (false)
#define CALL_INTERFACE_IF_PARENT_REACHABLE(name, ...)                         \
  do {                                                                        \
    DCHECK(!control_.empty());                                                \
    if (this->ok() && (control_.size() == 1 || control_at(1)->reachable())) { \
      interface_.name(this, ##__VA_ARGS__);                                   \
    }                                                                         \
  } while (false)

template <Decoder::ValidateFlag validate, typename Interface>
class WasmFullDecoder : public WasmDecoder<validate> {
  using Value = typename Interface::Value;
  using Control = typename Interface::Control;
  using MergeValues = Merge<Value>;

  // All Value types should be trivially copyable for performance. We push, pop,
  // and store them in local variables.
  ASSERT_TRIVIALLY_COPYABLE(Value);

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
        args_(zone),
        last_end_found_(false) {
    this->local_types_ = &local_type_vec_;
  }

  Interface& interface() { return interface_; }

  bool Decode() {
    DCHECK(stack_.empty());
    DCHECK(control_.empty());

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
    CALL_INTERFACE(StartFunction);
    DecodeFunctionBody();
    if (!this->failed()) CALL_INTERFACE(FinishFunction);

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
    return &control_.back() - depth;
  }

  inline uint32_t stack_size() const {
    DCHECK_GE(kMaxUInt32, stack_.size());
    return static_cast<uint32_t>(stack_.size());
  }

  inline Value* stack_value(uint32_t depth) {
    DCHECK_GT(stack_.size(), depth);
    return &stack_[stack_.size() - depth - 1];
  }

  inline Value& GetMergeValueFromStack(
      Control* c, Merge<Value>* merge, uint32_t i) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    DCHECK_GT(merge->arity, i);
    DCHECK_GE(stack_.size(), c->stack_depth + merge->arity);
    return stack_[stack_.size() - merge->arity + i];
  }

 private:
  static constexpr size_t kErrorMsgSize = 128;

  Zone* zone_;

  Interface interface_;

  ZoneVector<ValueType> local_type_vec_;  // types of local variables.
  ZoneVector<Value> stack_;               // stack of values.
  ZoneVector<Control> control_;           // stack of blocks, loops, and ifs.
  ZoneVector<Value> args_;                // parameters of current block or call
  bool last_end_found_;

  bool CheckHasMemory() {
    if (!VALIDATE(this->module_->has_memory)) {
      this->error(this->pc_ - 1, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  bool CheckHasSharedMemory() {
    if (!VALIDATE(this->module_->has_shared_memory)) {
      this->error(this->pc_ - 1, "Atomic opcodes used without shared memory");
      return false;
    }
    return true;
  }

  class TraceLine {
   public:
    static constexpr int kMaxLen = 512;
    ~TraceLine() {
      if (!FLAG_trace_wasm_decoder) return;
      PrintF("%.*s\n", len_, buffer_);
    }

    // Appends a formatted string.
    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {
      if (!FLAG_trace_wasm_decoder) return;
      va_list va_args;
      va_start(va_args, format);
      size_t remaining_len = kMaxLen - len_;
      Vector<char> remaining_msg_space(buffer_ + len_, remaining_len);
      int len = VSNPrintF(remaining_msg_space, format, va_args);
      va_end(va_args);
      len_ += len < 0 ? remaining_len : len;
    }

   private:
    char buffer_[kMaxLen];
    int len_ = 0;
  };

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n",
          reinterpret_cast<const void*>(this->start()),
          reinterpret_cast<const void*>(this->end()), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      auto* c = PushBlock();
      InitMerge(&c->start_merge, 0, [](uint32_t) -> Value { UNREACHABLE(); });
      InitMerge(&c->end_merge,
                static_cast<uint32_t>(this->sig_->return_count()),
                [&] (uint32_t i) {
                    return Value::New(this->pc_, this->sig_->GetReturn(i)); });
      CALL_INTERFACE(StartFunctionBody, c);
    }

    while (this->pc_ < this->end_) {  // decoding loop.
      unsigned len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*this->pc_);

      CALL_INTERFACE_IF_REACHABLE(NextInstruction, opcode);

#if DEBUG
      TraceLine trace_msg;
#define TRACE_PART(...) trace_msg.Append(__VA_ARGS__)
      if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                   WasmOpcodes::OpcodeName(opcode));
      }
#else
#define TRACE_PART(...)
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
            if (!LookupBlockType(&operand)) break;
            PopArgs(operand.sig);
            auto* block = PushBlock();
            SetBlockType(block, operand);
            CALL_INTERFACE_IF_REACHABLE(Block, block);
            PushMergeValues(block, &block->start_merge);
            len = 1 + operand.length;
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
            ExceptionIndexOperand<Decoder::kValidate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            PopArgs(operand.exception->ToFunctionSig());
            CALL_INTERFACE_IF_REACHABLE(Throw, operand, &control_.back(),
                                        vec2vec(args_));
            EndControl();
            break;
          }
          case kExprTry: {
            CHECK_PROTOTYPE_OPCODE(eh);
            BlockTypeOperand<validate> operand(this, this->pc_);
            if (!LookupBlockType(&operand)) break;
            PopArgs(operand.sig);
            auto* try_block = PushTry();
            SetBlockType(try_block, operand);
            len = 1 + operand.length;
            CALL_INTERFACE_IF_REACHABLE(Try, try_block);
            PushMergeValues(try_block, &try_block->start_merge);
            break;
          }
          case kExprCatch: {
            // TODO(kschimpf): Fix to use type signature of exception.
            CHECK_PROTOTYPE_OPCODE(eh);
            ExceptionIndexOperand<Decoder::kValidate> operand(this, this->pc_);
            len = 1 + operand.length;

            if (!this->Validate(this->pc_, operand)) break;

            if (!VALIDATE(!control_.empty())) {
              this->error("catch does not match any try");
              break;
            }

            Control* c = &control_.back();
            if (!VALIDATE(c->is_try())) {
              this->error("catch does not match any try");
              break;
            }

            if (!VALIDATE(c->is_incomplete_try())) {
              OPCODE_ERROR(opcode, "multiple catch blocks not implemented");
              break;
            }
            c->kind = kControlTryCatch;
            FallThruTo(c);
            stack_.resize(c->stack_depth);
            const WasmExceptionSig* sig = operand.exception->sig;
            for (size_t i = 0, e = sig->parameter_count(); i < e; ++i) {
              Push(sig->GetParam(i));
            }
            Vector<Value> values(stack_.data() + c->stack_depth,
                                 sig->parameter_count());
            CALL_INTERFACE_IF_PARENT_REACHABLE(CatchException, operand, c,
                                               values);
            c->reachability = control_at(1)->innerReachability();
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
            if (!LookupBlockType(&operand)) break;
            PopArgs(operand.sig);
            auto* block = PushLoop();
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            CALL_INTERFACE_IF_REACHABLE(Loop, block);
            PushMergeValues(block, &block->start_merge);
            break;
          }
          case kExprIf: {
            BlockTypeOperand<validate> operand(this, this->pc_);
            if (!LookupBlockType(&operand)) break;
            auto cond = Pop(0, kWasmI32);
            PopArgs(operand.sig);
            if (!this->ok()) break;
            auto* if_block = PushIf();
            SetBlockType(if_block, operand);
            CALL_INTERFACE_IF_REACHABLE(If, cond, if_block);
            len = 1 + operand.length;
            PushMergeValues(if_block, &if_block->start_merge);
            break;
          }
          case kExprElse: {
            if (!VALIDATE(!control_.empty())) {
              this->error("else does not match any if");
              break;
            }
            Control* c = &control_.back();
            if (!VALIDATE(c->is_if())) {
              this->error(this->pc_, "else does not match an if");
              break;
            }
            if (c->is_if_else()) {
              this->error(this->pc_, "else already present for if");
              break;
            }
            FallThruTo(c);
            c->kind = kControlIfElse;
            CALL_INTERFACE_IF_PARENT_REACHABLE(Else, c);
            PushMergeValues(c, &c->start_merge);
            c->reachability = control_at(1)->innerReachability();
            break;
          }
          case kExprEnd: {
            if (!VALIDATE(!control_.empty())) {
              this->error("end does not match any if, try, or block");
              return;
            }
            Control* c = &control_.back();
            if (!VALIDATE(!c->is_incomplete_try())) {
              this->error(this->pc_, "missing catch in try");
              break;
            }
            if (c->is_onearmed_if()) {
              // Emulate empty else arm.
              FallThruTo(c);
              if (this->failed()) break;
              CALL_INTERFACE_IF_PARENT_REACHABLE(Else, c);
              PushMergeValues(c, &c->start_merge);
              c->reachability = control_at(1)->innerReachability();
            }

            FallThruTo(c);
            // A loop just leaves the values on the stack.
            if (!c->is_loop()) PushMergeValues(c, &c->end_merge);

            if (control_.size() == 1) {
              // If at the last (implicit) control, check we are at end.
              if (!VALIDATE(this->pc_ + 1 == this->end_)) {
                this->error(this->pc_ + 1, "trailing code after function end");
                break;
              }
              last_end_found_ = true;
              // The result of the block is the return value.
              TRACE_PART("\n" TRACE_INST_FORMAT, startrel(this->pc_),
                         "(implicit) return");
              DoReturn(c, true);
            }

            PopControl(c);
            break;
          }
          case kExprSelect: {
            auto cond = Pop(2, kWasmI32);
            auto fval = Pop();
            auto tval = Pop(0, fval.type);
            auto* result = Push(tval.type == kWasmVar ? fval.type : tval.type);
            CALL_INTERFACE_IF_REACHABLE(Select, cond, fval, tval, result);
            break;
          }
          case kExprBr: {
            BreakDepthOperand<validate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand, control_.size())) break;
            Control* c = control_at(operand.depth);
            if (!TypeCheckBreak(c)) break;
            if (control_.back().reachable()) {
              CALL_INTERFACE(Br, c);
              c->br_merge()->reached = true;
            }
            len = 1 + operand.length;
            EndControl();
            break;
          }
          case kExprBrIf: {
            BreakDepthOperand<validate> operand(this, this->pc_);
            auto cond = Pop(0, kWasmI32);
            if (this->failed()) break;
            if (!this->Validate(this->pc_, operand, control_.size())) break;
            Control* c = control_at(operand.depth);
            if (!TypeCheckBreak(c)) break;
            if (control_.back().reachable()) {
              CALL_INTERFACE(BrIf, cond, c);
              c->br_merge()->reached = true;
            }
            len = 1 + operand.length;
            break;
          }
          case kExprBrTable: {
            BranchTableOperand<validate> operand(this, this->pc_);
            BranchTableIterator<validate> iterator(this, operand);
            auto key = Pop(0, kWasmI32);
            if (this->failed()) break;
            if (!this->Validate(this->pc_, operand, control_.size())) break;
            uint32_t br_arity = 0;
            std::vector<bool> br_targets(control_.size());
            while (iterator.has_next()) {
              const uint32_t i = iterator.cur_index();
              const byte* pos = iterator.pc();
              uint32_t target = iterator.next();
              if (!VALIDATE(target < control_.size())) {
                this->errorf(pos,
                             "improper branch in br_table target %u (depth %u)",
                             i, target);
                break;
              }
              // Avoid redundant break target checks.
              if (br_targets[target]) continue;
              br_targets[target] = true;
              // Check that label types match up.
              Control* c = control_at(target);
              uint32_t arity = c->br_merge()->arity;
              if (i == 0) {
                br_arity = arity;
              } else if (!VALIDATE(br_arity == arity)) {
                this->errorf(pos,
                             "inconsistent arity in br_table target %u"
                             " (previous was %u, this one %u)",
                             i, br_arity, arity);
              }
              if (!TypeCheckBreak(c)) break;
            }
            if (this->failed()) break;

            if (control_.back().reachable()) {
              CALL_INTERFACE(BrTable, operand, key);

              for (uint32_t depth = control_depth(); depth-- > 0;) {
                if (!br_targets[depth]) continue;
                control_at(depth)->br_merge()->reached = true;
              }
            }

            len = 1 + iterator.length();
            EndControl();
            break;
          }
          case kExprReturn: {
            DoReturn(&control_.back(), false);
            break;
          }
          case kExprUnreachable: {
            CALL_INTERFACE_IF_REACHABLE(Unreachable);
            EndControl();
            break;
          }
          case kExprI32Const: {
            ImmI32Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmI32);
            CALL_INTERFACE_IF_REACHABLE(I32Const, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprI64Const: {
            ImmI64Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmI64);
            CALL_INTERFACE_IF_REACHABLE(I64Const, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprF32Const: {
            ImmF32Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmF32);
            CALL_INTERFACE_IF_REACHABLE(F32Const, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprF64Const: {
            ImmF64Operand<validate> operand(this, this->pc_);
            auto* value = Push(kWasmF64);
            CALL_INTERFACE_IF_REACHABLE(F64Const, value, operand.value);
            len = 1 + operand.length;
            break;
          }
          case kExprRefNull: {
            CHECK_PROTOTYPE_OPCODE(anyref);
            auto* value = Push(kWasmAnyRef);
            CALL_INTERFACE_IF_REACHABLE(RefNull, value);
            len = 1;
            break;
          }
          case kExprGetLocal: {
            LocalIndexOperand<Decoder::kValidate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto* value = Push(operand.type);
            CALL_INTERFACE_IF_REACHABLE(GetLocal, value, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprSetLocal: {
            LocalIndexOperand<Decoder::kValidate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto value = Pop(0, local_type_vec_[operand.index]);
            CALL_INTERFACE_IF_REACHABLE(SetLocal, value, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprTeeLocal: {
            LocalIndexOperand<Decoder::kValidate> operand(this, this->pc_);
            if (!this->Validate(this->pc_, operand)) break;
            auto value = Pop(0, local_type_vec_[operand.index]);
            auto* result = Push(value.type);
            CALL_INTERFACE_IF_REACHABLE(TeeLocal, value, result, operand);
            len = 1 + operand.length;
            break;
          }
          case kExprDrop: {
            auto value = Pop();
            CALL_INTERFACE_IF_REACHABLE(Drop, value);
            break;
          }
          case kExprGetGlobal: {
            GlobalIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            auto* result = Push(operand.type);
            CALL_INTERFACE_IF_REACHABLE(GetGlobal, result, operand);
            break;
          }
          case kExprSetGlobal: {
            GlobalIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            if (!VALIDATE(operand.global->mutability)) {
              this->errorf(this->pc_, "immutable global #%u cannot be assigned",
                           operand.index);
              break;
            }
            auto value = Pop(0, operand.type);
            CALL_INTERFACE_IF_REACHABLE(SetGlobal, value, operand);
            break;
          }
          case kExprI32LoadMem8S:
            len = 1 + DecodeLoadMem(LoadType::kI32Load8S);
            break;
          case kExprI32LoadMem8U:
            len = 1 + DecodeLoadMem(LoadType::kI32Load8U);
            break;
          case kExprI32LoadMem16S:
            len = 1 + DecodeLoadMem(LoadType::kI32Load16S);
            break;
          case kExprI32LoadMem16U:
            len = 1 + DecodeLoadMem(LoadType::kI32Load16U);
            break;
          case kExprI32LoadMem:
            len = 1 + DecodeLoadMem(LoadType::kI32Load);
            break;
          case kExprI64LoadMem8S:
            len = 1 + DecodeLoadMem(LoadType::kI64Load8S);
            break;
          case kExprI64LoadMem8U:
            len = 1 + DecodeLoadMem(LoadType::kI64Load8U);
            break;
          case kExprI64LoadMem16S:
            len = 1 + DecodeLoadMem(LoadType::kI64Load16S);
            break;
          case kExprI64LoadMem16U:
            len = 1 + DecodeLoadMem(LoadType::kI64Load16U);
            break;
          case kExprI64LoadMem32S:
            len = 1 + DecodeLoadMem(LoadType::kI64Load32S);
            break;
          case kExprI64LoadMem32U:
            len = 1 + DecodeLoadMem(LoadType::kI64Load32U);
            break;
          case kExprI64LoadMem:
            len = 1 + DecodeLoadMem(LoadType::kI64Load);
            break;
          case kExprF32LoadMem:
            len = 1 + DecodeLoadMem(LoadType::kF32Load);
            break;
          case kExprF64LoadMem:
            len = 1 + DecodeLoadMem(LoadType::kF64Load);
            break;
          case kExprI32StoreMem8:
            len = 1 + DecodeStoreMem(StoreType::kI32Store8);
            break;
          case kExprI32StoreMem16:
            len = 1 + DecodeStoreMem(StoreType::kI32Store16);
            break;
          case kExprI32StoreMem:
            len = 1 + DecodeStoreMem(StoreType::kI32Store);
            break;
          case kExprI64StoreMem8:
            len = 1 + DecodeStoreMem(StoreType::kI64Store8);
            break;
          case kExprI64StoreMem16:
            len = 1 + DecodeStoreMem(StoreType::kI64Store16);
            break;
          case kExprI64StoreMem32:
            len = 1 + DecodeStoreMem(StoreType::kI64Store32);
            break;
          case kExprI64StoreMem:
            len = 1 + DecodeStoreMem(StoreType::kI64Store);
            break;
          case kExprF32StoreMem:
            len = 1 + DecodeStoreMem(StoreType::kF32Store);
            break;
          case kExprF64StoreMem:
            len = 1 + DecodeStoreMem(StoreType::kF64Store);
            break;
          case kExprGrowMemory: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            DCHECK_NOT_NULL(this->module_);
            if (!VALIDATE(this->module_->is_wasm())) {
              this->error("grow_memory is not supported for asmjs modules");
              break;
            }
            auto value = Pop(0, kWasmI32);
            auto* result = Push(kWasmI32);
            CALL_INTERFACE_IF_REACHABLE(GrowMemory, value, result);
            break;
          }
          case kExprMemorySize: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<validate> operand(this, this->pc_);
            auto* result = Push(kWasmI32);
            len = 1 + operand.length;
            CALL_INTERFACE_IF_REACHABLE(CurrentMemoryPages, result);
            break;
          }
          case kExprCallFunction: {
            CallFunctionOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            // TODO(clemensh): Better memory management.
            PopArgs(operand.sig);
            auto* returns = PushReturns(operand.sig);
            CALL_INTERFACE_IF_REACHABLE(CallDirect, operand, args_.data(),
                                        returns);
            break;
          }
          case kExprCallIndirect: {
            CallIndirectOperand<validate> operand(this, this->pc_);
            len = 1 + operand.length;
            if (!this->Validate(this->pc_, operand)) break;
            auto index = Pop(0, kWasmI32);
            PopArgs(operand.sig);
            auto* returns = PushReturns(operand.sig);
            CALL_INTERFACE_IF_REACHABLE(CallIndirect, index, operand,
                                        args_.data(), returns);
            break;
          }
          case kNumericPrefix: {
            CHECK_PROTOTYPE_OPCODE(sat_f2i_conversions);
            ++len;
            byte numeric_index = this->template read_u8<validate>(
                this->pc_ + 1, "numeric index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | numeric_index);
            TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                       WasmOpcodes::OpcodeName(opcode));
            sig = WasmOpcodes::Signature(opcode);
            if (sig == nullptr) {
              this->errorf(this->pc_, "Unrecognized numeric opcode: %x\n",
                           opcode);
              return;
            }
            BuildSimpleOperator(opcode, sig);
            break;
          }
          case kSimdPrefix: {
            CHECK_PROTOTYPE_OPCODE(simd);
            len++;
            byte simd_index =
                this->template read_u8<validate>(this->pc_ + 1, "simd index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
            TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                       WasmOpcodes::OpcodeName(opcode));
            len += DecodeSimdOpcode(opcode);
            break;
          }
          case kAtomicPrefix: {
            CHECK_PROTOTYPE_OPCODE(threads);
            if (!CheckHasSharedMemory()) break;
            len++;
            byte atomic_index =
                this->template read_u8<validate>(this->pc_ + 1, "atomic index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | atomic_index);
            TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
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
        TRACE_PART(" ");
        for (Control& c : control_) {
          switch (c.kind) {
            case kControlIf:
              TRACE_PART("I");
              break;
            case kControlBlock:
              TRACE_PART("B");
              break;
            case kControlLoop:
              TRACE_PART("L");
              break;
            case kControlTry:
              TRACE_PART("T");
              break;
            default:
              break;
          }
          if (c.start_merge.arity) TRACE_PART("%u-", c.start_merge.arity);
          TRACE_PART("%u", c.end_merge.arity);
          if (!c.reachable()) TRACE_PART("%c", c.unreachable() ? '*' : '#');
        }
        TRACE_PART(" | ");
        for (size_t i = 0; i < stack_.size(); ++i) {
          auto& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = static_cast<WasmOpcode>(opcode << 8 | *(val.pc + 1));
          }
          TRACE_PART(" %c@%d:%s", WasmOpcodes::ShortNameOf(val.type),
                     static_cast<int>(val.pc - this->start_),
                     WasmOpcodes::OpcodeName(opcode));
          // If the decoder failed, don't try to decode the operands, as this
          // can trigger a DCHECK failure.
          if (this->failed()) continue;
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Operand<Decoder::kNoValidate> operand(this, val.pc);
              TRACE_PART("[%d]", operand.value);
              break;
            }
            case kExprGetLocal:
            case kExprSetLocal:
            case kExprTeeLocal: {
              LocalIndexOperand<Decoder::kNoValidate> operand(this, val.pc);
              TRACE_PART("[%u]", operand.index);
              break;
            }
            case kExprGetGlobal:
            case kExprSetGlobal: {
              GlobalIndexOperand<Decoder::kNoValidate> operand(this, val.pc);
              TRACE_PART("[%u]", operand.index);
              break;
            }
            default:
              break;
          }
        }
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
    CALL_INTERFACE_IF_REACHABLE(EndControl, current);
    current->reachability = kUnreachable;
  }

  bool LookupBlockType(BlockTypeOperand<validate>* operand) {
    if (operand->type == kWasmVar) {
      if (!VALIDATE(this->module_ &&
                    operand->sig_index < this->module_->signatures.size())) {
        this->errorf(
            this->pc_, "block type index %u out of bounds (%d signatures)",
            operand->sig_index,
            static_cast<int>(this->module_
                                 ? this->module_->signatures.size() : 0));
        return false;
      }
      operand->sig = this->module_->signatures[operand->sig_index];
    }
    return true;
  }

  template<typename func>
  void InitMerge(Merge<Value>* merge, uint32_t arity, func get_val) {
    merge->arity = arity;
    if (arity == 1) {
      merge->vals.first = get_val(0);
    } else if (arity > 1) {
      merge->vals.array = zone_->NewArray<Value>(arity);
      for (unsigned i = 0; i < arity; i++) {
        merge->vals.array[i] = get_val(i);
      }
    }
  }

  void SetBlockType(Control* c, BlockTypeOperand<validate>& operand) {
    DCHECK_EQ(operand.in_arity(), this->args_.size());
    const byte* pc = this->pc_;
    Value* args = this->args_.data();
    InitMerge(&c->end_merge, operand.out_arity(), [pc, &operand](uint32_t i) {
      return Value::New(pc, operand.out_type(i));
    });
    InitMerge(&c->start_merge, operand.in_arity(),
              [args](uint32_t i) { return args[i]; });
  }

  // Pops arguments as required by signature into {args_}.
  V8_INLINE void PopArgs(FunctionSig* sig) {
    int count = sig ? static_cast<int>(sig->parameter_count()) : 0;
    args_.resize(count);
    for (int i = count - 1; i >= 0; --i) {
      args_[i] = Pop(i, sig->GetParam(i));
    }
  }

  ValueType GetReturnType(FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    return sig->return_count() == 0 ? kWasmStmt : sig->GetReturn();
  }

  Control* PushControl(Control&& new_control) {
    Reachability reachability =
        control_.empty() ? kReachable : control_.back().innerReachability();
    control_.emplace_back(std::move(new_control));
    Control* c = &control_.back();
    c->reachability = reachability;
    c->start_merge.reached = c->reachable();
    return c;
  }

  Control* PushBlock() {
    return PushControl(Control::Block(this->pc_, stack_size()));
  }
  Control* PushLoop() {
    return PushControl(Control::Loop(this->pc_, stack_size()));
  }
  Control* PushIf() {
    return PushControl(Control::If(this->pc_, stack_size()));
  }
  Control* PushTry() {
    // current_catch_ = static_cast<int32_t>(control_.size() - 1);
    return PushControl(Control::Try(this->pc_, stack_size()));
  }

  void PopControl(Control* c) {
    DCHECK_EQ(c, &control_.back());
    CALL_INTERFACE_IF_PARENT_REACHABLE(PopControl, c);
    bool reached = c->end_merge.reached;
    control_.pop_back();
    // If the parent block was reachable before, but the popped control does not
    // return to here, this block becomes indirectly unreachable.
    if (!control_.empty() && !reached && control_.back().reachable()) {
      control_.back().reachability = kSpecOnlyReachable;
    }
  }

  int DecodeLoadMem(LoadType type, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(this, this->pc_ + prefix_len,
                                          type.size_log_2());
    auto index = Pop(0, kWasmI32);
    auto* result = Push(type.value_type());
    CALL_INTERFACE_IF_REACHABLE(LoadMem, type, operand, index, result);
    return operand.length;
  }

  int DecodeStoreMem(StoreType store, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<validate> operand(this, this->pc_ + prefix_len,
                                          store.size_log_2());
    auto value = Pop(1, store.value_type());
    auto index = Pop(0, kWasmI32);
    CALL_INTERFACE_IF_REACHABLE(StoreMem, store, operand, index, value);
    return operand.length;
  }

  unsigned SimdExtractLane(WasmOpcode opcode, ValueType type) {
    SimdLaneOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, opcode, operand)) {
      Value inputs[] = {Pop(0, ValueType::kSimd128)};
      auto* result = Push(type);
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, operand,
                                  ArrayVector(inputs), result);
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
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, operand,
                                  ArrayVector(inputs), result);
    }
    return operand.length;
  }

  unsigned SimdShiftOp(WasmOpcode opcode) {
    SimdShiftOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, opcode, operand)) {
      auto input = Pop(0, ValueType::kSimd128);
      auto* result = Push(ValueType::kSimd128);
      CALL_INTERFACE_IF_REACHABLE(SimdShiftOp, opcode, operand, input, result);
    }
    return operand.length;
  }

  unsigned Simd8x16ShuffleOp() {
    Simd8x16ShuffleOperand<validate> operand(this, this->pc_);
    if (this->Validate(this->pc_, operand)) {
      auto input1 = Pop(1, ValueType::kSimd128);
      auto input0 = Pop(0, ValueType::kSimd128);
      auto* result = Push(ValueType::kSimd128);
      CALL_INTERFACE_IF_REACHABLE(Simd8x16ShuffleOp, operand, input0, input1,
                                  result);
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
        len = DecodeLoadMem(LoadType::kS128Load, 1);
        break;
      case kExprS128StoreMem:
        len = DecodeStoreMem(StoreType::kS128Store, 1);
        break;
      default: {
        FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (!VALIDATE(sig != nullptr)) {
          this->error("invalid simd opcode");
          break;
        }
        PopArgs(sig);
        auto* results =
            sig->return_count() == 0 ? nullptr : Push(GetReturnType(sig));
        CALL_INTERFACE_IF_REACHABLE(SimdOp, opcode, vec2vec(args_), results);
      }
    }
    return len;
  }

  unsigned DecodeAtomicOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    ValueType ret_type;
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (sig != nullptr) {
      MachineType memtype;
      switch (opcode) {
#define CASE_ATOMIC_STORE_OP(Name, Type)     \
  case kExpr##Name: {                        \
    memtype = MachineType::Type();           \
    ret_type = MachineRepresentation::kNone; \
    break;                                   \
  }
        ATOMIC_STORE_OP_LIST(CASE_ATOMIC_STORE_OP)
#undef CASE_ATOMIC_OP
#define CASE_ATOMIC_OP(Name, Type) \
  case kExpr##Name: {              \
    memtype = MachineType::Type(); \
    ret_type = GetReturnType(sig); \
    break;                         \
  }
        ATOMIC_OP_LIST(CASE_ATOMIC_OP)
#undef CASE_ATOMIC_OP
        default:
          this->error("invalid atomic opcode");
          return 0;
      }
      MemoryAccessOperand<validate> operand(
          this, this->pc_ + 1, ElementSizeLog2Of(memtype.representation()));
      len += operand.length;
      PopArgs(sig);
      auto result = ret_type == MachineRepresentation::kNone
                        ? nullptr
                        : Push(GetReturnType(sig));
      CALL_INTERFACE_IF_REACHABLE(AtomicOp, opcode, vec2vec(args_), operand,
                                  result);
    } else {
      this->error("invalid atomic opcode");
    }
    return len;
  }

  void DoReturn(Control* c, bool implicit) {
    int return_count = static_cast<int>(this->sig_->return_count());
    args_.resize(return_count);

    // Pop return values off the stack in reverse order.
    for (int i = return_count - 1; i >= 0; --i) {
      args_[i] = Pop(i, this->sig_->GetReturn(i));
    }

    // Simulate that an implicit return morally comes after the current block.
    if (implicit && c->end_merge.reached) c->reachability = kReachable;
    CALL_INTERFACE_IF_REACHABLE(DoReturn, vec2vec(args_), implicit);

    EndControl();
  }

  inline Value* Push(ValueType type) {
    DCHECK_NE(kWasmStmt, type);
    stack_.push_back(Value::New(this->pc_, type));
    return &stack_.back();
  }

  void PushMergeValues(Control* c, Merge<Value>* merge) {
    DCHECK_EQ(c, &control_.back());
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    stack_.resize(c->stack_depth);
    if (merge->arity == 1) {
      stack_.push_back(merge->vals.first);
    } else {
      for (unsigned i = 0; i < merge->arity; i++) {
        stack_.push_back(merge->vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + merge->arity, stack_.size());
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
    if (!VALIDATE(val.type == expected || val.type == kWasmVar ||
                  expected == kWasmVar)) {
      this->errorf(val.pc, "%s[%d] expected type %s, found %s of type %s",
                   SafeOpcodeNameAt(this->pc_), index,
                   WasmOpcodes::TypeName(expected), SafeOpcodeNameAt(val.pc),
                   WasmOpcodes::TypeName(val.type));
    }
    return val;
  }

  Value Pop() {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (stack_.size() <= limit) {
      // Popping past the current control start in reachable code.
      if (!VALIDATE(control_.back().unreachable())) {
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

  void FallThruTo(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!TypeCheckFallThru(c)) return;
    if (!c->reachable()) return;

    if (!c->is_loop()) CALL_INTERFACE(FallThruTo, c);
    c->end_merge.reached = true;
  }

  bool TypeCheckMergeValues(Control* c, Merge<Value>* merge) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    DCHECK_GE(stack_.size(), c->stack_depth + merge->arity);
    // Typecheck the topmost {merge->arity} values on the stack.
    for (uint32_t i = 0; i < merge->arity; ++i) {
      auto& val = GetMergeValueFromStack(c, merge, i);
      auto& old = (*merge)[i];
      if (val.type != old.type) {
        // If {val.type} is polymorphic, which results from unreachable, make
        // it more specific by using the merge value's expected type.
        // If it is not polymorphic, this is a type error.
        if (!VALIDATE(val.type == kWasmVar)) {
          this->errorf(
              this->pc_, "type error in merge[%u] (expected %s, got %s)", i,
              WasmOpcodes::TypeName(old.type), WasmOpcodes::TypeName(val.type));
          return false;
        }
        val.type = old.type;
      }
    }

    return true;
  }

  bool TypeCheckFallThru(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!validate) return true;
    uint32_t expected = c->end_merge.arity;
    DCHECK_GE(stack_.size(), c->stack_depth);
    uint32_t actual = static_cast<uint32_t>(stack_.size()) - c->stack_depth;
    // Fallthrus must match the arity of the control exactly.
    if (!InsertUnreachablesIfNecessary(expected, actual) || actual > expected) {
      this->errorf(
          this->pc_,
          "expected %u elements on the stack for fallthru to @%d, found %u",
          expected, startrel(c->pc), actual);
      return false;
    }

    return TypeCheckMergeValues(c, &c->end_merge);
  }

  bool TypeCheckBreak(Control* c) {
    // Breaks must have at least the number of values expected; can have more.
    uint32_t expected = c->br_merge()->arity;
    DCHECK_GE(stack_.size(), control_.back().stack_depth);
    uint32_t actual =
        static_cast<uint32_t>(stack_.size()) - control_.back().stack_depth;
    if (!InsertUnreachablesIfNecessary(expected, actual)) {
      this->errorf(this->pc_,
                   "expected %u elements on the stack for br to @%d, found %u",
                   expected, startrel(c->pc), actual);
      return false;
    }
    return TypeCheckMergeValues(c, c->br_merge());
  }

  inline bool InsertUnreachablesIfNecessary(uint32_t expected,
                                            uint32_t actual) {
    if (V8_LIKELY(actual >= expected)) {
      return true;  // enough actual values are there.
    }
    if (!VALIDATE(control_.back().unreachable())) {
      // There aren't enough values on the stack.
      return false;
    }
    // A slow path. When the actual number of values on the stack is less
    // than the expected number of values and the current control is
    // unreachable, insert unreachable values below the actual values.
    // This simplifies {TypeCheckMergeValues}.
    auto pos = stack_.begin() + (stack_.size() - actual);
    stack_.insert(pos, (expected - actual), Value::Unreachable(this->pc_));
    return true;
  }

  virtual void onFirstError() {
    this->end_ = this->pc_;  // Terminate decoding loop.
    TRACE(" !%s\n", this->error_msg_.c_str());
    CALL_INTERFACE(OnFirstError);
  }

  inline void BuildSimpleOperator(WasmOpcode opcode, FunctionSig* sig) {
    if (WasmOpcodes::IsSignExtensionOpcode(opcode)) {
      RET_ON_PROTOTYPE_OPCODE(se);
    }
    if (WasmOpcodes::IsAnyRefOpcode(opcode)) {
      RET_ON_PROTOTYPE_OPCODE(anyref);
    }

    switch (sig->parameter_count()) {
      case 1: {
        auto val = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, sig, val, ret);
        break;
      }
      case 2: {
        auto rval = Pop(1, sig->GetParam(1));
        auto lval = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(BinOp, opcode, sig, lval, rval, ret);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
};

#undef CALL_INTERFACE
#undef CALL_INTERFACE_IF_REACHABLE
#undef CALL_INTERFACE_IF_PARENT_REACHABLE

class EmptyInterface {
 public:
  static constexpr wasm::Decoder::ValidateFlag validate =
      wasm::Decoder::kValidate;
  using Value = ValueBase;
  using Control = ControlBase<Value>;
  using Decoder = WasmFullDecoder<validate, EmptyInterface>;

#define DEFINE_EMPTY_CALLBACK(name, ...) \
  void name(Decoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_FUNCTIONS(DEFINE_EMPTY_CALLBACK)
#undef DEFINE_EMPTY_CALLBACK
};

#undef TRACE
#undef TRACE_INST_FORMAT
#undef VALIDATE
#undef CHECK_PROTOTYPE_OPCODE
#undef OPCODE_ERROR

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
