// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/machine-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/simplified-optimization-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using MachineLoweringReducerTest = ReducerTest;

TEST_F(MachineLoweringReducerTest,
       Float64ToAdditiveSafeIntegerSingleDeoptBranch) {
  base::SmallVector<RegisterRepresentation, 1> reps = {
      RegisterRepresentation::Float64()};
  for (std::pair<CheckForMinusZeroMode, size_t> mode_and_deopts :
       {std::pair{CheckForMinusZeroMode::kDontCheckForMinusZero, size_t{1}},
        std::pair{CheckForMinusZeroMode::kCheckForMinusZero, size_t{2}}}) {
    CheckForMinusZeroMode mode = mode_and_deopts.first;
    TestInstance test =
        CreateFromGraph(base::VectorOf(reps), [mode](TestInstance& t) {
          V<Float64> input = t.GetParameter<Float64>(0);
          V<EagerFrameState> frame_state = t.BuildFrameState();
          V<Word64> converted =
              t.Asm().ChangeFloat64ToAdditiveSafeIntegerOrDeopt(
                  input, frame_state, mode, FeedbackSource{});
          t.Asm().Return(converted);
        });

    test.Run<MachineLoweringReducer, VariableReducer>();
    EXPECT_EQ(test.CountOp(Opcode::kDeoptimizeIf), mode_and_deopts.second);
  }
}

TEST_F(MachineLoweringReducerTest, ConvertObjectToHeapObject) {
  base::SmallVector<RegisterRepresentation, 1> reps = {
      RegisterRepresentation::Tagged()};
  {
    // Unknown Tagged input lowers to an ObjectIsSmi branch and HeapNumber
    // allocation on the Smi arm without deoptimizing.
    TestInstance test =
        CreateFromGraph(base::VectorOf(reps), [](TestInstance& t) {
          V<Object> input = t.GetParameter<Object>(0);
          V<HeapObject> converted = t.Asm().ConvertToHeapObject(input);
          t.Asm().Return(converted);
        });

    test.Run<SimplifiedOptimizationReducer>();
    test.Run<MachineLoweringReducer, VariableReducer>();
    EXPECT_EQ(test.CountOp(Opcode::kDeoptimizeIf), 0u);
    EXPECT_EQ(test.CountOp(Opcode::kBranch), 1u);
    EXPECT_EQ(test.CountOp(Opcode::kAllocate), 1u);
  }
  {
    // Known HeapObject input folds away completely.
    TestInstance test =
        CreateFromGraph(base::VectorOf(reps), [this](TestInstance& t) {
          V<HeapObject> input =
              t.Asm().HeapConstant(isolate()->factory()->null_value());
          V<HeapObject> converted = t.Asm().ConvertToHeapObject(input);
          t.Asm().Return(converted);
        });

    test.Run<SimplifiedOptimizationReducer>();
    test.Run<MachineLoweringReducer, VariableReducer>();
    EXPECT_EQ(test.CountOp(Opcode::kDeoptimizeIf), 0u);
    EXPECT_EQ(test.CountOp(Opcode::kBranch), 0u);
    EXPECT_EQ(test.CountOp(Opcode::kAllocate), 0u);
  }
  {
    // Known Smi input folds directly to unconditional HeapNumber allocation
    // without a runtime ObjectIsSmi branch.
    TestInstance test =
        CreateFromGraph(base::VectorOf(reps), [](TestInstance& t) {
          V<Smi> input = t.Asm().SmiConstant(Smi::FromInt(42));
          V<HeapObject> converted = t.Asm().ConvertToHeapObject(input);
          t.Asm().Return(converted);
        });

    test.Run<SimplifiedOptimizationReducer>();
    test.Run<MachineLoweringReducer, VariableReducer>();
    EXPECT_EQ(test.CountOp(Opcode::kDeoptimizeIf), 0u);
    EXPECT_EQ(test.CountOp(Opcode::kBranch), 0u);
    EXPECT_EQ(test.CountOp(Opcode::kAllocate), 1u);
  }
}

}  // namespace v8::internal::compiler::turboshaft
