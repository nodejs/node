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

#include "frames-inl.h"
#include "mips/assembler-mips-inl.h"

namespace v8 {
namespace internal {


StackFrame::Type StackFrame::ComputeType(State* state) {
  ASSERT(state->fp != NULL);
  if (StandardFrame::IsArgumentsAdaptorFrame(state->fp)) {
    return ARGUMENTS_ADAPTOR;
  }
  // The marker and function offsets overlap. If the marker isn't a
  // smi then the frame is a JavaScript frame -- and the marker is
  // really the function.
  const int offset = StandardFrameConstants::kMarkerOffset;
  Object* marker = Memory::Object_at(state->fp + offset);
  if (!marker->IsSmi()) return JAVA_SCRIPT;
  return static_cast<StackFrame::Type>(Smi::cast(marker)->value());
}


Address ExitFrame::ComputeStackPointer(Address fp) {
  Address sp = fp + ExitFrameConstants::kSPDisplacement;
  const int offset = ExitFrameConstants::kCodeOffset;
  Object* code = Memory::Object_at(fp + offset);
  bool is_debug_exit = code->IsSmi();
  if (is_debug_exit) {
    sp -= kNumJSCallerSaved * kPointerSize;
  }
  return sp;
}


void ExitFrame::Iterate(ObjectVisitor* v) const {
  // Do nothing
}


int JavaScriptFrame::GetProvidedParametersCount() const {
  return ComputeParametersCount();
}


Address JavaScriptFrame::GetCallerStackPointer() const {
  UNIMPLEMENTED_MIPS();
  return static_cast<Address>(NULL);  // UNIMPLEMENTED RETURN
}


Address ArgumentsAdaptorFrame::GetCallerStackPointer() const {
  UNIMPLEMENTED_MIPS();
  return static_cast<Address>(NULL);  // UNIMPLEMENTED RETURN
}


Address InternalFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
