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
#include "src/wasm/function-body-decoder.h"
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
constexpr int kMaxExceptions = 4;

class DataRange {
  Vector<const uint8_t> data_;

 public:
  explicit DataRange(Vector<const uint8_t> data) : data_(data) {}
  DataRange(const DataRange&) = delete;
  DataRange& operator=(const DataRange&) = delete;

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
  template <WasmOpcode Op, ValueKind... Args>
  void op(DataRange* data) {
    Generate<Args...>(data);
    builder_->Emit(Op);
  }

  class V8_NODISCARD BlockScope {
   public:
    BlockScope(WasmGenerator* gen, WasmOpcode block_type,
               Vector<const ValueType> param_types,
               Vector<const ValueType> result_types,
               Vector<const ValueType> br_types, bool emit_end = true)
        : gen_(gen), emit_end_(emit_end) {
      gen->blocks_.emplace_back(br_types.begin(), br_types.end());
      if (param_types.size() == 0 && result_types.size() == 0) {
        gen->builder_->EmitWithU8(block_type, kWasmVoid.value_type_code());
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
        DCHECK_NE(type, kWasmVoid);
        builder.AddParam(type);
      }
      for (auto& type : result_types) {
        DCHECK_NE(type, kWasmVoid);
        builder.AddReturn(type);
      }
      FunctionSig* sig = builder.Build();
      int sig_id = gen->builder_->builder()->AddSignature(sig);
      gen->builder_->EmitWithI32V(block_type, sig_id);
    }

    ~BlockScope() {
      if (emit_end_) gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
    bool emit_end_;
  };

  void block(Vector<const ValueType> param_types,
             Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprBlock, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
  void block(DataRange* data) {
    block({}, VectorOf({ValueType::Primitive(T)}), data);
  }

  void loop(Vector<const ValueType> param_types,
            Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprLoop, param_types, return_types,
                           param_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
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

  template <ValueKind T, IfType type>
  void if_(DataRange* data) {
    static_assert(T == kVoid || type == kIfElse,
                  "if without else cannot produce a value");
    if_({},
        T == kVoid ? Vector<ValueType>{} : VectorOf({ValueType::Primitive(T)}),
        type, data);
  }

  void try_block_helper(ValueType return_type, DataRange* data) {
    bool has_catch_all = data->get<uint8_t>() % 2;
    uint8_t num_catch =
        data->get<uint8_t>() % (builder_->builder()->NumExceptions() + 1);
    bool is_delegate =
        num_catch == 0 && !has_catch_all && data->get<uint8_t>() % 2 == 0;
    // Allow one more target than there are enclosing try blocks, for delegating
    // to the caller.
    uint8_t delegate_target = data->get<uint8_t>() % (try_blocks_.size() + 1);
    bool is_unwind = num_catch == 0 && !has_catch_all && !is_delegate;

    Vector<const ValueType> return_type_vec = return_type.kind() == kVoid
                                                  ? Vector<ValueType>{}
                                                  : VectorOf(&return_type, 1);
    BlockScope block_scope(this, kExprTry, {}, return_type_vec, return_type_vec,
                           !is_delegate);
    int control_depth = static_cast<int>(blocks_.size()) - 1;
    try_blocks_.push_back(control_depth);
    Generate(return_type, data);
    try_blocks_.pop_back();
    catch_blocks_.push_back(control_depth);
    for (int i = 0; i < num_catch; ++i) {
      const FunctionSig* exception_type =
          builder_->builder()->GetExceptionType(i);
      auto exception_type_vec = VectorOf(exception_type->parameters().begin(),
                                         exception_type->parameter_count());
      builder_->EmitWithU32V(kExprCatch, i);
      ConsumeAndGenerate(exception_type_vec, return_type_vec, data);
    }
    if (has_catch_all) {
      builder_->Emit(kExprCatchAll);
      Generate(return_type, data);
    }
    if (is_delegate) {
      DCHECK_GT(blocks_.size(), try_blocks_.size());
      // If {delegate_target == try_blocks_.size()}, delegate to the caller.
      int delegate_depth = delegate_target == try_blocks_.size()
                               ? static_cast<int>(blocks_.size()) - 2
                               : static_cast<int>(blocks_.size() - 2 -
                                                  try_blocks_[delegate_target]);
      builder_->EmitWithU32V(kExprDelegate, delegate_depth);
    }
    catch_blocks_.pop_back();
    if (is_unwind) {
      builder_->Emit(kExprUnwind);
      Generate(return_type, data);
    }
  }

  template <ValueKind T>
  void try_block(DataRange* data) {
    try_block_helper(ValueType::Primitive(T), data);
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

  template <ValueKind wanted_kind>
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
                       wanted_kind == kVoid
                           ? Vector<ValueType>{}
                           : VectorOf({ValueType::Primitive(wanted_kind)}),
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
      case kExprS128Load8x8S:
      case kExprS128Load8x8U:
      case kExprS128Load16x4S:
      case kExprS128Load16x4U:
      case kExprS128Load32x2S:
      case kExprS128Load32x2U:
      case kExprS128Load64Splat:
      case kExprS128Load64Zero:
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
      case kExprS128Load32Splat:
      case kExprS128Load32Zero:
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
      case kExprS128Load16Splat:
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
      case kExprS128Load8Splat:
        return 0;
      default:
        return 0;
    }
  }

  template <WasmOpcode memory_op, ValueKind... arg_kinds>
  void memop(DataRange* data) {
    const uint8_t align = data->get<uint8_t>() % (max_alignment(memory_op) + 1);
    const uint32_t offset = data->get<uint32_t>();

    // Generate the index and the arguments, if any.
    Generate<kI32, arg_kinds...>(data);

    if (WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(memory_op >> 8))) {
      DCHECK(memory_op >> 8 == kAtomicPrefix || memory_op >> 8 == kSimdPrefix);
      builder_->EmitWithPrefix(memory_op);
    } else {
      builder_->Emit(memory_op);
    }
    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueKind... Args>
  void atomic_op(DataRange* data) {
    const uint8_t align = data->get<uint8_t>() % (max_alignment(Op) + 1);
    const uint32_t offset = data->get<uint32_t>();

    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);

    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueKind... Args>
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

  template <WasmOpcode Op, int lanes, ValueKind... Args>
  void simd_lane_op(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
    builder_->EmitByte(data->get<byte>() % lanes);
  }

  template <WasmOpcode Op, int lanes, ValueKind... Args>
  void simd_lane_memop(DataRange* data) {
    // Simd load/store instructions that have a lane immediate.
    memop<Op, Args...>(data);
    builder_->EmitByte(data->get<byte>() % lanes);
  }

  void simd_shuffle(DataRange* data) {
    Generate<kS128, kS128>(data);
    builder_->EmitWithPrefix(kExprI8x16Shuffle);
    for (int i = 0; i < kSimd128Size; i++) {
      builder_->EmitByte(static_cast<uint8_t>(data->get<byte>() % 32));
    }
  }

  void drop(DataRange* data) {
    Generate(GetValueType(data), data);
    builder_->Emit(kExprDrop);
  }

  enum CallDirect : bool { kCallDirect = true, kCallIndirect = false };

  template <ValueKind wanted_kind>
  void call(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallDirect);
  }

  template <ValueKind wanted_kind>
  void call_indirect(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallIndirect);
  }

  void Convert(ValueType src, ValueType dst) {
    auto idx = [](ValueType t) -> int {
      switch (t.kind()) {
        case kI32:
          return 0;
        case kI64:
          return 1;
        case kF32:
          return 2;
        case kF64:
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
    if (src == kWasmVoid && dst != kWasmVoid) {
      Generate(dst, data);
    } else if (dst == kWasmVoid && src != kWasmVoid) {
      builder_->Emit(kExprDrop);
    } else {
      Convert(src, dst);
    }
  }

  void call(DataRange* data, ValueType wanted_kind, CallDirect call_direct) {
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
    if (sig->return_count() == 0 && wanted_kind != kWasmVoid) {
      // The call did not generate a value. Thus just generate it here.
      Generate(wanted_kind, data);
      return;
    }
    if (wanted_kind == kWasmVoid) {
      // The call did generate values, but we did not want one.
      for (size_t i = 0; i < sig->return_count(); ++i) {
        builder_->Emit(kExprDrop);
      }
      return;
    }
    auto return_types = VectorOf(sig->returns().begin(), sig->return_count());
    auto wanted_types =
        VectorOf(&wanted_kind, wanted_kind == kWasmVoid ? 0 : 1);
    ConsumeAndGenerate(return_types, wanted_types, data);
  }

  struct Var {
    uint32_t index;
    ValueType type = kWasmVoid;
    Var() = default;
    Var(uint32_t index, ValueType type) : index(index), type(type) {}
    bool is_valid() const { return type != kWasmVoid; }
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

  template <ValueKind wanted_kind>
  void local_op(DataRange* data, WasmOpcode opcode) {
    Var local = GetRandomLocal(data);
    // If there are no locals and no parameters, just generate any value (if a
    // value is needed), or do nothing.
    if (!local.is_valid()) {
      if (wanted_kind == kVoid) return;
      return Generate<wanted_kind>(data);
    }

    if (opcode != kExprLocalGet) Generate(local.type, data);
    builder_->EmitWithU32V(opcode, local.index);
    if (wanted_kind != kVoid && local.type.kind() != wanted_kind) {
      Convert(local.type, ValueType::Primitive(wanted_kind));
    }
  }

  template <ValueKind wanted_kind>
  void get_local(DataRange* data) {
    static_assert(wanted_kind != kVoid, "illegal type");
    local_op<wanted_kind>(data, kExprLocalGet);
  }

  void set_local(DataRange* data) { local_op<kVoid>(data, kExprLocalSet); }

  template <ValueKind wanted_kind>
  void tee_local(DataRange* data) {
    local_op<wanted_kind>(data, kExprLocalTee);
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

  template <ValueKind wanted_kind>
  void global_op(DataRange* data) {
    constexpr bool is_set = wanted_kind == kVoid;
    Var global = GetRandomGlobal(data, is_set);
    // If there are no globals, just generate any value (if a value is needed),
    // or do nothing.
    if (!global.is_valid()) {
      if (wanted_kind == kVoid) return;
      return Generate<wanted_kind>(data);
    }

    if (is_set) Generate(global.type, data);
    builder_->EmitWithU32V(is_set ? kExprGlobalSet : kExprGlobalGet,
                           global.index);
    if (!is_set && global.type.kind() != wanted_kind) {
      Convert(global.type, ValueType::Primitive(wanted_kind));
    }
  }

  template <ValueKind wanted_kind>
  void get_global(DataRange* data) {
    static_assert(wanted_kind != kVoid, "illegal type");
    global_op<wanted_kind>(data);
  }

  template <ValueKind select_kind>
  void select_with_type(DataRange* data) {
    static_assert(select_kind != kVoid, "illegal kind for select");
    Generate<select_kind, select_kind, kI32>(data);
    // num_types is always 1.
    uint8_t num_types = 1;
    builder_->EmitWithU8U8(kExprSelectWithType, num_types,
                           ValueType::Primitive(select_kind).value_type_code());
  }

  void set_global(DataRange* data) { global_op<kVoid>(data); }

  void throw_or_rethrow(DataRange* data) {
    bool rethrow = data->get<uint8_t>() % 2;
    if (rethrow && !catch_blocks_.empty()) {
      int control_depth = static_cast<int>(blocks_.size() - 1);
      int catch_index =
          data->get<uint8_t>() % static_cast<int>(catch_blocks_.size());
      builder_->EmitWithU32V(kExprRethrow,
                             control_depth - catch_blocks_[catch_index]);
    } else {
      int tag = data->get<uint8_t>() % builder_->builder()->NumExceptions();
      FunctionSig* exception_sig = builder_->builder()->GetExceptionType(tag);
      Vector<const ValueType> exception_types(
          exception_sig->parameters().begin(),
          exception_sig->parameter_count());
      Generate(exception_types, data);
      builder_->EmitWithU32V(kExprThrow, tag);
    }
  }

  template <ValueKind... Types>
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

  template <ValueKind T>
  void Generate(DataRange* data);

  template <ValueKind T1, ValueKind T2, ValueKind... Ts>
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
  bool HasSimd() { return has_simd_; }

 private:
  WasmFunctionBuilder* builder_;
  std::vector<std::vector<ValueType>> blocks_;
  const std::vector<uint32_t>& functions_;
  std::vector<ValueType> locals_;
  std::vector<ValueType> globals_;
  std::vector<uint8_t> mutable_globals_;  // indexes into {globals_}.
  uint32_t recursion_depth = 0;
  std::vector<int> try_blocks_;
  std::vector<int> catch_blocks_;
  bool has_simd_;

  static constexpr uint32_t kMaxRecursionDepth = 64;

  bool recursion_limit_reached() {
    return recursion_depth >= kMaxRecursionDepth;
  }
};

template <>
void WasmGenerator::block<kVoid>(DataRange* data) {
  block({}, {}, data);
}

template <>
void WasmGenerator::loop<kVoid>(DataRange* data) {
  loop({}, {}, data);
}

template <>
void WasmGenerator::Generate<kVoid>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() == 0) return;

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kVoid, kVoid>,
      &WasmGenerator::sequence<kVoid, kVoid, kVoid, kVoid>,
      &WasmGenerator::sequence<kVoid, kVoid, kVoid, kVoid, kVoid, kVoid, kVoid,
                               kVoid>,
      &WasmGenerator::block<kVoid>,
      &WasmGenerator::loop<kVoid>,
      &WasmGenerator::if_<kVoid, kIf>,
      &WasmGenerator::if_<kVoid, kIfElse>,
      &WasmGenerator::br,
      &WasmGenerator::br_if<kVoid>,

      &WasmGenerator::memop<kExprI32StoreMem, kI32>,
      &WasmGenerator::memop<kExprI32StoreMem8, kI32>,
      &WasmGenerator::memop<kExprI32StoreMem16, kI32>,
      &WasmGenerator::memop<kExprI64StoreMem, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem8, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem16, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem32, kI64>,
      &WasmGenerator::memop<kExprF32StoreMem, kF32>,
      &WasmGenerator::memop<kExprF64StoreMem, kF64>,
      &WasmGenerator::memop<kExprI32AtomicStore, kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore8U, kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore16U, kI32>,
      &WasmGenerator::memop<kExprI64AtomicStore, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore8U, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore16U, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore32U, kI64>,
      &WasmGenerator::memop<kExprS128StoreMem, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store8Lane, 16, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store16Lane, 8, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store32Lane, 4, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store64Lane, 2, kS128>,

      &WasmGenerator::drop,

      &WasmGenerator::call<kVoid>,
      &WasmGenerator::call_indirect<kVoid>,

      &WasmGenerator::set_local,
      &WasmGenerator::set_global,
      &WasmGenerator::throw_or_rethrow,
      &WasmGenerator::try_block<kVoid>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kI32>(DataRange* data) {
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

      &WasmGenerator::sequence<kI32, kVoid>,
      &WasmGenerator::sequence<kVoid, kI32>,
      &WasmGenerator::sequence<kVoid, kI32, kVoid>,

      &WasmGenerator::op<kExprI32Eqz, kI32>,
      &WasmGenerator::op<kExprI32Eq, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ne, kI32, kI32>,
      &WasmGenerator::op<kExprI32LtS, kI32, kI32>,
      &WasmGenerator::op<kExprI32LtU, kI32, kI32>,
      &WasmGenerator::op<kExprI32GeS, kI32, kI32>,
      &WasmGenerator::op<kExprI32GeU, kI32, kI32>,

      &WasmGenerator::op<kExprI64Eqz, kI64>,
      &WasmGenerator::op<kExprI64Eq, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ne, kI64, kI64>,
      &WasmGenerator::op<kExprI64LtS, kI64, kI64>,
      &WasmGenerator::op<kExprI64LtU, kI64, kI64>,
      &WasmGenerator::op<kExprI64GeS, kI64, kI64>,
      &WasmGenerator::op<kExprI64GeU, kI64, kI64>,

      &WasmGenerator::op<kExprF32Eq, kF32, kF32>,
      &WasmGenerator::op<kExprF32Ne, kF32, kF32>,
      &WasmGenerator::op<kExprF32Lt, kF32, kF32>,
      &WasmGenerator::op<kExprF32Ge, kF32, kF32>,

      &WasmGenerator::op<kExprF64Eq, kF64, kF64>,
      &WasmGenerator::op<kExprF64Ne, kF64, kF64>,
      &WasmGenerator::op<kExprF64Lt, kF64, kF64>,
      &WasmGenerator::op<kExprF64Ge, kF64, kF64>,

      &WasmGenerator::op<kExprI32Add, kI32, kI32>,
      &WasmGenerator::op<kExprI32Sub, kI32, kI32>,
      &WasmGenerator::op<kExprI32Mul, kI32, kI32>,

      &WasmGenerator::op<kExprI32DivS, kI32, kI32>,
      &WasmGenerator::op<kExprI32DivU, kI32, kI32>,
      &WasmGenerator::op<kExprI32RemS, kI32, kI32>,
      &WasmGenerator::op<kExprI32RemU, kI32, kI32>,

      &WasmGenerator::op<kExprI32And, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ior, kI32, kI32>,
      &WasmGenerator::op<kExprI32Xor, kI32, kI32>,
      &WasmGenerator::op<kExprI32Shl, kI32, kI32>,
      &WasmGenerator::op<kExprI32ShrU, kI32, kI32>,
      &WasmGenerator::op<kExprI32ShrS, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ror, kI32, kI32>,
      &WasmGenerator::op<kExprI32Rol, kI32, kI32>,

      &WasmGenerator::op<kExprI32Clz, kI32>,
      &WasmGenerator::op<kExprI32Ctz, kI32>,
      &WasmGenerator::op<kExprI32Popcnt, kI32>,

      &WasmGenerator::op<kExprI32ConvertI64, kI64>,
      &WasmGenerator::op<kExprI32SConvertF32, kF32>,
      &WasmGenerator::op<kExprI32UConvertF32, kF32>,
      &WasmGenerator::op<kExprI32SConvertF64, kF64>,
      &WasmGenerator::op<kExprI32UConvertF64, kF64>,
      &WasmGenerator::op<kExprI32ReinterpretF32, kF32>,

      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF64, kF64>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF64, kF64>,

      &WasmGenerator::block<kI32>,
      &WasmGenerator::loop<kI32>,
      &WasmGenerator::if_<kI32, kIfElse>,
      &WasmGenerator::br_if<kI32>,

      &WasmGenerator::memop<kExprI32LoadMem>,
      &WasmGenerator::memop<kExprI32LoadMem8S>,
      &WasmGenerator::memop<kExprI32LoadMem8U>,
      &WasmGenerator::memop<kExprI32LoadMem16S>,
      &WasmGenerator::memop<kExprI32LoadMem16U>,
      &WasmGenerator::memop<kExprI32AtomicLoad>,
      &WasmGenerator::memop<kExprI32AtomicLoad8U>,
      &WasmGenerator::memop<kExprI32AtomicLoad16U>,

      &WasmGenerator::atomic_op<kExprI32AtomicAdd, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange, kI32, kI32,
                                kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange8U, kI32, kI32,
                                kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange16U, kI32, kI32,
                                kI32>,

      &WasmGenerator::op_with_prefix<kExprV128AnyTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2BitMask, kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneS, 16, kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneU, 16, kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneS, 8, kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneU, 8, kS128>,
      &WasmGenerator::simd_lane_op<kExprI32x4ExtractLane, 4, kS128>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory,

      &WasmGenerator::get_local<kI32>,
      &WasmGenerator::tee_local<kI32>,
      &WasmGenerator::get_global<kI32>,
      &WasmGenerator::op<kExprSelect, kI32, kI32, kI32>,
      &WasmGenerator::select_with_type<kI32>,

      &WasmGenerator::call<kI32>,
      &WasmGenerator::call_indirect<kI32>,
      &WasmGenerator::try_block<kI32>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kI64>(DataRange* data) {
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

      &WasmGenerator::sequence<kI64, kVoid>,
      &WasmGenerator::sequence<kVoid, kI64>,
      &WasmGenerator::sequence<kVoid, kI64, kVoid>,

      &WasmGenerator::op<kExprI64Add, kI64, kI64>,
      &WasmGenerator::op<kExprI64Sub, kI64, kI64>,
      &WasmGenerator::op<kExprI64Mul, kI64, kI64>,

      &WasmGenerator::op<kExprI64DivS, kI64, kI64>,
      &WasmGenerator::op<kExprI64DivU, kI64, kI64>,
      &WasmGenerator::op<kExprI64RemS, kI64, kI64>,
      &WasmGenerator::op<kExprI64RemU, kI64, kI64>,

      &WasmGenerator::op<kExprI64And, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ior, kI64, kI64>,
      &WasmGenerator::op<kExprI64Xor, kI64, kI64>,
      &WasmGenerator::op<kExprI64Shl, kI64, kI64>,
      &WasmGenerator::op<kExprI64ShrU, kI64, kI64>,
      &WasmGenerator::op<kExprI64ShrS, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ror, kI64, kI64>,
      &WasmGenerator::op<kExprI64Rol, kI64, kI64>,

      &WasmGenerator::op<kExprI64Clz, kI64>,
      &WasmGenerator::op<kExprI64Ctz, kI64>,
      &WasmGenerator::op<kExprI64Popcnt, kI64>,

      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF64, kF64>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF64, kF64>,

      &WasmGenerator::block<kI64>,
      &WasmGenerator::loop<kI64>,
      &WasmGenerator::if_<kI64, kIfElse>,
      &WasmGenerator::br_if<kI64>,

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

      &WasmGenerator::atomic_op<kExprI64AtomicAdd, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange8U, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange16U, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange32U, kI32, kI64,
                                kI64>,

      &WasmGenerator::simd_lane_op<kExprI64x2ExtractLane, 2, kS128>,

      &WasmGenerator::get_local<kI64>,
      &WasmGenerator::tee_local<kI64>,
      &WasmGenerator::get_global<kI64>,
      &WasmGenerator::op<kExprSelect, kI64, kI64, kI32>,
      &WasmGenerator::select_with_type<kI64>,

      &WasmGenerator::call<kI64>,
      &WasmGenerator::call_indirect<kI64>,
      &WasmGenerator::try_block<kI64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kF32>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(float)) {
    builder_->EmitF32Const(data->get<float>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kF32, kVoid>,
      &WasmGenerator::sequence<kVoid, kF32>,
      &WasmGenerator::sequence<kVoid, kF32, kVoid>,

      &WasmGenerator::op<kExprF32Abs, kF32>,
      &WasmGenerator::op<kExprF32Neg, kF32>,
      &WasmGenerator::op<kExprF32Ceil, kF32>,
      &WasmGenerator::op<kExprF32Floor, kF32>,
      &WasmGenerator::op<kExprF32Trunc, kF32>,
      &WasmGenerator::op<kExprF32NearestInt, kF32>,
      &WasmGenerator::op<kExprF32Sqrt, kF32>,
      &WasmGenerator::op<kExprF32Add, kF32, kF32>,
      &WasmGenerator::op<kExprF32Sub, kF32, kF32>,
      &WasmGenerator::op<kExprF32Mul, kF32, kF32>,
      &WasmGenerator::op<kExprF32Div, kF32, kF32>,
      &WasmGenerator::op<kExprF32Min, kF32, kF32>,
      &WasmGenerator::op<kExprF32Max, kF32, kF32>,
      &WasmGenerator::op<kExprF32CopySign, kF32, kF32>,

      &WasmGenerator::op<kExprF32SConvertI32, kI32>,
      &WasmGenerator::op<kExprF32UConvertI32, kI32>,
      &WasmGenerator::op<kExprF32SConvertI64, kI64>,
      &WasmGenerator::op<kExprF32UConvertI64, kI64>,
      &WasmGenerator::op<kExprF32ConvertF64, kF64>,
      &WasmGenerator::op<kExprF32ReinterpretI32, kI32>,

      &WasmGenerator::block<kF32>,
      &WasmGenerator::loop<kF32>,
      &WasmGenerator::if_<kF32, kIfElse>,
      &WasmGenerator::br_if<kF32>,

      &WasmGenerator::memop<kExprF32LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF32x4ExtractLane, 4, kS128>,

      &WasmGenerator::get_local<kF32>,
      &WasmGenerator::tee_local<kF32>,
      &WasmGenerator::get_global<kF32>,
      &WasmGenerator::op<kExprSelect, kF32, kF32, kI32>,
      &WasmGenerator::select_with_type<kF32>,

      &WasmGenerator::call<kF32>,
      &WasmGenerator::call_indirect<kF32>,
      &WasmGenerator::try_block<kF32>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kF64>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(double)) {
    builder_->EmitF64Const(data->get<double>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kF64, kVoid>,
      &WasmGenerator::sequence<kVoid, kF64>,
      &WasmGenerator::sequence<kVoid, kF64, kVoid>,

      &WasmGenerator::op<kExprF64Abs, kF64>,
      &WasmGenerator::op<kExprF64Neg, kF64>,
      &WasmGenerator::op<kExprF64Ceil, kF64>,
      &WasmGenerator::op<kExprF64Floor, kF64>,
      &WasmGenerator::op<kExprF64Trunc, kF64>,
      &WasmGenerator::op<kExprF64NearestInt, kF64>,
      &WasmGenerator::op<kExprF64Sqrt, kF64>,
      &WasmGenerator::op<kExprF64Add, kF64, kF64>,
      &WasmGenerator::op<kExprF64Sub, kF64, kF64>,
      &WasmGenerator::op<kExprF64Mul, kF64, kF64>,
      &WasmGenerator::op<kExprF64Div, kF64, kF64>,
      &WasmGenerator::op<kExprF64Min, kF64, kF64>,
      &WasmGenerator::op<kExprF64Max, kF64, kF64>,
      &WasmGenerator::op<kExprF64CopySign, kF64, kF64>,

      &WasmGenerator::op<kExprF64SConvertI32, kI32>,
      &WasmGenerator::op<kExprF64UConvertI32, kI32>,
      &WasmGenerator::op<kExprF64SConvertI64, kI64>,
      &WasmGenerator::op<kExprF64UConvertI64, kI64>,
      &WasmGenerator::op<kExprF64ConvertF32, kF32>,
      &WasmGenerator::op<kExprF64ReinterpretI64, kI64>,

      &WasmGenerator::block<kF64>,
      &WasmGenerator::loop<kF64>,
      &WasmGenerator::if_<kF64, kIfElse>,
      &WasmGenerator::br_if<kF64>,

      &WasmGenerator::memop<kExprF64LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF64x2ExtractLane, 2, kS128>,

      &WasmGenerator::get_local<kF64>,
      &WasmGenerator::tee_local<kF64>,
      &WasmGenerator::get_global<kF64>,
      &WasmGenerator::op<kExprSelect, kF64, kF64, kI32>,
      &WasmGenerator::select_with_type<kF64>,

      &WasmGenerator::call<kF64>,
      &WasmGenerator::call_indirect<kF64>,
      &WasmGenerator::try_block<kF64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kS128>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  has_simd_ = true;
  if (recursion_limit_reached() || data->size() <= sizeof(int32_t)) {
    // TODO(v8:8460): v128.const is not implemented yet, and we need a way to
    // "bottom-out", so use a splat to generate this.
    builder_->EmitI32Const(data->get<int32_t>());
    builder_->EmitWithPrefix(kExprI8x16Splat);
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::simd_const,
      &WasmGenerator::simd_lane_op<kExprI8x16ReplaceLane, 16, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI16x8ReplaceLane, 8, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI32x4ReplaceLane, 4, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI64x2ReplaceLane, 2, kS128, kI64>,
      &WasmGenerator::simd_lane_op<kExprF32x4ReplaceLane, 4, kS128, kF32>,
      &WasmGenerator::simd_lane_op<kExprF64x2ReplaceLane, 2, kS128, kF64>,

      &WasmGenerator::op_with_prefix<kExprI8x16Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16RoundingAverageU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Popcnt, kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8RoundingAverageU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulLowI8x16S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulLowI8x16U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulHighI8x16S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulHighI8x16U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Q15MulRSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtAddPairwiseI8x16S, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtAddPairwiseI8x16U, kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4DotI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulLowI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulLowI16x8U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulHighI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulHighI16x8U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtAddPairwiseI16x8S, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtAddPairwiseI16x8U, kS128>,

      &WasmGenerator::op_with_prefix<kExprI64x2Splat, kI64>,
      &WasmGenerator::op_with_prefix<kExprI64x2Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulLowI32x4S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulLowI32x4U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulHighI32x4S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulHighI32x4U, kS128, kS128>,

      &WasmGenerator::op_with_prefix<kExprF32x4Splat, kF32>,
      &WasmGenerator::op_with_prefix<kExprF32x4Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Lt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Gt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Le, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ge, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sqrt, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Div, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Min, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Max, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Pmin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Pmax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ceil, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Floor, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Trunc, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4NearestInt, kS128>,

      &WasmGenerator::op_with_prefix<kExprF64x2Splat, kF64>,
      &WasmGenerator::op_with_prefix<kExprF64x2Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Lt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Gt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Le, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ge, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sqrt, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Div, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Min, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Max, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Pmin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Pmax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ceil, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Floor, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Trunc, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2NearestInt, kS128>,

      &WasmGenerator::op_with_prefix<kExprF64x2PromoteLowF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2ConvertLowI32x4S, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2ConvertLowI32x4U, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4DemoteF64x2Zero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4TruncSatF64x2SZero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4TruncSatF64x2UZero, kS128>,

      &WasmGenerator::op_with_prefix<kExprI64x2SConvertI32x4Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2SConvertI32x4High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2UConvertI32x4Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2UConvertI32x4High, kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4SConvertF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4SConvertI32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4UConvertI32x4, kS128>,

      &WasmGenerator::op_with_prefix<kExprI8x16SConvertI16x8, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16UConvertI16x8, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI32x4, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI32x4, kS128, kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8High, kS128>,

      &WasmGenerator::op_with_prefix<kExprS128Not, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128And, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128AndNot, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Or, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Xor, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Select, kS128, kS128, kS128>,

      &WasmGenerator::simd_shuffle,
      &WasmGenerator::op_with_prefix<kExprI8x16Swizzle, kS128, kS128>,

      &WasmGenerator::memop<kExprS128LoadMem>,
      &WasmGenerator::memop<kExprS128Load8x8S>,
      &WasmGenerator::memop<kExprS128Load8x8U>,
      &WasmGenerator::memop<kExprS128Load16x4S>,
      &WasmGenerator::memop<kExprS128Load16x4U>,
      &WasmGenerator::memop<kExprS128Load32x2S>,
      &WasmGenerator::memop<kExprS128Load32x2U>,
      &WasmGenerator::memop<kExprS128Load8Splat>,
      &WasmGenerator::memop<kExprS128Load16Splat>,
      &WasmGenerator::memop<kExprS128Load32Splat>,
      &WasmGenerator::memop<kExprS128Load64Splat>,
      &WasmGenerator::memop<kExprS128Load32Zero>,
      &WasmGenerator::memop<kExprS128Load64Zero>,
      &WasmGenerator::simd_lane_memop<kExprS128Load8Lane, 16, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load16Lane, 8, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load32Lane, 4, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load64Lane, 2, kS128>,
  };

  GenerateOneOf(alternatives, data);
}

void WasmGenerator::grow_memory(DataRange* data) {
  Generate<kI32>(data);
  builder_->EmitWithU8(kExprMemoryGrow, 0);
}

void WasmGenerator::Generate(ValueType type, DataRange* data) {
  switch (type.kind()) {
    case kVoid:
      return Generate<kVoid>(data);
    case kI32:
      return Generate<kI32>(data);
    case kI64:
      return Generate<kI64>(data);
    case kF32:
      return Generate<kF32>(data);
    case kF64:
      return Generate<kF64>(data);
    case kS128:
      return Generate<kS128>(data);
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
    Generate(kWasmVoid, data);
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

enum SigKind { kFunctionSig, kExceptionSig };

FunctionSig* GenerateSig(Zone* zone, DataRange* data, SigKind sig_kind) {
  // Generate enough parameters to spill some to the stack.
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  int num_returns = sig_kind == kFunctionSig
                        ? int{data->get<uint8_t>()} % (kMaxReturns + 1)
                        : 0;

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
      FunctionSig* sig = GenerateSig(zone, &range, kFunctionSig);
      uint32_t signature_index = builder.AddSignature(sig);
      function_signatures.push_back(signature_index);
    }

    int num_globals = range.get<uint8_t>() % (kMaxGlobals + 1);
    std::vector<ValueType> globals;
    std::vector<uint8_t> mutable_globals;
    globals.reserve(num_globals);
    mutable_globals.reserve(num_globals);

    int num_exceptions = 1 + (range.get<uint8_t>() % kMaxExceptions);
    for (int i = 0; i < num_exceptions; ++i) {
      FunctionSig* sig = GenerateSig(zone, &range, kExceptionSig);
      builder.AddException(sig);
    }

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
      if (!CheckHardwareSupportsSimd() && gen.HasSimd()) return false;
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
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
