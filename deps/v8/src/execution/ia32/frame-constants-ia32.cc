// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/execution/ia32/frame-constants-ia32.h"

#include "src/codegen/assembler.h"
#include "src/codegen/ia32/assembler-ia32-inl.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

Register JavaScriptFrame::fp_register() { return ebp; }
Register JavaScriptFrame::context_register() { return esi; }
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

#endif  // V8_TARGET_ARCH_IA32
