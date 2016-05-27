// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {


class CodeStubAssemblerTester : public CodeStubAssembler {
 public:
  // Test generating code for a stub.
  CodeStubAssemblerTester(Isolate* isolate,
                          const CallInterfaceDescriptor& descriptor)
      : CodeStubAssembler(isolate, isolate->runtime_zone(), descriptor,
                          Code::ComputeFlags(Code::STUB), "test"),
        scope_(isolate) {}

  // Test generating code for a JS function (e.g. builtins).
  CodeStubAssemblerTester(Isolate* isolate, int parameter_count)
      : CodeStubAssembler(isolate, isolate->runtime_zone(), parameter_count,
                          Code::ComputeFlags(Code::FUNCTION), "test"),
        scope_(isolate) {}

 private:
  HandleScope scope_;
  LocalContext context_;
};


TEST(SimpleSmiReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  m.Return(m.SmiTag(m.Int32Constant(37)));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(37, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleIntPtrReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  int test;
  m.Return(m.IntPtrConstant(reinterpret_cast<intptr_t>(&test)));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(reinterpret_cast<intptr_t>(&test),
           reinterpret_cast<intptr_t>(*result.ToHandleChecked()));
}


TEST(SimpleDoubleReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  m.Return(m.NumberConstant(0.5));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0.5, Handle<HeapNumber>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = m.SmiTag(m.Int32Constant(0));
  m.Return(m.CallRuntime(Runtime::kNumberToSmi, context, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleTailCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = m.SmiTag(m.Int32Constant(0));
  m.TailCallRuntime(Runtime::kNumberToSmi, context, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = m.SmiTag(m.Int32Constant(2));
  Node* b = m.SmiTag(m.Int32Constant(4));
  m.Return(m.CallRuntime(Runtime::kMathPow, context, a, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleTailCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = m.SmiTag(m.Int32Constant(2));
  Node* b = m.SmiTag(m.Int32Constant(4));
  m.TailCallRuntime(Runtime::kMathPow, context, a, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(VariableMerge1) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Variable var1(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Label l1(&m), l2(&m), merge(&m);
  Node* temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&l2);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK_EQ(var1.value(), temp);
}

TEST(VariableMerge2) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Variable var1(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Label l1(&m), l2(&m), merge(&m);
  Node* temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&l2);
  Node* temp2 = m.Int32Constant(2);
  var1.Bind(temp2);
  CHECK_EQ(var1.value(), temp2);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK_NE(var1.value(), temp);
}

TEST(VariableMerge3) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Variable var1(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Variable var2(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Label l1(&m), l2(&m), merge(&m);
  Node* temp = m.Int32Constant(0);
  var1.Bind(temp);
  var2.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&l2);
  Node* temp2 = m.Int32Constant(2);
  var1.Bind(temp2);
  CHECK_EQ(var1.value(), temp2);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK_NE(var1.value(), temp);
  CHECK_NE(var1.value(), temp2);
  CHECK_EQ(var2.value(), temp);
}

TEST(VariableMergeBindFirst) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Variable var1(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Label l1(&m), l2(&m), merge(&m, &var1), end(&m);
  Node* temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK(var1.value() != temp);
  CHECK(var1.value() != nullptr);
  m.Goto(&end);
  m.Bind(&l2);
  Node* temp2 = m.Int32Constant(2);
  var1.Bind(temp2);
  CHECK_EQ(var1.value(), temp2);
  m.Goto(&merge);
  m.Bind(&end);
  CHECK(var1.value() != temp);
  CHECK(var1.value() != nullptr);
}

TEST(VariableMergeSwitch) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Variable var1(&m, MachineRepresentation::kTagged);
  CodeStubAssembler::Label l1(&m), l2(&m), default_label(&m);
  CodeStubAssembler::Label* labels[] = {&l1, &l2};
  int32_t values[] = {1, 2};
  Node* temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Switch(m.Int32Constant(2), &default_label, values, labels, 2);
  m.Bind(&l1);
  DCHECK_EQ(temp, var1.value());
  m.Return(temp);
  m.Bind(&l2);
  DCHECK_EQ(temp, var1.value());
  m.Return(temp);
  m.Bind(&default_label);
  DCHECK_EQ(temp, var1.value());
  m.Return(temp);
}

TEST(FixedArrayAccessSmiIndex) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(5);
  array->set(4, Smi::FromInt(733));
  m.Return(m.LoadFixedArrayElementSmiIndex(m.HeapConstant(array),
                                           m.SmiTag(m.Int32Constant(4))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(733, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadHeapNumberValue) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(1234);
  m.Return(m.SmiTag(
      m.ChangeFloat64ToUint32(m.LoadHeapNumberValue(m.HeapConstant(number)))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(1234, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadInstanceType) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<HeapObject> undefined = isolate->factory()->undefined_value();
  m.Return(m.SmiTag(m.LoadInstanceType(m.HeapConstant(undefined))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(InstanceType::ODDBALL_TYPE,
           Handle<Smi>::cast(result.ToHandleChecked())->value());
}

namespace {

class TestBitField : public BitField<unsigned, 3, 3> {};

}  // namespace

TEST(BitFieldDecode) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  m.Return(m.SmiTag(m.BitFieldDecode<TestBitField>(m.Int32Constant(0x2f))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  // value  = 00101111
  // mask   = 00111000
  // result = 101
  CHECK_EQ(5, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

namespace {

Handle<JSFunction> CreateFunctionFromCode(int parameter_count_with_receiver,
                                          Handle<Code> code) {
  Isolate* isolate = code->GetIsolate();
  Handle<String> name = isolate->factory()->InternalizeUtf8String("test");
  Handle<JSFunction> function =
      isolate->factory()->NewFunctionWithoutPrototype(name, code);
  function->shared()->set_internal_formal_parameter_count(
      parameter_count_with_receiver - 1);  // Implicit undefined receiver.
  return function;
}

}  // namespace

TEST(JSFunction) {
  const int kNumParams = 3;  // Receiver, left, right.
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeStubAssemblerTester m(isolate, kNumParams);
  m.Return(m.SmiTag(m.Int32Add(m.SmiToWord32(m.Parameter(1)),
                               m.SmiToWord32(m.Parameter(2)))));
  Handle<Code> code = m.GenerateCode();
  Handle<JSFunction> function = CreateFunctionFromCode(kNumParams, code);
  Handle<Object> args[] = {Handle<Smi>(Smi::FromInt(23), isolate),
                           Handle<Smi>(Smi::FromInt(34), isolate)};
  MaybeHandle<Object> result =
      Execution::Call(isolate, function, isolate->factory()->undefined_value(),
                      arraysize(args), args);
  CHECK_EQ(57, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(SplitEdgeBranchMerge) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Label l1(&m), merge(&m);
  m.Branch(m.Int32Constant(1), &l1, &merge);
  m.Bind(&l1);
  m.Goto(&merge);
  m.Bind(&merge);
  USE(m.GenerateCode());
}

TEST(SplitEdgeSwitchMerge) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  CodeStubAssembler::Label l1(&m), l2(&m), l3(&m), default_label(&m);
  CodeStubAssembler::Label* labels[] = {&l1, &l2};
  int32_t values[] = {1, 2};
  m.Branch(m.Int32Constant(1), &l3, &l1);
  m.Bind(&l3);
  m.Switch(m.Int32Constant(2), &default_label, values, labels, 2);
  m.Bind(&l1);
  m.Goto(&l2);
  m.Bind(&l2);
  m.Goto(&default_label);
  m.Bind(&default_label);
  USE(m.GenerateCode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
