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

#ifndef V8_ARM_REGISTER_ALLOCATOR_ARM_INL_H_
#define V8_ARM_REGISTER_ALLOCATOR_ARM_INL_H_

#include "v8.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// RegisterAllocator implementation.

bool RegisterAllocator::IsReserved(Register reg) {
  return reg.is(cp) || reg.is(fp) || reg.is(sp) || reg.is(pc);
}



// The register allocator uses small integers to represent the
// non-reserved assembler registers.  The mapping is:
//
// r0 <-> 0
// r1 <-> 1
// r2 <-> 2
// r3 <-> 3
// r4 <-> 4
// r5 <-> 5
// r6 <-> 6
// r7 <-> 7
// r9 <-> 8
// r10 <-> 9
// ip <-> 10
// lr <-> 11

int RegisterAllocator::ToNumber(Register reg) {
  ASSERT(reg.is_valid() && !IsReserved(reg));
  static int numbers[] = {
    0,   // r0
    1,   // r1
    2,   // r2
    3,   // r3
    4,   // r4
    5,   // r5
    6,   // r6
    7,   // r7
    -1,  // cp
    8,   // r9
    9,   // r10
    -1,  // fp
    10,  // ip
    -1,  // sp
    11,  // lr
    -1   // pc
  };
  return numbers[reg.code()];
}


Register RegisterAllocator::ToRegister(int num) {
  ASSERT(num >= 0 && num < kNumRegisters);
  static Register registers[] =
      { r0, r1, r2, r3, r4, r5, r6, r7, r9, r10, ip, lr };
  return registers[num];
}


void RegisterAllocator::Initialize() {
  Reset();
  // The non-reserved r1 and lr registers are live on JS function entry.
  Use(r1);  // JS function.
  Use(lr);  // Return address.
}


} }  // namespace v8::internal

#endif  // V8_ARM_REGISTER_ALLOCATOR_ARM_INL_H_
