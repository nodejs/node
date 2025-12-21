// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-zimop.h"

#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-zimop.h"

namespace v8 {
namespace internal {

void AssemblerRISCVZimop::mop_r(int n, Register rd, Register rs1) {
  DCHECK(is_uint5(n));
  Instr instr = SYSTEM | (rd.code() << kRdShift) | (0b100 << kFunct3Shift) |
                0b1 << 31 | 0b00 << 28 | 0b0 << 25 | 0b111 << 22 |
                (rs1.code() << kRs1Shift) | (n & 0b11) << 20 |
                ((n >> 2) & 0b11) << 26 | ((n >> 4) & 0b1) << 30;
  emit(instr);
}
void AssemblerRISCVZimop::mop_rr(int n, Register rd, Register rs1,
                                 Register rs2) {
  DCHECK(is_uint3(n));
  Instr instr = SYSTEM | (rd.code() << kRdShift) | (0b100 << kFunct3Shift) |
                0b1 << 31 | 0b00 << 28 | 0b1 << 25 | (rs1.code() << kRs1Shift) |
                (rs2.code() << kRs2Shift) | (n & 0b11) << 26 |
                ((n >> 2) & 0b1) << 30;
  emit(instr);
}

void AssemblerRISCVZicfiss::ssamoswap_x(bool aq, bool rl, Register rd,
                                        Register rs1, Register rs2) {
  GenInstrRAtomic(0b01001, aq, rl, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVZicfiss::ssamoswap_d(bool aq, bool rl, Register rd,
                                        Register rs1, Register rs2) {
  GenInstrRAtomic(0b01001, aq, rl, 0b011, rd, rs1, rs2);
}

}  // namespace internal
}  // namespace v8
