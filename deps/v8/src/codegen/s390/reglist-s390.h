// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_S390_REGLIST_S390_H_
#define V8_CODEGEN_S390_REGLIST_S390_H_

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
const RegList kJSCallerSaved = {r1, r2,  // r2  a1
                                r3,      // r3  a2
                                r4,      // r4  a3
                                r5};     // r5  a4

const int kNumJSCallerSaved = 5;

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = {r6,    // r6 (argument passing in CEntryStub)
                                     //    (HandleScope logic in MacroAssembler)
                              r7,    // r7 (argument passing in CEntryStub)
                                     //    (HandleScope logic in MacroAssembler)
                              r8,    // r8 (argument passing in CEntryStub)
                                     //    (HandleScope logic in MacroAssembler)
                              r9,    // r9 (HandleScope logic in MacroAssembler)
                              r10,   // r10 (Roots register in Javascript)
                              fp,    // r11 (fp in Javascript)
                              ip,    // r12 (ip in Javascript)
                              r13};  // r13 (cp in Javascript)
// r15;   // r15 (sp in Javascript)

const int kNumCalleeSaved = 8;

const DoubleRegList kCallerSavedDoubles = {d0, d1, d2, d3, d4, d5, d6, d7};

const int kNumCallerSavedDoubles = 8;

const DoubleRegList kCalleeSavedDoubles = {d8,  d9,  d10, d11,
                                           d12, d13, d14, d15};

const int kNumCalleeSavedDoubles = 8;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_S390_REGLIST_S390_H_
