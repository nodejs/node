// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using namespace std::string_literals;

class CallRuntimeTest : public ReducerTest {};

START_ALLOW_MISSING_DESIGNATED_FIELD_INITIALIZERS()

TEST_F(CallRuntimeTest, OptionalArguments) {
  auto test = CreateFromGraph(2, [isolate = isolate()](auto& Asm) {
    V<Smi> arg = V<Smi>::Cast(Asm.GetParameter(1));

    IF (__ SmiEqual(arg, Smi::FromInt(0))) {
      __ template CallRuntime<runtime::ThrowTypeError>(
          Asm.context(), {.template_index = __ SmiConstant(Smi::FromEnum(
                              MessageTemplate::kDebuggerLoading))});
    } ELSE IF (__ SmiEqual(arg, Smi::FromInt(1))) {
      __ template CallRuntime<runtime::ThrowTypeError>(
          Asm.context(),
          {.template_index =
               __ SmiConstant(Smi::FromEnum(MessageTemplate::kNotCallable)),
           .arg0 = __ HeapConstantNoHole(BigInt::FromInt64(isolate, 42))});
    } ELSE IF (__ SmiEqual(arg, Smi::FromInt(2))) {
      __ template CallRuntime<runtime::ThrowTypeError>(
          Asm.context(),
          {.template_index = __ SmiConstant(
               Smi::FromEnum(MessageTemplate::kInvalidInOperatorUse)),
           .arg0 = __ SmiConstant(Smi::FromInt(8)),
           .arg1 = __ HeapConstantNoHole(BigInt::FromInt64(isolate, 9))});
    } ELSE IF (__ SmiEqual(arg, Smi::FromInt(3))) {
      V<Undefined> undefined =
          __ HeapConstantNoHole(Asm.factory().undefined_value());
      __ template CallRuntime<runtime::ThrowTypeError>(
          Asm.context(), {.template_index = __ SmiConstant(Smi::FromEnum(
                              MessageTemplate::kTargetNonFunction)),
                          .arg0 = undefined,
                          .arg1 = undefined,
                          .arg2 = __ SmiConstant(Smi::FromInt(1337))});
    }

    __ Unreachable();
  });

  Handle<Code> code = test.CompileAsJSBuiltin();
  FunctionTester ft(isolate(), code,
                    test.GetParameterCount() - kJSArgcReceiverSlots);

  // 0 Arguments
  {
    Handle<Smi> argc = handle(Smi::FromInt(0), isolate());
    auto output = ft.CheckThrowsReturnMessage(argc);
    v8::String::Utf8Value message(v8_isolate(), output->Get());
    CHECK_EQ("Uncaught TypeError: Error loading debugger"s,
             std::string(*message));
  }

  // 1 Argument
  {
    Handle<Smi> argc = handle(Smi::FromInt(1), isolate());
    auto output = ft.CheckThrowsReturnMessage(argc);
    v8::String::Utf8Value message(v8_isolate(), output->Get());
    CHECK_EQ("Uncaught TypeError: 42 is not a function"s,
             std::string(*message));
  }

  // 2 Arguments
  {
    Handle<Smi> argc = handle(Smi::FromInt(2), isolate());
    auto output = ft.CheckThrowsReturnMessage(argc);
    v8::String::Utf8Value message(v8_isolate(), output->Get());
    CHECK_EQ(
        "Uncaught TypeError: Cannot use 'in' operator to search for '8' in 9"s,
        std::string(*message));
  }

  // 3 Arguments
  {
    Handle<Smi> argc = handle(Smi::FromInt(3), isolate());
    auto output = ft.CheckThrowsReturnMessage(argc);
    v8::String::Utf8Value message(v8_isolate(), output->Get());
    CHECK_EQ(
        "Uncaught TypeError: undefined was called on undefined, which is "
        "1337 and not a function"s,
        std::string(*message));
  }
}

END_ALLOW_MISSING_DESIGNATED_FIELD_INITIALIZERS()

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
