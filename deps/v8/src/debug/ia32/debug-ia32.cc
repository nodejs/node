// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/debug/debug.h"

#include "src/codegen/macro-assembler.h"
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
  __ ret(0);
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by eax.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ mov(ebp, eax);
  __ mov(edi, Operand(ebp, StandardFrameConstants::kFunctionOffset));
  __ mov(eax, Operand(ebp, StandardFrameConstants::kArgCOffset));
  __ leave();

  // The arguments are already in the stack (including any necessary padding),
  // we should not try to massage the arguments again.
  __ mov(ecx, Immediate(kDontAdaptArgumentsSentinel));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  __ InvokeFunctionCode(edi, no_reg, ecx, eax, JUMP_FUNCTION);
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
