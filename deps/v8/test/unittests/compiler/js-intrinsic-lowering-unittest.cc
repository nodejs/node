// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
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
  ~JSIntrinsicLoweringTest() OVERRIDE {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &machine);
    JSIntrinsicLowering reducer(&jsgraph);
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
// %_IsNonNegativeSmi


TEST_F(JSIntrinsicLoweringTest, InlineIsNonNegativeSmi) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineIsNonNegativeSmi, 1), input,
      context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsObjectIsNonNegativeSmi(input));
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
          static_cast<MachineType>(kTypeBool | kRepTagged), IsFalseConstant(),
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
// %_IsFunction


TEST_F(JSIntrinsicLoweringTest, InlineIsFunction) {
  Node* const input = Parameter(0);
  Node* const context = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(
      graph()->NewNode(javascript()->CallRuntime(Runtime::kInlineIsFunction, 1),
                       input, context, effect, control));
  ASSERT_TRUE(r.Changed());

  Node* phi = r.replacement();
  Capture<Node*> branch, if_false;
  EXPECT_THAT(
      phi,
      IsPhi(
          static_cast<MachineType>(kTypeBool | kRepTagged), IsFalseConstant(),
          IsWord32Equal(IsLoadField(AccessBuilder::ForMapInstanceType(),
                                    IsLoadField(AccessBuilder::ForMap(), input,
                                                effect, CaptureEq(&if_false)),
                                    effect, _),
                        IsInt32Constant(JS_FUNCTION_TYPE)),
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
          static_cast<MachineType>(kTypeBool | kRepTagged), IsFalseConstant(),
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
          kMachAnyTagged, input,
          IsPhi(kMachAnyTagged, IsLoadField(AccessBuilder::ForValue(), input,
                                            effect, CaptureEq(&if_true1)),
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
