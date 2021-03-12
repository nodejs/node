// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_RISCV64

#include "src/codegen/riscv64/constants-riscv64.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Registers.

// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* Registers::names_[kNumSimuRegisters] = {
    "zero_reg", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
    "a1",       "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",       "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc"};

// List of alias names which can be used when referring to RISC-V registers.
const Registers::RegisterAlias Registers::aliases_[] = {
    {0, "zero"},
    {33, "pc"},
    {8, "s0"},
    {8, "s0_fp"},
    {kInvalidRegister, nullptr}};

const char* Registers::Name(int reg) {
  const char* result;
  if ((0 <= reg) && (reg < kNumSimuRegisters)) {
    result = names_[reg];
  } else {
    result = "noreg";
  }
  return result;
}

int Registers::Number(const char* name) {
  // Look through the canonical names.
  for (int i = 0; i < kNumSimuRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
    }
  }

  // Look through the alias names.
  int i = 0;
  while (aliases_[i].reg != kInvalidRegister) {
    if (strcmp(aliases_[i].name, name) == 0) {
      return aliases_[i].reg;
    }
    i++;
  }

  // No register with the reguested name found.
  return kInvalidRegister;
}

/*
const char* FPURegisters::names_[kNumFPURegisters] = {
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",  "f8",  "f9",  "f10",
    "f11", "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21",
    "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"};
*/
const char* FPURegisters::names_[kNumFPURegisters] = {
    "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
    "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

// List of alias names which can be used when referring to RISC-V FP registers.
const FPURegisters::RegisterAlias FPURegisters::aliases_[] = {
    {kInvalidRegister, nullptr}};

const char* FPURegisters::Name(int creg) {
  const char* result;
  if ((0 <= creg) && (creg < kNumFPURegisters)) {
    result = names_[creg];
  } else {
    result = "nocreg";
  }
  return result;
}

int FPURegisters::Number(const char* name) {
  // Look through the canonical names.
  for (int i = 0; i < kNumFPURegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
    }
  }

  // Look through the alias names.
  int i = 0;
  while (aliases_[i].creg != kInvalidRegister) {
    if (strcmp(aliases_[i].name, name) == 0) {
      return aliases_[i].creg;
    }
    i++;
  }

  // No Cregister with the reguested name found.
  return kInvalidFPURegister;
}

InstructionBase::Type InstructionBase::InstructionType() const {
  // RV64C Instruction
  if (IsShortInstruction()) {
    switch (InstructionBits() & kRvcOpcodeMask) {
      case RO_C_ADDI4SPN:
        return kCIWType;
      case RO_C_FLD:
      case RO_C_LW:
      case RO_C_LD:
        return kCLType;
      case RO_C_FSD:
      case RO_C_SW:
      case RO_C_SD:
        return kCSType;
      case RO_C_NOP_ADDI:
      case RO_C_ADDIW:
      case RO_C_LI:
      case RO_C_LUI_ADD:
        return kCIType;
      case RO_C_MISC_ALU:
        if (Bits(11, 10) != 0b11)
          return kCBType;
        else
          return kCAType;
      case RO_C_J:
        return kCJType;
      case RO_C_BEQZ:
      case RO_C_BNEZ:
        return kCBType;
      case RO_C_SLLI:
      case RO_C_FLDSP:
      case RO_C_LWSP:
      case RO_C_LDSP:
        return kCIType;
      case RO_C_JR_MV_ADD:
        return kCRType;
      case RO_C_FSDSP:
      case RO_C_SWSP:
      case RO_C_SDSP:
        return kCSSType;
      default:
        break;
    }
  } else {
    // RISCV routine
    switch (InstructionBits() & kBaseOpcodeMask) {
      case LOAD:
        return kIType;
      case LOAD_FP:
        return kIType;
      case MISC_MEM:
        return kIType;
      case OP_IMM:
        return kIType;
      case AUIPC:
        return kUType;
      case OP_IMM_32:
        return kIType;
      case STORE:
        return kSType;
      case STORE_FP:
        return kSType;
      case AMO:
        return kRType;
      case OP:
        return kRType;
      case LUI:
        return kUType;
      case OP_32:
        return kRType;
      case MADD:
      case MSUB:
      case NMSUB:
      case NMADD:
        return kR4Type;
      case OP_FP:
        return kRType;
      case BRANCH:
        return kBType;
      case JALR:
        return kIType;
      case JAL:
        return kJType;
      case SYSTEM:
        return kIType;
    }
  }
  return kUnsupported;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64
