// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_F_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_F_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

// RV32F Standard Extension
constexpr Opcode RO_FLW = LOAD_FP | (0b010 << kFunct3Shift);
constexpr Opcode RO_FSW = STORE_FP | (0b010 << kFunct3Shift);
constexpr Opcode RO_FMADD_S = MADD | (0b00 << kFunct2Shift);
constexpr Opcode RO_FMSUB_S = MSUB | (0b00 << kFunct2Shift);
constexpr Opcode RO_FNMSUB_S = NMSUB | (0b00 << kFunct2Shift);
constexpr Opcode RO_FNMADD_S = NMADD | (0b00 << kFunct2Shift);
constexpr Opcode RO_FADD_S = OP_FP | (0b0000000 << kFunct7Shift);
constexpr Opcode RO_FSUB_S = OP_FP | (0b0000100 << kFunct7Shift);
constexpr Opcode RO_FMUL_S = OP_FP | (0b0001000 << kFunct7Shift);
constexpr Opcode RO_FDIV_S = OP_FP | (0b0001100 << kFunct7Shift);
constexpr Opcode RO_FSQRT_S =
    OP_FP | (0b0101100 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FSGNJ_S =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010000 << kFunct7Shift);
constexpr Opcode RO_FSGNJN_S =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010000 << kFunct7Shift);
constexpr Opcode RO_FSQNJX_S =
    OP_FP | (0b010 << kFunct3Shift) | (0b0010000 << kFunct7Shift);
constexpr Opcode RO_FMIN_S =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010100 << kFunct7Shift);
constexpr Opcode RO_FMAX_S =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010100 << kFunct7Shift);
constexpr Opcode RO_FCVT_W_S =
    OP_FP | (0b1100000 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_WU_S =
    OP_FP | (0b1100000 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FMV = OP_FP | (0b1110000 << kFunct7Shift) |
                          (0b000 << kFunct3Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FEQ_S =
    OP_FP | (0b010 << kFunct3Shift) | (0b1010000 << kFunct7Shift);
constexpr Opcode RO_FLT_S =
    OP_FP | (0b001 << kFunct3Shift) | (0b1010000 << kFunct7Shift);
constexpr Opcode RO_FLE_S =
    OP_FP | (0b000 << kFunct3Shift) | (0b1010000 << kFunct7Shift);
constexpr Opcode RO_FCLASS_S =
    OP_FP | (0b001 << kFunct3Shift) | (0b1110000 << kFunct7Shift);
constexpr Opcode RO_FCVT_S_W =
    OP_FP | (0b1101000 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_S_WU =
    OP_FP | (0b1101000 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FMV_W_X =
    OP_FP | (0b000 << kFunct3Shift) | (0b1111000 << kFunct7Shift);

#ifdef V8_TARGET_ARCH_RISCV64
  // RV64F Standard Extension (in addition to RV32F)
constexpr Opcode RO_FCVT_L_S =
    OP_FP | (0b1100000 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_LU_S =
    OP_FP | (0b1100000 << kFunct7Shift) | (0b00011 << kRs2Shift);
constexpr Opcode RO_FCVT_S_L =
    OP_FP | (0b1101000 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_S_LU =
    OP_FP | (0b1101000 << kFunct7Shift) | (0b00011 << kRs2Shift);
#endif  // V8_TARGET_ARCH_RISCV64
// clang-format on
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_F_H_
