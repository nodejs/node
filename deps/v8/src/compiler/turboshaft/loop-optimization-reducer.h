// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_LOOP_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_LOOP_OPTIMIZATION_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class LoopOptimizationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(LoopOptimization)

  // TODO(nicohartmann): This is currently only used for testing. Actual
  // optimizations will come with following CLs.
  V<None> REDUCE(PrepareForLoop)(V<EagerFrameState> frame_state,
                                 FeedbackSource feedback_source) {
    CHECK(v8_flags.turboshaft_loop_optimization);
    if (feedback_source.IsValid()) {
      SpeculationMode speculation_mode =
          data_->broker()->GetFeedbackForJumpLoop(feedback_source);
      if (speculation_mode == SpeculationMode::kAllowSpeculation) {
        // We emit a check here that will always deopt if speculation is enabled
        // and this can be used to test correct behavior of the speculation bit
        // from mjsunit tests.
        __ Deoptimize(frame_state, DeoptimizeReason::kLoopSpeculationFailed,
                      feedback_source);
      }
    }
    return V<None>::Invalid();
  }

 private:
  PipelineData* data_ = __ data();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_LOOP_OPTIMIZATION_REDUCER_H_
