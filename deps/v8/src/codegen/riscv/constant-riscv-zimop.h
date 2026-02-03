// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIMOP_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIMOP_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {

namespace internal {
// RV32/RV64 Zimop Standard Extension

constexpr Opcode RO_MOP = SYSTEM | (0b100 << kFunct3Shift);
constexpr Opcode RO_MOP_R_N =
    RO_MOP | 0b00 << 28 | 0b1 << 31 | 0b0 << 25;
constexpr Opcode RO_MOP_RR_N =
    RO_MOP | 0b00 << 28 | 0b1 << 31 | 0b1 << 25;

// Zicfiss instructions
constexpr int SSPUSH_MOP_NUM = 7;
constexpr int SSPOPCHK_MOP_NUM = 28;
constexpr int SSRDP_MOP_NUM = 28;

constexpr Opcode SSAMOSWAP_W =
    AMO | (0b010 << kFunct3Shift) | (0b01001 << kFunct7Shift);
constexpr Opcode SSAMOSWAP_D =
    AMO | (0b011 << kFunct3Shift) | (0b01001 << kFunct7Shift);

}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_ZIMOP_H_
