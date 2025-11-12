// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/dead-code-elimination.h"

#include "src/compiler/common-operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {
namespace dead_code_elimination_unittest {

class DeadCodeEliminationTest : public GraphTest {
 public:
  explicit DeadCodeEliminationTest(int num_parameters = 4)
      : GraphTest(num_parameters) {}
  ~DeadCodeEliminationTest() override = default;

 protected:
  Reduction Reduce(AdvancedReducer::Editor* editor, Node* node) {
    DeadCodeElimination reducer(editor, graph(), common(), zone());
    return reducer.Reduce(node);
  }

  Reduction Reduce(Node* node) {
    StrictMock<MockAdvancedReducerEditor> editor;
    return Reduce(&editor, node);
  }
};


namespace {

const MachineRepresentation kMachineRepresentations[] = {
    MachineRepresentation::kBit,     MachineRepresentation::kWord8,
    MachineRepresentation::kWord16,  MachineRepresentation::kWord32,
    MachineRepresentation::kWord64,  MachineRepresentation::kFloat32,
    MachineRepresentation::kFloat64, MachineRepresentation::kTagged};


const int kMaxInputs = 16;


const Operator kOp0(0, Operator::kNoProperties, "Op0", 1, 1, 1, 1, 1, 1);

}  // namespace


// -----------------------------------------------------------------------------
// General dead propagation


TEST_F(DeadCodeEliminationTest, GeneralDeadPropagation) {
  Node* const value = Parameter(0);
  Node* const effect = graph()->start();
  Node* const dead = graph()->NewNode(common()->Dead());
  Node* const node = graph()->NewNode(&kOp0, value, effect, dead);
  Reduction const r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}


// -----------------------------------------------------------------------------
// Branch


TEST_F(DeadCodeEliminationTest, BranchWithDeadControlInput) {
  BranchHint const kHints[] = {BranchHint::kNone, BranchHint::kTrue,
                               BranchHint::kFalse};
  TRACED_FOREACH(BranchHint, hint, kHints) {
    Reduction const r =
        Reduce(graph()->NewNode(common()->Branch(hint), Parameter(0),
                                graph()->NewNode(common()->Dead())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


// -----------------------------------------------------------------------------
// IfTrue


TEST_F(DeadCodeEliminationTest, IfTrueWithDeadInput) {
  Reduction const r = Reduce(
      graph()->NewNode(common()->IfTrue(), graph()->NewNode(common()->Dead())));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}


// -----------------------------------------------------------------------------
// IfFalse


TEST_F(DeadCodeEliminationTest, IfFalseWithDeadInput) {
  Reduction const r = Reduce(graph()->NewNode(
      common()->IfFalse(), graph()->NewNode(common()->Dead())));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}


// -----------------------------------------------------------------------------
// IfSuccess


TEST_F(DeadCodeEliminationTest, IfSuccessWithDeadInput) {
  Reduction const r = Reduce(graph()->NewNode(
      common()->IfSuccess(), graph()->NewNode(common()->Dead())));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}


// -----------------------------------------------------------------------------
// IfException


TEST_F(DeadCodeEliminationTest, IfExceptionWithDeadControlInput) {
  Reduction const r =
      Reduce(graph()->NewNode(common()->IfException(), graph()->start(),
                              graph()->NewNode(common()->Dead())));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}


// -----------------------------------------------------------------------------
// End


TEST_F(DeadCodeEliminationTest, EndWithDeadAndStart) {
  Node* const dead = graph()->NewNode(common()->Dead());
  Node* const start = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(common()->End(2), dead, start));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsEnd(start));
}


TEST_F(DeadCodeEliminationTest, EndWithOnlyDeadInputs) {
  Node* inputs[kMaxInputs];
  TRACED_FORRANGE(int, input_count, 1, kMaxInputs - 1) {
    for (int i = 0; i < input_count; ++i) {
      inputs[i] = graph()->NewNode(common()->Dead());
    }
    Reduction const r = Reduce(
        graph()->NewNode(common()->End(input_count), input_count, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


// -----------------------------------------------------------------------------
// Merge


TEST_F(DeadCodeEliminationTest, MergeWithOnlyDeadInputs) {
  Node* inputs[kMaxInputs + 1];
  TRACED_FORRANGE(int, input_count, 1, kMaxInputs - 1) {
    for (int i = 0; i < input_count; ++i) {
      inputs[i] = graph()->NewNode(common()->Dead());
    }
    Reduction const r = Reduce(
        graph()->NewNode(common()->Merge(input_count), input_count, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


TEST_F(DeadCodeEliminationTest, MergeWithOneLiveAndOneDeadInput) {
  Node* const v0 = Parameter(0);
  Node* const v1 = Parameter(1);
  Node* const c0 =
      graph()->NewNode(&kOp0, v0, graph()->start(), graph()->start());
  Node* const c1 = graph()->NewNode(common()->Dead());
  Node* const e0 = graph()->NewNode(&kOp0, v0, graph()->start(), c0);
  Node* const e1 = graph()->NewNode(&kOp0, v1, graph()->start(), c1);
  Node* const merge = graph()->NewNode(common()->Merge(2), c0, c1);
  Node* const phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), v0, v1, merge);
  Node* const ephi = graph()->NewNode(common()->EffectPhi(2), e0, e1, merge);
  StrictMock<MockAdvancedReducerEditor> editor;
  EXPECT_CALL(editor, Replace(phi, v0));
  EXPECT_CALL(editor, Replace(ephi, e0));
  Reduction const r = Reduce(&editor, merge);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(c0, r.replacement());
}


TEST_F(DeadCodeEliminationTest, MergeWithTwoLiveAndTwoDeadInputs) {
  Node* const v0 = Parameter(0);
  Node* const v1 = Parameter(1);
  Node* const v2 = Parameter(2);
  Node* const v3 = Parameter(3);
  Node* const c0 =
      graph()->NewNode(&kOp0, v0, graph()->start(), graph()->start());
  Node* const c1 = graph()->NewNode(common()->Dead());
  Node* const c2 = graph()->NewNode(common()->Dead());
  Node* const c3 = graph()->NewNode(&kOp0, v3, graph()->start(), c0);
  Node* const e0 = graph()->start();
  Node* const e1 = graph()->NewNode(&kOp0, v1, e0, c0);
  Node* const e2 = graph()->NewNode(&kOp0, v2, e1, c0);
  Node* const e3 = graph()->NewNode(&kOp0, v3, graph()->start(), c3);
  Node* const merge = graph()->NewNode(common()->Merge(4), c0, c1, c2, c3);
  Node* const phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 4), v0, v1, v2, v3, merge);
  Node* const ephi =
      graph()->NewNode(common()->EffectPhi(4), e0, e1, e2, e3, merge);
  StrictMock<MockAdvancedReducerEditor> editor;
  EXPECT_CALL(editor, Revisit(phi));
  EXPECT_CALL(editor, Revisit(ephi));
  Reduction const r = Reduce(&editor, merge);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsMerge(c0, c3));
  EXPECT_THAT(phi,
              IsPhi(MachineRepresentation::kTagged, v0, v3, r.replacement()));
  EXPECT_THAT(ephi, IsEffectPhi(e0, e3, r.replacement()));
}


// -----------------------------------------------------------------------------
// Loop


TEST_F(DeadCodeEliminationTest, LoopWithDeadFirstInput) {
  Node* inputs[kMaxInputs + 1];
  TRACED_FORRANGE(int, input_count, 1, kMaxInputs - 1) {
    inputs[0] = graph()->NewNode(common()->Dead());
    for (int i = 1; i < input_count; ++i) {
      inputs[i] = graph()->start();
    }
    Reduction const r = Reduce(
        graph()->NewNode(common()->Loop(input_count), input_count, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


TEST_F(DeadCodeEliminationTest, LoopWithOnlyDeadInputs) {
  Node* inputs[kMaxInputs + 1];
  TRACED_FORRANGE(int, input_count, 1, kMaxInputs - 1) {
    for (int i = 0; i < input_count; ++i) {
      inputs[i] = graph()->NewNode(common()->Dead());
    }
    Reduction const r = Reduce(
        graph()->NewNode(common()->Loop(input_count), input_count, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


TEST_F(DeadCodeEliminationTest, LoopWithOneLiveAndOneDeadInput) {
  Node* const v0 = Parameter(0);
  Node* const v1 = Parameter(1);
  Node* const c0 =
      graph()->NewNode(&kOp0, v0, graph()->start(), graph()->start());
  Node* const c1 = graph()->NewNode(common()->Dead());
  Node* const e0 = graph()->NewNode(&kOp0, v0, graph()->start(), c0);
  Node* const e1 = graph()->NewNode(&kOp0, v1, graph()->start(), c1);
  Node* const loop = graph()->NewNode(common()->Loop(2), c0, c1);
  Node* const phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), v0, v1, loop);
  Node* const ephi = graph()->NewNode(common()->EffectPhi(2), e0, e1, loop);
  Node* const terminate = graph()->NewNode(common()->Terminate(), ephi, loop);
  StrictMock<MockAdvancedReducerEditor> editor;
  EXPECT_CALL(editor, Replace(phi, v0));
  EXPECT_CALL(editor, Replace(ephi, e0));
  EXPECT_CALL(editor, Replace(terminate, IsDead()));
  Reduction const r = Reduce(&editor, loop);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(c0, r.replacement());
}


TEST_F(DeadCodeEliminationTest, LoopWithTwoLiveAndTwoDeadInputs) {
  Node* const v0 = Parameter(0);
  Node* const v1 = Parameter(1);
  Node* const v2 = Parameter(2);
  Node* const v3 = Parameter(3);
  Node* const c0 =
      graph()->NewNode(&kOp0, v0, graph()->start(), graph()->start());
  Node* const c1 = graph()->NewNode(common()->Dead());
  Node* const c2 = graph()->NewNode(common()->Dead());
  Node* const c3 = graph()->NewNode(&kOp0, v3, graph()->start(), c0);
  Node* const e0 = graph()->start();
  Node* const e1 = graph()->NewNode(&kOp0, v1, e0, c0);
  Node* const e2 = graph()->NewNode(&kOp0, v2, e1, c0);
  Node* const e3 = graph()->NewNode(&kOp0, v3, graph()->start(), c3);
  Node* const loop = graph()->NewNode(common()->Loop(4), c0, c1, c2, c3);
  Node* const phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 4), v0, v1, v2, v3, loop);
  Node* const ephi =
      graph()->NewNode(common()->EffectPhi(4), e0, e1, e2, e3, loop);
  StrictMock<MockAdvancedReducerEditor> editor;
  EXPECT_CALL(editor, Revisit(phi));
  EXPECT_CALL(editor, Revisit(ephi));
  Reduction const r = Reduce(&editor, loop);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsLoop(c0, c3));
  EXPECT_THAT(phi,
              IsPhi(MachineRepresentation::kTagged, v0, v3, r.replacement()));
  EXPECT_THAT(ephi, IsEffectPhi(e0, e3, r.replacement()));
}


// -----------------------------------------------------------------------------
// Phi


TEST_F(DeadCodeEliminationTest, PhiWithDeadControlInput) {
  Node* inputs[kMaxInputs + 1];
  TRACED_FOREACH(MachineRepresentation, rep, kMachineRepresentations) {
    TRACED_FORRANGE(int, input_count, 1, kMaxInputs) {
      for (int i = 0; i < input_count; ++i) {
        inputs[i] = Parameter(i);
      }
      inputs[input_count] = graph()->NewNode(common()->Dead());
      Reduction const r = Reduce(graph()->NewNode(
          common()->Phi(rep, input_count), input_count + 1, inputs));
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsDead());
    }
  }
}


// -----------------------------------------------------------------------------
// EffectPhi


TEST_F(DeadCodeEliminationTest, EffectPhiWithDeadControlInput) {
  Node* inputs[kMaxInputs + 1];
  TRACED_FORRANGE(int, input_count, 1, kMaxInputs) {
    for (int i = 0; i < input_count; ++i) {
      inputs[i] = graph()->start();
    }
    inputs[input_count] = graph()->NewNode(common()->Dead());
    Reduction const r = Reduce(graph()->NewNode(
        common()->EffectPhi(input_count), input_count + 1, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsDead());
  }
}


// -----------------------------------------------------------------------------
// Terminate


TEST_F(DeadCodeEliminationTest, TerminateWithDeadControlInput) {
  Reduction const r =
      Reduce(graph()->NewNode(common()->Terminate(), graph()->start(),
                              graph()->NewNode(common()->Dead())));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsDead());
}

}  // namespace dead_code_elimination_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
