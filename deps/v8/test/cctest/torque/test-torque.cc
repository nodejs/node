// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "src/api-inl.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-promise-gen.h"
#include "src/builtins/builtins-string-gen.h"
#include "src/char-predicates.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/compiler/node.h"
#include "src/debug/debug.h"
#include "src/elements-kind.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/promise-inl.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"
#include "torque-generated/builtins-test-from-dsl-gen.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef CodeAssemblerLabel Label;
typedef CodeAssemblerVariable Variable;

class TestTorqueAssembler : public CodeStubAssembler,
                            public TestBuiltinsFromDSLAssembler {
 public:
  explicit TestTorqueAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state), TestBuiltinsFromDSLAssembler(state) {}
};

}  // namespace

TEST(TestConstexpr1) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexpr1();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestConstexprIf) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexprIf();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestConstexprReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexprReturn();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestGotoLabel) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  { m.Return(m.TestGotoLabel()); }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.CheckCall(ft.true_value());
}

TEST(TestGotoLabelWithOneParameter) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  { m.Return(m.TestGotoLabelWithOneParameter()); }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.CheckCall(ft.true_value());
}

TEST(TestGotoLabelWithTwoParameters) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  { m.Return(m.TestGotoLabelWithTwoParameters()); }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.CheckCall(ft.true_value());
}

TEST(TestPartiallyUnusedLabel) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  { m.Return(m.TestPartiallyUnusedLabel()); }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.CheckCall(ft.true_value());
}

TEST(TestBuiltinSpecialization) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    Node* temp = m.SmiConstant(0);
    m.TestBuiltinSpecialization(m.UncheckedCast<Context>(temp));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestMacroSpecialization) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestMacroSpecialization();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestFunctionPointers) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Context> context =
        m.UncheckedCast<Context>(m.Parameter(kNumParams + 2));
    m.Return(m.TestFunctionPointers(context));
  }
  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST(TestTernaryOperator) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 1;
  CodeAssemblerTester asm_tester(isolate, kNumParams);
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> arg = m.UncheckedCast<Smi>(m.Parameter(0));
    m.Return(m.TestTernaryOperator(arg));
  }
  FunctionTester ft(asm_tester.GenerateCode(), kNumParams);
  Handle<Object> result1 =
      ft.Call(Handle<Smi>(Smi::FromInt(-5), isolate)).ToHandleChecked();
  CHECK_EQ(-15, Handle<Smi>::cast(result1)->value());
  Handle<Object> result2 =
      ft.Call(Handle<Smi>(Smi::FromInt(3), isolate)).ToHandleChecked();
  CHECK_EQ(103, Handle<Smi>::cast(result2)->value());
}

TEST(TestFunctionPointerToGeneric) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    Node* temp = m.SmiConstant(0);
    m.TestFunctionPointerToGeneric(m.UncheckedCast<Context>(temp));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestUnsafeCast) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    Node* temp = m.SmiConstant(0);
    Node* n = m.SmiConstant(10);
    m.Return(m.TestUnsafeCast(m.UncheckedCast<Context>(temp),
                              m.UncheckedCast<Number>(n)));
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.CheckCall(ft.true_value());
}

TEST(TestHexLiteral) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestHexLiteral();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestModuleConstBindings) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestModuleConstBindings();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestLocalConstBindings) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestLocalConstBindings();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestForLoop) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestForLoop();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestTypeswitch) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestTypeswitch(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestGenericOverload) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestGenericOverload(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestLogicalOperators) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestLogicalOperators();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestOtherwiseAndLabels) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestOtherwiseWithCode1();
    m.TestOtherwiseWithCode2();
    m.TestOtherwiseWithCode3();
    m.TestForwardLabel();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestCatch1) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch1(m.UncheckedCast<Context>(m.HeapConstant(context)));
    USE(result);
    CSA_ASSERT(&m, m.WordEqual(result, m.SmiConstant(1)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestCatch2) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch2(m.UncheckedCast<Context>(m.HeapConstant(context)));
    USE(result);
    CSA_ASSERT(&m, m.WordEqual(result, m.SmiConstant(2)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestCatch3) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch3(m.UncheckedCast<Context>(m.HeapConstant(context)));
    USE(result);
    CSA_ASSERT(&m, m.WordEqual(result, m.SmiConstant(2)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestLookup) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate, 0);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestQualifiedAccess(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestFrame1) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestFrame1(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestNew) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestNew(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST(TestStructConstructor) {
  CcTest::InitializeVM();
  Isolate* isolate(CcTest::i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestStructConstructor(m.UncheckedCast<Context>(m.HeapConstant(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
