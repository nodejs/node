// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/debug/debug.h"

#include "src/codegen/assembler-inl.h"
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
  __ Ret();
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by r1.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ mov(fp, r1);
  __ ldr(r1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LeaveFrame(StackFrame::INTERNAL);

  __ ldr(r0, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldrh(r0,
          FieldMemOperand(r0, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(r2, r0);

  ParameterCount dummy1(r2);
  ParameterCount dummy2(r0);
  __ InvokeFunction(r1, dummy1, dummy2, JUMP_FUNCTION);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
