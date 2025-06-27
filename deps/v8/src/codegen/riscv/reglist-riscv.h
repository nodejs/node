// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_REGLIST_RISCV_H_
#define V8_CODEGEN_RISCV_REGLIST_RISCV_H_

#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

const RegList kJSCallerSaved = {t0, t1, t2, a0, a1, a2, a3, a4, a5, a6, a7, t4};

const int kNumJSCallerSaved = 12;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = {fp,    // fp/s0
                              s1,    // s1
                              s2,    // s2
                              s3,    // s3 scratch register
                              s4,    // s4 scratch register 2
                              s5,    // s5
                              s6,    // s6 (roots in Javascript code)
                              s7,    // s7 (cp in Javascript code)
                              s8,    // s8
                              s9,    // s9
                              s10,   // s10
                              s11};  // s11

const int kNumCalleeSaved = 12;

const DoubleRegList kCalleeSavedFPU = {fs0, fs1, fs2, fs3, fs4,  fs5,
                                       fs6, fs7, fs8, fs9, fs10, fs11};

const int kNumCalleeSavedFPU = kCalleeSavedFPU.Count();

const DoubleRegList kCallerSavedFPU = {ft0, ft1, ft2, ft3, ft4,  ft5, ft6,
                                       ft7, fa0, fa1, fa2, fa3,  fa4, fa5,
                                       fa6, fa7, ft8, ft9, ft10, ft11};

const int kNumCallerSavedFPU = kCallerSavedFPU.Count();

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 32;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_REGLIST_RISCV_H_
