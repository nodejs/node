// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/machine-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/operations.h"
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

}  // namespace v8::internal::compiler::turboshaft
