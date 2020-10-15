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
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"
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
constexpr int kMaxParameters = 15;
constexpr int kMaxReturns = 15;

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
    // DISABLE FOR BOOL
    // The -O3 on release will break the result. This creates a different
    // observable side effect when invoking get<bool> between debug and release
    // version, which eventually makes the code output different as well as
    // raising various unrecoverable errors on runtime. It is caused by
    // undefined behavior of assigning boolean via memcpy from randomized bytes.
    STATIC_ASSERT(!(std::is_same<T, bool>::value));
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
    BlockScope(WasmGenerator* gen, WasmOpcode block_type,
               Vector<const ValueType> param_types,
               Vector<const ValueType> result_types,
               Vector<const ValueType> br_types)
        : gen_(gen) {
      gen->blocks_.emplace_back(br_types.begin(), br_types.end());
      if (param_types.size() == 0 && result_types.size() == 0) {
        gen->builder_->EmitWithU8(block_type, kWasmStmt.value_type_code());
        return;
      }
      if (param_types.size() == 0 && result_types.size() == 1) {
        gen->builder_->EmitWithU8(block_type,
                                  result_types[0].value_type_code());
        return;
      }
      // Multi-value block.
      Zone* zone = gen->builder_->builder()->zone();
      FunctionSig::Builder builder(zone, result_types.size(),
                                   param_types.size());
      for (auto& type : param_types) {
        DCHECK_NE(type, kWasmStmt);
        builder.AddParam(type);
      }
      for (auto& type : result_types) {
        DCHECK_NE(type, kWasmStmt);
        builder.AddReturn(type);
      }
      FunctionSig* sig = builder.Build();
      int sig_id = gen->builder_->builder()->AddSignature(sig);
      gen->builder_->EmitWithI32V(block_type, sig_id);
    }

    ~BlockScope() {
      gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
  };

  void block(Vector<const ValueType> param_types,
             Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprBlock, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueType::Kind T>
  void block(DataRange* data) {
    block({}, VectorOf({ValueType::Primitive(T)}), data);
  }

  void loop(Vector<const ValueType> param_types,
            Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprLoop, param_types, return_types,
                           param_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueType::Kind T>
  void loop(DataRange* data) {
    loop({}, VectorOf({ValueType::Primitive(T)}), data);
  }

  enum IfType { kIf, kIfElse };

  void if_(Vector<const ValueType> param_types,
           Vector<const ValueType> return_types, IfType type, DataRange* data) {
    // One-armed "if" are only valid if the input and output types are the same.
    DCHECK_IMPLIES(type == kIf, param_types == return_types);
    Generate(kWasmI32, data);
    BlockScope block_scope(this, kExprIf, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
    if (type == kIfElse) {
      builder_->Emit(kExprElse);
      ConsumeAndGenerate(param_types, return_types, data);
    }
  }

  template <ValueType::Kind T, IfType type>
  void if_(DataRange* data) {
    static_assert(T == ValueType::kStmt || type == kIfElse,
                  "if without else cannot produce a value");
    if_({},
        T == ValueType::kStmt ? Vector<ValueType>{}
                              : VectorOf({ValueType::Primitive(T)}),
        type, data);
  }

  void any_block(Vector<const ValueType> param_types,
                 Vector<const ValueType> return_types, DataRange* data) {
    uint8_t block_type = data->get<uint8_t>() % 4;
    switch (block_type) {
      case 0:
        block(param_types, return_types, data);
        return;
      case 1:
        loop(param_types, return_types, data);
        return;
      case 2:
        if (param_types == return_types) {
          if_({}, {}, kIf, data);
          return;
        }
        V8_FALLTHROUGH;
      case 3:
        if_(param_types, return_types, kIfElse, data);
        return;
    }
  }

  void br(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const auto break_types = blocks_[target_block];

    Generate(VectorOf(break_types), data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
  }

  template <ValueType::Kind wanted_type>
  void br_if(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const auto break_types = VectorOf(blocks_[target_block]);

    Generate(break_types, data);
    Generate(kWasmI32, data);
    builder_->EmitWithI32V(
        kExprBrIf, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConsumeAndGenerate(break_types,
                       wanted_type == ValueType::kStmt
                           ? Vector<ValueType>{}
                           : VectorOf({ValueType::Primitive(wanted_type)}),
                       data);
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
      case kExprI16x8Load8x8S:
      case kExprI16x8Load8x8U:
      case kExprI32x4Load16x4S:
      case kExprI32x4Load16x4U:
      case kExprI64x2Load32x2S:
      case kExprI64x2Load32x2U:
      case kExprS64x2LoadSplat:
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
      case kExprS32x4LoadSplat:
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
      case kExprS16x8LoadSplat:
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
      case kExprS8x16LoadSplat:
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
  void op_with_prefix(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
  }

  void simd_const(DataRange* data) {
    builder_->EmitWithPrefix(kExprS128Const);
    for (int i = 0; i < kSimd128Size; i++) {
      builder_->EmitByte(data->get<byte>());
    }
  }

  template <WasmOpcode Op, int lanes, ValueType::Kind... Args>
  void simd_lane_op(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
    builder_->EmitByte(data->get<byte>() % lanes);
  }

  void simd_shuffle(DataRange* data) {
    Generate<ValueType::kS128, ValueType::kS128>(data);
    builder_->EmitWithPrefix(kExprS8x16Shuffle);
    for (int i = 0; i < kSimd128Size; i++) {
      builder_->EmitByte(static_cast<uint8_t>(data->get<byte>() % 32));
    }
  }

  void drop(DataRange* data) {
    Generate(GetValueType(data), data);
    builder_->Emit(kExprDrop);
  }

  enum CallDirect : bool { kCallDirect = true, kCallIndirect = false };

  template <ValueType::Kind wanted_type>
  void call(DataRange* data) {
    call(data, ValueType::Primitive(wanted_type), kCallDirect);
  }

  template <ValueType::Kind wanted_type>
  void call_indirect(DataRange* data) {
    call(data, ValueType::Primitive(wanted_type), kCallIndirect);
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

  void call(DataRange* data, ValueType wanted_type, CallDirect call_direct) {
    uint8_t random_byte = data->get<uint8_t>();
    int func_index = random_byte % functions_.size();
    uint32_t sig_index = functions_[func_index];
    FunctionSig* sig = builder_->builder()->GetSignature(sig_index);
    // Generate arguments.
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      Generate(sig->GetParam(i), data);
    }
    // Emit call.
    // If the return types of the callee happen to match the return types of the
    // caller, generate a tail call.
    bool use_return_call = random_byte > 127;
    if (use_return_call &&
        std::equal(sig->returns().begin(), sig->returns().end(),
                   builder_->signature()->returns().begin(),
                   builder_->signature()->returns().end())) {
      if (call_direct) {
        builder_->EmitWithU32V(kExprReturnCall, func_index);
      } else {
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprReturnCallIndirect, sig_index);
        builder_->EmitByte(0);  // Table index.
      }
      return;
    } else {
      if (call_direct) {
        builder_->EmitWithU32V(kExprCallFunction, func_index);
      } else {
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprCallIndirect, sig_index);
        builder_->EmitByte(0);  // Table index.
      }
    }
    if (sig->return_count() == 0 && wanted_type != kWasmStmt) {
      // The call did not generate a value. Thus just generate it here.
      Generate(wanted_type, data);
      return;
    }
    if (wanted_type == kWasmStmt) {
      // The call did generate values, but we did not want one.
      for (size_t i = 0; i < sig->return_count(); ++i) {
        builder_->Emit(kExprDrop);
      }
      return;
    }
    auto return_types = VectorOf(sig->returns().begin(), sig->return_count());
    auto wanted_types =
        VectorOf(&wanted_type, wanted_type == kWasmStmt ? 0 : 1);
    ConsumeAndGenerate(return_types, wanted_types, data);
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
      Convert(local.type, ValueType::Primitive(wanted_type));
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
      Convert(global.type, ValueType::Primitive(wanted_type));
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
                           ValueType::Primitive(select_type).value_type_code());
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
  WasmGenerator(WasmFunctionBuilder* fn, const std::vector<uint32_t>& functions,
                const std::vector<ValueType>& globals,
                const std::vector<uint8_t>& mutable_globals, DataRange* data)
      : builder_(fn),
        functions_(functions),
        globals_(globals),
        mutable_globals_(mutable_globals) {
    FunctionSig* sig = fn->signature();
    blocks_.emplace_back();
    for (size_t i = 0; i < sig->return_count(); ++i) {
      blocks_.back().push_back(sig->GetReturn(i));
    }

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

  std::vector<ValueType> GenerateTypes(DataRange* data);
  void Generate(Vector<const ValueType> types, DataRange* data);
  void ConsumeAndGenerate(Vector<const ValueType> parameter_types,
                          Vector<const ValueType> return_types,
                          DataRange* data);

 private:
  WasmFunctionBuilder* builder_;
  std::vector<std::vector<ValueType>> blocks_;
  const std::vector<uint32_t>& functions_;
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
void WasmGenerator::block<ValueType::kStmt>(DataRange* data) {
  block({}, {}, data);
}

template <>
void WasmGenerator::loop<ValueType::kStmt>(DataRange* data) {
  loop({}, {}, data);
}

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
      &WasmGenerator::call_indirect<ValueType::kStmt>,

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

      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF32, ValueType::kF32>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF32, ValueType::kF32>,
      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF64, ValueType::kF64>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF64, ValueType::kF64>,

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

      &WasmGenerator::op_with_prefix<kExprV8x16AnyTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprV8x16AllTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16BitMask, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprV16x8AnyTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprV16x8AllTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8BitMask, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprV32x4AnyTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprV32x4AllTrue, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4BitMask, ValueType::kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneS, 16,
                                   ValueType::kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneU, 16,
                                   ValueType::kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneS, 8, ValueType::kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneU, 8, ValueType::kS128>,
      &WasmGenerator::simd_lane_op<kExprI32x4ExtractLane, 4, ValueType::kS128>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory,

      &WasmGenerator::get_local<ValueType::kI32>,
      &WasmGenerator::tee_local<ValueType::kI32>,
      &WasmGenerator::get_global<ValueType::kI32>,
      &WasmGenerator::op<kExprSelect, ValueType::kI32, ValueType::kI32,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kI32>,

      &WasmGenerator::call<ValueType::kI32>,
      &WasmGenerator::call_indirect<ValueType::kI32>};

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

      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF32, ValueType::kF32>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF32, ValueType::kF32>,
      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF64, ValueType::kF64>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF64, ValueType::kF64>,

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

      &WasmGenerator::simd_lane_op<kExprI64x2ExtractLane, 2, ValueType::kS128>,

      &WasmGenerator::get_local<ValueType::kI64>,
      &WasmGenerator::tee_local<ValueType::kI64>,
      &WasmGenerator::get_global<ValueType::kI64>,
      &WasmGenerator::op<kExprSelect, ValueType::kI64, ValueType::kI64,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kI64>,

      &WasmGenerator::call<ValueType::kI64>,
      &WasmGenerator::call_indirect<ValueType::kI64>};

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

      &WasmGenerator::simd_lane_op<kExprF32x4ExtractLane, 4, ValueType::kS128>,

      &WasmGenerator::get_local<ValueType::kF32>,
      &WasmGenerator::tee_local<ValueType::kF32>,
      &WasmGenerator::get_global<ValueType::kF32>,
      &WasmGenerator::op<kExprSelect, ValueType::kF32, ValueType::kF32,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kF32>,

      &WasmGenerator::call<ValueType::kF32>,
      &WasmGenerator::call_indirect<ValueType::kF32>};

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

      &WasmGenerator::simd_lane_op<kExprF64x2ExtractLane, 2, ValueType::kS128>,

      &WasmGenerator::get_local<ValueType::kF64>,
      &WasmGenerator::tee_local<ValueType::kF64>,
      &WasmGenerator::get_global<ValueType::kF64>,
      &WasmGenerator::op<kExprSelect, ValueType::kF64, ValueType::kF64,
                         ValueType::kI32>,
      &WasmGenerator::select_with_type<ValueType::kF64>,

      &WasmGenerator::call<ValueType::kF64>,
      &WasmGenerator::call_indirect<ValueType::kF64>};

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
      &WasmGenerator::simd_const,
      &WasmGenerator::simd_lane_op<kExprI8x16ReplaceLane, 16, ValueType::kS128,
                                   ValueType::kI32>,
      &WasmGenerator::simd_lane_op<kExprI16x8ReplaceLane, 8, ValueType::kS128,
                                   ValueType::kI32>,
      &WasmGenerator::simd_lane_op<kExprI32x4ReplaceLane, 4, ValueType::kS128,
                                   ValueType::kI32>,
      &WasmGenerator::simd_lane_op<kExprI64x2ReplaceLane, 2, ValueType::kS128,
                                   ValueType::kI64>,
      &WasmGenerator::simd_lane_op<kExprF32x4ReplaceLane, 4, ValueType::kS128,
                                   ValueType::kF32>,
      &WasmGenerator::simd_lane_op<kExprF64x2ReplaceLane, 2, ValueType::kS128,
                                   ValueType::kF64>,

      &WasmGenerator::op_with_prefix<kExprI8x16Splat, ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Eq, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Ne, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Abs, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Shl, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrS, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrU, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSaturateS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSaturateU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSaturateS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSaturateU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinU, ValueType::kS128,
                                     ValueType::kS128>,
      // I8x16Mul is prototyped but not in the proposal, thus omitted here.
      &WasmGenerator::op_with_prefix<kExprI8x16MaxS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MaxU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16RoundingAverageU,
                                     ValueType::kS128, ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8Splat, ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Eq, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Ne, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Abs, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Shl, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrS, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrU, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSaturateS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSaturateU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSaturateS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSaturateU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Mul, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8RoundingAverageU,
                                     ValueType::kS128, ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4Splat, ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Eq, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Ne, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Abs, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Shl, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrS, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrU, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Mul, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinU, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxS, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxU, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI64x2Splat, ValueType::kI64>,
      &WasmGenerator::op_with_prefix<kExprI64x2Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Shl, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrS, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrU, ValueType::kS128,
                                     ValueType::kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Mul, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprF32x4Splat, ValueType::kF32>,
      &WasmGenerator::op_with_prefix<kExprF32x4Eq, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ne, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Lt, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Gt, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Le, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ge, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Abs, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sqrt, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Mul, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Div, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Min, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Max, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprF64x2Splat, ValueType::kF64>,
      &WasmGenerator::op_with_prefix<kExprF64x2Eq, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ne, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Lt, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Gt, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Le, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ge, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Abs, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Neg, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sqrt, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Add, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sub, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Mul, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Div, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Min, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Max, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4SConvertF32x4, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertF32x4, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4SConvertI32x4, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4UConvertI32x4, ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI8x16SConvertI16x8, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16UConvertI16x8, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI32x4, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI32x4, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16Low,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16High,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16Low,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16High,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8Low,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8High,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8Low,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8High,
                                     ValueType::kS128>,

      &WasmGenerator::op_with_prefix<kExprS128Not, ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprS128And, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprS128AndNot, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Or, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Xor, ValueType::kS128,
                                     ValueType::kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Select, ValueType::kS128,
                                     ValueType::kS128, ValueType::kS128>,

      &WasmGenerator::simd_shuffle,
      &WasmGenerator::op_with_prefix<kExprS8x16Swizzle, ValueType::kS128,
                                     ValueType::kS128>,

      &WasmGenerator::memop<kExprS128LoadMem>,
      &WasmGenerator::memop<kExprI16x8Load8x8S>,
      &WasmGenerator::memop<kExprI16x8Load8x8U>,
      &WasmGenerator::memop<kExprI32x4Load16x4S>,
      &WasmGenerator::memop<kExprI32x4Load16x4U>,
      &WasmGenerator::memop<kExprI64x2Load32x2S>,
      &WasmGenerator::memop<kExprI64x2Load32x2U>,
      &WasmGenerator::memop<kExprS8x16LoadSplat>,
      &WasmGenerator::memop<kExprS16x8LoadSplat>,
      &WasmGenerator::memop<kExprS32x4LoadSplat>,
      &WasmGenerator::memop<kExprS64x2LoadSplat>};

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

std::vector<ValueType> WasmGenerator::GenerateTypes(DataRange* data) {
  std::vector<ValueType> types;
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  for (int i = 0; i < num_params; ++i) {
    types.push_back(GetValueType(data));
  }
  return types;
}

void WasmGenerator::Generate(Vector<const ValueType> types, DataRange* data) {
  // Maybe emit a multi-value block with the expected return type. Use a
  // non-default value to indicate block generation to avoid recursion when we
  // reach the end of the data.
  bool generate_block = data->get<uint8_t>() % 32 == 1;
  if (generate_block) {
    GeneratorRecursionScope rec_scope(this);
    if (!recursion_limit_reached()) {
      const auto param_types = GenerateTypes(data);
      Generate(VectorOf(param_types), data);
      any_block(VectorOf(param_types), types, data);
      return;
    }
  }

  if (types.size() == 0) {
    Generate(kWasmStmt, data);
    return;
  }
  if (types.size() == 1) {
    Generate(types[0], data);
    return;
  }

  // Split the types in two halves and recursively generate each half.
  // Each half is non empty to ensure termination.
  size_t split_index = data->get<uint8_t>() % (types.size() - 1) + 1;
  Vector<const ValueType> lower_half = types.SubVector(0, split_index);
  Vector<const ValueType> upper_half =
      types.SubVector(split_index, types.size());
  DataRange first_range = data->split();
  Generate(lower_half, &first_range);
  Generate(upper_half, data);
}

// Emit code to match an arbitrary signature.
void WasmGenerator::ConsumeAndGenerate(Vector<const ValueType> param_types,
                                       Vector<const ValueType> return_types,
                                       DataRange* data) {
  if (param_types.size() == 0) {
    Generate(return_types, data);
    return;
  }
  // Keep exactly one of the parameters on the stack with a combination of drops
  // and selects, convert this value to the first return type, and generate the
  // remaining types.
  // TODO(thibaudm): Improve this strategy to potentially generate any sequence
  // of instructions matching the given signature.
  size_t return_index = data->get<uint8_t>() % param_types.size();
  for (size_t i = param_types.size() - 1; i > return_index; --i) {
    builder_->Emit(kExprDrop);
  }
  for (size_t i = return_index; i > 0; --i) {
    Convert(param_types[i], param_types[i - 1]);
    builder_->EmitI32Const(0);
    builder_->Emit(kExprSelect);
  }
  if (return_types.empty()) {
    builder_->Emit(kExprDrop);
  } else {
    Convert(param_types[0], return_types[0]);
    Generate(return_types + 1, data);
  }
}

FunctionSig* GenerateSig(Zone* zone, DataRange* data) {
  // Generate enough parameters to spill some to the stack.
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  int num_returns = int{data->get<uint8_t>()} % (kMaxReturns + 1);

  FunctionSig::Builder builder(zone, num_returns, num_params);
  for (int i = 0; i < num_returns; ++i) builder.AddReturn(GetValueType(data));
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
    std::vector<uint32_t> function_signatures;
    function_signatures.push_back(builder.AddSignature(sigs.i_iii()));

    static_assert(kMaxFunctions >= 1, "need min. 1 function");
    int num_functions = 1 + (range.get<uint8_t>() % kMaxFunctions);

    for (int i = 1; i < num_functions; ++i) {
      FunctionSig* sig = GenerateSig(zone, &range);
      uint32_t signature_index = builder.AddSignature(sig);
      function_signatures.push_back(signature_index);
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

      FunctionSig* sig = builder.GetSignature(function_signatures[i]);
      WasmFunctionBuilder* f = builder.AddFunction(sig);

      WasmGenerator gen(f, function_signatures, globals, mutable_globals,
                        &function_range);
      Vector<const ValueType> return_types(sig->returns().begin(),
                                           sig->return_count());
      gen.Generate(return_types, &function_range);

      f->Emit(kExprEnd);
      if (i == 0) builder.AddExport(CStrVector("main"), f);
    }

    builder.AllocateIndirectFunctions(num_functions);
    for (int i = 0; i < num_functions; ++i) {
      builder.SetIndirectFunction(i, i);
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
  EXPERIMENTAL_FLAG_SCOPE(reftypes);
  EXPERIMENTAL_FLAG_SCOPE(return_call);
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
