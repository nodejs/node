// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace compiler {

class BranchEliminationTest : public GraphTest {
 public:
  BranchEliminationTest()
      : machine_(zone(), MachineType::PointerRepresentation(),
                 MachineOperatorBuilder::kNoFlags) {}

  MachineOperatorBuilder* machine() { return &machine_; }

  void Reduce() {
    JSOperatorBuilder javascript(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, nullptr,
                    machine());
    GraphReducer graph_reducer(zone(), graph(), tick_counter(), jsgraph.Dead());
    BranchElimination branch_condition_elimination(&graph_reducer, &jsgraph,
                                                   zone());
    graph_reducer.AddReducer(&branch_condition_elimination);
    graph_reducer.ReduceGraph();
  }

 private:
  MachineOperatorBuilder machine_;
};

TEST_F(BranchEliminationTest, NestedBranchSameTrue) {
  // { return (x ? (x ? 1 : 2) : 3; }
  // should be reduced to
  // { return (x ? 1 : 3; }
  Node* condition = Parameter(0);
  Node* outer_branch =
      graph()->NewNode(common()->Branch(), condition, graph()->start());

  Node* outer_if_true = graph()->NewNode(common()->IfTrue(), outer_branch);
  Node* inner_branch =
      graph()->NewNode(common()->Branch(), condition, outer_if_true);
  Node* inner_if_true = graph()->NewNode(common()->IfTrue(), inner_branch);
  Node* inner_if_false = graph()->NewNode(common()->IfFalse(), inner_branch);
  Node* inner_merge =
      graph()->NewNode(common()->Merge(2), inner_if_true, inner_if_false);
  Node* inner_phi =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       Int32Constant(1), Int32Constant(2), inner_merge);

  Node* outer_if_false = graph()->NewNode(common()->IfFalse(), outer_branch);
  Node* outer_merge =
      graph()->NewNode(common()->Merge(2), inner_merge, outer_if_false);
  Node* outer_phi =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       inner_phi, Int32Constant(3), outer_merge);

  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret = graph()->NewNode(common()->Return(), zero, outer_phi,
                               graph()->start(), outer_merge);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduce();

  // Outer branch should not be rewritten, the inner branch should be discarded.
  EXPECT_THAT(outer_branch, IsBranch(condition, graph()->start()));
  EXPECT_THAT(inner_phi,
              IsPhi(MachineRepresentation::kWord32, IsInt32Constant(1),
                    IsInt32Constant(2), IsMerge(outer_if_true, IsDead())));
}

TEST_F(BranchEliminationTest, NestedBranchSameFalse) {
  // { return (x ? 1 : (x ? 2 : 3); }
  // should be reduced to
  // { return (x ? 1 : 3; }
  Node* condition = Parameter(0);
  Node* outer_branch =
      graph()->NewNode(common()->Branch(), condition, graph()->start());

  Node* outer_if_true = graph()->NewNode(common()->IfTrue(), outer_branch);

  Node* outer_if_false = graph()->NewNode(common()->IfFalse(), outer_branch);
  Node* inner_branch =
      graph()->NewNode(common()->Branch(), condition, outer_if_false);
  Node* inner_if_true = graph()->NewNode(common()->IfTrue(), inner_branch);
  Node* inner_if_false = graph()->NewNode(common()->IfFalse(), inner_branch);
  Node* inner_merge =
      graph()->NewNode(common()->Merge(2), inner_if_true, inner_if_false);
  Node* inner_phi =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       Int32Constant(2), Int32Constant(3), inner_merge);

  Node* outer_merge =
      graph()->NewNode(common()->Merge(2), outer_if_true, inner_merge);
  Node* outer_phi =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       Int32Constant(1), inner_phi, outer_merge);

  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret = graph()->NewNode(common()->Return(), zero, outer_phi,
                               graph()->start(), outer_merge);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduce();

  // Outer branch should not be rewritten, the inner branch should be discarded.
  EXPECT_THAT(outer_branch, IsBranch(condition, graph()->start()));
  EXPECT_THAT(inner_phi,
              IsPhi(MachineRepresentation::kWord32, IsInt32Constant(2),
                    IsInt32Constant(3), IsMerge(IsDead(), outer_if_false)));
}

TEST_F(BranchEliminationTest, BranchAfterDiamond) {
  // { var y = x ? 1 : 2; return y + x ? 3 : 4; }
  // second branch's condition should be replaced with a phi.
  Node* condition = Parameter(0);

  Node* branch1 =
      graph()->NewNode(common()->Branch(), condition, graph()->start());
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* merge1 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
  Node* phi1 =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       Int32Constant(1), Int32Constant(2), merge1);
  // Second branch use the same condition.
  Node* branch2 = graph()->NewNode(common()->Branch(), condition, merge1);
  Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
  Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
  Node* merge2 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
  Node* phi2 =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                       Int32Constant(3), Int32Constant(4), merge1);

  Node* add = graph()->NewNode(machine()->Int32Add(), phi1, phi2);
  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret =
      graph()->NewNode(common()->Return(), zero, add, graph()->start(), merge2);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduce();

  // The branch condition for branch2 should be a phi with constants.
  EXPECT_THAT(branch2,
              IsBranch(IsPhi(MachineRepresentation::kWord32, IsInt32Constant(1),
                             IsInt32Constant(0), merge1),
                       merge1));
}

TEST_F(BranchEliminationTest, BranchInsideLoopSame) {
  // if (x) while (x) { return 2; } else { return 1; }
  // should be rewritten to
  // if (x) while (true) { return 2; } else { return 1; }

  Node* condition = Parameter(0);

  Node* outer_branch =
      graph()->NewNode(common()->Branch(), condition, graph()->start());
  Node* outer_if_true = graph()->NewNode(common()->IfTrue(), outer_branch);

  Node* loop = graph()->NewNode(common()->Loop(1), outer_if_true);
  Node* effect =
      graph()->NewNode(common()->EffectPhi(1), graph()->start(), loop);

  Node* inner_branch = graph()->NewNode(common()->Branch(), condition, loop);

  Node* inner_if_true = graph()->NewNode(common()->IfTrue(), inner_branch);
  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret1 = graph()->NewNode(common()->Return(), zero, Int32Constant(2),
                                effect, inner_if_true);

  Node* inner_if_false = graph()->NewNode(common()->IfFalse(), inner_branch);
  loop->AppendInput(zone(), inner_if_false);
  NodeProperties::ChangeOp(loop, common()->Loop(2));
  effect->InsertInput(zone(), 1, effect);
  NodeProperties::ChangeOp(effect, common()->EffectPhi(2));

  Node* outer_if_false = graph()->NewNode(common()->IfFalse(), outer_branch);
  Node* outer_merge =
      graph()->NewNode(common()->Merge(2), loop, outer_if_false);
  Node* outer_ephi = graph()->NewNode(common()->EffectPhi(2), effect,
                                      graph()->start(), outer_merge);

  Node* ret2 = graph()->NewNode(common()->Return(), zero, Int32Constant(1),
                                outer_ephi, outer_merge);

  Node* terminate = graph()->NewNode(common()->Terminate(), effect, loop);
  graph()->SetEnd(graph()->NewNode(common()->End(3), ret1, ret2, terminate));

  Reduce();

  // Outer branch should not be rewritten, the inner branch should be discarded.
  EXPECT_THAT(outer_branch, IsBranch(condition, graph()->start()));
  EXPECT_THAT(ret1, IsReturn(IsInt32Constant(2), effect, loop));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
