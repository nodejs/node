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

#ifndef V8_X64_REGISTER_ALLOCATOR_X64_INL_H_
#define V8_X64_REGISTER_ALLOCATOR_X64_INL_H_

#include "v8.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// RegisterAllocator implementation.

bool RegisterAllocator::IsReserved(Register reg) {
  return reg.is(rsp) || reg.is(rbp) || reg.is(rsi) ||
      reg.is(kScratchRegister) || reg.is(kRootRegister) ||
      reg.is(kSmiConstantRegister);
}


// The register allocator uses small integers to represent the
// non-reserved assembler registers.
int RegisterAllocator::ToNumber(Register reg) {
  ASSERT(reg.is_valid() && !IsReserved(reg));
  const int kNumbers[] = {
    0,   // rax
    2,   // rcx
    3,   // rdx
    1,   // rbx
    -1,  // rsp  Stack pointer.
    -1,  // rbp  Frame pointer.
    -1,  // rsi  Context.
    4,   // rdi
    5,   // r8
    6,   // r9
    -1,  // r10  Scratch register.
    8,   // r11
    9,   // r12
    -1,  // r13  Roots array.  This is callee saved.
    7,   // r14
    -1   // r15  Smi constant register.
  };
  return kNumbers[reg.code()];
}


Register RegisterAllocator::ToRegister(int num) {
  ASSERT(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] =
      { rax, rbx, rcx, rdx, rdi, r8, r9, r14, r11, r12 };
  return kRegisters[num];
}


void RegisterAllocator::Initialize() {
  Reset();
  // The non-reserved rdi register is live on JS function entry.
  Use(rdi);  // JS function.
}
} }  // namespace v8::internal

#endif  // V8_X64_REGISTER_ALLOCATOR_X64_INL_H_
