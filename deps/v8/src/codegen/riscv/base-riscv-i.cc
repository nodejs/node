// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/base-riscv-i.h"

namespace v8 {
namespace internal {

void AssemblerRISCVI::lui(Register rd, int32_t imm20) {
  GenInstrU(LUI, rd, imm20);
}

void AssemblerRISCVI::auipc(Register rd, int32_t imm20) {
  GenInstrU(AUIPC, rd, imm20);
}

// Jumps

void AssemblerRISCVI::jal(Register rd, int32_t imm21) {
  GenInstrJ(JAL, rd, imm21);
  ClearVectorunit();
  BlockTrampolinePoolFor(1);
}

void AssemblerRISCVI::jalr(Register rd, Register rs1, int16_t imm12) {
  GenInstrI(0b000, JALR, rd, rs1, imm12);
  ClearVectorunit();
  BlockTrampolinePoolFor(1);
}

// Branches

void AssemblerRISCVI::beq(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b000, rs1, rs2, imm13);
  ClearVectorunit();
}

void AssemblerRISCVI::bne(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b001, rs1, rs2, imm13);
  ClearVectorunit();
}

void AssemblerRISCVI::blt(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b100, rs1, rs2, imm13);
  ClearVectorunit();
}

void AssemblerRISCVI::bge(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b101, rs1, rs2, imm13);
  ClearVectorunit();
}

void AssemblerRISCVI::bltu(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b110, rs1, rs2, imm13);
  ClearVectorunit();
}

void AssemblerRISCVI::bgeu(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b111, rs1, rs2, imm13);
  ClearVectorunit();
}

// Loads

void AssemblerRISCVI::lb(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b000, rd, rs1, imm12);
}

void AssemblerRISCVI::lh(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b001, rd, rs1, imm12);
}

void AssemblerRISCVI::lw(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b010, rd, rs1, imm12);
}

void AssemblerRISCVI::lbu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b100, rd, rs1, imm12);
}

void AssemblerRISCVI::lhu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b101, rd, rs1, imm12);
}

// Stores

void AssemblerRISCVI::sb(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b000, base, source, imm12);
}

void AssemblerRISCVI::sh(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b001, base, source, imm12);
}

void AssemblerRISCVI::sw(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b010, base, source, imm12);
}

// Arithmetic with immediate

void AssemblerRISCVI::addi(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b000, rd, rs1, imm12);
}

void AssemblerRISCVI::slti(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b010, rd, rs1, imm12);
}

void AssemblerRISCVI::sltiu(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b011, rd, rs1, imm12);
}

void AssemblerRISCVI::xori(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b100, rd, rs1, imm12);
}

void AssemblerRISCVI::ori(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b110, rd, rs1, imm12);
}

void AssemblerRISCVI::andi(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b111, rd, rs1, imm12);
}

void AssemblerRISCVI::slli(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(0, 0b001, rd, rs1, shamt & 0x3f);
}

void AssemblerRISCVI::srli(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(0, 0b101, rd, rs1, shamt & 0x3f);
}

void AssemblerRISCVI::srai(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(1, 0b101, rd, rs1, shamt & 0x3f);
}

// Arithmetic

void AssemblerRISCVI::add(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVI::sub(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0100000, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVI::sll(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVI::slt(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVI::sltu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b011, rd, rs1, rs2);
}

void AssemblerRISCVI::xor_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b100, rd, rs1, rs2);
}

void AssemblerRISCVI::srl(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b101, rd, rs1, rs2);
}

void AssemblerRISCVI::sra(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0100000, 0b101, rd, rs1, rs2);
}

void AssemblerRISCVI::or_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b110, rd, rs1, rs2);
}

void AssemblerRISCVI::and_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b111, rd, rs1, rs2);
}

// Memory fences

void AssemblerRISCVI::fence(uint8_t pred, uint8_t succ) {
  DCHECK(is_uint4(pred) && is_uint4(succ));
  uint16_t imm12 = succ | (pred << 4) | (0b0000 << 8);
  GenInstrI(0b000, MISC_MEM, ToRegister(0), ToRegister(0), imm12);
}

void AssemblerRISCVI::fence_tso() {
  uint16_t imm12 = (0b0011) | (0b0011 << 4) | (0b1000 << 8);
  GenInstrI(0b000, MISC_MEM, ToRegister(0), ToRegister(0), imm12);
}

// Environment call / break

void AssemblerRISCVI::ecall() {
  GenInstrI(0b000, SYSTEM, ToRegister(0), ToRegister(0), 0);
}

void AssemblerRISCVI::ebreak() {
  GenInstrI(0b000, SYSTEM, ToRegister(0), ToRegister(0), 1);
}

// This is a de facto standard (as set by GNU binutils) 32-bit unimplemented
// instruction (i.e., it should always trap, if your implementation has invalid
// instruction traps).
void AssemblerRISCVI::unimp() {
  GenInstrI(0b001, SYSTEM, ToRegister(0), ToRegister(0), 0b110000000000);
}

bool AssemblerRISCVI::IsBranch(Instr instr) {
  return (instr & kBaseOpcodeMask) == BRANCH;
}

bool AssemblerRISCVI::IsJump(Instr instr) {
  int Op = instr & kBaseOpcodeMask;
  return Op == JAL || Op == JALR;
}

bool AssemblerRISCVI::IsNop(Instr instr) { return instr == kNopByte; }

bool AssemblerRISCVI::IsJal(Instr instr) {
  return (instr & kBaseOpcodeMask) == JAL;
}

bool AssemblerRISCVI::IsJalr(Instr instr) {
  return (instr & kBaseOpcodeMask) == JALR;
}

bool AssemblerRISCVI::IsLui(Instr instr) {
  return (instr & kBaseOpcodeMask) == LUI;
}
bool AssemblerRISCVI::IsAuipc(Instr instr) {
  return (instr & kBaseOpcodeMask) == AUIPC;
}
bool AssemblerRISCVI::IsAddi(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ADDI;
}
bool AssemblerRISCVI::IsOri(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ORI;
}
bool AssemblerRISCVI::IsSlli(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_SLLI;
}

int AssemblerRISCVI::JumpOffset(Instr instr) {
  int32_t imm21 = ((instr & 0x7fe00000) >> 20) | ((instr & 0x100000) >> 9) |
                  (instr & 0xff000) | ((instr & 0x80000000) >> 11);
  imm21 = imm21 << 11 >> 11;
  return imm21;
}

int AssemblerRISCVI::JalrOffset(Instr instr) {
  DCHECK(IsJalr(instr));
  int32_t imm12 = static_cast<int32_t>(instr & kImm12Mask) >> 20;
  return imm12;
}

int AssemblerRISCVI::AuipcOffset(Instr instr) {
  DCHECK(IsAuipc(instr));
  int32_t imm20 = static_cast<int32_t>(instr & kImm20Mask);
  return imm20;
}

bool AssemblerRISCVI::IsLw(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_LW;
}

int AssemblerRISCVI::LoadOffset(Instr instr) {
#if V8_TARGET_ARCH_RISCV64
  DCHECK(IsLd(instr));
#elif V8_TARGET_ARCH_RISCV32
  DCHECK(IsLw(instr));
#endif
  int32_t imm12 = static_cast<int32_t>(instr & kImm12Mask) >> 20;
  return imm12;
}

#ifdef V8_TARGET_ARCH_RISCV64

bool AssemblerRISCVI::IsAddiw(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ADDIW;
}

bool AssemblerRISCVI::IsLd(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_LD;
}

void AssemblerRISCVI::lwu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b110, rd, rs1, imm12);
}

void AssemblerRISCVI::ld(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b011, rd, rs1, imm12);
}

void AssemblerRISCVI::sd(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b011, base, source, imm12);
}

void AssemblerRISCVI::addiw(Register rd, Register rs1, int16_t imm12) {
  GenInstrI(0b000, OP_IMM_32, rd, rs1, imm12);
}

void AssemblerRISCVI::slliw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(0, 0b001, rd, rs1, shamt & 0x1f);
}

void AssemblerRISCVI::srliw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(0, 0b101, rd, rs1, shamt & 0x1f);
}

void AssemblerRISCVI::sraiw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(1, 0b101, rd, rs1, shamt & 0x1f);
}

void AssemblerRISCVI::addw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVI::subw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0100000, 0b000, rd, rs1, rs2);
}

void AssemblerRISCVI::sllw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b001, rd, rs1, rs2);
}

void AssemblerRISCVI::srlw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b101, rd, rs1, rs2);
}

void AssemblerRISCVI::sraw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0100000, 0b101, rd, rs1, rs2);
}

#endif

}  // namespace internal
}  // namespace v8
