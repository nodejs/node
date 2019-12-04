// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/code-factory.h"
#include "src/compiler/code-assembler.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/execution/isolate.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

using Label = CodeAssemblerLabel;
using Variable = CodeAssemblerVariable;

Node* SmiTag(CodeAssembler& m,  // NOLINT(runtime/references)
             Node* value) {
  int32_t constant_value;
  if (m.ToInt32Constant(value, &constant_value) &&
      Smi::IsValid(constant_value)) {
    return m.SmiConstant(Smi::FromInt(constant_value));
  }
  return m.WordShl(value, m.IntPtrConstant(kSmiShiftSize + kSmiTagSize));
}

Node* UndefinedConstant(CodeAssembler& m) {  // NOLINT(runtime/references)
  return m.LoadRoot(RootIndex::kUndefinedValue);
}

Node* SmiFromInt32(CodeAssembler& m,  // NOLINT(runtime/references)
                   Node* value) {
  value = m.ChangeInt32ToIntPtr(value);
  return m.BitcastWordToTaggedSigned(
      m.WordShl(value, kSmiShiftSize + kSmiTagSize));
}

Node* LoadObjectField(CodeAssembler& m,  // NOLINT(runtime/references)
                      Node* object, int offset,
                      MachineType type = MachineType::AnyTagged()) {
  return m.Load(type, object, m.IntPtrConstant(offset - kHeapObjectTag));
}

Node* LoadMap(CodeAssembler& m,  // NOLINT(runtime/references)
              Node* object) {
  return LoadObjectField(m, object, JSObject::kMapOffset);
}

}  // namespace

TEST(SimpleSmiReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  m.Return(SmiTag(m, m.Int32Constant(37)));
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK_EQ(37, ft.CallChecked<Smi>()->value());
}

TEST(SimpleIntPtrReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  int test;
  m.Return(m.BitcastWordToTagged(
      m.IntPtrConstant(reinterpret_cast<intptr_t>(&test))));
  FunctionTester ft(asm_tester.GenerateCode());
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(reinterpret_cast<Address>(&test), result.ToHandleChecked()->ptr());
}

TEST(SimpleDoubleReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  m.Return(m.NumberConstant(0.5));
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK_EQ(0.5, ft.CallChecked<HeapNumber>()->value());
}

TEST(SimpleCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = SmiTag(m, m.Int32Constant(0));
  m.Return(m.CallRuntime(Runtime::kIsSmi, context, b));
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK(ft.CallChecked<Oddball>().is_identical_to(
      isolate->factory()->true_value()));
}

TEST(SimpleTailCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = SmiTag(m, m.Int32Constant(0));
  m.TailCallRuntime(Runtime::kIsSmi, context, b);
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK(ft.CallChecked<Oddball>().is_identical_to(
      isolate->factory()->true_value()));
}

TEST(SimpleCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = SmiTag(m, m.Int32Constant(2));
  Node* b = SmiTag(m, m.Int32Constant(4));
  m.Return(m.CallRuntime(Runtime::kAdd, context, a, b));
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK_EQ(6, ft.CallChecked<Smi>()->value());
}

TEST(SimpleTailCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = SmiTag(m, m.Int32Constant(2));
  Node* b = SmiTag(m, m.Int32Constant(4));
  m.TailCallRuntime(Runtime::kAdd, context, a, b);
  FunctionTester ft(asm_tester.GenerateCode());
  CHECK_EQ(6, ft.CallChecked<Smi>()->value());
}

namespace {

Handle<JSFunction> CreateSumAllArgumentsFunction(
    FunctionTester& ft) {  // NOLINT(runtime/references)
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
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());
  {
    Node* function = m.Parameter(0);
    Node* context = m.Parameter(kNumParams + 2);

    Node* receiver = SmiTag(m, m.Int32Constant(42));

    Callable callable = CodeFactory::Call(isolate);
    Node* result = m.CallJS(callable, context, function, receiver);
    m.Return(result);
  }
  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(42), *result.ToHandleChecked());
}

TEST(SimpleCallJSFunction1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());
  {
    Node* function = m.Parameter(0);
    Node* context = m.Parameter(1);

    Node* receiver = SmiTag(m, m.Int32Constant(42));
    Node* a = SmiTag(m, m.Int32Constant(13));

    Callable callable = CodeFactory::Call(isolate);
    Node* result = m.CallJS(callable, context, function, receiver, a);
    m.Return(result);
  }
  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(55), *result.ToHandleChecked());
}

TEST(SimpleCallJSFunction2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());
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
  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);

  Handle<JSFunction> sum = CreateSumAllArgumentsFunction(ft);
  MaybeHandle<Object> result = ft.Call(sum);
  CHECK_EQ(Smi::FromInt(208), *result.ToHandleChecked());
}

TEST(VariableMerge1) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Variable var1(&m, MachineRepresentation::kTagged);
  Label l1(&m), l2(&m), merge(&m);
  TNode<Int32T> temp = m.Int32Constant(0);
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
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Variable var1(&m, MachineRepresentation::kTagged);
  Label l1(&m), l2(&m), merge(&m);
  TNode<Int32T> temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&l2);
  TNode<Int32T> temp2 = m.Int32Constant(2);
  var1.Bind(temp2);
  CHECK_EQ(var1.value(), temp2);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK_NE(var1.value(), temp);
}

TEST(VariableMerge3) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Variable var1(&m, MachineRepresentation::kTagged);
  Variable var2(&m, MachineRepresentation::kTagged);
  Label l1(&m), l2(&m), merge(&m);
  TNode<Int32T> temp = m.Int32Constant(0);
  var1.Bind(temp);
  var2.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&l2);
  TNode<Int32T> temp2 = m.Int32Constant(2);
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
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Variable var1(&m, MachineRepresentation::kTagged);
  Label l1(&m), l2(&m), merge(&m, &var1), end(&m);
  TNode<Int32T> temp = m.Int32Constant(0);
  var1.Bind(temp);
  m.Branch(m.Int32Constant(1), &l1, &l2);
  m.Bind(&l1);
  CHECK_EQ(var1.value(), temp);
  m.Goto(&merge);
  m.Bind(&merge);
  CHECK(var1.value() != temp);
  CHECK_NOT_NULL(var1.value());
  m.Goto(&end);
  m.Bind(&l2);
  TNode<Int32T> temp2 = m.Int32Constant(2);
  var1.Bind(temp2);
  CHECK_EQ(var1.value(), temp2);
  m.Goto(&merge);
  m.Bind(&end);
  CHECK(var1.value() != temp);
  CHECK_NOT_NULL(var1.value());
}

TEST(VariableMergeSwitch) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Variable var1(&m, MachineRepresentation::kTagged);
  Label l1(&m), l2(&m), default_label(&m);
  Label* labels[] = {&l1, &l2};
  int32_t values[] = {1, 2};
  TNode<Smi> temp1 = m.SmiConstant(0);
  var1.Bind(temp1);
  m.Switch(m.Int32Constant(2), &default_label, values, labels, 2);
  m.Bind(&l1);
  CHECK_EQ(temp1, var1.value());
  m.Return(temp1);
  m.Bind(&l2);
  CHECK_EQ(temp1, var1.value());
  TNode<Smi> temp2 = m.SmiConstant(7);
  var1.Bind(temp2);
  m.Goto(&default_label);
  m.Bind(&default_label);
  CHECK_EQ(IrOpcode::kPhi, var1.value()->opcode());
  CHECK_EQ(2, var1.value()->op()->ValueInputCount());
  CHECK_EQ(temp1, NodeProperties::GetValueInput(var1.value(), 0));
  CHECK_EQ(temp2, NodeProperties::GetValueInput(var1.value(), 1));
  m.Return(temp1);
}

TEST(SplitEdgeBranchMerge) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Label l1(&m), merge(&m);
  m.Branch(m.Int32Constant(1), &l1, &merge);
  m.Bind(&l1);
  m.Goto(&merge);
  m.Bind(&merge);
  USE(asm_tester.GenerateCode());
}

TEST(SplitEdgeSwitchMerge) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Label l1(&m), l2(&m), l3(&m), default_label(&m);
  Label* labels[] = {&l1, &l2};
  int32_t values[] = {1, 2};
  m.Branch(m.Int32Constant(1), &l3, &l1);
  m.Bind(&l3);
  m.Switch(m.Int32Constant(2), &default_label, values, labels, 2);
  m.Bind(&l1);
  m.Goto(&l2);
  m.Bind(&l2);
  m.Goto(&default_label);
  m.Bind(&default_label);
  USE(asm_tester.GenerateCode());
}

TEST(TestToConstant) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  int32_t value32;
  int64_t value64;
  Node* a = m.Int32Constant(5);
  CHECK(m.ToInt32Constant(a, &value32));
  CHECK(m.ToInt64Constant(a, &value64));

  a = m.Int64Constant(static_cast<int64_t>(1) << 32);
  CHECK(!m.ToInt32Constant(a, &value32));
  CHECK(m.ToInt64Constant(a, &value64));

  a = m.Int64Constant(13);
  CHECK(m.ToInt32Constant(a, &value32));
  CHECK(m.ToInt64Constant(a, &value64));

  a = UndefinedConstant(m);
  CHECK(!m.ToInt32Constant(a, &value32));
  CHECK(!m.ToInt64Constant(a, &value64));

  a = UndefinedConstant(m);
  CHECK(!m.ToInt32Constant(a, &value32));
  CHECK(!m.ToInt64Constant(a, &value64));
}

TEST(DeferredCodePhiHints) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Label block1(&m, Label::kDeferred);
  m.Goto(&block1);
  m.Bind(&block1);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    Label loop(&m, &var_object);
    var_object.Bind(m.SmiConstant(0));
    m.Goto(&loop);
    m.Bind(&loop);
    {
      Node* map = LoadMap(m, var_object.value());
      var_object.Bind(map);
      m.Goto(&loop);
    }
  }
  CHECK(!asm_tester.GenerateCode().is_null());
}

TEST(TestOutOfScopeVariable) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  Label block1(&m);
  Label block2(&m);
  Label block3(&m);
  Label block4(&m);
  m.Branch(m.WordEqual(m.UncheckedCast<IntPtrT>(m.Parameter(0)),
                       m.IntPtrConstant(0)),
           &block1, &block4);
  m.Bind(&block4);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    m.Branch(m.WordEqual(m.UncheckedCast<IntPtrT>(m.Parameter(0)),
                         m.IntPtrConstant(0)),
             &block2, &block3);

    m.Bind(&block2);
    var_object.Bind(m.IntPtrConstant(55));
    m.Goto(&block1);

    m.Bind(&block3);
    var_object.Bind(m.IntPtrConstant(66));
    m.Goto(&block1);
  }
  m.Bind(&block1);
  CHECK(!asm_tester.GenerateCode().is_null());
}

TEST(GotoIfException) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());

  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  TNode<Symbol> to_string_tag =
      m.HeapConstant(isolate->factory()->to_string_tag_symbol());
  Variable exception(&m, MachineRepresentation::kTagged);

  Label exception_handler(&m);
  Callable to_string = Builtins::CallableFor(isolate, Builtins::kToString);
  TNode<Object> string = m.CallStub(to_string, context, to_string_tag);
  m.GotoIfException(string, &exception_handler, &exception);
  m.Return(string);

  m.Bind(&exception_handler);
  m.Return(exception.value());

  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);
  Handle<Object> result = ft.Call().ToHandleChecked();

  // Should be a TypeError.
  CHECK(result->IsJSObject());

  Handle<Object> constructor =
      Object::GetPropertyOrElement(isolate, result,
                                   isolate->factory()->constructor_string())
          .ToHandleChecked();
  CHECK(constructor->SameValue(*isolate->type_error_function()));
}

TEST(GotoIfExceptionMultiple) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;  // receiver, first, second, third
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());

  TNode<Context> context =
      m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* first_value = m.Parameter(0);
  Node* second_value = m.Parameter(1);
  Node* third_value = m.Parameter(2);

  Label exception_handler1(&m);
  Label exception_handler2(&m);
  Label exception_handler3(&m);
  Variable return_value(&m, MachineRepresentation::kWord32);
  Variable error(&m, MachineRepresentation::kTagged);

  return_value.Bind(m.Int32Constant(0));

  // try { return ToString(param1) } catch (e) { ... }
  Callable to_string = Builtins::CallableFor(isolate, Builtins::kToString);
  TNode<Object> string = m.CallStub(to_string, context, first_value);
  m.GotoIfException(string, &exception_handler1, &error);
  m.Return(string);

  // try { ToString(param2); return 7 } catch (e) { ... }
  m.Bind(&exception_handler1);
  return_value.Bind(m.Int32Constant(7));
  error.Bind(UndefinedConstant(m));
  string = m.CallStub(to_string, context, second_value);
  m.GotoIfException(string, &exception_handler2, &error);
  m.Return(SmiFromInt32(m, return_value.value()));

  // try { ToString(param3); return 7 & ~2; } catch (e) { return e; }
  m.Bind(&exception_handler2);
  // Return returnValue & ~2
  error.Bind(UndefinedConstant(m));
  string = m.CallStub(to_string, context, third_value);
  m.GotoIfException(string, &exception_handler3, &error);
  m.Return(SmiFromInt32(
      m, m.Word32And(return_value.value(),
                     m.Word32Xor(m.Int32Constant(2), m.Int32Constant(-1)))));

  m.Bind(&exception_handler3);
  m.Return(error.value());

  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);

  Handle<Object> result;
  // First handler does not throw, returns result of first value.
  result = ft.Call(isolate->factory()->undefined_value(),
                   isolate->factory()->to_string_tag_symbol())
               .ToHandleChecked();
  CHECK(String::cast(*result).IsOneByteEqualTo(OneByteVector("undefined")));

  // First handler returns a number.
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->undefined_value())
               .ToHandleChecked();
  CHECK_EQ(7, Smi::ToInt(*result));

  // First handler throws, second handler returns a number.
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->to_primitive_symbol())
               .ToHandleChecked();
  CHECK_EQ(7 & ~2, Smi::ToInt(*result));

  // First handler throws, second handler throws, third handler returns thrown
  // value.
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->to_primitive_symbol(),
                   isolate->factory()->unscopables_symbol())
               .ToHandleChecked();

  // Should be a TypeError.
  CHECK(result->IsJSObject());

  Handle<Object> constructor =
      Object::GetPropertyOrElement(isolate, result,
                                   isolate->factory()->constructor_string())
          .ToHandleChecked();
  CHECK(constructor->SameValue(*isolate->type_error_function()));
}

TEST(ExceptionHandler) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());

  CodeAssembler::TVariable<Object> var(m.SmiConstant(0), &m);
  Label exception(&m, {&var}, Label::kDeferred);
  {
    CodeAssemblerScopedExceptionHandler handler(&m, &exception, &var);
    TNode<Context> context =
        m.HeapConstant(Handle<Context>(isolate->native_context()));
    m.CallRuntime(Runtime::kThrow, context, m.SmiConstant(2));
  }
  m.Return(m.SmiConstant(1));

  m.Bind(&exception);
  m.Return(var.value());

  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);
  CHECK_EQ(2, ft.CallChecked<Smi>()->value());
}

TEST(TestCodeAssemblerCodeComment) {
  i::FLAG_code_comments = true;
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  CodeAssembler m(asm_tester.state());

  m.Comment("Comment1");
  m.Return(m.SmiConstant(1));

  Handle<Code> code = asm_tester.GenerateCode();
  CHECK_NE(code->code_comments(), kNullAddress);
  CodeCommentsIterator it(code->code_comments(), code->code_comments_size());
  CHECK(it.HasCurrent());
  bool found_comment = false;
  while (it.HasCurrent()) {
    if (strcmp(it.GetComment(), "Comment1") == 0) found_comment = true;
    it.Next();
  }
  CHECK(found_comment);
}

TEST(StaticAssert) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate);
  CodeAssembler m(asm_tester.state());
  m.StaticAssert(m.ReinterpretCast<BoolT>(m.Int32Constant(1)));
  USE(asm_tester.GenerateCode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
