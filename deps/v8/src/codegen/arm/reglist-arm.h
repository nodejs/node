// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ARM_REGLIST_ARM_H_
#define V8_CODEGEN_ARM_REGLIST_ARM_H_

#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding

// Caller-saved/arguments registers
const RegList kJSCallerSaved = {r0,   // r0 a1
                                r1,   // r1 a2
                                r2,   // r2 a3
                                r3};  // r3 a4

const int kNumJSCallerSaved = 4;

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = {r4,    //  r4 v1
                              r5,    //  r5 v2
                              r6,    //  r6 v3
                              r7,    //  r7 v4 (cp in JavaScript code)
                              r8,    //  r8 v5 (pp in JavaScript code)
                              r9,    //  r9 v6
                              r10,   // r10 v7
                              r11};  // r11 v8 (fp in JavaScript code)

// When calling into C++ (only for C++ calls that can't cause a GC).
// The call code will take care of lr, fp, etc.
const RegList kCallerSaved = {r0,   // r0
                              r1,   // r1
                              r2,   // r2
                              r3,   // r3
                              r9};  // r9

const int kNumCalleeSaved = 8;

// Double registers d8 to d15 are callee-saved.
const int kNumDoubleCalleeSaved = 8;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_ARM_REGLIST_ARM_H_
