// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-optimization-reducer.h"

#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/use-map.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using MachineOptimizationReducerTest = ReducerTest;

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithXorChain) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto xor1 = t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());
    auto xor2 = t.Asm().WordBinop(xor1, other2, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());
    auto xor3 = t.Asm().WordBinop(xor2, shr, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());

    t.Capture(xor3, "xor3");
    t.Asm().Return(xor3);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithOrChain) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto or1 = t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());
    auto or2 = t.Asm().WordBinop(or1, other2, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());
    auto or3 = t.Asm().WordBinop(or2, shr, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());

    t.Capture(or3, "or3");
    t.Asm().Return(or3);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithXorTree) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto xor_left =
        t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());
    auto xor_right =
        t.Asm().WordBinop(other2, shr, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());
    auto xor_root =
        t.Asm().WordBinop(xor_left, xor_right, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());

    t.Capture(xor_root, "xor_root");
    t.Asm().Return(xor_root);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

TEST_F(MachineOptimizationReducerTest,
       ReduceToWord32RorWithXorTreeMoreThan8Operands) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 9> reps;
  for (int i = 0; i < 9; ++i) reps.push_back(rep32);
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto curr = shl;
    for (auto i = 0; i < 8; ++i) {
      curr = t.Asm().WordBinop(curr, t.template GetParameter<Word32>(i + 1),
                               WordBinopOp::Kind::kBitwiseXor,
                               WordRepresentation::Word32());
    }
    curr = t.Asm().WordBinop(curr, shr, WordBinopOp::Kind::kBitwiseXor,
                             WordRepresentation::Word32());

    t.Asm().Return(curr);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  int reachable_xors = 0;
  base::SmallVector<OpIndex, 32> worklist;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (test.graph().Get(index).Is<ReturnOp>()) {
      worklist.push_back(
          test.graph().Get(index).Cast<ReturnOp>().return_values()[0]);
    }
  }
  std::unordered_set<OpIndex, base::hash<OpIndex>> visited;
  while (!worklist.empty()) {
    OpIndex index = worklist.back();
    worklist.pop_back();
    if (!visited.insert(index).second) continue;
    const Operation& op = test.graph().Get(index);
    if (const ShiftOp* shift = op.TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
      }
    } else if (const WordBinopOp* binop = op.TryCast<WordBinopOp>()) {
      if (binop->kind == WordBinopOp::Kind::kBitwiseXor) {
        reachable_xors++;
      }
    }
    for (OpIndex input : op.inputs()) {
      worklist.push_back(input);
    }
  }
  EXPECT_TRUE(found_ror);
  EXPECT_GE(reachable_xors, 8);
}

TEST_F(MachineOptimizationReducerTest,
       ReduceToWord32RorWithOrTreeMoreThan8Operands) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 9> reps;
  for (int i = 0; i < 9; ++i) reps.push_back(rep32);
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto curr = shl;
    for (auto i = 0; i < 8; ++i) {
      curr = t.Asm().WordBinop(curr, t.template GetParameter<Word32>(i + 1),
                               WordBinopOp::Kind::kBitwiseOr,
                               WordRepresentation::Word32());
    }
    curr = t.Asm().WordBinop(curr, shr, WordBinopOp::Kind::kBitwiseOr,
                             WordRepresentation::Word32());

    t.Asm().Return(curr);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  int reachable_ors = 0;
  base::SmallVector<OpIndex, 32> worklist;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (test.graph().Get(index).Is<ReturnOp>()) {
      worklist.push_back(
          test.graph().Get(index).Cast<ReturnOp>().return_values()[0]);
    }
  }
  std::unordered_set<OpIndex, base::hash<OpIndex>> visited;
  while (!worklist.empty()) {
    OpIndex index = worklist.back();
    worklist.pop_back();
    if (!visited.insert(index).second) continue;
    const Operation& op = test.graph().Get(index);
    if (const ShiftOp* shift = op.TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
      }
    } else if (const WordBinopOp* binop = op.TryCast<WordBinopOp>()) {
      if (binop->kind == WordBinopOp::Kind::kBitwiseOr) {
        reachable_ors++;
      }
    }
    for (OpIndex input : op.inputs()) {
      worklist.push_back(input);
    }
  }
  EXPECT_TRUE(found_ror);
  EXPECT_GE(reachable_ors, 8);
}

namespace {
// SupportedOperations is initialized by the Assembler constructor. These tests
// query it before building their graph, so initialize it explicitly.
bool Word32ShiftIsSafe() {
  SupportedOperations::Initialize();
  return SupportedOperations::word32_shift_is_safe();
}
}  // namespace

// A mask of the shift amount that the machine instruction applies anyway is
// redundant and should be dropped, whether or not it is exactly 0x1f.
TEST_F(MachineOptimizationReducerTest, DropRedundantShiftAmountMask) {
  if (!Word32ShiftIsSafe()) return;

  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 2> reps = {rep32, rep32};
  for (uint32_t mask : {0x1fu, 0x3fu, 0xffffffffu}) {
    auto test = CreateFromGraph(base::VectorOf(reps), [mask](auto& t) {
      auto value = t.template GetParameter<Word32>(0);
      auto amount = t.template GetParameter<Word32>(1);
      V<Word32> masked = V<Word32>::Cast(t.Asm().WordBinop(
          amount, t.Asm().Word32Constant(mask), WordBinopOp::Kind::kBitwiseAnd,
          WordRepresentation::Word32()));
      t.Asm().Return(
          t.Capture(t.Asm().Shift(value, masked, ShiftOp::Kind::kShiftLeft,
                                  WordRepresentation::Word32()),
                    "shift"));
    });

    test.Run<MachineOptimizationReducer>();

    const ShiftOp* shift = test.GetCapturedAs<ShiftOp>("shift");
    ASSERT_NE(shift, nullptr);
    EXPECT_FALSE(test.graph()
                     .Get(shift->right())
                     .template Is<Opmask::kWord32BitwiseAnd>());
  }
}

// A mask that clears bits the machine would have honoured must be kept.
TEST_F(MachineOptimizationReducerTest, KeepNarrowingShiftAmountMask) {
  if (!Word32ShiftIsSafe()) return;

  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 2> reps = {rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto amount = t.template GetParameter<Word32>(1);
    V<Word32> masked = V<Word32>::Cast(t.Asm().WordBinop(
        amount, t.Asm().Word32Constant(0x0f), WordBinopOp::Kind::kBitwiseAnd,
        WordRepresentation::Word32()));
    t.Asm().Return(
        t.Capture(t.Asm().Shift(value, masked, ShiftOp::Kind::kShiftLeft,
                                WordRepresentation::Word32()),
                  "shift"));
  });

  test.Run<MachineOptimizationReducer>();

  const ShiftOp* shift = test.GetCapturedAs<ShiftOp>("shift");
  ASSERT_NE(shift, nullptr);
  EXPECT_TRUE(test.graph()
                  .Get(shift->right())
                  .template Is<Opmask::kWord32BitwiseAnd>());
}

TEST_F(MachineOptimizationReducerTest,
       MakeTupleOpExcludedFromUseCountAndUseMap) {
  OpIndex tuple;
  OpIndex proj0;
  OpIndex proj1;
  OpIndex consumer;

  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 2> reps = {rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [&](auto& t) {
    auto a = t.template GetParameter<Word32>(0);
    auto b = t.template GetParameter<Word32>(1);

    auto binop = t.Asm().OverflowCheckedBinop(
        a, b, OverflowCheckedBinopOp::Kind::kSignedAdd,
        WordRepresentation::Word32());
    tuple = binop;
    auto p0 = t.Asm().template Projection<0>(binop, rep32);
    auto p1 = t.Asm().template Projection<1>(binop, rep32);
    proj0 = p0;
    proj1 = p1;

    // MakeTupleOp itself must have 0 uses.
    EXPECT_TRUE(t.graph().Get(tuple).saturated_use_count.Is(0));
    // Both projections should initially have 0 uses despite being bundled in
    // MakeTupleOp.
    EXPECT_TRUE(t.graph().Get(proj0).saturated_use_count.Is(0));
    EXPECT_TRUE(t.graph().Get(proj1).saturated_use_count.Is(0));

    // Adding a temporary consumer increments proj0's saturated_use_count to 1.
    t.Asm().Word32Add(V<Word32>::Cast(proj0), a);
    EXPECT_TRUE(t.graph().Get(proj0).saturated_use_count.Is(1));

    // Removing the consumer decrements proj0's saturated_use_count back to 0.
    t.graph().RemoveLast();
    EXPECT_TRUE(t.graph().Get(proj0).saturated_use_count.Is(0));

    // Adding the actual consumer leaves saturated_use_count at 1.
    consumer = t.Asm().Word32Add(V<Word32>::Cast(proj0), a);
    EXPECT_TRUE(t.graph().Get(proj0).saturated_use_count.Is(1));
    EXPECT_TRUE(
        t.graph().Get(consumer).IsOnlyUserOf(t.graph().Get(proj0), t.graph()));

    // Adding and removing a MakeTupleOp leaves proj0's saturated_use_count
    // unchanged at 1.
    t.Asm().MakeTuple(proj0, proj1);
    t.graph().RemoveLast();
    EXPECT_TRUE(t.graph().Get(proj0).saturated_use_count.Is(1));

    t.Asm().Return(consumer);
  });

  const Graph& graph = test.graph();
  const Operation& tuple_op = graph.Get(tuple);
  EXPECT_TRUE(tuple_op.Is<MakeTupleOp>());
  EXPECT_TRUE(tuple_op.saturated_use_count.Is(0));

  const Operation& proj0_op = graph.Get(proj0);
  const Operation& proj1_op = graph.Get(proj1);
  EXPECT_TRUE(proj0_op.Is<ProjectionOp>());
  EXPECT_TRUE(proj1_op.Is<ProjectionOp>());
  EXPECT_TRUE(proj0_op.saturated_use_count.Is(1));
  EXPECT_TRUE(proj1_op.saturated_use_count.Is(0));
  EXPECT_TRUE(graph.Get(consumer).IsOnlyUserOf(proj0_op, graph));

  // Verify UseMap ignores MakeTupleOp: unused projection has no uses, and used
  // projection has only consumer (not MakeTupleOp).
  UseMap use_map(graph, test.zone());
  EXPECT_TRUE(use_map.uses(proj1).empty());
  auto proj0_uses = use_map.uses(proj0);
  ASSERT_EQ(proj0_uses.size(), 1u);
  EXPECT_EQ(proj0_uses[0], consumer);
}

#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) || \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_MIPS64)
TEST_F(MachineOptimizationReducerTest, ReduceNestedWord64Add128ToWord64Add3) {
  const RegisterRepresentation rep64 = RegisterRepresentation::Word64();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep64, rep64, rep64};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto a = t.template GetParameter<Word64>(0);
    auto b = t.template GetParameter<Word64>(1);
    auto c = t.template GetParameter<Word64>(2);
    auto zero = t.Asm().Word64Constant(uint64_t{0});

    auto add1 = t.Asm().Add128(a, zero, b, zero);
    auto add1_low = t.Asm().template Projection<0>(add1);
    auto add1_high = t.Asm().template Projection<1>(add1);

    auto add2 = t.Asm().Add128(add1_low, add1_high, c, zero);
    auto sum = t.Asm().template Projection<0>(add2);
    t.Asm().Return(sum);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_add3 = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (test.graph().Get(index).Is<Word64Add3Op>()) {
      found_add3 = true;
      break;
    }
  }
  EXPECT_TRUE(found_add3);
}
#endif

}  // namespace v8::internal::compiler::turboshaft
