// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/codegen/riscv/extension-riscv-a.h"

namespace v8 {
namespace internal {

// RV32A Standard Extension
void AssemblerRISCVA::lr_w(bool aq, bool rl, Register rd, Register addr) {
  GenInstrRAtomic(0b00010, aq, rl, 0b010, rd, addr, zero_reg);
}

void AssemblerRISCVA::sc_w(bool aq, bool rl, Register rd, Register addr,
                           Register src) {
  GenInstrRAtomic(0b00011, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amoswap_w(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b00001, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amoadd_w(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b00000, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amoxor_w(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b00100, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amoand_w(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b01100, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amoor_w(bool aq, bool rl, Register rd, Register addr,
                              Register src) {
  GenInstrRAtomic(0b01000, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amomin_w(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b10000, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amomax_w(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b10100, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amominu_w(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b11000, aq, rl, 0b010, rd, addr, src);
}

void AssemblerRISCVA::amomaxu_w(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b11100, aq, rl, 0b010, rd, addr, src);
}

// RV64A Standard Extension (in addition to RV32A)
#ifdef V8_TARGET_ARCH_RISCV64
void AssemblerRISCVA::lr_d(bool aq, bool rl, Register rd, Register addr) {
  GenInstrRAtomic(0b00010, aq, rl, 0b011, rd, addr, zero_reg);
}

void AssemblerRISCVA::sc_d(bool aq, bool rl, Register rd, Register addr,
                           Register src) {
  GenInstrRAtomic(0b00011, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amoswap_d(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b00001, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amoadd_d(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b00000, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amoxor_d(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b00100, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amoand_d(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b01100, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amoor_d(bool aq, bool rl, Register rd, Register addr,
                              Register src) {
  GenInstrRAtomic(0b01000, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amomin_d(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b10000, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amomax_d(bool aq, bool rl, Register rd, Register addr,
                               Register src) {
  GenInstrRAtomic(0b10100, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amominu_d(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b11000, aq, rl, 0b011, rd, addr, src);
}

void AssemblerRISCVA::amomaxu_d(bool aq, bool rl, Register rd, Register addr,
                                Register src) {
  GenInstrRAtomic(0b11100, aq, rl, 0b011, rd, addr, src);
}
#endif
}  // namespace internal
}  // namespace v8
