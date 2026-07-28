// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFA_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFA_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-zfa.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {

// Zfa Extension: Additional Floating-Point Instructions
// Provides IEEE 754-2019 compliant operations and additional conversions.
class AssemblerRISCVZfa : public AssemblerRiscvBase {
 public:
  // =========================================================================
  // Single-Precision Zfa Instructions
  // =========================================================================

  // fli.s: Load floating-point immediate (predefined constants)
  // The imm5 field (rs1) encodes which constant to load:
  //   0: reserved
  //   1: minimum positive normal
  //   2-29: various constants (0.5, 1.0, 2.0, 4.0, etc.)
  //   30: +infinity
  //   31: canonical NaN
  void fli_s(FPURegister rd, uint8_t imm5);

  // fminm.s / fmaxm.s: IEEE 754-2019 minimum/maximum (NaN-propagating)
  void fminm_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmaxm_s(FPURegister rd, FPURegister rs1, FPURegister rs2);

  // fround.s: Round to integer according to rounding mode
  void fround_s(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  // froundnx.s: Round to integer with inexact exception
  void froundnx_s(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  // fleq.s / fltq.s: Quiet comparisons (no signaling on NaN)
  void fleq_s(Register rd, FPURegister rs1, FPURegister rs2);
  void fltq_s(Register rd, FPURegister rs1, FPURegister rs2);

  // =========================================================================
  // Double-Precision Zfa Instructions
  // =========================================================================

  // fli.d: Load floating-point immediate (predefined constants)
  // The imm5 field (rs1) encodes which constant to load (same values as fli.s,
  // but with double-precision representation)
  void fli_d(FPURegister rd, uint8_t imm5);

  // fminm.d / fmaxm.d: IEEE 754-2019 minimum/maximum (NaN-propagating)
  void fminm_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmaxm_d(FPURegister rd, FPURegister rs1, FPURegister rs2);

  // fround.d: Round to integer according to rounding mode
  void fround_d(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  // froundnx.d: Round to integer with inexact exception
  void froundnx_d(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  // fcvtmod.w.d: Convert double to signed 32-bit integer modulo 2^32
  void fcvtmod_w_d(Register rd, FPURegister rs1);

  // fleq.d / fltq.d: Quiet comparisons (no signaling on NaN)
  void fleq_d(Register rd, FPURegister rs1, FPURegister rs2);
  void fltq_d(Register rd, FPURegister rs1, FPURegister rs2);
};

// =============================================================================
// FLI (Floating-Point Load Immediate) Constants
// =============================================================================
// The FLI instruction loads one of 32 predefined floating-point constants.
// The constant is selected by a 5-bit immediate (imm5) in the rs1 field.
//
// imm5 encoding:
//   0:  -1.0
//   1:  minimum positive normal (FLT_MIN / DBL_MIN)
//   2:  1.0 × 2⁻¹⁶
//   3:  1.0 × 2⁻¹⁵
//   ...
//   16: 1.0
//   ...
//   30: +∞
//   31: Canonical NaN

// Returns the single-precision floating-point value for the given FLI.S
// immediate encoding (imm5).
// Precondition: imm5 <= 31
float GetFLISValue(uint8_t imm5);

// Returns the double-precision floating-point value for the given FLI.D
// immediate encoding (imm5).
// Precondition: imm5 <= 31
double GetFLIDValue(uint8_t imm5);

// Returns the FLI.S immediate encoding (imm5) for the given single-precision
// floating-point value, or -1 if the value cannot be encoded by FLI.S.
int GetImm5ForFLIS(float value);

// Returns the FLI.D immediate encoding (imm5) for the given double-precision
// floating-point value, or -1 if the value cannot be encoded by FLI.D.
int GetImm5ForFLID(double value);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFA_H_
