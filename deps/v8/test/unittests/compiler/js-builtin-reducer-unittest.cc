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

    JSBuiltinReducer reducer(&graph_reducer, &jsgraph, nullptr,
                             native_context());
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
    Node* call =
        graph()->NewNode(javascript()->Call(3), function, UndefinedConstant(),
                         p0, context, frame_state, effect, control);
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
  Node* call =
      graph()->NewNode(javascript()->Call(3), function, UndefinedConstant(), p0,
                       context, frame_state, effect, control);
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
    Node* call =
        graph()->NewNode(javascript()->Call(3), function, UndefinedConstant(),
                         p0, context, frame_state, effect, control);
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
  Node* call =
      graph()->NewNode(javascript()->Call(3), function, UndefinedConstant(), p0,
                       context, frame_state, effect, control);
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(IsPlainPrimitiveToNumber(p0),
                                         IsPlainPrimitiveToNumber(p0))));
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
    Node* call =
        graph()->NewNode(javascript()->Call(3), function, UndefinedConstant(),
                         p0, context, frame_state, effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(p0, r.replacement());
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
    Node* call =
        graph()->NewNode(javascript()->Call(4), function, UndefinedConstant(),
                         p0, p1, context, frame_state, effect, control);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(p0, r.replacement());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
