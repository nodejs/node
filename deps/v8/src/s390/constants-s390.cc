// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/s390/constants-s390.h"

namespace v8 {
namespace internal {

// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* Registers::names_[kNumRegisters] = {
    "r0", "r1", "r2",  "r3", "r4", "r5",  "r6",  "r7",
    "r8", "r9", "r10", "fp", "ip", "r13", "r14", "sp"};

const char* DoubleRegisters::names_[kNumDoubleRegisters] = {
    "f0", "f1", "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15"};

int DoubleRegisters::Number(const char* name) {
  for (int i = 0; i < kNumDoubleRegisters; i++) {
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

  // No register with the requested name found.
  return kNoRegister;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
