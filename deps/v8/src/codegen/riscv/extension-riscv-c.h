// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_EXTENSION_RISCV_C_H_
#define V8_CODEGEN_RISCV_EXTENSION_RISCV_C_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/base-assembler-riscv.h"
#include "src/codegen/riscv/constant-riscv-c.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {
class AssemblerRISCVC : public AssemblerRiscvBase {
  // RV64C Standard Extension
 public:
  void c_nop();
  void c_addi(Register rd, int8_t imm6);

  void c_addi16sp(int16_t imm10);
  void c_addi4spn(Register rd, int16_t uimm10);
  void c_li(Register rd, int8_t imm6);
  void c_lui(Register rd, int8_t imm6);
  void c_slli(Register rd, uint8_t shamt6);
  void c_lwsp(Register rd, uint16_t uimm8);
  void c_jr(Register rs1);
  void c_mv(Register rd, Register rs2);
  void c_ebreak();
  void c_jalr(Register rs1);
  void c_j(int16_t imm12);
  void c_add(Register rd, Register rs2);
  void c_sub(Register rd, Register rs2);
  void c_and(Register rd, Register rs2);
  void c_xor(Register rd, Register rs2);
  void c_or(Register rd, Register rs2);
  void c_swsp(Register rs2, uint16_t uimm8);
  void c_lw(Register rd, Register rs1, uint16_t uimm7);
  void c_sw(Register rs2, Register rs1, uint16_t uimm7);
  void c_bnez(Register rs1, int16_t imm9);
  void c_beqz(Register rs1, int16_t imm9);
  void c_srli(Register rs1, int8_t shamt6);
  void c_srai(Register rs1, int8_t shamt6);
  void c_andi(Register rs1, int8_t imm6);

  void c_fld(FPURegister rd, Register rs1, uint16_t uimm8);
  void c_fsd(FPURegister rs2, Register rs1, uint16_t uimm8);
  void c_fldsp(FPURegister rd, uint16_t uimm9);
  void c_fsdsp(FPURegister rs2, uint16_t uimm9);
#ifdef V8_TARGET_ARCH_RISCV64
  void c_ld(Register rd, Register rs1, uint16_t uimm8);
  void c_sd(Register rs2, Register rs1, uint16_t uimm8);
  void c_subw(Register rd, Register rs2);
  void c_addw(Register rd, Register rs2);
  void c_addiw(Register rd, int8_t imm6);
  void c_ldsp(Register rd, uint16_t uimm9);
  void c_sdsp(Register rs2, uint16_t uimm9);
#endif

  int CJumpOffset(Instr instr);

  static bool IsCBranch(Instr instr);
  static bool IsCJal(Instr instr);

  inline int16_t cjump_offset(Label* L) {
    return (int16_t)branch_offset_helper(L, OffsetSize::kOffset11);
  }
  inline int32_t cbranch_offset(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset9);
  }

  void c_j(Label* L) { c_j(cjump_offset(L)); }
  void c_bnez(Register rs1, Label* L) { c_bnez(rs1, cbranch_offset(L)); }
  void c_beqz(Register rs1, Label* L) { c_beqz(rs1, cbranch_offset(L)); }
};
}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_RISCV_EXTENSION_RISCV_C_H_
