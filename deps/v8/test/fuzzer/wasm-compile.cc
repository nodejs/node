// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/ostreams.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

typedef uint8_t byte;

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
    memcpy(&result, data_.start(), num_bytes);
    data_ += num_bytes;
    return result;
  }

  DISALLOW_COPY_AND_ASSIGN(DataRange);
};

ValueType GetValueType(DataRange& data) {
  switch (data.get<uint8_t>() % 4) {
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
  template <WasmOpcode Op, ValueType... Args>
  void op(DataRange& data) {
    Generate<Args...>(data);
    builder_->Emit(Op);
  }

  class BlockScope {
   public:
    BlockScope(WasmGenerator* gen, WasmOpcode block_type, ValueType result_type,
               ValueType br_type)
        : gen_(gen) {
      gen->blocks_.push_back(br_type);
      gen->builder_->EmitWithU8(block_type,
                                ValueTypes::ValueTypeCodeFor(result_type));
    }

    ~BlockScope() {
      gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
  };

  template <ValueType T>
  void block(DataRange& data) {
    BlockScope block_scope(this, kExprBlock, T, T);
    Generate<T>(data);
  }

  template <ValueType T>
  void loop(DataRange& data) {
    // When breaking to a loop header, don't provide any input value (hence
    // kWasmStmt).
    BlockScope block_scope(this, kExprLoop, T, kWasmStmt);
    Generate<T>(data);
  }

  enum IfType { kIf, kIfElse };

  template <ValueType T, IfType type>
  void if_(DataRange& data) {
    static_assert(T == kWasmStmt || type == kIfElse,
                  "if without else cannot produce a value");
    Generate<kWasmI32>(data);
    BlockScope block_scope(this, kExprIf, T, T);
    Generate<T>(data);
    if (type == kIfElse) {
      builder_->Emit(kExprElse);
      Generate<T>(data);
    }
  }

  void br(DataRange& data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data.get<uint32_t>() % blocks_.size();
    const ValueType break_type = blocks_[target_block];

    Generate(break_type, data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
  }

  template <ValueType wanted_type>
  void br_if(DataRange& data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data.get<uint32_t>() % blocks_.size();
    const ValueType break_type = blocks_[target_block];

    Generate(break_type, data);
    Generate(kWasmI32, data);
    builder_->EmitWithI32V(
        kExprBrIf, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConvertOrGenerate(break_type, wanted_type, data);
  }

  // TODO(eholk): make this function constexpr once gcc supports it
  static uint8_t max_alignment(WasmOpcode memop) {
    switch (memop) {
      case kExprI64LoadMem:
      case kExprF64LoadMem:
      case kExprI64StoreMem:
      case kExprF64StoreMem:
        return 3;
      case kExprI32LoadMem:
      case kExprI64LoadMem32S:
      case kExprI64LoadMem32U:
      case kExprF32LoadMem:
      case kExprI32StoreMem:
      case kExprI64StoreMem32:
      case kExprF32StoreMem:
        return 2;
      case kExprI32LoadMem16S:
      case kExprI32LoadMem16U:
      case kExprI64LoadMem16S:
      case kExprI64LoadMem16U:
      case kExprI32StoreMem16:
      case kExprI64StoreMem16:
        return 1;
      case kExprI32LoadMem8S:
      case kExprI32LoadMem8U:
      case kExprI64LoadMem8S:
      case kExprI64LoadMem8U:
      case kExprI32StoreMem8:
      case kExprI64StoreMem8:
        return 0;
      default:
        return 0;
    }
  }

  template <WasmOpcode memory_op, ValueType... arg_types>
  void memop(DataRange& data) {
    const uint8_t align = data.get<uint8_t>() % (max_alignment(memory_op) + 1);
    const uint32_t offset = data.get<uint32_t>();

    // Generate the index and the arguments, if any.
    Generate<kWasmI32, arg_types...>(data);

    builder_->Emit(memory_op);
    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  void drop(DataRange& data) {
    Generate(GetValueType(data), data);
    builder_->Emit(kExprDrop);
  }

  template <ValueType wanted_type>
  void call(DataRange& data) {
    call(data, wanted_type);
  }

  void Convert(ValueType src, ValueType dst) {
    auto idx = [](ValueType t) -> int {
      switch (t) {
        case kWasmI32:
          return 0;
        case kWasmI64:
          return 1;
        case kWasmF32:
          return 2;
        case kWasmF64:
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

  void ConvertOrGenerate(ValueType src, ValueType dst, DataRange& data) {
    if (src == dst) return;
    if (src == kWasmStmt && dst != kWasmStmt) {
      Generate(dst, data);
    } else if (dst == kWasmStmt && src != kWasmStmt) {
      builder_->Emit(kExprDrop);
    } else {
      Convert(src, dst);
    }
  }

  void call(DataRange& data, ValueType wanted_type) {
    int func_index = data.get<uint8_t>() % functions_.size();
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

  Var GetRandomLocal(DataRange& data) {
    uint32_t num_params =
        static_cast<uint32_t>(builder_->signature()->parameter_count());
    uint32_t num_locals = static_cast<uint32_t>(locals_.size());
    if (num_params + num_locals == 0) return {};
    uint32_t index = data.get<uint8_t>() % (num_params + num_locals);
    ValueType type = index < num_params ? builder_->signature()->GetParam(index)
                                        : locals_[index - num_params];
    return {index, type};
  }

  template <ValueType wanted_type>
  void local_op(DataRange& data, WasmOpcode opcode) {
    Var local = GetRandomLocal(data);
    // If there are no locals and no parameters, just generate any value (if a
    // value is needed), or do nothing.
    if (!local.is_valid()) {
      if (wanted_type == kWasmStmt) return;
      return Generate<wanted_type>(data);
    }

    if (opcode != kExprGetLocal) Generate(local.type, data);
    builder_->EmitWithU32V(opcode, local.index);
    if (wanted_type != kWasmStmt && local.type != wanted_type) {
      Convert(local.type, wanted_type);
    }
  }

  template <ValueType wanted_type>
  void get_local(DataRange& data) {
    static_assert(wanted_type != kWasmStmt, "illegal type");
    local_op<wanted_type>(data, kExprGetLocal);
  }

  void set_local(DataRange& data) { local_op<kWasmStmt>(data, kExprSetLocal); }

  template <ValueType wanted_type>
  void tee_local(DataRange& data) {
    local_op<wanted_type>(data, kExprTeeLocal);
  }

  template <size_t num_bytes>
  void i32_const(DataRange& data) {
    builder_->EmitI32Const(data.get<int32_t, num_bytes>());
  }

  template <size_t num_bytes>
  void i64_const(DataRange& data) {
    builder_->EmitI64Const(data.get<int64_t, num_bytes>());
  }

  Var GetRandomGlobal(DataRange& data, bool ensure_mutable) {
    uint32_t index;
    if (ensure_mutable) {
      if (mutable_globals_.empty()) return {};
      index = mutable_globals_[data.get<uint8_t>() % mutable_globals_.size()];
    } else {
      if (globals_.empty()) return {};
      index = data.get<uint8_t>() % globals_.size();
    }
    ValueType type = globals_[index];
    return {index, type};
  }

  template <ValueType wanted_type>
  void global_op(DataRange& data) {
    constexpr bool is_set = wanted_type == kWasmStmt;
    Var global = GetRandomGlobal(data, is_set);
    // If there are no globals, just generate any value (if a value is needed),
    // or do nothing.
    if (!global.is_valid()) {
      if (wanted_type == kWasmStmt) return;
      return Generate<wanted_type>(data);
    }

    if (is_set) Generate(global.type, data);
    builder_->EmitWithU32V(is_set ? kExprSetGlobal : kExprGetGlobal,
                           global.index);
    if (!is_set && global.type != wanted_type) {
      Convert(global.type, wanted_type);
    }
  }

  template <ValueType wanted_type>
  void get_global(DataRange& data) {
    static_assert(wanted_type != kWasmStmt, "illegal type");
    global_op<wanted_type>(data);
  }

  void set_global(DataRange& data) { global_op<kWasmStmt>(data); }

  template <ValueType... Types>
  void sequence(DataRange& data) {
    Generate<Types...>(data);
  }

  void current_memory(DataRange& data) {
    builder_->EmitWithU8(kExprMemorySize, 0);
  }

  void grow_memory(DataRange& data);

  using generate_fn = void (WasmGenerator::*const)(DataRange&);

  template <size_t N>
  void GenerateOneOf(generate_fn (&alternates)[N], DataRange& data) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternates. Replace with a bigger type if needed.");
    const auto which = data.get<uint8_t>();

    generate_fn alternate = alternates[which % N];
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
                const std::vector<uint8_t>& mutable_globals, DataRange& data)
      : builder_(fn),
        functions_(functions),
        globals_(globals),
        mutable_globals_(mutable_globals) {
    FunctionSig* sig = fn->signature();
    DCHECK_GE(1, sig->return_count());
    blocks_.push_back(sig->return_count() == 0 ? kWasmStmt : sig->GetReturn(0));

    constexpr uint32_t kMaxLocals = 32;
    locals_.resize(data.get<uint8_t>() % kMaxLocals);
    for (ValueType& local : locals_) {
      local = GetValueType(data);
      fn->AddLocal(local);
    }
  }

  void Generate(ValueType type, DataRange& data);

  template <ValueType T>
  void Generate(DataRange& data);

  template <ValueType T1, ValueType T2, ValueType... Ts>
  void Generate(DataRange& data) {
    // TODO(clemensh): Implement a more even split.
    auto first_data = data.split();
    Generate<T1>(first_data);
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
void WasmGenerator::Generate<kWasmStmt>(DataRange& data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() == 0) return;

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmStmt, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmStmt, kWasmStmt, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmStmt, kWasmStmt, kWasmStmt,
                               kWasmStmt, kWasmStmt, kWasmStmt, kWasmStmt>,
      &WasmGenerator::block<kWasmStmt>,
      &WasmGenerator::loop<kWasmStmt>,
      &WasmGenerator::if_<kWasmStmt, kIf>,
      &WasmGenerator::if_<kWasmStmt, kIfElse>,
      &WasmGenerator::br,
      &WasmGenerator::br_if<kWasmStmt>,

      &WasmGenerator::memop<kExprI32StoreMem, kWasmI32>,
      &WasmGenerator::memop<kExprI32StoreMem8, kWasmI32>,
      &WasmGenerator::memop<kExprI32StoreMem16, kWasmI32>,
      &WasmGenerator::memop<kExprI64StoreMem, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem8, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem16, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem32, kWasmI64>,
      &WasmGenerator::memop<kExprF32StoreMem, kWasmF32>,
      &WasmGenerator::memop<kExprF64StoreMem, kWasmF64>,

      &WasmGenerator::drop,

      &WasmGenerator::call<kWasmStmt>,

      &WasmGenerator::set_local,
      &WasmGenerator::set_global};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmI32>(DataRange& data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= 1) {
    builder_->EmitI32Const(data.get<uint32_t>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::i32_const<1>,
      &WasmGenerator::i32_const<2>,
      &WasmGenerator::i32_const<3>,
      &WasmGenerator::i32_const<4>,

      &WasmGenerator::sequence<kWasmI32, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmI32>,
      &WasmGenerator::sequence<kWasmStmt, kWasmI32, kWasmStmt>,

      &WasmGenerator::op<kExprI32Eqz, kWasmI32>,
      &WasmGenerator::op<kExprI32Eq, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Ne, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32LtS, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32LtU, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32GeS, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32GeU, kWasmI32, kWasmI32>,

      &WasmGenerator::op<kExprI64Eqz, kWasmI64>,
      &WasmGenerator::op<kExprI64Eq, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Ne, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64LtS, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64LtU, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64GeS, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64GeU, kWasmI64, kWasmI64>,

      &WasmGenerator::op<kExprF32Eq, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Ne, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Lt, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Ge, kWasmF32, kWasmF32>,

      &WasmGenerator::op<kExprF64Eq, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Ne, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Lt, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Ge, kWasmF64, kWasmF64>,

      &WasmGenerator::op<kExprI32Add, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Sub, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Mul, kWasmI32, kWasmI32>,

      &WasmGenerator::op<kExprI32DivS, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32DivU, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32RemS, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32RemU, kWasmI32, kWasmI32>,

      &WasmGenerator::op<kExprI32And, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Ior, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Xor, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Shl, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32ShrU, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32ShrS, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Ror, kWasmI32, kWasmI32>,
      &WasmGenerator::op<kExprI32Rol, kWasmI32, kWasmI32>,

      &WasmGenerator::op<kExprI32Clz, kWasmI32>,
      &WasmGenerator::op<kExprI32Ctz, kWasmI32>,
      &WasmGenerator::op<kExprI32Popcnt, kWasmI32>,

      &WasmGenerator::op<kExprI32ConvertI64, kWasmI64>,
      &WasmGenerator::op<kExprI32SConvertF32, kWasmF32>,
      &WasmGenerator::op<kExprI32UConvertF32, kWasmF32>,
      &WasmGenerator::op<kExprI32SConvertF64, kWasmF64>,
      &WasmGenerator::op<kExprI32UConvertF64, kWasmF64>,
      &WasmGenerator::op<kExprI32ReinterpretF32, kWasmF32>,

      &WasmGenerator::block<kWasmI32>,
      &WasmGenerator::loop<kWasmI32>,
      &WasmGenerator::if_<kWasmI32, kIfElse>,
      &WasmGenerator::br_if<kWasmI32>,

      &WasmGenerator::memop<kExprI32LoadMem>,
      &WasmGenerator::memop<kExprI32LoadMem8S>,
      &WasmGenerator::memop<kExprI32LoadMem8U>,
      &WasmGenerator::memop<kExprI32LoadMem16S>,
      &WasmGenerator::memop<kExprI32LoadMem16U>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory,

      &WasmGenerator::get_local<kWasmI32>,
      &WasmGenerator::tee_local<kWasmI32>,
      &WasmGenerator::get_global<kWasmI32>,

      &WasmGenerator::call<kWasmI32>};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmI64>(DataRange& data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= 1) {
    builder_->EmitI64Const(data.get<int64_t>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::i64_const<1>,
      &WasmGenerator::i64_const<2>,
      &WasmGenerator::i64_const<3>,
      &WasmGenerator::i64_const<4>,
      &WasmGenerator::i64_const<5>,
      &WasmGenerator::i64_const<6>,
      &WasmGenerator::i64_const<7>,
      &WasmGenerator::i64_const<8>,

      &WasmGenerator::sequence<kWasmI64, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmI64>,
      &WasmGenerator::sequence<kWasmStmt, kWasmI64, kWasmStmt>,

      &WasmGenerator::op<kExprI64Add, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Sub, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Mul, kWasmI64, kWasmI64>,

      &WasmGenerator::op<kExprI64DivS, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64DivU, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64RemS, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64RemU, kWasmI64, kWasmI64>,

      &WasmGenerator::op<kExprI64And, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Ior, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Xor, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Shl, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64ShrU, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64ShrS, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Ror, kWasmI64, kWasmI64>,
      &WasmGenerator::op<kExprI64Rol, kWasmI64, kWasmI64>,

      &WasmGenerator::op<kExprI64Clz, kWasmI64>,
      &WasmGenerator::op<kExprI64Ctz, kWasmI64>,
      &WasmGenerator::op<kExprI64Popcnt, kWasmI64>,

      &WasmGenerator::block<kWasmI64>,
      &WasmGenerator::loop<kWasmI64>,
      &WasmGenerator::if_<kWasmI64, kIfElse>,
      &WasmGenerator::br_if<kWasmI64>,

      &WasmGenerator::memop<kExprI64LoadMem>,
      &WasmGenerator::memop<kExprI64LoadMem8S>,
      &WasmGenerator::memop<kExprI64LoadMem8U>,
      &WasmGenerator::memop<kExprI64LoadMem16S>,
      &WasmGenerator::memop<kExprI64LoadMem16U>,
      &WasmGenerator::memop<kExprI64LoadMem32S>,
      &WasmGenerator::memop<kExprI64LoadMem32U>,

      &WasmGenerator::get_local<kWasmI64>,
      &WasmGenerator::tee_local<kWasmI64>,
      &WasmGenerator::get_global<kWasmI64>,

      &WasmGenerator::call<kWasmI64>};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmF32>(DataRange& data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(float)) {
    builder_->EmitF32Const(data.get<float>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmF32, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmF32>,
      &WasmGenerator::sequence<kWasmStmt, kWasmF32, kWasmStmt>,

      &WasmGenerator::op<kExprF32Add, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Sub, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Mul, kWasmF32, kWasmF32>,

      &WasmGenerator::block<kWasmF32>,
      &WasmGenerator::loop<kWasmF32>,
      &WasmGenerator::if_<kWasmF32, kIfElse>,
      &WasmGenerator::br_if<kWasmF32>,

      &WasmGenerator::memop<kExprF32LoadMem>,

      &WasmGenerator::get_local<kWasmF32>,
      &WasmGenerator::tee_local<kWasmF32>,
      &WasmGenerator::get_global<kWasmF32>,

      &WasmGenerator::call<kWasmF32>};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmF64>(DataRange& data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(double)) {
    builder_->EmitF64Const(data.get<double>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmF64, kWasmStmt>,
      &WasmGenerator::sequence<kWasmStmt, kWasmF64>,
      &WasmGenerator::sequence<kWasmStmt, kWasmF64, kWasmStmt>,

      &WasmGenerator::op<kExprF64Add, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Sub, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Mul, kWasmF64, kWasmF64>,

      &WasmGenerator::block<kWasmF64>,
      &WasmGenerator::loop<kWasmF64>,
      &WasmGenerator::if_<kWasmF64, kIfElse>,
      &WasmGenerator::br_if<kWasmF64>,

      &WasmGenerator::memop<kExprF64LoadMem>,

      &WasmGenerator::get_local<kWasmF64>,
      &WasmGenerator::tee_local<kWasmF64>,
      &WasmGenerator::get_global<kWasmF64>,

      &WasmGenerator::call<kWasmF64>};

  GenerateOneOf(alternates, data);
}

void WasmGenerator::grow_memory(DataRange& data) {
  Generate<kWasmI32>(data);
  builder_->EmitWithU8(kExprMemoryGrow, 0);
}

void WasmGenerator::Generate(ValueType type, DataRange& data) {
  switch (type) {
    case kWasmStmt:
      return Generate<kWasmStmt>(data);
    case kWasmI32:
      return Generate<kWasmI32>(data);
    case kWasmI64:
      return Generate<kWasmI64>(data);
    case kWasmF32:
      return Generate<kWasmF32>(data);
    case kWasmF64:
      return Generate<kWasmF64>(data);
    default:
      UNREACHABLE();
  }
}

FunctionSig* GenerateSig(Zone* zone, DataRange& data) {
  // Generate enough parameters to spill some to the stack.
  constexpr int kMaxParameters = 15;
  int num_params = int{data.get<uint8_t>()} % (kMaxParameters + 1);
  bool has_return = data.get<bool>();

  FunctionSig::Builder builder(zone, has_return ? 1 : 0, num_params);
  if (has_return) builder.AddReturn(GetValueType(data));
  for (int i = 0; i < num_params; ++i) builder.AddParam(GetValueType(data));
  return builder.Build();
}

}  // namespace

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(
      Isolate* isolate, Zone* zone, Vector<const uint8_t> data,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmValue[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    DataRange range(data);
    std::vector<FunctionSig*> function_signatures;
    function_signatures.push_back(sigs.i_iii());

    static_assert(kMaxFunctions >= 1, "need min. 1 function");
    int num_functions = 1 + (range.get<uint8_t>() % kMaxFunctions);

    for (int i = 1; i < num_functions; ++i) {
      function_signatures.push_back(GenerateSig(zone, range));
    }

    int num_globals = range.get<uint8_t>() % (kMaxGlobals + 1);
    std::vector<ValueType> globals;
    std::vector<uint8_t> mutable_globals;
    globals.reserve(num_globals);
    mutable_globals.reserve(num_globals);

    for (int i = 0; i < num_globals; ++i) {
      ValueType type = GetValueType(range);
      const bool exported = range.get<bool>();
      // 1/8 of globals are immutable.
      const bool mutability = (range.get<uint8_t>() % 8) != 0;
      builder.AddGlobal(type, exported, mutability, WasmInitExpr());
      globals.push_back(type);
      if (mutability) mutable_globals.push_back(static_cast<uint8_t>(i));
    }

    for (int i = 0; i < num_functions; ++i) {
      DataRange function_range =
          i == num_functions - 1 ? std::move(range) : range.split();

      FunctionSig* sig = function_signatures[i];
      WasmFunctionBuilder* f = builder.AddFunction(sig);

      WasmGenerator gen(f, function_signatures, globals, mutable_globals,
                        function_range);
      ValueType return_type =
          sig->return_count() == 0 ? kWasmStmt : sig->GetReturn(0);
      gen.Generate(return_type, function_range);

      f->Emit(kExprEnd);
      if (i == 0) builder.AddExport(CStrVector("main"), f);
    }

    builder.SetMaxMemorySize(32);
    builder.WriteTo(buffer);

    num_args = 3;
    interpreter_args.reset(
        new WasmValue[3]{WasmValue(1), WasmValue(2), WasmValue(3)});

    compiler_args.reset(new Handle<Object>[3]{
        handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(2), isolate),
        handle(Smi::FromInt(3), isolate)});
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bool require_valid = true;
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
