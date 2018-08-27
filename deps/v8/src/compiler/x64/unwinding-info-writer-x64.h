// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_X64_UNWINDING_INFO_WRITER_X64_H_
#define V8_COMPILER_X64_UNWINDING_INFO_WRITER_X64_H_

#include "src/eh-frame.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionBlock;

class UnwindingInfoWriter {
 public:
  explicit UnwindingInfoWriter(Zone* zone)
      : zone_(zone),
        eh_frame_writer_(zone),
        tracking_fp_(false),
        block_will_exit_(false),
        block_initial_states_(zone) {
    if (enabled()) eh_frame_writer_.Initialize();
  }

  void MaybeIncreaseBaseOffsetAt(int pc_offset, int base_delta) {
    if (enabled() && !tracking_fp_) {
      eh_frame_writer_.AdvanceLocation(pc_offset);
      eh_frame_writer_.IncreaseBaseAddressOffset(base_delta);
    }
  }

  void SetNumberOfInstructionBlocks(int number) {
    if (enabled()) block_initial_states_.resize(number);
  }

  void BeginInstructionBlock(int pc_offset, const InstructionBlock* block);
  void EndInstructionBlock(const InstructionBlock* block);

  void MarkFrameConstructed(int pc_base);
  void MarkFrameDeconstructed(int pc_base);

  void MarkBlockWillExit() { block_will_exit_ = true; }

  void Finish(int code_size) {
    if (enabled()) eh_frame_writer_.Finish(code_size);
  }

  EhFrameWriter* eh_frame_writer() {
    return enabled() ? &eh_frame_writer_ : nullptr;
  }

 private:
  bool enabled() const { return FLAG_perf_prof_unwinding_info; }

  class BlockInitialState : public ZoneObject {
   public:
    BlockInitialState(Register reg, int offset, bool tracking_fp)
        : register_(reg), offset_(offset), tracking_fp_(tracking_fp) {}

    Register register_;
    int offset_;
    bool tracking_fp_;
  };

  Zone* zone_;
  EhFrameWriter eh_frame_writer_;
  bool tracking_fp_;
  bool block_will_exit_;

  ZoneVector<const BlockInitialState*> block_initial_states_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_X64_UNWINDING_INFO_WRITER_X64_H_
