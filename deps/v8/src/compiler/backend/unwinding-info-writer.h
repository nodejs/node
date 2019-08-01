// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_UNWINDING_INFO_WRITER_H_
#define V8_COMPILER_BACKEND_UNWINDING_INFO_WRITER_H_

#include "src/flags/flags.h"

#if V8_TARGET_ARCH_ARM
#include "src/compiler/backend/arm/unwinding-info-writer-arm.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/compiler/backend/arm64/unwinding-info-writer-arm64.h"
#elif V8_TARGET_ARCH_X64
#include "src/compiler/backend/x64/unwinding-info-writer-x64.h"
#else

// Placeholder for unsupported architectures.

#include "src/base/logging.h"

namespace v8 {
namespace internal {

class EhFrameWriter;

namespace compiler {

class InstructionBlock;

class UnwindingInfoWriter {
 public:
  explicit UnwindingInfoWriter(Zone* zone) {}

  void SetNumberOfInstructionBlocks(int number) {
    if (FLAG_perf_prof_unwinding_info) UNIMPLEMENTED();
  }

  void BeginInstructionBlock(int pc_offset, const InstructionBlock* block) {
    if (FLAG_perf_prof_unwinding_info) UNIMPLEMENTED();
  }
  void EndInstructionBlock(const InstructionBlock* block) {
    if (FLAG_perf_prof_unwinding_info) UNIMPLEMENTED();
  }

  void Finish(int code_size) {}

  EhFrameWriter* eh_frame_writer() { return nullptr; }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif

#endif  // V8_COMPILER_BACKEND_UNWINDING_INFO_WRITER_H_
