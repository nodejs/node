// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_I_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_I_H_
#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

enum OpcodeRISCV32I : uint32_t {
  // Note use RO (RiscV Opcode) prefix
  // RV32I Base Instruction Set
  RO_LUI = LUI,
  RO_AUIPC = AUIPC,
  RO_JAL = JAL,
  RO_JALR = JALR | (0b000 << kFunct3Shift),
  RO_BEQ = BRANCH | (0b000 << kFunct3Shift),
  RO_BNE = BRANCH | (0b001 << kFunct3Shift),
  RO_BLT = BRANCH | (0b100 << kFunct3Shift),
  RO_BGE = BRANCH | (0b101 << kFunct3Shift),
  RO_BLTU = BRANCH | (0b110 << kFunct3Shift),
  RO_BGEU = BRANCH | (0b111 << kFunct3Shift),
  RO_LB = LOAD | (0b000 << kFunct3Shift),
  RO_LH = LOAD | (0b001 << kFunct3Shift),
  RO_LW = LOAD | (0b010 << kFunct3Shift),
  RO_LBU = LOAD | (0b100 << kFunct3Shift),
  RO_LHU = LOAD | (0b101 << kFunct3Shift),
  RO_SB = STORE | (0b000 << kFunct3Shift),
  RO_SH = STORE | (0b001 << kFunct3Shift),
  RO_SW = STORE | (0b010 << kFunct3Shift),
  RO_ADDI = OP_IMM | (0b000 << kFunct3Shift),
  RO_SLTI = OP_IMM | (0b010 << kFunct3Shift),
  RO_SLTIU = OP_IMM | (0b011 << kFunct3Shift),
  RO_XORI = OP_IMM | (0b100 << kFunct3Shift),
  RO_ORI = OP_IMM | (0b110 << kFunct3Shift),
  RO_ANDI = OP_IMM | (0b111 << kFunct3Shift),
  RO_SLLI = OP_IMM | (0b001 << kFunct3Shift),
  RO_SRLI = OP_IMM | (0b101 << kFunct3Shift),
  // RO_SRAI = OP_IMM | (0b101 << kFunct3Shift), // Same as SRLI, use func7
  RO_ADD = OP | (0b000 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SUB = OP | (0b000 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_SLL = OP | (0b001 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SLT = OP | (0b010 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SLTU = OP | (0b011 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_XOR = OP | (0b100 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRL = OP | (0b101 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRA = OP | (0b101 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_OR = OP | (0b110 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_AND = OP | (0b111 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_FENCE = MISC_MEM | (0b000 << kFunct3Shift),
  RO_ECALL = SYSTEM | (0b000 << kFunct3Shift),
// RO_EBREAK = SYSTEM | (0b000 << kFunct3Shift), // Same as ECALL, use imm12

#if V8_TARGET_ARCH_RISCV64
  // RV64I Base Instruction Set (in addition to RV32I)
  RO_LWU = LOAD | (0b110 << kFunct3Shift),
  RO_LD = LOAD | (0b011 << kFunct3Shift),
  RO_SD = STORE | (0b011 << kFunct3Shift),
  RO_ADDIW = OP_IMM_32 | (0b000 << kFunct3Shift),
  RO_SLLIW = OP_IMM_32 | (0b001 << kFunct3Shift),
  RO_SRLIW = OP_IMM_32 | (0b101 << kFunct3Shift),
  // RO_SRAIW = OP_IMM_32 | (0b101 << kFunct3Shift), // Same as SRLIW, use func7
  RO_ADDW = OP_32 | (0b000 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SUBW = OP_32 | (0b000 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_SLLW = OP_32 | (0b001 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRLW = OP_32 | (0b101 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRAW = OP_32 | (0b101 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
#endif
};
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_I_H_
