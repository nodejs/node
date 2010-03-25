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

#include "constants-arm.h"


namespace assembler {
namespace arm {

namespace v8i = v8::internal;


// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* Registers::names_[kNumRegisters] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "fp", "ip", "sp", "lr", "pc",
};


// List of alias names which can be used when referring to ARM registers.
const Registers::RegisterAlias Registers::aliases_[] = {
  {10, "sl"},
  {11, "r11"},
  {12, "r12"},
  {13, "r13"},
  {14, "r14"},
  {15, "r15"},
  {kNoRegister, NULL}
};


const char* Registers::Name(int reg) {
  const char* result;
  if ((0 <= reg) && (reg < kNumRegisters)) {
    result = names_[reg];
  } else {
    result = "noreg";
  }
  return result;
}


// Support for VFP registers s0 to s31 (d0 to d15).
// Note that "sN:sM" is the same as "dN/2"
// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* VFPRegisters::names_[kNumVFPRegisters] = {
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",
    "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
    "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15"
};


const char* VFPRegisters::Name(int reg, bool is_double) {
  ASSERT((0 <= reg) && (reg < kNumVFPRegisters));
  return names_[reg + is_double ? kNumVFPSingleRegisters : 0];
}


int VFPRegisters::Number(const char* name, bool* is_double) {
  for (int i = 0; i < kNumVFPRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      if (i < kNumVFPSingleRegisters) {
        *is_double = false;
        return i;
      } else {
        *is_double = true;
        return i - kNumVFPSingleRegisters;
      }
    }
  }

  // No register with the requested name found.
  return kNoRegister;
}


int Registers::Number(const char* name) {
  // Look through the canonical names.
  for (int i = 0; i < kNumRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
    }
  }

  // Look through the alias names.
  int i = 0;
  while (aliases_[i].reg != kNoRegister) {
    if (strcmp(aliases_[i].name, name) == 0) {
      return aliases_[i].reg;
    }
    i++;
  }

  // No register with the requested name found.
  return kNoRegister;
}


} }  // namespace assembler::arm
