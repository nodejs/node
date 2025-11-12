// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/execution/x64/frame-constants-x64.h"

#include "src/codegen/x64/assembler-x64-inl.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

Register JavaScriptFrame::fp_register() { return rbp; }
Register JavaScriptFrame::context_register() { return rsi; }
Register JavaScriptFrame::constant_pool_pointer_register() { UNREACHABLE(); }

int UnoptimizedFrameConstants::RegisterStackSlotCount(int register_count) {
  return register_count;
}

int BuiltinContinuationFrameConstants::PaddingSlotCount(int register_count) {
  USE(register_count);
  return 0;
}

// static
intptr_t MaglevFrame::StackGuardFrameSize(int register_input_count) {
  // Include one extra slot for the single argument into StackGuardWithGap +
  // register input count.
  return StandardFrameConstants::kFixedFrameSizeFromFp +
         (1 + register_input_count) * kSystemPointerSize;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
