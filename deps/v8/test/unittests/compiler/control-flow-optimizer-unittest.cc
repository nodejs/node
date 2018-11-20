// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class ControlFlowOptimizerTest : public GraphTest {
 public:
  explicit ControlFlowOptimizerTest(int num_parameters = 3)
      : GraphTest(num_parameters), machine_(zone()), javascript_(zone()) {}
  ~ControlFlowOptimizerTest() override {}

 protected:
  void Optimize() {
    ControlFlowOptimizer optimizer(graph(), common(), machine(), zone());
    optimizer.Optimize();
  }

  JSOperatorBuilder* javascript() { return &javascript_; }
  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
};


TEST_F(ControlFlowOptimizerTest, BuildSwitch1) {
  Node* index = Parameter(0);
  Node* branch0 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(0)),
      start());
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* branch1 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(1)),
      if_false0);
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* merge =
      graph()->NewNode(common()->Merge(3), if_true0, if_true1, if_false1);
  graph()->SetEnd(graph()->NewNode(common()->End(1), merge));
  Optimize();
  Capture<Node*> switch_capture;
  EXPECT_THAT(
      end(), IsEnd(IsMerge(
                 IsIfValue(IfValueParameters(0, 1), CaptureEq(&switch_capture)),
                 IsIfValue(IfValueParameters(1, 2), CaptureEq(&switch_capture)),
                 IsIfDefault(AllOf(CaptureEq(&switch_capture),
                                   IsSwitch(index, start()))))));
}


TEST_F(ControlFlowOptimizerTest, BuildSwitch2) {
  Node* input = Parameter(0);
  Node* context = Parameter(1);
  Node* index = graph()->NewNode(javascript()->ToNumber(), input, context,
                                 start(), start(), start());
  Node* if_success = graph()->NewNode(common()->IfSuccess(), index);
  Node* branch0 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(0)),
      if_success);
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* branch1 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(1)),
      if_false0);
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* merge =
      graph()->NewNode(common()->Merge(3), if_true0, if_true1, if_false1);
  graph()->SetEnd(graph()->NewNode(common()->End(1), merge));
  Optimize();
  Capture<Node*> switch_capture;
  EXPECT_THAT(
      end(), IsEnd(IsMerge(
                 IsIfValue(IfValueParameters(0, 1), CaptureEq(&switch_capture)),
                 IsIfValue(IfValueParameters(1, 2), CaptureEq(&switch_capture)),
                 IsIfDefault(AllOf(CaptureEq(&switch_capture),
                                   IsSwitch(index, IsIfSuccess(index)))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
