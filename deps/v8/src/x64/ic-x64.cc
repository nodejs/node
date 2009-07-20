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
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


void KeyedLoadIC::ClearInlinedVersion(Address address) {
  UNIMPLEMENTED();
}

void KeyedStoreIC::ClearInlinedVersion(Address address) {
  UNIMPLEMENTED();
}

void KeyedStoreIC::RestoreInlinedVersion(Address address) {
  UNIMPLEMENTED();
}


void KeyedLoadIC::Generate(MacroAssembler* masm,
                           ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));

  // Move the return address below the arguments.
  __ pop(rbx);
  __ push(rcx);
  __ push(rax);
  __ push(rbx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC0AB));  // Debugging aid.
}

void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC1AB));  // Debugging aid.
}

bool KeyedLoadIC::PatchInlinedLoad(Address address, Object* map) {
  UNIMPLEMENTED();
  return false;
}

bool KeyedStoreIC::PatchInlinedStore(Address address, Object* map) {
  UNIMPLEMENTED();
  return false;
}

Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   Object* callback) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* object,
                                                JSObject* holder,
                                                int index) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

void KeyedStoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : key
  //  -- rsp[16] : receiver
  // -----------------------------------

  // Move the return address below the arguments.
  __ pop(rcx);
  __ push(Operand(rsp, 1 * kPointerSize));
  __ push(Operand(rsp, 1 * kPointerSize));
  __ push(rax);
  __ push(rcx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(f, 3);
}

void KeyedStoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC2AB));  // Debugging aid.
}

void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC3AB));  // Debugging aid.
}

Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                  int index,
                                                  Map* transition,
                                                  String* name) {
  UNIMPLEMENTED();
  return NULL;
}


void CallIC::Generate(MacroAssembler* masm,
                      int argc,
                      ExternalReference const& f) {
  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
  // Get the name of the function to call from the stack.
  // 2 ~ receiver, return address.
  __ movq(rbx, Operand(rsp, (argc + 2) * kPointerSize));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ push(rdx);
  __ push(rbx);

  // Call the entry.
  CEntryStub stub;
  __ movq(rax, Immediate(2));
  __ movq(rbx, f);
  __ CallStub(&stub);

  // Move result to rdi and exit the internal frame.
  __ movq(rdi, rax);
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
  Label invoke, global;
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));  // receiver
  __ testl(rdx, Immediate(kSmiTagMask));
  __ j(zero, &invoke);
  __ CmpObjectType(rdx, JS_GLOBAL_OBJECT_TYPE, rcx);
  __ j(equal, &global);
  __ CmpInstanceType(rcx, JS_BUILTINS_OBJECT_TYPE);
  __ j(not_equal, &invoke);

  // Patch the receiver on the stack.
  __ bind(&global);
  __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
  __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);

  // Invoke the function.
  ParameterCount actual(argc);
  __ bind(&invoke);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);
}

void CallIC::GenerateMegamorphic(MacroAssembler* a, int b) {
  UNIMPLEMENTED();
}

void CallIC::GenerateNormal(MacroAssembler* a, int b) {
  UNIMPLEMENTED();
}


const int LoadIC::kOffsetToLoadInstruction = 20;


void LoadIC::ClearInlinedVersion(Address address) {
  UNIMPLEMENTED();
}


void LoadIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  __ movq(rax, Operand(rsp, kPointerSize));

  // Move the return address below the arguments.
  __ pop(rbx);
  __ push(rax);
  __ push(rcx);
  __ push(rbx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2);
}


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC4AB));  // Debugging aid.
}

void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC5AB));  // Debugging aid.
}

void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC6AB));  // Debugging aid.
}

void LoadIC::GenerateMiss(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC7AB));  // Debugging aid.
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC8AB));  // Debugging aid.
}

void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xC9AB));  // Debugging aid.
}

bool LoadIC::PatchInlinedLoad(Address address, Object* map, int index) {
  UNIMPLEMENTED();
  return false;
}

void StoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  // Move the return address below the arguments.
  __ pop(rbx);
  __ push(Operand(rsp, 0));
  __ push(rcx);
  __ push(rax);
  __ push(rbx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 3);
}

void StoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xCAAB));  // Debugging aid.
}

void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
  masm->movq(kScratchRegister, Immediate(0xCBAB));  // Debugging aid.
}


#undef __


} }  // namespace v8::internal
