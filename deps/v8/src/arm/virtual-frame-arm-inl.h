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

#ifndef V8_VIRTUAL_FRAME_ARM_INL_H_
#define V8_VIRTUAL_FRAME_ARM_INL_H_

#include "assembler-arm.h"
#include "virtual-frame-arm.h"

namespace v8 {
namespace internal {

// These VirtualFrame methods should actually be in a virtual-frame-arm-inl.h
// file if such a thing existed.
MemOperand VirtualFrame::ParameterAt(int index) {
  // Index -1 corresponds to the receiver.
  ASSERT(-1 <= index);  // -1 is the receiver.
  ASSERT(index <= parameter_count());
  return MemOperand(fp, (1 + parameter_count() - index) * kPointerSize);
}

  // The receiver frame slot.
MemOperand VirtualFrame::Receiver() {
  return ParameterAt(-1);
}


void VirtualFrame::Forget(int count) {
  SpillAll();
  LowerHeight(count);
}

} }  // namespace v8::internal

#endif  // V8_VIRTUAL_FRAME_ARM_INL_H_
