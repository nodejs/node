// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_RISCV64

#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/execution/frames-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void DebugCodegen::GenerateHandleDebuggerStatement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kHandleDebuggerStatement, 0);
  }
  __ MaybeDropFrames();

  // Return to caller.
  __ Ret();
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by a1.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ mv(fp, a1);
  __ Ld(a1, MemOperand(fp, StandardFrameConstants::kFunctionOffset));

  // Pop return address and frame.
  __ LeaveFrame(StackFrame::INTERNAL);

  __ Ld(a0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Lhu(a0,
         FieldMemOperand(a0, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mv(a2, a0);

  __ InvokeFunction(a1, a2, a0, JUMP_FUNCTION);
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64
