// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/effect-control-linearizer.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/schedule.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

using testing::Capture;

class EffectControlLinearizerTest : public GraphTest {
 public:
  EffectControlLinearizerTest()
      : GraphTest(3),
        machine_(zone()),
        javascript_(zone()),
        simplified_(zone()),
        jsgraph_(isolate(), graph(), common(), &javascript_, &simplified_,
                 &machine_) {
    source_positions_ = new (zone()) SourcePositionTable(graph());
  }

  JSGraph* jsgraph() { return &jsgraph_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  SourcePositionTable* source_positions() { return source_positions_; }

 private:
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
  SourcePositionTable* source_positions_;
};

namespace {

BasicBlock* AddBlockToSchedule(Schedule* schedule) {
  BasicBlock* block = schedule->NewBasicBlock();
  block->set_rpo_number(static_cast<int32_t>(schedule->rpo_order()->size()));
  schedule->rpo_order()->push_back(block);
  return block;
}

}  // namespace

TEST_F(EffectControlLinearizerTest, SimpleLoad) {
  Schedule schedule(zone());

  // Create the graph.
  Node* heap_number = NumberConstant(0.5);
  Node* load = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), heap_number,
      graph()->start(), graph()->start());
  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret = graph()->NewNode(common()->Return(), zero, load, graph()->start(),
                               graph()->start());

  // Build the basic block structure.
  BasicBlock* start = schedule.start();
  schedule.rpo_order()->push_back(start);
  start->set_rpo_number(0);

  // Populate the basic blocks with nodes.
  schedule.AddNode(start, graph()->start());
  schedule.AddNode(start, heap_number);
  schedule.AddNode(start, load);
  schedule.AddReturn(start, ret);

  // Run the state effect introducer.
  EffectControlLinearizer introducer(jsgraph(), &schedule, zone(),
                                     source_positions());
  introducer.Run();

  EXPECT_THAT(load,
              IsLoadField(AccessBuilder::ForHeapNumberValue(), heap_number,
                          graph()->start(), graph()->start()));
  // The return should have reconnected effect edge to the load.
  EXPECT_THAT(ret, IsReturn(load, load, graph()->start()));
}

TEST_F(EffectControlLinearizerTest, DiamondLoad) {
  Schedule schedule(zone());

  // Create the graph.
  Node* branch =
      graph()->NewNode(common()->Branch(), Int32Constant(0), graph()->start());

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* heap_number = NumberConstant(0.5);
  Node* vtrue = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), heap_number,
      graph()->start(), if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = Float64Constant(2);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kFloat64, 2), vtrue, vfalse, merge);

  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret =
      graph()->NewNode(common()->Return(), zero, phi, graph()->start(), merge);

  // Build the basic block structure.
  BasicBlock* start = schedule.start();
  schedule.rpo_order()->push_back(start);
  start->set_rpo_number(0);

  BasicBlock* tblock = AddBlockToSchedule(&schedule);
  BasicBlock* fblock = AddBlockToSchedule(&schedule);
  BasicBlock* mblock = AddBlockToSchedule(&schedule);

  // Populate the basic blocks with nodes.
  schedule.AddNode(start, graph()->start());
  schedule.AddBranch(start, branch, tblock, fblock);

  schedule.AddNode(tblock, if_true);
  schedule.AddNode(tblock, heap_number);
  schedule.AddNode(tblock, vtrue);
  schedule.AddGoto(tblock, mblock);

  schedule.AddNode(fblock, if_false);
  schedule.AddNode(fblock, vfalse);
  schedule.AddGoto(fblock, mblock);

  schedule.AddNode(mblock, merge);
  schedule.AddNode(mblock, phi);
  schedule.AddReturn(mblock, ret);

  // Run the state effect introducer.
  EffectControlLinearizer introducer(jsgraph(), &schedule, zone(),
                                     source_positions());
  introducer.Run();

  // The effect input to the return should be an effect phi with the
  // newly introduced effectful change operators.
  ASSERT_THAT(
      ret, IsReturn(phi, IsEffectPhi(vtrue, graph()->start(), merge), merge));
}

TEST_F(EffectControlLinearizerTest, LoopLoad) {
  Schedule schedule(zone());

  // Create the graph.
  Node* loop = graph()->NewNode(common()->Loop(1), graph()->start());
  Node* effect_phi =
      graph()->NewNode(common()->EffectPhi(1), graph()->start(), loop);

  Node* cond = Int32Constant(0);
  Node* branch = graph()->NewNode(common()->Branch(), cond, loop);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);

  loop->AppendInput(zone(), if_false);
  NodeProperties::ChangeOp(loop, common()->Loop(2));

  effect_phi->InsertInput(zone(), 1, effect_phi);
  NodeProperties::ChangeOp(effect_phi, common()->EffectPhi(2));

  Node* heap_number = NumberConstant(0.5);
  Node* load = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), heap_number,
      graph()->start(), loop);

  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret =
      graph()->NewNode(common()->Return(), zero, load, effect_phi, if_true);

  // Build the basic block structure.
  BasicBlock* start = schedule.start();
  schedule.rpo_order()->push_back(start);
  start->set_rpo_number(0);

  BasicBlock* lblock = AddBlockToSchedule(&schedule);
  BasicBlock* fblock = AddBlockToSchedule(&schedule);
  BasicBlock* rblock = AddBlockToSchedule(&schedule);

  // Populate the basic blocks with nodes.
  schedule.AddNode(start, graph()->start());
  schedule.AddGoto(start, lblock);

  schedule.AddNode(lblock, loop);
  schedule.AddNode(lblock, effect_phi);
  schedule.AddNode(lblock, heap_number);
  schedule.AddNode(lblock, load);
  schedule.AddNode(lblock, cond);
  schedule.AddBranch(lblock, branch, rblock, fblock);

  schedule.AddNode(fblock, if_false);
  schedule.AddGoto(fblock, lblock);

  schedule.AddNode(rblock, if_true);
  schedule.AddReturn(rblock, ret);

  // Run the state effect introducer.
  EffectControlLinearizer introducer(jsgraph(), &schedule, zone(),
                                     source_positions());
  introducer.Run();

  ASSERT_THAT(ret, IsReturn(load, load, if_true));
  EXPECT_THAT(load, IsLoadField(AccessBuilder::ForHeapNumberValue(),
                                heap_number, effect_phi, loop));
}

TEST_F(EffectControlLinearizerTest, CloneBranch) {
  Schedule schedule(zone());

  Node* cond0 = Parameter(0);
  Node* cond1 = Parameter(1);
  Node* cond2 = Parameter(2);
  Node* branch0 = graph()->NewNode(common()->Branch(), cond0, start());
  Node* control1 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* control2 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* merge0 = graph()->NewNode(common()->Merge(2), control1, control2);
  Node* phi0 = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2),
                                cond1, cond2, merge0);
  Node* branch = graph()->NewNode(common()->Branch(), phi0, merge0);
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  graph()->SetEnd(graph()->NewNode(common()->End(1), merge));

  BasicBlock* start = schedule.start();
  schedule.rpo_order()->push_back(start);
  start->set_rpo_number(0);

  BasicBlock* f1block = AddBlockToSchedule(&schedule);
  BasicBlock* t1block = AddBlockToSchedule(&schedule);
  BasicBlock* bblock = AddBlockToSchedule(&schedule);

  BasicBlock* f2block = AddBlockToSchedule(&schedule);
  BasicBlock* t2block = AddBlockToSchedule(&schedule);
  BasicBlock* mblock = AddBlockToSchedule(&schedule);

  // Populate the basic blocks with nodes.
  schedule.AddNode(start, graph()->start());

  schedule.AddBranch(start, branch0, t1block, f1block);

  schedule.AddNode(t1block, control1);
  schedule.AddGoto(t1block, bblock);

  schedule.AddNode(f1block, control2);
  schedule.AddGoto(f1block, bblock);

  schedule.AddNode(bblock, merge0);
  schedule.AddNode(bblock, phi0);
  schedule.AddBranch(bblock, branch, t2block, f2block);

  schedule.AddNode(t2block, if_true);
  schedule.AddGoto(t2block, mblock);

  schedule.AddNode(f2block, if_false);
  schedule.AddGoto(f2block, mblock);

  schedule.AddNode(mblock, merge);
  schedule.AddNode(mblock, graph()->end());

  EffectControlLinearizer introducer(jsgraph(), &schedule, zone(),
                                     source_positions());
  introducer.Run();

  Capture<Node *> branch1_capture, branch2_capture;
  EXPECT_THAT(
      end(),
      IsEnd(IsMerge(IsMerge(IsIfTrue(CaptureEq(&branch1_capture)),
                            IsIfTrue(CaptureEq(&branch2_capture))),
                    IsMerge(IsIfFalse(AllOf(CaptureEq(&branch1_capture),
                                            IsBranch(cond1, control1))),
                            IsIfFalse(AllOf(CaptureEq(&branch2_capture),
                                            IsBranch(cond2, control2)))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
