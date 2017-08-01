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

using namespace v8::internal;
using namespace v8::internal::wasm;
using namespace v8::internal::wasm::fuzzer;

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
  std::function<void(DataRange)> op() {
    return [this](DataRange data) {
      Generate<Args...>(data);
      builder_->Emit(Op);
    };
  }

  template <ValueType T>
  std::function<void(DataRange)> block() {
    return [this](DataRange data) {
      blocks_.push_back(T);
      builder_->EmitWithU8(
          kExprBlock, static_cast<uint8_t>(WasmOpcodes::ValueTypeCodeFor(T)));
      Generate<T>(data);
      builder_->Emit(kExprEnd);
      blocks_.pop_back();
    };
  }

  template <ValueType T>
  std::function<void(DataRange)> block_br() {
    return [this](DataRange data) {
      blocks_.push_back(T);
      builder_->EmitWithU8(
          kExprBlock, static_cast<uint8_t>(WasmOpcodes::ValueTypeCodeFor(T)));

      const uint32_t target_block = data.get<uint32_t>() % blocks_.size();
      const ValueType break_type = blocks_[target_block];

      Generate(break_type, data);
      builder_->EmitWithI32V(kExprBr, target_block);
      builder_->Emit(kExprEnd);
      blocks_.pop_back();
    };
  }

 public:
  WasmGenerator(v8::internal::wasm::WasmFunctionBuilder* fn) : builder_(fn) {}

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
  v8::internal::wasm::WasmFunctionBuilder* builder_;
  std::vector<ValueType> blocks_;
};

template <>
void WasmGenerator::Generate<kWasmI32>(DataRange data) {
  if (data.size() <= sizeof(uint32_t)) {
    builder_->EmitI32Const(data.get<uint32_t>());
  } else {
    const std::function<void(DataRange)> alternates[] = {
        op<kExprI32Eqz, kWasmI32>(),  //
        op<kExprI32Eq, kWasmI32, kWasmI32>(),
        op<kExprI32Ne, kWasmI32, kWasmI32>(),
        op<kExprI32LtS, kWasmI32, kWasmI32>(),
        op<kExprI32LtU, kWasmI32, kWasmI32>(),
        op<kExprI32GeS, kWasmI32, kWasmI32>(),
        op<kExprI32GeU, kWasmI32, kWasmI32>(),

        op<kExprI64Eqz, kWasmI64>(),  //
        op<kExprI64Eq, kWasmI64, kWasmI64>(),
        op<kExprI64Ne, kWasmI64, kWasmI64>(),
        op<kExprI64LtS, kWasmI64, kWasmI64>(),
        op<kExprI64LtU, kWasmI64, kWasmI64>(),
        op<kExprI64GeS, kWasmI64, kWasmI64>(),
        op<kExprI64GeU, kWasmI64, kWasmI64>(),

        op<kExprF32Eq, kWasmF32, kWasmF32>(),
        op<kExprF32Ne, kWasmF32, kWasmF32>(),
        op<kExprF32Lt, kWasmF32, kWasmF32>(),
        op<kExprF32Ge, kWasmF32, kWasmF32>(),

        op<kExprF64Eq, kWasmF64, kWasmF64>(),
        op<kExprF64Ne, kWasmF64, kWasmF64>(),
        op<kExprF64Lt, kWasmF64, kWasmF64>(),
        op<kExprF64Ge, kWasmF64, kWasmF64>(),

        op<kExprI32Add, kWasmI32, kWasmI32>(),
        op<kExprI32Sub, kWasmI32, kWasmI32>(),
        op<kExprI32Mul, kWasmI32, kWasmI32>(),

        op<kExprI32DivS, kWasmI32, kWasmI32>(),
        op<kExprI32DivU, kWasmI32, kWasmI32>(),
        op<kExprI32RemS, kWasmI32, kWasmI32>(),
        op<kExprI32RemU, kWasmI32, kWasmI32>(),

        op<kExprI32And, kWasmI32, kWasmI32>(),
        op<kExprI32Ior, kWasmI32, kWasmI32>(),
        op<kExprI32Xor, kWasmI32, kWasmI32>(),
        op<kExprI32Shl, kWasmI32, kWasmI32>(),
        op<kExprI32ShrU, kWasmI32, kWasmI32>(),
        op<kExprI32ShrS, kWasmI32, kWasmI32>(),
        op<kExprI32Ror, kWasmI32, kWasmI32>(),
        op<kExprI32Rol, kWasmI32, kWasmI32>(),

        op<kExprI32Clz, kWasmI32>(),  //
        op<kExprI32Ctz, kWasmI32>(),  //
        op<kExprI32Popcnt, kWasmI32>(),

        op<kExprI32ConvertI64, kWasmI64>(),  //
        op<kExprI32SConvertF32, kWasmF32>(),
        op<kExprI32UConvertF32, kWasmF32>(),
        op<kExprI32SConvertF64, kWasmF64>(),
        op<kExprI32UConvertF64, kWasmF64>(),
        op<kExprI32ReinterpretF32, kWasmF32>(),

        block<kWasmI32>(),
        block_br<kWasmI32>()};

    static_assert(arraysize(alternates) < std::numeric_limits<uint8_t>::max(),
                  "Too many alternates. Replace with a bigger type if needed.");
    const auto which = data.get<uint8_t>();

    alternates[which % arraysize(alternates)](data);
  }
}

template <>
void WasmGenerator::Generate<kWasmI64>(DataRange data) {
  if (data.size() <= sizeof(uint64_t)) {
    builder_->EmitI64Const(data.get<int64_t>());
  } else {
    const std::function<void(DataRange)> alternates[] = {
        op<kExprI64Add, kWasmI64, kWasmI64>(),
        op<kExprI64Sub, kWasmI64, kWasmI64>(),
        op<kExprI64Mul, kWasmI64, kWasmI64>(),

        op<kExprI64DivS, kWasmI64, kWasmI64>(),
        op<kExprI64DivU, kWasmI64, kWasmI64>(),
        op<kExprI64RemS, kWasmI64, kWasmI64>(),
        op<kExprI64RemU, kWasmI64, kWasmI64>(),

        op<kExprI64And, kWasmI64, kWasmI64>(),
        op<kExprI64Ior, kWasmI64, kWasmI64>(),
        op<kExprI64Xor, kWasmI64, kWasmI64>(),
        op<kExprI64Shl, kWasmI64, kWasmI64>(),
        op<kExprI64ShrU, kWasmI64, kWasmI64>(),
        op<kExprI64ShrS, kWasmI64, kWasmI64>(),
        op<kExprI64Ror, kWasmI64, kWasmI64>(),
        op<kExprI64Rol, kWasmI64, kWasmI64>(),

        op<kExprI64Clz, kWasmI64>(),
        op<kExprI64Ctz, kWasmI64>(),
        op<kExprI64Popcnt, kWasmI64>(),

        block<kWasmI64>(),
        block_br<kWasmI64>()};

    static_assert(arraysize(alternates) < std::numeric_limits<uint8_t>::max(),
                  "Too many alternates. Replace with a bigger type if needed.");
    const auto which = data.get<uint8_t>();

    alternates[which % arraysize(alternates)](data);
  }
}

template <>
void WasmGenerator::Generate<kWasmF32>(DataRange data) {
  if (data.size() <= sizeof(float)) {
    builder_->EmitF32Const(data.get<float>());
  } else {
    const std::function<void(DataRange)> alternates[] = {
        op<kExprF32Add, kWasmF32, kWasmF32>(),
        op<kExprF32Sub, kWasmF32, kWasmF32>(),
        op<kExprF32Mul, kWasmF32, kWasmF32>(),

        block<kWasmF32>(), block_br<kWasmF32>()};

    static_assert(arraysize(alternates) < std::numeric_limits<uint8_t>::max(),
                  "Too many alternates. Replace with a bigger type if needed.");
    const auto which = data.get<uint8_t>();

    alternates[which % arraysize(alternates)](data);
  }
}

template <>
void WasmGenerator::Generate<kWasmF64>(DataRange data) {
  if (data.size() <= sizeof(double)) {
    builder_->EmitF64Const(data.get<double>());
  } else {
    const std::function<void(DataRange)> alternates[] = {
        op<kExprF64Add, kWasmF64, kWasmF64>(),
        op<kExprF64Sub, kWasmF64, kWasmF64>(),
        op<kExprF64Mul, kWasmF64, kWasmF64>(),

        block<kWasmF64>(), block_br<kWasmF64>()};

    static_assert(arraysize(alternates) < std::numeric_limits<uint8_t>::max(),
                  "Too many alternates. Replace with a bigger type if needed.");
    const auto which = data.get<uint8_t>();

    alternates[which % arraysize(alternates)](data);
  }
}

void WasmGenerator::Generate(ValueType type, DataRange data) {
  switch (type) {
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
}

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  virtual bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmVal[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    v8::internal::wasm::WasmFunctionBuilder* f =
        builder.AddFunction(sigs.i_iii());

    WasmGenerator gen(f);
    gen.Generate<kWasmI32>(DataRange(data, static_cast<uint32_t>(size)));

    uint8_t end_opcode = kExprEnd;
    f->EmitCode(&end_opcode, 1);
    builder.AddExport(v8::internal::CStrVector("main"), f);

    builder.WriteTo(buffer);

    num_args = 3;
    interpreter_args.reset(new WasmVal[3]{WasmVal(1), WasmVal(2), WasmVal(3)});

    compiler_args.reset(new Handle<Object>[3]{
        handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(1), isolate),
        handle(Smi::FromInt(1), isolate)});
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return WasmCompileFuzzer().FuzzWasmModule(data, size);
}
