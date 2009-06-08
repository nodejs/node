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

#ifndef V8_IA32_REGISTER_ALLOCATOR_IA32_INL_H_
#define V8_IA32_REGISTER_ALLOCATOR_IA32_INL_H_

#include "v8.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// RegisterAllocator implementation.

bool RegisterAllocator::IsReserved(Register reg) {
  // The code for this test relies on the order of register codes.
  return reg.code() >= esp.code() && reg.code() <= esi.code();
}


// The register allocator uses small integers to represent the
// non-reserved assembler registers.  The mapping is:

// eax <-> 0, ebx <-> 1, ecx <-> 2, edx <-> 3, edi <-> 4.

int RegisterAllocator::ToNumber(Register reg) {
  ASSERT(reg.is_valid() && !IsReserved(reg));
  static int numbers[] = {
    0,   // eax
    2,   // ecx
    3,   // edx
    1,   // ebx
    -1,  // esp
    -1,  // ebp
    -1,  // esi
    4    // edi
  };
  return numbers[reg.code()];
}


Register RegisterAllocator::ToRegister(int num) {
  ASSERT(num >= 0 && num < kNumRegisters);
  static Register registers[] = { eax, ebx, ecx, edx, edi };
  return registers[num];
}


void RegisterAllocator::Initialize() {
  Reset();
  // The non-reserved edi register is live on JS function entry.
  Use(edi);  // JS function.
}


} }  // namespace v8::internal

#endif  // V8_IA32_REGISTER_ALLOCATOR_IA32_INL_H_
