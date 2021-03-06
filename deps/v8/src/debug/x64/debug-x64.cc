// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/debug/debug.h"

#include "src/codegen/assembler.h"
#include "src/codegen/macro-assembler.h"
#include "src/debug/liveedit.h"
#include "src/execution/frames-inl.h"
#include "src/objects/objects-inl.h"

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
  __ ret(0);
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by rbx.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.

  __ movq(rbp, rbx);
  __ movq(rdi, Operand(rbp, StandardFrameConstants::kFunctionOffset));
  __ leave();

  __ LoadTaggedPointerField(
      rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movzxwq(
      rbx, FieldOperand(rbx, SharedFunctionInfo::kFormalParameterCountOffset));

  __ InvokeFunction(rdi, no_reg, rbx, rbx, JUMP_FUNCTION);
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
