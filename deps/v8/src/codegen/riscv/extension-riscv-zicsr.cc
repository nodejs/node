// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-zicsr.h"

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/constant-riscv-zicsr.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {

void AssemblerRISCVZicsr::csrrw(Register rd, ControlStatusReg csr,
                                Register rs1) {
  GenInstrCSR_ir(0b001, rd, csr, rs1);
}

void AssemblerRISCVZicsr::csrrs(Register rd, ControlStatusReg csr,
                                Register rs1) {
  GenInstrCSR_ir(0b010, rd, csr, rs1);
}

void AssemblerRISCVZicsr::csrrc(Register rd, ControlStatusReg csr,
                                Register rs1) {
  GenInstrCSR_ir(0b011, rd, csr, rs1);
}

void AssemblerRISCVZicsr::csrrwi(Register rd, ControlStatusReg csr,
                                 uint8_t imm5) {
  GenInstrCSR_ii(0b101, rd, csr, imm5);
}

void AssemblerRISCVZicsr::csrrsi(Register rd, ControlStatusReg csr,
                                 uint8_t imm5) {
  GenInstrCSR_ii(0b110, rd, csr, imm5);
}

void AssemblerRISCVZicsr::csrrci(Register rd, ControlStatusReg csr,
                                 uint8_t imm5) {
  GenInstrCSR_ii(0b111, rd, csr, imm5);
}

}  // namespace internal
}  // namespace v8
