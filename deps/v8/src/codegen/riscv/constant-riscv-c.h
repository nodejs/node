// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

enum OpcodeRISCVC : uint32_t {

  RO_C_ADDI4SPN = C0 | (0b000 << kRvcFunct3Shift),
  RO_C_ADDI16SP = C1 | (0b011 << kRvcFunct3Shift),
  RO_C_LW = C0 | (0b010 << kRvcFunct3Shift),
  RO_C_SW = C0 | (0b110 << kRvcFunct3Shift),
  RO_C_NOP_ADDI = C1 | (0b000 << kRvcFunct3Shift),
  RO_C_LI = C1 | (0b010 << kRvcFunct3Shift),
  RO_C_SUB = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift),
  RO_C_XOR = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift),
  RO_C_OR = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_2 << kRvcFunct2Shift),
  RO_C_AND = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_3 << kRvcFunct2Shift),
  RO_C_LUI_ADD = C1 | (0b011 << kRvcFunct3Shift),
  RO_C_MISC_ALU = C1 | (0b100 << kRvcFunct3Shift),
  RO_C_J = C1 | (0b101 << kRvcFunct3Shift),
  RO_C_BEQZ = C1 | (0b110 << kRvcFunct3Shift),
  RO_C_BNEZ = C1 | (0b111 << kRvcFunct3Shift),
  RO_C_SLLI = C2 | (0b000 << kRvcFunct3Shift),
  RO_C_LWSP = C2 | (0b010 << kRvcFunct3Shift),
  RO_C_JR_MV_ADD = C2 | (0b100 << kRvcFunct3Shift),
  RO_C_JR = C2 | (0b1000 << kRvcFunct4Shift),
  RO_C_MV = C2 | (0b1000 << kRvcFunct4Shift),
  RO_C_EBREAK = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_JALR = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_ADD = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_SWSP = C2 | (0b110 << kRvcFunct3Shift),

  RO_C_FSD = C0 | (0b101 << kRvcFunct3Shift),
  RO_C_FLD = C0 | (0b001 << kRvcFunct3Shift),
  RO_C_FLDSP = C2 | (0b001 << kRvcFunct3Shift),
  RO_C_FSDSP = C2 | (0b101 << kRvcFunct3Shift),
#ifdef V8_TARGET_ARCH_RISCV64
  RO_C_LD = C0 | (0b011 << kRvcFunct3Shift),
  RO_C_SD = C0 | (0b111 << kRvcFunct3Shift),
  RO_C_LDSP = C2 | (0b011 << kRvcFunct3Shift),
  RO_C_SDSP = C2 | (0b111 << kRvcFunct3Shift),
  RO_C_ADDIW = C1 | (0b001 << kRvcFunct3Shift),
  RO_C_SUBW =
      C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift),
  RO_C_ADDW =
      C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift),
#endif
#ifdef V8_TARGET_ARCH_RISCV32
  RO_C_FLWSP = C2 | (0b011 << kRvcFunct3Shift),
  RO_C_FSWSP = C2 | (0b111 << kRvcFunct3Shift),
  RO_C_FLW = C0 | (0b011 << kRvcFunct3Shift),
  RO_C_FSW = C0 | (0b111 << kRvcFunct3Shift),
#endif
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_C_H_
