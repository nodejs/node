// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "include/v8.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/utils/ostreams.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

namespace {

constexpr int kMaxFunctions = 4;
constexpr int kMaxGlobals = 64;

class DataRange {
  Vector<const uint8_t> data_;

 public:
  explicit DataRange(Vector<const uint8_t> data) : data_(data) {}

  // Don't accidentally pass DataRange by value. This will reuse bytes and might
  // lead to OOM because the end might not be reached.
  // Define move constructor and move assignment, disallow copy constructor and
  // copy assignment (below).
  DataRange(DataRange&& other) V8_NOEXCEPT : DataRange(other.data_) {
    other.data_ = {};
  }
  DataRange& operator=(DataRange&& other) V8_NOEXCEPT {
    data_ = other.data_;
    other.data_ = {};
    return *this;
  }

  size_t size() const { return data_.size(); }

  DataRange split() {
    uint16_t num_bytes = get<uint16_t>() % std::max(size_t{1}, data_.size());
    DataRange split(data_.SubVector(0, num_bytes));
    data_ += num_bytes;
    return split;
  }

  template <typename T, size_t max_bytes = sizeof(T)>
  T get() {
    STATIC_ASSERT(max_bytes <= sizeof(T));
    // We want to support the case where we have less than sizeof(T) bytes
    // remaining in the slice. For example, if we emit an i32 constant, it's
    // okay if we don't have a full four bytes available, we'll just use what
    // we have. We aren't concerned about endianness because we are generating
    // arbitrary expressions.
    const size_t num_bytes = std::min(max_bytes, data_.size());
    T result = T();
    memcpy(&result, data_.begin(), num_bytes);
    data_ += num_bytes;
    return result;
  }

  DISALLOW_COPY_AND_ASSIGN(DataRange);
};

ValueType GetValueType(DataRange* data) {
  // TODO(v8:8460): We do not add kWasmS128 here yet because this method is used
  // to generate globals, and since we do not have v128.const yet, there is no
  // way to specify an initial value a global of this type.
  switch (data->get<uint8_t>() % 4) {
    case 0:
      return kWasmI32;
    case 1:
      return kWasmI64;
    case 2:
      return kWasmF32;
    case 3:
      return kWasmF64;
  }
  UNREACHABLE();
}

class WasmGenerator {
  template <WasmOpcode Op, ValueType::Kind... Args>
  void op(DataRange* data) {
    Generate<Args...>(data);
    builder_->Emit(Op);
  }

  class BlockScope {
   public:
    BlockScope(WasmGenerator* gen, WasmOpcode block_type, ValueType result_type,
               ValueType br_type)
        : gen_(gen) {
      gen->blocks_.push_back(br_type);
      gen->builder_->EmitWithU8(block_type, result_type.value_type_code());
    }

    ~BlockScope() {
      gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
  };

  template <ValueType::Kind T>
  void block(DataRange* data) {
    BlockScope block_scope(this, kExprBlock, ValueType(T), ValueType(T));
    Generate<T>(data);
  }

  template <ValueType::Kind T>
  void loop(DataRange* data) {
    // When breaking to a loop header, don't provide any input value (hence
    // kWasmStmt).
    BlockScope block_scope(this, kExprLoop, ValueType(T), kWasmStmt);
    Generate<T>(data);
  }

  enum IfType { kIf, kIfElse };

  template <ValueType::Kind T, IfType type>
  void if_(DataRange* data) {
    static_assert(T == ValueType::kStmt || type == kIfElse,
                  "if without else cannot produce a value");
    Generate<ValueType::kI32>(data);
    BlockScope block_scope(this, kExprIf, ValueType(T), ValueType(T));
    Generate<T>(data);
    if (type == kIfElse) {
      builder_->Emit(kExprElse);
      Generate<T>(data);
    }
  }

  void br(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const ValueType break_type = blocks_[target_block];

    Generate(break_type, data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
  }

  template <ValueType::Kind wanted_type>
  void br_if(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const ValueType break_type = blocks_[target_block];

    Generate(break_type, data);
    Generate(kWasmI32, data);
    builder_->EmitWithI32V(
        kExprBrIf, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConvertOrGenerate(break_type, ValueType(wanted_type), data);
  }

  // TODO(eholk): make this function constexpr once gcc supports it
  static uint8_t max_alignment(WasmOpcode memop) {
    switch (memop) {
      case kExprS128LoadMem:
      case kExprS128StoreMem:
        return 4;
      case kExprI64LoadMem:
      case kExprF64LoadMem:
      case kExprI64StoreMem:
      case kExprF64StoreMem:
      case kExprI64AtomicStore:
      case kExprI64AtomicLoad:
      case kExprI64AtomicAdd:
      case kExprI64AtomicSub:
      case kExprI64AtomicAnd:
      case kExprI64AtomicOr:
      case kExprI64AtomicXor:
      case kExprI64AtomicExchange:
      case kExprI64AtomicCompareExchange:
        return 3;
      case kExprI32LoadMem:
      case kExprI64LoadMem32S:
      case kExprI64LoadMem32U:
      case kExprF32LoadMem:
      case kExprI32StoreMem:
      case kExprI64StoreMem32:
      case kExprF32StoreMem:
      case kExprI32AtomicStore:
      case kExprI64AtomicStore32U:
      case kExprI32AtomicLoad:
      case kExprI64AtomicLoad32U:
      case kExprI32AtomicAdd:
      case kExprI32AtomicSub:
      case kExprI32AtomicAnd:
      case kExprI32AtomicOr:
      case kExprI32AtomicXor:
      case kExprI32AtomicExchange:
      case kExprI32AtomicCompareExchange:
      case kExprI64AtomicAdd32U:
      case kExprI64AtomicSub32U:
      case kExprI64AtomicAnd32U:
      case kExprI64AtomicOr32U:
      case kExprI64AtomicXor32U:
      case kExprI64AtomicExchange32U:
      case kExprI64AtomicCompareExchange32U:
        return 2;
      case kExprI32LoadMem16S:
      case kExprI32LoadMem16U:
      case kExprI64LoadMem16S:
      case kExprI64LoadMem16U:
      case kExprI32StoreMem16:
      case kExprI64StoreMem16:
      case kExprI32AtomicStore16U:
      case kExprI64AtomicStore16U:
      case kExprI32AtomicLoad16U:
      case kExprI64AtomicLoad16U:
      case kExprI32AtomicAdd16U:
      case kExprI32AtomicSub16U:
      case kExprI32AtomicAnd16U:
      case kExprI32AtomicOr16U:
      case kExprI32AtomicXor16U:
      case kExprI32AtomicExchange16U:
      case kExprI32AtomicCompareExchange16U:
      case kExprI64AtomicAdd16U:
      case kExprI64AtomicSub16U:
      case kExprI64AtomicAnd16U:
      case kExprI64AtomicOr16U:
      case kExprI64AtomicXor16U:
      case kExprI64AtomicExchange16U:
      case kExprI64AtomicCompareExchange16U:
        return 1;
      case kExprI32LoadMem8S:
      case kExprI32LoadMem8U:
      case kExprI64LoadMem8S:
      case kExprI64LoadMem8U:
      case kExprI32StoreMem8:
      case kExprI64StoreMem8:
      case kExprI32AtomicStore8U:
      case kExprI64AtomicStore8U:
      case kExprI32AtomicLoad8U:
      case kExprI64AtomicLoad8U:
      case kExprI32AtomicAdd8U:
      case kExprI32AtomicSub8U:
      case kExprI32AtomicAnd8U:
      case kExprI32AtomicOr8U:
      case kExprI32AtomicXor8U:
      case kExprI32AtomicExchange8U:
      case kExprI32AtomicCompareExchange8U:
      case kExprI64AtomicAdd8U:
      case kExprI64AtomicSub8U:
      case kExprI64AtomicAnd8U:
      case kExprI64AtomicOr8U:
      case kExprI64AtomicXor8U:
      case kExprI64AtomicExchange8U:
      case kExprI64AtomicCompareExchange8U:
        return 0;
      default:
        return 0;
    }
  }

  template <WasmOpcode memory_op, ValueType::Kind... arg_types>
  void memop(DataRange* data) {
    const uint8_t align = data->get<uint8_t>() % (max_alignment(memory_op) + 1);
    const uint32_t offset = data->get<uint32_t>();

    // Generate the index and the arguments, if any.
    Generate<ValueType::kI32, arg_types...>(data);

    if (WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(memory_op >> 8))) {
      DCHECK(memory_op >> 8 == kAtomicPrefix || memory_op >> 8 == kSimdPrefix);
      builder_->EmitWithPrefix(memory_op);
    } else {
      builder_->Emit(memory_op);
    }
    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueType::Kind... Args>
  void atomic_op(DataRange* data) {
    const uint8_t align = data->get<uint8_t>() % (max_alignment(Op) + 1);
    const uint32_t offset = data->get<uint32_t>();

    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);

    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueType::Kind... Args>
  void simd_op(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
  }

  void drop(DataRange* data) {
    Generate(GetValueType(data), data);
    builder_->Emit(kExprDrop);
  }

  template <ValueType::Kind wanted_type>
  void call(DataRange* data) {
    call(data, ValueType(wanted_type));
  }

  void Convert(ValueType src, ValueType dst) {
    auto idx = [](ValueType t) -> int {
      switch (t.kind()) {
        case ValueType::kI32:
          return 0;
        case ValueType::kI64:
          return 1;
        case ValueType::kF32:
          return 2;
        case ValueType::kF64:
          return 3;
        default:
          UNREACHABLE();
      }
    };
    static constexpr WasmOpcode kConvertOpcodes[] = {
        // {i32, i64, f32, f64} -> i32
        kExprNop, kExprI32ConvertI64, kExprI32SConvertF32, kExprI32SConvertF64,
        // {i32, i64, f32, f64} -> i64
        kExprI64SConvertI32, kExprNop, kExprI64SConvertF32, kExprI64SConvertF64,
        // {i32, i64, f32, f64} -> f32
        kExprF32SConvertI32, kExprF32SConvertI64, kExprNop, kExprF32ConvertF64,
        // {i32, i64, f32, f64} -> f64
        kExprF64SConvertI32, kExprF64SConvertI64, kExprF64ConvertF32, kExprNop};
    int arr_idx = idx(dst) << 2 | idx(src);
    builder_->Emit(kConvertOpcodes[arr_idx]);
  }

  void ConvertOrGenerate(ValueType src, ValueType dst, DataRange* data) {
    if (src == dst) return;
    if (src == kWasmStmt && dst != kWasmStmt) {
      Generate(dst, data);
    } else if (dst == kWasmStmt && src != kWasmStmt) {
      builder_->Emit(kExprDrop);
    } else {
      Convert(src, dst);
    }
  }

  void call(DataRange* data, ValueType wanted_type) {
    int func_index = data->get<uint8_t>() % functions_.size();
    FunctionSig* sig = functions_[func_index];
    // Generate arguments.
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      Generate(sig->GetParam(i), data);
    }
    // Emit call.
    builder_->EmitWithU32V(kExprCallFunction, func_index);
    // Convert the return value to the wanted type.
    ValueType return_type =
        sig->return_count() == 0 ? kWasmStmt : sig->GetReturn(0);
    if (return_type == kWasmStmt && wanted_type != kWasmStmt) {
      // The call did not generate a value. Thus just generate it here.
      Generate(wanted_type, data);
    } else if (return_type != kWasmStmt && wanted_type == kWasmStmt) {
      // The call did generate a value, but we did not want one.
      builder_->Emit(kExprDrop);
    } else if (return_type != wanted_type) {
      // If the returned type does not match the wanted type, convert it.
      Convert(return_type, wanted_type);
    }
  }

  struct Var {
    uint32_t index;
    ValueType type = kWasmStmt;
    Var() = default;
    Var(uint32_t index, ValueType type) : index(index), type(type) {}
    bool is_valid() const { return type != kWasmStmt; }
  };

  Var GetRandomLocal(DataRange* data) {
    uint32_t num_params =
        static_cast<uint32_t>(builder_->signature()->parameter_count());
    uint32_t num_locals = static_cast<uint32_t>(locals_.size());
    if (num_params + num_locals == 0) return {};
    uint32_t index = data->get<uint8_t>() % (num_params + num_locals);
    ValueType type = index < num_params ? builder_->signature()->GetParam(index)
                                        : locals_[index - num_params];
    return {index, type};
  }

  template <ValueType::Kind wanted_type>
  void local_op(DataRange* data, WasmOpcode opcode) {
    Var local = GetRandomLocal(data);
    // If there are no locals and no parameters, just generate any value (if a
    // value is needed), or do nothing.
    if (!local.is_valid()) {
      if (wanted_type == ValueType::kStmt) return;
      return Generate<wanted_type>(data);
    }

    if (opcode != kExprLocalGet) Generate(local.type, data);
    builder_->EmitWithU32V(opcode, local.index);
    if (wanted_type != ValueType::kStmt && local.type.kind() != wanted_type) {
      Convert(local.type, ValueType(wanted_type));
    }
  }

  template <ValueType::Kind wanted_type>
  void get_local(DataRange* data) {
    static_assert(wanted_type != ValueType::kStmt, "illegal type");
    local_op<wanted_type>(data, kExprLocalGet);
  }

  void set_local(DataRange* data) {
    local_op<ValueType::kStmt>(data, kExprLocalSet);
  }

  template <ValueType::Kind wanted_type>
  void tee_local(DataRange* data) {
    local_op<wanted_type>(data, kExprLocalTee);
  }

  template <size_t num_bytes>
  void i32_const(DataRange* data) {
    builder_->EmitI32Const(data->get<int32_t, num_bytes>());
  }

  template <size_t num_bytes>
  void i64_const(DataRange* data) {
    builder_->EmitI64Const(data->get<int64_t, num_bytes>());
  }

  Var GetRandomGlobal(DataRange* data, bool ensure_mutable) {
    uint32_t index;
    if (ensure_mutable) {
      if (mutable_globals_.empty()) return {};
      index = mutable_globals_[data->get<uint8_t>() % mutable_globals_.size()];
    } else {
      if (globals_.empty()) return {};
      index = data->get<uint8_t>() % globals_.size();
    }
    ValueType type = globals_[index];
    return {index, type};
  }

  template <ValueType::Kind wanted_type>
  void global_op(DataRange* data) {
    constexpr bool is_set = wanted_type == ValueType::kStmt;
    Var global = GetRandomGlobal(data, is_set);
    // If there are no globals, just generate any value (if a value is needed),
    // or do nothing.
    if (!global.is_valid()) {
      if (wanted_type == ValueType::kStmt) return;
      return Generate<wanted_type>(data);
    }

    if (is_set) Generate(global.type, data);
    builder_->EmitWithU32V(is_set ? kExprGlobalSet : kExprGlobalGet,
                           global.index);
    if (!is_set && global.type.kind() != wanted_type) {
      Convert(global.type, ValueType(wanted_type));
    }
  }

  template <ValueType::Kind wanted_type>
  void get_global(DataRange* data) {
    static_assert(wanted_type != ValueType::kStmt, "illegal type");
    global_op<wanted_type>(data);
  }

  template <ValueType::Kind select_type>
  void select_with_type(DataRange* data) {
    static_assert(select_type != ValueType::kStmt, "illegal type for select");
    Generate<select_type, select_type, ValueType::kI32>(data);
    // num_types is always 1.
    uint8_t num_types = 1;
    builder_->EmitWithU8U8(kExprSelectWithType, num_types,
                           ValueType(select_type).value_type_code());
  }

  void set_global(DataRange* data) { global_op<ValueType::kStmt>(data); }

  template <ValueType::Kind... Types>
  void sequence(DataRange* data) {
    Generate<Types...>(data);
  }

  void current_memory(DataRange* data) {
    builder_->EmitWithU8(kExprMemorySize, 0);
  }

  void grow_memory(DataRange* data);

  using GenerateFn = void (WasmGenerator::*const)(DataRange*);

  template <size_t N>
  void GenerateOneOf(GenerateFn (&alternatives)[N], DataRange* data) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternatives. Use a bigger type if needed.");
    const auto which = data->get<uint8_t>();

    GenerateFn alternate = alternatives[which % N];
    (this->*alternate)(data);
  }

  struct GeneratorRecursionScope {
    explicit GeneratorRecursionScope(WasmGenerator* gen) : gen(gen) {
      ++gen->recursion_depth;
      DCHECK_LE(gen->recursion_depth, kMaxRecursionDepth);
    }
    ~GeneratorRecursionScope() {
      DCHECK_GT(gen->recursion_depth, 0);
      --gen->recursion_depth;
    }
    WasmGenerator* gen;
  };

 public:
  WasmGenerator(WasmFunctionBuilder* fn,
                const std::vector<FunctionSig*>& functions,
                const std::vector<ValueType>& globals,
                const std::vector<uint8_t>& mutable_globals, DataRange* data)
      : builder_(fn),
        functions_(functions),
        globals_(globals),
        mutable_globals_(mutable_globals) {
    FunctionSig* sig = fn->signature();
    DCHECK_GE(1, sig->return_count());
    blocks_.push_back(sig->return_count() == 0 ? kWasmStmt : sig->GetReturn(0));

    constexpr uint32_t kMaxLocals = 32;
    locals_.resize(data->get<uint8_t>() % kMaxLocals);
    for (ValueType& local : locals_) {
      local = GetValueType(data);
      fn->AddLocal(local);
    }
  }

  void Generate(ValueType type, DataRange* data);

  template <ValueType::Kind T>
  void Generate(DataRange* data);

  template <ValueType::Kind T1, ValueType::Kind T2, ValueType::Kind... Ts>
  void Generate(DataRange* data) {
    // TODO(clemensb): Implement a more even split.
    auto first_data = data->split();
    Generate<T1>(&first_data);
    Generate<T2, Ts...>(data);
  }

 private:
  WasmFunctionBuilder* builder_;
  std::vector<ValueType> blocks_;
  const std::vector<FunctionSig*>& functions_;
  std::vector<ValueType> locals_;
  std::vector<ValueType> globals_;
  std::vector<uint8_t> mutable_globals_;  // indexes into {globals_}.
  uint32_t recursion_depth = 0;

  static constexpr uint32_t kMaxRecursionDepth = 64;

  bool recursion_limit_reached() {
    return recursion_depth >= kMaxRecursionDepth;
  }
};

template <>
void WasmGenerator::Generate<ValueType::kStmt>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() == 0) return;

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kStmt,
                               ValueType::kStmt, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kStmt,
                               ValueType::kStmt, ValueType::kStmt,
                               ValueType::kStmt, ValueType::kStmt,
                               ValueType::kStmt, ValueType::kStmt>,
      &WasmGenerator::block<ValueType::kStmt>,
      &WasmGenerator::loop<ValueType::kStmt>,
      &WasmGenerator::if_<ValueType::kStmt, kIf>,
      &WasmGenerator::if_<ValueType::kStmt, kIfElse>,
      &WasmGenerator::br,
      &WasmGenerator::br_if<ValueType::kStmt>,

      &WasmGenerator::memop<kExprI32StoreMem, ValueType::kI32>,
      &WasmGenerator::memop<kExprI32StoreMem8, ValueType::kI32>,
      &WasmGenerator::memop<kExprI32StoreMem16, ValueType::kI32>,
      &WasmGenerator::memop<kExprI64StoreMem, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64StoreMem8, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64StoreMem16, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64StoreMem32, ValueType::kI64>,
      &WasmGenerator::memop<kExprF32StoreMem, ValueType::kF32>,
      &WasmGenerator::memop<kExprF64StoreMem, ValueType::kF64>,
      &WasmGenerator::memop<kExprI32AtomicStore, ValueType::kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore8U, ValueType::kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore16U, ValueType::kI32>,
      &WasmGenerator::memop<kExprI64AtomicStore, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore8U, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore16U, ValueType::kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore32U, ValueType::kI64>,
      &WasmGenerator::memop<kExprS128StoreMem, ValueType::kS128>,

      &WasmGenerator::drop,

      &WasmGenerator::call<ValueType::kStmt>,

      &WasmGenerator::set_local,
      &WasmGenerator::set_global};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<ValueType::kI32>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= 1) {
    builder_->EmitI32Const(data->get<uint32_t>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::i32_const<1>,
      &WasmGenerator::i32_const<2>,
      &WasmGenerator::i32_const<3>,
      &WasmGenerator::i32_const<4>,

      &WasmGenerator::sequence<ValueType::kI32, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kI32>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kI32,
                               ValueType::kStmt>,

      &WasmGenerator::op<kExprI32Eqz, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Eq, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Ne, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32LtS, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32LtU, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32GeS, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32GeU, ValueType::kI32, ValueType::kI32>,

      &WasmGenerator::op<kExprI64Eqz, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Eq, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Ne, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64LtS, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64LtU, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64GeS, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64GeU, ValueType::kI64, ValueType::kI64>,

      &WasmGenerator::op<kExprF32Eq, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Ne, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Lt, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Ge, ValueType::kF32, ValueType::kF32>,

      &WasmGenerator::op<kExprF64Eq, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Ne, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Lt, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Ge, ValueType::kF64, ValueType::kF64>,

      &WasmGenerator::op<kExprI32Add, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Sub, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Mul, ValueType::kI32, ValueType::kI32>,

      &WasmGenerator::op<kExprI32DivS, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32DivU, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32RemS, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32RemU, ValueType::kI32, ValueType::kI32>,

      &WasmGenerator::op<kExprI32And, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Ior, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Xor, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Shl, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32ShrU, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32ShrS, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Ror, ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Rol, ValueType::kI32, ValueType::kI32>,

      &WasmGenerator::op<kExprI32Clz, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Ctz, ValueType::kI32>,
      &WasmGenerator::op<kExprI32Popcnt, ValueType::kI32>,

      &WasmGenerator::op<kExprI32ConvertI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI32SConvertF32, ValueType::kF32>,
      &WasmGenerator::op<kExprI32UConvertF32, ValueType::kF32>,
      &WasmGenerator::op<kExprI32SConvertF64, ValueType::kF64>,
      &WasmGenerator::op<kExprI32UConvertF64, ValueType::kF64>,
      &WasmGenerator::op<kExprI32ReinterpretF32, ValueType::kF32>,

      &WasmGenerator::block<ValueType::kI32>,
      &WasmGenerator::loop<ValueType::kI32>,
      &WasmGenerator::if_<ValueType::kI32, kIfElse>,
      &WasmGenerator::br_if<ValueType::kI32>,

      &WasmGenerator::memop<kExprI32LoadMem>,
      &WasmGenerator::memop<kExprI32LoadMem8S>,
      &WasmGenerator::memop<kExprI32LoadMem8U>,
      &WasmGenerator::memop<kExprI32LoadMem16S>,
      &WasmGenerator::memop<kExprI32LoadMem16U>,
      &WasmGenerator::memop<kExprI32AtomicLoad>,
      &WasmGenerator::memop<kExprI32AtomicLoad8U>,
      &WasmGenerator::memop<kExprI32AtomicLoad16U>,

      &WasmGenerator::atomic_op<kExprI32AtomicAdd, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange, ValueType::kI32,
                                ValueType::kI32, ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange8U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange8U,
                                ValueType::kI32, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange16U, ValueType::kI32,
                                ValueType::kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange16U,
                                ValueType::kI32, ValueType::kI32,
                                ValueType::kI32>,

      &WasmGenerator::simd_op<kExprS1x16AnyTrue, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprS1x8AnyTrue, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprS1x4AnyTrue, ValueType::kS128>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory,

      &WasmGenerator::get_local<ValueType::kI32>,
      &WasmGenerator::tee_local<ValueType::kI32>,
      &WasmGenerator::get_global<ValueType::kI32>,
      &WasmGenerator::op<kExprSelect, ValueType::kI32, ValueType::kI32,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kI32>,

      &WasmGenerator::call<ValueType::kI32>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<ValueType::kI64>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= 1) {
    builder_->EmitI64Const(data->get<int64_t>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::i64_const<1>,
      &WasmGenerator::i64_const<2>,
      &WasmGenerator::i64_const<3>,
      &WasmGenerator::i64_const<4>,
      &WasmGenerator::i64_const<5>,
      &WasmGenerator::i64_const<6>,
      &WasmGenerator::i64_const<7>,
      &WasmGenerator::i64_const<8>,

      &WasmGenerator::sequence<ValueType::kI64, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kI64>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kI64,
                               ValueType::kStmt>,

      &WasmGenerator::op<kExprI64Add, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Sub, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Mul, ValueType::kI64, ValueType::kI64>,

      &WasmGenerator::op<kExprI64DivS, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64DivU, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64RemS, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64RemU, ValueType::kI64, ValueType::kI64>,

      &WasmGenerator::op<kExprI64And, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Ior, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Xor, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Shl, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64ShrU, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64ShrS, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Ror, ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Rol, ValueType::kI64, ValueType::kI64>,

      &WasmGenerator::op<kExprI64Clz, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Ctz, ValueType::kI64>,
      &WasmGenerator::op<kExprI64Popcnt, ValueType::kI64>,

      &WasmGenerator::block<ValueType::kI64>,
      &WasmGenerator::loop<ValueType::kI64>,
      &WasmGenerator::if_<ValueType::kI64, kIfElse>,
      &WasmGenerator::br_if<ValueType::kI64>,

      &WasmGenerator::memop<kExprI64LoadMem>,
      &WasmGenerator::memop<kExprI64LoadMem8S>,
      &WasmGenerator::memop<kExprI64LoadMem8U>,
      &WasmGenerator::memop<kExprI64LoadMem16S>,
      &WasmGenerator::memop<kExprI64LoadMem16U>,
      &WasmGenerator::memop<kExprI64LoadMem32S>,
      &WasmGenerator::memop<kExprI64LoadMem32U>,
      &WasmGenerator::memop<kExprI64AtomicLoad>,
      &WasmGenerator::memop<kExprI64AtomicLoad8U>,
      &WasmGenerator::memop<kExprI64AtomicLoad16U>,
      &WasmGenerator::memop<kExprI64AtomicLoad32U>,

      &WasmGenerator::atomic_op<kExprI64AtomicAdd, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange, ValueType::kI32,
                                ValueType::kI64, ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange8U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange8U,
                                ValueType::kI32, ValueType::kI64,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange16U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange16U,
                                ValueType::kI32, ValueType::kI64,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange32U, ValueType::kI32,
                                ValueType::kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange32U,
                                ValueType::kI32, ValueType::kI64,
                                ValueType::kI64>,

      &WasmGenerator::get_local<ValueType::kI64>,
      &WasmGenerator::tee_local<ValueType::kI64>,
      &WasmGenerator::get_global<ValueType::kI64>,
      &WasmGenerator::op<kExprSelect, ValueType::kI64, ValueType::kI64,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kI64>,

      &WasmGenerator::call<ValueType::kI64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<ValueType::kF32>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(float)) {
    builder_->EmitF32Const(data->get<float>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<ValueType::kF32, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kF32>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kF32,
                               ValueType::kStmt>,

      &WasmGenerator::op<kExprF32Abs, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Neg, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Ceil, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Floor, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Trunc, ValueType::kF32>,
      &WasmGenerator::op<kExprF32NearestInt, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Sqrt, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Add, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Sub, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Mul, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Div, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Min, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32Max, ValueType::kF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF32CopySign, ValueType::kF32, ValueType::kF32>,

      &WasmGenerator::op<kExprF32SConvertI32, ValueType::kI32>,
      &WasmGenerator::op<kExprF32UConvertI32, ValueType::kI32>,
      &WasmGenerator::op<kExprF32SConvertI64, ValueType::kI64>,
      &WasmGenerator::op<kExprF32UConvertI64, ValueType::kI64>,
      &WasmGenerator::op<kExprF32ConvertF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF32ReinterpretI32, ValueType::kI32>,

      &WasmGenerator::block<ValueType::kF32>,
      &WasmGenerator::loop<ValueType::kF32>,
      &WasmGenerator::if_<ValueType::kF32, kIfElse>,
      &WasmGenerator::br_if<ValueType::kF32>,

      &WasmGenerator::memop<kExprF32LoadMem>,

      &WasmGenerator::get_local<ValueType::kF32>,
      &WasmGenerator::tee_local<ValueType::kF32>,
      &WasmGenerator::get_global<ValueType::kF32>,
      &WasmGenerator::op<kExprSelect, ValueType::kF32, ValueType::kF32,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kF32>,

      &WasmGenerator::call<ValueType::kF32>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<ValueType::kF64>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(double)) {
    builder_->EmitF64Const(data->get<double>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<ValueType::kF64, ValueType::kStmt>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kF64>,
      &WasmGenerator::sequence<ValueType::kStmt, ValueType::kF64,
                               ValueType::kStmt>,

      &WasmGenerator::op<kExprF64Abs, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Neg, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Ceil, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Floor, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Trunc, ValueType::kF64>,
      &WasmGenerator::op<kExprF64NearestInt, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Sqrt, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Add, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Sub, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Mul, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Div, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Min, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64Max, ValueType::kF64, ValueType::kF64>,
      &WasmGenerator::op<kExprF64CopySign, ValueType::kF64, ValueType::kF64>,

      &WasmGenerator::op<kExprF64SConvertI32, ValueType::kI32>,
      &WasmGenerator::op<kExprF64UConvertI32, ValueType::kI32>,
      &WasmGenerator::op<kExprF64SConvertI64, ValueType::kI64>,
      &WasmGenerator::op<kExprF64UConvertI64, ValueType::kI64>,
      &WasmGenerator::op<kExprF64ConvertF32, ValueType::kF32>,
      &WasmGenerator::op<kExprF64ReinterpretI64, ValueType::kI64>,

      &WasmGenerator::block<ValueType::kF64>,
      &WasmGenerator::loop<ValueType::kF64>,
      &WasmGenerator::if_<ValueType::kF64, kIfElse>,
      &WasmGenerator::br_if<ValueType::kF64>,

      &WasmGenerator::memop<kExprF64LoadMem>,

      &WasmGenerator::get_local<ValueType::kF64>,
      &WasmGenerator::tee_local<ValueType::kF64>,
      &WasmGenerator::get_global<ValueType::kF64>,
      &WasmGenerator::op<kExprSelect, ValueType::kF64, ValueType::kF64,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kF64>,

      &WasmGenerator::call<ValueType::kF64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<ValueType::kS128>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(int32_t)) {
    // TODO(v8:8460): v128.const is not implemented yet, and we need a way to
    // "bottom-out", so use a splat to generate this.
    builder_->EmitI32Const(data->get<int32_t>());
    builder_->EmitWithPrefix(kExprI8x16Splat);
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::simd_op<kExprI8x16Splat, ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI8x16Eq, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16Ne, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16LtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16LtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16GtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16GtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16LeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16LeU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16GeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16GeU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16Neg, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16Shl, ValueType::kS128, ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI8x16ShrS, ValueType::kS128,
                              ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI8x16ShrU, ValueType::kS128,
                              ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI8x16Add, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16AddSaturateS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16AddSaturateU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16Sub, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16SubSaturateS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16SubSaturateU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16MinS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16MinU, ValueType::kS128,
                              ValueType::kS128>,
      // I8x16Mul is prototyped but not in the proposal, thus omitted here.
      &WasmGenerator::simd_op<kExprI8x16MaxS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI8x16MaxU, ValueType::kS128,
                              ValueType::kS128>,

      &WasmGenerator::simd_op<kExprI16x8Splat, ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI16x8Eq, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8Ne, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8LtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8LtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8GtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8GtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8LeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8LeU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8GeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8GeU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8Neg, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8Shl, ValueType::kS128, ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI16x8ShrS, ValueType::kS128,
                              ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI16x8ShrU, ValueType::kS128,
                              ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI16x8Add, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8AddSaturateS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8AddSaturateU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8Sub, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8SubSaturateS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8SubSaturateU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8Mul, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8MinS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8MinU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8MaxS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI16x8MaxU, ValueType::kS128,
                              ValueType::kS128>,

      &WasmGenerator::simd_op<kExprI32x4Splat, ValueType::kI32>,
      &WasmGenerator::simd_op<kExprI32x4Eq, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4Ne, ValueType::kS128, ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4LtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4LtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4GtS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4GtU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4LeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4LeU, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4GeS, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI32x4GeU, ValueType::kS128,
                              ValueType::kS128>,

      &WasmGenerator::simd_op<kExprI64x2Splat, ValueType::kI64>,
      &WasmGenerator::simd_op<kExprF32x4Splat, ValueType::kF32>,
      &WasmGenerator::simd_op<kExprF64x2Splat, ValueType::kF64>,

      &WasmGenerator::simd_op<kExprI32x4Add, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprI64x2Add, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprF32x4Add, ValueType::kS128,
                              ValueType::kS128>,
      &WasmGenerator::simd_op<kExprF64x2Add, ValueType::kS128,
                              ValueType::kS128>,

      &WasmGenerator::memop<kExprS128LoadMem>};

  GenerateOneOf(alternatives, data);
}

void WasmGenerator::grow_memory(DataRange* data) {
  Generate<ValueType::kI32>(data);
  builder_->EmitWithU8(kExprMemoryGrow, 0);
}

void WasmGenerator::Generate(ValueType type, DataRange* data) {
  switch (type.kind()) {
    case ValueType::kStmt:
      return Generate<ValueType::kStmt>(data);
    case ValueType::kI32:
      return Generate<ValueType::kI32>(data);
    case ValueType::kI64:
      return Generate<ValueType::kI64>(data);
    case ValueType::kF32:
      return Generate<ValueType::kF32>(data);
    case ValueType::kF64:
      return Generate<ValueType::kF64>(data);
    case ValueType::kS128:
      return Generate<ValueType::kS128>(data);
    default:
      UNREACHABLE();
  }
}

FunctionSig* GenerateSig(Zone* zone, DataRange* data) {
  // Generate enough parameters to spill some to the stack.
  constexpr int kMaxParameters = 15;
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  bool has_return = data->get<bool>();

  FunctionSig::Builder builder(zone, has_return ? 1 : 0, num_params);
  if (has_return) builder.AddReturn(GetValueType(data));
  for (int i = 0; i < num_params; ++i) builder.AddParam(GetValueType(data));
  return builder.Build();
}

}  // namespace

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(
      Isolate* isolate, Zone* zone, Vector<const uint8_t> data,
      ZoneBuffer* buffer, int32_t* num_args,
      std::unique_ptr<WasmValue[]>* interpreter_args,
      std::unique_ptr<Handle<Object>[]>* compiler_args) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    DataRange range(data);
    std::vector<FunctionSig*> function_signatures;
    function_signatures.push_back(sigs.i_iii());

    static_assert(kMaxFunctions >= 1, "need min. 1 function");
    int num_functions = 1 + (range.get<uint8_t>() % kMaxFunctions);

    for (int i = 1; i < num_functions; ++i) {
      function_signatures.push_back(GenerateSig(zone, &range));
    }

    int num_globals = range.get<uint8_t>() % (kMaxGlobals + 1);
    std::vector<ValueType> globals;
    std::vector<uint8_t> mutable_globals;
    globals.reserve(num_globals);
    mutable_globals.reserve(num_globals);

    for (int i = 0; i < num_globals; ++i) {
      ValueType type = GetValueType(&range);
      // 1/8 of globals are immutable.
      const bool mutability = (range.get<uint8_t>() % 8) != 0;
      builder.AddGlobal(type, mutability, WasmInitExpr());
      globals.push_back(type);
      if (mutability) mutable_globals.push_back(static_cast<uint8_t>(i));
    }

    for (int i = 0; i < num_functions; ++i) {
      DataRange function_range =
          i == num_functions - 1 ? std::move(range) : range.split();

      FunctionSig* sig = function_signatures[i];
      WasmFunctionBuilder* f = builder.AddFunction(sig);

      WasmGenerator gen(f, function_signatures, globals, mutable_globals,
                        &function_range);
      ValueType return_type =
          sig->return_count() == 0 ? kWasmStmt : sig->GetReturn(0);
      gen.Generate(return_type, &function_range);

      f->Emit(kExprEnd);
      if (i == 0) builder.AddExport(CStrVector("main"), f);
    }

    builder.SetMaxMemorySize(32);
    // We enable shared memory to be able to test atomics.
    builder.SetHasSharedMemory();
    builder.WriteTo(buffer);

    *num_args = 3;
    interpreter_args->reset(
        new WasmValue[3]{WasmValue(1), WasmValue(2), WasmValue(3)});

    compiler_args->reset(new Handle<Object>[3] {
      handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(2), isolate),
          handle(Smi::FromInt(3), isolate)
    });
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bool require_valid = true;
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
