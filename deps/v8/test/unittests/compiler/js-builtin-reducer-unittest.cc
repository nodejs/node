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
    JSBuiltinReducer reducer(&graph_reducer, &jsgraph);
    return reducer.Reduce(node);
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
// Math.max


TEST_F(JSBuiltinReducerTest, MathMax0) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  Node* call = graph()->NewNode(javascript()->CallFunction(2), function,
                                UndefinedConstant(), context, frame_state,
                                frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(-V8_INFINITY));
}


TEST_F(JSBuiltinReducerTest, MathMax1) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  frame_state, effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), p0);
  }
}


TEST_F(JSBuiltinReducerTest, MathMax2) {
  Node* function = MathFunction("max");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kIntegral32Types) {
    TRACED_FOREACH(Type*, t1, kIntegral32Types) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                    UndefinedConstant(), p0, p1, context,
                                    frame_state, frame_state, effect, control);
      Reduction r = Reduce(call);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsSelect(MachineRepresentation::kNone,
                                            IsNumberLessThan(p1, p0), p0, p1));
    }
  }
}


// -----------------------------------------------------------------------------
// Math.imul


TEST_F(JSBuiltinReducerTest, MathImul) {
  Node* function = MathFunction("imul");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* call = graph()->NewNode(javascript()->CallFunction(4), function,
                                    UndefinedConstant(), p0, p1, context,
                                    frame_state, frame_state, effect, control);
      Reduction r = Reduce(call);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(),
                  IsNumberImul(IsNumberToUint32(p0), IsNumberToUint32(p1)));
    }
  }
}


// -----------------------------------------------------------------------------
// Math.fround


TEST_F(JSBuiltinReducerTest, MathFround) {
  Node* function = MathFunction("fround");

  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* context = UndefinedConstant();
  Node* frame_state = graph()->start();
  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* call = graph()->NewNode(javascript()->CallFunction(3), function,
                                  UndefinedConstant(), p0, context, frame_state,
                                  frame_state, effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsTruncateFloat64ToFloat32(p0));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
