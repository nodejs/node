// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "src/base/vector.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/branch-elimination-reducer.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"
#include "src/compiler/turboshaft/loop-peeling-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class ControlFlowTest : public ReducerTest {};

// This test creates a chain of empty blocks linked by Gotos. CopyingPhase
// should automatically inline them, leading to the graph containing a single
// block after a single CopyingPhase.
TEST_F(ControlFlowTest, DefaultBlockInlining) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    OpIndex cond = Asm.GetParameter(0);
    for (int i = 0; i < 10000; i++) {
      Label<> l(&Asm);
      GOTO(l);
      BIND(l);
    }
    __ Return(cond);
  });

  test.Run<>();

  ASSERT_EQ(test.graph().block_count(), 1u);
}

// This test creates a fairly large graph, where a pattern similar to this is
// repeating:
//
//       B1        B2
//         \      /
//          \    /
//            Phi
//          Branch(Phi)
//          /     \
//         /       \
//        B3        B4
//
// BranchElimination should remove such branches by cloning the block with the
// branch. In the end, the graph should contain (almost) no branches anymore.
TEST_F(ControlFlowTest, BranchElimination) {
  static constexpr int kSize = 200;

  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Word32> cond =
        __ TaggedEqual(Asm.GetParameter(0), __ SmiConstant(Smi::FromInt(0)));

    Block* end = __ NewBlock();
    V<Word32> cst1 = __ Word32Constant(42);
    std::vector<Block*> destinations;
    for (int i = 0; i < kSize; i++) destinations.push_back(__ NewBlock());
    ZoneVector<SwitchOp::Case>* cases =
        Asm.graph().graph_zone()->template New<ZoneVector<SwitchOp::Case>>(
            Asm.graph().graph_zone());
    for (int i = 0; i < kSize; i++) {
      cases->push_back({i, destinations[i], BranchHint::kNone});
    }
    __ Switch(cond, base::VectorOf(*cases), end);

    __ Bind(destinations[0]);
    Block* b = __ NewBlock();
    __ Branch(cond, b, end);
    __ Bind(b);

    for (int i = 1; i < kSize; i++) {
      V<Word32> cst2 = __ Word32Constant(1);
      __ Goto(destinations[i]);
      __ Bind(destinations[i]);
      V<Word32> phi = __ Phi({cst1, cst2}, RegisterRepresentation::Word32());
      Block* b1 = __ NewBlock();
      __ Branch(phi, b1, end);
      __ Bind(b1);
    }
    __ Goto(end);
    __ Bind(end);

    __ Return(cond);
  });

  // BranchElimination should remove all branches, but will not inline the
  // destinations right away.
  test.Run<BranchEliminationReducer, MachineOptimizationReducer>();

  ASSERT_EQ(test.CountOp(Opcode::kBranch), 0u);

  // An empty phase will then inline the empty intermediate blocks.
  test.Run<>();

  // The graph should now contain 2 blocks per case (1 edge-split + 1 merge),
  // and a few blocks before and after (the switch and the return for
  // instance). To make this test a bit future proof, we just check that the
  // number of block is "number of cases * 2 + a few more blocks" rather than
  // computing the exact expected number of blocks.
  static constexpr int kMaxOtherBlocksCount = 10;
  ASSERT_LE(test.graph().block_count(),
            static_cast<size_t>(kSize * 2 + kMaxOtherBlocksCount));
}

// When the block following a loop header has a single predecessor and contains
// Phis with a single input, loop peeling should be careful not to think that
// these phis are loop phis.
TEST_F(ControlFlowTest, LoopPeelingSingleInputPhi) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    Block* loop = __ NewLoopHeader();
    Block *loop_body = __ NewBlock(), *outside = __ NewBlock();
    __ Goto(loop);
    __ Bind(loop);
    V<Word32> cst = __ Word32Constant(42);
    __ Goto(loop_body);
    __ Bind(loop_body);
    V<Word32> phi = __ Phi({cst}, RegisterRepresentation::Word32());
    __ GotoIf(phi, outside);
    __ Goto(loop);
    __ Bind(outside);
    __ Return(__ Word32Constant(17));
  });

  test.Run<LoopPeelingReducer>();
}

// This test checks that DeadCodeElimination (DCE) eliminates dead blocks
// regardless or whether they are reached through a Goto or a Branch.
TEST_F(ControlFlowTest, DCEGoto) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    // This whole graph only contains unused operations (except for the final
    // Return).
    Block *b1 = __ NewBlock(), *b2 = __ NewBlock(), *b3 = __ NewBlock(),
          *b4 = __ NewBlock();
    __ Bind(b1);
    __ Word32Constant(71);
    __ Goto(b4);
    __ Bind(b4);
    OpIndex cond = Asm.GetParameter(0);
    IF (cond) {
      __ Word32Constant(47);
      __ Goto(b2);
      __ Bind(b2);
      __ Word32Constant(53);
    } ELSE {
      __ Word32Constant(19);
    }
    __ Word32Constant(42);
    __ Goto(b3);
    __ Bind(b3);
    __ Return(__ Word32Constant(17));
  });

  test.Run<DeadCodeEliminationReducer>();

  // The final graph should contain at most 2 blocks (we currently don't
  // eliminate the initial empty block, so we end up with 2 blocks rather than
  // 1; a subsequent optimization phase would remove the empty 1st block).
  ASSERT_LE(test.graph().block_count(), static_cast<size_t>(2));
}

TEST_F(ControlFlowTest, LoopVar) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    OpIndex p = Asm.GetParameter(0);
    Variable v1 = __ NewVariable(RegisterRepresentation::Tagged());
    Variable v2 = __ NewVariable(RegisterRepresentation::Tagged());
    __ SetVariable(v1, p);
    __ SetVariable(v2, p);
    LoopLabel<Word32> loop(&Asm);
    Label<Word32> end(&Asm);
    GOTO(loop, 0);

    BIND_LOOP(loop, iter) {
      GOTO_IF(__ Word32Equal(iter, 42), end, 15);

      __ SetVariable(v1, __ SmiConstant(Smi::FromInt(17)));

      GOTO(loop, __ Word32Add(iter, 1));
    }

    BIND(end, ret);
    OpIndex t = __ Word32Mul(ret, __ GetVariable(v1));
    __ Return(__ Word32BitwiseAnd(t, __ GetVariable(v2)));
  });

  ASSERT_EQ(0u, test.CountOp(Opcode::kPendingLoopPhi));
}

// Checks that when SelectLowering lowers a Select feeding into a Goto to a
// shared merge block, BranchElimination folds the intermediate Phi+Goto merge
// block into the outer merge block.
TEST_F(ControlFlowTest, SelectLoweringMergeFolding) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> done(&Asm);

    IF (c1) {
      GOTO(done, 10);
    } ELSE {
      GOTO(done,
           __ Conditional(c2, __ Word32Constant(20), __ Word32Constant(30)));
    }

    BIND(done, result);
    // Emit enough operations so that `done` is not itself cloned by Return
    // tail-duplication (kMaxOpCountForCloning = 13).
    V<Word32> acc = result;
    for (int i = 0; i < 50; i++) {
      acc = __ Word32Add(acc, i);
    }
    __ Return(acc);
  });

  test.Run<SelectLoweringReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);
}

// Checks that when SelectLowering lowers a Select feeding into a Goto to a
// shared Return merge block, BranchElimination inlines both the intermediate
// Phi+Goto merge block and the Return block in a single pass.
TEST_F(ControlFlowTest, SelectLoweringReturnMergeFolding) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> done(&Asm);

    IF (c1) {
      GOTO(done, 10);
    } ELSE {
      GOTO(done,
           __ Conditional(c2, __ Word32Constant(20), __ Word32Constant(30)));
    }

    BIND(done, result);
    __ Return(result);
  });

  test.Run<SelectLoweringReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 0u);
}

namespace {

// Emits a Switch over {case_count} newly created case blocks. If
// {default_block} is null, the last created block is used as the default target
// instead.
std::vector<Block*> EmitSwitchToNewBlocks(TestInstance& Asm, V<Word32> cond,
                                          int case_count,
                                          Block* default_block = nullptr) {
  std::vector<Block*> destinations;
  destinations.reserve(case_count);
  for (int i = 0; i < case_count; i++) {
    destinations.push_back(__ NewBlock());
  }
  int explicit_cases = default_block ? case_count : case_count - 1;
  ZoneVector<SwitchOp::Case>* cases =
      Asm.graph().graph_zone()->template New<ZoneVector<SwitchOp::Case>>(
          Asm.graph().graph_zone());
  for (int i = 0; i < explicit_cases; i++) {
    cases->push_back({i, destinations[i], BranchHint::kNone});
  }
  __ Switch(cond, base::VectorOf(*cases),
            default_block ? default_block : destinations.back());
  return destinations;
}

// Captures {result} as "outer_phi" and emits enough operations before Return so
// Return tail-duplication (kMaxOpCountForCloning = 13) does not clone it.
void ReturnPreventingTailDuplication(TestInstance& Asm, V<Word32> result) {
  V<Word32> acc = Asm.Capture(result, "outer_phi");
  for (int i = 0; i < 50; i++) {
    acc = __ Word32Add(acc, i);
  }
  __ Return(acc);
}

}  // namespace

// The tests below check that BranchElimination bounds merge cloning so it does
// not blow up predecessor and Phi-input counts in nested diamond/switch CFGs:
//
//   Before folding inner merges M_1..M_3:
//
//                          B_top
//             /              |              \
//            v               v               v
//           S_1             S_2             S_3
//         /  |  \         /  |  \         /  |  \
//        v   v   v       v   v   v       v   v   v
//       E1  E2  E3      E4  E5  E6      E7  E8  E9
//        \   |   /       \   |   /       \   |   /
//         v  v  v         v  v  v         v  v  v
//           M_1             M_2             M_3
//             \              |              /
//              \             |             /
//               v            v            v
//                         B_outer
//                [ p_1 = Phi(v1, v2, v3) ]
//                [ ...                   ]
//                [ p_K = Phi(u1, u2, u3) ]
//
//   After folding M_1..M_3 into split-edge blocks E1..E9:
//
//                          B_top
//             /              |              \
//            v               v               v
//           S_1             S_2             S_3
//         /  |  \         /  |  \         /  |  \
//        v   v   v       v   v   v       v   v   v
//       E1  E2  E3      E4  E5  E6      E7  E8  E9
//        \   \   \       \   |   /       /   /   /
//         \   \   \_______\  |  /_______/   /   /
//          \   \___________\ | /___________/   /
//           \_______________\|/_______________/
//                          v v v
//                         B_outer
//       [ p_1 = Phi(v1,v1,v1, v2,v2,v2, v3,v3,v3) ]
//       [ ...       \___M__/  \___M__/  \___M__/  ]
//       [ p_K = Phi(u1,u1,u1, u2,u2,u2, u3,u3,u3) ]
//
// Checks that BranchElimination does not clone and inline a high-fan-in
// Phi+Goto merge block.
TEST_F(ControlFlowTest, BranchEliminationRespectsInnerMergePredecessorLimit) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  constexpr int kInnerPreds = 9;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> inner_merge(&Asm);
    Label<Word32> outer_merge(&Asm);

    IF (c1) {
      GOTO(outer_merge, 99);
    } ELSE {
      std::vector<Block*> destinations =
          EmitSwitchToNewBlocks(Asm, c2, kInnerPreds);
      for (int i = 0; i < kInnerPreds; i++) {
        __ Bind(destinations[i]);
        GOTO(inner_merge, i);
      }
    }

    BIND(inner_merge, inner_val);
    GOTO(outer_merge, inner_val);

    BIND(outer_merge, result);
    ReturnPreventingTailDuplication(Asm, result);
  });

  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);
  ASSERT_EQ(test.GetCapturedAs<PhiOp>("outer_phi")->input_count, 2);
}

// Checks that BranchElimination does not fold a small Phi+Goto merge block
// into an outer merge block that already exceeds the predecessor limit.
TEST_F(ControlFlowTest, BranchEliminationRespectsOuterMergePredecessorLimit) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  constexpr int kOuterSwitchPreds = 32;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> inner_merge(&Asm);
    Label<Word32> outer_merge(&Asm);

    Block* default_block = __ NewBlock();
    std::vector<Block*> destinations =
        EmitSwitchToNewBlocks(Asm, c1, kOuterSwitchPreds, default_block);

    for (int i = 0; i < kOuterSwitchPreds; i++) {
      __ Bind(destinations[i]);
      GOTO(outer_merge, i);
    }

    __ Bind(default_block);
    IF (c2) {
      GOTO(inner_merge, 100);
    } ELSE {
      GOTO(inner_merge, 200);
    }

    BIND(inner_merge, inner_val);
    GOTO(outer_merge, inner_val);

    BIND(outer_merge, result);
    ReturnPreventingTailDuplication(Asm, result);
  });

  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 2u);
  ASSERT_EQ(test.GetCapturedAs<PhiOp>("outer_phi")->input_count,
            kOuterSwitchPreds + 1);
}

// Checks that BranchElimination stops folding into an outer merge once the
// rewritten outer merge grows past the predecessor limit.
TEST_F(ControlFlowTest, BranchEliminationStopsFoldingWhenOuterMergeGrows) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  constexpr int kOuterSwitchPreds = 16;
  constexpr int kInnerDiamonds = 12;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> outer_merge(&Asm);

    std::vector<Block*> destinations =
        EmitSwitchToNewBlocks(Asm, c1, kOuterSwitchPreds + kInnerDiamonds);

    for (int i = 0; i < kOuterSwitchPreds; i++) {
      __ Bind(destinations[i]);
      GOTO(outer_merge, i);
    }

    for (int k = 0; k < kInnerDiamonds; k++) {
      __ Bind(destinations[kOuterSwitchPreds + k]);
      Label<Word32> inner_merge(&Asm);
      IF (c2) {
        GOTO(inner_merge, 100 + 2 * k);
      } ELSE {
        GOTO(inner_merge, 101 + 2 * k);
      }
      BIND(inner_merge, inner_val);
      GOTO(outer_merge, inner_val);
    }

    BIND(outer_merge, result);
    ReturnPreventingTailDuplication(Asm, result);
  });

  // 16 direct + 12 inner diamonds = 28 <= 32 input predecessors on outer_merge.
  ASSERT_EQ(test.CountOp(Opcode::kPhi),
            static_cast<size_t>(1 + kInnerDiamonds));

  test.Run<BranchEliminationReducer>();
  // Folding stops once the rewritten outer merge grows past 32 predecessors,
  // rather than folding all 12 diamonds to 40 Phi inputs.
  ASSERT_GT(test.CountOp(Opcode::kPhi), 1u);
  ASSERT_LT(test.GetCapturedAs<PhiOp>("outer_phi")->input_count,
            kOuterSwitchPreds + 2 * kInnerDiamonds);
}

// Checks that BranchElimination folds small 0-Phi forwarding merge blocks into
// small outer merge blocks when intermediate branches target distinct blocks.
TEST_F(ControlFlowTest, BranchEliminationFoldsSmallEmptyForwardingMerge) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    V<Word32> c3 = Asm.GetParameter<Word32>(2);
    V<Word32> cst20 = __ Word32Constant(20);
    Label<> inner_merge(&Asm);
    Label<Word32> outer_merge(&Asm);

    IF (c1) {
      GOTO(outer_merge, 10);
    } ELSE {
      IF (c2) {
        IF (c3) {
          GOTO(inner_merge);
        } ELSE {
          GOTO(outer_merge, 30);
        }
      } ELSE {
        GOTO(inner_merge);
      }
    }

    BIND(inner_merge);
    GOTO(outer_merge, cst20);

    BIND(outer_merge, result);
    ReturnPreventingTailDuplication(Asm, result);
  });

  // 9 blocks before folding: entry, if_c1, else_c1, if_c2, if_c3, else_c3,
  // else_c2, inner_merge, outer_merge.
  ASSERT_EQ(test.graph().block_count(), 9u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.graph().block_count(), 8u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);
  ASSERT_EQ(test.GetCapturedAs<PhiOp>("outer_phi")->input_count, 4);
}

// Checks that BranchElimination does not clone a high-fan-in merge block
// ending with a Branch on a Phi (Optimization 2bis).
TEST_F(ControlFlowTest, BranchEliminationRespectsBranchMergePredecessorLimit) {
  constexpr std::array kReps = {RegisterRepresentation::Word32()};
  constexpr int kPreds = 9;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    Label<Word32> branch_merge(&Asm);

    std::vector<Block*> destinations = EmitSwitchToNewBlocks(Asm, c1, kPreds);
    for (int i = 0; i < kPreds; i++) {
      __ Bind(destinations[i]);
      GOTO(branch_merge, i & 1);
    }

    BIND(branch_merge, cond_phi);
    IF (cond_phi) {
      __ Return(__ Word32Constant(10));
    } ELSE {
      __ Return(__ Word32Constant(20));
    }
  });

  ASSERT_EQ(test.CountOp(Opcode::kBranch), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kBranch), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);
}

// Checks that BranchElimination stops cloning a merge ending in a BranchOp once
// a branch destination exceeds the output predecessor limit.
TEST_F(ControlFlowTest,
       BranchEliminationStopsCloningWhenBranchDestinationsGrow) {
  constexpr std::array kReps = {RegisterRepresentation::Word32(),
                                RegisterRepresentation::Word32()};
  constexpr int kOuterCases = 6;
  constexpr int kInnerPreds = 6;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    V<Word32> c2 = Asm.GetParameter<Word32>(1);
    Label<Word32> branch_merge(&Asm);

    std::vector<Block*> outer_cases =
        EmitSwitchToNewBlocks(Asm, c1, kOuterCases);
    for (int k = 0; k < kOuterCases; k++) {
      __ Bind(outer_cases[k]);
      Label<Word32> inner_merge(&Asm);
      std::vector<Block*> inner_cases =
          EmitSwitchToNewBlocks(Asm, c2, kInnerPreds);
      for (int i = 0; i < kInnerPreds; i++) {
        __ Bind(inner_cases[i]);
        // Keep distinct values per inner merge so Label::Bind keeps a PhiOp,
        // and send 35 truthy edges plus a final 0 edge so the branch cannot
        // fold once cloning stops after 33 predecessors on if_true.
        int val = (k == kOuterCases - 1 && i == kInnerPreds - 1)
                      ? 0
                      : (i == 0 ? 2 : 1);
        GOTO(inner_merge, val);
      }
      BIND(inner_merge, inner_val);
      GOTO(branch_merge, inner_val);
    }

    BIND(branch_merge, cond_phi);
    IF (cond_phi) {
      __ Return(__ Word32Constant(10));
    } ELSE {
      __ Return(__ Word32Constant(20));
    }
  });

  ASSERT_EQ(test.CountOp(Opcode::kBranch), 1u);

  test.Run<BranchEliminationReducer, MachineOptimizationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kBranch), 1u);
}

// Checks that BranchElimination does not clone a high-fan-in merge block
// ending with a Return (Optimization 3) even when predecessors are not empty
// Switch split-edge blocks.
TEST_F(ControlFlowTest, BranchEliminationRespectsReturnMergePredecessorLimit) {
  constexpr std::array kReps = {RegisterRepresentation::Word32()};
  constexpr int kPreds = 9;
  auto test = CreateFromGraph(base::VectorOf(kReps), [](TestInstance& Asm) {
    V<Word32> c1 = Asm.GetParameter<Word32>(0);
    Label<Word32> return_merge(&Asm);

    std::vector<Block*> destinations = EmitSwitchToNewBlocks(Asm, c1, kPreds);
    for (int i = 0; i < kPreds; i++) {
      __ Bind(destinations[i]);
      GOTO(return_merge, __ Word32Add(c1, i));
    }

    BIND(return_merge, ret_val);
    __ Return(ret_val);
  });

  ASSERT_EQ(test.CountOp(Opcode::kReturn), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);

  test.Run<BranchEliminationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kReturn), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kPhi), 1u);
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
