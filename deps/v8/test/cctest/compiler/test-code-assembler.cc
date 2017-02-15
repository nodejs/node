// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/compiler/code-assembler.h"
#include "src/isolate.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef CodeAssemblerTesterImpl<CodeAssembler> CodeAssemblerTester;

namespace {

Node* SmiTag(CodeAssemblerTester& m, Node* value) {
  int32_t constant_value;
  if (m.ToInt32Constant(value, constant_value) &&
      Smi::IsValid(constant_value)) {
    return m.SmiConstant(Smi::FromInt(constant_value));
  }
  return m.WordShl(value, m.IntPtrConstant(kSmiShiftSize + kSmiTagSize));
}

Node* UndefinedConstant(CodeAssemblerTester& m) {
  return m.LoadRoot(Heap::kUndefinedValueRootIndex);
}

Node* LoadObjectField(CodeAssemblerTester& m, Node* object, int offset,
                      MachineType rep = MachineType::AnyTagged()) {
  return m.Load(rep, object, m.IntPtrConstant(offset - kHeapObjectTag));
}

}  // namespace

TEST(SimpleSmiReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  m.Return(SmiTag(m, m.Int32Constant(37)));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(37, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(SimpleIntPtrReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
  m.Return(m.NumberConstant(0.5));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0.5, Handle<HeapNumber>::cast(result.ToHandleChecked())->value());
}

TEST(SimpleCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = SmiTag(m, m.Int32Constant(0));
  m.Return(m.CallRuntime(Runtime::kNumberToSmi, context, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(SimpleTailCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = SmiTag(m, m.Int32Constant(0));
  m.TailCallRuntime(Runtime::kNumberToSmi, context, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(SimpleCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = SmiTag(m, m.Int32Constant(2));
  Node* b = SmiTag(m, m.Int32Constant(4));
  m.Return(m.CallRuntime(Runtime::kAdd, context, a, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(6, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(SimpleTailCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = SmiTag(m, m.Int32Constant(2));
  Node* b = SmiTag(m, m.Int32Constant(4));
  m.TailCallRuntime(Runtime::kAdd, context, a, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(6, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

namespace {

Handle<JSFunction> CreateSumAllArgumentsFunction(FunctionTester& ft) {
  const char* source =
      "(function() {\n"
      "  var sum = 0 + this;\n"
      "  for (var i = 0; i < arguments.length; i++) {\n"
      "    sum += arguments[i];\n"
      "  }\n"
      "  return sum;\n"
      "})";
  return ft.NewFunction(source);
}

}  // namespace

TEST(SimpleCallJSFunction0Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 1;
  CodeAssemblerTester m(isolate, kNumParams);
  {
    Node* function = m.Parameter(0);
    Node* context = m.Parameter(kNumParams + 2);

    Node* receiver = SmiTag(m, m.Int32Constant(42));

    Callable callable = CodeFactory::Call(isolate);
    Node* result = m.CallJS(callable, context, function, receiver);
    m.Return(result);
  }
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(42), *result.ToHandleChecked());
}

TEST(SimpleCallJSFunction1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester m(isolate, kNumParams);
  {
    Node* function = m.Parameter(0);
    Node* context = m.Parameter(1);

    Node* receiver = SmiTag(m, m.Int32Constant(42));
    Node* a = SmiTag(m, m.Int32Constant(13));

    Callable callable = CodeFactory::Call(isolate);
    Node* result = m.CallJS(callable, context, function, receiver, a);
    m.Return(result);
  }
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(55), *result.ToHandleChecked());
}

TEST(SimpleCallJSFunction2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester m(isolate, kNumParams);
  {
    Node* function = m.Parameter(0);
    Node* context = m.Parameter(1);

    Node* receiver = SmiTag(m, m.Int32Constant(42));
    Node* a = SmiTag(m, m.Int32Constant(13));
    Node* b = SmiTag(m, m.Int32Constant(153));

    Callable callable = CodeFactory::Call(isolate);
    Node* result = m.CallJS(callable, context, function, receiver, a, b);
    m.Return(result);
  }
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(208), *result.ToHandleChecked());
}

TEST(VariableMerge1) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
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

TEST(SplitEdgeBranchMerge) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
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
  CodeAssemblerTester m(isolate, descriptor);
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

TEST(TestToConstant) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  int32_t value32;
  int64_t value64;
  Node* a = m.Int32Constant(5);
  CHECK(m.ToInt32Constant(a, value32));
  CHECK(m.ToInt64Constant(a, value64));

  a = m.Int64Constant(static_cast<int64_t>(1) << 32);
  CHECK(!m.ToInt32Constant(a, value32));
  CHECK(m.ToInt64Constant(a, value64));

  a = m.Int64Constant(13);
  CHECK(m.ToInt32Constant(a, value32));
  CHECK(m.ToInt64Constant(a, value64));

  a = UndefinedConstant(m);
  CHECK(!m.ToInt32Constant(a, value32));
  CHECK(!m.ToInt64Constant(a, value64));

  a = UndefinedConstant(m);
  CHECK(!m.ToInt32Constant(a, value32));
  CHECK(!m.ToInt64Constant(a, value64));
}

TEST(DeferredCodePhiHints) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Label block1(&m, Label::kDeferred);
  m.Goto(&block1);
  m.Bind(&block1);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    Label loop(&m, &var_object);
    var_object.Bind(m.IntPtrConstant(0));
    m.Goto(&loop);
    m.Bind(&loop);
    {
      Node* map = LoadObjectField(m, var_object.value(), JSObject::kMapOffset);
      var_object.Bind(map);
      m.Goto(&loop);
    }
  }
  CHECK(!m.GenerateCode().is_null());
}

TEST(TestOutOfScopeVariable) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeAssemblerTester m(isolate, descriptor);
  Label block1(&m);
  Label block2(&m);
  Label block3(&m);
  Label block4(&m);
  m.Branch(m.WordEqual(m.Parameter(0), m.IntPtrConstant(0)), &block1, &block4);
  m.Bind(&block4);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    m.Branch(m.WordEqual(m.Parameter(0), m.IntPtrConstant(0)), &block2,
             &block3);

    m.Bind(&block2);
    var_object.Bind(m.IntPtrConstant(55));
    m.Goto(&block1);

    m.Bind(&block3);
    var_object.Bind(m.IntPtrConstant(66));
    m.Goto(&block1);
  }
  m.Bind(&block1);
  CHECK(!m.GenerateCode().is_null());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
