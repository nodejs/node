// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/ppc/constants-ppc.h"


namespace v8 {
namespace internal {

// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* Registers::names_[kNumRegisters] = {
    "r0",  "sp",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",  "r9",  "r10",
    "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21",
    "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "r30", "fp"};


// List of alias names which can be used when referring to PPC registers.
const Registers::RegisterAlias Registers::aliases_[] = {{10, "sl"},
                                                        {11, "r11"},
                                                        {12, "r12"},
                                                        {13, "r13"},
                                                        {14, "r14"},
                                                        {15, "r15"},
                                                        {kNoRegister, NULL}};


const char* Registers::Name(int reg) {
  const char* result;
  if ((0 <= reg) && (reg < kNumRegisters)) {
    result = names_[reg];
  } else {
    result = "noreg";
  }
  return result;
}


const char* FPRegisters::names_[kNumFPRegisters] = {
    "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",  "d8",  "d9",  "d10",
    "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19", "d20", "d21",
    "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"};


const char* FPRegisters::Name(int reg) {
  DCHECK((0 <= reg) && (reg < kNumFPRegisters));
  return names_[reg];
}


int FPRegisters::Number(const char* name) {
  for (int i = 0; i < kNumFPRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
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
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
