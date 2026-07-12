// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_D_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_D_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

// RV32D Standard Extension
constexpr Opcode RO_FLD = LOAD_FP | (0b011 << kFunct3Shift);
constexpr Opcode RO_FSD = STORE_FP | (0b011 << kFunct3Shift);
constexpr Opcode RO_FMADD_D = MADD | (0b01 << kFunct2Shift);
constexpr Opcode RO_FMSUB_D = MSUB | (0b01 << kFunct2Shift);
constexpr Opcode RO_FNMSUB_D = NMSUB | (0b01 << kFunct2Shift);
constexpr Opcode RO_FNMADD_D = NMADD | (0b01 << kFunct2Shift);
constexpr Opcode RO_FADD_D = OP_FP | (0b0000001 << kFunct7Shift);
constexpr Opcode RO_FSUB_D = OP_FP | (0b0000101 << kFunct7Shift);
constexpr Opcode RO_FMUL_D = OP_FP | (0b0001001 << kFunct7Shift);
constexpr Opcode RO_FDIV_D = OP_FP | (0b0001101 << kFunct7Shift);
constexpr Opcode RO_FSQRT_D =
    OP_FP | (0b0101101 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FSGNJ_D =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010001 << kFunct7Shift);
constexpr Opcode RO_FSGNJN_D =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010001 << kFunct7Shift);
constexpr Opcode RO_FSQNJX_D =
    OP_FP | (0b010 << kFunct3Shift) | (0b0010001 << kFunct7Shift);
constexpr Opcode RO_FMIN_D =
    OP_FP | (0b000 << kFunct3Shift) | (0b0010101 << kFunct7Shift);
constexpr Opcode RO_FMAX_D =
    OP_FP | (0b001 << kFunct3Shift) | (0b0010101 << kFunct7Shift);
constexpr Opcode RO_FCVT_S_D =
    OP_FP | (0b0100000 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FCVT_D_S =
    OP_FP | (0b0100001 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FEQ_D =
    OP_FP | (0b010 << kFunct3Shift) | (0b1010001 << kFunct7Shift);
constexpr Opcode RO_FLT_D =
    OP_FP | (0b001 << kFunct3Shift) | (0b1010001 << kFunct7Shift);
constexpr Opcode RO_FLE_D =
    OP_FP | (0b000 << kFunct3Shift) | (0b1010001 << kFunct7Shift);
constexpr Opcode RO_FCLASS_D = OP_FP | (0b001 << kFunct3Shift) |
                               (0b1110001 << kFunct7Shift) |
                               (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_W_D =
    OP_FP | (0b1100001 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_WU_D =
    OP_FP | (0b1100001 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FCVT_D_W =
    OP_FP | (0b1101001 << kFunct7Shift) | (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_D_WU =
    OP_FP | (0b1101001 << kFunct7Shift) | (0b00001 << kRs2Shift);

#ifdef V8_TARGET_ARCH_RISCV64
  // RV64D Standard Extension (in addition to RV32D)
constexpr Opcode RO_FCVT_L_D =
    OP_FP | (0b1100001 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_LU_D =
    OP_FP | (0b1100001 << kFunct7Shift) | (0b00011 << kRs2Shift);
constexpr Opcode RO_FMV_X_D = OP_FP | (0b000 << kFunct3Shift) |
                              (0b1110001 << kFunct7Shift) |
                              (0b00000 << kRs2Shift);
constexpr Opcode RO_FCVT_D_L =
    OP_FP | (0b1101001 << kFunct7Shift) | (0b00010 << kRs2Shift);
constexpr Opcode RO_FCVT_D_LU =
    OP_FP | (0b1101001 << kFunct7Shift) | (0b00011 << kRs2Shift);
constexpr Opcode RO_FMV_D_X = OP_FP | (0b000 << kFunct3Shift) |
                              (0b1111001 << kFunct7Shift) |
                              (0b00000 << kRs2Shift);
#endif
// clang-format on
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_D_H_
