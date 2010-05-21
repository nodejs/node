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

#if defined(V8_TARGET_ARCH_MIPS)

#include "codegen-inl.h"
#include "debug.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT
bool BreakLocationIterator::IsDebugBreakAtReturn() {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


void BreakLocationIterator::SetDebugBreakAtReturn() {
  UNIMPLEMENTED_MIPS();
}


// Restore the JS frame exit code.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  UNIMPLEMENTED_MIPS();
}


// A debug break in the exit code is identified by a call.
bool Debug::IsDebugBreakAtReturn(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  return rinfo->IsPatchedReturnSequence();
}


#define __ ACCESS_MASM(masm)




void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
}


void Debug::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->Abort("LiveEdit frame dropping is not supported on mips");
}

void Debug::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  masm->Abort("LiveEdit frame dropping is not supported on mips");
}

#undef __


void Debug::SetUpFrameDropperFrame(StackFrame* bottom_js_frame,
                                   Handle<Code> code) {
  UNREACHABLE();
}
const int Debug::kFrameDropperFrameSize = -1;


#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
