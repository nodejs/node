// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "src/base/macros.h"
#include "src/base/v8-fallthrough.h"
#include "src/execution/isolate.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

namespace {

constexpr int kMaxArrays = 4;
constexpr int kMaxStructs = 4;
constexpr int kMaxStructFields = 4;
constexpr int kMaxFunctions = 4;
constexpr int kMaxGlobals = 64;
constexpr int kMaxParameters = 15;
constexpr int kMaxReturns = 15;
constexpr int kMaxExceptions = 4;
constexpr int kMaxTableSize = 32;
constexpr int kMaxTables = 4;
constexpr int kMaxArraySize = 20;

class DataRange {
  base::Vector<const uint8_t> data_;

 public:
  explicit DataRange(base::Vector<const uint8_t> data) : data_(data) {}
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
    // Bool needs special handling (see template specialization below).
    static_assert(!std::is_same<T, bool>::value, "bool needs special handling");
    static_assert(max_bytes <= sizeof(T));
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

// Explicit specialization must be defined outside of class body.
template <>
bool DataRange::get() {
  // The general implementation above is not instantiable for bool, as that
  // would cause undefinied behaviour when memcpy'ing random bytes to the
  // bool. This can result in different observable side effects when invoking
  // get<bool> between debug and release version, which eventually makes the
  // code output different as well as raising various unrecoverable errors on
  // runtime.
  // Hence we specialize get<bool> to consume a full byte and use the least
  // significant bit only (0 == false, 1 == true).
  return get<uint8_t>() % 2;
}

enum NonNullables { kAllowNonNullables, kDisallowNonNullables };
enum PackedTypes { kIncludePackedTypes, kExcludePackedTypes };
enum Generics { kIncludeGenerics, kExcludeGenerics };

ValueType GetValueTypeHelper(DataRange* data, bool liftoff_as_reference,
                             uint32_t num_nullable_types,
                             uint32_t num_non_nullable_types,
                             NonNullables allow_non_nullable,
                             PackedTypes include_packed_types,
                             Generics include_generics) {
  // Non wasm-gc types.
  std::vector<ValueType> types{kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                               kWasmS128};

  if (!liftoff_as_reference) {
    return types[data->get<uint8_t>() % types.size()];
  }

  // If {liftoff_as_reference}, include wasm-gc types.
  if (include_packed_types == kIncludePackedTypes) {
    types.insert(types.end(), {kWasmI8, kWasmI16});
  }
  // Decide if the return type will be nullable or not.
  const bool nullable =
      (allow_non_nullable == kAllowNonNullables) ? data->get<bool>() : true;
  if (nullable) {
    types.insert(types.end(),
                 {kWasmI31Ref, kWasmFuncRef, kWasmExternRef, kWasmNullRef,
                  kWasmNullExternRef, kWasmNullFuncRef});
  }
  if (include_generics == kIncludeGenerics) {
    types.insert(types.end(), {kWasmDataRef, kWasmAnyRef, kWasmEqRef});
  }

  // The last index of user-defined types allowed is different based on the
  // nullability of the output.
  const uint32_t num_user_defined_types =
      nullable ? num_nullable_types : num_non_nullable_types;

  // Conceptually, user-defined types are added to the end of the list. Pick a
  // random one among them.
  uint32_t id = data->get<uint8_t>() % (types.size() + num_user_defined_types);

  Nullability nullability = nullable ? kNullable : kNonNullable;

  if (id >= types.size()) {
    // Return user-defined type.
    return ValueType::RefMaybeNull(id - static_cast<uint32_t>(types.size()),
                                   nullability);
  }
  // If returning a reference type, fix its nullability according to {nullable}.
  if (types[id].is_reference()) {
    return ValueType::RefMaybeNull(types[id].heap_type(), nullability);
  }
  // Otherwise, just return the picked type.
  return types[id];
}

ValueType GetValueType(DataRange* data, bool liftoff_as_reference,
                       uint32_t num_types) {
  return GetValueTypeHelper(data, liftoff_as_reference, num_types, num_types,
                            kAllowNonNullables, kExcludePackedTypes,
                            kIncludeGenerics);
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
               base::Vector<const ValueType> param_types,
               base::Vector<const ValueType> result_types,
               base::Vector<const ValueType> br_types, bool emit_end = true)
        : gen_(gen), emit_end_(emit_end) {
      gen->blocks_.emplace_back(br_types.begin(), br_types.end());
      gen->builder_->EmitByte(block_type);

      if (param_types.size() == 0 && result_types.size() == 0) {
        gen->builder_->EmitValueType(kWasmVoid);
        return;
      }
      if (param_types.size() == 0 && result_types.size() == 1) {
        gen->builder_->EmitValueType(result_types[0]);
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
      gen->builder_->EmitI32V(sig_id);
    }

    ~BlockScope() {
      if (emit_end_) gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
    bool emit_end_;
  };

  void block(base::Vector<const ValueType> param_types,
             base::Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprBlock, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
  void block(DataRange* data) {
    block({}, base::VectorOf({ValueType::Primitive(T)}), data);
  }

  void loop(base::Vector<const ValueType> param_types,
            base::Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprLoop, param_types, return_types,
                           param_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
  void loop(DataRange* data) {
    loop({}, base::VectorOf({ValueType::Primitive(T)}), data);
  }

  enum IfType { kIf, kIfElse };

  void if_(base::Vector<const ValueType> param_types,
           base::Vector<const ValueType> return_types, IfType type,
           DataRange* data) {
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
        T == kVoid ? base::Vector<ValueType>{}
                   : base::VectorOf({ValueType::Primitive(T)}),
        type, data);
  }

  void try_block_helper(ValueType return_type, DataRange* data) {
    bool has_catch_all = data->get<bool>();
    uint8_t num_catch =
        data->get<uint8_t>() % (builder_->builder()->NumExceptions() + 1);
    bool is_delegate = num_catch == 0 && !has_catch_all && data->get<bool>();
    // Allow one more target than there are enclosing try blocks, for delegating
    // to the caller.

    base::Vector<const ValueType> return_type_vec =
        return_type.kind() == kVoid ? base::Vector<ValueType>{}
                                    : base::VectorOf(&return_type, 1);
    BlockScope block_scope(this, kExprTry, {}, return_type_vec, return_type_vec,
                           !is_delegate);
    int control_depth = static_cast<int>(blocks_.size()) - 1;
    Generate(return_type, data);
    catch_blocks_.push_back(control_depth);
    for (int i = 0; i < num_catch; ++i) {
      const FunctionSig* exception_type =
          builder_->builder()->GetExceptionType(i);
      auto exception_type_vec =
          base::VectorOf(exception_type->parameters().begin(),
                         exception_type->parameter_count());
      builder_->EmitWithU32V(kExprCatch, i);
      ConsumeAndGenerate(exception_type_vec, return_type_vec, data);
    }
    if (has_catch_all) {
      builder_->Emit(kExprCatchAll);
      Generate(return_type, data);
    }
    if (is_delegate) {
      // The delegate target depth does not include the current try block,
      // because 'delegate' closes this scope. However it is still in the
      // {blocks_} list, so remove one to get the correct size.
      int delegate_depth = data->get<uint8_t>() % (blocks_.size() - 1);
      builder_->EmitWithU32V(kExprDelegate, delegate_depth);
    }
    catch_blocks_.pop_back();
  }

  template <ValueKind T>
  void try_block(DataRange* data) {
    try_block_helper(ValueType::Primitive(T), data);
  }

  void any_block(base::Vector<const ValueType> param_types,
                 base::Vector<const ValueType> return_types, DataRange* data) {
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

    Generate(base::VectorOf(break_types), data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
  }

  template <ValueKind wanted_kind>
  void br_if(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const auto break_types = base::VectorOf(blocks_[target_block]);

    Generate(break_types, data);
    Generate(kWasmI32, data);
    builder_->EmitWithI32V(
        kExprBrIf, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConsumeAndGenerate(
        break_types,
        wanted_kind == kVoid
            ? base::Vector<ValueType>{}
            : base::VectorOf({ValueType::Primitive(wanted_kind)}),
        data);
  }

  template <ValueKind wanted_kind>
  void br_on_null(DataRange* data) {
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint32_t>() % blocks_.size();
    const auto break_types = base::VectorOf(blocks_[target_block]);
    if (!liftoff_as_reference_) {
      Generate<wanted_kind>(data);
      return;
    }
    Generate(break_types, data);
    GenerateRef(HeapType(HeapType::kAny), data);
    builder_->EmitWithI32V(
        kExprBrOnNull,
        static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    builder_->Emit(kExprDrop);
    ConsumeAndGenerate(
        break_types,
        wanted_kind == kVoid
            ? base::Vector<ValueType>{}
            : base::VectorOf({ValueType::Primitive(wanted_kind)}),
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
    Generate(GetValueType(data, liftoff_as_reference_,
                          static_cast<uint32_t>(functions_.size()) +
                              num_structs_ + num_arrays_),
             data);
    builder_->Emit(kExprDrop);
  }

  enum CallKind { kCallDirect, kCallIndirect, kCallRef };

  template <ValueKind wanted_kind>
  void call(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallDirect);
  }

  template <ValueKind wanted_kind>
  void call_indirect(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallIndirect);
  }

  template <ValueKind wanted_kind>
  void call_ref(DataRange* data) {
    if (liftoff_as_reference_) {
      call(data, ValueType::Primitive(wanted_kind), kCallRef);
    } else {
      Generate<wanted_kind>(data);
    }
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

  void call(DataRange* data, ValueType wanted_kind, CallKind call_kind) {
    uint8_t random_byte = data->get<uint8_t>();
    int func_index = random_byte % functions_.size();
    uint32_t sig_index = functions_[func_index];
    const FunctionSig* sig = builder_->builder()->GetSignature(sig_index);
    // Generate arguments.
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      Generate(sig->GetParam(i), data);
    }
    // Emit call.
    // If the return types of the callee happen to match the return types of the
    // caller, generate a tail call.
    // TODO(thibaudm): Re-enable when crbug.com/1269989 is fixed.
    bool use_return_call = false;
    if (use_return_call &&
        std::equal(sig->returns().begin(), sig->returns().end(),
                   builder_->signature()->returns().begin(),
                   builder_->signature()->returns().end())) {
      if (call_kind == kCallDirect) {
        builder_->EmitWithU32V(kExprReturnCall, func_index);
      } else if (call_kind == kCallIndirect) {
        // This will not trap because table[func_index] always contains function
        // func_index.
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprReturnCallIndirect, sig_index);
        // TODO(11954): Use other table indices too.
        builder_->EmitByte(0);  // Table index.
      } else {
        GenerateRef(HeapType(sig_index), data);
        builder_->EmitWithU32V(kExprReturnCallRef, sig_index);
      }
      return;
    } else {
      if (call_kind == kCallDirect) {
        builder_->EmitWithU32V(kExprCallFunction, func_index);
      } else if (call_kind == kCallIndirect) {
        // This will not trap because table[func_index] always contains function
        // func_index.
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprCallIndirect, sig_index);
        // TODO(11954): Use other table indices too.
        builder_->EmitByte(0);  // Table index.
      } else {
        GenerateRef(HeapType(sig_index), data);
        builder_->EmitWithU32V(kExprCallRef, sig_index);
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
    auto return_types =
        base::VectorOf(sig->returns().begin(), sig->return_count());
    auto wanted_types =
        base::VectorOf(&wanted_kind, wanted_kind == kWasmVoid ? 0 : 1);
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

  constexpr static bool is_convertible_kind(ValueKind kind) {
    return kind == kI32 || kind == kI64 || kind == kF32 || kind == kF64;
  }

  template <ValueKind wanted_kind>
  void local_op(DataRange* data, WasmOpcode opcode) {
    static_assert(wanted_kind == kVoid || is_convertible_kind(wanted_kind));
    Var local = GetRandomLocal(data);
    // If there are no locals and no parameters, just generate any value (if a
    // value is needed), or do nothing.
    if (!local.is_valid() || !is_convertible_kind(local.type.kind())) {
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
    static_assert(wanted_kind == kVoid || is_convertible_kind(wanted_kind));
    constexpr bool is_set = wanted_kind == kVoid;
    Var global = GetRandomGlobal(data, is_set);
    // If there are no globals, just generate any value (if a value is needed),
    // or do nothing.
    if (!global.is_valid() || !is_convertible_kind(global.type.kind())) {
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
    bool rethrow = data->get<bool>();
    if (rethrow && !catch_blocks_.empty()) {
      int control_depth = static_cast<int>(blocks_.size() - 1);
      int catch_index =
          data->get<uint8_t>() % static_cast<int>(catch_blocks_.size());
      builder_->EmitWithU32V(kExprRethrow,
                             control_depth - catch_blocks_[catch_index]);
    } else {
      int tag = data->get<uint8_t>() % builder_->builder()->NumExceptions();
      const FunctionSig* exception_sig =
          builder_->builder()->GetExceptionType(tag);
      base::Vector<const ValueType> exception_types(
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

  void ref_null(HeapType type, DataRange* data) {
    builder_->EmitWithI32V(kExprRefNull, type.code());
  }

  bool get_local_ref(HeapType type, DataRange* data, Nullability nullable) {
    Var local = GetRandomLocal(data);
    // TODO(manoskouk): Ideally we would check for subtyping here over type
    // equality, but we don't have a module.
    // TODO(7748): Allow initialized non-nullable locals.
    if (nullable == kNullable && local.is_valid() &&
        local.type.is_object_reference() && type == local.type.heap_type()) {
      builder_->EmitWithU32V(kExprLocalGet, local.index);
      return true;
    }

    return false;
  }

  bool new_object(HeapType type, DataRange* data, Nullability nullable) {
    DCHECK(liftoff_as_reference_ && type.is_index());

    uint32_t index = type.ref_index();
    bool new_default = data->get<bool>();

    if (builder_->builder()->IsStructType(index)) {
      const StructType* struct_gen = builder_->builder()->GetStructType(index);
      int field_count = struct_gen->field_count();
      bool can_be_defaultable = std::all_of(
          struct_gen->fields().begin(), struct_gen->fields().end(),
          [](ValueType type) -> bool { return type.is_defaultable(); });
      bool is_mutable = std::all_of(
          struct_gen->mutabilities().begin(), struct_gen->mutabilities().end(),
          [](bool mutability) -> bool { return mutability; });
      if (new_default && can_be_defaultable && is_mutable) {
        builder_->EmitWithPrefix(kExprStructNewDefault);
        builder_->EmitU32V(index);
      } else {
        for (int i = 0; i < field_count; i++) {
          Generate(struct_gen->field(i).Unpacked(), data);
        }
        builder_->EmitWithPrefix(kExprStructNew);
        builder_->EmitU32V(index);
      }
    } else if (builder_->builder()->IsArrayType(index)) {
      bool can_be_defaultable = builder_->builder()
                                    ->GetArrayType(index)
                                    ->element_type()
                                    .is_defaultable();
      if (new_default && can_be_defaultable) {
        Generate(kWasmI32, data);
        builder_->EmitI32Const(kMaxArraySize);
        builder_->Emit(kExprI32RemS);
        builder_->EmitWithPrefix(kExprArrayNewDefault);
        builder_->EmitU32V(index);
      } else {
        Generate(
            builder_->builder()->GetArrayType(index)->element_type().Unpacked(),
            data);
        Generate(kWasmI32, data);
        builder_->EmitI32Const(kMaxArraySize);
        builder_->Emit(kExprI32RemS);
        builder_->EmitWithPrefix(kExprArrayNew);
        builder_->EmitU32V(index);
      }
    } else {
      // Map the type index to a function index.
      // TODO(11954. 7748): Once we have type canonicalization, choose a random
      // function from among those matching the signature (consider function
      // subtyping?).
      uint32_t func_index = index - (num_arrays_ + num_structs_);
      DCHECK_EQ(builder_->builder()->GetSignature(index),
                builder_->builder()->GetFunction(func_index)->signature());
      builder_->EmitWithU32V(kExprRefFunc, func_index);
    }

    return true;
  }

  template <ValueKind wanted_kind>
  void table_op(std::vector<ValueType> types, DataRange* data,
                WasmOpcode opcode) {
    DCHECK(opcode == kExprTableSet || opcode == kExprTableSize ||
           opcode == kExprTableGrow || opcode == kExprTableFill);
    int num_tables = builder_->builder()->NumTables();
    DCHECK_GT(num_tables, 0);
    int index = data->get<uint8_t>() % num_tables;
    for (size_t i = 0; i < types.size(); i++) {
      // When passing the reftype by default kWasmFuncRef is used.
      // Then the type is changed according to its table type.
      if (types[i] == kWasmFuncRef) {
        types[i] = builder_->builder()->GetTableType(index);
      }
    }
    Generate(base::VectorOf(types), data);
    if (opcode == kExprTableSet) {
      builder_->Emit(opcode);
    } else {
      builder_->EmitWithPrefix(opcode);
    }
    builder_->EmitU32V(index);
  }

  bool table_get(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    int table_count = builder_->builder()->NumTables();
    ZoneVector<uint32_t> table(builder_->builder()->zone());
    for (int i = 0; i < table_count; i++) {
      if (builder_->builder()->GetTableType(i) == needed_type) {
        table.push_back(i);
      }
    }
    if (table.empty()) {
      return false;
    }
    int index = data->get<uint8_t>() % static_cast<int>(table.size());
    Generate(kWasmI32, data);
    builder_->Emit(kExprTableGet);
    builder_->EmitU32V(table[index]);
    return true;
  }

  void table_set(DataRange* data) {
    table_op<kVoid>({kWasmI32, kWasmFuncRef}, data, kExprTableSet);
  }
  void table_size(DataRange* data) { table_op<kI32>({}, data, kExprTableSize); }
  void table_grow(DataRange* data) {
    table_op<kI32>({kWasmFuncRef, kWasmI32}, data, kExprTableGrow);
  }
  void table_fill(DataRange* data) {
    table_op<kVoid>({kWasmI32, kWasmFuncRef, kWasmI32}, data, kExprTableFill);
  }
  void table_copy(DataRange* data) {
    ValueType needed_type = data->get<bool>() ? kWasmFuncRef : kWasmExternRef;
    int table_count = builder_->builder()->NumTables();
    ZoneVector<uint32_t> table(builder_->builder()->zone());
    for (int i = 0; i < table_count; i++) {
      if (builder_->builder()->GetTableType(i) == needed_type) {
        table.push_back(i);
      }
    }
    if (table.empty()) {
      return;
    }
    int first_index = data->get<uint8_t>() % static_cast<int>(table.size());
    int second_index = data->get<uint8_t>() % static_cast<int>(table.size());
    Generate(kWasmI32, data);
    Generate(kWasmI32, data);
    Generate(kWasmI32, data);
    builder_->EmitWithPrefix(kExprTableCopy);
    builder_->EmitU32V(table[first_index]);
    builder_->EmitU32V(table[second_index]);
  }

  bool array_get_helper(ValueType value_type, DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> array_indices(builder->zone());

    for (uint32_t i = num_structs_; i < num_arrays_ + num_structs_; i++) {
      DCHECK(builder->IsArrayType(i));
      if (builder->GetArrayType(i)->element_type().Unpacked() == value_type) {
        array_indices.push_back(i);
      }
    }

    if (!array_indices.empty()) {
      int index = data->get<uint8_t>() % static_cast<int>(array_indices.size());
      GenerateRef(HeapType(array_indices[index]), data, kNullable);
      Generate(kWasmI32, data);
      if (builder->GetArrayType(array_indices[index])
              ->element_type()
              .is_packed()) {
        builder_->EmitWithPrefix(data->get<bool>() ? kExprArrayGetS
                                                   : kExprArrayGetU);

      } else {
        builder_->EmitWithPrefix(kExprArrayGet);
      }
      builder_->EmitU32V(array_indices[index]);
      return true;
    }

    return false;
  }

  template <ValueKind wanted_kind>
  void array_get(DataRange* data) {
    bool got_array_value =
        array_get_helper(ValueType::Primitive(wanted_kind), data);
    if (!got_array_value) {
      Generate<wanted_kind>(data);
    }
  }
  bool array_get_ref(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    return array_get_helper(needed_type, data);
  }

  void i31_get(DataRange* data) {
    if (!liftoff_as_reference_) {
      Generate(kWasmI32, data);
      return;
    }
    GenerateRef(HeapType(HeapType::kI31), data);
    builder_->Emit(kExprRefAsNonNull);
    if (data->get<bool>()) {
      builder_->EmitWithPrefix(kExprI31GetS);
    } else {
      builder_->EmitWithPrefix(kExprI31GetU);
    }
  }

  void array_len(DataRange* data) {
    if (num_arrays_ > 1) {
      int array_index = (data->get<uint8_t>() % num_arrays_) + num_structs_;
      DCHECK(builder_->builder()->IsArrayType(array_index));
      GenerateRef(HeapType(array_index), data);
      builder_->EmitWithPrefix(kExprArrayLen);
    } else {
      Generate(kWasmI32, data);
    }
  }

  void array_set(DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> array_indices(builder->zone());
    for (uint32_t i = num_structs_; i < num_arrays_ + num_structs_; i++) {
      DCHECK(builder->IsArrayType(i));
      if (builder->GetArrayType(i)->mutability()) {
        array_indices.push_back(i);
      }
    }

    if (array_indices.empty()) {
      return;
    }

    int index = data->get<uint8_t>() % static_cast<int>(array_indices.size());
    GenerateRef(HeapType(array_indices[index]), data);
    Generate(kWasmI32, data);
    Generate(
        builder->GetArrayType(array_indices[index])->element_type().Unpacked(),
        data);
    builder_->EmitWithPrefix(kExprArraySet);
    builder_->EmitU32V(array_indices[index]);
  }

  bool struct_get_helper(ValueType value_type, DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> field_index(builder->zone());
    ZoneVector<uint32_t> struct_index(builder->zone());
    for (uint32_t i = 0; i < num_structs_; i++) {
      DCHECK(builder->IsStructType(i));
      int field_count = builder->GetStructType(i)->field_count();
      for (int index = 0; index < field_count; index++) {
        if (builder->GetStructType(i)->field(index) == value_type) {
          field_index.push_back(index);
          struct_index.push_back(i);
        }
      }
    }
    if (!field_index.empty()) {
      int index = data->get<uint8_t>() % static_cast<int>(field_index.size());
      GenerateRef(HeapType(struct_index[index]), data, kNullable);
      if (builder->GetStructType(struct_index[index])
              ->field(field_index[index])
              .is_packed()) {
        builder_->EmitWithPrefix(data->get<bool>() ? kExprStructGetS
                                                   : kExprStructGetU);
      } else {
        builder_->EmitWithPrefix(kExprStructGet);
      }
      builder_->EmitU32V(struct_index[index]);
      builder_->EmitU32V(field_index[index]);
      return true;
    }
    return false;
  }

  template <ValueKind wanted_kind>
  void struct_get(DataRange* data) {
    bool got_struct_value =
        struct_get_helper(ValueType::Primitive(wanted_kind), data);
    if (!got_struct_value) {
      Generate<wanted_kind>(data);
    }
  }

  bool struct_get_ref(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    return struct_get_helper(needed_type, data);
  }

  void struct_set(DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    if (num_structs_ > 0) {
      int struct_index = data->get<uint8_t>() % num_structs_;
      DCHECK(builder->IsStructType(struct_index));
      const StructType* struct_type = builder->GetStructType(struct_index);
      ZoneVector<uint32_t> field_indices(builder->zone());
      for (uint32_t i = 0; i < struct_type->field_count(); i++) {
        if (struct_type->mutability(i)) {
          field_indices.push_back(i);
        }
      }
      if (field_indices.empty()) {
        return;
      }
      int field_index =
          field_indices[data->get<uint8_t>() % field_indices.size()];
      GenerateRef(HeapType(struct_index), data);
      Generate(struct_type->field(field_index).Unpacked(), data);
      builder_->EmitWithPrefix(kExprStructSet);
      builder_->EmitU32V(struct_index);
      builder_->EmitU32V(field_index);
    }
  }

  template <ValueKind wanted_kind>
  void ref_is_null(DataRange* data) {
    GenerateRef(HeapType(HeapType::kAny), data);
    builder_->Emit(kExprRefIsNull);
  }

  void ref_eq(DataRange* data) {
    if (!liftoff_as_reference_) {
      Generate(kWasmI32, data);
      return;
    }
    GenerateRef(HeapType(HeapType::kEq), data);
    GenerateRef(HeapType(HeapType::kEq), data);
    builder_->Emit(kExprRefEq);
  }

  using GenerateFn = void (WasmGenerator::*const)(DataRange*);
  using GenerateFnWithHeap = bool (WasmGenerator::*const)(HeapType, DataRange*,
                                                          Nullability);

  template <size_t N>
  void GenerateOneOf(GenerateFn (&alternatives)[N], DataRange* data) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternatives. Use a bigger type if needed.");
    const auto which = data->get<uint8_t>();

    GenerateFn alternate = alternatives[which % N];
    (this->*alternate)(data);
  }

  // Returns true if it had succesfully generated the reference
  // and false otherwise.
  template <size_t N>
  bool GenerateOneOf(GenerateFnWithHeap (&alternatives)[N], HeapType type,
                     DataRange* data, Nullability nullability) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternatives. Use a bigger type if needed.");

    int index = data->get<uint8_t>() % (N + 1);

    if (nullability && index == N) {
      ref_null(type, data);
      return true;
    }

    for (int i = index; i < static_cast<int>(N); i++) {
      if ((this->*alternatives[i])(type, data, nullability)) {
        return true;
      }
    }

    for (int i = 0; i < index; i++) {
      if ((this->*alternatives[i])(type, data, nullability)) {
        return true;
      }
    }

    if (nullability == kNullable) {
      ref_null(type, data);
      return true;
    }

    return false;
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
                const std::vector<uint8_t>& mutable_globals,
                uint32_t num_structs, uint32_t num_arrays, DataRange* data,
                bool liftoff_as_reference)
      : builder_(fn),
        functions_(functions),
        globals_(globals),
        mutable_globals_(mutable_globals),
        num_structs_(num_structs),
        num_arrays_(num_arrays),
        liftoff_as_reference_(liftoff_as_reference) {
    const FunctionSig* sig = fn->signature();
    blocks_.emplace_back();
    for (size_t i = 0; i < sig->return_count(); ++i) {
      blocks_.back().push_back(sig->GetReturn(i));
    }
    constexpr uint32_t kMaxLocals = 32;
    locals_.resize(data->get<uint8_t>() % kMaxLocals);
    uint32_t num_types =
        static_cast<uint32_t>(functions_.size()) + num_structs_ + num_arrays_;
    for (ValueType& local : locals_) {
      local = GetValueTypeHelper(data, liftoff_as_reference_, num_types,
                                 num_types, kDisallowNonNullables,
                                 kExcludePackedTypes, kIncludeGenerics);
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

  void GenerateRef(HeapType type, DataRange* data,
                   Nullability nullability = kNullable);

  std::vector<ValueType> GenerateTypes(DataRange* data);
  void Generate(base::Vector<const ValueType> types, DataRange* data);
  void ConsumeAndGenerate(base::Vector<const ValueType> parameter_types,
                          base::Vector<const ValueType> return_types,
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
  std::vector<int> catch_blocks_;
  bool has_simd_;
  uint32_t num_structs_;
  uint32_t num_arrays_;
  bool liftoff_as_reference_;
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
      &WasmGenerator::br_on_null<kVoid>,

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
      &WasmGenerator::call_ref<kVoid>,

      &WasmGenerator::set_local,
      &WasmGenerator::set_global,
      &WasmGenerator::throw_or_rethrow,
      &WasmGenerator::try_block<kVoid>,

      &WasmGenerator::struct_set,
      &WasmGenerator::array_set,

      &WasmGenerator::table_set,
      &WasmGenerator::table_fill,
      &WasmGenerator::table_copy};

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
      &WasmGenerator::br_on_null<kI32>,

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
      &WasmGenerator::call_ref<kI32>,
      &WasmGenerator::try_block<kI32>,

      &WasmGenerator::i31_get,

      &WasmGenerator::struct_get<kI32>,
      &WasmGenerator::array_get<kI32>,
      &WasmGenerator::array_len,

      &WasmGenerator::ref_is_null<kI32>,
      &WasmGenerator::ref_eq,

      &WasmGenerator::table_size,
      &WasmGenerator::table_grow};

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
      &WasmGenerator::br_on_null<kI64>,

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
      &WasmGenerator::call_ref<kI64>,
      &WasmGenerator::try_block<kI64>,

      &WasmGenerator::struct_get<kI64>,
      &WasmGenerator::array_get<kI64>};

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
      &WasmGenerator::br_on_null<kF32>,

      &WasmGenerator::memop<kExprF32LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF32x4ExtractLane, 4, kS128>,

      &WasmGenerator::get_local<kF32>,
      &WasmGenerator::tee_local<kF32>,
      &WasmGenerator::get_global<kF32>,
      &WasmGenerator::op<kExprSelect, kF32, kF32, kI32>,
      &WasmGenerator::select_with_type<kF32>,

      &WasmGenerator::call<kF32>,
      &WasmGenerator::call_indirect<kF32>,
      &WasmGenerator::call_ref<kF32>,
      &WasmGenerator::try_block<kF32>,

      &WasmGenerator::struct_get<kF32>,
      &WasmGenerator::array_get<kF32>};

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
      &WasmGenerator::br_on_null<kF64>,

      &WasmGenerator::memop<kExprF64LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF64x2ExtractLane, 2, kS128>,

      &WasmGenerator::get_local<kF64>,
      &WasmGenerator::tee_local<kF64>,
      &WasmGenerator::get_global<kF64>,
      &WasmGenerator::op<kExprSelect, kF64, kF64, kI32>,
      &WasmGenerator::select_with_type<kF64>,

      &WasmGenerator::call<kF64>,
      &WasmGenerator::call_indirect<kF64>,
      &WasmGenerator::call_ref<kF64>,
      &WasmGenerator::try_block<kF64>,

      &WasmGenerator::struct_get<kF64>,
      &WasmGenerator::array_get<kF64>};

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
    case kRefNull:
      return GenerateRef(type.heap_type(), data, kNullable);
    case kRef:
      return GenerateRef(type.heap_type(), data, kNonNullable);
    default:
      UNREACHABLE();
  }
}

void WasmGenerator::GenerateRef(HeapType type, DataRange* data,
                                Nullability nullability) {
  base::Optional<GeneratorRecursionScope> rec_scope;
  if (nullability) {
    rec_scope.emplace(this);
  }

  if (recursion_limit_reached() || data->size() == 0) {
    if (nullability == kNullable) {
      ref_null(type, data);
      return;
    }
    // It is ok not to return here because the non-nullable types are not
    // recursive by construction, so the depth is limited already.
  }

  constexpr GenerateFnWithHeap alternatives_indexed_type[] = {
      &WasmGenerator::new_object, &WasmGenerator::get_local_ref,
      &WasmGenerator::array_get_ref, &WasmGenerator::struct_get_ref};

  constexpr GenerateFnWithHeap alternatives_func_any[] = {
      &WasmGenerator::table_get, &WasmGenerator::get_local_ref,
      &WasmGenerator::array_get_ref, &WasmGenerator::struct_get_ref};

  constexpr GenerateFnWithHeap alternatives_other[] = {
      &WasmGenerator::array_get_ref, &WasmGenerator::get_local_ref,
      &WasmGenerator::struct_get_ref};

  switch (type.representation()) {
    // For abstract types, sometimes generate one of their subtypes.
    case HeapType::kAny: {
      // Note: It is possible we land here even without {liftoff_as_reference_}.
      // In this case, we do not support any subtyping, and just fall back to
      // directly generating anyref.
      if (!liftoff_as_reference_) {
        DCHECK(nullability);
        GenerateOneOf(alternatives_func_any, type, data, nullability);
        return;
      }
      // Weighed according to the types in the module:
      // If there are D data types and F function types, the relative
      // frequencies for dataref is D, for funcref F, and for i31ref and falling
      // back to anyref 2.
      const uint8_t num_data_types = num_structs_ + num_arrays_;
      const uint8_t emit_i31ref = 2;
      const uint8_t fallback_to_anyref = 2;
      uint8_t random = data->get<uint8_t>() %
                       (num_data_types + emit_i31ref + fallback_to_anyref);
      // We have to compute this first so in case GenerateOneOf fails
      // we will continue to fall back on an alternative that is guaranteed
      // to generate a value of the wanted type.
      // In order to know which alternative to fall back to in case
      // GenerateOneOf failed, the random variable is recomputed.
      if (random >= num_data_types + emit_i31ref) {
        DCHECK(liftoff_as_reference_);
        if (GenerateOneOf(alternatives_func_any, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % (num_data_types + emit_i31ref);
      }
      if (random < num_data_types) {
        GenerateRef(HeapType(HeapType::kData), data, nullability);
      } else {
        GenerateRef(HeapType(HeapType::kI31), data, nullability);
      }
      return;
    }
    case HeapType::kArray: {
      DCHECK(liftoff_as_reference_);
      constexpr uint8_t fallback_to_dataref = 1;
      uint8_t random =
          data->get<uint8_t>() % (num_arrays_ + fallback_to_dataref);
      // Try generating one of the alternatives and continue to the rest of the
      // methods in case it fails.
      if (random >= num_arrays_) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) return;
        random = data->get<uint8_t>() % num_arrays_;
      }
      GenerateRef(HeapType(random), data, nullability);
      return;
    }
    case HeapType::kData: {
      DCHECK(liftoff_as_reference_);
      constexpr uint8_t fallback_to_dataref = 2;
      uint8_t random = data->get<uint8_t>() %
                       (num_arrays_ + num_structs_ + fallback_to_dataref);
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= num_arrays_ + num_structs_) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % (num_arrays_ + num_structs_);
      }
      GenerateRef(HeapType(random), data, nullability);
      return;
    }
    case HeapType::kEq: {
      DCHECK(liftoff_as_reference_);
      const uint8_t num_types = num_arrays_ + num_structs_;
      const uint8_t emit_i31ref = 2;
      constexpr uint8_t fallback_to_eqref = 1;
      uint8_t random =
          data->get<uint8_t>() % (num_types + emit_i31ref + fallback_to_eqref);
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= num_types + emit_i31ref) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % (num_types + emit_i31ref);
      }
      if (random < num_types) {
        GenerateRef(HeapType(random), data, nullability);
      } else {
        GenerateRef(HeapType(HeapType::kI31), data, nullability);
      }
      return;
    }
    case HeapType::kFunc: {
      uint32_t random = data->get<uint32_t>() % (functions_.size() + 1);
      /// Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= functions_.size()) {
        if (GenerateOneOf(alternatives_func_any, type, data, nullability)) {
          return;
        }
        random = data->get<uint32_t>() % functions_.size();
      }
      if (liftoff_as_reference_) {
        // Only reduce to indexed type with liftoff as reference.
        uint32_t signature_index = functions_[random];
        DCHECK(builder_->builder()->IsSignature(signature_index));
        GenerateRef(HeapType(signature_index), data, nullability);
      } else {
        // If interpreter is used as reference, generate a ref.func directly.
        builder_->EmitWithU32V(kExprRefFunc, random);
      }
      return;
    }
    case HeapType::kI31: {
      DCHECK(liftoff_as_reference_);
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (data->get<bool>() &&
          GenerateOneOf(alternatives_other, type, data, nullability)) {
        return;
      }
      Generate(kWasmI32, data);
      builder_->EmitWithPrefix(kExprI31New);
      return;
    }
    case HeapType::kExtern:
    case HeapType::kNoExtern:
    case HeapType::kNoFunc:
    case HeapType::kNone:
      DCHECK(nullability == Nullability::kNullable);
      ref_null(type, data);
      return;
    default:
      // Indexed type.
      DCHECK(type.is_index());
      DCHECK(liftoff_as_reference_);
      GenerateOneOf(alternatives_indexed_type, type, data, nullability);
      return;
  }
  UNREACHABLE();
}

std::vector<ValueType> WasmGenerator::GenerateTypes(DataRange* data) {
  std::vector<ValueType> types;
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  for (int i = 0; i < num_params; ++i) {
    types.push_back(GetValueType(
        data, liftoff_as_reference_,
        num_structs_ + num_arrays_ + static_cast<uint32_t>(functions_.size())));
  }
  return types;
}

void WasmGenerator::Generate(base::Vector<const ValueType> types,
                             DataRange* data) {
  // Maybe emit a multi-value block with the expected return type. Use a
  // non-default value to indicate block generation to avoid recursion when we
  // reach the end of the data.
  bool generate_block = data->get<uint8_t>() % 32 == 1;
  if (generate_block) {
    GeneratorRecursionScope rec_scope(this);
    if (!recursion_limit_reached()) {
      const auto param_types = GenerateTypes(data);
      Generate(base::VectorOf(param_types), data);
      any_block(base::VectorOf(param_types), types, data);
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
  base::Vector<const ValueType> lower_half = types.SubVector(0, split_index);
  base::Vector<const ValueType> upper_half =
      types.SubVector(split_index, types.size());
  DataRange first_range = data->split();
  Generate(lower_half, &first_range);
  Generate(upper_half, data);
}

// Emit code to match an arbitrary signature.
// TODO(11954): Add the missing reference type conversion/upcasting.
void WasmGenerator::ConsumeAndGenerate(
    base::Vector<const ValueType> param_types,
    base::Vector<const ValueType> return_types, DataRange* data) {
  // This numeric conversion logic consists of picking exactly one
  // index in the return values and dropping all the values that come
  // before that index. Then we convert the value from that index to the
  // wanted type. If we don't find any value we generate it.
  auto primitive = [](ValueType t) -> bool {
    switch (t.kind()) {
      case kI32:
      case kI64:
      case kF32:
      case kF64:
        return true;
      default:
        return false;
    }
  };

  if (return_types.size() == 0 || param_types.size() == 0 ||
      !primitive(return_types[0])) {
    for (unsigned i = 0; i < param_types.size(); i++) {
      builder_->Emit(kExprDrop);
    }
    Generate(return_types, data);
    return;
  }

  int bottom_primitives = 0;

  while (static_cast<int>(param_types.size()) > bottom_primitives &&
         primitive(param_types[bottom_primitives])) {
    bottom_primitives++;
  }
  int return_index =
      bottom_primitives > 0 ? (data->get<uint8_t>() % bottom_primitives) : -1;
  for (int i = static_cast<int>(param_types.size() - 1); i > return_index;
       --i) {
    builder_->Emit(kExprDrop);
  }
  for (int i = return_index; i > 0; --i) {
    Convert(param_types[i], param_types[i - 1]);
    builder_->EmitI32Const(0);
    builder_->Emit(kExprSelect);
  }
  DCHECK(!return_types.empty());
  if (return_index >= 0) {
    Convert(param_types[0], return_types[0]);
    Generate(return_types + 1, data);
  } else {
    Generate(return_types, data);
  }
}

enum SigKind { kFunctionSig, kExceptionSig };

FunctionSig* GenerateSig(Zone* zone, DataRange* data, SigKind sig_kind,
                         bool liftoff_as_reference, int num_types) {
  // Generate enough parameters to spill some to the stack.
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  int num_returns = sig_kind == kFunctionSig
                        ? int{data->get<uint8_t>()} % (kMaxReturns + 1)
                        : 0;

  FunctionSig::Builder builder(zone, num_returns, num_params);
  for (int i = 0; i < num_returns; ++i) {
    builder.AddReturn(GetValueType(data, liftoff_as_reference, num_types));
  }
  for (int i = 0; i < num_params; ++i) {
    builder.AddParam(GetValueType(data, liftoff_as_reference, num_types));
  }
  return builder.Build();
}

WasmInitExpr GenerateInitExpr(Zone* zone, WasmModuleBuilder* builder,
                              ValueType type,
                              uint32_t num_struct_and_array_types);

WasmInitExpr GenerateStructNewInitExpr(Zone* zone, WasmModuleBuilder* builder,
                                       uint32_t index,
                                       uint32_t num_struct_and_array_types) {
  const StructType* struct_type = builder->GetStructType(index);
  ZoneVector<WasmInitExpr>* elements =
      zone->New<ZoneVector<WasmInitExpr>>(zone);
  int field_count = struct_type->field_count();
  for (int field_index = 0; field_index < field_count; field_index++) {
    elements->push_back(GenerateInitExpr(zone, builder,
                                         struct_type->field(field_index),
                                         num_struct_and_array_types));
  }
  return WasmInitExpr::StructNew(index, elements);
}

WasmInitExpr GenerateInitExpr(Zone* zone, WasmModuleBuilder* builder,
                              ValueType type,
                              uint32_t num_struct_and_array_types) {
  switch (type.kind()) {
    case kRefNull:
      return WasmInitExpr::RefNullConst(type.heap_type().representation());
    case kI8:
    case kI16:
    case kI32:
      return WasmInitExpr(int32_t{0});
    case kI64:
      return WasmInitExpr(int64_t{0});
    case kF32:
      return WasmInitExpr(0.0f);
    case kF64:
      return WasmInitExpr(0.0);
    case kS128: {
      uint8_t s128_const[kSimd128Size] = {0};
      return WasmInitExpr(s128_const);
    }
    case kRef: {
      switch (type.heap_type().representation()) {
        case HeapType::kData:
        case HeapType::kAny:
        case HeapType::kEq: {
          // We materialize all these types with a struct because they are all
          // its supertypes.
          DCHECK(builder->IsStructType(0));
          return GenerateStructNewInitExpr(zone, builder, 0,
                                           num_struct_and_array_types);
        }
        case HeapType::kFunc:
          // We just pick the function at index 0.
          DCHECK_GT(builder->NumFunctions(), 0);
          return WasmInitExpr::RefFuncConst(0);
        default: {
          uint32_t index = type.ref_index();
          if (builder->IsStructType(index)) {
            return GenerateStructNewInitExpr(zone, builder, index,
                                             num_struct_and_array_types);
          }
          if (builder->IsArrayType(index)) {
            ZoneVector<WasmInitExpr>* elements =
                zone->New<ZoneVector<WasmInitExpr>>(zone);
            elements->push_back(GenerateInitExpr(
                zone, builder, builder->GetArrayType(index)->element_type(),
                num_struct_and_array_types));
            return WasmInitExpr::ArrayNewFixed(index, elements);
          }
          if (builder->IsSignature(index)) {
            // Transform from signature index to function index.
            return WasmInitExpr::RefFuncConst(index -
                                              num_struct_and_array_types);
          }
          UNREACHABLE();
        }
      }
    }
    case kVoid:
    case kRtt:
    case kBottom:
      UNREACHABLE();
  }
}
}  // namespace

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(Isolate* isolate, Zone* zone,
                      base::Vector<const uint8_t> data, ZoneBuffer* buffer,
                      bool liftoff_as_reference) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    DataRange range(data);
    std::vector<uint32_t> function_signatures;

    // Add struct and array types first so that we get a chance to generate
    // these types in function signatures.
    // Currently, WasmGenerator assumes this order for struct/array/signature
    // definitions.

    uint8_t num_structs = 0;
    uint8_t num_arrays = 0;
    static_assert(kMaxFunctions >= 1, "need min. 1 function");
    uint8_t num_functions = 1 + (range.get<uint8_t>() % kMaxFunctions);
    uint16_t num_types = num_functions;

    if (liftoff_as_reference) {
      // We need at least one struct/array in order to support WasmInitExpr
      // for kData, kAny and kEq.
      num_structs = 1 + range.get<uint8_t>() % kMaxStructs;
      num_arrays = range.get<uint8_t>() % (kMaxArrays + 1);
      num_types += num_structs + num_arrays;

      for (int struct_index = 0; struct_index < num_structs; struct_index++) {
        uint8_t num_fields = range.get<uint8_t>() % (kMaxStructFields + 1);
        StructType::Builder struct_builder(zone, num_fields);
        for (int field_index = 0; field_index < num_fields; field_index++) {
          // Notes:
          // - We allow a type to only have non-nullable fields of types that
          //   are defined earlier. This way we avoid infinite non-nullable
          //   constructions. Also relevant for arrays and functions.
          // - Currently, we also allow nullable fields to only reference types
          //   that are defined earlier. The reason is that every type can only
          //   reference types in its own or earlier recursive groups, and we do
          //   not support recursive groups yet. Also relevant for arrays and
          //   functions. TODO(7748): Change the number of nullable types once
          //   we support rec. groups.
          // - We exclude the generics types anyref, dataref, and eqref from the
          //   fields of struct 0. This is because in GenerateInitExpr we
          //   materialize these types with (ref 0), and having such fields in
          //   struct 0 would produce an infinite recursion.
          ValueType type = GetValueTypeHelper(
              &range, true, builder.NumTypes(), builder.NumTypes(),
              kAllowNonNullables, kIncludePackedTypes,
              struct_index != 0 ? kIncludeGenerics : kExcludeGenerics);

          bool mutability = range.get<bool>();
          struct_builder.AddField(type, mutability);
        }
        StructType* struct_fuz = struct_builder.Build();
        builder.AddStructType(struct_fuz);
      }

      for (int array_index = 0; array_index < num_arrays; array_index++) {
        ValueType type = GetValueTypeHelper(
            &range, true, builder.NumTypes(), builder.NumTypes(),
            kAllowNonNullables, kIncludePackedTypes, kIncludeGenerics);
        ArrayType* array_fuz = zone->New<ArrayType>(type, true);
        builder.AddArrayType(array_fuz);
      }
    }

    // We keep the signature for the first (main) function constant.
    function_signatures.push_back(builder.ForceAddSignature(sigs.i_iii()));

    for (uint8_t i = 1; i < num_functions; i++) {
      FunctionSig* sig = GenerateSig(zone, &range, kFunctionSig,
                                     liftoff_as_reference, builder.NumTypes());
      uint32_t signature_index = builder.ForceAddSignature(sig);
      function_signatures.push_back(signature_index);
    }

    int num_exceptions = 1 + (range.get<uint8_t>() % kMaxExceptions);
    for (int i = 0; i < num_exceptions; ++i) {
      FunctionSig* sig = GenerateSig(zone, &range, kExceptionSig,
                                     liftoff_as_reference, num_types);
      builder.AddException(sig);
    }

    // Generate function declarations before tables. This will be needed once we
    // have typed-function tables.
    std::vector<WasmFunctionBuilder*> functions;
    for (uint8_t i = 0; i < num_functions; i++) {
      const FunctionSig* sig = builder.GetSignature(function_signatures[i]);
      // If we are using wasm-gc, we cannot allow signature normalization
      // performed by adding a function by {FunctionSig}, because we emit
      // everything in one recursive group which blocks signature
      // canonicalization.
      // TODO(7748): Relax this when we implement proper recursive-group
      // support.
      functions.push_back(liftoff_as_reference
                              ? builder.AddFunction(function_signatures[i])
                              : builder.AddFunction(sig));
    }

    int num_globals = range.get<uint8_t>() % (kMaxGlobals + 1);
    std::vector<ValueType> globals;
    std::vector<uint8_t> mutable_globals;
    globals.reserve(num_globals);
    mutable_globals.reserve(num_globals);

    for (int i = 0; i < num_globals; ++i) {
      ValueType type = GetValueTypeHelper(
          &range, liftoff_as_reference, num_types, num_types,
          kAllowNonNullables, kExcludePackedTypes, kIncludeGenerics);
      // 1/8 of globals are immutable.
      const bool mutability = (range.get<uint8_t>() % 8) != 0;

      builder.AddGlobal(
          type, mutability,
          GenerateInitExpr(zone, &builder, type,
                           static_cast<uint32_t>(num_structs + num_arrays)));
      globals.push_back(type);
      if (mutability) mutable_globals.push_back(static_cast<uint8_t>(i));
    }

    // Generate tables before function bodies, so they are available for table
    // operations.
    // Always generate at least one table for call_indirect.
    int num_tables = range.get<uint8_t>() % kMaxTables + 1;
    for (int i = 0; i < num_tables; i++) {
      // Table 0 has to reference all functions in the program. This is so that
      // all functions count as declared so they can be referenced with
      // ref.func.
      // TODO(11954): Consider removing this restriction.
      uint32_t min_size =
          i == 0 ? num_functions : range.get<uint8_t>() % kMaxTableSize;
      uint32_t max_size =
          range.get<uint8_t>() % (kMaxTableSize - min_size) + min_size;
      // Table 0 is always funcref.
      // TODO(11954): Remove this requirement once we support call_indirect with
      // other table indices.
      // TODO(11954): Support typed function tables.
      bool use_funcref = i == 0 || range.get<bool>();
      ValueType type = use_funcref ? kWasmFuncRef : kWasmExternRef;
      uint32_t table_index = builder.AddTable(type, min_size, max_size);
      if (type == kWasmFuncRef) {
        // For function tables, initialize them with functions from the program.
        // Currently, the fuzzer assumes that every function table contains the
        // functions in the program in the order they are defined.
        // TODO(11954): Consider generalizing this.
        WasmModuleBuilder::WasmElemSegment segment(
            zone, kWasmFuncRef, table_index, WasmInitExpr(0));
        for (int entry_index = 0; entry_index < static_cast<int>(min_size);
             entry_index++) {
          segment.entries.emplace_back(
              WasmModuleBuilder::WasmElemSegment::Entry::kRefFuncEntry,
              entry_index % num_functions);
        }
        builder.AddElementSegment(std::move(segment));
      }
    }

    for (int i = 0; i < num_functions; ++i) {
      WasmFunctionBuilder* f = functions[i];
      DataRange function_range = range.split();
      WasmGenerator gen(f, function_signatures, globals, mutable_globals,
                        num_structs, num_arrays, &function_range,
                        liftoff_as_reference);
      const FunctionSig* sig = f->signature();
      base::Vector<const ValueType> return_types(sig->returns().begin(),
                                                 sig->return_count());
      gen.Generate(return_types, &function_range);
      if (!CheckHardwareSupportsSimd() && gen.HasSimd()) return false;
      f->Emit(kExprEnd);
      if (i == 0) builder.AddExport(base::CStrVector("main"), f);
    }

    builder.SetMaxMemorySize(32);
    builder.WriteTo(buffer);
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bool require_valid = true;
  EXPERIMENTAL_FLAG_SCOPE(typed_funcref);
  EXPERIMENTAL_FLAG_SCOPE(gc);
  EXPERIMENTAL_FLAG_SCOPE(simd);
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
