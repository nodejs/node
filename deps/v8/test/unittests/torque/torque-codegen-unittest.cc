// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "src/api/api-inl.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-promise-gen.h"
#include "src/builtins/builtins-string-gen.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/compiler/node.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/elements-kind.h"
#include "src/objects/objects-inl.h"
#include "src/objects/promise-inl.h"
#include "src/strings/char-predicates.h"
#include "test/common/code-assembler-tester.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace torque_codegen_unittest {

#include "src/codegen/define-code-stub-assembler-macros.inc"

namespace {

using TorqueCodegenTest = TestWithNativeContext;

class TestTorqueAssembler : public CodeStubAssembler {
 public:
  explicit TestTorqueAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state) {}
};

TEST_F(TorqueCodegenTest, TestConstexpr1) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexpr1();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestConstexprIf) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexprIf();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestConstexprReturn) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestConstexprReturn();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestGotoLabel) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestGotoLabel());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestGotoLabelWithOneParameter) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestGotoLabelWithOneParameter());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestGotoLabelWithTwoParameters) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestGotoLabelWithTwoParameters());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestPartiallyUnusedLabel) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestPartiallyUnusedLabel());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestBuiltinSpecialization) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestBuiltinSpecialization();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestMacroSpecialization) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestMacroSpecialization();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestFunctionPointers) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();
    m.Return(m.TestFunctionPointers(context));
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestTernaryOperator) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 1;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> arg = m.Parameter<Smi>(1);
    m.Return(m.TestTernaryOperator(arg));
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  DirectHandle<Object> result1 =
      ft.Call(Handle<Smi>(Smi::FromInt(-5), isolate)).ToHandleChecked();
  CHECK_EQ(-15, Cast<Smi>(*result1).value());
  DirectHandle<Object> result2 =
      ft.Call(Handle<Smi>(Smi::FromInt(3), isolate)).ToHandleChecked();
  CHECK_EQ(103, Cast<Smi>(*result2).value());
}

TEST_F(TorqueCodegenTest, TestFunctionPointerToGeneric) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestFunctionPointerToGeneric();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestUnsafeCast) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Object> temp = m.SmiConstant(0);
    TNode<Smi> n = m.SmiConstant(10);
    m.Return(m.TestUnsafeCast(m.UncheckedCast<Context>(temp),
                              m.UncheckedCast<Number>(n)));
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.CheckCall(ft.true_value());
}

TEST_F(TorqueCodegenTest, TestHexLiteral) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestHexLiteral();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestModuleConstBindings) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestModuleConstBindings();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestLocalConstBindings) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestLocalConstBindings();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestForLoop) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestForLoop();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestTypeswitch) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestTypeswitch(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestGenericOverload) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestGenericOverload(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestEquality) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestEquality(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestLogicalOperators) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestLogicalOperators();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestOtherwiseAndLabels) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestOtherwiseWithCode1();
    m.TestOtherwiseWithCode2();
    m.TestOtherwiseWithCode3();
    m.TestForwardLabel();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestCatch1) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch1(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    USE(result);
    CSA_DCHECK(&m, m.TaggedEqual(result, m.SmiConstant(1)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestCatch2) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch2(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    USE(result);
    CSA_DCHECK(&m, m.TaggedEqual(result, m.SmiConstant(2)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestCatch3) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<Smi> result =
        m.TestCatch3(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    USE(result);
    CSA_DCHECK(&m, m.TaggedEqual(result, m.SmiConstant(2)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestLookup) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestQualifiedAccess(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestFrame1) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestFrame1(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestNew) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestNew(m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestStructConstructor) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestStructConstructor(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestNewFixedArrayFromSpread) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestNewFixedArrayFromSpread(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestSlices) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestSlices();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestSliceEnumeration) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestSliceEnumeration(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestStaticAssert) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestStaticAssert();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestRedundantArrayElementCheck) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestRedundantArrayElementCheck(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context))));
  }
  asm_tester.GenerateCode();
}

TEST_F(TorqueCodegenTest, TestRedundantSmiCheck) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestRedundantSmiCheck(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context))));
  }
  asm_tester.GenerateCode();
}

TEST_F(TorqueCodegenTest, TestGenericStruct1) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestGenericStruct1();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestGenericStruct2) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  CodeAssemblerTester asm_tester(isolate);
  TestTorqueAssembler m(asm_tester.state());
  {
    m.Return(m.TestGenericStruct2().snd.fst);
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), 0);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestBitFieldLoad) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 5;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    // Untag all of the parameters to get plain integer values.
    TNode<Uint8T> val =
        m.UncheckedCast<Uint8T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(1))));
    TNode<BoolT> expected_a =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(2))));
    TNode<Uint16T> expected_b =
        m.UncheckedCast<Uint16T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(3))));
    TNode<Uint32T> expected_c =
        m.UncheckedCast<Uint32T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(4))));
    TNode<BoolT> expected_d =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(5))));

    // Call the Torque-defined macro, which verifies that reading each bitfield
    // out of val yields the correct result.
    m.TestBitFieldLoad(val, expected_a, expected_b, expected_c, expected_d);
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);

  // Test every possible bit combination for this 8-bit value.
  for (int a = 0; a <= 1; ++a) {
    for (int b = 0; b <= 7; ++b) {
      for (int c = 0; c <= 7; ++c) {
        for (int d = 0; d <= 1; ++d) {
          int val = a | ((b & 7) << 1) | (c << 4) | (d << 7);
          ft.Call(ft.NewNumber(val), ft.NewNumber(a), ft.NewNumber(b),
                  ft.NewNumber(c), ft.NewNumber(d));
        }
      }
    }
  }
}

TEST_F(TorqueCodegenTest, TestBitFieldStore) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 1;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    // Untag the parameters to get a plain integer value.
    TNode<Uint8T> val =
        m.UncheckedCast<Uint8T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(1))));

    m.TestBitFieldStore(val);
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);

  // Test every possible bit combination for this 8-bit value.
  for (int i = 0; i < 256; ++i) {
    ft.Call(ft.NewNumber(i));
  }
}

TEST_F(TorqueCodegenTest, TestBitFieldInit) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 4;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    // Untag all of the parameters to get plain integer values.
    TNode<BoolT> a =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(1))));
    TNode<Uint16T> b =
        m.UncheckedCast<Uint16T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(2))));
    TNode<Uint32T> c =
        m.UncheckedCast<Uint32T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(3))));
    TNode<BoolT> d =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(4))));

    // Call the Torque-defined macro, which verifies that reading each bitfield
    // out of val yields the correct result.
    m.TestBitFieldInit(a, b, c, d);
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);

  // Test every possible bit combination for this 8-bit value.
  for (int a = 0; a <= 1; ++a) {
    for (int b = 0; b <= 7; ++b) {
      for (int c = 0; c <= 7; ++c) {
        for (int d = 0; d <= 1; ++d) {
          ft.Call(ft.NewNumber(a), ft.NewNumber(b), ft.NewNumber(c),
                  ft.NewNumber(d));
        }
      }
    }
  }
}

TEST_F(TorqueCodegenTest, TestBitFieldUintptrOps) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 2;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    // Untag the parameters to get a plain integer value.
    TNode<Uint32T> val2 =
        m.UncheckedCast<Uint32T>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(1))));
    TNode<UintPtrT> val3 = m.UncheckedCast<UintPtrT>(
        m.ChangeUint32ToWord(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(2)))));

    m.TestBitFieldUintptrOps(val2, val3);
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);

  // Construct the expected test values.
  int val2 = 3 | (61 << 5);
  int val3 = 1 | (500 << 1) | (0x1cc << 10);

  ft.Call(ft.NewNumber(val2), ft.NewNumber(val3));
}

TEST_F(TorqueCodegenTest, TestBitFieldMultipleFlags) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 3;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    TNode<BoolT> a =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(0))));
    TNode<Int32T> b = m.SmiToInt32(m.Parameter<Smi>(1));
    TNode<BoolT> c =
        m.UncheckedCast<BoolT>(m.Unsigned(m.SmiToInt32(m.Parameter<Smi>(2))));
    m.TestBitFieldMultipleFlags(a, b, c);
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  // No need to call it; we just checked StaticAsserts during compilation.
}

TEST_F(TorqueCodegenTest, TestTestParentFrameArguments) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  Handle<Context> context =
      Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestParentFrameArguments(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  asm_tester.GenerateCode();
}

TEST_F(TorqueCodegenTest, TestWord8Phi) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestWord8Phi();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestOffHeapSlice) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  std::string data = "Hello World!";
  {
    m.TestOffHeapSlice(m.PointerConstant(const_cast<char*>(data.data())),
                       m.IntPtrConstant(data.size()));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestCallMultiReturnBuiltin) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 1;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    Handle<Context> context =
        Utils::OpenHandle(*v8::Isolate::GetCurrent()->GetCurrentContext());
    m.TestCallMultiReturnBuiltin(
        m.UncheckedCast<Context>(m.HeapConstantNoHole(context)));
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestRunLazyTwice) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  int lazyNumber = 3;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    CodeStubAssembler::LazyNode<Smi> lazy = [&]() {
      return m.SmiConstant(lazyNumber++);
    };
    m.Return(m.TestRunLazyTwice(lazy));
  }
  CHECK_EQ(lazyNumber, 5);
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  DirectHandle<Object> result = ft.Call().ToHandleChecked();
  CHECK_EQ(7, Cast<Smi>(*result).value());
}

TEST_F(TorqueCodegenTest, TestCreateLazyNodeFromTorque) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.TestCreateLazyNodeFromTorque();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

TEST_F(TorqueCodegenTest, TestReturnNever_NotCalled) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();
    TNode<Smi> arg = m.SmiConstant(42);
    TNode<Object> result = m.CallBuiltin(Builtin::kTestCallNever, context, arg);
    m.Return(result);
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  DirectHandle<Object> result = ft.Call().ToHandleChecked();
  CHECK_EQ(42, Cast<Smi>(*result).value());
}

// Test calling a builtin that calls a runtime fct with return type {never}.
TEST_F(TorqueCodegenTest, TestReturnNever_Runtime_Called) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();
    TNode<Smi> arg = m.SmiConstant(1);
    TNode<Object> result = m.CallBuiltin(Builtin::kTestCallNever, context, arg);
    m.Return(result);
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  MaybeDirectHandle<Object> result = ft.Call();
  CHECK(result.is_null());
  CHECK(isolate->has_exception());
  isolate->clear_exception();
}

// Test calling a builtin that calls another builtin with return type {never}.
TEST_F(TorqueCodegenTest, TestReturnNever_Builtin_Called) {
  Isolate* isolate(i_isolate());
  i::HandleScope scope(isolate);
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();
    TNode<Smi> arg = m.SmiConstant(-1);
    TNode<Object> result = m.CallBuiltin(Builtin::kTestCallNever, context, arg);
    m.Return(result);
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  MaybeDirectHandle<Object> result = ft.Call();
  CHECK(result.is_null());
  CHECK(isolate->has_exception());
  isolate->clear_exception();
}

int* global_use_counts = nullptr;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  if (global_use_counts != nullptr) ++global_use_counts[feature];
}

// Test @incrementUseCounter
TEST_F(TorqueCodegenTest, TestIncrementUseCounterInBuiltin) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  isolate->SetUseCounterCallback(MockUseCounterCallback);

  Isolate* i_isolate(this->i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(i_isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();
    TNode<Object> result =
        m.CallBuiltin(Builtin::kTestIncrementArraySpeciesModified, context);
    m.Return(result);
  }
  FunctionTester ft(i_isolate, asm_tester.GenerateCode(), kNumParams);
  CHECK_EQ(0, use_counts[v8::Isolate::kArraySpeciesModified]);
  ft.Call();
  CHECK_EQ(1, use_counts[v8::Isolate::kArraySpeciesModified]);

  global_use_counts = nullptr;
}

TEST_F(TorqueCodegenTest, TestNodePower) {
  Isolate* isolate(i_isolate());
  const int kNumParams = 0;
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(kNumParams));
  TestTorqueAssembler m(asm_tester.state());
  {
    auto context = m.GetJSContextParameter();

    // Fixed test cases
    struct TestCase {
      int s1, n1, n2, n;
    } cases[] = {{0, 2, 2, 8},     {0, 1, 1, 4},    {2, 1, 1, 4},
                 {0, 3, 3, 8},     {0, 1, 1, 2},    {0, 2, 2, 4},
                 {0, 4, 4, 8},     {0, 1, 3, 4},    {0, 3, 1, 4},
                 {1, 1, 1, 3},     {1, 1, 2, 4},    {0, 8, 8, 16},
                 {0, 16, 16, 32},  {0, 32, 32, 64}, {0, 10, 20, 64},
                 {10, 10, 10, 64}, {0, 31, 32, 64}};

    for (const auto& c : cases) {
      TNode<Smi> s1 = m.SmiConstant(c.s1);
      TNode<Smi> n1 = m.SmiConstant(c.n1);
      TNode<Smi> n2 = m.SmiConstant(c.n2);
      TNode<Smi> n = m.SmiConstant(c.n);

      TNode<Smi> p32 = m.NodePower32(context, s1, n1, n2, n);
      TNode<Smi> p = m.NodePower(context, s1, n1, n2, n);

      CSA_CHECK(&m, m.TaggedEqual(p32, p));
    }

    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(isolate, asm_tester.GenerateCode(), kNumParams);
  ft.Call();
}

}  // namespace

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace torque_codegen_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
