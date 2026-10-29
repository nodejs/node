// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_ZIMOP_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_ZIMOP_H_
#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {

namespace internal {

class AssemblerRISCVZimop : public AssemblerRiscvBase {
 public:
  void mop_r(int n, Register rd, Register rs1);
  void mop_rr(int n, Register rd, Register rs1, Register rs2);
};

// extension for Zicfiss instructions.
class AssemblerRISCVZicfiss : public AssemblerRISCVZimop {
 public:
  void sspush_ra() { mop_rr(SSPUSH_MOP_NUM, zero_reg, zero_reg, ra); }
  void sspush_t0() { mop_rr(SSPUSH_MOP_NUM, zero_reg, zero_reg, t0); }
  void sspopchk_ra() { mop_r(SSPOPCHK_MOP_NUM, zero_reg, ra); }
  void sspopchk_t0() { mop_r(SSPOPCHK_MOP_NUM, zero_reg, t0); }

  void ssrdp(Register rd) {
    DCHECK_NE(rd, zero_reg);
    mop_r(SSRDP_MOP_NUM, rd, zero_reg);
  }

  void ssamoswap_x(bool aq, bool rl, Register rd, Register addr, Register src);
  void ssamoswap_d(bool aq, bool rl, Register rd, Register addr, Register src);
};

}  // namespace internal

}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_ZIMOP_H_
