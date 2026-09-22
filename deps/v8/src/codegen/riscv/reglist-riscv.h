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
using Simd128RegList = RegListBase<Simd128Register>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);
ASSERT_TRIVIALLY_COPYABLE(Simd128RegList);

const RegList kJSCallerSaved = {t0, t1, t2, a0, a1, a2, a3, a4, a5, a6, a7, t4};

const int kNumJSCallerSaved = 12;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = {C_CALL_CALLEE_SAVE_REGISTERS};

const int kNumCalleeSaved = 12;

const DoubleRegList kCalleeSavedFPU = {C_CALL_CALLEE_SAVE_FP_REGISTERS};

const int kNumCalleeSavedFPU = kCalleeSavedFPU.Count();

const DoubleRegList kCallerSavedFPU = {ft0, ft1, ft2, ft3, ft4,  ft5, ft6,
                                       ft7, fa0, fa1, fa2, fa3,  fa4, fa5,
                                       fa6, fa7, ft8, ft9, ft10, ft11};

const int kNumCallerSavedFPU = kCallerSavedFPU.Count();

// Caller-saved vector registers. The RISC-V V psABI defines v1-v7 and v24-v31
// as callee-saved, so only v8-v23 need to be preserved across C calls. v0 is
// the vector mask register and is never used by V8 for SIMD values.
const Simd128RegList kCallerSavedVR = {v8,  v9,  v10, v11, v12, v13, v14, v15,
                                       v16, v17, v18, v19, v20, v21, v22, v23};

const int kNumCallerSavedVR = kCallerSavedVR.Count();

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
