// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_S390_UNWINDING_INFO_WRITER_S390_H_
#define V8_COMPILER_BACKEND_S390_UNWINDING_INFO_WRITER_S390_H_

#include "src/diagnostics/eh-frame.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionBlock;

class UnwindingInfoWriter {
 public:
  explicit UnwindingInfoWriter(Zone* zone)
      : zone_(zone),
        eh_frame_writer_(zone),
        saved_lr_(false),
        block_will_exit_(false),
        block_initial_states_(zone) {
    if (enabled()) eh_frame_writer_.Initialize();
  }

  void SetNumberOfInstructionBlocks(int number) {
    if (enabled()) block_initial_states_.resize(number);
  }

  void BeginInstructionBlock(int pc_offset, const InstructionBlock* block);
  void EndInstructionBlock(const InstructionBlock* block);

  void MarkLinkRegisterOnTopOfStack(int pc_offset);
  void MarkPopLinkRegisterFromTopOfStack(int pc_offset);

  void MarkFrameConstructed(int at_pc);
  void MarkFrameDeconstructed(int at_pc);

  void MarkBlockWillExit() { block_will_exit_ = true; }

  void Finish(int code_size) {
    if (enabled()) eh_frame_writer_.Finish(code_size);
  }

  EhFrameWriter* eh_frame_writer() {
    return enabled() ? &eh_frame_writer_ : nullptr;
  }

 private:
  bool enabled() const { return v8_flags.perf_prof_unwinding_info; }

  class BlockInitialState : public ZoneObject {
   public:
    explicit BlockInitialState(bool saved_lr) : saved_lr_(saved_lr) {}

    bool saved_lr_;
  };

  Zone* zone_;
  EhFrameWriter eh_frame_writer_;
  bool saved_lr_;
  bool block_will_exit_;

  ZoneVector<const BlockInitialState*> block_initial_states_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_S390_UNWINDING_INFO_WRITER_S390_H_
