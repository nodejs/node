// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-optimization-reducer.h"

#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
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

}  // namespace v8::internal::compiler::turboshaft
