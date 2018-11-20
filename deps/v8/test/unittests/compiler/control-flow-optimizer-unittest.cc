// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/js-graph.h"
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
      : GraphTest(num_parameters), machine_(zone()) {}
  ~ControlFlowOptimizerTest() OVERRIDE {}

 protected:
  void Optimize() {
    JSOperatorBuilder javascript(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, machine());
    ControlFlowOptimizer optimizer(&jsgraph, zone());
    optimizer.Optimize();
  }

  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
};


TEST_F(ControlFlowOptimizerTest, Switch) {
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
  graph()->SetEnd(graph()->NewNode(common()->End(), merge));
  Optimize();
  Capture<Node*> switch_capture;
  EXPECT_THAT(end(),
              IsEnd(IsMerge(IsIfValue(0, CaptureEq(&switch_capture)),
                            IsIfValue(1, CaptureEq(&switch_capture)),
                            IsIfDefault(AllOf(CaptureEq(&switch_capture),
                                              IsSwitch(index, start()))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
