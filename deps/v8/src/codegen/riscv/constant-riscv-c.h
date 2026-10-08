// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

constexpr Opcode RO_C_ADDI4SPN = C0 | (0b000 << kRvcFunct3Shift);
constexpr Opcode RO_C_ADDI16SP = C1 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_LW = C0 | (0b010 << kRvcFunct3Shift);
constexpr Opcode RO_C_SW = C0 | (0b110 << kRvcFunct3Shift);
constexpr Opcode RO_C_NOP_ADDI = C1 | (0b000 << kRvcFunct3Shift);
constexpr Opcode RO_C_LI = C1 | (0b010 << kRvcFunct3Shift);
constexpr Opcode RO_C_SUB =
    C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift);
constexpr Opcode RO_C_XOR =
    C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift);
constexpr Opcode RO_C_OR =
    C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_2 << kRvcFunct2Shift);
constexpr Opcode RO_C_AND =
    C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_3 << kRvcFunct2Shift);
constexpr Opcode RO_C_LUI_ADD = C1 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_MISC_ALU = C1 | (0b100 << kRvcFunct3Shift);
constexpr Opcode RO_C_J = C1 | (0b101 << kRvcFunct3Shift);
constexpr Opcode RO_C_BEQZ = C1 | (0b110 << kRvcFunct3Shift);
constexpr Opcode RO_C_BNEZ = C1 | (0b111 << kRvcFunct3Shift);
constexpr Opcode RO_C_SLLI = C2 | (0b000 << kRvcFunct3Shift);
constexpr Opcode RO_C_LWSP = C2 | (0b010 << kRvcFunct3Shift);
constexpr Opcode RO_C_JR_MV_ADD = C2 | (0b100 << kRvcFunct3Shift);
constexpr Opcode RO_C_JR = C2 | (0b1000 << kRvcFunct4Shift);
constexpr Opcode RO_C_MV = C2 | (0b1000 << kRvcFunct4Shift);
constexpr Opcode RO_C_EBREAK = C2 | (0b1001 << kRvcFunct4Shift);
constexpr Opcode RO_C_JALR = C2 | (0b1001 << kRvcFunct4Shift);
constexpr Opcode RO_C_ADD = C2 | (0b1001 << kRvcFunct4Shift);
constexpr Opcode RO_C_SWSP = C2 | (0b110 << kRvcFunct3Shift);

constexpr Opcode RO_C_FSD = C0 | (0b101 << kRvcFunct3Shift);
constexpr Opcode RO_C_FLD = C0 | (0b001 << kRvcFunct3Shift);
constexpr Opcode RO_C_FLDSP = C2 | (0b001 << kRvcFunct3Shift);
constexpr Opcode RO_C_FSDSP = C2 | (0b101 << kRvcFunct3Shift);
#ifdef V8_TARGET_ARCH_RISCV64
constexpr Opcode RO_C_LD = C0 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_SD = C0 | (0b111 << kRvcFunct3Shift);
constexpr Opcode RO_C_LDSP = C2 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_SDSP = C2 | (0b111 << kRvcFunct3Shift);
constexpr Opcode RO_C_ADDIW = C1 | (0b001 << kRvcFunct3Shift);
constexpr Opcode RO_C_SUBW =
    C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift);
constexpr Opcode RO_C_ADDW =
    C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift);
#endif
#ifdef V8_TARGET_ARCH_RISCV32
constexpr Opcode RO_C_FLWSP = C2 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_FSWSP = C2 | (0b111 << kRvcFunct3Shift);
constexpr Opcode RO_C_FLW = C0 | (0b011 << kRvcFunct3Shift);
constexpr Opcode RO_C_FSW = C0 | (0b111 << kRvcFunct3Shift);
#endif
// Zcb extension.
//
// C0 quadrant: all five load/store instructions share funct3=100 and are
// distinguished by the fixed bits [12:10] and bit 6 (see
// https://github.com/riscv/riscv-isa-manual/blob/main/src/unpriv/zcb.adoc):
//   c.lbu rd', uimm(rs1'): 100 000 rs1' uimm[1:0] rd' 00
//     (uimm[1] = enc[5], uimm[0] = enc[6])
//   c.lhu rd', uimm(rs1'): 100 001 rs1' 0   uimm[1] rd' 00
//   c.lh  rd', uimm(rs1'): 100 001 rs1' 1   uimm[1] rd' 00
//   c.sb  rs2', uimm(rs1'): 100 010 rs1' uimm[1:0] rs2' 00
//   c.sh  rs2', uimm(rs1'): 100 011 rs1' 0   uimm[1] rs2' 00
constexpr Opcode RO_C_LBU = C0 | (0b100 << kRvcFunct3Shift) | (0b000 << 10);
constexpr Opcode RO_C_LHU = C0 | (0b100 << kRvcFunct3Shift) | (0b001 << 10);
constexpr Opcode RO_C_LH =
    C0 | (0b100 << kRvcFunct3Shift) | (0b001 << 10) | (0b1 << 6);
constexpr Opcode RO_C_SB = C0 | (0b100 << kRvcFunct3Shift) | (0b010 << 10);
constexpr Opcode RO_C_SH = C0 | (0b100 << kRvcFunct3Shift) | (0b011 << 10);
// C1 quadrant (CA format): c.mul reuses the reserved funct6=100111 slot.
constexpr Opcode RO_C_MUL =
    C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_2 << kRvcFunct2Shift);
// C1 quadrant (CU format, funct2=11): unary instructions whose opcode is
// determined by nzuimm[5:0] = 111xxx (56..61).
constexpr Opcode RO_C_ZEXT_B = C1 | (0b100111 << kRvcFunct6Shift) |
                               (FUNCT2_3 << kRvcFunct2Shift) |
                               (0b000 << kRvcRs2sShift);
constexpr Opcode RO_C_SEXT_B = C1 | (0b100111 << kRvcFunct6Shift) |
                               (FUNCT2_3 << kRvcFunct2Shift) |
                               (0b001 << kRvcRs2sShift);
constexpr Opcode RO_C_ZEXT_H = C1 | (0b100111 << kRvcFunct6Shift) |
                               (FUNCT2_3 << kRvcFunct2Shift) |
                               (0b010 << kRvcRs2sShift);
constexpr Opcode RO_C_SEXT_H = C1 | (0b100111 << kRvcFunct6Shift) |
                               (FUNCT2_3 << kRvcFunct2Shift) |
                               (0b011 << kRvcRs2sShift);
constexpr Opcode RO_C_NOT = C1 | (0b100111 << kRvcFunct6Shift) |
                            (FUNCT2_3 << kRvcFunct2Shift) |
                            (0b101 << kRvcRs2sShift);
#ifdef V8_TARGET_ARCH_RISCV64
constexpr Opcode RO_C_ZEXT_W = C1 | (0b100111 << kRvcFunct6Shift) |
                               (FUNCT2_3 << kRvcFunct2Shift) |
                               (0b100 << kRvcRs2sShift);
#endif
// clang-format on
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_
