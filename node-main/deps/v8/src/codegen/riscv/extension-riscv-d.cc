// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-d.h"

namespace v8 {
namespace internal {
// RV32D Standard Extension

void AssemblerRISCVD::fld(FPURegister rd, Register rs1, int16_t imm12) {
  GenInstrLoadFP_ri(0b011, rd, rs1, imm12);
}

void AssemblerRISCVD::fsd(FPURegister source, Register base, int16_t imm12) {
  GenInstrStoreFP_rri(0b011, base, source, imm12);
}

void AssemblerRISCVD::fmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                              FPURegister rs3, FPURoundingMode frm) {
  GenInstrR4(0b01, MADD, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVD::fmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                              FPURegister rs3, FPURoundingMode frm) {
  GenInstrR4(0b01, MSUB, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVD::fnmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURegister rs3, FPURoundingMode frm) {
  GenInstrR4(0b01, NMSUB, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVD::fnmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURegister rs3, FPURoundingMode frm) {
  GenInstrR4(0b01, NMADD, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVD::fadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                             FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0000001, frm, rd, rs1, rs2);
}

void AssemblerRISCVD::fsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                             FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0000101, frm, rd, rs1, rs2);
}

void AssemblerRISCVD::fmul_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                             FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0001001, frm, rd, rs1, rs2);
}

void AssemblerRISCVD::fdiv_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                             FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0001101, frm, rd, rs1, rs2);
}

void AssemblerRISCVD::fsqrt_d(FPURegister rd, FPURegister rs1,
                              FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0101101, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVD::fsgnj_d(FPURegister rd, FPURegister rs1,
                              FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVD::fsgnjn_d(FPURegister rd, FPURegister rs1,
                               FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVD::fsgnjx_d(FPURegister rd, FPURegister rs1,
                               FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVD::fmin_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010101, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVD::fmax_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010101, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVD::fcvt_s_d(FPURegister rd, FPURegister rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100000, frm, rd, rs1, ToRegister(1));
}

void AssemblerRISCVD::fcvt_d_s(FPURegister rd, FPURegister rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100001, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVD::feq_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVD::flt_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVD::fle_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVD::fclass_d(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110001, 0b001, rd, rs1, zero_reg);
}

void AssemblerRISCVD::fcvt_w_d(Register rd, FPURegister rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVD::fcvt_wu_d(Register rd, FPURegister rs1,
                                FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(1));
}

void AssemblerRISCVD::fcvt_d_w(FPURegister rd, Register rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVD::fcvt_d_wu(FPURegister rd, Register rs1,
                                FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(1));
}

#ifdef V8_TARGET_ARCH_RISCV64
// RV64D Standard Extension (in addition to RV32D)

void AssemblerRISCVD::fcvt_l_d(Register rd, FPURegister rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(2));
}

void AssemblerRISCVD::fcvt_lu_d(Register rd, FPURegister rs1,
                                FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(3));
}

void AssemblerRISCVD::fmv_x_d(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110001, 0b000, rd, rs1, zero_reg);
}

void AssemblerRISCVD::fcvt_d_l(FPURegister rd, Register rs1,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(2));
}

void AssemblerRISCVD::fcvt_d_lu(FPURegister rd, Register rs1,
                                FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(3));
}

void AssemblerRISCVD::fmv_d_x(FPURegister rd, Register rs1) {
  GenInstrALUFP_rr(0b1111001, 0b000, rd, rs1, zero_reg);
}
#endif

}  // namespace internal
}  // namespace v8
