// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-unittest.h"
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/typer.h"
#include "testing/gmock-support.h"

using testing::Capture;

namespace v8 {
namespace internal {
namespace compiler {

class JSBuiltinReducerTest : public GraphTest {
 public:
  JSBuiltinReducerTest() : javascript_(zone()) {}

 protected:
  Reduction Reduce(Node* node) {
    Typer typer(zone());
    MachineOperatorBuilder machine;
    JSGraph jsgraph(graph(), common(), javascript(), &typer, &machine);
    JSBuiltinReducer reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  Node* Parameter(Type* t, int32_t index = 0) {
    Node* n = graph()->NewNode(common()->Parameter(index), graph()->start());
    NodeProperties::SetBounds(n, Bounds(Type::None(), t));
    return n;
  }

  Node* UndefinedConstant() {
    return HeapConstant(
        Unique<HeapObject>::CreateImmovable(factory()->undefined_value()));
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


namespace {

// TODO(mstarzinger): Find a common place and unify with test-js-typed-lowering.
Type* const kNumberTypes[] = {
    Type::UnsignedSmall(),   Type::OtherSignedSmall(), Type::OtherUnsigned31(),
    Type::OtherUnsigned32(), Type::OtherSigned32(),    Type::SignedSmall(),
    Type::Signed32(),        Type::Unsigned32(),       Type::Integral32(),
    Type::MinusZero(),       Type::NaN(),              Type::OtherNumber(),
    Type::OrderedNumber(),   Type::Number()};

}  // namespace


// -----------------------------------------------------------------------------
// Math.abs


TEST_F(JSBuiltinReducerTest, MathAbs) {
  Handle<JSFunction> f(isolate()->context()->math_abs_fun());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call = graph()->NewNode(javascript()->Call(3, NO_CALL_FUNCTION_FLAGS),
                                  fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    if (t0->Is(Type::Unsigned32())) {
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), p0);
    } else {
      Capture<Node*> branch;
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsPhi(kMachNone, p0, IsNumberSubtract(IsNumberConstant(0), p0),
                IsMerge(IsIfTrue(CaptureEq(&branch)),
                        IsIfFalse(AllOf(
                            CaptureEq(&branch),
                            IsBranch(IsNumberLessThan(IsNumberConstant(0), p0),
                                     graph()->start()))))));
    }
  }
}


// -----------------------------------------------------------------------------
// Math.sqrt


TEST_F(JSBuiltinReducerTest, MathSqrt) {
  Handle<JSFunction> f(isolate()->context()->math_sqrt_fun());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call = graph()->NewNode(javascript()->Call(3, NO_CALL_FUNCTION_FLAGS),
                                  fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFloat64Sqrt(p0));
  }
}


// -----------------------------------------------------------------------------
// Math.max


TEST_F(JSBuiltinReducerTest, MathMax0) {
  Handle<JSFunction> f(isolate()->context()->math_max_fun());

  Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
  Node* call = graph()->NewNode(javascript()->Call(2, NO_CALL_FUNCTION_FLAGS),
                                fun, UndefinedConstant());
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(-V8_INFINITY));
}


TEST_F(JSBuiltinReducerTest, MathMax1) {
  Handle<JSFunction> f(isolate()->context()->math_max_fun());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call = graph()->NewNode(javascript()->Call(3, NO_CALL_FUNCTION_FLAGS),
                                  fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), p0);
  }
}


TEST_F(JSBuiltinReducerTest, MathMax2) {
  Handle<JSFunction> f(isolate()->context()->math_max_fun());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
      Node* call =
          graph()->NewNode(javascript()->Call(4, NO_CALL_FUNCTION_FLAGS), fun,
                           UndefinedConstant(), p0, p1);
      Reduction r = Reduce(call);

      if (t0->Is(Type::Integral32()) && t1->Is(Type::Integral32())) {
        Capture<Node*> branch;
        ASSERT_TRUE(r.Changed());
        EXPECT_THAT(
            r.replacement(),
            IsPhi(kMachNone, p1, p0,
                  IsMerge(IsIfTrue(CaptureEq(&branch)),
                          IsIfFalse(AllOf(CaptureEq(&branch),
                                          IsBranch(IsNumberLessThan(p0, p1),
                                                   graph()->start()))))));
      } else {
        ASSERT_FALSE(r.Changed());
        EXPECT_EQ(IrOpcode::kJSCallFunction, call->opcode());
      }
    }
  }
}


// -----------------------------------------------------------------------------
// Math.imul


TEST_F(JSBuiltinReducerTest, MathImul) {
  Handle<JSFunction> f(isolate()->context()->math_imul_fun());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
      Node* call =
          graph()->NewNode(javascript()->Call(4, NO_CALL_FUNCTION_FLAGS), fun,
                           UndefinedConstant(), p0, p1);
      Reduction r = Reduce(call);

      if (t0->Is(Type::Integral32()) && t1->Is(Type::Integral32())) {
        ASSERT_TRUE(r.Changed());
        EXPECT_THAT(r.replacement(), IsInt32Mul(p0, p1));
      } else {
        ASSERT_FALSE(r.Changed());
        EXPECT_EQ(IrOpcode::kJSCallFunction, call->opcode());
      }
    }
  }
}


// -----------------------------------------------------------------------------
// Math.fround


TEST_F(JSBuiltinReducerTest, MathFround) {
  Handle<Object> m =
      JSObject::GetProperty(isolate()->global_object(),
                            isolate()->factory()->NewStringFromAsciiChecked(
                                "Math")).ToHandleChecked();
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      JSObject::GetProperty(m, isolate()->factory()->NewStringFromAsciiChecked(
                                   "fround")).ToHandleChecked());

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call = graph()->NewNode(javascript()->Call(3, NO_CALL_FUNCTION_FLAGS),
                                  fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsTruncateFloat64ToFloat32(p0));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
