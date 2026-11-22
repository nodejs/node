// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/duplication-optimization-reducer.h"

#include <array>

#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class DuplicationOptimizationReducerTest : public ReducerTest {};

namespace {

// Returns the branch on the comparison (the other branch is on a parameter).
const BranchOp& GetBranchOnComparison(TestInstance& test) {
  const BranchOp* result = nullptr;
  for (const Operation& op : test.graph().AllOperations()) {
    const BranchOp* branch = op.TryCast<BranchOp>();
    if (branch == nullptr ||
        test.graph().Get(branch->condition()).Is<ParameterOp>()) {
      continue;
    }
    CHECK_NULL(result);
    result = branch;
  }
  CHECK_NOT_NULL(result);
  return *result;
}

// The number of comparisons that are not used (and thus not emitted by the
// InstructionSelector).
size_t CountUnusedComparisons(TestInstance& test) {
  size_t count = 0;
  for (const Operation& op : test.graph().AllOperations()) {
    if (op.Is<ComparisonOp>() && op.saturated_use_count.Is(0)) count++;
  }
  return count;
}

constexpr std::array kParameterReps = {
    RegisterRepresentation::Word32(), RegisterRepresentation::Word32(),
    RegisterRepresentation::Word32(), RegisterRepresentation::Word32()};

}  // namespace

// A comparison whose only use is a branch in a later block is re-emitted right
// before that branch, so that the InstructionSelector can combine them.
TEST_F(DuplicationOptimizationReducerTest, SinkComparisonIntoBranchBlock) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    V<Word32> comparison = __ Word32Equal(a, b);
    // An unrelated branch, so that the branch on {comparison} is in
    // another block.
    IF (c) {
      __ Return(__ Word32Constant(2));
    }
    IF (comparison) {
      __ Return(__ Word32Constant(1));
    } ELSE {
      __ Return(__ Word32Constant(0));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  const BranchOp& branch = GetBranchOnComparison(test);
  const Graph& graph = test.graph();
  const ComparisonOp* condition =
      graph.Get(branch.condition()).TryCast<ComparisonOp>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(ComparisonOp::Kind::kEqual, condition->kind);
  EXPECT_EQ(graph.BlockOf(branch.condition()),
            graph.BlockOf(graph.Index(branch)));
  // The original comparison is still in the graph, but unused.
  EXPECT_EQ(2u, test.CountOp(Opcode::kComparison));
  EXPECT_EQ(1u, CountUnusedComparisons(test));
}

// Same for a select.
TEST_F(DuplicationOptimizationReducerTest, SinkComparisonIntoSelectBlock) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    V<Word32> comparison = __ Word32Equal(a, b);
    IF (c) {
      __ Return(__ Word32Constant(2));
    }
    __ Return(__ Word32Select(comparison, a, b));
  });

  test.Run<DuplicationOptimizationReducer>();

  const Graph& graph = test.graph();
  const SelectOp* select = nullptr;
  for (const Operation& op : graph.AllOperations()) {
    if (const SelectOp* s = op.TryCast<SelectOp>()) {
      ASSERT_EQ(select, nullptr);
      select = s;
    }
  }
  ASSERT_NE(select, nullptr);
  ASSERT_TRUE(graph.Get(select->cond()).Is<ComparisonOp>());
  EXPECT_EQ(graph.BlockOf(select->cond()), graph.BlockOf(graph.Index(*select)));
  EXPECT_EQ(1u, CountUnusedComparisons(test));
}

// A comparison that is already in the block of the branch using it is left
// alone.
TEST_F(DuplicationOptimizationReducerTest, KeepComparisonInBranchBlock) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    IF (c) {
      __ Return(__ Word32Constant(2));
    }
    IF (__ Word32Equal(a, b)) {
      __ Return(__ Word32Constant(1));
    } ELSE {
      __ Return(__ Word32Constant(0));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  EXPECT_EQ(1u, test.CountOp(Opcode::kComparison));
  const BranchOp& branch = GetBranchOnComparison(test);
  const Graph& graph = test.graph();
  EXPECT_EQ(graph.BlockOf(branch.condition()),
            graph.BlockOf(graph.Index(branch)));
}

// A comparison that is not in the single predecessor of the block of the
// branch, and whose inputs are only used by it, is left alone, so as not to
// extend the live ranges of its inputs.
TEST_F(DuplicationOptimizationReducerTest, KeepComparisonNotInPredecessor) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    V<Word32> d = Asm.template GetParameter<Word32>(3);
    V<Word32> comparison = __ Word32Equal(a, b);
    IF (c) {
      __ Return(__ Word32Constant(2));
    }
    IF (d) {
      __ Return(__ Word32Constant(3));
    }
    IF (comparison) {
      __ Return(__ Word32Constant(1));
    } ELSE {
      __ Return(__ Word32Constant(0));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  EXPECT_EQ(1u, test.CountOp(Opcode::kComparison));
  const BranchOp& branch = GetBranchOnComparison(test);
  const Graph& graph = test.graph();
  EXPECT_NE(graph.BlockOf(branch.condition()),
            graph.BlockOf(graph.Index(branch)));
}

// When one of its inputs has other uses, a comparison is re-emitted right
// before the branch even if it is further away.
TEST_F(DuplicationOptimizationReducerTest, SinkComparisonWithSharedInput) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    V<Word32> d = Asm.template GetParameter<Word32>(3);
    V<Word32> comparison = __ Word32Equal(a, b);
    IF (c) {
      __ Return(__ Word32Constant(2));
    }
    IF (d) {
      __ Return(__ Word32Constant(3));
    }
    IF (comparison) {
      __ Return(a);
    } ELSE {
      __ Return(__ Word32Constant(0));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  const BranchOp& branch = GetBranchOnComparison(test);
  const Graph& graph = test.graph();
  EXPECT_EQ(graph.BlockOf(branch.condition()),
            graph.BlockOf(graph.Index(branch)));
  EXPECT_EQ(1u, CountUnusedComparisons(test));
}

// A comparison is not re-emitted into a loop header, even though it is in the
// only predecessor of the loop header seen so far: it would be recomputed at
// each iteration.
TEST_F(DuplicationOptimizationReducerTest, KeepComparisonOutOfLoopHeader) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> c = Asm.template GetParameter<Word32>(2);
    V<Word32> comparison = __ Word32Equal(a, b);
    LoopLabel<Word32> loop(&Asm);
    GOTO(loop, c);
    BIND_LOOP(loop, iteration) {
      IF (comparison) {
        __ Return(a);
      }
      GOTO(loop, __ Word32Add(iteration, 1));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  EXPECT_EQ(1u, test.CountOp(Opcode::kComparison));
}

// Only the first use of a comparison in the output graph is considered: here,
// it is a branch in the same block, so the comparison is not re-emitted for
// the second branch.
TEST_F(DuplicationOptimizationReducerTest, KeepComparisonWithEarlierUse) {
  auto test = CreateFromGraph(base::VectorOf(kParameterReps), [](auto& Asm) {
    V<Word32> a = Asm.template GetParameter<Word32>(0);
    V<Word32> b = Asm.template GetParameter<Word32>(1);
    V<Word32> comparison = __ Word32Equal(a, b);
    IF (comparison) {
      __ Return(__ Word32Constant(2));
    }
    IF (comparison) {
      __ Return(__ Word32Constant(1));
    } ELSE {
      __ Return(__ Word32Constant(0));
    }
  });

  test.Run<DuplicationOptimizationReducer>();

  EXPECT_EQ(1u, test.CountOp(Opcode::kComparison));
  EXPECT_EQ(0u, CountUnusedComparisons(test));
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
