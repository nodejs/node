// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/ic/ic.h"
#include "src/codegen.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}

bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a CHI, nothing
  // was inlined.
  return (Instruction::S390OpcodeValue(cmp_instruction_address) == CHI);
}

//
// This code is paired with the JumpPatchSite class in full-codegen-s390.cc
//
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (Instruction::S390OpcodeValue(cmp_instruction_address) != CHI) {
    return;
  }

  if (Instruction::S390OpcodeValue(address) != BRASL) {
    return;
  }
  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = instr & 0x0000ffff;

  // If the delta is 0 the instruction is cmp r0, #0 which also signals that
  // nothing was inlined.
  if (delta == 0) {
    return;
  }

  if (FLAG_trace_ic) {
    LOG(isolate, PatchIC(address, cmp_instruction_address, delta));
  }

  // Expected sequence to enable by changing the following
  //   CR/CGR  Rx, Rx    // 2 / 4 bytes
  //   LR  R0, R0        // 2 bytes   // 31-bit only!
  //   BRC/BRCL          // 4 / 6 bytes
  // into
  //   TMLL    Rx, XXX   // 4 bytes
  //   BRC/BRCL          // 4 / 6 bytes
  // And vice versa to disable.

  // The following constant is the size of the CR/CGR + LR + LR
  const int kPatchAreaSizeNoBranch = 4;
  Address patch_address = cmp_instruction_address - delta;
  Address branch_address = patch_address + kPatchAreaSizeNoBranch;

  Instr instr_at_patch = Assembler::instr_at(patch_address);
  SixByteInstr branch_instr = Assembler::instr_at(branch_address);

  // This is patching a conditional "jump if not smi/jump if smi" site.
  size_t patch_size = 0;
  if (Instruction::S390OpcodeValue(branch_address) == BRC) {
    patch_size = kPatchAreaSizeNoBranch + 4;
  } else if (Instruction::S390OpcodeValue(branch_address) == BRCL) {
    patch_size = kPatchAreaSizeNoBranch + 6;
  } else {
    DCHECK(false);
  }
  CodePatcher patcher(isolate, patch_address, patch_size);
  Register reg;
  reg.reg_code = instr_at_patch & 0xf;
  if (check == ENABLE_INLINED_SMI_CHECK) {
    patcher.masm()->TestIfSmi(reg);
  } else {
    // Emit the NOP to ensure sufficient place for patching
    // (replaced by LR + NILL)
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    patcher.masm()->CmpP(reg, reg);
#ifndef V8_TARGET_ARCH_S390X
    patcher.masm()->nop();
#endif
  }

  Condition cc = al;
  if (Instruction::S390OpcodeValue(branch_address) == BRC) {
    cc = static_cast<Condition>((branch_instr & 0x00f00000) >> 20);
    DCHECK((cc == ne) || (cc == eq));
    cc = (cc == ne) ? eq : ne;
    patcher.masm()->brc(cc, Operand(branch_instr & 0xffff));
  } else if (Instruction::S390OpcodeValue(branch_address) == BRCL) {
    cc = static_cast<Condition>(
        (branch_instr & (static_cast<uint64_t>(0x00f0) << 32)) >> 36);
    DCHECK((cc == ne) || (cc == eq));
    cc = (cc == ne) ? eq : ne;
    patcher.masm()->brcl(cc, Operand(branch_instr & 0xffffffff));
  } else {
    DCHECK(false);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
