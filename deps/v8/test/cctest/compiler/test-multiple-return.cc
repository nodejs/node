// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <limits>
#include <memory>

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

CallDescriptor* CreateCallDescriptor(Zone* zone, int return_count,
                                     int param_count, MachineType type) {
  wasm::FunctionSig::Builder builder(zone, return_count, param_count);

  for (int i = 0; i < param_count; i++) {
    builder.AddParam(type.representation());
  }

  for (int i = 0; i < return_count; i++) {
    builder.AddReturn(type.representation());
  }
  return compiler::GetWasmCallDescriptor(zone, builder.Build());
}

}  // namespace

Node* Constant(RawMachineAssembler& m, MachineType type, int value) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Constant(static_cast<int32_t>(value));
    case MachineRepresentation::kWord64:
      return m.Int64Constant(static_cast<int64_t>(value));
    case MachineRepresentation::kFloat32:
      return m.Float32Constant(static_cast<float>(value));
    case MachineRepresentation::kFloat64:
      return m.Float64Constant(static_cast<double>(value));
    default:
      UNREACHABLE();
  }
}

Node* Add(RawMachineAssembler& m, MachineType type, Node* a, Node* b) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Add(a, b);
    case MachineRepresentation::kWord64:
      return m.Int64Add(a, b);
    case MachineRepresentation::kFloat32:
      return m.Float32Add(a, b);
    case MachineRepresentation::kFloat64:
      return m.Float64Add(a, b);
    default:
      UNREACHABLE();
  }
}

Node* Sub(RawMachineAssembler& m, MachineType type, Node* a, Node* b) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Sub(a, b);
    case MachineRepresentation::kWord64:
      return m.Int64Sub(a, b);
    case MachineRepresentation::kFloat32:
      return m.Float32Sub(a, b);
    case MachineRepresentation::kFloat64:
      return m.Float64Sub(a, b);
    default:
      UNREACHABLE();
  }
}

Node* Mul(RawMachineAssembler& m, MachineType type, Node* a, Node* b) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Mul(a, b);
    case MachineRepresentation::kWord64:
      return m.Int64Mul(a, b);
    case MachineRepresentation::kFloat32:
      return m.Float32Mul(a, b);
    case MachineRepresentation::kFloat64:
      return m.Float64Mul(a, b);
    default:
      UNREACHABLE();
  }
}

Node* ToInt32(RawMachineAssembler& m, MachineType type, Node* a) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return a;
    case MachineRepresentation::kWord64:
      return m.TruncateInt64ToInt32(a);
    case MachineRepresentation::kFloat32:
      return m.TruncateFloat32ToInt32(a);
    case MachineRepresentation::kFloat64:
      return m.RoundFloat64ToInt32(a);
    default:
      UNREACHABLE();
  }
}

std::unique_ptr<wasm::NativeModule> AllocateNativeModule(Isolate* isolate,
                                                         size_t code_size) {
  // We have to add the code object to a NativeModule, because the
  // WasmCallDescriptor assumes that code is on the native heap and not
  // within a code object.
  std::unique_ptr<wasm::NativeModule> module =
      isolate->wasm_engine()->code_manager()->NewNativeModule(code_size, 1, 0,
                                                              false);
  return module;
}

void TestReturnMultipleValues(MachineType type) {
  const int kMaxCount = 20;
  for (int count = 0; count < kMaxCount; ++count) {
    printf("\n==== type = %s, count = %d ====\n\n\n",
           MachineReprToString(type.representation()), count);
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    CallDescriptor* desc = CreateCallDescriptor(&zone, count, 2, type);
    HandleAndZoneScope handles;
    RawMachineAssembler m(handles.main_isolate(),
                          new (handles.main_zone()) Graph(handles.main_zone()),
                          desc, MachineType::PointerRepresentation(),
                          InstructionSelector::SupportedMachineOperatorFlags());

    // m.Parameter(0) is the WasmContext.
    Node* p0 = m.Parameter(1);
    Node* p1 = m.Parameter(2);
    typedef Node* Node_ptr;
    std::unique_ptr<Node_ptr[]> returns(new Node_ptr[count]);
    for (int i = 0; i < count; ++i) {
      if (i % 3 == 0) returns[i] = Add(m, type, p0, p1);
      if (i % 3 == 1) returns[i] = Sub(m, type, p0, p1);
      if (i % 3 == 2) returns[i] = Mul(m, type, p0, p1);
    }
    m.Return(count, returns.get());

    OptimizedCompilationInfo info(ArrayVector("testing"), handles.main_zone(),
                                  Code::STUB);
    Handle<Code> code = Pipeline::GenerateCodeForTesting(
        &info, handles.main_isolate(), desc, m.graph(), m.Export());
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code) {
      OFStream os(stdout);
      code->Disassemble("multi_value", os);
    }
#endif

    const int a = 47, b = 12;
    int expect = 0;
    for (int i = 0, sign = +1; i < count; ++i) {
      if (i % 3 == 0) expect += sign * (a + b);
      if (i % 3 == 1) expect += sign * (a - b);
      if (i % 3 == 2) expect += sign * (a * b);
      if (i % 4 == 0) sign = -sign;
    }

    std::unique_ptr<wasm::NativeModule> module = AllocateNativeModule(
        handles.main_isolate(), code->raw_instruction_size());
    byte* code_start = module->AddCodeCopy(code, wasm::WasmCode::kFunction, 0)
                           ->instructions()
                           .start();

    RawMachineAssemblerTester<int32_t> mt;
    Node* call_inputs[] = {mt.PointerConstant(code_start),
                           // WasmContext dummy
                           mt.PointerConstant(nullptr),
                           // Inputs
                           Constant(mt, type, a), Constant(mt, type, b)};

    Node* ret_multi = mt.AddNode(mt.common()->Call(desc),
                                 arraysize(call_inputs), call_inputs);
    Node* ret = Constant(mt, type, 0);
    bool sign = false;
    for (int i = 0; i < count; ++i) {
      Node* x = (count == 1)
                    ? ret_multi
                    : mt.AddNode(mt.common()->Projection(i), ret_multi);
      ret = sign ? Sub(mt, type, ret, x) : Add(mt, type, ret, x);
      if (i % 4 == 0) sign = !sign;
    }
    mt.Return(ToInt32(mt, type, ret));
#ifdef ENABLE_DISASSEMBLER
    Handle<Code> code2 = mt.GetCode();
    if (FLAG_print_code) {
      OFStream os(stdout);
      code2->Disassemble("multi_value_call", os);
    }
#endif
    CHECK_EQ(expect, mt.Call());
  }
}

#define TEST_MULTI(Type, type) \
  TEST(ReturnMultiple##Type) { TestReturnMultipleValues(type); }

TEST_MULTI(Int32, MachineType::Int32())
#if (!V8_TARGET_ARCH_32_BIT)
TEST_MULTI(Int64, MachineType::Int64())
#endif
TEST_MULTI(Float32, MachineType::Float32())
TEST_MULTI(Float64, MachineType::Float64())

#undef TEST_MULTI

void ReturnLastValue(MachineType type) {
  int slot_counts[] = {1, 2, 3, 600};
  for (auto slot_count : slot_counts) {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    // The wasm-linkage provides 2 return registers at the moment, on all
    // platforms.
    const int return_count = 2 + slot_count;

    CallDescriptor* desc = CreateCallDescriptor(&zone, return_count, 0, type);

    HandleAndZoneScope handles;
    RawMachineAssembler m(handles.main_isolate(),
                          new (handles.main_zone()) Graph(handles.main_zone()),
                          desc, MachineType::PointerRepresentation(),
                          InstructionSelector::SupportedMachineOperatorFlags());

    std::unique_ptr<Node* []> returns(new Node*[return_count]);

    for (int i = 0; i < return_count; ++i) {
      returns[i] = Constant(m, type, i);
    }

    m.Return(return_count, returns.get());

    OptimizedCompilationInfo info(ArrayVector("testing"), handles.main_zone(),
                                  Code::STUB);
    Handle<Code> code = Pipeline::GenerateCodeForTesting(
        &info, handles.main_isolate(), desc, m.graph(), m.Export());

    std::unique_ptr<wasm::NativeModule> module = AllocateNativeModule(
        handles.main_isolate(), code->raw_instruction_size());
    byte* code_start = module->AddCodeCopy(code, wasm::WasmCode::kFunction, 0)
                           ->instructions()
                           .start();

    // Generate caller.
    int expect = return_count - 1;
    RawMachineAssemblerTester<int32_t> mt;
    Node* inputs[] = {mt.PointerConstant(code_start),
                      // WasmContext dummy
                      mt.PointerConstant(nullptr)};

    Node* call = mt.AddNode(mt.common()->Call(desc), 2, inputs);

    mt.Return(ToInt32(
        mt, type, mt.AddNode(mt.common()->Projection(return_count - 1), call)));

    CHECK_EQ(expect, mt.Call());
  }
}

TEST(ReturnLastValueInt32) { ReturnLastValue(MachineType::Int32()); }
#if (!V8_TARGET_ARCH_32_BIT)
TEST(ReturnLastValueInt64) { ReturnLastValue(MachineType::Int64()); }
#endif
TEST(ReturnLastValueFloat32) { ReturnLastValue(MachineType::Float32()); }
TEST(ReturnLastValueFloat64) { ReturnLastValue(MachineType::Float64()); }

void ReturnSumOfReturns(MachineType type) {
  for (int unused_stack_slots = 0; unused_stack_slots <= 2;
       ++unused_stack_slots) {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    // Let {unused_stack_slots + 1} returns be on the stack.
    // The wasm-linkage provides 2 return registers at the moment, on all
    // platforms.
    const int return_count = 2 + unused_stack_slots + 1;

    CallDescriptor* desc = CreateCallDescriptor(&zone, return_count, 0, type);

    HandleAndZoneScope handles;
    RawMachineAssembler m(handles.main_isolate(),
                          new (handles.main_zone()) Graph(handles.main_zone()),
                          desc, MachineType::PointerRepresentation(),
                          InstructionSelector::SupportedMachineOperatorFlags());

    std::unique_ptr<Node* []> returns(new Node*[return_count]);

    for (int i = 0; i < return_count; ++i) {
      returns[i] = Constant(m, type, i);
    }

    m.Return(return_count, returns.get());

    OptimizedCompilationInfo info(ArrayVector("testing"), handles.main_zone(),
                                  Code::STUB);
    Handle<Code> code = Pipeline::GenerateCodeForTesting(
        &info, handles.main_isolate(), desc, m.graph(), m.Export());

    std::unique_ptr<wasm::NativeModule> module = AllocateNativeModule(
        handles.main_isolate(), code->raw_instruction_size());
    byte* code_start = module->AddCodeCopy(code, wasm::WasmCode::kFunction, 0)
                           ->instructions()
                           .start();

    // Generate caller.
    RawMachineAssemblerTester<int32_t> mt;
    Node* call_inputs[] = {mt.PointerConstant(code_start),
                           // WasmContext dummy
                           mt.PointerConstant(nullptr)};

    Node* call = mt.AddNode(mt.common()->Call(desc), 2, call_inputs);

    uint32_t expect = 0;
    Node* result = mt.Int32Constant(0);

    for (int i = 0; i < return_count; ++i) {
      expect += i;
      result = mt.Int32Add(
          result,
          ToInt32(mt, type, mt.AddNode(mt.common()->Projection(i), call)));
    }

    mt.Return(result);

    CHECK_EQ(expect, mt.Call());
  }
}

TEST(ReturnSumOfReturnsInt32) { ReturnSumOfReturns(MachineType::Int32()); }
#if (!V8_TARGET_ARCH_32_BIT)
TEST(ReturnSumOfReturnsInt64) { ReturnSumOfReturns(MachineType::Int64()); }
#endif
TEST(ReturnSumOfReturnsFloat32) { ReturnSumOfReturns(MachineType::Float32()); }
TEST(ReturnSumOfReturnsFloat64) { ReturnSumOfReturns(MachineType::Float64()); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
