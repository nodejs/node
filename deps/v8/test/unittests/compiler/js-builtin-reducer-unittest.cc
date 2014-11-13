// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/typer.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

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
    MachineOperatorBuilder machine(kMachPtr, flags);
    JSGraph jsgraph(graph(), common(), javascript(), &machine);
    JSBuiltinReducer reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  Node* Parameter(Type* t, int32_t index = 0) {
    Node* n = graph()->NewNode(common()->Parameter(index), graph()->start());
    NodeProperties::SetBounds(n, Bounds(Type::None(), t));
    return n;
  }

  Handle<JSFunction> MathFunction(const char* name) {
    Handle<Object> m =
        JSObject::GetProperty(isolate()->global_object(),
                              isolate()->factory()->NewStringFromAsciiChecked(
                                  "Math")).ToHandleChecked();
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        JSObject::GetProperty(
            m, isolate()->factory()->NewStringFromAsciiChecked(name))
            .ToHandleChecked());
    return f;
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
  Handle<JSFunction> f = MathFunction("abs");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    if (t0->Is(Type::Unsigned32())) {
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), p0);
    } else {
      Capture<Node*> branch;
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(),
                  IsSelect(kMachNone, IsNumberLessThan(IsNumberConstant(0), p0),
                           p0, IsNumberSubtract(IsNumberConstant(0), p0)));
    }
  }
}


// -----------------------------------------------------------------------------
// Math.sqrt


TEST_F(JSBuiltinReducerTest, MathSqrt) {
  Handle<JSFunction> f = MathFunction("sqrt");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFloat64Sqrt(p0));
  }
}


// -----------------------------------------------------------------------------
// Math.max


TEST_F(JSBuiltinReducerTest, MathMax0) {
  Handle<JSFunction> f = MathFunction("max");

  Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
  Node* call =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS),
                       fun, UndefinedConstant());
  Reduction r = Reduce(call);

  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(-V8_INFINITY));
}


TEST_F(JSBuiltinReducerTest, MathMax1) {
  Handle<JSFunction> f = MathFunction("max");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), p0);
  }
}


TEST_F(JSBuiltinReducerTest, MathMax2) {
  Handle<JSFunction> f = MathFunction("max");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
      Node* call = graph()->NewNode(
          javascript()->CallFunction(4, NO_CALL_FUNCTION_FLAGS), fun,
          UndefinedConstant(), p0, p1);
      Reduction r = Reduce(call);

      if (t0->Is(Type::Integral32()) && t1->Is(Type::Integral32())) {
        ASSERT_TRUE(r.Changed());
        EXPECT_THAT(r.replacement(),
                    IsSelect(kMachNone, IsNumberLessThan(p1, p0), p1, p0));
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
  Handle<JSFunction> f = MathFunction("imul");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    TRACED_FOREACH(Type*, t1, kNumberTypes) {
      Node* p0 = Parameter(t0, 0);
      Node* p1 = Parameter(t1, 1);
      Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
      Node* call = graph()->NewNode(
          javascript()->CallFunction(4, NO_CALL_FUNCTION_FLAGS), fun,
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
  Handle<JSFunction> f = MathFunction("fround");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsTruncateFloat64ToFloat32(p0));
  }
}


// -----------------------------------------------------------------------------
// Math.floor


TEST_F(JSBuiltinReducerTest, MathFloorAvailable) {
  Handle<JSFunction> f = MathFunction("floor");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call, MachineOperatorBuilder::Flag::kFloat64Floor);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFloat64Floor(p0));
  }
}


TEST_F(JSBuiltinReducerTest, MathFloorUnavailable) {
  Handle<JSFunction> f = MathFunction("floor");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call, MachineOperatorBuilder::Flag::kNoFlags);

    ASSERT_FALSE(r.Changed());
  }
}


// -----------------------------------------------------------------------------
// Math.ceil


TEST_F(JSBuiltinReducerTest, MathCeilAvailable) {
  Handle<JSFunction> f = MathFunction("ceil");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call, MachineOperatorBuilder::Flag::kFloat64Ceil);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFloat64Ceil(p0));
  }
}


TEST_F(JSBuiltinReducerTest, MathCeilUnavailable) {
  Handle<JSFunction> f = MathFunction("ceil");

  TRACED_FOREACH(Type*, t0, kNumberTypes) {
    Node* p0 = Parameter(t0, 0);
    Node* fun = HeapConstant(Unique<HeapObject>::CreateUninitialized(f));
    Node* call =
        graph()->NewNode(javascript()->CallFunction(3, NO_CALL_FUNCTION_FLAGS),
                         fun, UndefinedConstant(), p0);
    Reduction r = Reduce(call, MachineOperatorBuilder::Flag::kNoFlags);

    ASSERT_FALSE(r.Changed());
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
