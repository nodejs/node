// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-zfh.h"

namespace v8 {
namespace internal {

// RV32F Standard Extension

void AssemblerRISCVZfh::flh(FPURegister rd, Register rs1, int16_t imm12) {
  GenInstrLoadFP_ri(0b001, rd, rs1, imm12);
}

void AssemblerRISCVZfh::fsh(FPURegister source, Register base, int16_t imm12) {
  GenInstrStoreFP_rri(0b001, base, source, imm12);
}

void AssemblerRISCVZfh::fmadd_h(FPURegister rd, FPURegister rs1,
                                FPURegister rs2, FPURegister rs3,
                                FPURoundingMode frm) {
  GenInstrR4(0b10, MADD, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVZfh::fmsub_h(FPURegister rd, FPURegister rs1,
                                FPURegister rs2, FPURegister rs3,
                                FPURoundingMode frm) {
  GenInstrR4(0b10, MSUB, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVZfh::fnmsub_h(FPURegister rd, FPURegister rs1,
                                 FPURegister rs2, FPURegister rs3,
                                 FPURoundingMode frm) {
  GenInstrR4(0b10, NMSUB, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVZfh::fnmadd_h(FPURegister rd, FPURegister rs1,
                                 FPURegister rs2, FPURegister rs3,
                                 FPURoundingMode frm) {
  GenInstrR4(0b10, NMADD, rd, rs1, rs2, rs3, frm);
}

void AssemblerRISCVZfh::fadd_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0000010, frm, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fsub_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0000110, frm, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fmul_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0001010, frm, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fdiv_h(FPURegister rd, FPURegister rs1, FPURegister rs2,
                               FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0001110, frm, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fsqrt_h(FPURegister rd, FPURegister rs1,
                                FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0101110, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fsgnj_h(FPURegister rd, FPURegister rs1,
                                FPURegister rs2) {
  GenInstrALUFP_rr(0b0010010, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fsgnjn_h(FPURegister rd, FPURegister rs1,
                                 FPURegister rs2) {
  GenInstrALUFP_rr(0b0010010, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fsgnjx_h(FPURegister rd, FPURegister rs1,
                                 FPURegister rs2) {
  GenInstrALUFP_rr(0b0010010, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fmin_h(FPURegister rd, FPURegister rs1,
                               FPURegister rs2) {
  GenInstrALUFP_rr(0b0010110, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fmax_h(FPURegister rd, FPURegister rs1,
                               FPURegister rs2) {
  GenInstrALUFP_rr(0b0010110, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVZfh::feq_h(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010010, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVZfh::flt_h(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010010, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fle_h(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010010, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVZfh::fclass_h(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110010, 0b001, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fcvt_h_w(FPURegister rd, Register rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101010, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fcvt_h_wu(FPURegister rd, Register rs1,
                                  FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101010, frm, rd, rs1, ToRegister(1));
}

void AssemblerRISCVZfh::fcvt_w_h(Register rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100010, frm, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fcvt_wu_h(Register rd, FPURegister rs1,
                                  FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100010, frm, rd, rs1, ToRegister(1));
}

void AssemblerRISCVZfh::fmv_h_x(FPURegister rd, Register rs1) {
  GenInstrALUFP_rr(0b1111010, 0b000, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fmv_x_h(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110010, 0b000, rd, rs1, zero_reg);
}

void AssemblerRISCVZfh::fcvt_h_d(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100010, frm, rd, rs1, ToRegister(1));
}
void AssemblerRISCVZfh::fcvt_d_h(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100001, frm, rd, rs1, ToRegister(2));
}

void AssemblerRISCVZfh::fcvt_h_s(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100010, frm, rd, rs1, zero_reg);
}
void AssemblerRISCVZfh::fcvt_s_h(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b0100000, frm, rd, rs1, ToRegister(2));
}

#ifdef V8_TARGET_ARCH_RISCV64
// RV64F Standard Extension (in addition to RV32F)

void AssemblerRISCVZfh::fcvt_l_h(Register rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100010, frm, rd, rs1, ToRegister(2));
}

void AssemblerRISCVZfh::fcvt_lu_h(Register rd, FPURegister rs1,
                                  FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1100010, frm, rd, rs1, ToRegister(3));
}

void AssemblerRISCVZfh::fcvt_h_l(FPURegister rd, Register rs1,
                                 FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101010, frm, rd, rs1, ToRegister(2));
}

void AssemblerRISCVZfh::fcvt_h_lu(FPURegister rd, Register rs1,
                                  FPURoundingMode frm) {
  GenInstrALUFP_rr(0b1101010, frm, rd, rs1, ToRegister(3));
}
#endif

}  // namespace internal
}  // namespace v8
