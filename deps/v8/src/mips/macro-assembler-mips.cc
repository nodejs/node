// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#include "v8.h"

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      unresolved_(0),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}



void MacroAssembler::Jump(Register target, Condition cond,
                          Register r1, const Operand& r2) {
  Jump(Operand(target), cond, r1, r2);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  Jump(Operand(target), cond, r1, r2);
}


void MacroAssembler::Jump(byte* target, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, r1, r2);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


void MacroAssembler::Call(Register target,
                          Condition cond, Register r1, const Operand& r2) {
  Call(Operand(target), cond, r1, r2);
}


void MacroAssembler::Call(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  Call(Operand(target), cond, r1, r2);
}


void MacroAssembler::Call(byte* target, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Call(reinterpret_cast<intptr_t>(target), rmode, cond, r1, r2);
}


void MacroAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register r1, const Operand& r2) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  Call(reinterpret_cast<intptr_t>(code.location()), rmode, cond, r1, r2);
}


void MacroAssembler::Ret(Condition cond, Register r1, const Operand& r2) {
  Jump(Operand(ra), cond, r1, r2);
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index) {
  lw(destination, MemOperand(s4, index << kPointerSizeLog2));
}

void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond,
                              Register src1, const Operand& src2) {
  Branch(NegateCondition(cond), 2, src1, src2);
  nop();
  lw(destination, MemOperand(s4, index << kPointerSizeLog2));
}


void MacroAssembler::RecordWrite(Register object, Register offset,
                                 Register scratch) {
  UNIMPLEMENTED_MIPS();
}


// ---------------------------------------------------------------------------
// Instruction macros

void MacroAssembler::Add(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    add(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      addi(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      add(rd, rs, at);
    }
  }
}


void MacroAssembler::Addu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    addu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      addiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      addu(rd, rs, at);
    }
  }
}


void MacroAssembler::Mul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mul(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    mul(rd, rs, at);
  }
}


void MacroAssembler::Mult(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mult(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    mult(rs, at);
  }
}


void MacroAssembler::Multu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    multu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    multu(rs, at);
  }
}


void MacroAssembler::Div(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    div(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    div(rs, at);
  }
}


void MacroAssembler::Divu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    divu(rs, at);
  }
}


void MacroAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    and_(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      andi(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      and_(rd, rs, at);
    }
  }
}


void MacroAssembler::Or(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    or_(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      ori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      or_(rd, rs, at);
    }
  }
}


void MacroAssembler::Xor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    xor_(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      xori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      xor_(rd, rs, at);
    }
  }
}


void MacroAssembler::Nor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    nor(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    nor(rd, rs, at);
  }
}


void MacroAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      slti(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      slt(rd, rs, at);
    }
  }
}


void MacroAssembler::Sltu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseAt(rt.rmode_)) {
      sltiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      sltu(rd, rs, at);
    }
  }
}


//------------Pseudo-instructions-------------

void MacroAssembler::movn(Register rd, Register rt) {
  addiu(at, zero_reg, -1);  // Fill at with ones.
  xor_(rd, rt, at);
}


// load wartd in a register
void MacroAssembler::li(Register rd, Operand j, bool gen2instr) {
  ASSERT(!j.is_reg());

  if (!MustUseAt(j.rmode_) && !gen2instr) {
    // Normal load of an immediate value which does not need Relocation Info.
    if (is_int16(j.imm32_)) {
      addiu(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & HIMask)) {
      ori(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & LOMask)) {
      lui(rd, (HIMask & j.imm32_) >> 16);
    } else {
      lui(rd, (HIMask & j.imm32_) >> 16);
      ori(rd, rd, (LOMask & j.imm32_));
    }
  } else if (MustUseAt(j.rmode_) || gen2instr) {
    if (MustUseAt(j.rmode_)) {
      RecordRelocInfo(j.rmode_, j.imm32_);
    }
    // We need always the same number of instructions as we may need to patch
    // this code to load another value which may need 2 instructions to load.
    if (is_int16(j.imm32_)) {
      nop();
      addiu(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & HIMask)) {
      nop();
      ori(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & LOMask)) {
      nop();
      lui(rd, (HIMask & j.imm32_) >> 16);
    } else {
      lui(rd, (HIMask & j.imm32_) >> 16);
      ori(rd, rd, (LOMask & j.imm32_));
    }
  }
}


// Exception-generating instructions and debugging support
void MacroAssembler::stop(const char* msg) {
  // TO_UPGRADE: Just a break for now. Maybe we could upgrade it.
  // We use the 0x54321 value to be able to find it easily when reading memory.
  break_(0x54321);
}


void MacroAssembler::MultiPush(RegList regs) {
  int16_t NumSaved = 0;
  int16_t NumToPush = NumberOfBitsSet(regs);

  addiu(sp, sp, -4 * NumToPush);
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      sw(ToRegister(i), MemOperand(sp, 4 * (NumToPush - ++NumSaved)));
    }
  }
}


void MacroAssembler::MultiPushReversed(RegList regs) {
  int16_t NumSaved = 0;
  int16_t NumToPush = NumberOfBitsSet(regs);

  addiu(sp, sp, -4 * NumToPush);
  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      sw(ToRegister(i), MemOperand(sp, 4 * (NumToPush - ++NumSaved)));
    }
  }
}


void MacroAssembler::MultiPop(RegList regs) {
  int16_t NumSaved = 0;

  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, 4 * (NumSaved++)));
    }
  }
  addiu(sp, sp, 4 * NumSaved);
}


void MacroAssembler::MultiPopReversed(RegList regs) {
  int16_t NumSaved = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, 4 * (NumSaved++)));
    }
  }
  addiu(sp, sp, 4 * NumSaved);
}


// Emulated condtional branches do not emit a nop in the branch delay slot.

// Trashes the at register if no scratch register is provided.
void MacroAssembler::Branch(Condition cond, int16_t offset, Register rs,
                            const Operand& rt, Register scratch) {
  Register r2 = no_reg;
  if (rt.is_reg()) {
    // We don't want any other register but scratch clobbered.
    ASSERT(!scratch.is(rs) && !scratch.is(rt.rm_));
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    // We don't want any other register but scratch clobbered.
    ASSERT(!scratch.is(rs));
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      b(offset);
      break;
    case eq:
      beq(rs, r2, offset);
      break;
    case ne:
      bne(rs, r2, offset);
      break;

      // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      bne(scratch, zero_reg, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      beq(scratch, zero_reg, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      bne(scratch, zero_reg, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      beq(scratch, zero_reg, offset);
      break;

      // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      bne(scratch, zero_reg, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      beq(scratch, zero_reg, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      bne(scratch, zero_reg, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      beq(scratch, zero_reg, offset);
      break;

    default:
      UNREACHABLE();
  }
}


void MacroAssembler::Branch(Condition cond,  Label* L, Register rs,
                            const Operand& rt, Register scratch) {
  Register r2 = no_reg;
  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  // We use branch_offset as an argument for the branch instructions to be sure
  // it is called just before generating the branch instruction, as needed.

  switch (cond) {
    case cc_always:
      b(shifted_branch_offset(L, false));
      break;
    case eq:
      beq(rs, r2, shifted_branch_offset(L, false));
      break;
    case ne:
      bne(rs, r2, shifted_branch_offset(L, false));
      break;

    // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      bne(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      beq(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case less:
      slt(scratch, rs, r2);
      bne(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case less_equal:
      slt(scratch, r2, rs);
      beq(scratch, zero_reg, shifted_branch_offset(L, false));
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      bne(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      beq(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case Uless:
      sltu(scratch, rs, r2);
      bne(scratch, zero_reg, shifted_branch_offset(L, false));
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      beq(scratch, zero_reg, shifted_branch_offset(L, false));
      break;

    default:
      UNREACHABLE();
  }
}


// Trashes the at register if no scratch register is provided.
// We need to use a bgezal or bltzal, but they can't be used directly with the
// slt instructions. We could use sub or add instead but we would miss overflow
// cases, so we keep slt and add an intermediate third instruction.
void MacroAssembler::BranchAndLink(Condition cond, int16_t offset, Register rs,
                                   const Operand& rt, Register scratch) {
  Register r2 = no_reg;
  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      bal(offset);
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      bal(offset);
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      bal(offset);
      break;

    // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }
}


void MacroAssembler::BranchAndLink(Condition cond, Label* L, Register rs,
                                   const Operand& rt, Register scratch) {
  Register r2 = no_reg;
  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      bal(shifted_branch_offset(L, false));
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      bal(shifted_branch_offset(L, false));
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      bal(shifted_branch_offset(L, false));
      break;

    // Signed comparison
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, shifted_branch_offset(L, false));
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, shifted_branch_offset(L, false));
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, shifted_branch_offset(L, false));
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, shifted_branch_offset(L, false));
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, shifted_branch_offset(L, false));
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, shifted_branch_offset(L, false));
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, shifted_branch_offset(L, false));
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, shifted_branch_offset(L, false));
      break;

    default:
      UNREACHABLE();
  }
}


void MacroAssembler::Jump(const Operand& target,
                          Condition cond, Register rs, const Operand& rt) {
  if (target.is_reg()) {
    if (cond == cc_always) {
      jr(target.rm());
    } else {
      Branch(NegateCondition(cond), 2, rs, rt);
      nop();
      jr(target.rm());
    }
  } else {    // !target.is_reg()
    if (!MustUseAt(target.rmode_)) {
      if (cond == cc_always) {
        j(target.imm32_);
      } else {
        Branch(NegateCondition(cond), 2, rs, rt);
        nop();
        j(target.imm32_);  // will generate only one instruction.
      }
    } else {  // MustUseAt(target)
      li(at, rt);
      if (cond == cc_always) {
        jr(at);
      } else {
        Branch(NegateCondition(cond), 2, rs, rt);
        nop();
        jr(at);  // will generate only one instruction.
      }
    }
  }
}


void MacroAssembler::Call(const Operand& target,
                          Condition cond, Register rs, const Operand& rt) {
  if (target.is_reg()) {
    if (cond == cc_always) {
      jalr(target.rm());
    } else {
      Branch(NegateCondition(cond), 2, rs, rt);
      nop();
      jalr(target.rm());
    }
  } else {    // !target.is_reg()
    if (!MustUseAt(target.rmode_)) {
      if (cond == cc_always) {
        jal(target.imm32_);
      } else {
        Branch(NegateCondition(cond), 2, rs, rt);
        nop();
        jal(target.imm32_);  // will generate only one instruction.
      }
    } else {  // MustUseAt(target)
      li(at, rt);
      if (cond == cc_always) {
        jalr(at);
      } else {
        Branch(NegateCondition(cond), 2, rs, rt);
        nop();
        jalr(at);  // will generate only one instruction.
      }
    }
  }
}

void MacroAssembler::StackLimitCheck(Label* on_stack_overflow) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::Drop(int count, Condition cond) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::Call(Label* target) {
  UNIMPLEMENTED_MIPS();
}


#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

  void MacroAssembler::DebugBreak() {
    UNIMPLEMENTED_MIPS();
  }
#endif


// ---------------------------------------------------------------------------
// Exception handling

void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::PopTryHandler() {
  UNIMPLEMENTED_MIPS();
}



// ---------------------------------------------------------------------------
// Activation frames

void MacroAssembler::CallStub(CodeStub* stub, Condition cond,
                              Register r1, const Operand& r2) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::StubReturn(int argc) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid), num_arguments, result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  UNIMPLEMENTED_MIPS();
}


Handle<Code> MacroAssembler::ResolveBuiltin(Builtins::JavaScript id,
                                            bool* resolved) {
  UNIMPLEMENTED_MIPS();
  return Handle<Code>(reinterpret_cast<Code*>(NULL));   // UNIMPLEMENTED RETURN
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeJSFlags flags) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  UNIMPLEMENTED_MIPS();
}



void MacroAssembler::Assert(Condition cc, const char* msg,
                            Register rs, Operand rt) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::Check(Condition cc, const char* msg,
                           Register rs, Operand rt) {
  UNIMPLEMENTED_MIPS();
}


void MacroAssembler::Abort(const char* msg) {
  UNIMPLEMENTED_MIPS();
}

} }  // namespace v8::internal

