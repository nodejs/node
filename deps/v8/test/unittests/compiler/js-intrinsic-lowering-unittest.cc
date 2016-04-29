// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/diamond.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-intrinsic-lowering.h"
#include "src/compiler/js-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"


using testing::_;
using testing::AllOf;
using testing::BitEq;
using testing::Capture;
using testing::CaptureEq;


namespace v8 {
namespace internal {
namespace compiler {

class JSIntrinsicLoweringTest : public GraphTest {
 public:
  JSIntrinsicLoweringTest() : GraphTest(3), javascript_(zone()) {}
  ~JSIntrinsicLoweringTest() override {}

 protected:
  Reduction Reduce(Node* node, MachineOperatorBuilder::Flags flags =
                                   MachineOperatorBuilder::kNoFlags) {
    MachineOperatorBuilder machine(zone(), MachineType::PointerRepresentation(),
                                   flags);
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSIntrinsicLowering reducer(&graph_reducer, &jsgraph,
                                JSIntrinsicLowering::kDeoptimizationEnabled);
    return reducer.Reduce(node);
  }

  Node* EmptyFrameState() {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), nullptr,
                    &machine);
    return jsgraph.EmptyFrameState();
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


// -----------------------------------------------------------------------------
// %_ConstructDouble


TEST_F(JSIntrinsicLoweringTest, InlineOptimizedConstructDouble) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineConstructDouble, 2), input0,
      input1, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64InsertHighWord32(
                                   IsFloat64InsertLowWord32(
                                       IsNumberConstant(BitEq(0.0)), input1),
                                   input0));
}


// -----------------------------------------------------------------------------
// %_DoubleLo


TEST_F(JSIntrinsicLoweringTest, InlineOptimizedDoubleLo) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineDoubleLo, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsFloat64ExtractLowWord32(IsGuard(Type::Number(), input, _)));
}


// -----------------------------------------------------------------------------
// %_DoubleHi


TEST_F(JSIntrinsicLoweringTest, InlineOptimizedDoubleHi) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineDoubleHi, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsFloat64ExtractHighWord32(IsGuard(Type::Number(), input, _)));
}


// -----------------------------------------------------------------------------
// %_IsSmi


TEST_F(JSIntrinsicLoweringTest, InlineIsSmi) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineIsSmi, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsObjectIsSmi(input));
}


// -----------------------------------------------------------------------------
// %_IsArray


TEST_F(JSIntrinsicLoweringTest, InlineIsArray) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineIsArray, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());

  Node* phi = r.replacement();
  Capture<Node*> branch, if_false;
  EXPECT_THAT(
      phi,
      IsPhi(
          MachineRepresentation::kTagged, IsFalseConstant(),
          IsWord32Equal(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsInt32Constant(JS_ARRAY_TYPE)),
          IsMerge(IsIfTrue(AllOf(CaptureEq(&branch),
                                 IsBranch(IsObjectIsSmi(input), control))),
                  AllOf(CaptureEq(&if_false), IsIfFalse(CaptureEq(&branch))))));
}


// -----------------------------------------------------------------------------
// %_IsTypedArray


TEST_F(JSIntrinsicLoweringTest, InlineIsTypedArray) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineIsTypedArray, 1), input,
      context, effect, control));
  ASSERT_TRUE(r.Changed());

  Node* phi = r.replacement();
  Capture<Node*> branch, if_false;
  EXPECT_THAT(
      phi,
      IsPhi(
          MachineRepresentation::kTagged, IsFalseConstant(),
          IsWord32Equal(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsInt32Constant(JS_TYPED_ARRAY_TYPE)),
          IsMerge(IsIfTrue(AllOf(CaptureEq(&branch),
                                 IsBranch(IsObjectIsSmi(input), control))),
                  AllOf(CaptureEq(&if_false), IsIfFalse(CaptureEq(&branch))))));
}


// -----------------------------------------------------------------------------
// %_IsRegExp


TEST_F(JSIntrinsicLoweringTest, InlineIsRegExp) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineIsRegExp, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());

  Node* phi = r.replacement();
  Capture<Node*> branch, if_false;
  EXPECT_THAT(
      phi,
      IsPhi(
          MachineRepresentation::kTagged, IsFalseConstant(),
          IsWord32Equal(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsInt32Constant(JS_REGEXP_TYPE)),
          IsMerge(IsIfTrue(AllOf(CaptureEq(&branch),
                                 IsBranch(IsObjectIsSmi(input), control))),
                  AllOf(CaptureEq(&if_false), IsIfFalse(CaptureEq(&branch))))));
}


// -----------------------------------------------------------------------------
// %_IsJSReceiver


TEST_F(JSIntrinsicLoweringTest, InlineIsJSReceiver) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineIsJSReceiver, 1), input,
      context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsObjectIsReceiver(input));
}


// -----------------------------------------------------------------------------
// %_ValueOf


TEST_F(JSIntrinsicLoweringTest, InlineValueOf) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineValueOf, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());

  Node* phi = r.replacement();
  Capture<Node*> branch0, if_false0, branch1, if_true1;
  EXPECT_THAT(
      phi,
      IsPhi(
          MachineRepresentation::kTagged, input,
          IsPhi(MachineRepresentation::kTagged,
                IsLoadField(AccessBuilder::ForValue(), input, effect,
                            CaptureEq(&if_true1)),
                input,
                IsMerge(
                    AllOf(CaptureEq(&if_true1), IsIfTrue(CaptureEq(&branch1))),
                    IsIfFalse(AllOf(
                        CaptureEq(&branch1),
                        IsBranch(
                            IsWord32Equal(
                                IsLoadField(
                                    AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false0)),
                                    effect, _),
                                IsInt32Constant(JS_VALUE_TYPE)),
                            CaptureEq(&if_false0)))))),
          IsMerge(
              IsIfTrue(AllOf(CaptureEq(&branch0),
                             IsBranch(IsObjectIsSmi(input), control))),
              AllOf(CaptureEq(&if_false0), IsIfFalse(CaptureEq(&branch0))))));
}

// -----------------------------------------------------------------------------
// %_GetOrdinaryHasInstance

TEST_F(JSIntrinsicLoweringTest, InlineGetOrdinaryHasInstance) {
  Node* const context = Parameter(0);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineGetOrdinaryHasInstance, 0),
      context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsLoadContext(
          ContextAccess(0, Context::ORDINARY_HAS_INSTANCE_INDEX, true), _));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
