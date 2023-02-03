// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-a.h"
#include "src/codegen/riscv/register-riscv.h"
#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_A_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_A_H_

namespace v8 {
namespace internal {
class AssemblerRISCVA : public AssemblerRiscvBase {
  // RV32A Standard Extension
 public:
  void lr_w(bool aq, bool rl, Register rd, Register rs1);
  void sc_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoswap_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoadd_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoxor_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoand_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoor_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomin_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomax_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amominu_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomaxu_w(bool aq, bool rl, Register rd, Register rs1, Register rs2);

#ifdef V8_TARGET_ARCH_RISCV64
  // RV64A Standard Extension (in addition to RV32A)
  void lr_d(bool aq, bool rl, Register rd, Register rs1);
  void sc_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoswap_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoadd_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoxor_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoand_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amoor_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomin_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomax_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amominu_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
  void amomaxu_d(bool aq, bool rl, Register rd, Register rs1, Register rs2);
#endif
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_A_H_
