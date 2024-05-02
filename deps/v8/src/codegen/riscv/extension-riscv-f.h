// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-f.h"
#include "src/codegen/riscv/register-riscv.h"
#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_F_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_F_H_

namespace v8 {
namespace internal {
class AssemblerRISCVF : public AssemblerRiscvBase {
  // RV32F Standard Extension
 public:
  void flw(FPURegister rd, Register rs1, int16_t imm12);
  void fsw(FPURegister source, Register base, int16_t imm12);
  void fmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);
  void fadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fmul_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fdiv_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fsqrt_s(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fsgnj_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjn_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjx_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmin_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmax_s(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fcvt_w_s(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_wu_s(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fmv_x_w(Register rd, FPURegister rs1);
  void feq_s(Register rd, FPURegister rs1, FPURegister rs2);
  void flt_s(Register rd, FPURegister rs1, FPURegister rs2);
  void fle_s(Register rd, FPURegister rs1, FPURegister rs2);
  void fclass_s(Register rd, FPURegister rs1);
  void fcvt_s_w(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_s_wu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fmv_w_x(FPURegister rd, Register rs1);

#ifdef V8_TARGET_ARCH_RISCV64
  // RV64F Standard Extension (in addition to RV32F)
  void fcvt_l_s(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_lu_s(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_s_l(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_s_lu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
#endif

  void fmv_s(FPURegister rd, FPURegister rs) { fsgnj_s(rd, rs, rs); }
  void fabs_s(FPURegister rd, FPURegister rs) { fsgnjx_s(rd, rs, rs); }
  void fneg_s(FPURegister rd, FPURegister rs) { fsgnjn_s(rd, rs, rs); }
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_F_H_
