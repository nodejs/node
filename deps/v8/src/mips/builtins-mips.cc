// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_ArrayConstructCode(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // arguments slots
  // handler frame
  // entry frame
  // callee saved registers + ra
  // 4 args slots
  // args

  // Clear the context before we push it when entering the JS frame.
  __ li(cp, Operand(0));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Set up the context from the function argument.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Set up the roots register.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ li(s6, Operand(roots_address));

  // Push the function and the receiver onto the stack.
  __ MultiPushReversed(a1.bit() | a2.bit());

  // Copy arguments to the stack in a loop.
  // a3: argc
  // s0: argv, ie points to first arg
  Label loop, entry;
  __ sll(t0, a3, kPointerSizeLog2);
  __ add(t2, s0, t0);
  __ b(&entry);
  __ nop();   // Branch delay slot nop.
  // t2 points past last arg.
  __ bind(&loop);
  __ lw(t0, MemOperand(s0));  // Read next parameter.
  __ addiu(s0, s0, kPointerSize);
  __ lw(t0, MemOperand(t0));  // Dereference handle.
  __ Push(t0);  // Push parameter.
  __ bind(&entry);
  __ Branch(ne, &loop, s0, Operand(t2));

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  // s6: roots_address
  //
  // Stack:
  // arguments
  // receiver
  // function
  // arguments slots
  // handler frame
  // entry frame
  // callee saved registers + ra
  // 4 args slots
  // args

  // Initialize all JavaScript callee-saved registers, since they will be seen
  // by the garbage collector as part of handlers.
  __ LoadRoot(t4, Heap::kUndefinedValueRootIndex);
  __ mov(s1, t4);
  __ mov(s2, t4);
  __ mov(s3, t4);
  __ mov(s4, s4);
  __ mov(s5, t4);
  // s6 holds the root address. Do not clobber.
  // s7 is cp. Do not init.

  // Invoke the code and pass argc as a0.
  __ mov(a0, a3);
  if (is_construct) {
    UNIMPLEMENTED_MIPS();
    __ break_(0x164);
  } else {
    ParameterCount actual(a0);
    __ InvokeFunction(a1, actual, CALL_FUNCTION);
  }

  __ LeaveInternalFrame();

  __ Jump(ra);
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x201);
}


#undef __

} }  // namespace v8::internal

