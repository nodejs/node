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
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone(),
                                   MachineType::PointerRepresentation());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSIntrinsicLowering reducer(&graph_reducer, &jsgraph,
                                JSIntrinsicLowering::kDeoptimizationEnabled);
    return reducer.Reduce(node);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


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
          IsNumberEqual(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsNumberConstant(JS_ARRAY_TYPE)),
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
          IsNumberEqual(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsNumberConstant(JS_TYPED_ARRAY_TYPE)),
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
// %_CreateJSGeneratorObject

TEST_F(JSIntrinsicLoweringTest, InlineCreateJSGeneratorObject) {
  Node* const function = Parameter(0);
  Node* const receiver = Parameter(1);
  Node* const context = Parameter(2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineCreateJSGeneratorObject, 2),
      function, receiver, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(IrOpcode::kJSCreateGeneratorObject,
            r.replacement()->op()->opcode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
