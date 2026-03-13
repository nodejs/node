// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFH_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFH_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

// RV32F Standard Extension
constexpr Opcode RO_FLH = LOAD_FP | (0b001 << kFunct3Shift);
constexpr Opcode RO_FSH = STORE_FP | (0b001 << kFunct3Shift);
constexpr Opcode RO_FMADD_H = MADD | (0b10 << kFunct2Shift);
constexpr Opcode RO_FMSUB_H = MSUB | (0b10 << kFunct2Shift);
constexpr Opcode RO_FNMSUB_H = NMSUB | (0b10 << kFunct2Shift);
constexpr Opcode RO_FNMADD_H = NMADD | (0b10 << kFunct2Shift);
constexpr Opcode RO_FADD_H = OP_FP | (0b0000010 << kFunct7Shift);
constexpr Opcode RO_FSUB_H = OP_FP | (0b0000110 << kFunct7Shift);
constexpr Opcode RO_FMUL_H = OP_FP | (0b0001010 << kFunct7Shift);
constexpr Opcode RO_FDIV_H = OP_FP | (0b0001110 << kFunct7Shift);
constexpr Opcode RO_FSQRT_H =
    OP_FP | (0b0101110 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FSGNJ_H =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010010 << kFunct7Shift);
constexpr Opcode RO_FSGNJN_H =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010010 << kFunct7Shift);
constexpr Opcode RO_FSQNJX_H =
    OP_FP | (0b010 << kFunct3Shift) | (0b0010010 << kFunct7Shift);
constexpr Opcode RO_FMIN_H =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010110 << kFunct7Shift);
constexpr Opcode RO_FMAX_H =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010110 << kFunct7Shift);
constexpr Opcode RO_FCVT_W_H =
    OP_FP | (0b1100010 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_WU_H =
    OP_FP | (0b1100010 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FMV_X_H = OP_FP | (0b1110010 << kFunct7Shift) |
                              (0b000 << kFunct3Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FEQ_H =
    OP_FP | (0b010 << kFunct3Shift) | (0b1010010 << kFunct7Shift);
constexpr Opcode RO_FLT_H =
    OP_FP | (0b001 << kFunct3Shift) | (0b1010010 << kFunct7Shift);
constexpr Opcode RO_FLE_H =
    OP_FP | (0b000 << kFunct3Shift) | (0b1010010 << kFunct7Shift);
constexpr Opcode RO_FCLASS_H =
    OP_FP | (0b001 << kFunct3Shift) | (0b1110010 << kFunct7Shift);
constexpr Opcode RO_FMV_H_X =
    OP_FP | (0b000 << kFunct3Shift) | (0b1111010 << kFunct7Shift);

constexpr Opcode RO_FCVT_H_W =
    OP_FP | (0b1101010 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_H_WU =
    OP_FP | (0b1101010 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FCVT_D_H =
    OP_FP | (0b0100001 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_S_H =
    OP_FP | (0b0100000 << kFunct7Shift) | (0b00010 << kRs2Shift);

constexpr Opcode RO_FCVT_H_D =
    OP_FP | (0b0100010 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FCVT_H_S =
    OP_FP | (0b0100010 << kFunct7Shift) | (0b00000 << kRs2Shift);

#ifdef V8_TARGET_ARCH_RISCV64
// RV64F Standard Extension (in addition to RV32F)
constexpr Opcode RO_FCVT_L_H =
    OP_FP | (0b1100010 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_LU_H =
    OP_FP | (0b1100010 << kFunct7Shift) | (0b00011 << kRs2Shift);
constexpr Opcode RO_FCVT_H_L =
    OP_FP | (0b1101010 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_H_LU =
    OP_FP | (0b1101010 << kFunct7Shift) | (0b00011 << kRs2Shift);
#endif  // V8_TARGET_ARCH_RISCV64
// clang-format on
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFH_H_
