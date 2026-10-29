// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_M_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_M_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-m.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {
class AssemblerRISCVM : public AssemblerRiscvBase {
  // RV32M Standard Extension
 public:
  void mul(Register rd, Register rs1, Register rs2);
  void mulh(Register rd, Register rs1, Register rs2);
  void mulhsu(Register rd, Register rs1, Register rs2);
  void mulhu(Register rd, Register rs1, Register rs2);
  void div(Register rd, Register rs1, Register rs2);
  void divu(Register rd, Register rs1, Register rs2);
  void rem(Register rd, Register rs1, Register rs2);
  void remu(Register rd, Register rs1, Register rs2);
#ifdef V8_TARGET_ARCH_RISCV64
  // RV64M Standard Extension (in addition to RV32M)
  void mulw(Register rd, Register rs1, Register rs2);
  void divw(Register rd, Register rs1, Register rs2);
  void divuw(Register rd, Register rs1, Register rs2);
  void remw(Register rd, Register rs1, Register rs2);
  void remuw(Register rd, Register rs1, Register rs2);
#endif
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_M_H_
