// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"
#include "codegen-inl.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                Builtins::CFunctionId id) {
  masm->int3();  // UNIMPLEMENTED.
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(rbp);
  __ movq(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(ArgumentsAdaptorFrame::SENTINEL));

  // Push the function on the stack.
  __ push(rdi);

  // Preserve the number of arguments on the stack. Must preserve both
  // eax and ebx because these registers are used when copying the
  // arguments and the receiver.
  ASSERT(kSmiTagSize == 1);
  __ lea(rcx, Operand(rax, rax, kTimes1, kSmiTag));
  __ push(rcx);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movq(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movq(rsp, rbp);
  __ pop(rbp);

  // Remove caller arguments from the stack.
  // rbx holds a Smi, so we convery to dword offset by multiplying by 4.
  ASSERT_EQ(kSmiTagSize, 1 && kSmiTag == 0);
  ASSERT_EQ(kPointerSize, (1 << kSmiTagSize) * 4);
  __ pop(rcx);
  __ lea(rsp, Operand(rsp, rbx, kTimes4, 1 * kPointerSize));  // 1 ~ receiver
  __ push(rcx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  __ IncrementCounter(&Counters::arguments_adaptors, 1);

  Label enough, too_few;
  __ cmpq(rax, rbx);
  __ j(less, &too_few);
  __ cmpq(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rax, Operand(rbp, rax, kTimesPointerSize, offset));
    __ movq(rcx, Immediate(-1));  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(rcx);
    __ push(Operand(rax, 0));
    __ subq(rax, Immediate(kPointerSize));
    __ cmpq(rcx, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rdi, Operand(rbp, rax, kTimesPointerSize, offset));
    __ movq(rcx, Immediate(-1));  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(rcx);
    __ push(Operand(rdi, 0));
    __ subq(rdi, Immediate(kPointerSize));
    __ cmpq(rcx, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ movq(kScratchRegister,
            Factory::undefined_value(),
            RelocInfo::EMBEDDED_OBJECT);
    __ bind(&fill);
    __ incq(rcx);
    __ push(kScratchRegister);
    __ cmpq(rcx, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ call(rdx);

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(rdx);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Expects five C++ function parameters.
  // - Address entry (ignored)
  // - JSFunction* function (
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Platform specific argument handling. After this, the stack contains
  // an internal frame and the pushed function and receiver, and
  // register rax and rbx holds the argument count and argument array,
  // while rdi holds the function pointer and rsi the context.
#ifdef __MSVC__
  // MSVC parameters in:
  // rcx : entry (ignored)
  // rdx : function
  // r8 : receiver
  // r9 : argc
  // [rsp+0x20] : argv

  // Clear the context before we push it when entering the JS frame.
  __ xor_(rsi, rsi);
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Load the function context into rsi.
  __ movq(rsi, FieldOperand(rdx, JSFunction::kContextOffset));

  // Push the function and the receiver onto the stack.
  __ push(rdx);
  __ push(r8);

  // Load the number of arguments and setup pointer to the arguments.
  __ movq(rax, r9);
  // Load the previous frame pointer to access C argument on stack
  __ movq(kScratchRegister, Operand(rbp, 0));
  __ movq(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
  // Load the function pointer into rdi.
  __ movq(rdi, rdx);
#else  // !defined(__MSVC__)
  // GCC parameters in:
  // rdi : entry (ignored)
  // rsi : function
  // rdx : receiver
  // rcx : argc
  // r8  : argv

  __ movq(rdi, rsi);
  // rdi : function

  // Clear the context before we push it when entering the JS frame.
  __ xor_(rsi, rsi);
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the function and receiver and setup the context.
  __ push(rdi);
  __ push(rdx);
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Load the number of arguments and setup pointer to the arguments.
  __ movq(rax, rcx);
  __ movq(rbx, r8);
#endif  // __MSVC__
  // Current stack contents:
  // [rsp + 2 * kPointerSize ... ]: Internal frame
  // [rsp + kPointerSize]         : function
  // [rsp]                        : receiver
  // Current register contents:
  // rax : argc
  // rbx : argv
  // rsi : context
  // rdi : function

  // Copy arguments to the stack in a loop.
  // Register rbx points to array of pointers to handle locations.
  // Push the values of these handles.
  Label loop, entry;
  __ xor_(rcx, rcx);  // Set loop variable to 0.
  __ jmp(&entry);
  __ bind(&loop);
  __ movq(kScratchRegister, Operand(rbx, rcx, kTimesPointerSize, 0));
  __ push(Operand(kScratchRegister, 0));  // dereference handle
  __ addq(rcx, Immediate(1));
  __ bind(&entry);
  __ cmpq(rcx, rax);
  __ j(not_equal, &loop);

  // Invoke the code.
  if (is_construct) {
    // Expects rdi to hold function pointer.
    __ movq(kScratchRegister,
            Handle<Code>(Builtins::builtin(Builtins::JSConstructCall)),
            RelocInfo::CODE_TARGET);
    __ call(kScratchRegister);
  } else {
    ParameterCount actual(rax);
    // Function must be in rdi.
    __ InvokeFunction(rdi, actual, CALL_FUNCTION);
  }

  // Exit the JS frame. Notice that this also removes the empty
  // context and the function left on the stack by the code
  // invocation.
  __ LeaveInternalFrame();
  // TODO(X64): Is argument correct? Is there a receiver to remove?
  __ ret(1 * kPointerSize);  // remove receiver
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

} }  // namespace v8::internal
