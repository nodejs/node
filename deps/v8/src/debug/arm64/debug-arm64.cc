// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/debug/debug.h"

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/debug/liveedit.h"
#include "src/frame-constants.h"
#include "src/frames-inl.h"
#include "src/objects-inl.h"

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
  // - Drop to the target frame specified by x1.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ Mov(fp, x1);
  __ AssertStackConsistency();
  __ Ldr(x1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  __ Mov(masm->StackPointer(), Operand(fp));
  __ Pop(fp, lr);  // Frame, Return address.

  __ Ldr(x0, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x0,
         FieldMemOperand(x0, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(x2, x0);

  ParameterCount dummy1(x2);
  ParameterCount dummy2(x0);
  __ InvokeFunction(x1, dummy1, dummy2, JUMP_FUNCTION);
}


const bool LiveEdit::kFrameDropperSupported = true;

}  // namespace internal
}  // namespace v8

#undef __

#endif  // V8_TARGET_ARCH_ARM64
