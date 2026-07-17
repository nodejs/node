// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/typeswitch.h"

#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class TypeswitchTest : public ReducerTest {};

TEST_F(TypeswitchTest, Simple) {
  auto test = CreateFromGraph(2, [isolate = isolate()](auto& Asm) {
    V<Object> arg = Asm.GetParameter(1);

    Label<Word32> done(&Asm);

    TYPESWITCH(arg) {
      CASE_(V<Smi>, smi): {
        GOTO(done, __ UntagSmi(smi));
      }
      CASE_(V<String>, str): {
        GOTO(done, __ StringLength(str));
      }
      CASE_(V<Number>, num): {
        GOTO(done, 1001);
      }
      CASE_(V<BigInt>, bi): {
        auto bi888 = __ HeapConstantNoHole(BigInt::FromInt64(isolate, 888));
        GOTO_IF(__ ConvertBooleanToWord32(__ BigIntEqual(bi, bi888)), done,
                1002);
        // Otherwise unhandled.
      }
      CASE_(V<Undefined>, ud): {
        GOTO(done, 1003);
      }
    }

    GOTO(done, 0);

    BIND(done, result);
    __ Return(__ TagSmi(result));
  });

  // Run MachineLoweringReducer so that we can use BigIntEqual and
  // StringLength.
  test.Run<MachineLoweringReducer>();

  Handle<Code> code = test.CompileAsJSBuiltin();
  FunctionTester ft(isolate(), code,
                    test.GetParameterCount() - kJSArgcReceiverSlots);

  // Smi
  {
    DirectHandle<Smi> arg = handle(Smi::FromInt(18), isolate());
    DirectHandle<Smi> res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(18, (*res).ToSmi().value());
  }

  // String
  {
    const char* str = "Hello V8!";
    DirectHandle<Smi> res = ft.CallChecked<Smi>(MakeString(str));
    CHECK_EQ(strlen(str), (*res).ToSmi().value());
  }

  // Number (HeapNumber)
  {
    DirectHandle<HeapNumber> arg = factory()->NewHeapNumber(3.14159);
    DirectHandle<Smi> res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(1001, (*res).ToSmi().value());
  }

  // BigInt
  {
    // Unhandled case.
    DirectHandle<BigInt> arg = BigInt::FromUint64(isolate(), 1888);
    DirectHandle<Smi> res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(0, (*res).ToSmi().value());
    // Handled case.
    arg = BigInt::FromUint64(isolate(), 888);
    res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(1002, (*res).ToSmi().value());
  }

  // Undefined
  {
    DirectHandle<Undefined> arg = factory()->undefined_value();
    DirectHandle<Smi> res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(1003, (*res).ToSmi().value());
  }

  // Unhandled
  {
    DirectHandle<Boolean> arg = factory()->true_value();
    DirectHandle<Smi> res = ft.CallChecked<Smi>(arg);
    CHECK_EQ(0, (*res).ToSmi().value());
  }
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
