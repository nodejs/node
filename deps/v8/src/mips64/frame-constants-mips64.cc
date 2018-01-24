// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/assembler.h"
#include "src/frame-constants.h"
#include "src/mips64/assembler-mips64-inl.h"
#include "src/mips64/assembler-mips64.h"

#include "src/mips64/frame-constants-mips64.h"

namespace v8 {
namespace internal {

Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() { UNREACHABLE(); }

int InterpreterFrameConstants::RegisterStackSlotCount(int register_count) {
  return register_count;
}

int BuiltinContinuationFrameConstants::PaddingSlotCount(int register_count) {
  USE(register_count);
  return 0;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
