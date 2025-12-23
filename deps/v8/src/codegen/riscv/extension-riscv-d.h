// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_D_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_D_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-d.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {
class AssemblerRISCVD : public AssemblerRiscvBase {
  // RV32D Standard Extension
 public:
  void fld(FPURegister rd, Register rs1, int16_t imm12);
  void fsd(FPURegister source, Register base, int16_t imm12);
  void fmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);
  void fadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fmul_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fdiv_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fsqrt_d(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fsgnj_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjn_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjx_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmin_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmax_d(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fcvt_s_d(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_d_s(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void feq_d(Register rd, FPURegister rs1, FPURegister rs2);
  void flt_d(Register rd, FPURegister rs1, FPURegister rs2);
  void fle_d(Register rd, FPURegister rs1, FPURegister rs2);
  void fclass_d(Register rd, FPURegister rs1);
  void fcvt_w_d(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_wu_d(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_d_w(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_d_wu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);

#ifdef V8_TARGET_ARCH_RISCV64
  // RV64D Standard Extension (in addition to RV32D)
  void fcvt_l_d(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_lu_d(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fmv_x_d(Register rd, FPURegister rs1);
  void fcvt_d_l(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_d_lu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fmv_d_x(FPURegister rd, Register rs1);
#endif

  void fmv_d(FPURegister rd, FPURegister rs) { fsgnj_d(rd, rs, rs); }
  void fabs_d(FPURegister rd, FPURegister rs) { fsgnjx_d(rd, rs, rs); }
  void fneg_d(FPURegister rd, FPURegister rs) { fsgnjn_d(rd, rs, rs); }
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_D_H_
