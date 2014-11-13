// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime.h"

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
      sp_(jssp),
      tmp_list_(DefaultTmpList()),
      fptmp_list_(DefaultFPTmpList()) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


CPURegList MacroAssembler::DefaultTmpList() {
  return CPURegList(ip0, ip1);
}


CPURegList MacroAssembler::DefaultFPTmpList() {
  return CPURegList(fp_scratch1, fp_scratch2);
}


void MacroAssembler::LogicalMacro(const Register& rd,
                                  const Register& rn,
                                  const Operand& operand,
                                  LogicalOp op) {
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    Logical(rd, rn, temp, op);

  } else if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    unsigned reg_size = rd.SizeInBits();

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = ~immediate;
    }

    // Ignore the top 32 bits of an immediate if we're moving to a W register.
    if (rd.Is32Bits()) {
      // Check that the top 32 bits are consistent.
      DCHECK(((immediate >> kWRegSizeInBits) == 0) ||
             ((immediate >> kWRegSizeInBits) == -1));
      immediate &= kWRegMask;
    }

    DCHECK(rd.Is64Bits() || is_uint32(immediate));

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
      Register temp = temps.AcquireSameSizeAs(rn);
      Operand imm_operand = MoveImmediateForShiftedOp(temp, immediate);
      if (rd.Is(csp)) {
        // If rd is the stack pointer we cannot use it as the destination
        // register so we use the temp register as an intermediate again.
        Logical(temp, rn, imm_operand, op);
        Mov(csp, temp);
        AssertStackConsistency();
      } else {
        Logical(rd, rn, imm_operand, op);
      }
    }

  } else if (operand.IsExtendedRegister()) {
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports shift <= 4. We want to support exactly the
    // same modes here.
    DCHECK(operand.shift_amount() <= 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    Logical(rd, rn, temp, op);

  } else {
    // The operand can be encoded in the instruction.
    DCHECK(operand.IsShiftedRegister());
    Logical(rd, rn, operand, op);
  }
}


void MacroAssembler::Mov(const Register& rd, uint64_t imm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(is_uint32(imm) || is_int32(imm) || rd.Is64Bits());
  DCHECK(!rd.IsZero());

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

  // Try to move the immediate in one instruction, and if that fails, switch to
  // using multiple instructions.
  if (!TryOneInstrMoveImmediate(rd, imm)) {
    unsigned reg_size = rd.SizeInBits();

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

    // Mov instructions can't move immediate values into the stack pointer, so
    // set up a temporary register, if needed.
    UseScratchRegisterScope temps(this);
    Register temp = rd.IsSP() ? temps.AcquireSameSizeAs(rd) : rd;

    // Iterate through the halfwords. Use movn/movz for the first non-ignored
    // halfword, and movk for subsequent halfwords.
    DCHECK((reg_size % 16) == 0);
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
    DCHECK(first_mov_done);

    // Move the temporary if the original destination register was the stack
    // pointer.
    if (rd.IsSP()) {
      mov(rd, temp);
      AssertStackConsistency();
    }
  }
}


void MacroAssembler::Mov(const Register& rd,
                         const Operand& operand,
                         DiscardMoveMode discard_mode) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());

  // Provide a swap register for instructions that need to write into the
  // system stack pointer (and can't do this inherently).
  UseScratchRegisterScope temps(this);
  Register dst = (rd.IsSP()) ? temps.AcquireSameSizeAs(rd) : rd;

  if (operand.NeedsRelocation(this)) {
    Ldr(dst, operand.immediate());

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(dst, operand.ImmediateValue());

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
    DCHECK(rd.IsSP());
    Assembler::mov(rd, dst);
  }
}


void MacroAssembler::Mvn(const Register& rd, const Operand& operand) {
  DCHECK(allow_macro_instructions_);

  if (operand.NeedsRelocation(this)) {
    Ldr(rd, operand.immediate());
    mvn(rd, rd);

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(rd, ~operand.ImmediateValue());

  } else if (operand.IsExtendedRegister()) {
    // Emit two instructions for the extend case. This differs from Mov, as
    // the extend and invert can't be achieved in one instruction.
    EmitExtendShift(rd, operand.reg(), operand.extend(),
                    operand.shift_amount());
    mvn(rd, rd);

  } else {
    mvn(rd, operand);
  }
}


unsigned MacroAssembler::CountClearHalfWords(uint64_t imm, unsigned reg_size) {
  DCHECK((reg_size % 8) == 0);
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
  DCHECK((reg_size == kXRegSizeInBits) || (reg_size == kWRegSizeInBits));
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
  DCHECK((cond != al) && (cond != nv));
  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    ConditionalCompareMacro(rn, temp, nzcv, cond, op);

  } else if ((operand.IsShiftedRegister() && (operand.shift_amount() == 0)) ||
             (operand.IsImmediate() &&
              IsImmConditionalCompare(operand.ImmediateValue()))) {
    // The immediate can be encoded in the instruction, or the operand is an
    // unshifted register: call the assembler.
    ConditionalCompare(rn, operand, nzcv, cond, op);

  } else {
    // The operand isn't directly supported by the instruction: perform the
    // operation on a temporary register.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    ConditionalCompare(rn, temp, nzcv, cond, op);
  }
}


void MacroAssembler::Csel(const Register& rd,
                          const Register& rn,
                          const Operand& operand,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  if (operand.IsImmediate()) {
    // Immediate argument. Handle special cases of 0, 1 and -1 using zero
    // register.
    int64_t imm = operand.ImmediateValue();
    Register zr = AppropriateZeroRegFor(rn);
    if (imm == 0) {
      csel(rd, rn, zr, cond);
    } else if (imm == 1) {
      csinc(rd, rn, zr, cond);
    } else if (imm == -1) {
      csinv(rd, rn, zr, cond);
    } else {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(rn);
      Mov(temp, imm);
      csel(rd, rn, temp, cond);
    }
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() == 0)) {
    // Unshifted register argument.
    csel(rd, rn, operand.reg(), cond);
  } else {
    // All other arguments.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    csel(rd, rn, temp, cond);
  }
}


bool MacroAssembler::TryOneInstrMoveImmediate(const Register& dst,
                                              int64_t imm) {
  unsigned n, imm_s, imm_r;
  int reg_size = dst.SizeInBits();
  if (IsImmMovz(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move zero instruction. Movz can't write
    // to the stack pointer.
    movz(dst, imm);
    return true;
  } else if (IsImmMovn(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move not instruction. Movn can't write
    // to the stack pointer.
    movn(dst, dst.Is64Bits() ? ~imm : (~imm & kWRegMask));
    return true;
  } else if (IsImmLogical(imm, reg_size, &n, &imm_s, &imm_r)) {
    // Immediate can be represented in a logical orr instruction.
    LogicalImmediate(dst, AppropriateZeroRegFor(dst), n, imm_s, imm_r, ORR);
    return true;
  }
  return false;
}


Operand MacroAssembler::MoveImmediateForShiftedOp(const Register& dst,
                                                  int64_t imm) {
  int reg_size = dst.SizeInBits();

  // Encode the immediate in a single move instruction, if possible.
  if (TryOneInstrMoveImmediate(dst, imm)) {
    // The move was successful; nothing to do here.
  } else {
    // Pre-shift the immediate to the least-significant bits of the register.
    int shift_low = CountTrailingZeros(imm, reg_size);
    int64_t imm_low = imm >> shift_low;

    // Pre-shift the immediate to the most-significant bits of the register. We
    // insert set bits in the least-significant bits, as this creates a
    // different immediate that may be encodable using movn or orr-immediate.
    // If this new immediate is encodable, the set bits will be eliminated by
    // the post shift on the following instruction.
    int shift_high = CountLeadingZeros(imm, reg_size);
    int64_t imm_high = (imm << shift_high) | ((1 << shift_high) - 1);

    if (TryOneInstrMoveImmediate(dst, imm_low)) {
      // The new immediate has been moved into the destination's low bits:
      // return a new leftward-shifting operand.
      return Operand(dst, LSL, shift_low);
    } else if (TryOneInstrMoveImmediate(dst, imm_high)) {
      // The new immediate has been moved into the destination's high bits:
      // return a new rightward-shifting operand.
      return Operand(dst, LSR, shift_high);
    } else {
      // Use the generic move operation to set up the immediate.
      Mov(dst, imm);
    }
  }
  return Operand(dst);
}


void MacroAssembler::AddSubMacro(const Register& rd,
                                 const Register& rn,
                                 const Operand& operand,
                                 FlagsUpdate S,
                                 AddSubOp op) {
  if (operand.IsZero() && rd.Is(rn) && rd.Is64Bits() && rn.Is64Bits() &&
      !operand.NeedsRelocation(this) && (S == LeaveFlags)) {
    // The instruction would be a nop. Avoid generating useless code.
    return;
  }

  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubMacro(rd, rn, temp, S, op);
  } else if ((operand.IsImmediate() &&
              !IsImmAddSub(operand.ImmediateValue()))      ||
             (rn.IsZero() && !operand.IsShiftedRegister()) ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    if (operand.IsImmediate()) {
      Operand imm_operand =
          MoveImmediateForShiftedOp(temp, operand.ImmediateValue());
      AddSub(rd, rn, imm_operand, S, op);
    } else {
      Mov(temp, operand);
      AddSub(rd, rn, temp, S, op);
    }
  } else {
    AddSub(rd, rn, operand, S, op);
  }
}


void MacroAssembler::AddSubWithCarryMacro(const Register& rd,
                                          const Register& rn,
                                          const Operand& operand,
                                          FlagsUpdate S,
                                          AddSubWithCarryOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubWithCarryMacro(rd, rn, temp, S, op);

  } else if (operand.IsImmediate() ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    // Add/sub with carry (immediate or ROR shifted register.)
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Add/sub with carry (shifted register).
    DCHECK(operand.reg().SizeInBits() == rd.SizeInBits());
    DCHECK(operand.shift() != ROR);
    DCHECK(is_uintn(operand.shift_amount(),
          rd.SizeInBits() == kXRegSizeInBits ? kXRegSizeInBitsLog2
                                             : kWRegSizeInBitsLog2));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitShift(temp, operand.reg(), operand.shift(), operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsExtendedRegister()) {
    // Add/sub with carry (extended register).
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports a shift <= 4. We want to support exactly the
    // same modes.
    DCHECK(operand.shift_amount() <= 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
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
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(addr.base());
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

void MacroAssembler::LoadStorePairMacro(const CPURegister& rt,
                                        const CPURegister& rt2,
                                        const MemOperand& addr,
                                        LoadStorePairOp op) {
  // TODO(all): Should we support register offset for load-store-pair?
  DCHECK(!addr.IsRegisterOffset());

  int64_t offset = addr.offset();
  LSDataSize size = CalcLSPairDataSize(op);

  // Check if the offset fits in the immediate field of the appropriate
  // instruction. If not, emit two instructions to perform the operation.
  if (IsImmLSPair(offset, size)) {
    // Encodable in one load/store pair instruction.
    LoadStorePair(rt, rt2, addr, op);
  } else {
    Register base = addr.base();
    if (addr.IsImmediateOffset()) {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(base);
      Add(temp, base, offset);
      LoadStorePair(rt, rt2, MemOperand(temp), op);
    } else if (addr.IsPostIndex()) {
      LoadStorePair(rt, rt2, MemOperand(base), op);
      Add(base, base, offset);
    } else {
      DCHECK(addr.IsPreIndex());
      Add(base, base, offset);
      LoadStorePair(rt, rt2, MemOperand(base), op);
    }
  }
}


void MacroAssembler::Load(const Register& rt,
                          const MemOperand& addr,
                          Representation r) {
  DCHECK(!r.IsDouble());

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
    DCHECK(rt.Is64Bits());
    Ldr(rt, addr);
  }
}


void MacroAssembler::Store(const Register& rt,
                           const MemOperand& addr,
                           Representation r) {
  DCHECK(!r.IsDouble());

  if (r.IsInteger8() || r.IsUInteger8()) {
    Strb(rt, addr);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    Strh(rt, addr);
  } else if (r.IsInteger32()) {
    Str(rt.W(), addr);
  } else {
    DCHECK(rt.Is64Bits());
    if (r.IsHeapObject()) {
      AssertNotSmi(rt);
    } else if (r.IsSmi()) {
      AssertSmi(rt);
    }
    Str(rt, addr);
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
    // Also maintain the next pool check.
    next_veneer_pool_check_ =
      Min(next_veneer_pool_check_,
          max_reachable_pc - kVeneerDistanceCheckMargin);
  }
  return need_longer_range;
}


void MacroAssembler::Adr(const Register& rd, Label* label, AdrHint hint) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());

  if (hint == kAdrNear) {
    adr(rd, label);
    return;
  }

  DCHECK(hint == kAdrFar);
  if (label->is_bound()) {
    int label_offset = label->pos() - pc_offset();
    if (Instruction::IsValidPCRelOffset(label_offset)) {
      adr(rd, label);
    } else {
      DCHECK(label_offset <= 0);
      int min_adr_offset = -(1 << (Instruction::ImmPCRelRangeBitwidth - 1));
      adr(rd, min_adr_offset);
      Add(rd, rd, label_offset - min_adr_offset);
    }
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();

    InstructionAccurateScope scope(
        this, PatchingAssembler::kAdrFarPatchableNInstrs);
    adr(rd, label);
    for (int i = 0; i < PatchingAssembler::kAdrFarPatchableNNops; ++i) {
      nop(ADR_FAR_NOP);
    }
    movz(scratch, 0);
  }
}


void MacroAssembler::B(Label* label, BranchType type, Register reg, int bit) {
  DCHECK((reg.Is(NoReg) || type >= kBranchTypeFirstUsingReg) &&
         (bit == -1 || type >= kBranchTypeFirstUsingBit));
  if (kBranchTypeFirstCondition <= type && type <= kBranchTypeLastCondition) {
    B(static_cast<Condition>(type), label);
  } else {
    switch (type) {
      case always:        B(label);              break;
      case never:         break;
      case reg_zero:      Cbz(reg, label);       break;
      case reg_not_zero:  Cbnz(reg, label);      break;
      case reg_bit_clear: Tbz(reg, bit, label);  break;
      case reg_bit_set:   Tbnz(reg, bit, label); break;
      default:
        UNREACHABLE();
    }
  }
}


void MacroAssembler::B(Label* label, Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK((cond != al) && (cond != nv));

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CondBranchType);

  if (need_extra_instructions) {
    b(&done, NegateCondition(cond));
    B(label);
  } else {
    b(label, cond);
  }
  bind(&done);
}


void MacroAssembler::Tbnz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbz(rt, bit_pos, &done);
    B(label);
  } else {
    tbnz(rt, bit_pos, label);
  }
  bind(&done);
}


void MacroAssembler::Tbz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbnz(rt, bit_pos, &done);
    B(label);
  } else {
    tbz(rt, bit_pos, label);
  }
  bind(&done);
}


void MacroAssembler::Cbnz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbz(rt, &done);
    B(label);
  } else {
    cbnz(rt, label);
  }
  bind(&done);
}


void MacroAssembler::Cbz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions_);

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbnz(rt, &done);
    B(label);
  } else {
    cbz(rt, label);
  }
  bind(&done);
}


// Pseudo-instructions.


void MacroAssembler::Abs(const Register& rd, const Register& rm,
                         Label* is_not_representable,
                         Label* is_representable) {
  DCHECK(allow_macro_instructions_);
  DCHECK(AreSameSizeAndType(rd, rm));

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
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));

  int count = 1 + src1.IsValid() + src2.IsValid() + src3.IsValid();
  int size = src0.SizeInBytes();

  PushPreamble(count, size);
  PushHelper(count, size, src0, src1, src2, src3);
}


void MacroAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3,
                          const CPURegister& src4, const CPURegister& src5,
                          const CPURegister& src6, const CPURegister& src7) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3, src4, src5, src6, src7));

  int count = 5 + src5.IsValid() + src6.IsValid() + src6.IsValid();
  int size = src0.SizeInBytes();

  PushPreamble(count, size);
  PushHelper(4, size, src0, src1, src2, src3);
  PushHelper(count - 4, size, src4, src5, src6, src7);
}


void MacroAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(dst0.IsValid());

  int count = 1 + dst1.IsValid() + dst2.IsValid() + dst3.IsValid();
  int size = dst0.SizeInBytes();

  PopHelper(count, size, dst0, dst1, dst2, dst3);
  PopPostamble(count, size);
}


void MacroAssembler::Push(const Register& src0, const FPRegister& src1) {
  int size = src0.SizeInBytes() + src1.SizeInBytes();

  PushPreamble(size);
  // Reserve room for src0 and push src1.
  str(src1, MemOperand(StackPointer(), -size, PreIndex));
  // Fill the gap with src0.
  str(src0, MemOperand(StackPointer(), src1.SizeInBytes()));
}


void MacroAssembler::PushPopQueue::PushQueued(
    PreambleDirective preamble_directive) {
  if (queued_.empty()) return;

  if (preamble_directive == WITH_PREAMBLE) {
    masm_->PushPreamble(size_);
  }

  int count = queued_.size();
  int index = 0;
  while (index < count) {
    // PushHelper can only handle registers with the same size and type, and it
    // can handle only four at a time. Batch them up accordingly.
    CPURegister batch[4] = {NoReg, NoReg, NoReg, NoReg};
    int batch_index = 0;
    do {
      batch[batch_index++] = queued_[index++];
    } while ((batch_index < 4) && (index < count) &&
             batch[0].IsSameSizeAndType(queued_[index]));

    masm_->PushHelper(batch_index, batch[0].SizeInBytes(),
                      batch[0], batch[1], batch[2], batch[3]);
  }

  queued_.clear();
}


void MacroAssembler::PushPopQueue::PopQueued() {
  if (queued_.empty()) return;

  int count = queued_.size();
  int index = 0;
  while (index < count) {
    // PopHelper can only handle registers with the same size and type, and it
    // can handle only four at a time. Batch them up accordingly.
    CPURegister batch[4] = {NoReg, NoReg, NoReg, NoReg};
    int batch_index = 0;
    do {
      batch[batch_index++] = queued_[index++];
    } while ((batch_index < 4) && (index < count) &&
             batch[0].IsSameSizeAndType(queued_[index]));

    masm_->PopHelper(batch_index, batch[0].SizeInBytes(),
                     batch[0], batch[1], batch[2], batch[3]);
  }

  masm_->PopPostamble(size_);
  queued_.clear();
}


void MacroAssembler::PushCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PushPreamble(registers.Count(), size);
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
  PopPostamble(registers.Count(), size);
}


void MacroAssembler::PushMultipleTimes(CPURegister src, int count) {
  int size = src.SizeInBytes();

  PushPreamble(count, size);

  if (FLAG_optimize_for_size && count > 8) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Label loop;
    __ Mov(temp, count / 2);
    __ Bind(&loop);
    PushHelper(2, size, src, src, NoReg, NoReg);
    __ Subs(temp, temp, 1);
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
  DCHECK(count == 0);
}


void MacroAssembler::PushMultipleTimes(CPURegister src, Register count) {
  PushPreamble(Operand(count, UXTW, WhichPowerOf2(src.SizeInBytes())));

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireSameSizeAs(count);

  if (FLAG_optimize_for_size) {
    Label loop, done;

    Subs(temp, count, 1);
    B(mi, &done);

    // Push all registers individually, to save code size.
    Bind(&loop);
    Subs(temp, temp, 1);
    PushHelper(1, src.SizeInBytes(), src, NoReg, NoReg, NoReg);
    B(pl, &loop);

    Bind(&done);
  } else {
    Label loop, leftover2, leftover1, done;

    Subs(temp, count, 4);
    B(mi, &leftover2);

    // Push groups of four first.
    Bind(&loop);
    Subs(temp, temp, 4);
    PushHelper(4, src.SizeInBytes(), src, src, src, src);
    B(pl, &loop);

    // Push groups of two.
    Bind(&leftover2);
    Tbz(count, 1, &leftover1);
    PushHelper(2, src.SizeInBytes(), src, src, NoReg, NoReg);

    // Push the last one (if required).
    Bind(&leftover1);
    Tbz(count, 0, &done);
    PushHelper(1, src.SizeInBytes(), src, NoReg, NoReg, NoReg);

    Bind(&done);
  }
}


void MacroAssembler::PushHelper(int count, int size,
                                const CPURegister& src0,
                                const CPURegister& src1,
                                const CPURegister& src2,
                                const CPURegister& src3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));
  DCHECK(size == src0.SizeInBytes());

  // When pushing multiple registers, the store order is chosen such that
  // Push(a, b) is equivalent to Push(a) followed by Push(b).
  switch (count) {
    case 1:
      DCHECK(src1.IsNone() && src2.IsNone() && src3.IsNone());
      str(src0, MemOperand(StackPointer(), -1 * size, PreIndex));
      break;
    case 2:
      DCHECK(src2.IsNone() && src3.IsNone());
      stp(src1, src0, MemOperand(StackPointer(), -2 * size, PreIndex));
      break;
    case 3:
      DCHECK(src3.IsNone());
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

  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(size == dst0.SizeInBytes());

  // When popping multiple registers, the load order is chosen such that
  // Pop(a, b) is equivalent to Pop(a) followed by Pop(b).
  switch (count) {
    case 1:
      DCHECK(dst1.IsNone() && dst2.IsNone() && dst3.IsNone());
      ldr(dst0, MemOperand(StackPointer(), 1 * size, PostIndex));
      break;
    case 2:
      DCHECK(dst2.IsNone() && dst3.IsNone());
      ldp(dst0, dst1, MemOperand(StackPointer(), 2 * size, PostIndex));
      break;
    case 3:
      DCHECK(dst3.IsNone());
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


void MacroAssembler::PushPreamble(Operand total_size) {
  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    if (total_size.IsImmediate()) {
      DCHECK((total_size.ImmediateValue() % 16) == 0);
    }

    // Don't check access size for non-immediate sizes. It's difficult to do
    // well, and it will be caught by hardware (or the simulator) anyway.
  } else {
    // Even if the current stack pointer is not the system stack pointer (csp),
    // the system stack pointer will still be modified in order to comply with
    // ABI rules about accessing memory below the system stack pointer.
    BumpSystemStackPointer(total_size);
  }
}


void MacroAssembler::PopPostamble(Operand total_size) {
  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    if (total_size.IsImmediate()) {
      DCHECK((total_size.ImmediateValue() % 16) == 0);
    }

    // Don't check access size for non-immediate sizes. It's difficult to do
    // well, and it will be caught by hardware (or the simulator) anyway.
  } else if (emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    SyncSystemStackPointer();
  }
}


void MacroAssembler::Poke(const CPURegister& src, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK(offset.ImmediateValue() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Str(src, MemOperand(StackPointer(), offset));
}


void MacroAssembler::Peek(const CPURegister& dst, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK(offset.ImmediateValue() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Ldr(dst, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PokePair(const CPURegister& src1,
                              const CPURegister& src2,
                              int offset) {
  DCHECK(AreSameSizeAndType(src1, src2));
  DCHECK((offset >= 0) && ((offset % src1.SizeInBytes()) == 0));
  Stp(src1, src2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PeekPair(const CPURegister& dst1,
                              const CPURegister& dst2,
                              int offset) {
  DCHECK(AreSameSizeAndType(dst1, dst2));
  DCHECK((offset >= 0) && ((offset % dst1.SizeInBytes()) == 0));
  Ldp(dst1, dst2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PushCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is the
  // system stack pointer (csp).
  DCHECK(csp.Is(StackPointer()));

  MemOperand tos(csp, -2 * kXRegSize, PreIndex);

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
  DCHECK(csp.Is(StackPointer()));

  MemOperand tos(csp, 2 * kXRegSize, PostIndex);

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
  // Avoid emitting code when !use_real_abort() since non-real aborts cause too
  // much code to be generated.
  if (emit_debug_code() && use_real_aborts()) {
    if (csp.Is(StackPointer()) || CpuFeatures::IsSupported(ALWAYS_ALIGN_CSP)) {
      // Always check the alignment of csp if ALWAYS_ALIGN_CSP is true.  We
      // can't check the alignment of csp without using a scratch register (or
      // clobbering the flags), but the processor (or simulator) will abort if
      // it is not properly aligned during a load.
      ldr(xzr, MemOperand(csp, 0));
    }
    if (FLAG_enable_slow_asserts && !csp.Is(StackPointer())) {
      Label ok;
      // Check that csp <= StackPointer(), preserving all registers and NZCV.
      sub(StackPointer(), csp, StackPointer());
      cbz(StackPointer(), &ok);                 // Ok if csp == StackPointer().
      tbnz(StackPointer(), kXSignBit, &ok);     // Ok if csp < StackPointer().

      // Avoid generating AssertStackConsistency checks for the Push in Abort.
      { DontEmitDebugCodeScope dont_emit_debug_code_scope(this);
        Abort(kTheCurrentStackPointerIsBelowCsp);
      }

      bind(&ok);
      // Restore StackPointer().
      sub(StackPointer(), csp, StackPointer());
    }
  }
}


void MacroAssembler::AssertFPCRState(Register fpcr) {
  if (emit_debug_code()) {
    Label unexpected_mode, done;
    UseScratchRegisterScope temps(this);
    if (fpcr.IsNone()) {
      fpcr = temps.AcquireX();
      Mrs(fpcr, FPCR);
    }

    // Settings overridden by ConfiugreFPCR():
    //   - Assert that default-NaN mode is set.
    Tbz(fpcr, DN_offset, &unexpected_mode);

    // Settings left to their default values:
    //   - Assert that flush-to-zero is not set.
    Tbnz(fpcr, FZ_offset, &unexpected_mode);
    //   - Assert that the rounding mode is nearest-with-ties-to-even.
    STATIC_ASSERT(FPTieEven == 0);
    Tst(fpcr, RMode_mask);
    B(eq, &done);

    Bind(&unexpected_mode);
    Abort(kUnexpectedFPCRMode);

    Bind(&done);
  }
}


void MacroAssembler::ConfigureFPCR() {
  UseScratchRegisterScope temps(this);
  Register fpcr = temps.AcquireX();
  Mrs(fpcr, FPCR);

  // If necessary, enable default-NaN mode. The default values of the other FPCR
  // options should be suitable, and AssertFPCRState will verify that.
  Label no_write_required;
  Tbnz(fpcr, DN_offset, &no_write_required);

  Orr(fpcr, fpcr, DN_mask);
  Msr(FPCR, fpcr);

  Bind(&no_write_required);
  AssertFPCRState(fpcr);
}


void MacroAssembler::CanonicalizeNaN(const FPRegister& dst,
                                     const FPRegister& src) {
  AssertFPCRState();

  // With DN=1 and RMode=FPTieEven, subtracting 0.0 preserves all inputs except
  // for NaNs, which become the default NaN. We use fsub rather than fadd
  // because sub preserves -0.0 inputs: -0.0 + 0.0 = 0.0, but -0.0 - 0.0 = -0.0.
  Fsub(dst, src, fp_zero);
}


void MacroAssembler::LoadRoot(CPURegister destination,
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
  Ldrsw(dst, FieldMemOperand(map, Map::kBitField3Offset));
  And(dst, dst, Map::EnumLengthBits::kMask);
}


void MacroAssembler::EnumLengthSmi(Register dst, Register map) {
  EnumLengthUntagged(dst, map);
  SmiTag(dst, dst);
}


void MacroAssembler::CheckEnumCache(Register object,
                                    Register null_value,
                                    Register scratch0,
                                    Register scratch1,
                                    Register scratch2,
                                    Register scratch3,
                                    Label* call_runtime) {
  DCHECK(!AreAliased(object, null_value, scratch0, scratch1, scratch2,
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
  Cmp(scratch1, new_space_start);
  B(lt, no_memento_found);

  Mov(scratch2, new_space_allocation_top);
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
  DCHECK(exception.Is(x0));

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
  DCHECK(cond == eq || cond == ne);
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  And(temp, object, ExternalReference::new_space_mask(isolate()));
  Cmp(temp, ExternalReference::new_space_start(isolate()));
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
  DCHECK(value.Is(x0));

  // Drop the stack pointer to the top of the top handler.
  DCHECK(jssp.Is(StackPointer()));
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
  DCHECK(value.Is(x0));

  // Drop the stack pointer to the top of the top stack handler.
  DCHECK(jssp.Is(StackPointer()));
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
    AssertNotSmi(object, kOperandIsASmiAndNotAName);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(temp, temp, LAST_NAME_TYPE);
    Check(ls, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    JumpIfRoot(object, Heap::kUndefinedValueRootIndex, &done_checking);
    Ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    Bind(&done_checking);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(ne, kOperandIsASmiAndNotAString);
    Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(temp, temp, FIRST_NONSTRING_TYPE);
    Check(lo, kOperandIsNotAString);
  }
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All arguments must be on the stack before this function is called.
  // x0 holds the return value after the call.

  // Check that the number of arguments matches what the function expects.
  // If f->nargs is -1, the function can accept a variable number of arguments.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // Place the necessary arguments.
  Mov(x0, num_arguments);
  Mov(x1, ExternalReference(f, isolate()));

  CEntryStub stub(isolate(), 1, save_doubles);
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

  DCHECK(function_address.is(x1) || function_address.is(x2));

  Label profiler_disabled;
  Label end_profiler_check;
  Mov(x10, ExternalReference::is_profiling_address(isolate()));
  Ldrb(w10, MemOperand(x10));
  Cbz(w10, &profiler_disabled);
  Mov(x3, thunk_ref);
  B(&end_profiler_check);

  Bind(&profiler_disabled);
  Mov(x3, function_address);
  Bind(&end_profiler_check);

  // Save the callee-save registers we are going to use.
  // TODO(all): Is this necessary? ARM doesn't do it.
  STATIC_ASSERT(kCallApiFunctionSpillSpace == 4);
  Poke(x19, (spill_offset + 0) * kXRegSize);
  Poke(x20, (spill_offset + 1) * kXRegSize);
  Poke(x21, (spill_offset + 2) * kXRegSize);
  Poke(x22, (spill_offset + 3) * kXRegSize);

  // Allocate HandleScope in callee-save registers.
  // We will need to restore the HandleScope after the call to the API function,
  // by allocating it in callee-save registers they will be preserved by C code.
  Register handle_scope_base = x22;
  Register next_address_reg = x19;
  Register limit_reg = x20;
  Register level_reg = w21;

  Mov(handle_scope_base, next_address);
  Ldr(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  Ldr(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  Ldr(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  Add(level_reg, level_reg, 1);
  Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    Mov(x0, ExternalReference::isolate_address(isolate()));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(this, x3);

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    Mov(x0, ExternalReference::isolate_address(isolate()));
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
  Peek(x19, (spill_offset + 0) * kXRegSize);
  Peek(x20, (spill_offset + 1) * kXRegSize);
  Peek(x21, (spill_offset + 2) * kXRegSize);
  Peek(x22, (spill_offset + 3) * kXRegSize);

  // Check if the function scheduled an exception.
  Mov(x5, ExternalReference::scheduled_exception_address(isolate()));
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
        ExternalReference(
            Runtime::kPromoteScheduledException, isolate()), 0);
  }
  B(&exception_handled);

  // HandleScope limit has changed. Delete allocated extensions.
  Bind(&delete_allocated_handles);
  Str(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  // Save the return value in a callee-save register.
  Register saved_result = x19;
  Mov(saved_result, x0);
  Mov(x0, ExternalReference::isolate_address(isolate()));
  CallCFunction(
      ExternalReference::delete_handle_scope_extensions(isolate()), 1);
  Mov(x0, saved_result);
  B(&leave_exit_frame);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  Mov(x0, num_arguments);
  Mov(x1, ext);

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  Mov(x1, builtin);
  CEntryStub stub(isolate(), 1);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
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


void MacroAssembler::GetBuiltinEntry(Register target,
                                     Register function,
                                     Builtins::JavaScript id) {
  DCHECK(!AreAliased(target, function));
  GetBuiltinFunction(function, id);
  // Load the code entry point from the builtins object.
  Ldr(target, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  ASM_LOCATION("MacroAssembler::InvokeBuiltin");
  // You can't call a builtin without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Get the builtin entry in x2 and setup the function object in x1.
  GetBuiltinEntry(x2, x1, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(x2));
    Call(x2);
    call_wrapper.AfterCall();
  } else {
    DCHECK(flag == JUMP_FUNCTION);
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
  DCHECK(!AreAliased(string, length, scratch1, scratch2));
  LoadRoot(scratch2, map_index);
  SmiTag(scratch1, length);
  Str(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));

  Mov(scratch2, String::kEmptyHashField);
  Str(scratch1, FieldMemOperand(string, String::kLengthOffset));
  Str(scratch2, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_ARM64
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // V8_HOST_ARCH_ARM64
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM64
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args) {
  CallCFunction(function, num_of_reg_args, 0);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, function);
  CallCFunction(temp, num_of_reg_args, num_of_double_args);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  DCHECK(has_frame());
  // We can pass 8 integer arguments in registers. If we need to pass more than
  // that, we'll need to implement support for passing them on the stack.
  DCHECK(num_of_reg_args <= 8);

  // If we're passing doubles, we're limited to the following prototypes
  // (defined by ExternalReference::Type):
  //  BUILTIN_COMPARE_CALL:  int f(double, double)
  //  BUILTIN_FP_FP_CALL:    double f(double, double)
  //  BUILTIN_FP_CALL:       double f(double)
  //  BUILTIN_FP_INT_CALL:   double f(double, int)
  if (num_of_double_args > 0) {
    DCHECK(num_of_reg_args <= 1);
    DCHECK((num_of_double_args + num_of_reg_args) <= 2);
  }


  // If the stack pointer is not csp, we need to derive an aligned csp from the
  // current stack pointer.
  const Register old_stack_pointer = StackPointer();
  if (!csp.Is(old_stack_pointer)) {
    AssertStackConsistency();

    int sp_alignment = ActivationFrameAlignment();
    // The ABI mandates at least 16-byte alignment.
    DCHECK(sp_alignment >= 16);
    DCHECK(base::bits::IsPowerOfTwo32(sp_alignment));

    // The current stack pointer is a callee saved register, and is preserved
    // across the call.
    DCHECK(kCalleeSaved.IncludesAliasOf(old_stack_pointer));

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
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireX();
      DCHECK(ActivationFrameAlignment() == 16);
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
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, Operand(target, rmode));
  Br(temp);
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  AllowDeferredHandleDereference embedding_raw_address;
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode);
}


void MacroAssembler::Call(Register target) {
  BlockPoolsScope scope(this);
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
  BlockPoolsScope scope(this);
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
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif
  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  DCHECK(rmode != RelocInfo::NONE32);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();

  if (rmode == RelocInfo::NONE64) {
    // Addresses are 48 bits so we never need to load the upper 16 bits.
    uint64_t imm = reinterpret_cast<uint64_t>(target);
    // If we don't use ARM tagged addresses, the 16 higher bits must be 0.
    DCHECK(((imm >> 48) & 0xffff) == 0);
    movz(temp, (imm >> 0) & 0xffff, 0);
    movk(temp, (imm >> 16) & 0xffff, 16);
    movk(temp, (imm >> 32) & 0xffff, 32);
  } else {
    Ldr(temp, Immediate(reinterpret_cast<intptr_t>(target), rmode));
  }
  Blr(temp);
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
  DCHECK(rmode != RelocInfo::NONE32);

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
  DCHECK(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    return kCallSizeWithoutRelocation;
  } else {
    return kCallSizeWithRelocation;
  }
}


void MacroAssembler::JumpIfHeapNumber(Register object, Label* on_heap_number,
                                      SmiCheckType smi_check_type) {
  Label on_not_heap_number;

  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(object, &on_not_heap_number);
  }

  AssertNotSmi(object);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  JumpIfRoot(temp, Heap::kHeapNumberMapRootIndex, on_heap_number);

  Bind(&on_not_heap_number);
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Label* on_not_heap_number,
                                         SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(object, on_not_heap_number);
  }

  AssertNotSmi(object);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  JumpIfNotRoot(temp, Heap::kHeapNumberMapRootIndex, on_not_heap_number);
}


void MacroAssembler::LookupNumberStringCache(Register object,
                                             Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* not_found) {
  DCHECK(!AreAliased(object, result, scratch1, scratch2, scratch3));

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
  JumpIfNotHeapNumber(object, not_found);

  STATIC_ASSERT(kDoubleSize == (kWRegSize * 2));
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


void MacroAssembler::TryRepresentDoubleAsInt(Register as_int,
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


void MacroAssembler::TestForMinusZero(DoubleRegister input) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  // Floating point -0.0 is kMinInt as an integer, so subtracting 1 (cmp) will
  // cause overflow.
  Fmov(temp, input);
  Cmp(temp, 1);
}


void MacroAssembler::JumpIfMinusZero(DoubleRegister input,
                                     Label* on_negative_zero) {
  TestForMinusZero(input);
  B(vs, on_negative_zero);
}


void MacroAssembler::JumpIfMinusZero(Register input,
                                     Label* on_negative_zero) {
  DCHECK(input.Is64Bits());
  // Floating point value is in an integer register. Detect -0.0 by subtracting
  // 1 (cmp), which will cause overflow.
  Cmp(input, 1);
  B(vs, on_negative_zero);
}


void MacroAssembler::ClampInt32ToUint8(Register output, Register input) {
  // Clamp the value to [0..255].
  Cmp(input.W(), Operand(input.W(), UXTB));
  // If input < input & 0xff, it must be < 0, so saturate to 0.
  Csel(output.W(), wzr, input.W(), lt);
  // If input <= input & 0xff, it must be <= 255. Otherwise, saturate to 255.
  Csel(output.W(), output.W(), 255, le);
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
                                               Register scratch3,
                                               Register scratch4,
                                               Register scratch5) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in a tight loop.
  DCHECK(!AreAliased(dst, src,
                     scratch1, scratch2, scratch3, scratch4, scratch5));
  DCHECK(count >= 2);

  const Register& remaining = scratch3;
  Mov(remaining, count / 2);

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = scratch2;
  Sub(dst_untagged, dst, kHeapObjectTag);
  Sub(src_untagged, src, kHeapObjectTag);

  // Copy fields in pairs.
  Label loop;
  Bind(&loop);
  Ldp(scratch4, scratch5,
      MemOperand(src_untagged, kXRegSize* 2, PostIndex));
  Stp(scratch4, scratch5,
      MemOperand(dst_untagged, kXRegSize* 2, PostIndex));
  Sub(remaining, remaining, 1);
  Cbnz(remaining, &loop);

  // Handle the leftovers.
  if (count & 1) {
    Ldr(scratch4, MemOperand(src_untagged));
    Str(scratch4, MemOperand(dst_untagged));
  }
}


void MacroAssembler::CopyFieldsUnrolledPairsHelper(Register dst,
                                                   Register src,
                                                   unsigned count,
                                                   Register scratch1,
                                                   Register scratch2,
                                                   Register scratch3,
                                                   Register scratch4) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in an unrolled loop.
  DCHECK(!AreAliased(dst, src, scratch1, scratch2, scratch3, scratch4));

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = scratch2;
  sub(dst_untagged, dst, kHeapObjectTag);
  sub(src_untagged, src, kHeapObjectTag);

  // Copy fields in pairs.
  for (unsigned i = 0; i < count / 2; i++) {
    Ldp(scratch3, scratch4, MemOperand(src_untagged, kXRegSize * 2, PostIndex));
    Stp(scratch3, scratch4, MemOperand(dst_untagged, kXRegSize * 2, PostIndex));
  }

  // Handle the leftovers.
  if (count & 1) {
    Ldr(scratch3, MemOperand(src_untagged));
    Str(scratch3, MemOperand(dst_untagged));
  }
}


void MacroAssembler::CopyFieldsUnrolledHelper(Register dst,
                                              Register src,
                                              unsigned count,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3) {
  // Untag src and dst into scratch registers.
  // Copy src->dst in an unrolled loop.
  DCHECK(!AreAliased(dst, src, scratch1, scratch2, scratch3));

  const Register& dst_untagged = scratch1;
  const Register& src_untagged = scratch2;
  Sub(dst_untagged, dst, kHeapObjectTag);
  Sub(src_untagged, src, kHeapObjectTag);

  // Copy fields one by one.
  for (unsigned i = 0; i < count; i++) {
    Ldr(scratch3, MemOperand(src_untagged, kXRegSize, PostIndex));
    Str(scratch3, MemOperand(dst_untagged, kXRegSize, PostIndex));
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
  DCHECK(!AreAliased(dst, src));
  DCHECK(!temps.IncludesAliasOf(dst));
  DCHECK(!temps.IncludesAliasOf(src));
  DCHECK(!temps.IncludesAliasOf(xzr));

  if (emit_debug_code()) {
    Cmp(dst, src);
    Check(ne, kTheSourceAndDestinationAreTheSame);
  }

  // The value of 'count' at which a loop will be generated (if there are
  // enough scratch registers).
  static const unsigned kLoopThreshold = 8;

  UseScratchRegisterScope masm_temps(this);
  if ((temps.Count() >= 3) && (count >= kLoopThreshold)) {
    CopyFieldsLoopPairsHelper(dst, src, count,
                              Register(temps.PopLowestIndex()),
                              Register(temps.PopLowestIndex()),
                              Register(temps.PopLowestIndex()),
                              masm_temps.AcquireX(),
                              masm_temps.AcquireX());
  } else if (temps.Count() >= 2) {
    CopyFieldsUnrolledPairsHelper(dst, src, count,
                                  Register(temps.PopLowestIndex()),
                                  Register(temps.PopLowestIndex()),
                                  masm_temps.AcquireX(),
                                  masm_temps.AcquireX());
  } else if (temps.Count() == 1) {
    CopyFieldsUnrolledHelper(dst, src, count,
                             Register(temps.PopLowestIndex()),
                             masm_temps.AcquireX(),
                             masm_temps.AcquireX());
  } else {
    UNREACHABLE();
  }
}


void MacroAssembler::CopyBytes(Register dst,
                               Register src,
                               Register length,
                               Register scratch,
                               CopyHint hint) {
  UseScratchRegisterScope temps(this);
  Register tmp1 = temps.AcquireX();
  Register tmp2 = temps.AcquireX();
  DCHECK(!AreAliased(src, dst, length, scratch, tmp1, tmp2));
  DCHECK(!AreAliased(src, dst, csp));

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

  Label short_copy, short_loop, bulk_loop, done;

  if ((hint == kCopyLong || hint == kCopyUnknown) && !FLAG_optimize_for_size) {
    Register bulk_length = scratch;
    int pair_size = 2 * kXRegSize;
    int pair_mask = pair_size - 1;

    Bic(bulk_length, length, pair_mask);
    Cbz(bulk_length, &short_copy);
    Bind(&bulk_loop);
    Sub(bulk_length, bulk_length, pair_size);
    Ldp(tmp1, tmp2, MemOperand(src, pair_size, PostIndex));
    Stp(tmp1, tmp2, MemOperand(dst, pair_size, PostIndex));
    Cbnz(bulk_length, &bulk_loop);

    And(length, length, pair_mask);
  }

  Bind(&short_copy);
  Cbz(length, &done);
  Bind(&short_loop);
  Sub(length, length, 1);
  Ldrb(tmp1, MemOperand(src, 1, PostIndex));
  Strb(tmp1, MemOperand(dst, 1, PostIndex));
  Cbnz(length, &short_loop);


  Bind(&done);
}


void MacroAssembler::FillFields(Register dst,
                                Register field_count,
                                Register filler) {
  DCHECK(!dst.Is(csp));
  UseScratchRegisterScope temps(this);
  Register field_ptr = temps.AcquireX();
  Register counter = temps.AcquireX();
  Label done;

  // Decrement count. If the result < zero, count was zero, and there's nothing
  // to do. If count was one, flags are set to fail the gt condition at the end
  // of the pairs loop.
  Subs(counter, field_count, 1);
  B(lt, &done);

  // There's at least one field to fill, so do this unconditionally.
  Str(filler, MemOperand(dst, kPointerSize, PostIndex));

  // If the bottom bit of counter is set, there are an even number of fields to
  // fill, so pull the start pointer back by one field, allowing the pairs loop
  // to overwrite the field that was stored above.
  And(field_ptr, counter, 1);
  Sub(field_ptr, dst, Operand(field_ptr, LSL, kPointerSizeLog2));

  // Store filler to memory in pairs.
  Label entry, loop;
  B(&entry);
  Bind(&loop);
  Stp(filler, filler, MemOperand(field_ptr, 2 * kPointerSize, PostIndex));
  Subs(counter, counter, 2);
  Bind(&entry);
  B(gt, &loop);

  Bind(&done);
}


void MacroAssembler::JumpIfEitherIsNotSequentialOneByteStrings(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure, SmiCheckType smi_check) {
  if (smi_check == DO_SMI_CHECK) {
    JumpIfEitherSmi(first, second, failure);
  } else if (emit_debug_code()) {
    DCHECK(smi_check == DONT_DO_SMI_CHECK);
    Label not_smi;
    JumpIfEitherSmi(first, second, NULL, &not_smi);

    // At least one input is a smi, but the flags indicated a smi check wasn't
    // needed.
    Abort(kUnexpectedSmi);

    Bind(&not_smi);
  }

  // Test that both first and second are sequential one-byte strings.
  Ldr(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  Ldr(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  Ldrb(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfEitherInstanceTypeIsNotSequentialOneByte(scratch1, scratch2, scratch1,
                                                 scratch2, failure);
}


void MacroAssembler::JumpIfEitherInstanceTypeIsNotSequentialOneByte(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  DCHECK(!AreAliased(scratch1, second));
  DCHECK(!AreAliased(scratch1, scratch2));
  static const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  static const int kFlatOneByteStringTag = ONE_BYTE_STRING_TYPE;
  And(scratch1, first, kFlatOneByteStringMask);
  And(scratch2, second, kFlatOneByteStringMask);
  Cmp(scratch1, kFlatOneByteStringTag);
  Ccmp(scratch2, kFlatOneByteStringTag, NoFlag, eq);
  B(ne, failure);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialOneByte(Register type,
                                                              Register scratch,
                                                              Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  And(scratch, type, kFlatOneByteStringMask);
  Cmp(scratch, kFlatOneByteStringTag);
  B(ne, failure);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialOneByte(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  DCHECK(!AreAliased(first, second, scratch1, scratch2));
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  And(scratch1, first, kFlatOneByteStringMask);
  And(scratch2, second, kFlatOneByteStringMask);
  Cmp(scratch1, kFlatOneByteStringTag);
  Ccmp(scratch2, kFlatOneByteStringTag, NoFlag, eq);
  B(ne, failure);
}


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Register type,
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
  DCHECK(actual.is_immediate() || actual.reg().is(x0));
  DCHECK(expected.is_immediate() || expected.reg().is(x2));
  DCHECK((!code_constant.is_null() && code_reg.is(no_reg)) || code_reg.is(x3));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
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
  DCHECK(flag == JUMP_FUNCTION || has_frame());

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
      DCHECK(flag == JUMP_FUNCTION);
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
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.is(x1));

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
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.Is(x1));

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


void MacroAssembler::TryConvertDoubleToInt64(Register result,
                                             DoubleRegister double_input,
                                             Label* done) {
  // Try to convert with an FPU convert instruction. It's trivial to compute
  // the modulo operation on an integer register so we convert to a 64-bit
  // integer.
  //
  // Fcvtzs will saturate to INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff)
  // when the double is out of range. NaNs and infinities will be converted to 0
  // (as ECMA-262 requires).
  Fcvtzs(result.X(), double_input);

  // The values INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff) are not
  // representable using a double, so if the result is one of those then we know
  // that saturation occured, and we need to manually handle the conversion.
  //
  // It is easy to detect INT64_MIN and INT64_MAX because adding or subtracting
  // 1 will cause signed overflow.
  Cmp(result.X(), 1);
  Ccmp(result.X(), -1, VFlag, vc);

  B(vc, done);
}


void MacroAssembler::TruncateDoubleToI(Register result,
                                       DoubleRegister double_input) {
  Label done;

  // Try to convert the double to an int64. If successful, the bottom 32 bits
  // contain our truncated int32 result.
  TryConvertDoubleToInt64(result, double_input, &done);

  const Register old_stack_pointer = StackPointer();
  if (csp.Is(old_stack_pointer)) {
    // This currently only happens during compiler-unittest. If it arises
    // during regular code generation the DoubleToI stub should be updated to
    // cope with csp and have an extra parameter indicating which stack pointer
    // it should use.
    Push(jssp, xzr);  // Push xzr to maintain csp required 16-bytes alignment.
    Mov(jssp, csp);
    SetStackPointer(jssp);
  }

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr, double_input);

  DoubleToIStub stub(isolate(),
                     jssp,
                     result,
                     0,
                     true,   // is_truncating
                     true);  // skip_fastpath
  CallStub(&stub);  // DoubleToIStub preserves any registers it needs to clobber

  DCHECK_EQ(xzr.SizeInBytes(), double_input.SizeInBytes());
  Pop(xzr, lr);  // xzr to drop the double input on the stack.

  if (csp.Is(old_stack_pointer)) {
    Mov(csp, jssp);
    SetStackPointer(csp);
    AssertStackConsistency();
    Pop(xzr, jssp);
  }

  Bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result,
                                           Register object) {
  Label done;
  DCHECK(!result.is(object));
  DCHECK(jssp.Is(StackPointer()));

  Ldr(fp_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));

  // Try to convert the double to an int64. If successful, the bottom 32 bits
  // contain our truncated int32 result.
  TryConvertDoubleToInt64(result, fp_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr);
  DoubleToIStub stub(isolate(),
                     object,
                     result,
                     HeapNumber::kValueOffset - kHeapObjectTag,
                     true,   // is_truncating
                     true);  // skip_fastpath
  CallStub(&stub);  // DoubleToIStub preserves any registers it needs to clobber
  Pop(lr);

  Bind(&done);
}


void MacroAssembler::StubPrologue() {
  DCHECK(StackPointer().Is(jssp));
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  __ Mov(temp, Smi::FromInt(StackFrame::STUB));
  // Compiled stubs don't age, and so they don't need the predictable code
  // ageing sequence.
  __ Push(lr, fp, cp, temp);
  __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);
}


void MacroAssembler::Prologue(bool code_pre_aging) {
  if (code_pre_aging) {
    Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
    __ EmitCodeAgeSequence(stub);
  } else {
    __ EmitFrameSetupForCodeAgePatching();
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // Out-of-line constant pool not implemented on arm64.
  UNREACHABLE();
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  DCHECK(jssp.Is(StackPointer()));
  UseScratchRegisterScope temps(this);
  Register type_reg = temps.AcquireX();
  Register code_reg = temps.AcquireX();

  Push(lr, fp, cp);
  Mov(type_reg, Smi::FromInt(type));
  Mov(code_reg, Operand(CodeObject()));
  Push(type_reg, code_reg);
  // jssp[4] : lr
  // jssp[3] : fp
  // jssp[2] : cp
  // jssp[1] : type
  // jssp[0] : code object

  // Adjust FP to point to saved FP.
  Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  DCHECK(jssp.Is(StackPointer()));
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
  DCHECK(saved_fp_regs.Count() % 2 == 0);

  int offset = ExitFrameConstants::kLastExitFrameField;
  while (!saved_fp_regs.IsEmpty()) {
    const CPURegister& dst0 = saved_fp_regs.PopHighestIndex();
    const CPURegister& dst1 = saved_fp_regs.PopHighestIndex();
    offset -= 2 * kDRegSize;
    Ldp(dst1, dst0, MemOperand(fp, offset));
  }
}


void MacroAssembler::EnterExitFrame(bool save_doubles,
                                    const Register& scratch,
                                    int extra_space) {
  DCHECK(jssp.Is(StackPointer()));

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
  Claim(extra_space + 1, kXRegSize);
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: Space reserved for SPOffset.
  //         fp[-16]: CodeObject()
  //         fp[-16 - fp_size]: Saved doubles (if save_doubles is true).
  //         jssp[8]: Extra space reserved for caller (if extra_space != 0).
  // jssp -> jssp[0]: Space reserved for the return address.

  // Align and synchronize the system stack pointer with jssp.
  AlignAndSetCSPForFrame();
  DCHECK(csp.Is(StackPointer()));

  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: Space reserved for SPOffset.
  //         fp[-16]: CodeObject()
  //         fp[-16 - fp_size]: Saved doubles (if save_doubles is true).
  //         csp[8]: Memory reserved for the caller if extra_space != 0.
  //                 Alignment padding, if necessary.
  //  csp -> csp[0]: Space reserved for the return address.

  // ExitFrame::GetStateForFramePointer expects to find the return address at
  // the memory address immediately below the pointer stored in SPOffset.
  // It is not safe to derive much else from SPOffset, because the size of the
  // padding can vary.
  Add(scratch, csp, kXRegSize);
  Str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


// Leave the current exit frame.
void MacroAssembler::LeaveExitFrame(bool restore_doubles,
                                    const Register& scratch,
                                    bool restore_context) {
  DCHECK(csp.Is(StackPointer()));

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
    Mov(scratch2, ExternalReference(counter));
    Str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value != 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Mov(scratch2, ExternalReference(counter));
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


void MacroAssembler::DebugBreak() {
  Mov(x0, 0);
  Mov(x1, ExternalReference(Runtime::kDebugBreak, isolate()));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  DCHECK(jssp.Is(StackPointer()));
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
    DCHECK(Smi::FromInt(0) == 0);
    Push(xzr, xzr, x11, x10);
  } else {
    Push(fp, cp, x11, x10);
  }

  // Link the current handler as the next handler.
  Mov(x11, ExternalReference(Isolate::kHandlerAddress, isolate()));
  Ldr(x10, MemOperand(x11));
  Push(x10);
  // Set this new handler as the current one.
  Str(jssp, MemOperand(x11));
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  Pop(x10);
  Mov(x11, ExternalReference(Isolate::kHandlerAddress, isolate()));
  Drop(StackHandlerConstants::kSize - kXRegSize, kByteSizeInBytes);
  Str(x10, MemOperand(x11));
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
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

  UseScratchRegisterScope temps(this);
  Register scratch3 = temps.AcquireX();

  DCHECK(!AreAliased(result, scratch1, scratch2, scratch3));
  DCHECK(result.Is64Bits() && scratch1.Is64Bits() && scratch2.Is64Bits());

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK(0 == (object_size & kObjectAlignmentMask));

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDP.
  ExternalReference heap_allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference heap_allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(heap_allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(heap_allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

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
      Ldr(scratch3, MemOperand(top_address));
      Cmp(result, scratch3);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load the allocation limit. 'result' already contains the allocation top.
    Ldr(allocation_limit, MemOperand(top_address, limit - top));
  }

  // We can ignore DOUBLE_ALIGNMENT flags here because doubles and pointers have
  // the same alignment on ARM64.
  STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);

  // Calculate new top and bail out if new space is exhausted.
  Adds(scratch3, result, object_size);
  Ccmp(scratch3, allocation_limit, CFlag, cc);
  B(hi, gc_required);
  Str(scratch3, MemOperand(top_address));

  // Tag the object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    ObjectTag(result, result);
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

  UseScratchRegisterScope temps(this);
  Register scratch3 = temps.AcquireX();

  DCHECK(!AreAliased(object_size, result, scratch1, scratch2, scratch3));
  DCHECK(object_size.Is64Bits() && result.Is64Bits() &&
         scratch1.Is64Bits() && scratch2.Is64Bits());

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDP.
  ExternalReference heap_allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference heap_allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(heap_allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(heap_allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register top_address = scratch1;
  Register allocation_limit = scratch2;
  Mov(top_address, heap_allocation_top);

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and the allocation limit.
    Ldp(result, allocation_limit, MemOperand(top_address));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      Ldr(scratch3, MemOperand(top_address));
      Cmp(result, scratch3);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load the allocation limit. 'result' already contains the allocation top.
    Ldr(allocation_limit, MemOperand(top_address, limit - top));
  }

  // We can ignore DOUBLE_ALIGNMENT flags here because doubles and pointers have
  // the same alignment on ARM64.
  STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);

  // Calculate new top and bail out if new space is exhausted
  if ((flags & SIZE_IN_WORDS) != 0) {
    Adds(scratch3, result, Operand(object_size, LSL, kPointerSizeLog2));
  } else {
    Adds(scratch3, result, object_size);
  }

  if (emit_debug_code()) {
    Tst(scratch3, kObjectAlignmentMask);
    Check(eq, kUnalignedAllocationInNewSpace);
  }

  Ccmp(scratch3, allocation_limit, CFlag, cc);
  B(hi, gc_required);
  Str(scratch3, MemOperand(top_address));

  // Tag the object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    ObjectTag(result, result);
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
  Mov(scratch, new_space_allocation_top);
  Ldr(scratch, MemOperand(scratch));
  Cmp(object, scratch);
  Check(lt, kUndoAllocationOfNonAllocatedMemory);
#endif
  // Write the address of the object to un-allocate as the current top.
  Mov(scratch, new_space_allocation_top);
  Str(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  DCHECK(!AreAliased(result, length, scratch1, scratch2, scratch3));
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


void MacroAssembler::AllocateOneByteString(Register result, Register length,
                                           Register scratch1, Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  DCHECK(!AreAliased(result, length, scratch1, scratch2, scratch3));
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  STATIC_ASSERT(kCharSize == 1);
  Add(scratch1, length, kObjectAlignmentMask + SeqOneByteString::kHeaderSize);
  Bic(scratch1, scratch1, kObjectAlignmentMask);

  // Allocate one-byte string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result, length, Heap::kOneByteStringMapRootIndex,
                      scratch1, scratch2);
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


void MacroAssembler::AllocateOneByteConsString(Register result, Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kConsOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  DCHECK(!AreAliased(result, length, scratch1, scratch2));
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  DCHECK(!AreAliased(result, length, scratch1, scratch2));
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kSlicedOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Label* gc_required,
                                        Register scratch1,
                                        Register scratch2,
                                        CPURegister value,
                                        CPURegister heap_number_map,
                                        MutableMode mode) {
  DCHECK(!value.IsValid() || value.Is64Bits());
  UseScratchRegisterScope temps(this);

  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           NO_ALLOCATION_FLAGS);

  Heap::RootListIndex map_index = mode == MUTABLE
      ? Heap::kMutableHeapNumberMapRootIndex
      : Heap::kHeapNumberMapRootIndex;

  // Prepare the heap number map.
  if (!heap_number_map.IsValid()) {
    // If we have a valid value register, use the same type of register to store
    // the map so we can use STP to store both in one instruction.
    if (value.IsValid() && value.IsFPRegister()) {
      heap_number_map = temps.AcquireD();
    } else {
      heap_number_map = scratch1;
    }
    LoadRoot(heap_number_map, map_index);
  }
  if (emit_debug_code()) {
    Register map;
    if (heap_number_map.IsFPRegister()) {
      map = scratch1;
      Fmov(map, DoubleRegister(heap_number_map));
    } else {
      map = Register(heap_number_map);
    }
    AssertRegisterIsRoot(map, map_index);
  }

  // Store the heap number map and the value in the allocated object.
  if (value.IsSameSizeAndType(heap_number_map)) {
    STATIC_ASSERT(HeapObject::kMapOffset + kPointerSize ==
                  HeapNumber::kValueOffset);
    Stp(heap_number_map, value, MemOperand(result, HeapObject::kMapOffset));
  } else {
    Str(heap_number_map, MemOperand(result, HeapObject::kMapOffset));
    if (value.IsValid()) {
      Str(value, MemOperand(result, HeapNumber::kValueOffset));
    }
  }
  ObjectTag(result, result);
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


void MacroAssembler::CompareObjectMap(Register obj, Heap::RootListIndex index) {
  UseScratchRegisterScope temps(this);
  Register obj_map = temps.AcquireX();
  Ldr(obj_map, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareRoot(obj_map, index);
}


void MacroAssembler::CompareObjectMap(Register obj, Register scratch,
                                      Handle<Map> map) {
  Ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(scratch, map);
}


void MacroAssembler::CompareMap(Register obj_map,
                                Handle<Map> map) {
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

  CompareObjectMap(obj, scratch, map);
  B(ne, fail);
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

  CompareMap(obj_map, map);
  B(ne, fail);
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
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  Ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  Tst(temp, mask);
}


void MacroAssembler::LoadElementsKindFromMap(Register result, Register map) {
  // Load the map's "bit field 2".
  __ Ldrb(result, FieldMemOperand(map, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  DecodeField<Map::ElementsKindBits>(result);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             BoundFunctionAction action) {
  DCHECK(!AreAliased(function, result, scratch));

  Label non_instance;
  if (action == kMissOnBoundFunction) {
    // Check that the receiver isn't a smi.
    JumpIfSmi(function, miss);

    // Check that the function really is a function. Load map into result reg.
    JumpIfNotObjectType(function, result, scratch, JS_FUNCTION_TYPE, miss);

    Register scratch_w = scratch.W();
    Ldr(scratch,
        FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    // On 64-bit platforms, compiler hints field is not a smi. See definition of
    // kCompilerHintsOffset in src/objects.h.
    Ldr(scratch_w,
        FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    Tbnz(scratch, SharedFunctionInfo::kBoundFunction, miss);

    // Make sure that the function has an instance prototype.
    Ldrb(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
    Tbnz(scratch, Map::kHasNonInstancePrototype, &non_instance);
  }

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

  if (action == kMissOnBoundFunction) {
    B(&done);

    // Non-instance prototype: fetch prototype from constructor field in initial
    // map.
    Bind(&non_instance);
    Ldr(result, FieldMemOperand(result, Map::kConstructorOffset));
  }

  // All done.
  Bind(&done);
}


void MacroAssembler::CompareRoot(const Register& obj,
                                 Heap::RootListIndex index) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  DCHECK(!AreAliased(obj, temp));
  LoadRoot(temp, index);
  Cmp(obj, temp);
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
    CompareAndBranch(lhs, rhs, NegateCondition(cond), if_false);
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


// Note: The ARM version of this clobbers elements_reg, but this version does
// not. Some uses of this in ARM64 assume that elements_reg will be preserved.
void MacroAssembler::StoreNumberToDoubleElements(Register value_reg,
                                                 Register key_reg,
                                                 Register elements_reg,
                                                 Register scratch1,
                                                 FPRegister fpscratch1,
                                                 Label* fail,
                                                 int elements_offset) {
  DCHECK(!AreAliased(value_reg, key_reg, elements_reg, scratch1));
  Label store_num;

  // Speculatively convert the smi to a double - all smis can be exactly
  // represented as a double.
  SmiUntagToDouble(fpscratch1, value_reg, kSpeculativeUntag);

  // If value_reg is a smi, we're done.
  JumpIfSmi(value_reg, &store_num);

  // Ensure that the object is a heap number.
  JumpIfNotHeapNumber(value_reg, fail);

  Ldr(fpscratch1, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

  // Canonicalize NaNs.
  CanonicalizeNaN(fpscratch1);

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
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  DecodeField<String::ArrayIndexValueBits>(index, hash);
  SmiTag(index, index);
}


void MacroAssembler::EmitSeqStringSetCharCheck(
    Register string,
    Register index,
    SeqStringSetCharCheckIndexType index_type,
    Register scratch,
    uint32_t encoding_mask) {
  DCHECK(!AreAliased(string, index, scratch));

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

  DCHECK_EQ(0, Smi::FromInt(0));
  Cmp(index, 0);
  Check(ge, kIndexIsNegative);
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss) {
  DCHECK(!AreAliased(holder_reg, scratch1, scratch2));
  Label same_contexts;

  // Load current lexical context from the stack frame.
  Ldr(scratch1, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  Cmp(scratch1, 0);
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  Ldr(scratch1, FieldMemOperand(scratch1, offset));
  Ldr(scratch1, FieldMemOperand(scratch1, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Read the first word and compare to the global_context_map.
    Ldr(scratch2, FieldMemOperand(scratch1, HeapObject::kMapOffset));
    CompareRoot(scratch2, Heap::kNativeContextMapRootIndex);
    Check(eq, kExpectedNativeContext);
  }

  // Check if both contexts are the same.
  Ldr(scratch2, FieldMemOperand(holder_reg,
                                JSGlobalProxy::kNativeContextOffset));
  Cmp(scratch1, scratch2);
  B(&same_contexts, eq);

  // Check the context is a native context.
  if (emit_debug_code()) {
    // We're short on scratch registers here, so use holder_reg as a scratch.
    Push(holder_reg);
    Register scratch3 = holder_reg;

    CompareRoot(scratch2, Heap::kNullValueRootIndex);
    Check(ne, kExpectedNonNullContext);

    Ldr(scratch3, FieldMemOperand(scratch2, HeapObject::kMapOffset));
    CompareRoot(scratch3, Heap::kNativeContextMapRootIndex);
    Check(eq, kExpectedNativeContext);
    Pop(holder_reg);
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  Ldr(scratch1, FieldMemOperand(scratch1, token_offset));
  Ldr(scratch2, FieldMemOperand(scratch2, token_offset));
  Cmp(scratch1, scratch2);
  B(miss, ne);

  Bind(&same_contexts);
}


// Compute the hash code from the untagged key. This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register key, Register scratch) {
  DCHECK(!AreAliased(key, scratch));

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
  DCHECK(!AreAliased(elements, key, scratch0, scratch1, scratch2, scratch3));

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
    DCHECK(SeededNumberDictionary::kEntrySize == 3);
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
                                         Register scratch1,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  DCHECK(!AreAliased(object, address, scratch1));
  Label done, store_buffer_overflow;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, &ok);
    Abort(kRememberedSetPointerInNewSpace);
    bind(&ok);
  }
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.AcquireX();

  // Load store buffer top.
  Mov(scratch2, ExternalReference::store_buffer_top(isolate()));
  Ldr(scratch1, MemOperand(scratch2));
  // Store pointer to buffer and increment buffer top.
  Str(address, MemOperand(scratch1, kPointerSize, PostIndex));
  // Write back new top of buffer.
  Str(scratch1, MemOperand(scratch2));
  // Call stub on end of buffer.
  // Check for end of buffer.
  DCHECK(StoreBuffer::kStoreBufferOverflowBit ==
         (1 << (14 + kPointerSizeLog2)));
  if (and_then == kFallThroughAtEnd) {
    Tbz(scratch1, (14 + kPointerSizeLog2), &done);
  } else {
    DCHECK(and_then == kReturnAtEnd);
    Tbnz(scratch1, (14 + kPointerSizeLog2), &store_buffer_overflow);
    Ret();
  }

  Bind(&store_buffer_overflow);
  Push(lr);
  StoreBufferOverflowStub store_buffer_overflow_stub(isolate(), fp_mode);
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
  DCHECK(num_unsaved >= 0);
  Claim(num_unsaved);
  PushXRegList(kSafepointSavedRegisters);
}


void MacroAssembler::PushSafepointRegistersAndDoubles() {
  PushSafepointRegisters();
  PushCPURegList(CPURegList(CPURegister::kFPRegister, kDRegSizeInBits,
                            FPRegister::kAllocatableFPRegisters));
}


void MacroAssembler::PopSafepointRegistersAndDoubles() {
  PopCPURegList(CPURegList(CPURegister::kFPRegister, kDRegSizeInBits,
                           FPRegister::kAllocatableFPRegisters));
  PopSafepointRegisters();
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // Make sure the safepoint registers list is what we expect.
  DCHECK(CPURegList::GetSafepointSavedRegisters().list() == 0x6ffcffff);

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
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

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
              OMIT_SMI_CHECK,
              pointers_to_here_check_for_value);

  Bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(value, Operand(bit_cast<int64_t>(kZapValue + 4)));
    Mov(scratch, Operand(bit_cast<int64_t>(kZapValue + 8)));
  }
}


// Will clobber: object, map, dst.
// If lr_status is kLRHasBeenSaved, lr will also be clobbered.
void MacroAssembler::RecordWriteForMap(Register object,
                                       Register map,
                                       Register dst,
                                       LinkRegisterStatus lr_status,
                                       SaveFPRegsMode fp_mode) {
  ASM_LOCATION("MacroAssembler::RecordWrite");
  DCHECK(!AreAliased(object, map));

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectMap(map, temp, isolate()->factory()->meta_map());
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
    Cmp(temp, map);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlagClear(map,
                     map,  // Used as scratch.
                     MemoryChunk::kPointersToHereAreInterestingMask,
                     &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push(lr);
  }
  Add(dst, object, HeapObject::kMapOffset - kHeapObjectTag);
  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    Pop(lr);
  }

  Bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, map,
                   dst);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(dst, Operand(bit_cast<int64_t>(kZapValue + 12)));
    Mov(map, Operand(bit_cast<int64_t>(kZapValue + 16)));
  }
}


// Will clobber: object, address, value.
// If lr_status is kLRHasBeenSaved, lr will also be clobbered.
//
// The register 'object' contains a heap object pointer. The heap object tag is
// shifted away.
void MacroAssembler::RecordWrite(
    Register object,
    Register address,
    Register value,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  ASM_LOCATION("MacroAssembler::RecordWrite");
  DCHECK(!AreAliased(object, value));

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Ldr(temp, MemOperand(address));
    Cmp(temp, value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlagClear(value,
                       value,  // Used as scratch.
                       MemoryChunk::kPointersToHereAreInterestingMask,
                       &done);
  }
  CheckPageFlagClear(object,
                     value,  // Used as scratch.
                     MemoryChunk::kPointersFromHereAreInterestingMask,
                     &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push(lr);
  }
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    Pop(lr);
  }

  Bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, address,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(address, Operand(bit_cast<int64_t>(kZapValue + 12)));
    Mov(value, Operand(bit_cast<int64_t>(kZapValue + 16)));
  }
}


void MacroAssembler::AssertHasValidColor(const Register& reg) {
  if (emit_debug_code()) {
    // The bit sequence is backward. The first character in the string
    // represents the least significant bit.
    DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

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
  DCHECK(!AreAliased(addr_reg, bitmap_reg, shift_reg));
  DCHECK(addr_reg.Is64Bits() && bitmap_reg.Is64Bits() && shift_reg.Is64Bits());
  // addr_reg is divided into fields:
  // |63        page base        20|19    high      8|7   shift   3|2  0|
  // 'high' gives the index of the cell holding color bits for the object.
  // 'shift' gives the offset in the cell for this object's color.
  const int kShiftBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ubfx(temp, addr_reg, kShiftBits, kPageSizeBits - kShiftBits);
  Bic(bitmap_reg, addr_reg, Page::kPageAlignmentMask);
  Add(bitmap_reg, bitmap_reg, Operand(temp, LSL, Bitmap::kBytesPerCellLog2));
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
  DCHECK(!AreAliased(object, bitmap_scratch, shift_scratch));

  GetMarkBits(object, bitmap_scratch, shift_scratch);
  Ldr(bitmap_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Shift the bitmap down to get the color of the object in bits [1:0].
  Lsr(bitmap_scratch, bitmap_scratch, shift_scratch);

  AssertHasValidColor(bitmap_scratch);

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);

  // Check for the color.
  if (first_bit == 0) {
    // Checking for white.
    DCHECK(second_bit == 0);
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
    Ldrsw(scratch, FieldMemOperand(scratch, Map::kBitField3Offset));
    TestAndBranchIfAnySet(scratch, Map::Deprecated::kMask, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(
    Register object,
    Register scratch0,
    Register scratch1,
    Label* found) {
  DCHECK(!AreAliased(object, scratch0, scratch1));
  Factory* factory = isolate()->factory();
  Register current = scratch0;
  Label loop_again;

  // Scratch contains elements pointer.
  Mov(current, object);

  // Loop based on the map going up the prototype chain.
  Bind(&loop_again);
  Ldr(current, FieldMemOperand(current, HeapObject::kMapOffset));
  Ldrb(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  CompareAndBranch(scratch1, DICTIONARY_ELEMENTS, eq, found);
  Ldr(current, FieldMemOperand(current, Map::kPrototypeOffset));
  CompareAndBranch(current, Operand(factory->null_value()), ne, &loop_again);
}


void MacroAssembler::GetRelocatedValueLocation(Register ldr_location,
                                               Register result) {
  DCHECK(!result.Is(ldr_location));
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
  DCHECK(!AreAliased(
      value, bitmap_scratch, shift_scratch, load_scratch, length_scratch));

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);

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
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
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
  DCHECK_EQ(0, kSeqStringTag & kExternalStringTag);
  DCHECK_EQ(0, kConsStringTag & kExternalStringTag);
  Mov(length_scratch, ExternalString::kSize);
  TestAndBranchIfAnySet(instance_type, kExternalStringTag, &is_data_object);

  // Sequential string, either Latin1 or UC16.
  // For Latin1 (char-size of 1) we shift the smi tag away to get the length.
  // For UC16 (char-size of 2) we just leave the smi tag in place, thereby
  // getting the length multiplied by 2.
  DCHECK(kOneByteStringTag == 4 && kStringEncodingMask == 4);
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
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, reason);
  }
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
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
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
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

  // We need some scratch registers for the MacroAssembler, so make sure we have
  // some. This is safe here because Abort never returns.
  RegList old_tmp_list = TmpList()->list();
  TmpList()->Combine(MacroAssembler::DefaultTmpList());

  if (use_real_aborts()) {
    // Avoid infinite recursion; Push contains some assertions that use Abort.
    NoUseRealAbortsScope no_real_aborts(this);

    Mov(x0, Smi::FromInt(reason));
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
      BlockPoolsScope scope(this);
      Bind(&msg_address);
      EmitStringData(GetBailoutReason(reason));
    }
  }

  SetStackPointer(old_stack_pointer);
  TmpList()->set_list(old_tmp_list);
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch1,
    Register scratch2,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  Ldr(scratch1, GlobalObjectMemOperand());
  Ldr(scratch1, FieldMemOperand(scratch1, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  Ldr(scratch1, ContextMemOperand(scratch1, Context::JS_ARRAY_MAPS_INDEX));
  size_t offset = (expected_kind * kPointerSize) + FixedArrayBase::kHeaderSize;
  Ldr(scratch2, FieldMemOperand(scratch1, offset));
  Cmp(map_in_out, scratch2);
  B(ne, no_map_match);

  // Use the transitioned cached map.
  offset = (transitioned_kind * kPointerSize) + FixedArrayBase::kHeaderSize;
  Ldr(map_in_out, FieldMemOperand(scratch1, offset));
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
  DCHECK(!kCallerSaved.IncludesAliasOf(__ StackPointer()));

  // The provided arguments, and their proper procedure-call standard registers.
  CPURegister args[kPrintfMaxArgCount] = {arg0, arg1, arg2, arg3};
  CPURegister pcs[kPrintfMaxArgCount] = {NoReg, NoReg, NoReg, NoReg};

  int arg_count = kPrintfMaxArgCount;

  // The PCS varargs registers for printf. Note that x0 is used for the printf
  // format string.
  static const CPURegList kPCSVarargs =
      CPURegList(CPURegister::kRegister, kXRegSizeInBits, 1, arg_count);
  static const CPURegList kPCSVarargsFP =
      CPURegList(CPURegister::kFPRegister, kDRegSizeInBits, 0, arg_count - 1);

  // We can use caller-saved registers as scratch values, except for the
  // arguments and the PCS registers where they might need to go.
  CPURegList tmp_list = kCallerSaved;
  tmp_list.Remove(x0);      // Used to pass the format string.
  tmp_list.Remove(kPCSVarargs);
  tmp_list.Remove(arg0, arg1, arg2, arg3);

  CPURegList fp_tmp_list = kCallerSavedFP;
  fp_tmp_list.Remove(kPCSVarargsFP);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);

  // Override the MacroAssembler's scratch register list. The lists will be
  // reset automatically at the end of the UseScratchRegisterScope.
  UseScratchRegisterScope temps(this);
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  // Copies of the printf vararg registers that we can pop from.
  CPURegList pcs_varargs = kPCSVarargs;
  CPURegList pcs_varargs_fp = kPCSVarargsFP;

  // Place the arguments. There are lots of clever tricks and optimizations we
  // could use here, but Printf is a debug tool so instead we just try to keep
  // it simple: Move each input that isn't already in the right place to a
  // scratch register, then move everything back.
  for (unsigned i = 0; i < kPrintfMaxArgCount; i++) {
    // Work out the proper PCS register for this argument.
    if (args[i].IsRegister()) {
      pcs[i] = pcs_varargs.PopLowestIndex().X();
      // We might only need a W register here. We need to know the size of the
      // argument so we can properly encode it for the simulator call.
      if (args[i].Is32Bits()) pcs[i] = pcs[i].W();
    } else if (args[i].IsFPRegister()) {
      // In C, floats are always cast to doubles for varargs calls.
      pcs[i] = pcs_varargs_fp.PopLowestIndex().D();
    } else {
      DCHECK(args[i].IsNone());
      arg_count = i;
      break;
    }

    // If the argument is already in the right place, leave it where it is.
    if (args[i].Aliases(pcs[i])) continue;

    // Otherwise, if the argument is in a PCS argument register, allocate an
    // appropriate scratch register and then move it out of the way.
    if (kPCSVarargs.IncludesAliasOf(args[i]) ||
        kPCSVarargsFP.IncludesAliasOf(args[i])) {
      if (args[i].IsRegister()) {
        Register old_arg = Register(args[i]);
        Register new_arg = temps.AcquireSameSizeAs(old_arg);
        Mov(new_arg, old_arg);
        args[i] = new_arg;
      } else {
        FPRegister old_arg = FPRegister(args[i]);
        FPRegister new_arg = temps.AcquireSameSizeAs(old_arg);
        Fmov(new_arg, old_arg);
        args[i] = new_arg;
      }
    }
  }

  // Do a second pass to move values into their final positions and perform any
  // conversions that may be required.
  for (int i = 0; i < arg_count; i++) {
    DCHECK(pcs[i].type() == args[i].type());
    if (pcs[i].IsRegister()) {
      Mov(Register(pcs[i]), Register(args[i]), kDiscardForSameWReg);
    } else {
      DCHECK(pcs[i].IsFPRegister());
      if (pcs[i].SizeInBytes() == args[i].SizeInBytes()) {
        Fmov(FPRegister(pcs[i]), FPRegister(args[i]));
      } else {
        Fcvt(FPRegister(pcs[i]), FPRegister(args[i]));
      }
    }
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
  { BlockPoolsScope scope(this);
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

  CallPrintf(arg_count, pcs);
}


void MacroAssembler::CallPrintf(int arg_count, const CPURegister * args) {
  // A call to printf needs special handling for the simulator, since the system
  // printf function will use a different instruction set and the procedure-call
  // standard will not be compatible.
#ifdef USE_SIMULATOR
  { InstructionAccurateScope scope(this, kPrintfLength / kInstructionSize);
    hlt(kImmExceptionIsPrintf);
    dc32(arg_count);          // kPrintfArgCountOffset

    // Determine the argument pattern.
    uint32_t arg_pattern_list = 0;
    for (int i = 0; i < arg_count; i++) {
      uint32_t arg_pattern;
      if (args[i].IsRegister()) {
        arg_pattern = args[i].Is32Bits() ? kPrintfArgW : kPrintfArgX;
      } else {
        DCHECK(args[i].Is64Bits());
        arg_pattern = kPrintfArgD;
      }
      DCHECK(arg_pattern < (1 << kPrintfArgPatternBits));
      arg_pattern_list |= (arg_pattern << (kPrintfArgPatternBits * i));
    }
    dc32(arg_pattern_list);   // kPrintfArgPatternListOffset
  }
#else
  Call(FUNCTION_ADDR(printf), RelocInfo::EXTERNAL_REFERENCE);
#endif
}


void MacroAssembler::Printf(const char * format,
                            CPURegister arg0,
                            CPURegister arg1,
                            CPURegister arg2,
                            CPURegister arg3) {
  // We can only print sp if it is the current stack pointer.
  if (!csp.Is(StackPointer())) {
    DCHECK(!csp.Aliases(arg0));
    DCHECK(!csp.Aliases(arg1));
    DCHECK(!csp.Aliases(arg2));
    DCHECK(!csp.Aliases(arg3));
  }

  // Printf is expected to preserve all registers, so make sure that none are
  // available as scratch registers until we've preserved them.
  RegList old_tmp_list = TmpList()->list();
  RegList old_fp_tmp_list = FPTmpList()->list();
  TmpList()->set_list(0);
  FPTmpList()->set_list(0);

  // Preserve all caller-saved registers as well as NZCV.
  // If csp is the stack pointer, PushCPURegList asserts that the size of each
  // list is a multiple of 16 bytes.
  PushCPURegList(kCallerSaved);
  PushCPURegList(kCallerSavedFP);

  // We can use caller-saved registers as scratch values (except for argN).
  CPURegList tmp_list = kCallerSaved;
  CPURegList fp_tmp_list = kCallerSavedFP;
  tmp_list.Remove(arg0, arg1, arg2, arg3);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  { UseScratchRegisterScope temps(this);
    // If any of the arguments are the current stack pointer, allocate a new
    // register for them, and adjust the value to compensate for pushing the
    // caller-saved registers.
    bool arg0_sp = StackPointer().Aliases(arg0);
    bool arg1_sp = StackPointer().Aliases(arg1);
    bool arg2_sp = StackPointer().Aliases(arg2);
    bool arg3_sp = StackPointer().Aliases(arg3);
    if (arg0_sp || arg1_sp || arg2_sp || arg3_sp) {
      // Allocate a register to hold the original stack pointer value, to pass
      // to PrintfNoPreserve as an argument.
      Register arg_sp = temps.AcquireX();
      Add(arg_sp, StackPointer(),
          kCallerSaved.TotalSizeInBytes() + kCallerSavedFP.TotalSizeInBytes());
      if (arg0_sp) arg0 = Register::Create(arg_sp.code(), arg0.SizeInBits());
      if (arg1_sp) arg1 = Register::Create(arg_sp.code(), arg1.SizeInBits());
      if (arg2_sp) arg2 = Register::Create(arg_sp.code(), arg2.SizeInBits());
      if (arg3_sp) arg3 = Register::Create(arg_sp.code(), arg3.SizeInBits());
    }

    // Preserve NZCV.
    { UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Mrs(tmp, NZCV);
      Push(tmp, xzr);
    }

    PrintfNoPreserve(format, arg0, arg1, arg2, arg3);

    // Restore NZCV.
    { UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Pop(xzr, tmp);
      Msr(NZCV, tmp);
    }
  }

  PopCPURegList(kCallerSavedFP);
  PopCPURegList(kCallerSaved);

  TmpList()->set_list(old_tmp_list);
  FPTmpList()->set_list(old_fp_tmp_list);
}


void MacroAssembler::EmitFrameSetupForCodeAgePatching() {
  // TODO(jbramley): Other architectures use the internal memcpy to copy the
  // sequence. If this is a performance bottleneck, we should consider caching
  // the sequence and copying it in the same way.
  InstructionAccurateScope scope(this,
                                 kNoCodeAgeSequenceLength / kInstructionSize);
  DCHECK(jssp.Is(StackPointer()));
  EmitFrameSetupForCodeAgePatching(this);
}



void MacroAssembler::EmitCodeAgeSequence(Code* stub) {
  InstructionAccurateScope scope(this,
                                 kNoCodeAgeSequenceLength / kInstructionSize);
  DCHECK(jssp.Is(StackPointer()));
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
  __ sub(jssp, jssp, 4 * kXRegSize);
  __ sub(csp, csp, 4 * kXRegSize);
  __ stp(x1, cp, MemOperand(jssp, 0 * kXRegSize));
  __ stp(fp, lr, MemOperand(jssp, 2 * kXRegSize));
  __ add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);

  __ AssertSizeOfCodeGeneratedSince(&start, kNoCodeAgeSequenceLength);
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
  __ ldr_pcrel(ip0, kCodeAgeStubEntryOffset >> kLoadLiteralScaleLog2);
  __ adr(x0, &start);
  __ br(ip0);
  // IsCodeAgeSequence in codegen-arm64.cc assumes that the code generated up
  // until now (kCodeAgeStubEntryOffset) is the same for all code age sequences.
  __ AssertSizeOfCodeGeneratedSince(&start, kCodeAgeStubEntryOffset);
  if (stub) {
    __ dc64(reinterpret_cast<uint64_t>(stub->instruction_start()));
    __ AssertSizeOfCodeGeneratedSince(&start, kNoCodeAgeSequenceLength);
  }
}


bool MacroAssembler::IsYoungSequence(Isolate* isolate, byte* sequence) {
  bool is_young = isolate->code_aging_helper()->IsYoung(sequence);
  DCHECK(is_young ||
         isolate->code_aging_helper()->IsOld(sequence));
  return is_young;
}


void MacroAssembler::TruncatingDiv(Register result,
                                   Register dividend,
                                   int32_t divisor) {
  DCHECK(!AreAliased(result, dividend));
  DCHECK(result.Is32Bits() && dividend.Is32Bits());
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(static_cast<uint32_t>(divisor));
  Mov(result, mag.multiplier);
  Smull(result.X(), dividend, result);
  Asr(result.X(), result.X(), 32);
  bool neg = (mag.multiplier & (static_cast<uint32_t>(1) << 31)) != 0;
  if (divisor > 0 && neg) Add(result, result, dividend);
  if (divisor < 0 && !neg && mag.multiplier > 0) Sub(result, result, dividend);
  if (mag.shift > 0) Asr(result, result, mag.shift);
  Add(result, result, Operand(dividend, LSR, 31));
}


#undef __


UseScratchRegisterScope::~UseScratchRegisterScope() {
  available_->set_list(old_available_);
  availablefp_->set_list(old_availablefp_);
}


Register UseScratchRegisterScope::AcquireSameSizeAs(const Register& reg) {
  int code = AcquireNextAvailable(available_).code();
  return Register::Create(code, reg.SizeInBits());
}


FPRegister UseScratchRegisterScope::AcquireSameSizeAs(const FPRegister& reg) {
  int code = AcquireNextAvailable(availablefp_).code();
  return FPRegister::Create(code, reg.SizeInBits());
}


CPURegister UseScratchRegisterScope::AcquireNextAvailable(
    CPURegList* available) {
  CHECK(!available->IsEmpty());
  CPURegister result = available->PopLowestIndex();
  DCHECK(!AreAliased(result, xzr, csp));
  return result;
}


CPURegister UseScratchRegisterScope::UnsafeAcquire(CPURegList* available,
                                                   const CPURegister& reg) {
  DCHECK(available->IncludesAliasOf(reg));
  available->Remove(reg);
  return reg;
}


#define __ masm->


void InlineSmiCheckInfo::Emit(MacroAssembler* masm, const Register& reg,
                              const Label* smi_check) {
  Assembler::BlockPoolsScope scope(masm);
  if (reg.IsValid()) {
    DCHECK(smi_check->is_bound());
    DCHECK(reg.Is64Bits());

    // Encode the register (x0-x30) in the lowest 5 bits, then the offset to
    // 'check' in the other bits. The possible offset is limited in that we
    // use BitField to pack the data, and the underlying data type is a
    // uint32_t.
    uint32_t delta = __ InstructionsGeneratedSince(smi_check);
    __ InlineData(RegisterBits::encode(reg.code()) | DeltaBits::encode(delta));
  } else {
    DCHECK(!smi_check->is_bound());

    // An offset of 0 indicates that there is no patch site.
    __ InlineData(0);
  }
}


InlineSmiCheckInfo::InlineSmiCheckInfo(Address info)
    : reg_(NoReg), smi_check_(NULL) {
  InstructionSequence* inline_data = InstructionSequence::At(info);
  DCHECK(inline_data->IsInlineData());
  if (inline_data->IsInlineData()) {
    uint64_t payload = inline_data->InlineData();
    // We use BitField to decode the payload, and BitField can only handle
    // 32-bit values.
    DCHECK(is_uint32(payload));
    if (payload != 0) {
      int reg_code = RegisterBits::decode(payload);
      reg_ = Register::XRegFromCode(reg_code);
      uint64_t smi_check_delta = DeltaBits::decode(payload);
      DCHECK(smi_check_delta != 0);
      smi_check_ = inline_data->preceding(smi_check_delta);
    }
  }
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
