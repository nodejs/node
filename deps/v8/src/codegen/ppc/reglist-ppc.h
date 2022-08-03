// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_PPC_REGLIST_PPC_H_
#define V8_CODEGEN_PPC_REGLIST_PPC_H_

#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
using Simd128RegList = RegListBase<Simd128Register>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding

// Caller-saved/arguments registers
const RegList kJSCallerSaved = {r3,   // a1
                                r4,   // a2
                                r5,   // a3
                                r6,   // a4
                                r7,   // a5
                                r8,   // a6
                                r9,   // a7
                                r10,  // a8
                                r11};

const int kNumJSCallerSaved = 9;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = {r14, r15, r16, r17, r18, r19, r20, r21, r22,
                              r23, r24, r25, r26, r27, r28, r29, r30, fp};

const int kNumCalleeSaved = 18;

const DoubleRegList kCallerSavedDoubles = {d0, d1, d2, d3,  d4,  d5,  d6,
                                           d7, d8, d9, d10, d11, d12, d13};

const Simd128RegList kCallerSavedSimd128s = {v0, v1, v2, v3,  v4,  v5,  v6,
                                             v7, v8, v9, v10, v11, v12, v13};

const int kNumCallerSavedDoubles = 14;

const DoubleRegList kCalleeSavedDoubles = {d14, d15, d16, d17, d18, d19,
                                           d20, d21, d22, d23, d24, d25,
                                           d26, d27, d28, d29, d30, d31};

const int kNumCalleeSavedDoubles = 18;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_PPC_REGLIST_PPC_H_
