// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)

static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreWithVectorDescriptor::ValueRegister(),
          StoreWithVectorDescriptor::SlotRegister(),
          StoreWithVectorDescriptor::VectorRegister(),
          StoreWithVectorDescriptor::ReceiverRegister(),
          StoreWithVectorDescriptor::NameRegister());
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  __ TailCallRuntime(Runtime::kKeyedStoreIC_Miss);
}

void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Slow);
}

#undef __


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
  Address andi_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a andi at, rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(andi_instruction_address);
  return Assembler::IsAndImmediate(instr) &&
         Assembler::GetRt(instr) == static_cast<uint32_t>(zero_reg.code());
}


void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  Address andi_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a andi at, rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(andi_instruction_address);
  if (!(Assembler::IsAndImmediate(instr) &&
        Assembler::GetRt(instr) == static_cast<uint32_t>(zero_reg.code()))) {
    return;
  }

  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = Assembler::GetImmediate16(instr);
  delta += Assembler::GetRs(instr) * kImm16Mask;
  // If the delta is 0 the instruction is andi at, zero_reg, #0 which also
  // signals that nothing was inlined.
  if (delta == 0) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, andi=%p, delta=%d\n",
           static_cast<void*>(address),
           static_cast<void*>(andi_instruction_address), delta);
  }

  Address patch_address =
      andi_instruction_address - delta * Instruction::kInstrSize;
  Instr instr_at_patch = Assembler::instr_at(patch_address);
  // This is patching a conditional "jump if not smi/jump if smi" site.
  // Enabling by changing from
  //   andi at, rx, 0
  //   Branch <target>, eq, at, Operand(zero_reg)
  // to:
  //   andi at, rx, #kSmiTagMask
  //   Branch <target>, ne, at, Operand(zero_reg)
  // and vice-versa to be disabled again.
  CodePatcher patcher(isolate, patch_address, 2);
  Register reg = Register::from_code(Assembler::GetRs(instr_at_patch));
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(Assembler::IsAndImmediate(instr_at_patch));
    DCHECK_EQ(0u, Assembler::GetImmediate16(instr_at_patch));
    patcher.masm()->andi(at, reg, kSmiTagMask);
  } else {
    DCHECK_EQ(check, DISABLE_INLINED_SMI_CHECK);
    DCHECK(Assembler::IsAndImmediate(instr_at_patch));
    patcher.masm()->andi(at, reg, 0);
  }
  Instr branch_instr =
      Assembler::instr_at(patch_address + Instruction::kInstrSize);
  DCHECK(Assembler::IsBranch(branch_instr));

  uint32_t opcode = Assembler::GetOpcodeField(branch_instr);
  // Currently only the 'eq' and 'ne' cond values are supported and the simple
  // branch instructions and their r6 variants (with opcode being the branch
  // type). There are some special cases (see Assembler::IsBranch()) so
  // extending this would be tricky.
  DCHECK(opcode == BEQ ||    // BEQ
         opcode == BNE ||    // BNE
         opcode == POP10 ||  // BEQC
         opcode == POP30 ||  // BNEC
         opcode == POP66 ||  // BEQZC
         opcode == POP76);   // BNEZC
  switch (opcode) {
    case BEQ:
      opcode = BNE;  // change BEQ to BNE.
      break;
    case POP10:
      opcode = POP30;  // change BEQC to BNEC.
      break;
    case POP66:
      opcode = POP76;  // change BEQZC to BNEZC.
      break;
    case BNE:
      opcode = BEQ;  // change BNE to BEQ.
      break;
    case POP30:
      opcode = POP10;  // change BNEC to BEQC.
      break;
    case POP76:
      opcode = POP66;  // change BNEZC to BEQZC.
      break;
    default:
      UNIMPLEMENTED();
  }
  patcher.ChangeBranchCondition(branch_instr, opcode);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
