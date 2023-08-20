// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-c.h"

namespace v8 {
namespace internal {
// RV64C Standard Extension
void AssemblerRISCVC::c_nop() { GenInstrCI(0b000, C1, zero_reg, 0); }

void AssemblerRISCVC::c_addi(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg && imm6 != 0);
  GenInstrCI(0b000, C1, rd, imm6);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_addiw(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg);
  GenInstrCI(0b001, C1, rd, imm6);
}
#endif

void AssemblerRISCVC::c_addi16sp(int16_t imm10) {
  DCHECK(is_int10(imm10) && (imm10 & 0xf) == 0);
  uint8_t uimm6 = ((imm10 & 0x200) >> 4) | (imm10 & 0x10) |
                  ((imm10 & 0x40) >> 3) | ((imm10 & 0x180) >> 6) |
                  ((imm10 & 0x20) >> 5);
  GenInstrCIU(0b011, C1, sp, uimm6);
}

void AssemblerRISCVC::c_addi4spn(Register rd, int16_t uimm10) {
  DCHECK(is_uint10(uimm10) && (uimm10 != 0));
  uint8_t uimm8 = ((uimm10 & 0x4) >> 1) | ((uimm10 & 0x8) >> 3) |
                  ((uimm10 & 0x30) << 2) | ((uimm10 & 0x3c0) >> 4);
  GenInstrCIW(0b000, C0, rd, uimm8);
}

void AssemblerRISCVC::c_li(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg);
  GenInstrCI(0b010, C1, rd, imm6);
}

void AssemblerRISCVC::c_lui(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg && rd != sp && imm6 != 0);
  GenInstrCI(0b011, C1, rd, imm6);
}

void AssemblerRISCVC::c_slli(Register rd, uint8_t shamt6) {
  DCHECK(rd != zero_reg && shamt6 != 0);
  GenInstrCIU(0b000, C2, rd, shamt6);
}

void AssemblerRISCVC::c_fldsp(FPURegister rd, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCIU(0b001, C2, rd, uimm6);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_ldsp(Register rd, uint16_t uimm9) {
  DCHECK(rd != zero_reg && is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCIU(0b011, C2, rd, uimm6);
}
#endif

void AssemblerRISCVC::c_lwsp(Register rd, uint16_t uimm8) {
  DCHECK(rd != zero_reg && is_uint8(uimm8) && (uimm8 & 0x3) == 0);
  uint8_t uimm6 = (uimm8 & 0x3c) | ((uimm8 & 0xc0) >> 6);
  GenInstrCIU(0b010, C2, rd, uimm6);
}

void AssemblerRISCVC::c_jr(Register rs1) {
  DCHECK(rs1 != zero_reg);
  GenInstrCR(0b1000, C2, rs1, zero_reg);
  BlockTrampolinePoolFor(1);
}

void AssemblerRISCVC::c_mv(Register rd, Register rs2) {
  DCHECK(rd != zero_reg && rs2 != zero_reg);
  GenInstrCR(0b1000, C2, rd, rs2);
}

void AssemblerRISCVC::c_ebreak() { GenInstrCR(0b1001, C2, zero_reg, zero_reg); }

void AssemblerRISCVC::c_jalr(Register rs1) {
  DCHECK(rs1 != zero_reg);
  GenInstrCR(0b1001, C2, rs1, zero_reg);
  BlockTrampolinePoolFor(1);
}

void AssemblerRISCVC::c_add(Register rd, Register rs2) {
  DCHECK(rd != zero_reg && rs2 != zero_reg);
  GenInstrCR(0b1001, C2, rd, rs2);
}

// CA Instructions
void AssemblerRISCVC::c_sub(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b00, rs2);
}

void AssemblerRISCVC::c_xor(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b01, rs2);
}

void AssemblerRISCVC::c_or(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b10, rs2);
}

void AssemblerRISCVC::c_and(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b11, rs2);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_subw(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100111, C1, rd, 0b00, rs2);
}

void AssemblerRISCVC::c_addw(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100111, C1, rd, 0b01, rs2);
}
#endif

void AssemblerRISCVC::c_swsp(Register rs2, uint16_t uimm8) {
  DCHECK(is_uint8(uimm8) && (uimm8 & 0x3) == 0);
  uint8_t uimm6 = (uimm8 & 0x3c) | ((uimm8 & 0xc0) >> 6);
  GenInstrCSS(0b110, C2, rs2, uimm6);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_sdsp(Register rs2, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCSS(0b111, C2, rs2, uimm6);
}
#endif

void AssemblerRISCVC::c_fsdsp(FPURegister rs2, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCSS(0b101, C2, rs2, uimm6);
}

// CL Instructions

void AssemblerRISCVC::c_lw(Register rd, Register rs1, uint16_t uimm7) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint7(uimm7) &&
         ((uimm7 & 0x3) == 0));
  uint8_t uimm5 =
      ((uimm7 & 0x4) >> 1) | ((uimm7 & 0x40) >> 6) | ((uimm7 & 0x38) >> 1);
  GenInstrCL(0b010, C0, rd, rs1, uimm5);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_ld(Register rd, Register rs1, uint16_t uimm8) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8) &&
         ((uimm8 & 0x7) == 0));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCL(0b011, C0, rd, rs1, uimm5);
}
#endif

void AssemblerRISCVC::c_fld(FPURegister rd, Register rs1, uint16_t uimm8) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8) &&
         ((uimm8 & 0x7) == 0));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCL(0b001, C0, rd, rs1, uimm5);
}

// CS Instructions

void AssemblerRISCVC::c_sw(Register rs2, Register rs1, uint16_t uimm7) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint7(uimm7) &&
         ((uimm7 & 0x3) == 0));
  uint8_t uimm5 =
      ((uimm7 & 0x4) >> 1) | ((uimm7 & 0x40) >> 6) | ((uimm7 & 0x38) >> 1);
  GenInstrCS(0b110, C0, rs2, rs1, uimm5);
}

#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVC::c_sd(Register rs2, Register rs1, uint16_t uimm8) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8) &&
         ((uimm8 & 0x7) == 0));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCS(0b111, C0, rs2, rs1, uimm5);
}
#endif

void AssemblerRISCVC::c_fsd(FPURegister rs2, Register rs1, uint16_t uimm8) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8) &&
         ((uimm8 & 0x7) == 0));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCS(0b101, C0, rs2, rs1, uimm5);
}

// CJ Instructions

void AssemblerRISCVC::c_j(int16_t imm12) {
  DCHECK(is_int12(imm12));
  int16_t uimm11 = ((imm12 & 0x800) >> 1) | ((imm12 & 0x400) >> 4) |
                   ((imm12 & 0x300) >> 1) | ((imm12 & 0x80) >> 3) |
                   ((imm12 & 0x40) >> 1) | ((imm12 & 0x20) >> 5) |
                   ((imm12 & 0x10) << 5) | (imm12 & 0xe);
  GenInstrCJ(0b101, C1, uimm11);
  BlockTrampolinePoolFor(1);
}

// CB Instructions

void AssemblerRISCVC::c_bnez(Register rs1, int16_t imm9) {
  DCHECK(((rs1.code() & 0b11000) == 0b01000) && is_int9(imm9));
  uint8_t uimm8 = ((imm9 & 0x20) >> 5) | ((imm9 & 0x6)) | ((imm9 & 0xc0) >> 3) |
                  ((imm9 & 0x18) << 2) | ((imm9 & 0x100) >> 1);
  GenInstrCB(0b111, C1, rs1, uimm8);
}

void AssemblerRISCVC::c_beqz(Register rs1, int16_t imm9) {
  DCHECK(((rs1.code() & 0b11000) == 0b01000) && is_int9(imm9));
  uint8_t uimm8 = ((imm9 & 0x20) >> 5) | ((imm9 & 0x6)) | ((imm9 & 0xc0) >> 3) |
                  ((imm9 & 0x18) << 2) | ((imm9 & 0x100) >> 1);
  GenInstrCB(0b110, C1, rs1, uimm8);
}

void AssemblerRISCVC::c_srli(Register rs1, int8_t shamt6) {
  DCHECK(((rs1.code() & 0b11000) == 0b01000) && is_int6(shamt6));
  GenInstrCBA(0b100, 0b00, C1, rs1, shamt6);
}

void AssemblerRISCVC::c_srai(Register rs1, int8_t shamt6) {
  DCHECK(((rs1.code() & 0b11000) == 0b01000) && is_int6(shamt6));
  GenInstrCBA(0b100, 0b01, C1, rs1, shamt6);
}

void AssemblerRISCVC::c_andi(Register rs1, int8_t imm6) {
  DCHECK(((rs1.code() & 0b11000) == 0b01000) && is_int6(imm6));
  GenInstrCBA(0b100, 0b10, C1, rs1, imm6);
}

bool AssemblerRISCVC::IsCJal(Instr instr) {
  return (instr & kRvcOpcodeMask) == RO_C_J;
}

bool AssemblerRISCVC::IsCBranch(Instr instr) {
  int Op = instr & kRvcOpcodeMask;
  return Op == RO_C_BNEZ || Op == RO_C_BEQZ;
}

int AssemblerRISCVC::CJumpOffset(Instr instr) {
  int32_t imm12 = ((instr & 0x4) << 3) | ((instr & 0x38) >> 2) |
                  ((instr & 0x40) << 1) | ((instr & 0x80) >> 1) |
                  ((instr & 0x100) << 2) | ((instr & 0x600) >> 1) |
                  ((instr & 0x800) >> 7) | ((instr & 0x1000) >> 1);
  imm12 = imm12 << 20 >> 20;
  return imm12;
}

}  // namespace internal
}  // namespace v8
