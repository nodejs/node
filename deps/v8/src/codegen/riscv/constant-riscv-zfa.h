// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFA_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFA_H_

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "src/base/macros.h"
#include "src/codegen/riscv/base-constants-riscv.h"

namespace v8 {
namespace internal {

// =============================================================================
// Zfa Extension (Additional Floating-Point Instructions)
// =============================================================================
// Instruction encoding reference (funct7 = funct5 << 2 | funct2):
//   funct2=0: single-precision, funct2=1: double-precision
//
//   fli.s/fli.d:      funct7=0b1111000/0b1111001, funct3=0, rs2=1
//   fminm.s/fminm.d:  funct7=0b0010100/0b0010101, funct3=2
//   fmaxm.s/fmaxm.d:  funct7=0b0010100/0b0010101, funct3=3
//   fround.s/fround.d:    funct7=0b0100000/0b0100001, funct3=rm, rs2=4
//   froundnx.s/froundnx.d: funct7=0b0100000/0b0100001, funct3=rm, rs2=5
//   fleq.s/fleq.d:    funct7=0b1010000/0b1010001, funct3=4
//   fltq.s/fltq.d:    funct7=0b1010000/0b1010001, funct3=5
//   fcvtmod.w.d:      funct7=0b1100001, funct3=1, rs2=8

// RV32Zfa / RV64Zfa Single-Precision Instructions
constexpr Opcode RO_FLI_S =
    OP_FP | (0b1111000 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FMINM_S =
    OP_FP | (0b0010100 << kFunct7Shift) | (0b010 << kFunct3Shift);
constexpr Opcode RO_FMAXM_S =
    OP_FP | (0b0010100 << kFunct7Shift) | (0b011 << kFunct3Shift);
constexpr Opcode RO_FROUND_S =
    OP_FP | (0b0100000 << kFunct7Shift) | (0b00100 << kRs2Shift);
constexpr Opcode RO_FROUNDNX_S =
    OP_FP | (0b0100000 << kFunct7Shift) | (0b00101 << kRs2Shift);
constexpr Opcode RO_FLEQ_S =
    OP_FP | (0b1010000 << kFunct7Shift) | (0b100 << kFunct3Shift);
constexpr Opcode RO_FLTQ_S =
    OP_FP | (0b1010000 << kFunct7Shift) | (0b101 << kFunct3Shift);

// RV32Zfa / RV64Zfa Double-Precision Instructions
constexpr Opcode RO_FLI_D =
    OP_FP | (0b1111001 << kFunct7Shift) | (0b00001 << kRs2Shift);
constexpr Opcode RO_FMINM_D =
    OP_FP | (0b0010101 << kFunct7Shift) | (0b010 << kFunct3Shift);
constexpr Opcode RO_FMAXM_D =
    OP_FP | (0b0010101 << kFunct7Shift) | (0b011 << kFunct3Shift);
constexpr Opcode RO_FROUND_D =
    OP_FP | (0b0100001 << kFunct7Shift) | (0b00100 << kRs2Shift);
constexpr Opcode RO_FROUNDNX_D =
    OP_FP | (0b0100001 << kFunct7Shift) | (0b00101 << kRs2Shift);
constexpr Opcode RO_FCVTMOD_W_D = OP_FP | (0b1100001 << kFunct7Shift) |
                                  (0b001 << kFunct3Shift) |
                                  (0b01000 << kRs2Shift);
constexpr Opcode RO_FLEQ_D =
    OP_FP | (0b1010001 << kFunct7Shift) | (0b100 << kFunct3Shift);
constexpr Opcode RO_FLTQ_D =
    OP_FP | (0b1010001 << kFunct7Shift) | (0b101 << kFunct3Shift);
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_ZFA_H_
