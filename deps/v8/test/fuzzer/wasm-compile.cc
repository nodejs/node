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

class DataRange {
  const uint8_t* data_;
  size_t size_;

 public:
  DataRange(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  size_t size() const { return size_; }

  std::pair<DataRange, DataRange> split(uint32_t index) const {
    return std::make_pair(DataRange(data_, index),
                          DataRange(data_ + index, size() - index));
  }

  std::pair<DataRange, DataRange> split() {
    uint16_t index = get<uint16_t>();
    if (size() > 0) {
      index = index % size();
    } else {
      index = 0;
    }
    return split(index);
  }

  template <typename T>
  T get() {
    if (size() == 0) {
      return T();
    } else {
      // We want to support the case where we have less than sizeof(T) bytes
      // remaining in the slice. For example, if we emit an i32 constant, it's
      // okay if we don't have a full four bytes available, we'll just use what
      // we have. We aren't concerned about endianness because we are generating
      // arbitrary expressions.
      const size_t num_bytes = std::min(sizeof(T), size());
      T result = T();
      memcpy(&result, data_, num_bytes);
      data_ += num_bytes;
      size_ -= num_bytes;
      return result;
    }
  }
};

class WasmGenerator {
  template <WasmOpcode Op, ValueType... Args>
  void op(DataRange data) {
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
                                WasmOpcodes::ValueTypeCodeFor(result_type));
    }

    ~BlockScope() {
      gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
  };

  template <ValueType T>
  void block(DataRange data) {
    BlockScope block_scope(this, kExprBlock, T, T);
    Generate<T>(data);
  }

  template <ValueType T>
  void loop(DataRange data) {
    // When breaking to a loop header, don't provide any input value (hence
    // kWasmStmt).
    BlockScope block_scope(this, kExprLoop, T, kWasmStmt);
    Generate<T>(data);
  }

  void br(DataRange data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data.get<uint32_t>() % blocks_.size();
    const ValueType break_type = blocks_[target_block];

    Generate(break_type, data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
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
  void memop(DataRange data) {
    const uint8_t align = data.get<uint8_t>() % (max_alignment(memory_op) + 1);
    const uint32_t offset = data.get<uint32_t>();

    // Generate the index and the arguments, if any.
    Generate<kWasmI32, arg_types...>(data);

    builder_->Emit(memory_op);
    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <ValueType T1, ValueType T2>
  void sequence(DataRange data) {
    Generate<T1, T2>(data);
  }

  void current_memory(DataRange data) {
    builder_->EmitWithU8(kExprMemorySize, 0);
  }

  void grow_memory(DataRange data);

  using generate_fn = void (WasmGenerator::*const)(DataRange);

  template <size_t N>
  void GenerateOneOf(generate_fn (&alternates)[N], DataRange data) {
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
  explicit WasmGenerator(WasmFunctionBuilder* fn) : builder_(fn) {
    DCHECK_EQ(1, fn->signature()->return_count());
    blocks_.push_back(fn->signature()->GetReturn(0));
  }

  void Generate(ValueType type, DataRange data);

  template <ValueType T>
  void Generate(DataRange data);

  template <ValueType T1, ValueType T2, ValueType... Ts>
  void Generate(DataRange data) {
    const auto parts = data.split();
    Generate<T1>(parts.first);
    Generate<T2, Ts...>(parts.second);
  }

 private:
  WasmFunctionBuilder* builder_;
  std::vector<ValueType> blocks_;
  uint32_t recursion_depth = 0;

  static constexpr uint32_t kMaxRecursionDepth = 64;

  bool recursion_limit_reached() {
    return recursion_depth >= kMaxRecursionDepth;
  }
};

template <>
void WasmGenerator::Generate<kWasmStmt>(DataRange data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() == 0) return;

  constexpr generate_fn alternates[] = {
      &WasmGenerator::block<kWasmStmt>,
      &WasmGenerator::loop<kWasmStmt>,
      &WasmGenerator::br,

      &WasmGenerator::memop<kExprI32StoreMem, kWasmI32>,
      &WasmGenerator::memop<kExprI32StoreMem8, kWasmI32>,
      &WasmGenerator::memop<kExprI32StoreMem16, kWasmI32>,
      &WasmGenerator::memop<kExprI64StoreMem, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem8, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem16, kWasmI64>,
      &WasmGenerator::memop<kExprI64StoreMem32, kWasmI64>,
      &WasmGenerator::memop<kExprF32StoreMem, kWasmF32>,
      &WasmGenerator::memop<kExprF64StoreMem, kWasmF64>,
  };

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmI32>(DataRange data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(uint32_t)) {
    builder_->EmitI32Const(data.get<uint32_t>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmStmt, kWasmI32>,

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

      &WasmGenerator::memop<kExprI32LoadMem>,
      &WasmGenerator::memop<kExprI32LoadMem8S>,
      &WasmGenerator::memop<kExprI32LoadMem8U>,
      &WasmGenerator::memop<kExprI32LoadMem16S>,
      &WasmGenerator::memop<kExprI32LoadMem16U>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmI64>(DataRange data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(uint64_t)) {
    builder_->EmitI64Const(data.get<int64_t>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmStmt, kWasmI64>,

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

      &WasmGenerator::memop<kExprI64LoadMem>,
      &WasmGenerator::memop<kExprI64LoadMem8S>,
      &WasmGenerator::memop<kExprI64LoadMem8U>,
      &WasmGenerator::memop<kExprI64LoadMem16S>,
      &WasmGenerator::memop<kExprI64LoadMem16U>,
      &WasmGenerator::memop<kExprI64LoadMem32S>,
      &WasmGenerator::memop<kExprI64LoadMem32U>};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmF32>(DataRange data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(float)) {
    builder_->EmitF32Const(data.get<float>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmStmt, kWasmF32>,

      &WasmGenerator::op<kExprF32Add, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Sub, kWasmF32, kWasmF32>,
      &WasmGenerator::op<kExprF32Mul, kWasmF32, kWasmF32>,

      &WasmGenerator::block<kWasmF32>,
      &WasmGenerator::loop<kWasmF32>,

      &WasmGenerator::memop<kExprF32LoadMem>};

  GenerateOneOf(alternates, data);
}

template <>
void WasmGenerator::Generate<kWasmF64>(DataRange data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data.size() <= sizeof(double)) {
    builder_->EmitF64Const(data.get<double>());
    return;
  }

  constexpr generate_fn alternates[] = {
      &WasmGenerator::sequence<kWasmStmt, kWasmF64>,

      &WasmGenerator::op<kExprF64Add, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Sub, kWasmF64, kWasmF64>,
      &WasmGenerator::op<kExprF64Mul, kWasmF64, kWasmF64>,

      &WasmGenerator::block<kWasmF64>,
      &WasmGenerator::loop<kWasmF64>,

      &WasmGenerator::memop<kExprF64LoadMem>};

  GenerateOneOf(alternates, data);
}

void WasmGenerator::grow_memory(DataRange data) {
  Generate<kWasmI32>(data);
  builder_->EmitWithU8(kExprGrowMemory, 0);
}

void WasmGenerator::Generate(ValueType type, DataRange data) {
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
}  // namespace

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmValue[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());

    WasmGenerator gen(f);
    gen.Generate<kWasmI32>(DataRange(data, static_cast<uint32_t>(size)));

    f->Emit(kExprEnd);
    builder.AddExport(CStrVector("main"), f);

    builder.SetMaxMemorySize(32);
    builder.WriteTo(buffer);

    num_args = 3;
    interpreter_args.reset(
        new WasmValue[3]{WasmValue(1), WasmValue(2), WasmValue(3)});

    compiler_args.reset(new Handle<Object>[3]{
        handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(1), isolate),
        handle(Smi::FromInt(1), isolate)});
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bool require_valid = true;
  return WasmCompileFuzzer().FuzzWasmModule(data, size, require_valid);
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
