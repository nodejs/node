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
#include "register-allocator-inl.h"
#include "scopes.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// VirtualFrame implementation.

#define __ ACCESS_MASM(masm())

void VirtualFrame::SyncElementBelowStackPointer(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncElementByPushing(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncRange(int begin, int end) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::MergeTo(VirtualFrame* expected) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::Enter() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::Exit() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::AllocateStackSlots() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::SaveContextRegister() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::RestoreContextRegister() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::PushReceiverSlotAddress() {
  UNIMPLEMENTED_MIPS();
}


int VirtualFrame::InvalidateFrameSlotAt(int index) {
  return kIllegalIndex;
}


void VirtualFrame::TakeFrameSlotAt(int index) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::StoreToFrameSlotAt(int index) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::PushTryHandler(HandlerType type) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::RawCallStub(CodeStub* stub) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallStub(CodeStub* stub, Result* arg) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallStub(CodeStub* stub, Result* arg0, Result* arg1) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallRuntime(Runtime::Function* f, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallRuntime(Runtime::FunctionId id, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallAlignedRuntime(Runtime::Function* f, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallAlignedRuntime(Runtime::FunctionId id, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::InvokeBuiltin(Builtins::JavaScript id,
                                 InvokeJSFlags flags,
                                 Result* arg_count_register,
                                 int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::RawCallCodeObject(Handle<Code> code,
                                       RelocInfo::Mode rmode) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  int dropped_args) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  Result* arg,
                                  int dropped_args) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  Result* arg0,
                                  Result* arg1,
                                  int dropped_args,
                                  bool set_auto_args_slots) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::Drop(int count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::DropFromVFrameOnly(int count) {
  UNIMPLEMENTED_MIPS();
}


Result VirtualFrame::Pop() {
  UNIMPLEMENTED_MIPS();
  Result res = Result();
  return res;    // UNIMPLEMENTED RETUR
}


void VirtualFrame::EmitPop(Register reg) {
  UNIMPLEMENTED_MIPS();
}

void VirtualFrame::EmitMultiPop(RegList regs) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::EmitPush(Register reg) {
  UNIMPLEMENTED_MIPS();
}

void VirtualFrame::EmitMultiPush(RegList regs) {
  UNIMPLEMENTED_MIPS();
}

void VirtualFrame::EmitArgumentSlots(RegList reglist) {
  UNIMPLEMENTED_MIPS();
}

#undef __

} }  // namespace v8::internal

