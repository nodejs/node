// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_A64

#include "bootstrapper.h"
#include "codegen.h"
#include "cpu-profiler.h"
#include "debug.h"
#include "isolate-inl.h"
#include "runtime.h"

namespace v8 {
namespace internal {

// Define a fake double underscore to use with the ASM_UNIMPLEMENTED macros.
#define __


MacroAssembler::MacroAssembler(Isolate* arg_isolate,
                               byte * buffer,
                               unsigned buffer_size)
    : Assembler(arg_isolate, buffer, buffer_size),
      generating_stub_(false),
#if DEBUG
      allow_macro_instructions_(true),
#endif
      has_frame_(false),
      use_real_aborts_(true),
      sp_(jssp), tmp0_(ip0), tmp1_(ip1), fptmp0_(fp_scratch) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


void MacroAssembler::LogicalMacro(const Register& rd,
                                  const Register& rn,
                                  const Operand& operand,
                                  LogicalOp op) {
  if (operand.NeedsRelocation()) {
    LoadRelocated(Tmp0(), operand);
    Logical(rd, rn, Tmp0(), op);

  } else if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    unsigned reg_size = rd.SizeInBits();
    ASSERT(rd.Is64Bits() || is_uint32(immediate));

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = ~immediate;
      if (rd.Is32Bits()) {
        immediate &= kWRegMask;
      }
    }

    // Special cases for all set or all clear immediates.
    if (immediate == 0) {
      switch (op) {
        case AND:
          Mov(rd, 0);
          return;
        case ORR:  // Fall through.
        case EOR:
          Mov(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    } else if ((rd.Is64Bits() && (immediate == -1L)) ||
               (rd.Is32Bits() && (immediate == 0xffffffffL))) {
      switch (op) {
        case AND:
          Mov(rd, rn);
          return;
        case ORR:
          Mov(rd, immediate);
          return;
        case EOR:
          Mvn(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // Immediate can't be encoded: synthesize using move immediate.
      Register temp = AppropriateTempFor(rn);
      Mov(temp, immediate);
      if (rd.Is(csp)) {
        // If rd is the stack pointer we cannot use it as the destination
        // register so we use the temp register as an intermediate again.
        Logical(temp, rn, temp, op);
        Mov(csp, temp);
      } else {
        Logical(rd, rn, temp, op);
      }
    }

  } else if (operand.IsExtendedRegister()) {
    ASSERT(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports shift <= 4. We want to support exactly the
    // same modes here.
    ASSERT(operand.shift_amount() <= 4);
    ASSERT(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    Logical(rd, rn, temp, op);

  } else {
    // The operand can be encoded in the instruction.
    ASSERT(operand.IsShiftedRegister());
    Logical(rd, rn, operand, op);
  }
}


void MacroAssembler::Mov(const Register& rd, uint64_t imm) {
  ASSERT(allow_macro_instructions_);
  ASSERT(is_uint32(imm) || is_int32(imm) || rd.Is64Bits());
  ASSERT(!rd.IsZero());

  // TODO(all) extend to support more immediates.
  //
  // Immediates on Aarch64 can be produced using an initial value, and zero to
  // three move keep operations.
  //
  // Initial values can be generated with:
  //  1. 64-bit move zero (movz).
  //  2. 32-bit move inverted (movn).
  //  3. 64-bit move inverted.
  //  4. 32-bit orr immediate.
  //  5. 64-bit orr immediate.
  // Move-keep may then be used to modify each of the 16-bit half-words.
  //
  // The code below supports all five initial value generators, and
  // applying move-keep operations to move-zero and move-inverted initial
  // values.

  unsigned reg_size = rd.SizeInBits();
  unsigned n, imm_s, imm_r;
  if (IsImmMovz(imm, reg_size) && !rd.IsSP()) {
    // Immediate can be represented in a move zero instruction. Movz can't
    // write to the stack pointer.
    movz(rd, imm);
  } else if (IsImmMovn(imm, reg_size) && !rd.IsSP()) {
    // Immediate can be represented in a move inverted instruction. Movn can't
    // write to the stack pointer.
    movn(rd, rd.Is64Bits() ? ~imm : (~imm & kWRegMask));
  } else if (IsImmLogical(imm, reg_size, &n, &imm_s, &imm_r)) {
    // Immediate can be represented in a logical orr instruction.
    LogicalImmediate(rd, AppropriateZeroRegFor(rd), n, imm_s, imm_r, ORR);
  } else {
    // Generic immediate case. Imm will be represented by
    //   [imm3, imm2, imm1, imm0], where each imm is 16 bits.
    // A move-zero or move-inverted is generated for the first non-zero or
    // non-0xffff immX, and a move-keep for subsequent non-zero immX.

    uint64_t ignored_halfword = 0;
    bool invert_move = false;
    // If the number of 0xffff halfwords is greater than the number of 0x0000
    // halfwords, it's more efficient to use move-inverted.
    if (CountClearHalfWords(~imm, reg_size) >
        CountClearHalfWords(imm, reg_size)) {
      ignored_halfword = 0xffffL;
      invert_move = true;
    }

    // Mov instructions can't move value into the stack pointer, so set up a
    // temporary register, if needed.
    Register temp = rd.IsSP() ? AppropriateTempFor(rd) : rd;

    // Iterate through the halfwords. Use movn/movz for the first non-ignored
    // halfword, and movk for subsequent halfwords.
    ASSERT((reg_size % 16) == 0);
    bool first_mov_done = false;
    for (unsigned i = 0; i < (rd.SizeInBits() / 16); i++) {
      uint64_t imm16 = (imm >> (16 * i)) & 0xffffL;
      if (imm16 != ignored_halfword) {
        if (!first_mov_done) {
          if (invert_move) {
            movn(temp, (~imm16) & 0xffffL, 16 * i);
          } else {
            movz(temp, imm16, 16 * i);
          }
          first_mov_done = true;
        } else {
          // Construct a wider constant.
          movk(temp, imm16, 16 * i);
        }
      }
    }
    ASSERT(first_mov_done);

    // Move the temporary if the original destination register was the stack
    // pointer.
    if (rd.IsSP()) {
      mov(rd, temp);
    }
  }
}


void MacroAssembler::Mov(const Register& rd,
                         const Operand& operand,
                         DiscardMoveMode discard_mode) {
  ASSERT(allow_macro_instructions_);
  ASSERT(!rd.IsZero());
  // Provide a swap register for instructions that need to write into the
  // system stack pointer (and can't do this inherently).
  Register dst = (rd.Is(csp)) ? (Tmp1()) : (rd);

  if (operand.NeedsRelocation()) {
    LoadRelocated(dst, operand);

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(dst, operand.immediate());

  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Emit a shift instruction if moving a shifted register. This operation
    // could also be achieved using an orr instruction (like orn used by Mvn),
    // but using a shift instruction makes the disassembly clearer.
    EmitShift(dst, operand.reg(), operand.shift(), operand.shift_amount());

  } else if (operand.IsExtendedRegister()) {
    // Emit an extend instruction if moving an extended register. This handles
    // extend with post-shift operations, too.
    EmitExtendShift(dst, operand.reg(), operand.extend(),
                    operand.shift_amount());

  } else {
    // Otherwise, emit a register move only if the registers are distinct, or
    // if they are not X registers.
    //
    // Note that mov(w0, w0) is not a no-op because it clears the top word of
    // x0. A flag is provided (kDiscardForSameWReg) if a move between the same W
    // registers is not required to clear the top word of the X register. In
    // this case, the instruction is discarded.
    //
    // If csp is an operand, add #0 is emitted, otherwise, orr #0.
    if (!rd.Is(operand.reg()) || (rd.Is32Bits() &&
                                  (discard_mode == kDontDiscardForSameWReg))) {
      Assembler::mov(rd, operand.reg());
    }
    // This case can handle writes into the system stack pointer directly.
    dst = rd;
  }

  // Copy the result to the system stack pointer.
  if (!dst.Is(rd)) {
    ASSERT(rd.IsZero());
    ASSERT(dst.Is(Tmp1()));
    Assembler::mov(rd, dst);
  }
}


void MacroAssembler::Mvn(const Register& rd, const Operand& operand) {
  ASSERT(allow_macro_instructions_);

  if (operand.NeedsRelocation()) {
    LoadRelocated(Tmp0(), operand);
    Mvn(rd, Tmp0());

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(rd, ~operand.immediate());

  } else if (operand.IsExtendedRegister()) {
    // Emit two instructions for the extend case. This differs from Mov, as
    // the extend and invert can't be achieved in one instruction.
    Register temp = AppropriateTempFor(rd, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    mvn(rd, temp);

  } else {
    // Otherwise, emit a register move only if the registers are distinct.
    // If the jssp is an operand, add #0 is emitted, otherwise, orr #0.
    mvn(rd, operand);
  }
}


unsigned MacroAssembler::CountClearHalfWords(uint64_t imm, unsigned reg_size) {
  ASSERT((reg_size % 8) == 0);
  int count = 0;
  for (unsigned i = 0; i < (reg_size / 16); i++) {
    if ((imm & 0xffff) == 0) {
      count++;
    }
    imm >>= 16;
  }
  return count;
}


// The movz instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits clear, eg. 0x00001234, 0x0000123400000000.
bool MacroAssembler::IsImmMovz(uint64_t imm, unsigned reg_size) {
  ASSERT((reg_size == kXRegSize) || (reg_size == kWRegSize));
  return CountClearHalfWords(imm, reg_size) >= ((reg_size / 16) - 1);
}


// The movn instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits set, eg. 0xffff1234, 0xffff1234ffffffff.
bool MacroAssembler::IsImmMovn(uint64_t imm, unsigned reg_size) {
  return IsImmMovz(~imm, reg_size);
}


void MacroAssembler::ConditionalCompareMacro(const Register& rn,
                                             const Operand& operand,
                                             StatusFlags nzcv,
                                             Condition cond,
                                             ConditionalCompareOp op) {
  ASSERT((cond != al) && (cond != nv));
  if (operand.NeedsRelocation()) {
    LoadRelocated(Tmp0(), operand);
    ConditionalCompareMacro(rn, Tmp0(), nzcv, cond, op);

  } else if ((operand.IsShiftedRegister() && (operand.shift_amount() == 0)) ||
      (operand.IsImmediate() && IsImmConditionalCompare(operand.immediate()))) {
    // The immediate can be encoded in the instruction, or the operand is an
    // unshifted register: call the assembler.
    ConditionalCompare(rn, operand, nzcv, cond, op);

  } else {
    // The operand isn't directly supported by the instruction: perform the
    // operation on a temporary register.
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    ConditionalCompare(rn, temp, nzcv, cond, op);
  }
}


void MacroAssembler::Csel(const Register& rd,
                          const Register& rn,
                          const Operand& operand,
                          Condition cond) {
  ASSERT(allow_macro_instructions_);
  ASSERT(!rd.IsZero());
  ASSERT((cond != al) && (cond != nv));
  if (operand.IsImmediate()) {
    // Immediate argument. Handle special cases of 0, 1 and -1 using zero
    // register.
    int64_t imm = operand.immediate();
    Register zr = AppropriateZeroRegFor(rn);
    if (imm == 0) {
      csel(rd, rn, zr, cond);
    } else if (imm == 1) {
      csinc(rd, rn, zr, cond);
    } else if (imm == -1) {
      csinv(rd, rn, zr, cond);
    } else {
      Register temp = AppropriateTempFor(rn);
      Mov(temp, operand.immediate());
      csel(rd, rn, temp, cond);
    }
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() == 0)) {
    // Unshifted register argument.
    csel(rd, rn, operand.reg(), cond);
  } else {
    // All other arguments.
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    csel(rd, rn, temp, cond);
  }
}


void MacroAssembler::AddSubMacro(const Register& rd,
                                 const Register& rn,
                                 const Operand& operand,
                                 FlagsUpdate S,
                                 AddSubOp op) {
  if (operand.IsZero() && rd.Is(rn) && rd.Is64Bits() && rn.Is64Bits() &&
      !operand.NeedsRelocation() && (S == LeaveFlags)) {
    // The instruction would be a nop. Avoid generating useless code.
    return;
  }

  if (operand.NeedsRelocation()) {
    LoadRelocated(Tmp0(), operand);
    AddSubMacro(rd, rn, Tmp0(), S, op);
  } else if ((operand.IsImmediate() && !IsImmAddSub(operand.immediate())) ||
             (rn.IsZero() && !operand.IsShiftedRegister())                ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    AddSub(rd, rn, temp, S, op);
  } else {
    AddSub(rd, rn, operand, S, op);
  }
}


void MacroAssembler::AddSubWithCarryMacro(const Register& rd,
                                          const Register& rn,
                                          const Operand& operand,
                                          FlagsUpdate S,
                                          AddSubWithCarryOp op) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());

  if (operand.NeedsRelocation()) {
    LoadRelocated(Tmp0(), operand);
    AddSubWithCarryMacro(rd, rn, Tmp0(), S, op);

  } else if (operand.IsImmediate() ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    // Add/sub with carry (immediate or ROR shifted register.)
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    AddSubWithCarry(rd, rn, temp, S, op);
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Add/sub with carry (shifted register).
    ASSERT(operand.reg().SizeInBits() == rd.SizeInBits());
    ASSERT(operand.shift() != ROR);
    ASSERT(is_uintn(operand.shift_amount(),
          rd.SizeInBits() == kXRegSize ? kXRegSizeLog2 : kWRegSizeLog2));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitShift(temp, operand.reg(), operand.shift(), operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsExtendedRegister()) {
    // Add/sub with carry (extended register).
    ASSERT(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports a shift <= 4. We want to support exactly the
    // same modes.
    ASSERT(operand.shift_amount() <= 4);
    ASSERT(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else {
    // The addressing mode is directly supported by the instruction.
    AddSubWithCarry(rd, rn, operand, S, op);
  }
}


void MacroAssembler::LoadStoreMacro(const CPURegister& rt,
                                    const MemOperand& addr,
                                    LoadStoreOp op) {
  int64_t offset = addr.offset();
  LSDataSize size = CalcLSDataSize(op);

  // Check if an immediate offset fits in the immediate field of the
  // appropriate instruction. If not, emit two instructions to perform
  // the operation.
  if (addr.IsImmediateOffset() && !IsImmLSScaled(offset, size) &&
      !IsImmLSUnscaled(offset)) {
    // Immediate offset that can't be encoded using unsigned or unscaled
    // addressing modes.
    Register temp = AppropriateTempFor(addr.base());
    Mov(temp, addr.offset());
    LoadStore(rt, MemOperand(addr.base(), temp), op);
  } else if (addr.IsPostIndex() && !IsImmLSUnscaled(offset)) {
    // Post-index beyond unscaled addressing range.
    LoadStore(rt, MemOperand(addr.base()), op);
    add(addr.base(), addr.base(), offset);
  } else if (addr.IsPreIndex() && !IsImmLSUnscaled(offset)) {
    // Pre-index beyond unscaled addressing range.
    add(addr.base(), addr.base(), offset);
    LoadStore(rt, MemOperand(addr.base()), op);
  } else {
    // Encodable in one load/store instruction.
    LoadStore(rt, addr, op);
  }
}


void MacroAssembler::Load(const Register& rt,
                          const MemOperand& addr,
                          Representation r) {
  ASSERT(!r.IsDouble());

  if (r.IsInteger8()) {
    Ldrsb(rt, addr);
  } else if (r.IsUInteger8()) {
    Ldrb(rt, addr);
  } else if (r.IsInteger16()) {
    Ldrsh(rt, addr);
  } else if (r.IsUInteger16()) {
    Ldrh(rt, addr);
  } else if (r.IsInteger32()) {
    Ldr(rt.W(), addr);
  } else {
    ASSERT(rt.Is64Bits());
    Ldr(rt, addr);
  }
}


void MacroAssembler::Store(const Register& rt,
                           const MemOperand& addr,
                           Representation r) {
  ASSERT(!r.IsDouble());

  if (r.IsInteger8() || r.IsUInteger8()) {
    Strb(rt, addr);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    Strh(rt, addr);
  } else if (r.IsInteger32()) {
    Str(rt.W(), addr);
  } else {
    ASSERT(rt.Is64Bits());
    Str(rt, addr);
  }
}


bool MacroAssembler::ShouldEmitVeneer(int max_reachable_pc, int margin) {
  // Account for the branch around the veneers and the guard.
  int protection_offset = 2 * kInstructionSize;
  return pc_offset() > max_reachable_pc - margin - protection_offset -
    static_cast<int>(unresolved_branches_.size() * kMaxVeneerCodeSize);
}


void MacroAssembler::EmitVeneers(bool need_protection) {
  RecordComment("[ Veneers");

  Label end;
  if (need_protection) {
    B(&end);
  }

  EmitVeneersGuard();

  {
    InstructionAccurateScope scope(this);
    Label size_check;

    std::multimap<int, FarBranchInfo>::iterator it, it_to_delete;

    it = unresolved_branches_.begin();
    while (it != unresolved_branches_.end()) {
      if (ShouldEmitVeneer(it->first)) {
        Instruction* branch = InstructionAt(it->second.pc_offset_);
        Label* label = it->second.label_;

#ifdef DEBUG
        __ bind(&size_check);
#endif
        // Patch the branch to point to the current position, and emit a branch
        // to the label.
        Instruction* veneer = reinterpret_cast<Instruction*>(pc_);
        RemoveBranchFromLabelLinkChain(branch, label, veneer);
        branch->SetImmPCOffsetTarget(veneer);
        b(label);
#ifdef DEBUG
        ASSERT(SizeOfCodeGeneratedSince(&size_check) <=
               static_cast<uint64_t>(kMaxVeneerCodeSize));
        size_check.Unuse();
#endif

        it_to_delete = it++;
        unresolved_branches_.erase(it_to_delete);
      } else {
        ++it;
      }
    }
  }

  Bind(&end);

  RecordComment("]");
}


void MacroAssembler::EmitVeneersGuard() {
  if (emit_debug_code()) {
    Unreachable();
  }
}


void MacroAssembler::CheckVeneers(bool need_protection) {
  if (unresolved_branches_.empty()) {
    return;
  }

  CHECK(pc_offset() < unresolved_branches_first_limit());
  int margin = kVeneerDistanceMargin;
  if (!need_protection) {
    // Prefer emitting veneers protected by an existing instruction.
    // The 4 divisor is a finger in the air guess. With a default margin of 2KB,
    // that leaves 512B = 128 instructions of extra margin to avoid requiring a
    // protective branch.
    margin += margin / 4;
  }
  if (ShouldEmitVeneer(unresolved_branches_first_limit(), margin)) {
    EmitVeneers(need_protection);
  }
}


bool MacroAssembler::NeedExtraInstructionsOrRegisterBranch(
    Label *label, ImmBranchType b_type) {
  bool need_longer_range = false;
  // There are two situations in which we care about the offset being out of
  // range:
  //  - The label is bound but too far away.
  //  - The label is not bound but linked, and the previous branch
  //    instruction in the chain is too far away.
  if (label->is_bound() || label->is_linked()) {
    need_longer_range =
      !Instruction::IsValidImmPCOffset(b_type, label->pos() - pc_offset());
  }
  if (!need_longer_range && !label->is_bound()) {
    int max_reachable_pc = pc_offset() + Instruction::ImmBranchRange(b_type);
    unresolved_branches_.insert(
        std::pair<int, FarBranchInfo>(max_reachable_pc,
                                      FarBranchInfo(pc_offset(), label)));
  }
  return need_longer_range;
}


void MacroAssembler::B(Label* label, Condition cond) {
  ASSERT(allow_macro_instructions_);
  ASSERT((cond != al) && (cond != nv));

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CondBranchType);

  if (need_extra_instructions) {
    b(&done, InvertCondition(cond));
    b(label);
  } else {
    b(label, cond);
  }
  CheckVeneers(!need_extra_instructions);
  bind(&done);
}


void MacroAssembler::Tbnz(const Register& rt, unsigned bit_pos, Label* label) {
  ASSERT(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbz(rt, bit_pos, &done);
    b(label);
  } else {
    tbnz(rt, bit_pos, label);
  }
  CheckVeneers(!need_extra_instructions);
  bind(&done);
}


void MacroAssembler::Tbz(const Register& rt, unsigned bit_pos, Label* label) {
  ASSERT(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbnz(rt, bit_pos, &done);
    b(label);
  } else {
    tbz(rt, bit_pos, label);
  }
  CheckVeneers(!need_extra_instructions);
  bind(&done);
}


void MacroAssembler::Cbnz(const Register& rt, Label* label) {
  ASSERT(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbz(rt, &done);
    b(label);
  } else {
    cbnz(rt, label);
  }
  CheckVeneers(!need_extra_instructions);
  bind(&done);
}


void MacroAssembler::Cbz(const Register& rt, Label* label) {
  ASSERT(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbnz(rt, &done);
    b(label);
  } else {
    cbz(rt, label);
  }
  CheckVeneers(!need_extra_instructions);
  bind(&done);
}


// Pseudo-instructions.


void MacroAssembler::Abs(const Register& rd, const Register& rm,
                         Label* is_not_representable,
                         Label* is_representable) {
  ASSERT(allow_macro_instructions_);
  ASSERT(AreSameSizeAndType(rd, rm));

  Cmp(rm, 1);
  Cneg(rd, rm, lt);

  // If the comparison sets the v flag, the input was the smallest value
  // representable by rm, and the mathematical result of abs(rm) is not
  // representable using two's complement.
  if ((is_not_representable != NULL) && (is_representable != NULL)) {
    B(is_not_representable, vs);
    B(is_representable);
  } else if (is_not_representable != NULL) {
    B(is_not_representable, vs);
  } else if (is_representable != NULL) {
    B(is_representable, vc);
  }
}


// Abstracted stack operations.


void MacroAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3) {
  ASSERT(AreSameSizeAndType(src0, src1, src2, src3));
  ASSERT(src0.IsValid());

  int count = 1 + src1.IsValid() + src2.IsValid() + src3.IsValid();
  int size = src0.SizeInBytes();

  PrepareForPush(count, size);
  PushHelper(count, size, src0, src1, src2, src3);
}


void MacroAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  ASSERT(!AreAliased(dst0, dst1, dst2, dst3));
  ASSERT(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  ASSERT(dst0.IsValid());

  int count = 1 + dst1.IsValid() + dst2.IsValid() + dst3.IsValid();
  int size = dst0.SizeInBytes();

  PrepareForPop(count, size);
  PopHelper(count, size, dst0, dst1, dst2, dst3);

  if (!csp.Is(StackPointer()) && emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    Mov(csp, StackPointer());
  }
}


void MacroAssembler::PushCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PrepareForPush(registers.Count(), size);
  // Push up to four registers at a time because if the current stack pointer is
  // csp and reg_size is 32, registers must be pushed in blocks of four in order
  // to maintain the 16-byte alignment for csp.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& src0 = registers.PopHighestIndex();
    const CPURegister& src1 = registers.PopHighestIndex();
    const CPURegister& src2 = registers.PopHighestIndex();
    const CPURegister& src3 = registers.PopHighestIndex();
    int count = count_before - registers.Count();
    PushHelper(count, size, src0, src1, src2, src3);
  }
}


void MacroAssembler::PopCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PrepareForPop(registers.Count(), size);
  // Pop up to four registers at a time because if the current stack pointer is
  // csp and reg_size is 32, registers must be pushed in blocks of four in
  // order to maintain the 16-byte alignment for csp.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    const CPURegister& dst2 = registers.PopLowestIndex();
    const CPURegister& dst3 = registers.PopLowestIndex();
    int count = count_before - registers.Count();
    PopHelper(count, size, dst0, dst1, dst2, dst3);
  }

  if (!csp.Is(StackPointer()) && emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    Mov(csp, StackPointer());
  }
}


void MacroAssembler::PushMultipleTimes(int count, Register src) {
  int size = src.SizeInBytes();

  PrepareForPush(count, size);

  if (FLAG_optimize_for_size && count > 8) {
    Label loop;
    __ Mov(Tmp0(), count / 2);
    __ Bind(&loop);
    PushHelper(2, size, src, src, NoReg, NoReg);
    __ Subs(Tmp0(), Tmp0(), 1);
    __ B(ne, &loop);

    count %= 2;
  }

  // Push up to four registers at a time if possible because if the current
  // stack pointer is csp and the register size is 32, registers must be pushed
  // in blocks of four in order to maintain the 16-byte alignment for csp.
  while (count >= 4) {
    PushHelper(4, size, src, src, src, src);
    count -= 4;
  }
  if (count >= 2) {
    PushHelper(2, size, src, src, NoReg, NoReg);
    count -= 2;
  }
  if (count == 1) {
    PushHelper(1, size, src, NoReg, NoReg, NoReg);
    count -= 1;
  }
  ASSERT(count == 0);
}


void MacroAssembler::PushHelper(int count, int size,
                                const CPURegister& src0,
                                const CPURegister& src1,
                                const CPURegister& src2,
                                const CPURegister& src3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  ASSERT(AreSameSizeAndType(src0, src1, src2, src3));
  ASSERT(size == src0.SizeInBytes());

  // When pushing multiple registers, the store order is chosen such that
  // Push(a, b) is equivalent to Push(a) followed by Push(b).
  switch (count) {
    case 1:
      ASSERT(src1.IsNone() && src2.IsNone() && src3.IsNone());
      str(src0, MemOperand(StackPointer(), -1 * size, PreIndex));
      break;
    case 2:
      ASSERT(src2.IsNone() && src3.IsNone());
      stp(src1, src0, MemOperand(StackPointer(), -2 * size, PreIndex));
      break;
    case 3:
      ASSERT(src3.IsNone());
      stp(src2, src1, MemOperand(StackPointer(), -3 * size, PreIndex));
      str(src0, MemOperand(StackPointer(), 2 * size));
      break;
    case 4:
      // Skip over 4 * size, then fill in the gap. This allows four W registers
      // to be pushed using csp, whilst maintaining 16-byte alignment for csp
      // at all times.
      stp(src3, src2, MemOperand(StackPointer(), -4 * size, PreIndex));
      stp(src1, src0, MemOperand(StackPointer(), 2 * size));
      break;
    default:
      UNREACHABLE();
  }
}


void MacroAssembler::PopHelper(int count, int size,
                               const CPURegister& dst0,
                               const CPURegister& dst1,
                               const CPURegister& dst2,
                               const CPURegister& dst3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  ASSERT(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  ASSERT(size == dst0.SizeInBytes());

  // When popping multiple registers, the load order is chosen such that
  // Pop(a, b) is equivalent to Pop(a) followed by Pop(b).
  switch (count) {
    case 1:
      ASSERT(dst1.IsNone() && dst2.IsNone() && dst3.IsNone());
      ldr(dst0, MemOperand(StackPointer(), 1 * size, PostIndex));
      break;
    case 2:
      ASSERT(dst2.IsNone() && dst3.IsNone());
      ldp(dst0, dst1, MemOperand(StackPointer(), 2 * size, PostIndex));
      break;
    case 3:
      ASSERT(dst3.IsNone());
      ldr(dst2, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 3 * size, PostIndex));
      break;
    case 4:
      // Load the higher addresses first, then load the lower addresses and
      // skip the whole block in the second instruction. This allows four W
      // registers to be popped using csp, whilst maintaining 16-byte alignment
      // for csp at all times.
      ldp(dst2, dst3, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 4 * size, PostIndex));
      break;
    default:
      UNREACHABLE();
  }
}


void MacroAssembler::PrepareForPush(int count, int size) {
  // TODO(jbramley): Use AssertStackConsistency here, if possible. See the
  // AssertStackConsistency for details of why we can't at the moment.
  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    ASSERT((count * size) % 16 == 0);
  } else {
    // Even if the current stack pointer is not the system stack pointer (csp),
    // the system stack pointer will still be modified in order to comply with
    // ABI rules about accessing memory below the system stack pointer.
    BumpSystemStackPointer(count * size);
  }
}


void MacroAssembler::PrepareForPop(int count, int size) {
  AssertStackConsistency();
  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    ASSERT((count * size) % 16 == 0);
  }
}


void MacroAssembler::Poke(const CPURegister& src, const Operand& offset) {
  if (offset.IsImmediate()) {
    ASSERT(offset.immediate() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Str(src, MemOperand(StackPointer(), offset));
}


void MacroAssembler::Peek(const CPURegister& dst, const Operand& offset) {
  if (offset.IsImmediate()) {
    ASSERT(offset.immediate() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Ldr(dst, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PokePair(const CPURegister& src1,
                              const CPURegister& src2,
                              int offset) {
  ASSERT(AreSameSizeAndType(src1, src2));
  ASSERT((offset >= 0) && ((offset % src1.SizeInBytes()) == 0));
  Stp(src1, src2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PeekPair(const CPURegister& dst1,
                              const CPURegister& dst2,
                              int offset) {
  ASSERT(AreSameSizeAndType(dst1, dst2));
  ASSERT((offset >= 0) && ((offset % dst1.SizeInBytes()) == 0));
  Ldp(dst1, dst2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PushCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is the
  // system stack pointer (csp).
  ASSERT(csp.Is(StackPointer()));

  MemOperand tos(csp, -2 * kXRegSizeInBytes, PreIndex);

  stp(d14, d15, tos);
  stp(d12, d13, tos);
  stp(d10, d11, tos);
  stp(d8, d9, tos);

  stp(x29, x30, tos);
  stp(x27, x28, tos);    // x28 = jssp
  stp(x25, x26, tos);
  stp(x23, x24, tos);
  stp(x21, x22, tos);
  stp(x19, x20, tos);
}


void MacroAssembler::PopCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is the
  // system stack pointer (csp).
  ASSERT(csp.Is(StackPointer()));

  MemOperand tos(csp, 2 * kXRegSizeInBytes, PostIndex);

  ldp(x19, x20, tos);
  ldp(x21, x22, tos);
  ldp(x23, x24, tos);
  ldp(x25, x26, tos);
  ldp(x27, x28, tos);    // x28 = jssp
  ldp(x29, x30, tos);

  ldp(d8, d9, tos);
  ldp(d10, d11, tos);
  ldp(d12, d13, tos);
  ldp(d14, d15, tos);
}


void MacroAssembler::AssertStackConsistency() {
  if (emit_debug_code() && !csp.Is(StackPointer())) {
    if (csp.Is(StackPointer())) {
      // TODO(jbramley): Check for csp alignment if it is the stack pointer.
    } else {
      // TODO(jbramley): Currently we cannot use this assertion in Push because
      // some calling code assumes that the flags are preserved. For an example,
      // look at Builtins::Generate_ArgumentsAdaptorTrampoline.
      Cmp(csp, StackPointer());
      Check(ls, kTheCurrentStackPointerIsBelowCsp);
    }
  }
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index) {
  // TODO(jbramley): Most root values are constants, and can be synthesized
  // without a load. Refer to the ARM back end for details.
  Ldr(destination, MemOperand(root, index << kPointerSizeLog2));
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index) {
  Str(source, MemOperand(root, index << kPointerSizeLog2));
}


void MacroAssembler::LoadTrueFalseRoots(Register true_root,
                                        Register false_root) {
  STATIC_ASSERT((Heap::kTrueValueRootIndex + 1) == Heap::kFalseValueRootIndex);
  Ldp(true_root, false_root,
      MemOperand(root, Heap::kTrueValueRootIndex << kPointerSizeLog2));
}


void MacroAssembler::LoadHeapObject(Register result,
                                    Handle<HeapObject> object) {
  AllowDeferredHandleDereference using_raw_address;
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    Mov(result, Operand(cell));
    Ldr(result, FieldMemOperand(result, Cell::kValueOffset));
  } else {
    Mov(result, Operand(object));
  }
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  Ldr(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  Ldr(dst, FieldMemOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLengthUntagged(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  Ldrsw(dst, UntagSmiFieldMemOperand(map, Map::kBitField3Offset));
  And(dst, dst, Map::EnumLengthBits::kMask);
}


void MacroAssembler::EnumLengthSmi(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  Ldr(dst, FieldMemOperand(map, Map::kBitField3Offset));
  And(dst, dst, Operand(Smi::FromInt(Map::EnumLengthBits::kMask)));
}


void MacroAssembler::CheckEnumCache(Register object,
                                    Register null_value,
                                    Register scratch0,
                                    Register scratch1,
                                    Register scratch2,
                                    Register scratch3,
                                    Label* call_runtime) {
  ASSERT(!AreAliased(object, null_value, scratch0, scratch1, scratch2,
                     scratch3));

  Register empty_fixed_array_value = scratch0;
  Register current_object = scratch1;

  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Label next, start;

  Mov(current_object, object);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  Register map = scratch2;
  Register enum_length = scratch3;
  Ldr(map, FieldMemOperand(current_object, HeapObject::kMapOffset));

  EnumLengthUntagged(enum_length, map);
  Cmp(enum_length, kInvalidEnumCacheSentinel);
  B(eq, call_runtime);

  B(&start);

  Bind(&next);
  Ldr(map, FieldMemOperand(current_object, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLengthUntagged(enum_length, map);
  Cbnz(enum_length, call_runtime);

  Bind(&start);

  // Check that there are no elements. Register current_object contains the
  // current JS object we've reached through the prototype chain.
  Label no_elements;
  Ldr(current_object, FieldMemOperand(current_object,
                                      JSObject::kElementsOffset));
  Cmp(current_object, empty_fixed_array_value);
  B(eq, &no_elements);

  // Second chance, the object may be using the empty slow element dictionary.
  CompareRoot(current_object, Heap::kEmptySlowElementDictionaryRootIndex);
  B(ne, call_runtime);

  Bind(&no_elements);
  Ldr(current_object, FieldMemOperand(map, Map::kPrototypeOffset));
  Cmp(current_object, null_value);
  B(ne, &next);
}


void MacroAssembler::TestJSArrayForAllocationMemento(Register receiver,
                                                     Register scratch1,
                                                     Register scratch2,
                                                     Label* no_memento_found) {
  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  Add(scratch1, receiver,
      JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag);
  Cmp(scratch1, Operand(new_space_start));
  B(lt, no_memento_found);

  Mov(scratch2, Operand(new_space_allocation_top));
  Ldr(scratch2, MemOperand(scratch2));
  Cmp(scratch1, scratch2);
  B(gt, no_memento_found);

  Ldr(scratch1, MemOperand(scratch1, -AllocationMemento::kSize));
  Cmp(scratch1,
      Operand(isolate()->factory()->allocation_memento_map()));
}


void MacroAssembler::JumpToHandlerEntry(Register exception,
                                        Register object,
                                        Register state,
                                        Register scratch1,
                                        Register scratch2) {
  // Handler expects argument in x0.
  ASSERT(exception.Is(x0));

  // Compute the handler entry address and jump to it. The handler table is
  // a fixed array of (smi-tagged) code offsets.
  Ldr(scratch1, FieldMemOperand(object, Code::kHandlerTableOffset));
  Add(scratch1, scratch1, FixedArray::kHeaderSize - kHeapObjectTag);
  STATIC_ASSERT(StackHandler::kKindWidth < kPointerSizeLog2);
  Lsr(scratch2, state, StackHandler::kKindWidth);
  Ldr(scratch2, MemOperand(scratch1, scratch2, LSL, kPointerSizeLog2));
  Add(scratch1, object, Code::kHeaderSize - kHeapObjectTag);
  Add(scratch1, scratch1, Operand::UntagSmi(scratch2));
  Br(scratch1);
}


void MacroAssembler::InNewSpace(Register object,
                                Condition cond,
                                Label* branch) {
  ASSERT(cond == eq || cond == ne);
  // Use Tmp1() to have a different destination register, as Tmp0() will be used
  // for relocation.
  And(Tmp1(), object, Operand(ExternalReference::new_space_mask(isolate())));
  Cmp(Tmp1(), Operand(ExternalReference::new_space_start(isolate())));
  B(cond, branch);
}


void MacroAssembler::Throw(Register value,
                           Register scratch1,
                           Register scratch2,
                           Register scratch3,
                           Register scratch4) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The handler expects the exception in x0.
  ASSERT(value.Is(x0));

  // Drop the stack pointer to the top of the top handler.
  ASSERT(jssp.Is(StackPointer()));
  Mov(scratch1, Operand(ExternalReference(Isolate::kHandlerAddress,
                                          isolate())));
  Ldr(jssp, MemOperand(scratch1));
  // Restore the next handler.
  Pop(scratch2);
  Str(scratch2, MemOperand(scratch1));

  // Get the code object and state.  Restore the context and frame pointer.
  Register object = scratch1;
  Register state = scratch2;
  Pop(object, state, cp, fp);

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (fp == 0) == (cp == 0), so we could test either fp
  // or cp.
  Label not_js_frame;
  Cbz(cp, &not_js_frame);
  Str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  Bind(&not_js_frame);

  JumpToHandlerEntry(value, object, state, scratch3, scratch4);
}


void MacroAssembler::ThrowUncatchable(Register value,
                                      Register scratch1,
                                      Register scratch2,
                                      Register scratch3,
                                      Register scratch4) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The handler expects the exception in x0.
  ASSERT(value.Is(x0));

  // Drop the stack pointer to the top of the top stack handler.
  ASSERT(jssp.Is(StackPointer()));
  Mov(scratch1, Operand(ExternalReference(Isolate::kHandlerAddress,
                                          isolate())));
  Ldr(jssp, MemOperand(scratch1));

  // Unwind the handlers until the ENTRY handler is found.
  Label fetch_next, check_kind;
  B(&check_kind);
  Bind(&fetch_next);
  Peek(jssp, StackHandlerConstants::kNextOffset);

  Bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  Peek(scratch2, StackHandlerConstants::kStateOffset);
  TestAndBranchIfAnySet(scratch2, StackHandler::KindField::kMask, &fetch_next);

  // Set the top handler address to next handler past the top ENTRY handler.
  Pop(scratch2);
  Str(scratch2, MemOperand(scratch1));

  // Get the code object and state.  Clear the context and frame pointer (0 was
  // saved in the handler).
  Register object = scratch1;
  Register state = scratch2;
  Pop(object, state, cp, fp);

  JumpToHandlerEntry(value, object, state, scratch3, scratch4);
}


void MacroAssembler::Throw(BailoutReason reason) {
  Label throw_start;
  Bind(&throw_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  RecordComment("Throw message: ");
  RecordComment((msg != NULL) ? msg : "UNKNOWN");
#endif

  Mov(x0, Operand(Smi::FromInt(reason)));
  Push(x0);

  // Disable stub call restrictions to always allow calls to throw.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kThrowMessage, 1);
  } else {
    CallRuntime(Runtime::kThrowMessage, 1);
  }
  // ThrowMessage should not return here.
  Unreachable();
}


void MacroAssembler::ThrowIf(Condition cc, BailoutReason reason) {
  Label ok;
  B(InvertCondition(cc), &ok);
  Throw(reason);
  Bind(&ok);
}


void MacroAssembler::ThrowIfSmi(const Register& value, BailoutReason reason) {
  Label ok;
  JumpIfNotSmi(value, &ok);
  Throw(reason);
  Bind(&ok);
}


void MacroAssembler::SmiAbs(const Register& smi, Label* slow) {
  ASSERT(smi.Is64Bits());
  Abs(smi, smi, slow);
}


void MacroAssembler::AssertSmi(Register object, BailoutReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(eq, reason);
  }
}


void MacroAssembler::AssertNotSmi(Register object, BailoutReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(ne, reason);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    // TODO(jbramley): Add AbortIfSmi and related functions.
    Label not_smi;
    JumpIfNotSmi(object, &not_smi);
    Abort(kOperandIsASmiAndNotAName);
    Bind(&not_smi);

    Ldr(Tmp1(), FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(Tmp1(), Tmp1(), LAST_NAME_TYPE);
    Check(ls, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    Register temp = Tmp1();
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(ne, kOperandIsASmiAndNotAString);
    Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(temp, temp, FIRST_NONSTRING_TYPE);
    Check(lo, kOperandIsNotAString);
  }
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  ASSERT(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(isolate()), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All arguments must be on the stack before this function is called.
  // x0 holds the return value after the call.

  // Check that the number of arguments matches what the function expects.
  // If f->nargs is -1, the function can accept a variable number of arguments.
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    // Illegal operation: drop the stack arguments and return undefined.
    if (num_arguments > 0) {
      Drop(num_arguments);
    }
    LoadRoot(x0, Heap::kUndefinedValueRootIndex);
    return;
  }

  // Place the necessary arguments.
  Mov(x0, num_arguments);
  Mov(x1, Operand(ExternalReference(f, isolate())));

  CEntryStub stub(1, save_doubles);
  CallStub(&stub);
}


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}


void MacroAssembler::CallApiFunctionAndReturn(
    Register function_address,
    ExternalReference thunk_ref,
    int stack_space,
    int spill_offset,
    MemOperand return_value_operand,
    MemOperand* context_restore_operand) {
  ASM_LOCATION("CallApiFunctionAndReturn");
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate());
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate()),
      next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate()),
      next_address);

  ASSERT(function_address.is(x1) || function_address.is(x2));

  Label profiler_disabled;
  Label end_profiler_check;
  bool* is_profiling_flag = isolate()->cpu_profiler()->is_profiling_address();
  STATIC_ASSERT(sizeof(*is_profiling_flag) == 1);
  Mov(x10, reinterpret_cast<uintptr_t>(is_profiling_flag));
  Ldrb(w10, MemOperand(x10));
  Cbz(w10, &profiler_disabled);
  Mov(x3, Operand(thunk_ref));
  B(&end_profiler_check);

  Bind(&profiler_disabled);
  Mov(x3, function_address);
  Bind(&end_profiler_check);

  // Save the callee-save registers we are going to use.
  // TODO(all): Is this necessary? ARM doesn't do it.
  STATIC_ASSERT(kCallApiFunctionSpillSpace == 4);
  Poke(x19, (spill_offset + 0) * kXRegSizeInBytes);
  Poke(x20, (spill_offset + 1) * kXRegSizeInBytes);
  Poke(x21, (spill_offset + 2) * kXRegSizeInBytes);
  Poke(x22, (spill_offset + 3) * kXRegSizeInBytes);

  // Allocate HandleScope in callee-save registers.
  // We will need to restore the HandleScope after the call to the API function,
  // by allocating it in callee-save registers they will be preserved by C code.
  Register handle_scope_base = x22;
  Register next_address_reg = x19;
  Register limit_reg = x20;
  Register level_reg = w21;

  Mov(handle_scope_base, Operand(next_address));
  Ldr(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  Ldr(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  Ldr(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  Add(level_reg, level_reg, 1);
  Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    Mov(x0, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub;
  stub.GenerateCall(this, x3);

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    Mov(x0, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_leave_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label exception_handled;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  Ldr(x0, return_value_operand);
  Bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  Str(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  if (emit_debug_code()) {
    Ldr(w1, MemOperand(handle_scope_base, kLevelOffset));
    Cmp(w1, level_reg);
    Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  Sub(level_reg, level_reg, 1);
  Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  Ldr(x1, MemOperand(handle_scope_base, kLimitOffset));
  Cmp(limit_reg, x1);
  B(ne, &delete_allocated_handles);

  Bind(&leave_exit_frame);
  // Restore callee-saved registers.
  Peek(x19, (spill_offset + 0) * kXRegSizeInBytes);
  Peek(x20, (spill_offset + 1) * kXRegSizeInBytes);
  Peek(x21, (spill_offset + 2) * kXRegSizeInBytes);
  Peek(x22, (spill_offset + 3) * kXRegSizeInBytes);

  // Check if the function scheduled an exception.
  Mov(x5, Operand(ExternalReference::scheduled_exception_address(isolate())));
  Ldr(x5, MemOperand(x5));
  JumpIfNotRoot(x5, Heap::kTheHoleValueRootIndex, &promote_scheduled_exception);
  Bind(&exception_handled);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    Ldr(cp, *context_restore_operand);
  }

  LeaveExitFrame(false, x1, !restore_context);
  Drop(stack_space);
  Ret();

  Bind(&promote_scheduled_exception);
  {
    FrameScope frame(this, StackFrame::INTERNAL);
    CallExternalReference(
        ExternalReference(Runtime::kPromoteScheduledException, isolate()), 0);
  }
  B(&exception_handled);

  // HandleScope limit has changed. Delete allocated extensions.
  Bind(&delete_allocated_handles);
  Str(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  // Save the return value in a callee-save register.
  Register saved_result = x19;
  Mov(saved_result, x0);
  Mov(x0, Operand(ExternalReference::isolate_address(isolate())));
  CallCFunction(
      ExternalReference::delete_handle_scope_extensions(isolate()), 1);
  Mov(x0, saved_result);
  B(&leave_exit_frame);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  Mov(x0, num_arguments);
  Mov(x1, Operand(ext));

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  Mov(x1, Operand(builtin));
  CEntryStub stub(1);
  Jump(stub.GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  Ldr(target, GlobalObjectMemOperand());
  Ldr(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  Ldr(target, FieldMemOperand(target,
                          JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(x1));
  GetBuiltinFunction(x1, id);
  // Load the code entry point from the builtins object.
  Ldr(target, FieldMemOperand(x1, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  ASM_LOCATION("MacroAssembler::InvokeBuiltin");
  // You can't call a builtin without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  GetBuiltinEntry(x2, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(x2));
    Call(x2);
    call_wrapper.AfterCall();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    Jump(x2);
  }
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Mov(x0, num_arguments);
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


void MacroAssembler::InitializeNewString(Register string,
                                         Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1,
                                         Register scratch2) {
  ASSERT(!AreAliased(string, length, scratch1, scratch2));
  LoadRoot(scratch2, map_index);
  SmiTag(scratch1, length);
  Str(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));

  Mov(scratch2, String::kEmptyHashField);
  Str(scratch1, FieldMemOperand(string, String::kLengthOffset));
  Str(scratch2, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_A64
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return OS::ActivationFrameAlignment();
#else  // V8_HOST_ARCH_A64
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_A64
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args) {
  CallCFunction(function, num_of_reg_args, 0);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  Mov(Tmp0(), Operand(function));
  CallCFunction(Tmp0(), num_of_reg_args, num_of_double_args);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  ASSERT(has_frame());
  // We can pass 8 integer arguments in registers. If we need to pass more than
  // that, we'll need to implement support for passing them on the stack.
  ASSERT(num_of_reg_args <= 8);

  // If we're passing doubles, we're limited to the following prototypes
  // (defined by ExternalReference::Type):
  //  BUILTIN_COMPARE_CALL:  int f(double, double)
  //  BUILTIN_FP_FP_CALL:    double f(double, double)
  //  BUILTIN_FP_CALL:       double f(double)
  //  BUILTIN_FP_INT_CALL:   double f(double, int)
  if (num_of_double_args > 0) {
    ASSERT(num_of_reg_args <= 1);
    ASSERT((num_of_double_args + num_of_reg_args) <= 2);
  }


  // If the stack pointer is not csp, we need to derive an aligned csp from the
  // current stack pointer.
  const Register old_stack_pointer = StackPointer();
  if (!csp.Is(old_stack_pointer)) {
    AssertStackConsistency();

    int sp_alignment = ActivationFrameAlignment();
    // The ABI mandates at least 16-byte alignment.
    ASSERT(sp_alignment >= 16);
    ASSERT(IsPowerOf2(sp_alignment));

    // The current stack pointer is a callee saved register, and is preserved
    // across the call.
    ASSERT(kCalleeSaved.IncludesAliasOf(old_stack_pointer));

    // Align and synchronize the system stack pointer with jssp.
    Bic(csp, old_stack_pointer, sp_alignment - 1);
    SetStackPointer(csp);
  }

  // Call directly. The function called cannot cause a GC, or allow preemption,
  // so the return address in the link register stays correct.
  Call(function);

  if (!csp.Is(old_stack_pointer)) {
    if (emit_debug_code()) {
      // Because the stack pointer must be aligned on a 16-byte boundary, the
      // aligned csp can be up to 12 bytes below the jssp. This is the case
      // where we only pushed one W register on top of an aligned jssp.
      Register temp = Tmp1();
      ASSERT(ActivationFrameAlignment() == 16);
      Sub(temp, csp, old_stack_pointer);
      // We want temp <= 0 && temp >= -12.
      Cmp(temp, 0);
      Ccmp(temp, -12, NFlag, le);
      Check(ge, kTheStackWasCorruptedByMacroAssemblerCall);
    }
    SetStackPointer(old_stack_pointer);
  }
}


void MacroAssembler::Jump(Register target) {
  Br(target);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode) {
  Mov(Tmp0(), Operand(target, rmode));
  Br(Tmp0());
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  AllowDeferredHandleDereference embedding_raw_address;
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode);
}


void MacroAssembler::Call(Register target) {
  BlockConstPoolScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  Blr(target);

#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target));
#endif
}


void MacroAssembler::Call(Label* target) {
  BlockConstPoolScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  Bl(target);

#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target));
#endif
}


// MacroAssembler::CallSize is sensitive to changes in this function, as it
// requires to know how many instructions are used to branch to the target.
void MacroAssembler::Call(Address target, RelocInfo::Mode rmode) {
  BlockConstPoolScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif
  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  ASSERT(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    uint64_t imm = reinterpret_cast<uint64_t>(target);
    movz(Tmp0(), (imm >> 0) & 0xffff, 0);
    movk(Tmp0(), (imm >> 16) & 0xffff, 16);
    movk(Tmp0(), (imm >> 32) & 0xffff, 32);
    movk(Tmp0(), (imm >> 48) & 0xffff, 48);
  } else {
    LoadRelocated(Tmp0(), Operand(reinterpret_cast<intptr_t>(target), rmode));
  }
  Blr(Tmp0());
#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target, rmode));
#endif
}


void MacroAssembler::Call(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id) {
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  if ((rmode == RelocInfo::CODE_TARGET) && (!ast_id.IsNone())) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }

  AllowDeferredHandleDereference embedding_raw_address;
  Call(reinterpret_cast<Address>(code.location()), rmode);

#ifdef DEBUG
  // Check the size of the code generated.
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(code, rmode, ast_id));
#endif
}


int MacroAssembler::CallSize(Register target) {
  USE(target);
  return kInstructionSize;
}


int MacroAssembler::CallSize(Label* target) {
  USE(target);
  return kInstructionSize;
}


int MacroAssembler::CallSize(Address target, RelocInfo::Mode rmode) {
  USE(target);

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  ASSERT(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    return kCallSizeWithoutRelocation;
  } else {
    return kCallSizeWithRelocation;
  }
}


int MacroAssembler::CallSize(Handle<Code> code,
                             RelocInfo::Mode rmode,
                             TypeFeedbackId ast_id) {
  USE(code);
  USE(ast_id);

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  ASSERT(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    return kCallSizeWithoutRelocation;
  } else {
    return kCallSizeWithRelocation;
  }
}





void MacroAssembler::JumpForHeapNumber(Register object,
                                       Register heap_number_map,
                                       Label* on_heap_number,
                                       Label* on_not_heap_number) {
  ASSERT(on_heap_number || on_not_heap_number);
  // Tmp0() is used as a scratch register.
  ASSERT(!AreAliased(Tmp0(), heap_number_map));
  AssertNotSmi(object);

  // Load the HeapNumber map if it is not passed.
  if (heap_number_map.Is(NoReg)) {
    heap_number_map = Tmp1();
    LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  } else {
    // This assert clobbers Tmp0(), so do it before loading Tmp0() with the map.
    AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  }

  Ldr(Tmp0(), FieldMemOperand(object, HeapObject::kMapOffset));
  Cmp(Tmp0(), heap_number_map);

  if (on_heap_number) {
    B(eq, on_heap_number);
  }
  if (on_not_heap_number) {
    B(ne, on_not_heap_number);
  }
}


void MacroAssembler::JumpIfHeapNumber(Register object,
                                      Label* on_heap_number,
                                      Register heap_number_map) {
  JumpForHeapNumber(object,
                    heap_number_map,
                    on_heap_number,
                    NULL);
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Label* on_not_heap_number,
                                         Register heap_number_map) {
  JumpForHeapNumber(object,
                    heap_number_map,
                    NULL,
                    on_not_heap_number);
}


void MacroAssembler::LookupNumberStringCache(Register object,
                                             Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* not_found) {
  ASSERT(!AreAliased(object, result, scratch1, scratch2, scratch3));

  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  Ldrsw(mask, UntagSmiFieldMemOperand(number_string_cache,
                                      FixedArray::kLengthOffset));
  Asr(mask, mask, 1);  // Divide length by two.
  Sub(mask, mask, 1);  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;

  JumpIfSmi(object, &is_smi);
  CheckMap(object, scratch1, Heap::kHeapNumberMapRootIndex, not_found,
           DONT_DO_SMI_CHECK);

  STATIC_ASSERT(kDoubleSize == (kWRegSizeInBytes * 2));
  Add(scratch1, object, HeapNumber::kValueOffset - kHeapObjectTag);
  Ldp(scratch1.W(), scratch2.W(), MemOperand(scratch1));
  Eor(scratch1, scratch1, scratch2);
  And(scratch1, scratch1, mask);

  // Calculate address of entry in string cache: each entry consists of two
  // pointer sized fields.
  Add(scratch1, number_string_cache,
      Operand(scratch1, LSL, kPointerSizeLog2 + 1));

  Register probe = mask;
  Ldr(probe, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  JumpIfSmi(probe, not_found);
  Ldr(d0, FieldMemOperand(object, HeapNumber::kValueOffset));
  Ldr(d1, FieldMemOperand(probe, HeapNumber::kValueOffset));
  Fcmp(d0, d1);
  B(ne, not_found);
  B(&load_result_from_cache);

  Bind(&is_smi);
  Register scratch = scratch1;
  And(scratch, mask, Operand::UntagSmi(object));
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  Add(scratch, number_string_cache,
      Operand(scratch, LSL, kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  Ldr(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  Cmp(object, probe);
  B(ne, not_found);

  // Get the result from the cache.
  Bind(&load_result_from_cache);
  Ldr(result, FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  IncrementCounter(isolate()->counters()->number_to_string_native(), 1,
                   scratch1, scratch2);
}


void MacroAssembler::TryConvertDoubleToInt(Register as_int,
                                           FPRegister value,
                                           FPRegister scratch_d,
                                           Label* on_successful_conversion,
                                           Label* on_failed_conversion) {
  // Convert to an int and back again, then compare with the original value.
  Fcvtzs(as_int, value);
  Scvtf(scratch_d, as_int);
  Fcmp(value, scratch_d);

  if (on_successful_conversion) {
    B(on_successful_conversion, eq);
  }
  if (on_failed_conversion) {
    B(on_failed_conversion, ne);
  }
}


void MacroAssembler::JumpIfMinusZero(DoubleRegister input,
                                     Label* on_negative_zero) {
  // Floating point -0.0 is kMinInt as an integer, so subtracting 1 (cmp) will
  // cause overflow.
  Fmov(Tmp0(), input);
  Cmp(Tmp0(), 1);
  B(vs, on_negative_zero);
}


void MacroAssembler::ClampInt32ToUint8(Register output, Register input) {
  // Clamp the value to [0..255].
  Cmp(input.W(), Operand(input.W(), UXTB));
  // If input < input & 0xff, it must be < 0, so saturate to 0.
  Csel(output.W(), wzr, input.W(), lt);
  // Create a constant 0xff.
  Mov(WTmp0(), 255);
  // If input > input & 0xff, it must be > 255, so saturate to 255.
  Csel(output.W(), WTmp0(), output.W(), gt);
}


void MacroAssembler::ClampInt32ToUint8(Register in_out) {
  ClampInt32ToUint8(in_out, in_out);
}


void MacroAssembler::ClampDoubleToUint8(Register output,
                                        DoubleRegister input,
                                        DoubleRegister dbl_scratch) {
  // This conversion follows the WebIDL "[Clamp]" rules for PIXEL types:
  //   - Inputs lower than 0 (including -infinity) produce 0.
  //   - Inputs higher than 255 (including +infinity) produce 255.
  // Also, it seems that PIXEL types use round-to-nearest rather than
  // round-towards-zero.

  // Squash +infinity before the conversion, since Fcvtnu will normally
  // convert it to 0.
  Fmov(dbl_scratch, 255);
  Fmin(dbl_scratch, dbl_scratch, input);

  // Convert double to unsigned integer. Values less than zero become zero.
  // Values greater than 255 have already been clamped to 255.
  Fcvtnu(output, dbl_scratch);
}


void MacroAssembler::CopyFieldsLoopPairsHelper(Register dst,
                                               Register src,
                                               unsigned count,
                                               Register scratch1,
                                               Register scratch2,
                                               Register scratch3) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in a tight loop.
  ASSERT(!AreAliased(dst, src, scratch1, scratch2, scratch3, Tmp0(), Tmp1()));
  ASSERT(count >= 2);

  const Register& remaining = scratch3;
  Mov(remaining, count / 2);

  // Only use the Assembler, so we can use Tmp0() and Tmp1().
  InstructionAccurateScope scope(this);

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = scratch2;
  sub(dst_untagged, dst, kHeapObjectTag);
  sub(src_untagged, src, kHeapObjectTag);

  // Copy fields in pairs.
  Label loop;
  bind(&loop);
  ldp(Tmp0(), Tmp1(), MemOperand(src_untagged, kXRegSizeInBytes * 2,
                                 PostIndex));
  stp(Tmp0(), Tmp1(), MemOperand(dst_untagged, kXRegSizeInBytes * 2,
                                 PostIndex));
  sub(remaining, remaining, 1);
  cbnz(remaining, &loop);

  // Handle the leftovers.
  if (count & 1) {
    ldr(Tmp0(), MemOperand(src_untagged));
    str(Tmp0(), MemOperand(dst_untagged));
  }
}


void MacroAssembler::CopyFieldsUnrolledPairsHelper(Register dst,
                                                   Register src,
                                                   unsigned count,
                                                   Register scratch1,
                                                   Register scratch2) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in an unrolled loop.
  ASSERT(!AreAliased(dst, src, scratch1, scratch2, Tmp0(), Tmp1()));

  // Only use the Assembler, so we can use Tmp0() and Tmp1().
  InstructionAccurateScope scope(this);

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = scratch2;
  sub(dst_untagged, dst, kHeapObjectTag);
  sub(src_untagged, src, kHeapObjectTag);

  // Copy fields in pairs.
  for (unsigned i = 0; i < count / 2; i++) {
    ldp(Tmp0(), Tmp1(), MemOperand(src_untagged, kXRegSizeInBytes * 2,
                                   PostIndex));
    stp(Tmp0(), Tmp1(), MemOperand(dst_untagged, kXRegSizeInBytes * 2,
                                   PostIndex));
  }

  // Handle the leftovers.
  if (count & 1) {
    ldr(Tmp0(), MemOperand(src_untagged));
    str(Tmp0(), MemOperand(dst_untagged));
  }
}


void MacroAssembler::CopyFieldsUnrolledHelper(Register dst,
                                              Register src,
                                              unsigned count,
                                              Register scratch1) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in an unrolled loop.
  ASSERT(!AreAliased(dst, src, scratch1, Tmp0(), Tmp1()));

  // Only use the Assembler, so we can use Tmp0() and Tmp1().
  InstructionAccurateScope scope(this);

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = Tmp1();
  sub(dst_untagged, dst, kHeapObjectTag);
  sub(src_untagged, src, kHeapObjectTag);

  // Copy fields one by one.
  for (unsigned i = 0; i < count; i++) {
    ldr(Tmp0(), MemOperand(src_untagged, kXRegSizeInBytes, PostIndex));
    str(Tmp0(), MemOperand(dst_untagged, kXRegSizeInBytes, PostIndex));
  }
}


void MacroAssembler::CopyFields(Register dst, Register src, CPURegList temps,
                                unsigned count) {
  // One of two methods is used:
  //
  // For high 'count' values where many scratch registers are available:
  //    Untag src and dst into scratch registers.
  //    Copy src->dst in a tight loop.
  //
  // For low 'count' values or where few scratch registers are available:
  //    Untag src and dst into scratch registers.
  //    Copy src->dst in an unrolled loop.
  //
  // In both cases, fields are copied in pairs if possible, and left-overs are
  // handled separately.
  ASSERT(!temps.IncludesAliasOf(dst));
  ASSERT(!temps.IncludesAliasOf(src));
  ASSERT(!temps.IncludesAliasOf(Tmp0()));
  ASSERT(!temps.IncludesAliasOf(Tmp1()));
  ASSERT(!temps.IncludesAliasOf(xzr));
  ASSERT(!AreAliased(dst, src, Tmp0(), Tmp1()));

  if (emit_debug_code()) {
    Cmp(dst, src);
    Check(ne, kTheSourceAndDestinationAreTheSame);
  }

  // The value of 'count' at which a loop will be generated (if there are
  // enough scratch registers).
  static const unsigned kLoopThreshold = 8;

  ASSERT(!temps.IsEmpty());
  Register scratch1 = Register(temps.PopLowestIndex());
  Register scratch2 = Register(temps.PopLowestIndex());
  Register scratch3 = Register(temps.PopLowestIndex());

  if (scratch3.IsValid() && (count >= kLoopThreshold)) {
    CopyFieldsLoopPairsHelper(dst, src, count, scratch1, scratch2, scratch3);
  } else if (scratch2.IsValid()) {
    CopyFieldsUnrolledPairsHelper(dst, src, count, scratch1, scratch2);
  } else if (scratch1.IsValid()) {
    CopyFieldsUnrolledHelper(dst, src, count, scratch1);
  } else {
    UNREACHABLE();
  }
}


void MacroAssembler::CopyBytes(Register dst,
                               Register src,
                               Register length,
                               Register scratch,
                               CopyHint hint) {
  ASSERT(!AreAliased(src, dst, length, scratch));

  // TODO(all): Implement a faster copy function, and use hint to determine
  // which algorithm to use for copies.
  if (emit_debug_code()) {
    // Check copy length.
    Cmp(length, 0);
    Assert(ge, kUnexpectedNegativeValue);

    // Check src and dst buffers don't overlap.
    Add(scratch, src, length);  // Calculate end of src buffer.
    Cmp(scratch, dst);
    Add(scratch, dst, length);  // Calculate end of dst buffer.
    Ccmp(scratch, src, ZFlag, gt);
    Assert(le, kCopyBuffersOverlap);
  }

  Label loop, done;
  Cbz(length, &done);

  Bind(&loop);
  Sub(length, length, 1);
  Ldrb(scratch, MemOperand(src, 1, PostIndex));
  Strb(scratch, MemOperand(dst, 1, PostIndex));
  Cbnz(length, &loop);
  Bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  B(&entry);
  Bind(&loop);
  // TODO(all): consider using stp here.
  Str(filler, MemOperand(start_offset, kPointerSize, PostIndex));
  Bind(&entry);
  Cmp(start_offset, end_offset);
  B(lt, &loop);
}


void MacroAssembler::JumpIfEitherIsNotSequentialAsciiStrings(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure,
    SmiCheckType smi_check) {

  if (smi_check == DO_SMI_CHECK) {
    JumpIfEitherSmi(first, second, failure);
  } else if (emit_debug_code()) {
    ASSERT(smi_check == DONT_DO_SMI_CHECK);
    Label not_smi;
    JumpIfEitherSmi(first, second, NULL, &not_smi);

    // At least one input is a smi, but the flags indicated a smi check wasn't
    // needed.
    Abort(kUnexpectedSmi);

    Bind(&not_smi);
  }

  // Test that both first and second are sequential ASCII strings.
  Ldr(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  Ldr(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  Ldrb(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfEitherInstanceTypeIsNotSequentialAscii(scratch1,
                                               scratch2,
                                               scratch1,
                                               scratch2,
                                               failure);
}


void MacroAssembler::JumpIfEitherInstanceTypeIsNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  ASSERT(!AreAliased(scratch1, second));
  ASSERT(!AreAliased(scratch1, scratch2));
  static const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  static const int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  And(scratch1, first, kFlatAsciiStringMask);
  And(scratch2, second, kFlatAsciiStringMask);
  Cmp(scratch1, kFlatAsciiStringTag);
  Ccmp(scratch2, kFlatAsciiStringTag, NoFlag, eq);
  B(ne, failure);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                                            Register scratch,
                                                            Label* failure) {
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  And(scratch, type, kFlatAsciiStringMask);
  Cmp(scratch, kFlatAsciiStringTag);
  B(ne, failure);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  ASSERT(!AreAliased(first, second, scratch1, scratch2));
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  And(scratch1, first, kFlatAsciiStringMask);
  And(scratch2, second, kFlatAsciiStringMask);
  Cmp(scratch1, kFlatAsciiStringTag);
  Ccmp(scratch2, kFlatAsciiStringTag, NoFlag, eq);
  B(ne, failure);
}


void MacroAssembler::JumpIfNotUniqueName(Register type,
                                         Label* not_unique_name) {
  STATIC_ASSERT((kInternalizedTag == 0) && (kStringTag == 0));
  // if ((type is string && type is internalized) || type == SYMBOL_TYPE) {
  //   continue
  // } else {
  //   goto not_unique_name
  // }
  Tst(type, kIsNotStringMask | kIsNotInternalizedMask);
  Ccmp(type, SYMBOL_TYPE, ZFlag, ne);
  B(ne, not_unique_name);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    InvokeFlag flag,
                                    bool* definitely_mismatches,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  x0: actual arguments count.
  //  x1: function (passed through to callee).
  //  x2: expected arguments count.

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  ASSERT(actual.is_immediate() || actual.reg().is(x0));
  ASSERT(expected.is_immediate() || expected.reg().is(x2));
  ASSERT((!code_constant.is_null() && code_reg.is(no_reg)) || code_reg.is(x3));

  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;

    } else {
      Mov(x0, actual.immediate());
      if (expected.immediate() ==
          SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        // Set up x2 for the argument adaptor.
        Mov(x2, expected.immediate());
      }
    }

  } else {  // expected is a register.
    Operand actual_op = actual.is_immediate() ? Operand(actual.immediate())
                                              : Operand(actual.reg());
    // If actual == expected perform a regular invocation.
    Cmp(expected.reg(), actual_op);
    B(eq, &regular_invoke);
    // Otherwise set up x0 for the argument adaptor.
    Mov(x0, actual_op);
  }

  // If the argument counts may mismatch, generate a call to the argument
  // adaptor.
  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      Mov(x3, Operand(code_constant));
      Add(x3, x3, Code::kHeaderSize - kHeapObjectTag);
    }

    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        // If the arg counts don't match, no extra code is emitted by
        // MAsm::InvokeCode and we can just fall through.
        B(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
  }
  Bind(&regular_invoke);
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;

  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done, flag,
                 &definitely_mismatches, call_wrapper);

  // If we are certain that actual != expected, then we know InvokePrologue will
  // have handled the call through the argument adaptor mechanism.
  // The called function expects the call kind in x5.
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      Call(code);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      Jump(code);
    }
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  Bind(&done);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  ASSERT(function.is(x1));

  Register expected_reg = x2;
  Register code_reg = x3;

  Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));
  // The number of arguments is stored as an int32_t, and -1 is a marker
  // (SharedFunctionInfo::kDontAdaptArgumentsSentinel), so we need sign
  // extension to correctly handle it.
  Ldr(expected_reg, FieldMemOperand(function,
                                    JSFunction::kSharedFunctionInfoOffset));
  Ldrsw(expected_reg,
        FieldMemOperand(expected_reg,
                        SharedFunctionInfo::kFormalParameterCountOffset));
  Ldr(code_reg,
      FieldMemOperand(function, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  ASSERT(function.Is(x1));

  Register code_reg = x3;

  // Set up the context.
  Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Ldr(code_reg, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
  InvokeCode(code_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  __ LoadObject(x1, function);
  InvokeFunction(x1, expected, actual, flag, call_wrapper);
}


void MacroAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiValueSize == 32);

  // Try to convert with a FPU convert instruction. It's trivial to compute
  // the modulo operation on an integer register so we convert to a 64-bit
  // integer, then find the 32-bit result from that.
  //
  // Fcvtzs will saturate to INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff)
  // when the double is out of range. NaNs and infinities will be converted to 0
  // (as ECMA-262 requires).
  Fcvtzs(result, double_input);

  // The values INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff) are not
  // representable using a double, so if the result is one of those then we know
  // that saturation occured, and we need to manually handle the conversion.
  //
  // It is easy to detect INT64_MIN and INT64_MAX because adding or subtracting
  // 1 will cause signed overflow.
  Cmp(result, 1);
  Ccmp(result, -1, VFlag, vc);

  B(vc, done);
}


void MacroAssembler::TruncateDoubleToI(Register result,
                                       DoubleRegister double_input) {
  Label done;
  ASSERT(jssp.Is(StackPointer()));

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr);
  Push(double_input);  // Put input on stack.

  DoubleToIStub stub(jssp,
                     result,
                     0,
                     true,   // is_truncating
                     true);  // skip_fastpath
  CallStub(&stub);  // DoubleToIStub preserves any registers it needs to clobber

  Drop(1, kDoubleSize);  // Drop the double input on the stack.
  Pop(lr);

  Bind(&done);

  // TODO(rmcilroy): Remove this Sxtw once the following bug is fixed:
  // https://code.google.com/p/v8/issues/detail?id=3149
  Sxtw(result, result.W());
}


void MacroAssembler::TruncateHeapNumberToI(Register result,
                                           Register object) {
  Label done;
  ASSERT(!result.is(object));
  ASSERT(jssp.Is(StackPointer()));

  Ldr(fp_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));
  TryInlineTruncateDoubleToI(result, fp_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr);
  DoubleToIStub stub(object,
                     result,
                     HeapNumber::kValueOffset - kHeapObjectTag,
                     true,   // is_truncating
                     true);  // skip_fastpath
  CallStub(&stub);  // DoubleToIStub preserves any registers it needs to clobber
  Pop(lr);

  Bind(&done);

  // TODO(rmcilroy): Remove this Sxtw once the following bug is fixed:
  // https://code.google.com/p/v8/issues/detail?id=3149
  Sxtw(result, result.W());
}


void MacroAssembler::Prologue(PrologueFrameMode frame_mode) {
  if (frame_mode == BUILD_STUB_FRAME) {
    ASSERT(StackPointer().Is(jssp));
    // TODO(jbramley): Does x1 contain a JSFunction here, or does it already
    // have the special STUB smi?
    __ Mov(Tmp0(), Operand(Smi::FromInt(StackFrame::STUB)));
    // Compiled stubs don't age, and so they don't need the predictable code
    // ageing sequence.
    __ Push(lr, fp, cp, Tmp0());
    __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);
  } else {
    if (isolate()->IsCodePreAgingActive()) {
      Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
      __ EmitCodeAgeSequence(stub);
    } else {
      __ EmitFrameSetupForCodeAgePatching();
    }
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  ASSERT(jssp.Is(StackPointer()));
  Push(lr, fp, cp);
  Mov(Tmp1(), Operand(Smi::FromInt(type)));
  Mov(Tmp0(), Operand(CodeObject()));
  Push(Tmp1(), Tmp0());
  // jssp[4] : lr
  // jssp[3] : fp
  // jssp[2] : cp
  // jssp[1] : type
  // jssp[0] : code object

  // Adjust FP to point to saved FP.
  add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  ASSERT(jssp.Is(StackPointer()));
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  Mov(jssp, fp);
  AssertStackConsistency();
  Pop(fp, lr);
}


void MacroAssembler::ExitFramePreserveFPRegs() {
  PushCPURegList(kCallerSavedFP);
}


void MacroAssembler::ExitFrameRestoreFPRegs() {
  // Read the registers from the stack without popping them. The stack pointer
  // will be reset as part of the unwinding process.
  CPURegList saved_fp_regs = kCallerSavedFP;
  ASSERT(saved_fp_regs.Count() % 2 == 0);

  int offset = ExitFrameConstants::kLastExitFrameField;
  while (!saved_fp_regs.IsEmpty()) {
    const CPURegister& dst0 = saved_fp_regs.PopHighestIndex();
    const CPURegister& dst1 = saved_fp_regs.PopHighestIndex();
    offset -= 2 * kDRegSizeInBytes;
    Ldp(dst1, dst0, MemOperand(fp, offset));
  }
}


// TODO(jbramley): Check that we're handling the frame pointer correctly.
void MacroAssembler::EnterExitFrame(bool save_doubles,
                                    const Register& scratch,
                                    int extra_space) {
  ASSERT(jssp.Is(StackPointer()));

  // Set up the new stack frame.
  Mov(scratch, Operand(CodeObject()));
  Push(lr, fp);
  Mov(fp, StackPointer());
  Push(xzr, scratch);
  //          fp[8]: CallerPC (lr)
  //    fp -> fp[0]: CallerFP (old fp)
  //          fp[-8]: Space reserved for SPOffset.
  //  jssp -> fp[-16]: CodeObject()
  STATIC_ASSERT((2 * kPointerSize) ==
                ExitFrameConstants::kCallerSPDisplacement);
  STATIC_ASSERT((1 * kPointerSize) == ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT((0 * kPointerSize) == ExitFrameConstants::kCallerFPOffset);
  STATIC_ASSERT((-1 * kPointerSize) == ExitFrameConstants::kSPOffset);
  STATIC_ASSERT((-2 * kPointerSize) == ExitFrameConstants::kCodeOffset);

  // Save the frame pointer and context pointer in the top frame.
  Mov(scratch, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                         isolate())));
  Str(fp, MemOperand(scratch));
  Mov(scratch, Operand(ExternalReference(Isolate::kContextAddress,
                                         isolate())));
  Str(cp, MemOperand(scratch));

  STATIC_ASSERT((-2 * kPointerSize) ==
                ExitFrameConstants::kLastExitFrameField);
  if (save_doubles) {
    ExitFramePreserveFPRegs();
  }

  // Reserve space for the return address and for user requested memory.
  // We do this before aligning to make sure that we end up correctly
  // aligned with the minimum of wasted space.
  Claim(extra_space + 1, kXRegSizeInBytes);
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: Space reserved for SPOffset.
  //         fp[-16]: CodeObject()
  //         jssp[-16 - fp_size]: Saved doubles (if save_doubles is true).
  //         jssp[8]: Extra space reserved for caller (if extra_space != 0).
  // jssp -> jssp[0]: Space reserved for the return address.

  // Align and synchronize the system stack pointer with jssp.
  AlignAndSetCSPForFrame();
  ASSERT(csp.Is(StackPointer()));

  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: Space reserved for SPOffset.
  //         fp[-16]: CodeObject()
  //         csp[...]: Saved doubles, if saved_doubles is true.
  //         csp[8]: Memory reserved for the caller if extra_space != 0.
  //                 Alignment padding, if necessary.
  //  csp -> csp[0]: Space reserved for the return address.

  // ExitFrame::GetStateForFramePointer expects to find the return address at
  // the memory address immediately below the pointer stored in SPOffset.
  // It is not safe to derive much else from SPOffset, because the size of the
  // padding can vary.
  Add(scratch, csp, kXRegSizeInBytes);
  Str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


// Leave the current exit frame.
void MacroAssembler::LeaveExitFrame(bool restore_doubles,
                                    const Register& scratch,
                                    bool restore_context) {
  ASSERT(csp.Is(StackPointer()));

  if (restore_doubles) {
    ExitFrameRestoreFPRegs();
  }

  // Restore the context pointer from the top frame.
  if (restore_context) {
    Mov(scratch, Operand(ExternalReference(Isolate::kContextAddress,
                                           isolate())));
    Ldr(cp, MemOperand(scratch));
  }

  if (emit_debug_code()) {
    // Also emit debug code to clear the cp in the top frame.
    Mov(scratch, Operand(ExternalReference(Isolate::kContextAddress,
                                           isolate())));
    Str(xzr, MemOperand(scratch));
  }
  // Clear the frame pointer from the top frame.
  Mov(scratch, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                         isolate())));
  Str(xzr, MemOperand(scratch));

  // Pop the exit frame.
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[...]: The rest of the frame.
  Mov(jssp, fp);
  SetStackPointer(jssp);
  AssertStackConsistency();
  Pop(fp, lr);
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    Mov(scratch1, value);
    Mov(scratch2, Operand(ExternalReference(counter)));
    Str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value != 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Mov(scratch2, Operand(ExternalReference(counter)));
    Ldr(scratch1, MemOperand(scratch2));
    Add(scratch1, scratch1, value);
    Str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  IncrementCounter(counter, -value, scratch1, scratch2);
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    Ldr(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      Ldr(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in cp).
    Mov(dst, cp);
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  Mov(x0, 0);
  Mov(x1, Operand(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(1);
  ASSERT(AllowThisStubCall(&ces));
  Call(ces.GetCode(isolate()), RelocInfo::DEBUG_BREAK);
}
#endif


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  ASSERT(jssp.Is(StackPointer()));
  // Adjust this code if the asserts don't hold.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // For the JSEntry handler, we must preserve the live registers x0-x4.
  // (See JSEntryStub::GenerateBody().)

  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);

  // Set up the code object and the state for pushing.
  Mov(x10, Operand(CodeObject()));
  Mov(x11, state);

  // Push the frame pointer, context, state, and code object.
  if (kind == StackHandler::JS_ENTRY) {
    ASSERT(Smi::FromInt(0) == 0);
    Push(xzr, xzr, x11, x10);
  } else {
    Push(fp, cp, x11, x10);
  }

  // Link the current handler as the next handler.
  Mov(x11, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  Ldr(x10, MemOperand(x11));
  Push(x10);
  // Set this new handler as the current one.
  Str(jssp, MemOperand(x11));
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  Pop(x10);
  Mov(x11, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  Drop(StackHandlerConstants::kSize - kXRegSizeInBytes, kByteSizeInBytes);
  Str(x10, MemOperand(x11));
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT(object_size <= Page::kMaxRegularHeapObjectSize);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      // We apply salt to the original zap value to easily spot the values.
      Mov(result, (kDebugZapValue & ~0xffL) | 0x11L);
      Mov(scratch1, (kDebugZapValue & ~0xffL) | 0x21L);
      Mov(scratch2, (kDebugZapValue & ~0xffL) | 0x21L);
    }
    B(gc_required);
    return;
  }

  ASSERT(!AreAliased(result, scratch1, scratch2, Tmp0(), Tmp1()));
  ASSERT(result.Is64Bits() && scratch1.Is64Bits() && scratch2.Is64Bits() &&
         Tmp0().Is64Bits() && Tmp1().Is64Bits());

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  ASSERT(0 == (object_size & kObjectAlignmentMask));

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDP.
  ExternalReference heap_allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference heap_allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(heap_allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(heap_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register top_address = scratch1;
  Register allocation_limit = scratch2;
  Mov(top_address, Operand(heap_allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and the allocation limit.
    Ldp(result, allocation_limit, MemOperand(top_address));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      Ldr(Tmp0(), MemOperand(top_address));
      Cmp(result, Tmp0());
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load the allocation limit. 'result' already contains the allocation top.
    Ldr(allocation_limit, MemOperand(top_address, limit - top));
  }

  // We can ignore DOUBLE_ALIGNMENT flags here because doubles and pointers have
  // the same alignment on A64.
  STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);

  // Calculate new top and bail out if new space is exhausted.
  Adds(Tmp1(), result, object_size);
  B(vs, gc_required);
  Cmp(Tmp1(), allocation_limit);
  B(hi, gc_required);
  Str(Tmp1(), MemOperand(top_address));

  // Tag the object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    Orr(result, result, kHeapObjectTag);
  }
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      // We apply salt to the original zap value to easily spot the values.
      Mov(result, (kDebugZapValue & ~0xffL) | 0x11L);
      Mov(scratch1, (kDebugZapValue & ~0xffL) | 0x21L);
      Mov(scratch2, (kDebugZapValue & ~0xffL) | 0x21L);
    }
    B(gc_required);
    return;
  }

  ASSERT(!AreAliased(object_size, result, scratch1, scratch2, Tmp0(), Tmp1()));
  ASSERT(object_size.Is64Bits() && result.Is64Bits() && scratch1.Is64Bits() &&
         scratch2.Is64Bits() && Tmp0().Is64Bits() && Tmp1().Is64Bits());

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDP.
  ExternalReference heap_allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference heap_allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(heap_allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(heap_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register top_address = scratch1;
  Register allocation_limit = scratch2;
  Mov(top_address, Operand(heap_allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and the allocation limit.
    Ldp(result, allocation_limit, MemOperand(top_address));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      Ldr(Tmp0(), MemOperand(top_address));
      Cmp(result, Tmp0());
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load the allocation limit. 'result' already contains the allocation top.
    Ldr(allocation_limit, MemOperand(top_address, limit - top));
  }

  // We can ignore DOUBLE_ALIGNMENT flags here because doubles and pointers have
  // the same alignment on A64.
  STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);

  // Calculate new top and bail out if new space is exhausted
  if ((flags & SIZE_IN_WORDS) != 0) {
    Adds(Tmp1(), result, Operand(object_size, LSL, kPointerSizeLog2));
  } else {
    Adds(Tmp1(), result, object_size);
  }

  if (emit_debug_code()) {
    Tst(Tmp1(), kObjectAlignmentMask);
    Check(eq, kUnalignedAllocationInNewSpace);
  }

  B(vs, gc_required);
  Cmp(Tmp1(), allocation_limit);
  B(hi, gc_required);
  Str(Tmp1(), MemOperand(top_address));

  // Tag the object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    Orr(result, result, kHeapObjectTag);
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  Bic(object, object, kHeapObjectTagMask);
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  Mov(scratch, Operand(new_space_allocation_top));
  Ldr(scratch, MemOperand(scratch));
  Cmp(object, scratch);
  Check(lt, kUndoAllocationOfNonAllocatedMemory);
#endif
  // Write the address of the object to un-allocate as the current top.
  Mov(scratch, Operand(new_space_allocation_top));
  Str(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  ASSERT(!AreAliased(result, length, scratch1, scratch2, scratch3));
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  Add(scratch1, length, length);  // Length in bytes, not chars.
  Add(scratch1, scratch1, kObjectAlignmentMask + SeqTwoByteString::kHeaderSize);
  Bic(scratch1, scratch1, kObjectAlignmentMask);

  // Allocate two-byte string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  ASSERT(!AreAliased(result, length, scratch1, scratch2, scratch3));
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  STATIC_ASSERT(kCharSize == 1);
  Add(scratch1, length, kObjectAlignmentMask + SeqOneByteString::kHeaderSize);
  Bic(scratch1, scratch1, kObjectAlignmentMask);

  // Allocate ASCII string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kConsStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register length,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  Label allocate_new_space, install_map;
  AllocationFlags flags = TAG_OBJECT;

  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(isolate());
  Mov(scratch1, Operand(high_promotion_mode));
  Ldr(scratch1, MemOperand(scratch1));
  Cbz(scratch1, &allocate_new_space);

  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           static_cast<AllocationFlags>(flags | PRETENURE_OLD_POINTER_SPACE));

  B(&install_map);

  Bind(&allocate_new_space);
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           flags);

  Bind(&install_map);

  InitializeNewString(result,
                      length,
                      Heap::kConsAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  ASSERT(!AreAliased(result, length, scratch1, scratch2));
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  ASSERT(!AreAliased(result, length, scratch1, scratch2));
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Label* gc_required,
                                        Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Store heap number map in the allocated object.
  if (heap_number_map.Is(NoReg)) {
    heap_number_map = scratch1;
    LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  }
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  Str(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
}


void MacroAssembler::AllocateHeapNumberWithValue(Register result,
                                                 DoubleRegister value,
                                                 Label* gc_required,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Register heap_number_map) {
  // TODO(all): Check if it would be more efficient to use STP to store both
  // the map and the value.
  AllocateHeapNumber(result, gc_required, scratch1, scratch2, heap_number_map);
  Str(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}


void MacroAssembler::JumpIfObjectType(Register object,
                                      Register map,
                                      Register type_reg,
                                      InstanceType type,
                                      Label* if_cond_pass,
                                      Condition cond) {
  CompareObjectType(object, map, type_reg, type);
  B(cond, if_cond_pass);
}


void MacroAssembler::JumpIfNotObjectType(Register object,
                                         Register map,
                                         Register type_reg,
                                         InstanceType type,
                                         Label* if_not_object) {
  JumpIfObjectType(object, map, type_reg, type, if_not_object, ne);
}


// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareObjectType(Register object,
                                       Register map,
                                       Register type_reg,
                                       InstanceType type) {
  Ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, type_reg, type);
}


// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareInstanceType(Register map,
                                         Register type_reg,
                                         InstanceType type) {
  Ldrb(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Cmp(type_reg, type);
}


void MacroAssembler::CompareMap(Register obj,
                                Register scratch,
                                Handle<Map> map,
                                Label* early_success) {
  // TODO(jbramley): The early_success label isn't used. Remove it.
  Ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(scratch, map, early_success);
}


void MacroAssembler::CompareMap(Register obj_map,
                                Handle<Map> map,
                                Label* early_success) {
  // TODO(jbramley): The early_success label isn't used. Remove it.
  Cmp(obj_map, Operand(map));
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, scratch, map, &success);
  B(ne, fail);
  Bind(&success);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Heap::RootListIndex index,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  Ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  JumpIfNotRoot(scratch, index, fail);
}


void MacroAssembler::CheckMap(Register obj_map,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj_map, fail);
  }
  Label success;
  CompareMap(obj_map, map, &success);
  B(ne, fail);
  Bind(&success);
}


void MacroAssembler::DispatchMap(Register obj,
                                 Register scratch,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  Ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  Cmp(scratch, Operand(map));
  B(ne, &fail);
  Jump(success, RelocInfo::CODE_TARGET);
  Bind(&fail);
}


void MacroAssembler::TestMapBitfield(Register object, uint64_t mask) {
  Ldr(Tmp0(), FieldMemOperand(object, HeapObject::kMapOffset));
  Ldrb(Tmp0(), FieldMemOperand(Tmp0(), Map::kBitFieldOffset));
  Tst(Tmp0(), mask);
}


void MacroAssembler::LoadElementsKind(Register result, Register object) {
  // Load map.
  __ Ldr(result, FieldMemOperand(object, HeapObject::kMapOffset));
  // Load the map's "bit field 2".
  __ Ldrb(result, FieldMemOperand(result, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ Ubfx(result, result, Map::kElementsKindShift, Map::kElementsKindBitCount);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             BoundFunctionAction action) {
  ASSERT(!AreAliased(function, result, scratch));

  // Check that the receiver isn't a smi.
  JumpIfSmi(function, miss);

  // Check that the function really is a function. Load map into result reg.
  JumpIfNotObjectType(function, result, scratch, JS_FUNCTION_TYPE, miss);

  if (action == kMissOnBoundFunction) {
    Register scratch_w = scratch.W();
    Ldr(scratch,
        FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    // On 64-bit platforms, compiler hints field is not a smi. See definition of
    // kCompilerHintsOffset in src/objects.h.
    Ldr(scratch_w,
        FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    Tbnz(scratch, SharedFunctionInfo::kBoundFunction, miss);
  }

  // Make sure that the function has an instance prototype.
  Label non_instance;
  Ldrb(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  Tbnz(scratch, Map::kHasNonInstancePrototype, &non_instance);

  // Get the prototype or initial map from the function.
  Ldr(result,
      FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and simply
  // miss the cache instead. This will allow us to allocate a prototype object
  // on-demand in the runtime system.
  JumpIfRoot(result, Heap::kTheHoleValueRootIndex, miss);

  // If the function does not have an initial map, we're done.
  Label done;
  JumpIfNotObjectType(result, scratch, scratch, MAP_TYPE, &done);

  // Get the prototype from the initial map.
  Ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));
  B(&done);

  // Non-instance prototype: fetch prototype from constructor field in initial
  // map.
  Bind(&non_instance);
  Ldr(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  Bind(&done);
}


void MacroAssembler::CompareRoot(const Register& obj,
                                 Heap::RootListIndex index) {
  ASSERT(!AreAliased(obj, Tmp0()));
  LoadRoot(Tmp0(), index);
  Cmp(obj, Tmp0());
}


void MacroAssembler::JumpIfRoot(const Register& obj,
                                Heap::RootListIndex index,
                                Label* if_equal) {
  CompareRoot(obj, index);
  B(eq, if_equal);
}


void MacroAssembler::JumpIfNotRoot(const Register& obj,
                                   Heap::RootListIndex index,
                                   Label* if_not_equal) {
  CompareRoot(obj, index);
  B(ne, if_not_equal);
}


void MacroAssembler::CompareAndSplit(const Register& lhs,
                                     const Operand& rhs,
                                     Condition cond,
                                     Label* if_true,
                                     Label* if_false,
                                     Label* fall_through) {
  if ((if_true == if_false) && (if_false == fall_through)) {
    // Fall through.
  } else if (if_true == if_false) {
    B(if_true);
  } else if (if_false == fall_through) {
    CompareAndBranch(lhs, rhs, cond, if_true);
  } else if (if_true == fall_through) {
    CompareAndBranch(lhs, rhs, InvertCondition(cond), if_false);
  } else {
    CompareAndBranch(lhs, rhs, cond, if_true);
    B(if_false);
  }
}


void MacroAssembler::TestAndSplit(const Register& reg,
                                  uint64_t bit_pattern,
                                  Label* if_all_clear,
                                  Label* if_any_set,
                                  Label* fall_through) {
  if ((if_all_clear == if_any_set) && (if_any_set == fall_through)) {
    // Fall through.
  } else if (if_all_clear == if_any_set) {
    B(if_all_clear);
  } else if (if_all_clear == fall_through) {
    TestAndBranchIfAnySet(reg, bit_pattern, if_any_set);
  } else if (if_any_set == fall_through) {
    TestAndBranchIfAllClear(reg, bit_pattern, if_all_clear);
  } else {
    TestAndBranchIfAnySet(reg, bit_pattern, if_any_set);
    B(if_all_clear);
  }
}


void MacroAssembler::CheckFastElements(Register map,
                                       Register scratch,
                                       Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  Ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Cmp(scratch, Map::kMaximumBitField2FastHoleyElementValue);
  B(hi, fail);
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Register scratch,
                                             Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  Ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Cmp(scratch, Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  // If cond==ls, set cond=hi, otherwise compare.
  Ccmp(scratch,
       Operand(Map::kMaximumBitField2FastHoleyElementValue), CFlag, hi);
  B(hi, fail);
}


void MacroAssembler::CheckFastSmiElements(Register map,
                                          Register scratch,
                                          Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  Ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Cmp(scratch, Map::kMaximumBitField2FastHoleySmiElementValue);
  B(hi, fail);
}


// Note: The ARM version of this clobbers elements_reg, but this version does
// not. Some uses of this in A64 assume that elements_reg will be preserved.
void MacroAssembler::StoreNumberToDoubleElements(Register value_reg,
                                                 Register key_reg,
                                                 Register elements_reg,
                                                 Register scratch1,
                                                 FPRegister fpscratch1,
                                                 FPRegister fpscratch2,
                                                 Label* fail,
                                                 int elements_offset) {
  ASSERT(!AreAliased(value_reg, key_reg, elements_reg, scratch1));
  Label store_num;

  // Speculatively convert the smi to a double - all smis can be exactly
  // represented as a double.
  SmiUntagToDouble(fpscratch1, value_reg, kSpeculativeUntag);

  // If value_reg is a smi, we're done.
  JumpIfSmi(value_reg, &store_num);

  // Ensure that the object is a heap number.
  CheckMap(value_reg, scratch1, isolate()->factory()->heap_number_map(),
           fail, DONT_DO_SMI_CHECK);

  Ldr(fpscratch1, FieldMemOperand(value_reg, HeapNumber::kValueOffset));
  Fmov(fpscratch2, FixedDoubleArray::canonical_not_the_hole_nan_as_double());

  // Check for NaN by comparing the number to itself: NaN comparison will
  // report unordered, indicated by the overflow flag being set.
  Fcmp(fpscratch1, fpscratch1);
  Fcsel(fpscratch1, fpscratch2, fpscratch1, vs);

  // Store the result.
  Bind(&store_num);
  Add(scratch1, elements_reg,
      Operand::UntagSmiAndScale(key_reg, kDoubleSizeLog2));
  Str(fpscratch1,
      FieldMemOperand(scratch1,
                      FixedDoubleArray::kHeaderSize - elements_offset));
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key.  kArrayIndexValueMask has zeros in
  // the low kHashShift bits.
  STATIC_ASSERT(kSmiTag == 0);
  Ubfx(hash, hash, String::kHashShift, String::kArrayIndexValueBits);
  SmiTag(index, hash);
}


void MacroAssembler::EmitSeqStringSetCharCheck(
    Register string,
    Register index,
    SeqStringSetCharCheckIndexType index_type,
    Register scratch,
    uint32_t encoding_mask) {
  ASSERT(!AreAliased(string, index, scratch));

  if (index_type == kIndexIsSmi) {
    AssertSmi(index);
  }

  // Check that string is an object.
  AssertNotSmi(string, kNonObject);

  // Check that string has an appropriate map.
  Ldr(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
  Ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

  And(scratch, scratch, kStringRepresentationMask | kStringEncodingMask);
  Cmp(scratch, encoding_mask);
  Check(eq, kUnexpectedStringType);

  Ldr(scratch, FieldMemOperand(string, String::kLengthOffset));
  Cmp(index, index_type == kIndexIsSmi ? scratch : Operand::UntagSmi(scratch));
  Check(lt, kIndexIsTooLarge);

  ASSERT_EQ(0, Smi::FromInt(0));
  Cmp(index, 0);
  Check(ge, kIndexIsNegative);
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  // TODO(jbramley): Sort out the uses of Tmp0() and Tmp1() in this function.
  // The ARM version takes two scratch registers, and that should be enough for
  // all of the checks.

  Label same_contexts;

  ASSERT(!AreAliased(holder_reg, scratch));

  // Load current lexical context from the stack frame.
  Ldr(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  Cmp(scratch, 0);
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  Ldr(scratch, FieldMemOperand(scratch, offset));
  Ldr(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Read the first word and compare to the global_context_map.
    Register temp = Tmp1();
    Ldr(temp, FieldMemOperand(scratch, HeapObject::kMapOffset));
    CompareRoot(temp, Heap::kNativeContextMapRootIndex);
    Check(eq, kExpectedNativeContext);
  }

  // Check if both contexts are the same.
  ldr(Tmp0(), FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  cmp(scratch, Tmp0());
  b(&same_contexts, eq);

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Move Tmp0() into a different register, as CompareRoot will use it.
    Register temp = Tmp1();
    mov(temp, Tmp0());
    CompareRoot(temp, Heap::kNullValueRootIndex);
    Check(ne, kExpectedNonNullContext);

    Ldr(temp, FieldMemOperand(temp, HeapObject::kMapOffset));
    CompareRoot(temp, Heap::kNativeContextMapRootIndex);
    Check(eq, kExpectedNativeContext);

    // Let's consider that Tmp0() has been cloberred by the MacroAssembler.
    // We reload it with its value.
    ldr(Tmp0(), FieldMemOperand(holder_reg,
                                JSGlobalProxy::kNativeContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  ldr(scratch, FieldMemOperand(scratch, token_offset));
  ldr(Tmp0(), FieldMemOperand(Tmp0(), token_offset));
  cmp(scratch, Tmp0());
  b(miss, ne);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key. This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericElementStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register key, Register scratch) {
  ASSERT(!AreAliased(key, scratch));

  // Xor original key with a seed.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  Eor(key, key, Operand::UntagSmi(scratch));

  // The algorithm uses 32-bit integer values.
  key = key.W();
  scratch = scratch.W();

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash <<1 15);
  Mvn(scratch, key);
  Add(key, scratch, Operand(key, LSL, 15));
  // hash = hash ^ (hash >> 12);
  Eor(key, key, Operand(key, LSR, 12));
  // hash = hash + (hash << 2);
  Add(key, key, Operand(key, LSL, 2));
  // hash = hash ^ (hash >> 4);
  Eor(key, key, Operand(key, LSR, 4));
  // hash = hash * 2057;
  Mov(scratch, Operand(key, LSL, 11));
  Add(key, key, Operand(key, LSL, 3));
  Add(key, key, scratch);
  // hash = hash ^ (hash >> 16);
  Eor(key, key, Operand(key, LSR, 16));
}


void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register result,
                                              Register scratch0,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3) {
  ASSERT(!AreAliased(elements, key, scratch0, scratch1, scratch2, scratch3));

  Label done;

  SmiUntag(scratch0, key);
  GetNumberHash(scratch0, scratch1);

  // Compute the capacity mask.
  Ldrsw(scratch1,
        UntagSmiFieldMemOperand(elements,
                                SeededNumberDictionary::kCapacityOffset));
  Sub(scratch1, scratch1, 1);

  // Generate an unrolled loop that performs a few probes before giving up.
  for (int i = 0; i < kNumberDictionaryProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      Add(scratch2, scratch0, SeededNumberDictionary::GetProbeOffset(i));
    } else {
      Mov(scratch2, scratch0);
    }
    And(scratch2, scratch2, scratch1);

    // Scale the index by multiplying by the element size.
    ASSERT(SeededNumberDictionary::kEntrySize == 3);
    Add(scratch2, scratch2, Operand(scratch2, LSL, 1));

    // Check if the key is identical to the name.
    Add(scratch2, elements, Operand(scratch2, LSL, kPointerSizeLog2));
    Ldr(scratch3,
        FieldMemOperand(scratch2,
                        SeededNumberDictionary::kElementsStartOffset));
    Cmp(key, scratch3);
    if (i != (kNumberDictionaryProbes - 1)) {
      B(eq, &done);
    } else {
      B(ne, miss);
    }
  }

  Bind(&done);
  // Check that the value is a normal property.
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  Ldrsw(scratch1, UntagSmiFieldMemOperand(scratch2, kDetailsOffset));
  TestAndBranchIfAnySet(scratch1, PropertyDetails::TypeField::kMask, miss);

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  Ldr(result, FieldMemOperand(scratch2, kValueOffset));
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address,
                                         Register scratch,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  ASSERT(!AreAliased(object, address, scratch));
  Label done, store_buffer_overflow;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, &ok);
    Abort(kRememberedSetPointerInNewSpace);
    bind(&ok);
  }
  // Load store buffer top.
  Mov(Tmp0(), Operand(ExternalReference::store_buffer_top(isolate())));
  Ldr(scratch, MemOperand(Tmp0()));
  // Store pointer to buffer and increment buffer top.
  Str(address, MemOperand(scratch, kPointerSize, PostIndex));
  // Write back new top of buffer.
  Str(scratch, MemOperand(Tmp0()));
  // Call stub on end of buffer.
  // Check for end of buffer.
  ASSERT(StoreBuffer::kStoreBufferOverflowBit ==
         (1 << (14 + kPointerSizeLog2)));
  if (and_then == kFallThroughAtEnd) {
    Tbz(scratch, (14 + kPointerSizeLog2), &done);
  } else {
    ASSERT(and_then == kReturnAtEnd);
    Tbnz(scratch, (14 + kPointerSizeLog2), &store_buffer_overflow);
    Ret();
  }

  Bind(&store_buffer_overflow);
  Push(lr);
  StoreBufferOverflowStub store_buffer_overflow_stub =
      StoreBufferOverflowStub(fp_mode);
  CallStub(&store_buffer_overflow_stub);
  Pop(lr);

  Bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  PopXRegList(kSafepointSavedRegisters);
  Drop(num_unsaved);
}


void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the stack, so
  // adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  ASSERT(num_unsaved >= 0);
  Claim(num_unsaved);
  PushXRegList(kSafepointSavedRegisters);
}


void MacroAssembler::PushSafepointFPRegisters() {
  PushCPURegList(CPURegList(CPURegister::kFPRegister, kDRegSize,
                            FPRegister::kAllocatableFPRegisters));
}


void MacroAssembler::PopSafepointFPRegisters() {
  PopCPURegList(CPURegList(CPURegister::kFPRegister, kDRegSize,
                           FPRegister::kAllocatableFPRegisters));
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // Make sure the safepoint registers list is what we expect.
  ASSERT(CPURegList::GetSafepointSavedRegisters().list() == 0x6ffcffff);

  // Safepoint registers are stored contiguously on the stack, but not all the
  // registers are saved. The following registers are excluded:
  //  - x16 and x17 (ip0 and ip1) because they shouldn't be preserved outside of
  //    the macro assembler.
  //  - x28 (jssp) because JS stack pointer doesn't need to be included in
  //    safepoint registers.
  //  - x31 (csp) because the system stack pointer doesn't need to be included
  //    in safepoint registers.
  //
  // This function implements the mapping of register code to index into the
  // safepoint register slots.
  if ((reg_code >= 0) && (reg_code <= 15)) {
    return reg_code;
  } else if ((reg_code >= 18) && (reg_code <= 27)) {
    // Skip ip0 and ip1.
    return reg_code - 2;
  } else if ((reg_code == 29) || (reg_code == 30)) {
    // Also skip jssp.
    return reg_code - 3;
  } else {
    // This register has no safepoint register slot.
    UNREACHABLE();
    return -1;
  }
}


void MacroAssembler::CheckPageFlagSet(const Register& object,
                                      const Register& scratch,
                                      int mask,
                                      Label* if_any_set) {
  And(scratch, object, ~Page::kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  TestAndBranchIfAnySet(scratch, mask, if_any_set);
}


void MacroAssembler::CheckPageFlagClear(const Register& object,
                                        const Register& scratch,
                                        int mask,
                                        Label* if_all_clear) {
  And(scratch, object, ~Page::kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  TestAndBranchIfAllClear(scratch, mask, if_all_clear);
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register scratch,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kPointerSize.
  ASSERT(IsAligned(offset, kPointerSize));

  Add(scratch, object, offset - kHeapObjectTag);
  if (emit_debug_code()) {
    Label ok;
    Tst(scratch, (1 << kPointerSizeLog2) - 1);
    B(eq, &ok);
    Abort(kUnalignedCellInWriteBarrier);
    Bind(&ok);
  }

  RecordWrite(object,
              scratch,
              value,
              lr_status,
              save_fp,
              remembered_set_action,
              OMIT_SMI_CHECK);

  Bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(value, Operand(BitCast<int64_t>(kZapValue + 4)));
    Mov(scratch, Operand(BitCast<int64_t>(kZapValue + 8)));
  }
}


// Will clobber: object, address, value, Tmp0(), Tmp1().
// If lr_status is kLRHasBeenSaved, lr will also be clobbered.
//
// The register 'object' contains a heap object pointer. The heap object tag is
// shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value,
                                 LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASM_LOCATION("MacroAssembler::RecordWrite");
  ASSERT(!AreAliased(object, value));

  if (emit_debug_code()) {
    Ldr(Tmp0(), MemOperand(address));
    Cmp(Tmp0(), value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  // TODO(mstarzinger): Dynamic counter missing.

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    ASSERT_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  CheckPageFlagClear(value,
                     value,  // Used as scratch.
                     MemoryChunk::kPointersToHereAreInterestingMask,
                     &done);
  CheckPageFlagClear(object,
                     value,  // Used as scratch.
                     MemoryChunk::kPointersFromHereAreInterestingMask,
                     &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push(lr);
  }
  RecordWriteStub stub(object, value, address, remembered_set_action, fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    Pop(lr);
  }

  Bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(address, Operand(BitCast<int64_t>(kZapValue + 12)));
    Mov(value, Operand(BitCast<int64_t>(kZapValue + 16)));
  }
}


void MacroAssembler::AssertHasValidColor(const Register& reg) {
  if (emit_debug_code()) {
    // The bit sequence is backward. The first character in the string
    // represents the least significant bit.
    ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

    Label color_is_valid;
    Tbnz(reg, 0, &color_is_valid);
    Tbz(reg, 1, &color_is_valid);
    Abort(kUnexpectedColorFound);
    Bind(&color_is_valid);
  }
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register shift_reg) {
  ASSERT(!AreAliased(addr_reg, bitmap_reg, shift_reg, no_reg));
  // addr_reg is divided into fields:
  // |63        page base        20|19    high      8|7   shift   3|2  0|
  // 'high' gives the index of the cell holding color bits for the object.
  // 'shift' gives the offset in the cell for this object's color.
  const int kShiftBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  Ubfx(Tmp0(), addr_reg, kShiftBits, kPageSizeBits - kShiftBits);
  Bic(bitmap_reg, addr_reg, Page::kPageAlignmentMask);
  Add(bitmap_reg, bitmap_reg, Operand(Tmp0(), LSL, Bitmap::kBytesPerCellLog2));
  // bitmap_reg:
  // |63        page base        20|19 zeros 15|14      high      3|2  0|
  Ubfx(shift_reg, addr_reg, kPointerSizeLog2, Bitmap::kBitsPerCellLog2);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register shift_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  // See mark-compact.h for color definitions.
  ASSERT(!AreAliased(object, bitmap_scratch, shift_scratch));

  GetMarkBits(object, bitmap_scratch, shift_scratch);
  Ldr(bitmap_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Shift the bitmap down to get the color of the object in bits [1:0].
  Lsr(bitmap_scratch, bitmap_scratch, shift_scratch);

  AssertHasValidColor(bitmap_scratch);

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);

  // Check for the color.
  if (first_bit == 0) {
    // Checking for white.
    ASSERT(second_bit == 0);
    // We only need to test the first bit.
    Tbz(bitmap_scratch, 0, has_color);
  } else {
    Label other_color;
    // Checking for grey or black.
    Tbz(bitmap_scratch, 0, &other_color);
    if (second_bit == 0) {
      Tbz(bitmap_scratch, 1, has_color);
    } else {
      Tbnz(bitmap_scratch, 1, has_color);
    }
    Bind(&other_color);
  }

  // Fall through if it does not have the right color.
}


void MacroAssembler::CheckMapDeprecated(Handle<Map> map,
                                        Register scratch,
                                        Label* if_deprecated) {
  if (map->CanBeDeprecated()) {
    Mov(scratch, Operand(map));
    Ldrsw(scratch, UntagSmiFieldMemOperand(scratch, Map::kBitField3Offset));
    TestAndBranchIfAnySet(scratch, Map::Deprecated::kMask, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(
    Register object,
    Register scratch0,
    Register scratch1,
    Label* found) {
  ASSERT(!AreAliased(object, scratch0, scratch1));
  Factory* factory = isolate()->factory();
  Register current = scratch0;
  Label loop_again;

  // Scratch contains elements pointer.
  Mov(current, object);

  // Loop based on the map going up the prototype chain.
  Bind(&loop_again);
  Ldr(current, FieldMemOperand(current, HeapObject::kMapOffset));
  Ldrb(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  Ubfx(scratch1, scratch1, Map::kElementsKindShift, Map::kElementsKindBitCount);
  CompareAndBranch(scratch1, DICTIONARY_ELEMENTS, eq, found);
  Ldr(current, FieldMemOperand(current, Map::kPrototypeOffset));
  CompareAndBranch(current, Operand(factory->null_value()), ne, &loop_again);
}


void MacroAssembler::GetRelocatedValueLocation(Register ldr_location,
                                               Register result) {
  ASSERT(!result.Is(ldr_location));
  const uint32_t kLdrLitOffset_lsb = 5;
  const uint32_t kLdrLitOffset_width = 19;
  Ldr(result, MemOperand(ldr_location));
  if (emit_debug_code()) {
    And(result, result, LoadLiteralFMask);
    Cmp(result, LoadLiteralFixed);
    Check(eq, kTheInstructionToPatchShouldBeAnLdrLiteral);
    // The instruction was clobbered. Reload it.
    Ldr(result, MemOperand(ldr_location));
  }
  Sbfx(result, result, kLdrLitOffset_lsb, kLdrLitOffset_width);
  Add(result, ldr_location, Operand(result, LSL, kWordSizeInBytesLog2));
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register shift_scratch,
    Register load_scratch,
    Register length_scratch,
    Label* value_is_white_and_not_data) {
  ASSERT(!AreAliased(
      value, bitmap_scratch, shift_scratch, load_scratch, length_scratch));

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);

  GetMarkBits(value, bitmap_scratch, shift_scratch);
  Ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  Lsr(load_scratch, load_scratch, shift_scratch);

  AssertHasValidColor(load_scratch);

  // If the value is black or grey we don't need to do anything.
  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  Label done;
  Tbnz(load_scratch, 0, &done);

  // Value is white.  We check whether it is data that doesn't need scanning.
  Register map = load_scratch;  // Holds map while checking type.
  Label is_data_object;

  // Check for heap-number.
  Ldr(map, FieldMemOperand(value, HeapObject::kMapOffset));
  Mov(length_scratch, HeapNumber::kSize);
  JumpIfRoot(map, Heap::kHeapNumberMapRootIndex, &is_data_object);

  // Check for strings.
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = load_scratch;
  Ldrb(instance_type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  TestAndBranchIfAnySet(instance_type,
                        kIsIndirectStringMask | kIsNotStringMask,
                        value_is_white_and_not_data);

  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  ASSERT_EQ(0, kSeqStringTag & kExternalStringTag);
  ASSERT_EQ(0, kConsStringTag & kExternalStringTag);
  Mov(length_scratch, ExternalString::kSize);
  TestAndBranchIfAnySet(instance_type, kExternalStringTag, &is_data_object);

  // Sequential string, either ASCII or UC16.
  // For ASCII (char-size of 1) we shift the smi tag away to get the length.
  // For UC16 (char-size of 2) we just leave the smi tag in place, thereby
  // getting the length multiplied by 2.
  ASSERT(kOneByteStringTag == 4 && kStringEncodingMask == 4);
  Ldrsw(length_scratch, UntagSmiFieldMemOperand(value,
                                                String::kLengthOffset));
  Tst(instance_type, kStringEncodingMask);
  Cset(load_scratch, eq);
  Lsl(length_scratch, length_scratch, load_scratch);
  Add(length_scratch,
      length_scratch,
      SeqString::kHeaderSize + kObjectAlignmentMask);
  Bic(length_scratch, length_scratch, kObjectAlignmentMask);

  Bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  Register mask = shift_scratch;
  Mov(load_scratch, 1);
  Lsl(mask, load_scratch, shift_scratch);

  Ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  Orr(load_scratch, load_scratch, mask);
  Str(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));

  Bic(bitmap_scratch, bitmap_scratch, Page::kPageAlignmentMask);
  Ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
  Add(load_scratch, load_scratch, length_scratch);
  Str(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));

  Bind(&done);
}


void MacroAssembler::Assert(Condition cond, BailoutReason reason) {
  if (emit_debug_code()) {
    Check(cond, reason);
  }
}



void MacroAssembler::AssertRegisterIsClear(Register reg, BailoutReason reason) {
  if (emit_debug_code()) {
    CheckRegisterIsClear(reg, reason);
  }
}


void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index,
                                          BailoutReason reason) {
  // CompareRoot uses Tmp0().
  ASSERT(!reg.Is(Tmp0()));
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, reason);
  }
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    Register temp = Tmp1();
    Label ok;
    Ldr(temp, FieldMemOperand(elements, HeapObject::kMapOffset));
    JumpIfRoot(temp, Heap::kFixedArrayMapRootIndex, &ok);
    JumpIfRoot(temp, Heap::kFixedDoubleArrayMapRootIndex, &ok);
    JumpIfRoot(temp, Heap::kFixedCOWArrayMapRootIndex, &ok);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    Bind(&ok);
  }
}


void MacroAssembler::AssertIsString(const Register& object) {
  if (emit_debug_code()) {
    Register temp = Tmp1();
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsNotAString);
    Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(temp, temp, FIRST_NONSTRING_TYPE);
    Check(lo, kOperandIsNotAString);
  }
}


void MacroAssembler::Check(Condition cond, BailoutReason reason) {
  Label ok;
  B(cond, &ok);
  Abort(reason);
  // Will not return here.
  Bind(&ok);
}


void MacroAssembler::CheckRegisterIsClear(Register reg, BailoutReason reason) {
  Label ok;
  Cbz(reg, &ok);
  Abort(reason);
  // Will not return here.
  Bind(&ok);
}


void MacroAssembler::Abort(BailoutReason reason) {
#ifdef DEBUG
  RecordComment("Abort message: ");
  RecordComment(GetBailoutReason(reason));

  if (FLAG_trap_on_abort) {
    Brk(0);
    return;
  }
#endif

  // Abort is used in some contexts where csp is the stack pointer. In order to
  // simplify the CallRuntime code, make sure that jssp is the stack pointer.
  // There is no risk of register corruption here because Abort doesn't return.
  Register old_stack_pointer = StackPointer();
  SetStackPointer(jssp);
  Mov(jssp, old_stack_pointer);

  if (use_real_aborts()) {
    Mov(x0, Operand(Smi::FromInt(reason)));
    Push(x0);

    if (!has_frame_) {
      // We don't actually want to generate a pile of code for this, so just
      // claim there is a stack frame, without generating one.
      FrameScope scope(this, StackFrame::NONE);
      CallRuntime(Runtime::kAbort, 1);
    } else {
      CallRuntime(Runtime::kAbort, 1);
    }
  } else {
    // Load the string to pass to Printf.
    Label msg_address;
    Adr(x0, &msg_address);

    // Call Printf directly to report the error.
    CallPrintf();

    // We need a way to stop execution on both the simulator and real hardware,
    // and Unreachable() is the best option.
    Unreachable();

    // Emit the message string directly in the instruction stream.
    {
      BlockConstPoolScope scope(this);
      Bind(&msg_address);
      EmitStringData(GetBailoutReason(reason));
    }
  }

  SetStackPointer(old_stack_pointer);
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  Ldr(scratch, GlobalObjectMemOperand());
  Ldr(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  Ldr(scratch, ContextMemOperand(scratch, Context::JS_ARRAY_MAPS_INDEX));
  size_t offset = (expected_kind * kPointerSize) + FixedArrayBase::kHeaderSize;
  Ldr(Tmp0(), FieldMemOperand(scratch, offset));
  Cmp(map_in_out, Tmp0());
  B(ne, no_map_match);

  // Use the transitioned cached map.
  offset = (transitioned_kind * kPointerSize) + FixedArrayBase::kHeaderSize;
  Ldr(map_in_out, FieldMemOperand(scratch, offset));
}


void MacroAssembler::LoadInitialArrayMap(Register function_in,
                                         Register scratch,
                                         Register map_out,
                                         ArrayHasHoles holes) {
  ASSERT(!AreAliased(function_in, scratch, map_out));
  Label done;
  Ldr(map_out, FieldMemOperand(function_in,
                               JSFunction::kPrototypeOrInitialMapOffset));

  if (!FLAG_smi_only_arrays) {
    ElementsKind kind = (holes == kArrayCanHaveHoles) ? FAST_HOLEY_ELEMENTS
                                                      : FAST_ELEMENTS;
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS, kind, map_out,
                                        scratch, &done);
  } else if (holes == kArrayCanHaveHoles) {
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                        FAST_HOLEY_SMI_ELEMENTS, map_out,
                                        scratch, &done);
  }
  Bind(&done);
}


void MacroAssembler::LoadArrayFunction(Register function) {
  // Load the global or builtins object from the current context.
  Ldr(function, GlobalObjectMemOperand());
  // Load the global context from the global or builtins object.
  Ldr(function,
      FieldMemOperand(function, GlobalObject::kGlobalContextOffset));
  // Load the array function from the native context.
  Ldr(function, ContextMemOperand(function, Context::ARRAY_FUNCTION_INDEX));
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  Ldr(function, GlobalObjectMemOperand());
  // Load the native context from the global or builtins object.
  Ldr(function, FieldMemOperand(function,
                                GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  Ldr(function, ContextMemOperand(function, index));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  Ldr(map, FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    B(&ok);
    Bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    Bind(&ok);
  }
}


// This is the main Printf implementation. All other Printf variants call
// PrintfNoPreserve after setting up one or more PreserveRegisterScopes.
void MacroAssembler::PrintfNoPreserve(const char * format,
                                      const CPURegister& arg0,
                                      const CPURegister& arg1,
                                      const CPURegister& arg2,
                                      const CPURegister& arg3) {
  // We cannot handle a caller-saved stack pointer. It doesn't make much sense
  // in most cases anyway, so this restriction shouldn't be too serious.
  ASSERT(!kCallerSaved.IncludesAliasOf(__ StackPointer()));

  // We cannot print Tmp0() or Tmp1() as they're used internally by the macro
  // assembler. We cannot print the stack pointer because it is typically used
  // to preserve caller-saved registers (using other Printf variants which
  // depend on this helper).
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg0));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg1));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg2));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg3));

  static const int kMaxArgCount = 4;
  // Assume that we have the maximum number of arguments until we know
  // otherwise.
  int arg_count = kMaxArgCount;

  // The provided arguments.
  CPURegister args[kMaxArgCount] = {arg0, arg1, arg2, arg3};

  // The PCS registers where the arguments need to end up.
  CPURegister pcs[kMaxArgCount] = {NoCPUReg, NoCPUReg, NoCPUReg, NoCPUReg};

  // Promote FP arguments to doubles, and integer arguments to X registers.
  // Note that FP and integer arguments cannot be mixed, but we'll check
  // AreSameSizeAndType once we've processed these promotions.
  for (int i = 0; i < kMaxArgCount; i++) {
    if (args[i].IsRegister()) {
      // Note that we use x1 onwards, because x0 will hold the format string.
      pcs[i] = Register::XRegFromCode(i + 1);
      // For simplicity, we handle all integer arguments as X registers. An X
      // register argument takes the same space as a W register argument in the
      // PCS anyway. The only limitation is that we must explicitly clear the
      // top word for W register arguments as the callee will expect it to be
      // clear.
      if (!args[i].Is64Bits()) {
        const Register& as_x = args[i].X();
        And(as_x, as_x, 0x00000000ffffffff);
        args[i] = as_x;
      }
    } else if (args[i].IsFPRegister()) {
      pcs[i] = FPRegister::DRegFromCode(i);
      // C and C++ varargs functions (such as printf) implicitly promote float
      // arguments to doubles.
      if (!args[i].Is64Bits()) {
        FPRegister s(args[i]);
        const FPRegister& as_d = args[i].D();
        Fcvt(as_d, s);
        args[i] = as_d;
      }
    } else {
      // This is the first empty (NoCPUReg) argument, so use it to set the
      // argument count and bail out.
      arg_count = i;
      break;
    }
  }
  ASSERT((arg_count >= 0) && (arg_count <= kMaxArgCount));
  // Check that every remaining argument is NoCPUReg.
  for (int i = arg_count; i < kMaxArgCount; i++) {
    ASSERT(args[i].IsNone());
  }
  ASSERT((arg_count == 0) || AreSameSizeAndType(args[0], args[1],
                                                args[2], args[3],
                                                pcs[0], pcs[1],
                                                pcs[2], pcs[3]));

  // Move the arguments into the appropriate PCS registers.
  //
  // Arranging an arbitrary list of registers into x1-x4 (or d0-d3) is
  // surprisingly complicated.
  //
  //  * For even numbers of registers, we push the arguments and then pop them
  //    into their final registers. This maintains 16-byte stack alignment in
  //    case csp is the stack pointer, since we're only handling X or D
  //    registers at this point.
  //
  //  * For odd numbers of registers, we push and pop all but one register in
  //    the same way, but the left-over register is moved directly, since we
  //    can always safely move one register without clobbering any source.
  if (arg_count >= 4) {
    Push(args[3], args[2], args[1], args[0]);
  } else if (arg_count >= 2) {
    Push(args[1], args[0]);
  }

  if ((arg_count % 2) != 0) {
    // Move the left-over register directly.
    const CPURegister& leftover_arg = args[arg_count - 1];
    const CPURegister& leftover_pcs = pcs[arg_count - 1];
    if (leftover_arg.IsRegister()) {
      Mov(Register(leftover_pcs), Register(leftover_arg));
    } else {
      Fmov(FPRegister(leftover_pcs), FPRegister(leftover_arg));
    }
  }

  if (arg_count >= 4) {
    Pop(pcs[0], pcs[1], pcs[2], pcs[3]);
  } else if (arg_count >= 2) {
    Pop(pcs[0], pcs[1]);
  }

  // Load the format string into x0, as per the procedure-call standard.
  //
  // To make the code as portable as possible, the format string is encoded
  // directly in the instruction stream. It might be cleaner to encode it in a
  // literal pool, but since Printf is usually used for debugging, it is
  // beneficial for it to be minimally dependent on other features.
  Label format_address;
  Adr(x0, &format_address);

  // Emit the format string directly in the instruction stream.
  { BlockConstPoolScope scope(this);
    Label after_data;
    B(&after_data);
    Bind(&format_address);
    EmitStringData(format);
    Unreachable();
    Bind(&after_data);
  }

  // We don't pass any arguments on the stack, but we still need to align the C
  // stack pointer to a 16-byte boundary for PCS compliance.
  if (!csp.Is(StackPointer())) {
    Bic(csp, StackPointer(), 0xf);
  }

  CallPrintf(pcs[0].type());
}


void MacroAssembler::CallPrintf(CPURegister::RegisterType type) {
  // A call to printf needs special handling for the simulator, since the system
  // printf function will use a different instruction set and the procedure-call
  // standard will not be compatible.
#ifdef USE_SIMULATOR
  { InstructionAccurateScope scope(this, kPrintfLength / kInstructionSize);
    hlt(kImmExceptionIsPrintf);
    dc32(type);
  }
#else
  Call(FUNCTION_ADDR(printf), RelocInfo::EXTERNAL_REFERENCE);
#endif
}


void MacroAssembler::Printf(const char * format,
                            const CPURegister& arg0,
                            const CPURegister& arg1,
                            const CPURegister& arg2,
                            const CPURegister& arg3) {
  // Preserve all caller-saved registers as well as NZCV.
  // If csp is the stack pointer, PushCPURegList asserts that the size of each
  // list is a multiple of 16 bytes.
  PushCPURegList(kCallerSaved);
  PushCPURegList(kCallerSavedFP);
  // Use Tmp0() as a scratch register. It is not accepted by Printf so it will
  // never overlap an argument register.
  Mrs(Tmp0(), NZCV);
  Push(Tmp0(), xzr);

  PrintfNoPreserve(format, arg0, arg1, arg2, arg3);

  Pop(xzr, Tmp0());
  Msr(NZCV, Tmp0());
  PopCPURegList(kCallerSavedFP);
  PopCPURegList(kCallerSaved);
}


void MacroAssembler::EmitFrameSetupForCodeAgePatching() {
  // TODO(jbramley): Other architectures use the internal memcpy to copy the
  // sequence. If this is a performance bottleneck, we should consider caching
  // the sequence and copying it in the same way.
  InstructionAccurateScope scope(this, kCodeAgeSequenceSize / kInstructionSize);
  ASSERT(jssp.Is(StackPointer()));
  EmitFrameSetupForCodeAgePatching(this);
}



void MacroAssembler::EmitCodeAgeSequence(Code* stub) {
  InstructionAccurateScope scope(this, kCodeAgeSequenceSize / kInstructionSize);
  ASSERT(jssp.Is(StackPointer()));
  EmitCodeAgeSequence(this, stub);
}


#undef __
#define __ assm->


void MacroAssembler::EmitFrameSetupForCodeAgePatching(Assembler * assm) {
  Label start;
  __ bind(&start);

  // We can do this sequence using four instructions, but the code ageing
  // sequence that patches it needs five, so we use the extra space to try to
  // simplify some addressing modes and remove some dependencies (compared to
  // using two stp instructions with write-back).
  __ sub(jssp, jssp, 4 * kXRegSizeInBytes);
  __ sub(csp, csp, 4 * kXRegSizeInBytes);
  __ stp(x1, cp, MemOperand(jssp, 0 * kXRegSizeInBytes));
  __ stp(fp, lr, MemOperand(jssp, 2 * kXRegSizeInBytes));
  __ add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);

  __ AssertSizeOfCodeGeneratedSince(&start, kCodeAgeSequenceSize);
}


void MacroAssembler::EmitCodeAgeSequence(Assembler * assm,
                                         Code * stub) {
  Label start;
  __ bind(&start);
  // When the stub is called, the sequence is replaced with the young sequence
  // (as in EmitFrameSetupForCodeAgePatching). After the code is replaced, the
  // stub jumps to &start, stored in x0. The young sequence does not call the
  // stub so there is no infinite loop here.
  //
  // A branch (br) is used rather than a call (blr) because this code replaces
  // the frame setup code that would normally preserve lr.
  __ LoadLiteral(ip0, kCodeAgeStubEntryOffset);
  __ adr(x0, &start);
  __ br(ip0);
  // IsCodeAgeSequence in codegen-a64.cc assumes that the code generated up
  // until now (kCodeAgeStubEntryOffset) is the same for all code age sequences.
  __ AssertSizeOfCodeGeneratedSince(&start, kCodeAgeStubEntryOffset);
  if (stub) {
    __ dc64(reinterpret_cast<uint64_t>(stub->instruction_start()));
    __ AssertSizeOfCodeGeneratedSince(&start, kCodeAgeSequenceSize);
  }
}


bool MacroAssembler::IsYoungSequence(byte* sequence) {
  // Generate a young sequence to compare with.
  const int length = kCodeAgeSequenceSize / kInstructionSize;
  static bool initialized = false;
  static byte young[kCodeAgeSequenceSize];
  if (!initialized) {
    PatchingAssembler patcher(young, length);
    // The young sequence is the frame setup code for FUNCTION code types. It is
    // generated by FullCodeGenerator::Generate.
    MacroAssembler::EmitFrameSetupForCodeAgePatching(&patcher);
    initialized = true;
  }

  bool is_young = (memcmp(sequence, young, kCodeAgeSequenceSize) == 0);
  ASSERT(is_young || IsCodeAgeSequence(sequence));
  return is_young;
}


#ifdef DEBUG
bool MacroAssembler::IsCodeAgeSequence(byte* sequence) {
  // The old sequence varies depending on the code age. However, the code up
  // until kCodeAgeStubEntryOffset does not change, so we can check that part to
  // get a reasonable level of verification.
  const int length = kCodeAgeStubEntryOffset / kInstructionSize;
  static bool initialized = false;
  static byte old[kCodeAgeStubEntryOffset];
  if (!initialized) {
    PatchingAssembler patcher(old, length);
    MacroAssembler::EmitCodeAgeSequence(&patcher, NULL);
    initialized = true;
  }
  return memcmp(sequence, old, kCodeAgeStubEntryOffset) == 0;
}
#endif


#undef __
#define __ masm->


void InlineSmiCheckInfo::Emit(MacroAssembler* masm, const Register& reg,
                              const Label* smi_check) {
  Assembler::BlockConstPoolScope scope(masm);
  if (reg.IsValid()) {
    ASSERT(smi_check->is_bound());
    ASSERT(reg.Is64Bits());

    // Encode the register (x0-x30) in the lowest 5 bits, then the offset to
    // 'check' in the other bits. The possible offset is limited in that we
    // use BitField to pack the data, and the underlying data type is a
    // uint32_t.
    uint32_t delta = __ InstructionsGeneratedSince(smi_check);
    __ InlineData(RegisterBits::encode(reg.code()) | DeltaBits::encode(delta));
  } else {
    ASSERT(!smi_check->is_bound());

    // An offset of 0 indicates that there is no patch site.
    __ InlineData(0);
  }
}


InlineSmiCheckInfo::InlineSmiCheckInfo(Address info)
    : reg_(NoReg), smi_check_(NULL) {
  InstructionSequence* inline_data = InstructionSequence::At(info);
  ASSERT(inline_data->IsInlineData());
  if (inline_data->IsInlineData()) {
    uint64_t payload = inline_data->InlineData();
    // We use BitField to decode the payload, and BitField can only handle
    // 32-bit values.
    ASSERT(is_uint32(payload));
    if (payload != 0) {
      int reg_code = RegisterBits::decode(payload);
      reg_ = Register::XRegFromCode(reg_code);
      uint64_t smi_check_delta = DeltaBits::decode(payload);
      ASSERT(smi_check_delta != 0);
      smi_check_ = inline_data - (smi_check_delta * kInstructionSize);
    }
  }
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_A64
