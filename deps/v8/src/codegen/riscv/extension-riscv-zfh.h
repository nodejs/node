// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFH_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFH_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-f.h"
#include "src/codegen/riscv/register-riscv.h"
namespace v8 {
namespace internal {

class AssemblerRISCVZfh : public AssemblerRiscvBase {
  // RVZFH Standard Extension
 public:
  void flh(FPURegister rd, Register rs1, int16_t imm12);
  void fsh(FPURegister source, Register base, int16_t imm12);
  void fadd_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fsub_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fmul_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fdiv_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
              FPURoundingMode frm = RNE);
  void fmin_h(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fmax_h(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsqrt_h(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  void fmadd_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fmsub_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
               FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmsub_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);
  void fnmadd_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                FPURegister rs3, FPURoundingMode frm = RNE);

  void fcvt_h_w(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_h_wu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);

  void fcvt_w_h(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_wu_h(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);

  void fcvt_h_s(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_s_h(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  void fcvt_h_d(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_d_h(FPURegister rd, FPURegister rs1, FPURoundingMode frm = RNE);

  void fsgnj_h(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjn_h(FPURegister rd, FPURegister rs1, FPURegister rs2);
  void fsgnjx_h(FPURegister rd, FPURegister rs1, FPURegister rs2);

  void fmv_h_x(FPURegister rd, Register rs1);
  void fmv_x_h(Register rd, FPURegister rs1);

  void feq_h(Register rd, FPURegister rs1, FPURegister rs2);
  void flt_h(Register rd, FPURegister rs1, FPURegister rs2);
  void fle_h(Register rd, FPURegister rs1, FPURegister rs2);

  void fclass_h(Register rd, FPURegister rs1);
#ifdef V8_TARGET_ARCH_RISCV64
  // RV64F Standard Extension (in addition to RV32F)
  void fcvt_h_l(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);
  void fcvt_h_lu(FPURegister rd, Register rs1, FPURoundingMode frm = RNE);

  void fcvt_l_h(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
  void fcvt_lu_h(Register rd, FPURegister rs1, FPURoundingMode frm = RNE);
#endif
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_ZFH_H_
