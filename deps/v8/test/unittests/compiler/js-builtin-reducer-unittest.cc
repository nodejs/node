// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/typer.h"
#include "src/isolate-inl.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::BitEq;
using testing::Capture;

namespace v8 {
namespace internal {
namespace compiler {

class JSBuiltinReducerTest : public TypedGraphTest {
 public:
  JSBuiltinReducerTest() : javascript_(zone()) {}

 protected:
  Reduction Reduce(Node* node, MachineOperatorBuilder::Flags flags =
                                   MachineOperatorBuilder::Flag::kNoFlags) {
    MachineOperatorBuilder machine(zone(), MachineType::PointerRepresentation(),
                                   flags);
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSBuiltinReducer reducer(&graph_reducer, &jsgraph,
                             JSBuiltinReducer::kNoFlags, nullptr);
    return reducer.Reduce(node);
  }

  Node* GlobalFunction(const char* name) {
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        Object::GetProperty(
            isolate()->global_object(),
            isolate()->factory()->NewStringFromAsciiChecked(name))
            .ToHandleChecked());
    return HeapConstant(f);
  }

  Node* MathFunction(const char* name) {
    Handle<Object> m =
        JSObject::GetProperty(isolate()->global_object(),
                              isolate()->factory()->NewStringFromAsciiChecked(
                                  "Math")).ToHandleChecked();
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        Object::GetProperty(
            m, isolate()->factory()->NewStringFromAsciiChecked(name))
            .ToHandleChecked());
    return HeapConstant(f);
  }

  Node* NumberFunction(const char* name) {
    Handle<Object> m =
        JSObject::GetProperty(
            isolate()->global_object(),
            isolate()->factory()->NewStringFromAsciiChecked("Number"))
            .ToHandleChecked();
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        Object::GetProperty(
            m, isolate()->factory()->NewStringFromAsciiChecked(name))
            .ToHandleChecked());
    return HeapConstant(f);
  }

  Node* StringFunction(const char* name) {
    Handle<Object> m =
        JSObject::GetProperty(
            isolate()->global_object(),
            isolate()->factory()->NewStringFromAsciiChecked("String"))
            .ToHandleChecked();
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        Object::GetProperty(
            m, isolate()->factory()->NewStringFromAsciiChecked(name))
            .ToHandleChecked());
    return HeapConstant(f);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


namespace {

Type* const kIntegral32Types[] = {Type::UnsignedSmall(), Type::Negative32(),
                                  Type::Unsigned31(),    Type::SignedSmall(),
                                  Type::Signed32(),      Type::Unsigned32(),
                                  Type::Integral32()};


Type* const kNumberTypes[] = {
    Type::UnsignedSmall(), Type::Negative32(),  Type::Unsigned31(),
    Type::SignedSmall(),   Type::Signed32(),    Type::Unsigned32(),
    Type::Integral32(),    Type::MinusZero(),   Type::NaN(),
    Type::OrderedNumber(), Type::PlainNumber(), Type::Number()};

}  // namespace


// -----------------------------------------------------------------------------
// isFinite

TEST_F(JSBuiltinReducerTest, GlobalIsFiniteWithNumber) {
  Node* function = GlobalFunction("isFinite");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberEqual(IsNumberSubtract(p0, p0),
                                               IsNumberSubtract(p0, p0)));
  }
}

TEST_F(JSBuiltinReducerTest, GlobalIsFiniteWithPlainPrimitive) {
  Node* function = GlobalFunction("isFinite");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsNumberEqual(IsNumberSubtract(IsPlainPrimitiveToNumber(p0),
                                             IsPlainPrimitiveToNumber(p0)),
                            IsNumberSubtract(IsPlainPrimitiveToNumber(p0),
                                             IsPlainPrimitiveToNumber(p0))));
}

// -----------------------------------------------------------------------------
// isNaN

TEST_F(JSBuiltinReducerTest, GlobalIsNaNWithNumber) {
  Node* function = GlobalFunction("isNaN");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsBooleanNot(IsNumberEqual(p0, p0)));
  }
}

TEST_F(JSBuiltinReducerTest, GlobalIsNaNWithPlainPrimitive) {
  Node* function = GlobalFunction("isNaN");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(IsPlainPrimitiveToNumber(p0),
                                         IsPlainPrimitiveToNumber(p0))));
}

// -----------------------------------------------------------------------------
// Math.abs

TEST_F(JSBuiltinReducerTest, MathAbsWithNumber) {
  Node* function = MathFunction("abs");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAbs(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAbsWithPlainPrimitive) {
  Node* function = MathFunction("abs");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAbs(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.acos

TEST_F(JSBuiltinReducerTest, MathAcosWithNumber) {
  Node* function = MathFunction("acos");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAcos(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAcosWithPlainPrimitive) {
  Node* function = MathFunction("acos");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAcos(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.acosh

TEST_F(JSBuiltinReducerTest, MathAcoshWithNumber) {
  Node* function = MathFunction("acosh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAcosh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAcoshWithPlainPrimitive) {
  Node* function = MathFunction("acosh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAcosh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.asin

TEST_F(JSBuiltinReducerTest, MathAsinWithNumber) {
  Node* function = MathFunction("asin");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAsin(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAsinWithPlainPrimitive) {
  Node* function = MathFunction("asin");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAsin(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.asinh

TEST_F(JSBuiltinReducerTest, MathAsinhWithNumber) {
  Node* function = MathFunction("asinh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAsinh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAsinhWithPlainPrimitive) {
  Node* function = MathFunction("asinh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAsinh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.atan

TEST_F(JSBuiltinReducerTest, MathAtanWithNumber) {
  Node* function = MathFunction("atan");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAtan(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAtanWithPlainPrimitive) {
  Node* function = MathFunction("atan");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAtan(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.atanh

TEST_F(JSBuiltinReducerTest, MathAtanhWithNumber) {
  Node* function = MathFunction("atanh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberAtanh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathAtanhWithPlainPrimitive) {
  Node* function = MathFunction("atanh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAtanh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.atan2

TEST_F(JSBuiltinReducerTest, MathAtan2WithNumber) {
  Node* function = MathFunction("atan2");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p1 = Parameter(t1, 0);
      Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                    UndefinedConstant(), p0, p1, context,
                                    frame_state, effect, control);
      Reduction r = Reduce(call);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsNumberAtan2(p0, p1));
    }
  }
}

TEST_F(JSBuiltinReducerTest, MathAtan2WithPlainPrimitive) {
  Node* function = MathFunction("atan2");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* p1 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberAtan2(IsPlainPrimitiveToNumber(p0),
                                             IsPlainPrimitiveToNumber(p1)));
}

// -----------------------------------------------------------------------------
// Math.ceil

TEST_F(JSBuiltinReducerTest, MathCeilWithNumber) {
  Node* function = MathFunction("ceil");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberCeil(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathCeilWithPlainPrimitive) {
  Node* function = MathFunction("ceil");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberCeil(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.clz32

TEST_F(JSBuiltinReducerTest, MathClz32WithUnsigned32) {
  Node* function = MathFunction("clz32");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::Unsigned32(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberClz32(p0));
}

TEST_F(JSBuiltinReducerTest, MathClz32WithNumber) {
  Node* function = MathFunction("clz32");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::Number(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberClz32(IsNumberToUint32(p0)));
}

TEST_F(JSBuiltinReducerTest, MathClz32WithPlainPrimitive) {
  Node* function = MathFunction("clz32");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsNumberClz32(IsNumberToUint32(IsPlainPrimitiveToNumber(p0))));
}

// -----------------------------------------------------------------------------
// Math.cos

TEST_F(JSBuiltinReducerTest, MathCosWithNumber) {
  Node* function = MathFunction("cos");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberCos(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathCosWithPlainPrimitive) {
  Node* function = MathFunction("cos");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberCos(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.cosh

TEST_F(JSBuiltinReducerTest, MathCoshWithNumber) {
  Node* function = MathFunction("cosh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberCosh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathCoshWithPlainPrimitive) {
  Node* function = MathFunction("cosh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberCosh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.exp

TEST_F(JSBuiltinReducerTest, MathExpWithNumber) {
  Node* function = MathFunction("exp");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberExp(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathExpWithPlainPrimitive) {
  Node* function = MathFunction("exp");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberExp(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.floor

TEST_F(JSBuiltinReducerTest, MathFloorWithNumber) {
  Node* function = MathFunction("floor");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberFloor(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathFloorWithPlainPrimitive) {
  Node* function = MathFunction("floor");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberFloor(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.fround

TEST_F(JSBuiltinReducerTest, MathFroundWithNumber) {
  Node* function = MathFunction("fround");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberFround(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathFroundWithPlainPrimitive) {
  Node* function = MathFunction("fround");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberFround(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.imul

TEST_F(JSBuiltinReducerTest, MathImulWithUnsigned32) {
  Node* function = MathFunction("imul");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::Unsigned32(), 0);
  Node* p1 = Parameter(Type::Unsigned32(), 1);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberImul(p0, p1));
}

TEST_F(JSBuiltinReducerTest, MathImulWithNumber) {
  Node* function = MathFunction("imul");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::Number(), 0);
  Node* p1 = Parameter(Type::Number(), 1);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsNumberImul(IsNumberToUint32(p0), IsNumberToUint32(p1)));
}

TEST_F(JSBuiltinReducerTest, MathImulWithPlainPrimitive) {
  Node* function = MathFunction("imul");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* p1 = Parameter(Type::PlainPrimitive(), 1);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsNumberImul(IsNumberToUint32(IsPlainPrimitiveToNumber(p0)),
                           IsNumberToUint32(IsPlainPrimitiveToNumber(p1))));
}

// -----------------------------------------------------------------------------
// Math.log

TEST_F(JSBuiltinReducerTest, MathLogWithNumber) {
  Node* function = MathFunction("log");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberLog(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathLogWithPlainPrimitive) {
  Node* function = MathFunction("log");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberLog(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.log1p

TEST_F(JSBuiltinReducerTest, MathLog1pWithNumber) {
  Node* function = MathFunction("log1p");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberLog1p(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathLog1pWithPlainPrimitive) {
  Node* function = MathFunction("log1p");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberLog1p(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.max

TEST_F(JSBuiltinReducerTest, MathMaxWithNoArguments) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* call = graph()->NewNode(javascript()->CallFunction(2), function,
                                UndefinedConstant(), context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(-V8_INFINITY));
}

TEST_F(JSBuiltinReducerTest, MathMaxWithNumber) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), p0);
  }
}

TEST_F(JSBuiltinReducerTest, MathMaxWithPlainPrimitive) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* p1 = Parameter(Type::PlainPrimitive(), 1);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberMax(IsPlainPrimitiveToNumber(p0),
                                           IsPlainPrimitiveToNumber(p1)));
}

// -----------------------------------------------------------------------------
// Math.min

TEST_F(JSBuiltinReducerTest, MathMinWithNoArguments) {
  Node* function = MathFunction("min");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* call = graph()->NewNode(javascript()->CallFunction(2), function,
                                UndefinedConstant(), context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(V8_INFINITY));
}

TEST_F(JSBuiltinReducerTest, MathMinWithNumber) {
  Node* function = MathFunction("min");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), p0);
  }
}

TEST_F(JSBuiltinReducerTest, MathMinWithPlainPrimitive) {
  Node* function = MathFunction("min");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* p1 = Parameter(Type::PlainPrimitive(), 1);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberMin(IsPlainPrimitiveToNumber(p0),
                                           IsPlainPrimitiveToNumber(p1)));
}

// -----------------------------------------------------------------------------
// Math.round

TEST_F(JSBuiltinReducerTest, MathRoundWithNumber) {
  Node* function = MathFunction("round");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberRound(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathRoundWithPlainPrimitive) {
  Node* function = MathFunction("round");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberRound(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.pow

TEST_F(JSBuiltinReducerTest, MathPowWithNumber) {
  Node* function = MathFunction("pow");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p1 = Parameter(t1, 0);
      Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                    UndefinedConstant(), p0, p1, context,
                                    frame_state, effect, control);
      Reduction r = Reduce(call);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsNumberPow(p0, p1));
    }
  }
}

TEST_F(JSBuiltinReducerTest, MathPowWithPlainPrimitive) {
  Node* function = MathFunction("pow");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* p1 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                UndefinedConstant(), p0, p1, context,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberPow(IsPlainPrimitiveToNumber(p0),
                                           IsPlainPrimitiveToNumber(p1)));
}

// -----------------------------------------------------------------------------
// Math.sign

TEST_F(JSBuiltinReducerTest, MathSignWithNumber) {
  Node* function = MathFunction("sign");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberSign(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathSignWithPlainPrimitive) {
  Node* function = MathFunction("sign");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberSign(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.sin

TEST_F(JSBuiltinReducerTest, MathSinWithNumber) {
  Node* function = MathFunction("sin");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberSin(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathSinWithPlainPrimitive) {
  Node* function = MathFunction("sin");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberSin(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.sinh

TEST_F(JSBuiltinReducerTest, MathSinhWithNumber) {
  Node* function = MathFunction("sinh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberSinh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathSinhWithPlainPrimitive) {
  Node* function = MathFunction("sinh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberSinh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.sqrt

TEST_F(JSBuiltinReducerTest, MathSqrtWithNumber) {
  Node* function = MathFunction("sqrt");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberSqrt(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathSqrtWithPlainPrimitive) {
  Node* function = MathFunction("sqrt");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberSqrt(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.tan

TEST_F(JSBuiltinReducerTest, MathTanWithNumber) {
  Node* function = MathFunction("tan");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberTan(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathTanWithPlainPrimitive) {
  Node* function = MathFunction("tan");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberTan(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.tanh

TEST_F(JSBuiltinReducerTest, MathTanhWithNumber) {
  Node* function = MathFunction("tanh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberTanh(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathTanhWithPlainPrimitive) {
  Node* function = MathFunction("tanh");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberTanh(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Math.trunc

TEST_F(JSBuiltinReducerTest, MathTruncWithNumber) {
  Node* function = MathFunction("trunc");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberTrunc(p0));
  }
}

TEST_F(JSBuiltinReducerTest, MathTruncWithPlainPrimitive) {
  Node* function = MathFunction("trunc");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberTrunc(IsPlainPrimitiveToNumber(p0)));
}

// -----------------------------------------------------------------------------
// Number.isFinite

TEST_F(JSBuiltinReducerTest, NumberIsFiniteWithNumber) {
  Node* function = NumberFunction("isFinite");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberEqual(IsNumberSubtract(p0, p0),
                                               IsNumberSubtract(p0, p0)));
  }
}

// -----------------------------------------------------------------------------
// Number.isInteger

TEST_F(JSBuiltinReducerTest, NumberIsIntegerWithNumber) {
  Node* function = NumberFunction("isInteger");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberEqual(IsNumberSubtract(p0, IsNumberTrunc(p0)),
                              IsNumberConstant(0.0)));
  }
}

// -----------------------------------------------------------------------------
// Number.isNaN

TEST_F(JSBuiltinReducerTest, NumberIsNaNWithNumber) {
  Node* function = NumberFunction("isNaN");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsBooleanNot(IsNumberEqual(p0, p0)));
  }
}

// -----------------------------------------------------------------------------
// Number.isSafeInteger

TEST_F(JSBuiltinReducerTest, NumberIsSafeIntegerWithIntegral32) {
  Node* function = NumberFunction("isSafeInteger");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kIntegral32Types) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsTrueConstant());
  }
}

// -----------------------------------------------------------------------------
// Number.parseInt

TEST_F(JSBuiltinReducerTest, NumberParseIntWithIntegral32) {
  Node* function = NumberFunction("parseInt");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kIntegral32Types) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberToInt32(p0));
  }
}

TEST_F(JSBuiltinReducerTest, NumberParseIntWithIntegral32AndUndefined) {
  Node* function = NumberFunction("parseInt");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kIntegral32Types) {
    Node* p0 = Parameter(t0, 0);
    Node* p1 = Parameter(Type::Undefined(), 1);
    Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                  UndefinedConstant(), p0, p1, context,
                                  frame_state, effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberToInt32(p0));
  }
}

// -----------------------------------------------------------------------------
// String.fromCharCode

TEST_F(JSBuiltinReducerTest, StringFromCharCodeWithNumber) {
  Node* function = StringFunction("fromCharCode");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsStringFromCharCode(p0));
  }
}

TEST_F(JSBuiltinReducerTest, StringFromCharCodeWithPlainPrimitive) {
  Node* function = StringFunction("fromCharCode");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* p0 = Parameter(Type::PlainPrimitive(), 0);
  Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                UndefinedConstant(), p0, context, frame_state,
                                effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsStringFromCharCode(IsPlainPrimitiveToNumber(p0)));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
