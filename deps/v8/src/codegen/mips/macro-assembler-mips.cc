// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_MIPS

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/heap/heap-inl.h"  // For MemoryChunk.
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/heap-number.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-code-manager.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/mips/macro-assembler-mips.h"
#endif

namespace v8 {
namespace internal {

static inline bool IsZero(const Operand& rt) {
  if (rt.is_reg()) {
    return rt.rm() == zero_reg;
  } else {
    return rt.immediate() == 0;
  }
}

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = kJSCallerSaved & ~exclusions;
  bytes += NumRegs(list) * kPointerSize;

  if (fp_mode == kSaveFPRegs) {
    bytes += NumRegs(kCallerSavedFPU) * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  int bytes = 0;
  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = kJSCallerSaved & ~exclusions;
  MultiPush(list);
  bytes += NumRegs(list) * kPointerSize;

  if (fp_mode == kSaveFPRegs) {
    MultiPushFPU(kCallerSavedFPU);
    bytes += NumRegs(kCallerSavedFPU) * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    MultiPopFPU(kCallerSavedFPU);
    bytes += NumRegs(kCallerSavedFPU) * kDoubleSize;
  }

  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = kJSCallerSaved & ~exclusions;
  MultiPop(list);
  bytes += NumRegs(list) * kPointerSize;

  return bytes;
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  lw(destination,
     MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition cond, Register src1,
                              const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  lw(destination,
     MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Push(ra, fp, marker_reg);
    Addu(fp, sp, Operand(kPointerSize));
  } else {
    Push(ra, fp);
    mov(fp, sp);
  }
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  int offset = -StandardFrameConstants::kContextOffset;
  if (function_reg.is_valid()) {
    Push(ra, fp, cp, function_reg);
    offset += kPointerSize;
  } else {
    Push(ra, fp, cp);
  }
  Addu(fp, sp, Operand(offset));
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  return kSafepointRegisterStackIndexMap[reg_code];
}

// Clobbers object, dst, value, and ra, if (ra_status == kRAHasBeenSaved)
// The register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register dst,
                                      RAStatus ra_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  DCHECK(!AreAliased(value, dst, t8, object));
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  Addu(dst, object, Operand(offset - kHeapObjectTag));
  if (emit_debug_code()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Label ok;
    And(t8, dst, Operand(kPointerSize - 1));
    Branch(&ok, eq, t8, Operand(zero_reg));
    stop();
    bind(&ok);
  }

  RecordWrite(object, dst, value, ra_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    li(value, Operand(bit_cast<int32_t>(kZapValue + 4)));
    li(dst, Operand(bit_cast<int32_t>(kZapValue + 8)));
  }
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  RegList regs = 0;
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs |= Register::from_code(i).bit();
    }
  }
  MultiPush(regs);
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  RegList regs = 0;
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs |= Register::from_code(i).bit();
    }
  }
  MultiPop(regs);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object, Register address,
                                             SaveFPRegsMode fp_mode) {
  EphemeronKeyBarrierDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kObject));
  Register slot_parameter(descriptor.GetRegisterParameter(
      EphemeronKeyBarrierDescriptor::kSlotAddress));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kFPMode));

  Push(object);
  Push(address);

  Pop(slot_parameter);
  Pop(object_parameter);

  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(isolate()->builtins()->builtin_handle(Builtins::kEphemeronKeyBarrier),
       RelocInfo::CODE_TARGET);
  RestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  CallRecordWriteStub(
      object, address, remembered_set_action, fp_mode,
      isolate()->builtins()->builtin_handle(Builtins::kRecordWrite),
      kNullAddress);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    Address wasm_target) {
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode,
                      Handle<Code>::null(), wasm_target);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    Handle<Code> code_target, Address wasm_target) {
  DCHECK_NE(code_target.is_null(), wasm_target == kNullAddress);
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  RecordWriteDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);
  Register object_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kObject));
  Register slot_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register remembered_set_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kFPMode));

  Push(object);
  Push(address);

  Pop(slot_parameter);
  Pop(object_parameter);

  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  if (code_target.is_null()) {
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(code_target, RelocInfo::CODE_TARGET);
  }

  RestoreRegisters(registers);
}

// Clobbers object, address, value, and ra, if (ra_status == kRAHasBeenSaved)
// The register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, RAStatus ra_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(!AreAliased(object, address, value, t8));
  DCHECK(!AreAliased(object, address, value, t9));

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    lw(scratch, MemOperand(address));
    Assert(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite, scratch,
           Operand(value));
  }

  if ((remembered_set_action == OMIT_REMEMBERED_SET &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  // Record the actual write.
  if (ra_status == kRAHasNotBeenSaved) {
    push(ra);
  }
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);
  if (ra_status == kRAHasNotBeenSaved) {
    pop(ra);
  }

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    li(address, Operand(bit_cast<int32_t>(kZapValue + 12)));
    li(value, Operand(bit_cast<int32_t>(kZapValue + 16)));
  }
}

// ---------------------------------------------------------------------------
// Instruction macros.

void TurboAssembler::Addu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    addu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiu(rd, rs, rt.immediate());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      addu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Subu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    subu(rd, rs, rt.rm());
  } else {
    if (is_int16(-rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiu(rd, rs, -rt.immediate());  // No subiu instr, use addiu(x, y, -imm).
    } else if (!(-rt.immediate() & kHiMask) &&
               !MustUseReg(rt.rmode())) {  // Use load
      // -imm and addu for cases where loading -imm generates one instruction.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, -rt.immediate());
      addu(rd, rs, scratch);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      subu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (IsMipsArchVariant(kLoongson)) {
      mult(rs, rt.rm());
      mflo(rd);
    } else {
      mul(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (IsMipsArchVariant(kLoongson)) {
      mult(rs, scratch);
      mflo(rd);
    } else {
      mul(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mul(Register rd_hi, Register rd_lo, Register rs,
                         const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      mult(rs, rt.rm());
      mflo(rd_lo);
      mfhi(rd_hi);
    } else {
      if (rd_lo == rs) {
        DCHECK(rd_hi != rs);
        DCHECK(rd_hi != rt.rm() && rd_lo != rt.rm());
        muh(rd_hi, rs, rt.rm());
        mul(rd_lo, rs, rt.rm());
      } else {
        DCHECK(rd_hi != rt.rm() && rd_lo != rt.rm());
        mul(rd_lo, rs, rt.rm());
        muh(rd_hi, rs, rt.rm());
      }
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      mult(rs, scratch);
      mflo(rd_lo);
      mfhi(rd_hi);
    } else {
      if (rd_lo == rs) {
        DCHECK(rd_hi != rs);
        DCHECK(rd_hi != scratch && rd_lo != scratch);
        muh(rd_hi, rs, scratch);
        mul(rd_lo, rs, scratch);
      } else {
        DCHECK(rd_hi != scratch && rd_lo != scratch);
        mul(rd_lo, rs, scratch);
        muh(rd_hi, rs, scratch);
      }
    }
  }
}

void TurboAssembler::Mulu(Register rd_hi, Register rd_lo, Register rs,
                          const Operand& rt) {
  Register reg = no_reg;
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (rt.is_reg()) {
    reg = rt.rm();
  } else {
    DCHECK(rs != scratch);
    reg = scratch;
    li(reg, rt);
  }

  if (!IsMipsArchVariant(kMips32r6)) {
    multu(rs, reg);
    mflo(rd_lo);
    mfhi(rd_hi);
  } else {
    if (rd_lo == rs) {
      DCHECK(rd_hi != rs);
      DCHECK(rd_hi != reg && rd_lo != reg);
      muhu(rd_hi, rs, reg);
      mulu(rd_lo, rs, reg);
    } else {
      DCHECK(rd_hi != reg && rd_lo != reg);
      mulu(rd_lo, rs, reg);
      muhu(rd_hi, rs, reg);
    }
  }
}

void TurboAssembler::Mulh(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      mult(rs, rt.rm());
      mfhi(rd);
    } else {
      muh(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      mult(rs, scratch);
      mfhi(rd);
    } else {
      muh(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mult(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mult(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    mult(rs, scratch);
  }
}

void TurboAssembler::Mulhu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      multu(rs, rt.rm());
      mfhi(rd);
    } else {
      muhu(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      multu(rs, scratch);
      mfhi(rd);
    } else {
      muhu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Multu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    multu(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    multu(rs, scratch);
  }
}

void TurboAssembler::Div(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    div(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    div(rs, scratch);
  }
}

void TurboAssembler::Div(Register rem, Register res, Register rs,
                         const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, rt.rm());
      mflo(res);
      mfhi(rem);
    } else {
      div(res, rs, rt.rm());
      mod(rem, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, scratch);
      mflo(res);
      mfhi(rem);
    } else {
      div(res, rs, scratch);
      mod(rem, rs, scratch);
    }
  }
}

void TurboAssembler::Div(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, rt.rm());
      mflo(res);
    } else {
      div(res, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, scratch);
      mflo(res);
    } else {
      div(res, rs, scratch);
    }
  }
}

void TurboAssembler::Mod(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, rt.rm());
      mfhi(rd);
    } else {
      mod(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      div(rs, scratch);
      mfhi(rd);
    } else {
      mod(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Modu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      divu(rs, rt.rm());
      mfhi(rd);
    } else {
      modu(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      divu(rs, scratch);
      mfhi(rd);
    } else {
      modu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Divu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divu(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    divu(rs, scratch);
  }
}

void TurboAssembler::Divu(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (!IsMipsArchVariant(kMips32r6)) {
      divu(rs, rt.rm());
      mflo(res);
    } else {
      divu(res, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (!IsMipsArchVariant(kMips32r6)) {
      divu(rs, scratch);
      mflo(res);
    } else {
      divu(res, rs, scratch);
    }
  }
}

void TurboAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    and_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      andi(rd, rs, rt.immediate());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      and_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Or(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    or_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      ori(rd, rs, rt.immediate());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      or_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Xor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    xor_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      xori(rd, rs, rt.immediate());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      xor_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Nor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    nor(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    nor(rd, rs, scratch);
  }
}

void TurboAssembler::Neg(Register rs, const Operand& rt) {
  subu(rs, zero_reg, rt.rm());
}

void TurboAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      slti(rd, rs, rt.immediate());
    } else {
      // li handles the relocation.
      BlockTrampolinePoolScope block_trampoline_pool(this);
      UseScratchRegisterScope temps(this);
      Register scratch = rd == at ? t8 : temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      slt(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Sltu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rs, rt.rm());
  } else {
    const uint32_t int16_min = std::numeric_limits<int16_t>::min();
    if (is_uint15(rt.immediate()) && !MustUseReg(rt.rmode())) {
      // Imm range is: [0, 32767].
      sltiu(rd, rs, rt.immediate());
    } else if (is_uint15(rt.immediate() - int16_min) &&
               !MustUseReg(rt.rmode())) {
      // Imm range is: [max_unsigned-32767,max_unsigned].
      sltiu(rd, rs, static_cast<uint16_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      BlockTrampolinePoolScope block_trampoline_pool(this);
      UseScratchRegisterScope temps(this);
      Register scratch = rd == at ? t8 : temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      sltu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Sle(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rt.rm(), rs);
  } else {
    // li handles the relocation.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    DCHECK(rs != scratch);
    li(scratch, rt);
    slt(rd, scratch, rs);
  }
  xori(rd, rd, 1);
}

void TurboAssembler::Sleu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rt.rm(), rs);
  } else {
    // li handles the relocation.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    DCHECK(rs != scratch);
    li(scratch, rt);
    sltu(rd, scratch, rs);
  }
  xori(rd, rd, 1);
}

void TurboAssembler::Sge(Register rd, Register rs, const Operand& rt) {
  Slt(rd, rs, rt);
  xori(rd, rd, 1);
}

void TurboAssembler::Sgeu(Register rd, Register rs, const Operand& rt) {
  Sltu(rd, rs, rt);
  xori(rd, rd, 1);
}

void TurboAssembler::Sgt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rt.rm(), rs);
  } else {
    // li handles the relocation.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    DCHECK(rs != scratch);
    li(scratch, rt);
    slt(rd, scratch, rs);
  }
}

void TurboAssembler::Sgtu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rt.rm(), rs);
  } else {
    // li handles the relocation.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    DCHECK(rs != scratch);
    li(scratch, rt);
    sltu(rd, scratch, rs);
  }
}

void TurboAssembler::Ror(Register rd, Register rs, const Operand& rt) {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    if (rt.is_reg()) {
      rotrv(rd, rs, rt.rm());
    } else {
      rotr(rd, rs, rt.immediate() & 0x1F);
    }
  } else {
    if (rt.is_reg()) {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      UseScratchRegisterScope temps(this);
      Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
      subu(scratch, zero_reg, rt.rm());
      sllv(scratch, rs, scratch);
      srlv(rd, rs, rt.rm());
      or_(rd, rd, scratch);
    } else {
      if (rt.immediate() == 0) {
        srl(rd, rs, 0);
      } else {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        srl(scratch, rs, rt.immediate() & 0x1F);
        sll(rd, rs, (0x20 - (rt.immediate() & 0x1F)) & 0x1F);
        or_(rd, rd, scratch);
      }
    }
  }
}

void MacroAssembler::Pref(int32_t hint, const MemOperand& rs) {
  if (IsMipsArchVariant(kLoongson)) {
    lw(zero_reg, rs);
  } else {
    pref(hint, rs);
  }
}

void TurboAssembler::Lsa(Register rd, Register rt, Register rs, uint8_t sa,
                         Register scratch) {
  DCHECK(sa >= 1 && sa <= 31);
  if (IsMipsArchVariant(kMips32r6) && sa <= 4) {
    lsa(rd, rt, rs, sa - 1);
  } else {
    Register tmp = rd == rt ? scratch : rd;
    DCHECK(tmp != rt);
    sll(tmp, rs, sa);
    Addu(rd, rt, tmp);
  }
}

void TurboAssembler::Bovc(Register rs, Register rt, Label* L) {
  if (is_trampoline_emitted()) {
    Label skip;
    bnvc(rs, rt, &skip);
    BranchLong(L, PROTECT);
    bind(&skip);
  } else {
    bovc(rs, rt, L);
  }
}

void TurboAssembler::Bnvc(Register rs, Register rt, Label* L) {
  if (is_trampoline_emitted()) {
    Label skip;
    bovc(rs, rt, &skip);
    BranchLong(L, PROTECT);
    bind(&skip);
  } else {
    bnvc(rs, rt, L);
  }
}

// ------------Pseudo-instructions-------------

// Word Swap Byte
void TurboAssembler::ByteSwapSigned(Register dest, Register src,
                                    int operand_size) {
  DCHECK(operand_size == 2 || operand_size == 4);

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    if (operand_size == 2) {
      wsbh(dest, src);
      seh(dest, dest);
    } else {
      wsbh(dest, src);
      rotr(dest, dest, 16);
    }
  } else if (IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kLoongson)) {
    if (operand_size == 2) {
      DCHECK(src != at && dest != at);
      srl(at, src, 8);
      andi(at, at, 0xFF);
      sll(dest, src, 8);
      or_(dest, dest, at);

      // Sign-extension
      sll(dest, dest, 16);
      sra(dest, dest, 16);
    } else {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Register tmp = at;
      Register tmp2 = t8;
      DCHECK(dest != tmp && dest != tmp2);
      DCHECK(src != tmp && src != tmp2);

      andi(tmp2, src, 0xFF);
      sll(tmp, tmp2, 24);

      andi(tmp2, src, 0xFF00);
      sll(tmp2, tmp2, 8);
      or_(tmp, tmp, tmp2);

      srl(tmp2, src, 8);
      andi(tmp2, tmp2, 0xFF00);
      or_(tmp, tmp, tmp2);

      srl(tmp2, src, 24);
      or_(dest, tmp, tmp2);
    }
  }
}

void TurboAssembler::ByteSwapUnsigned(Register dest, Register src,
                                      int operand_size) {
  DCHECK_EQ(operand_size, 2);

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    wsbh(dest, src);
    andi(dest, dest, 0xFFFF);
  } else if (IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kLoongson)) {
    DCHECK(src != at && dest != at);
    srl(at, src, 8);
    andi(at, at, 0xFF);
    sll(dest, src, 8);
    or_(dest, dest, at);

    // Zero-extension
    andi(dest, dest, 0xFFFF);
  }
}

void TurboAssembler::Ulw(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (IsMipsArchVariant(kMips32r6)) {
    lw(rd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    DCHECK(kMipsLwrOffset <= 3 && kMipsLwlOffset <= 3);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 3 fits into int16_t.
    AdjustBaseAndOffset(&source, OffsetAccessType::TWO_ACCESSES, 3);
    if (rd != source.rm()) {
      lwr(rd, MemOperand(source.rm(), source.offset() + kMipsLwrOffset));
      lwl(rd, MemOperand(source.rm(), source.offset() + kMipsLwlOffset));
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      lwr(scratch, MemOperand(rs.rm(), rs.offset() + kMipsLwrOffset));
      lwl(scratch, MemOperand(rs.rm(), rs.offset() + kMipsLwlOffset));
      mov(rd, scratch);
    }
  }
}

void TurboAssembler::Usw(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  DCHECK(rd != rs.rm());
  if (IsMipsArchVariant(kMips32r6)) {
    sw(rd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    DCHECK(kMipsSwrOffset <= 3 && kMipsSwlOffset <= 3);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 3 fits into int16_t.
    AdjustBaseAndOffset(&source, OffsetAccessType::TWO_ACCESSES, 3);
    swr(rd, MemOperand(source.rm(), source.offset() + kMipsSwrOffset));
    swl(rd, MemOperand(source.rm(), source.offset() + kMipsSwlOffset));
  }
}

void TurboAssembler::Ulh(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (IsMipsArchVariant(kMips32r6)) {
    lh(rd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(&source, OffsetAccessType::TWO_ACCESSES, 1);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (source.rm() == scratch) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      lb(rd, MemOperand(source.rm(), source.offset() + 1));
      lbu(scratch, source);
#elif defined(V8_TARGET_BIG_ENDIAN)
      lb(rd, source);
      lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
#endif
    } else {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      lbu(scratch, source);
      lb(rd, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
      lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
      lb(rd, source);
#endif
    }
    sll(rd, rd, 8);
    or_(rd, rd, scratch);
  }
}

void TurboAssembler::Ulhu(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (IsMipsArchVariant(kMips32r6)) {
    lhu(rd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(&source, OffsetAccessType::TWO_ACCESSES, 1);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (source.rm() == scratch) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      lbu(rd, MemOperand(source.rm(), source.offset() + 1));
      lbu(scratch, source);
#elif defined(V8_TARGET_BIG_ENDIAN)
      lbu(rd, source);
      lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
#endif
    } else {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      lbu(scratch, source);
      lbu(rd, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
      lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
      lbu(rd, source);
#endif
    }
    sll(rd, rd, 8);
    or_(rd, rd, scratch);
  }
}

void TurboAssembler::Ush(Register rd, const MemOperand& rs, Register scratch) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  DCHECK(rs.rm() != scratch);
  DCHECK(scratch != at);
  if (IsMipsArchVariant(kMips32r6)) {
    sh(rd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(&source, OffsetAccessType::TWO_ACCESSES, 1);

    if (scratch != rd) {
      mov(scratch, rd);
    }

#if defined(V8_TARGET_LITTLE_ENDIAN)
    sb(scratch, source);
    srl(scratch, scratch, 8);
    sb(scratch, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
    sb(scratch, MemOperand(source.rm(), source.offset() + 1));
    srl(scratch, scratch, 8);
    sb(scratch, source);
#endif
  }
}

void TurboAssembler::Ulwc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  if (IsMipsArchVariant(kMips32r6)) {
    lwc1(fd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    Ulw(scratch, rs);
    mtc1(scratch, fd);
  }
}

void TurboAssembler::Uswc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  if (IsMipsArchVariant(kMips32r6)) {
    swc1(fd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    mfc1(scratch, fd);
    Usw(scratch, rs);
  }
}

void TurboAssembler::Uldc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  DCHECK(scratch != at);
  if (IsMipsArchVariant(kMips32r6)) {
    Ldc1(fd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    Ulw(scratch, MemOperand(rs.rm(), rs.offset() + Register::kMantissaOffset));
    mtc1(scratch, fd);
    Ulw(scratch, MemOperand(rs.rm(), rs.offset() + Register::kExponentOffset));
    Mthc1(scratch, fd);
  }
}

void TurboAssembler::Usdc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  DCHECK(scratch != at);
  if (IsMipsArchVariant(kMips32r6)) {
    Sdc1(fd, rs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    mfc1(scratch, fd);
    Usw(scratch, MemOperand(rs.rm(), rs.offset() + Register::kMantissaOffset));
    Mfhc1(scratch, fd);
    Usw(scratch, MemOperand(rs.rm(), rs.offset() + Register::kExponentOffset));
  }
}

void TurboAssembler::Ldc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // load to two 32-bit loads.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK(Register::kMantissaOffset <= 4 && Register::kExponentOffset <= 4);
    MemOperand tmp = src;
    AdjustBaseAndOffset(&tmp, OffsetAccessType::TWO_ACCESSES);
    lwc1(fd, MemOperand(tmp.rm(), tmp.offset() + Register::kMantissaOffset));
    if (IsFp32Mode()) {  // fp32 mode.
      FPURegister nextfpreg = FPURegister::from_code(fd.code() + 1);
      lwc1(nextfpreg,
           MemOperand(tmp.rm(), tmp.offset() + Register::kExponentOffset));
    } else {
      DCHECK(IsFp64Mode() || IsFpxxMode());
      // Currently we support FPXX and FP64 on Mips32r2 and Mips32r6
      DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(src.rm() != scratch);
      lw(scratch,
         MemOperand(tmp.rm(), tmp.offset() + Register::kExponentOffset));
      Mthc1(scratch, fd);
    }
  }
  CheckTrampolinePoolQuick(1);
}

void TurboAssembler::Sdc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // store to two 32-bit stores.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK(Register::kMantissaOffset <= 4 && Register::kExponentOffset <= 4);
    MemOperand tmp = src;
    AdjustBaseAndOffset(&tmp, OffsetAccessType::TWO_ACCESSES);
    swc1(fd, MemOperand(tmp.rm(), tmp.offset() + Register::kMantissaOffset));
    if (IsFp32Mode()) {  // fp32 mode.
      FPURegister nextfpreg = FPURegister::from_code(fd.code() + 1);
      swc1(nextfpreg,
           MemOperand(tmp.rm(), tmp.offset() + Register::kExponentOffset));
    } else {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      DCHECK(IsFp64Mode() || IsFpxxMode());
      // Currently we support FPXX and FP64 on Mips32r2 and Mips32r6
      DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
      DCHECK(src.rm() != t8);
      Mfhc1(t8, fd);
      sw(t8, MemOperand(tmp.rm(), tmp.offset() + Register::kExponentOffset));
    }
  }
  CheckTrampolinePoolQuick(1);
}

void TurboAssembler::Lw(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(&source);
  lw(rd, source);
}

void TurboAssembler::Sw(Register rd, const MemOperand& rs) {
  MemOperand dest = rs;
  AdjustBaseAndOffset(&dest);
  sw(rd, dest);
}

void TurboAssembler::Ll(Register rd, const MemOperand& rs) {
  bool is_one_instruction = IsMipsArchVariant(kMips32r6)
                                ? is_int9(rs.offset())
                                : is_int16(rs.offset());
  if (is_one_instruction) {
    ll(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    addu(scratch, scratch, rs.rm());
    ll(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::Sc(Register rd, const MemOperand& rs) {
  bool is_one_instruction = IsMipsArchVariant(kMips32r6)
                                ? is_int9(rs.offset())
                                : is_int16(rs.offset());
  if (is_one_instruction) {
    sc(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    addu(scratch, scratch, rs.rm());
    sc(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::li(Register dst, Handle<HeapObject> value, LiFlags mode) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(dst, value);
    return;
  }
  li(dst, Operand(value), mode);
}

void TurboAssembler::li(Register dst, ExternalReference value, LiFlags mode) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadExternalReference(dst, value);
    return;
  }
  li(dst, Operand(value), mode);
}

void TurboAssembler::li(Register dst, const StringConstantBase* string,
                        LiFlags mode) {
  li(dst, Operand::EmbeddedStringConstant(string), mode);
}

void TurboAssembler::li(Register rd, Operand j, LiFlags mode) {
  DCHECK(!j.is_reg());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (!MustUseReg(j.rmode()) && mode == OPTIMIZE_SIZE) {
    // Normal load of an immediate value which does not need Relocation Info.
    if (is_int16(j.immediate())) {
      addiu(rd, zero_reg, j.immediate());
    } else if (!(j.immediate() & kHiMask)) {
      ori(rd, zero_reg, j.immediate());
    } else {
      lui(rd, (j.immediate() >> kLuiShift) & kImm16Mask);
      if (j.immediate() & kImm16Mask) {
        ori(rd, rd, (j.immediate() & kImm16Mask));
      }
    }
  } else {
    int32_t immediate;
    if (j.IsHeapObjectRequest()) {
      RequestHeapObject(j.heap_object_request());
      immediate = 0;
    } else {
      immediate = j.immediate();
    }

    if (MustUseReg(j.rmode())) {
      RecordRelocInfo(j.rmode(), immediate);
    }
    // We always need the same number of instructions as we may need to patch
    // this code to load another value which may need 2 instructions to load.

    lui(rd, (immediate >> kLuiShift) & kImm16Mask);
    ori(rd, rd, (immediate & kImm16Mask));
  }
}

void TurboAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      sw(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}

void TurboAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  addiu(sp, sp, stack_offset);
}

void TurboAssembler::MultiPushFPU(RegList regs) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kDoubleSize;
      Sdc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
    }
  }
}

void TurboAssembler::MultiPopFPU(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      Ldc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addiu(sp, sp, stack_offset);
}

void TurboAssembler::AddPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high,
                             Register right_low, Register right_high,
                             Register scratch1, Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch3 = t8;
  Addu(scratch1, left_low, right_low);
  Sltu(scratch3, scratch1, left_low);
  Addu(scratch2, left_high, right_high);
  Addu(dst_high, scratch2, scratch3);
  Move(dst_low, scratch1);
}

void TurboAssembler::AddPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high, int32_t imm,
                             Register scratch1, Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch3 = t8;
  li(dst_low, Operand(imm));
  sra(dst_high, dst_low, 31);
  Addu(scratch1, left_low, dst_low);
  Sltu(scratch3, scratch1, left_low);
  Addu(scratch2, left_high, dst_high);
  Addu(dst_high, scratch2, scratch3);
  Move(dst_low, scratch1);
}

void TurboAssembler::SubPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high,
                             Register right_low, Register right_high,
                             Register scratch1, Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch3 = t8;
  Sltu(scratch3, left_low, right_low);
  Subu(scratch1, left_low, right_low);
  Subu(scratch2, left_high, right_high);
  Subu(dst_high, scratch2, scratch3);
  Move(dst_low, scratch1);
}

void TurboAssembler::AndPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high,
                             Register right_low, Register right_high) {
  And(dst_low, left_low, right_low);
  And(dst_high, left_high, right_high);
}

void TurboAssembler::OrPair(Register dst_low, Register dst_high,
                            Register left_low, Register left_high,
                            Register right_low, Register right_high) {
  Or(dst_low, left_low, right_low);
  Or(dst_high, left_high, right_high);
}
void TurboAssembler::XorPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high,
                             Register right_low, Register right_high) {
  Xor(dst_low, left_low, right_low);
  Xor(dst_high, left_high, right_high);
}

void TurboAssembler::MulPair(Register dst_low, Register dst_high,
                             Register left_low, Register left_high,
                             Register right_low, Register right_high,
                             Register scratch1, Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch3 = t8;
  Mulu(scratch2, scratch1, left_low, right_low);
  Mul(scratch3, left_low, right_high);
  Addu(scratch2, scratch2, scratch3);
  Mul(scratch3, left_high, right_low);
  Addu(dst_high, scratch2, scratch3);
  Move(dst_low, scratch1);
}

void TurboAssembler::ShlPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift, Register scratch1,
                             Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label done;
  Register scratch3 = t8;
  And(scratch3, shift, 0x3F);
  sllv(dst_low, src_low, scratch3);
  Nor(scratch2, zero_reg, scratch3);
  srl(scratch1, src_low, 1);
  srlv(scratch1, scratch1, scratch2);
  sllv(dst_high, src_high, scratch3);
  Or(dst_high, dst_high, scratch1);
  And(scratch1, scratch3, 32);
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    Branch(&done, eq, scratch1, Operand(zero_reg));
    mov(dst_high, dst_low);
    mov(dst_low, zero_reg);
  } else {
    movn(dst_high, dst_low, scratch1);
    movn(dst_low, zero_reg, scratch1);
  }
  bind(&done);
}

void TurboAssembler::ShlPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift, Register scratch) {
  DCHECK_NE(dst_low, src_low);
  DCHECK_NE(dst_high, src_low);
  shift = shift & 0x3F;
  if (shift == 0) {
    mov(dst_high, src_high);
    mov(dst_low, src_low);
  } else if (shift < 32) {
    if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
      DCHECK_NE(dst_high, src_high);
      srl(dst_high, src_low, 32 - shift);
      Ins(dst_high, src_high, shift, 32 - shift);
      sll(dst_low, src_low, shift);
    } else {
      sll(dst_high, src_high, shift);
      sll(dst_low, src_low, shift);
      srl(scratch, src_low, 32 - shift);
      Or(dst_high, dst_high, scratch);
    }
  } else if (shift == 32) {
    mov(dst_low, zero_reg);
    mov(dst_high, src_low);
  } else {
    shift = shift - 32;
    mov(dst_low, zero_reg);
    sll(dst_high, src_low, shift);
  }
}

void TurboAssembler::ShrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift, Register scratch1,
                             Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label done;
  Register scratch3 = t8;
  And(scratch3, shift, 0x3F);
  srlv(dst_high, src_high, scratch3);
  Nor(scratch2, zero_reg, scratch3);
  sll(scratch1, src_high, 1);
  sllv(scratch1, scratch1, scratch2);
  srlv(dst_low, src_low, scratch3);
  Or(dst_low, dst_low, scratch1);
  And(scratch1, scratch3, 32);
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    Branch(&done, eq, scratch1, Operand(zero_reg));
    mov(dst_low, dst_high);
    mov(dst_high, zero_reg);
  } else {
    movn(dst_low, dst_high, scratch1);
    movn(dst_high, zero_reg, scratch1);
  }
  bind(&done);
}

void TurboAssembler::ShrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift, Register scratch) {
  DCHECK_NE(dst_low, src_high);
  DCHECK_NE(dst_high, src_high);
  shift = shift & 0x3F;
  if (shift == 0) {
    mov(dst_low, src_low);
    mov(dst_high, src_high);
  } else if (shift < 32) {
    if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
      srl(dst_low, src_low, shift);
      Ins(dst_low, src_high, 32 - shift, shift);
      srl(dst_high, src_high, shift);
    } else {
      srl(dst_low, src_low, shift);
      srl(dst_high, src_high, shift);
      shift = 32 - shift;
      sll(scratch, src_high, shift);
      Or(dst_low, dst_low, scratch);
    }
  } else if (shift == 32) {
    mov(dst_high, zero_reg);
    mov(dst_low, src_high);
  } else {
    shift = shift - 32;
    mov(dst_high, zero_reg);
    srl(dst_low, src_high, shift);
  }
}

void TurboAssembler::SarPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift, Register scratch1,
                             Register scratch2) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label done;
  Register scratch3 = t8;
  And(scratch3, shift, 0x3F);
  srav(dst_high, src_high, scratch3);
  Nor(scratch2, zero_reg, scratch3);
  sll(scratch1, src_high, 1);
  sllv(scratch1, scratch1, scratch2);
  srlv(dst_low, src_low, scratch3);
  Or(dst_low, dst_low, scratch1);
  And(scratch1, scratch3, 32);
  Branch(&done, eq, scratch1, Operand(zero_reg));
  mov(dst_low, dst_high);
  sra(dst_high, dst_high, 31);
  bind(&done);
}

void TurboAssembler::SarPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift, Register scratch) {
  DCHECK_NE(dst_low, src_high);
  DCHECK_NE(dst_high, src_high);
  shift = shift & 0x3F;
  if (shift == 0) {
    mov(dst_low, src_low);
    mov(dst_high, src_high);
  } else if (shift < 32) {
    if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
      srl(dst_low, src_low, shift);
      Ins(dst_low, src_high, 32 - shift, shift);
      sra(dst_high, src_high, shift);
    } else {
      srl(dst_low, src_low, shift);
      sra(dst_high, src_high, shift);
      shift = 32 - shift;
      sll(scratch, src_high, shift);
      Or(dst_low, dst_low, scratch);
    }
  } else if (shift == 32) {
    sra(dst_high, src_high, 31);
    mov(dst_low, src_high);
  } else {
    shift = shift - 32;
    sra(dst_high, src_high, 31);
    sra(dst_low, src_high, shift);
  }
}

void TurboAssembler::Ext(Register rt, Register rs, uint16_t pos,
                         uint16_t size) {
  DCHECK_LT(pos, 32);
  DCHECK_LT(pos + size, 33);

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    ext_(rt, rs, pos, size);
  } else {
    // Move rs to rt and shift it left then right to get the
    // desired bitfield on the right side and zeroes on the left.
    int shift_left = 32 - (pos + size);
    sll(rt, rs, shift_left);  // Acts as a move if shift_left == 0.

    int shift_right = 32 - size;
    if (shift_right > 0) {
      srl(rt, rt, shift_right);
    }
  }
}

void TurboAssembler::Ins(Register rt, Register rs, uint16_t pos,
                         uint16_t size) {
  DCHECK_LT(pos, 32);
  DCHECK_LE(pos + size, 32);
  DCHECK_NE(size, 0);

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    ins_(rt, rs, pos, size);
  } else {
    DCHECK(rt != t8 && rs != t8);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Subu(scratch, zero_reg, Operand(1));
    srl(scratch, scratch, 32 - size);
    and_(t8, rs, scratch);
    sll(t8, t8, pos);
    sll(scratch, scratch, pos);
    nor(scratch, scratch, zero_reg);
    and_(scratch, rt, scratch);
    or_(rt, t8, scratch);
  }
}

void TurboAssembler::ExtractBits(Register dest, Register source, Register pos,
                                 int size, bool sign_extend) {
  srav(dest, source, pos);
  Ext(dest, dest, 0, size);
  if (size == 8) {
    if (sign_extend) {
      Seb(dest, dest);
    }
  } else if (size == 16) {
    if (sign_extend) {
      Seh(dest, dest);
    }
  } else {
    UNREACHABLE();
  }
}

void TurboAssembler::InsertBits(Register dest, Register source, Register pos,
                                int size) {
  Ror(dest, dest, pos);
  Ins(dest, source, 0, size);
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Subu(scratch, zero_reg, pos);
    Ror(dest, dest, scratch);
  }
}

void TurboAssembler::Seb(Register rd, Register rt) {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    seb(rd, rt);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kLoongson));
    sll(rd, rt, 24);
    sra(rd, rd, 24);
  }
}

void TurboAssembler::Seh(Register rd, Register rt) {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    seh(rd, rt);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kLoongson));
    sll(rd, rt, 16);
    sra(rd, rd, 16);
  }
}

void TurboAssembler::Neg_s(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kMips32r6)) {
    // r6 neg_s changes the sign for NaN-like operands as well.
    neg_s(fd, fs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Label is_nan, done;
    Register scratch1 = t8;
    Register scratch2 = t9;
    CompareIsNanF32(fs, fs);
    BranchTrueShortF(&is_nan);
    Branch(USE_DELAY_SLOT, &done);
    // For NaN input, neg_s will return the same NaN value,
    // while the sign has to be changed separately.
    neg_s(fd, fs);  // In delay slot.
    bind(&is_nan);
    mfc1(scratch1, fs);
    li(scratch2, kBinary32SignMask);
    Xor(scratch1, scratch1, scratch2);
    mtc1(scratch1, fd);
    bind(&done);
  }
}

void TurboAssembler::Neg_d(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kMips32r6)) {
    // r6 neg_d changes the sign for NaN-like operands as well.
    neg_d(fd, fs);
  } else {
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kLoongson));
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Label is_nan, done;
    Register scratch1 = t8;
    Register scratch2 = t9;
    CompareIsNanF64(fs, fs);
    BranchTrueShortF(&is_nan);
    Branch(USE_DELAY_SLOT, &done);
    // For NaN input, neg_d will return the same NaN value,
    // while the sign has to be changed separately.
    neg_d(fd, fs);  // In delay slot.
    bind(&is_nan);
    Move(fd, fs);
    Mfhc1(scratch1, fd);
    li(scratch2, HeapNumber::kSignMask);
    Xor(scratch1, scratch1, scratch2);
    Mthc1(scratch1, fd);
    bind(&done);
  }
}

void TurboAssembler::Cvt_d_uw(FPURegister fd, Register rs,
                              FPURegister scratch) {
  // In FP64Mode we do conversion from long.
  if (IsFp64Mode()) {
    mtc1(rs, scratch);
    Mthc1(zero_reg, scratch);
    cvt_d_l(fd, scratch);
  } else {
    // Convert rs to a FP value in fd.
    DCHECK(fd != scratch);
    DCHECK(rs != at);

    Label msb_clear, conversion_done;
    // For a value which is < 2^31, regard it as a signed positve word.
    Branch(&msb_clear, ge, rs, Operand(zero_reg), USE_DELAY_SLOT);
    mtc1(rs, fd);
    {
      UseScratchRegisterScope temps(this);
      Register scratch1 = temps.Acquire();
      li(scratch1, 0x41F00000);  // FP value: 2^32.

      // For unsigned inputs > 2^31, we convert to double as a signed int32,
      // then add 2^32 to move it back to unsigned value in range 2^31..2^31-1.
      mtc1(zero_reg, scratch);
      Mthc1(scratch1, scratch);
    }

    cvt_d_w(fd, fd);

    Branch(USE_DELAY_SLOT, &conversion_done);
    add_d(fd, fd, scratch);

    bind(&msb_clear);
    cvt_d_w(fd, fd);

    bind(&conversion_done);
  }
}

void TurboAssembler::Trunc_uw_d(FPURegister fd, FPURegister fs,
                                FPURegister scratch) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Trunc_uw_d(t8, fs, scratch);
  mtc1(t8, fd);
}

void TurboAssembler::Trunc_uw_s(FPURegister fd, FPURegister fs,
                                FPURegister scratch) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Trunc_uw_s(t8, fs, scratch);
  mtc1(t8, fd);
}

void TurboAssembler::Trunc_w_d(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kLoongson) && fd == fs) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Mfhc1(t8, fs);
    trunc_w_d(fd, fs);
    Mthc1(t8, fs);
  } else {
    trunc_w_d(fd, fs);
  }
}

void TurboAssembler::Round_w_d(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kLoongson) && fd == fs) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Mfhc1(t8, fs);
    round_w_d(fd, fs);
    Mthc1(t8, fs);
  } else {
    round_w_d(fd, fs);
  }
}

void TurboAssembler::Floor_w_d(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kLoongson) && fd == fs) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Mfhc1(t8, fs);
    floor_w_d(fd, fs);
    Mthc1(t8, fs);
  } else {
    floor_w_d(fd, fs);
  }
}

void TurboAssembler::Ceil_w_d(FPURegister fd, FPURegister fs) {
  if (IsMipsArchVariant(kLoongson) && fd == fs) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Mfhc1(t8, fs);
    ceil_w_d(fd, fs);
    Mthc1(t8, fs);
  } else {
    ceil_w_d(fd, fs);
  }
}

void TurboAssembler::Trunc_uw_d(Register rd, FPURegister fs,
                                FPURegister scratch) {
  DCHECK(fs != scratch);
  DCHECK(rd != at);

  {
    // Load 2^31 into scratch as its float representation.
    UseScratchRegisterScope temps(this);
    Register scratch1 = temps.Acquire();
    li(scratch1, 0x41E00000);
    mtc1(zero_reg, scratch);
    Mthc1(scratch1, scratch);
  }
  // Test if scratch > fs.
  // If fs < 2^31 we can convert it normally.
  Label simple_convert;
  CompareF64(OLT, fs, scratch);
  BranchTrueShortF(&simple_convert);

  // First we subtract 2^31 from fs, then trunc it to rd
  // and add 2^31 to rd.
  sub_d(scratch, fs, scratch);
  trunc_w_d(scratch, scratch);
  mfc1(rd, scratch);
  Or(rd, rd, 1 << 31);

  Label done;
  Branch(&done);
  // Simple conversion.
  bind(&simple_convert);
  trunc_w_d(scratch, fs);
  mfc1(rd, scratch);

  bind(&done);
}

void TurboAssembler::Trunc_uw_s(Register rd, FPURegister fs,
                                FPURegister scratch) {
  DCHECK(fs != scratch);
  DCHECK(rd != at);

  {
    // Load 2^31 into scratch as its float representation.
    UseScratchRegisterScope temps(this);
    Register scratch1 = temps.Acquire();
    li(scratch1, 0x4F000000);
    mtc1(scratch1, scratch);
  }
  // Test if scratch > fs.
  // If fs < 2^31 we can convert it normally.
  Label simple_convert;
  CompareF32(OLT, fs, scratch);
  BranchTrueShortF(&simple_convert);

  // First we subtract 2^31 from fs, then trunc it to rd
  // and add 2^31 to rd.
  sub_s(scratch, fs, scratch);
  trunc_w_s(scratch, scratch);
  mfc1(rd, scratch);
  Or(rd, rd, 1 << 31);

  Label done;
  Branch(&done);
  // Simple conversion.
  bind(&simple_convert);
  trunc_w_s(scratch, fs);
  mfc1(rd, scratch);

  bind(&done);
}

template <typename RoundFunc>
void TurboAssembler::RoundDouble(FPURegister dst, FPURegister src,
                                 FPURoundingMode mode, RoundFunc round) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = t8;
  Register scratch2 = t9;
  if (IsMipsArchVariant(kMips32r6)) {
    cfc1(scratch, FCSR);
    li(at, Operand(mode));
    ctc1(at, FCSR);
    rint_d(dst, src);
    ctc1(scratch, FCSR);
  } else {
    Label done;
    Mfhc1(scratch, src);
    Ext(at, scratch, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
    Branch(USE_DELAY_SLOT, &done, hs, at,
           Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits));
    mov_d(dst, src);
    round(this, dst, src);
    Move(at, scratch2, dst);
    or_(at, at, scratch2);
    Branch(USE_DELAY_SLOT, &done, ne, at, Operand(zero_reg));
    cvt_d_l(dst, dst);
    srl(at, scratch, 31);
    sll(at, at, 31);
    Mthc1(at, dst);
    bind(&done);
  }
}

void TurboAssembler::Floor_d_d(FPURegister dst, FPURegister src) {
  RoundDouble(dst, src, mode_floor,
              [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
                tasm->floor_l_d(dst, src);
              });
}

void TurboAssembler::Ceil_d_d(FPURegister dst, FPURegister src) {
  RoundDouble(dst, src, mode_ceil,
              [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
                tasm->ceil_l_d(dst, src);
              });
}

void TurboAssembler::Trunc_d_d(FPURegister dst, FPURegister src) {
  RoundDouble(dst, src, mode_trunc,
              [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
                tasm->trunc_l_d(dst, src);
              });
}

void TurboAssembler::Round_d_d(FPURegister dst, FPURegister src) {
  RoundDouble(dst, src, mode_round,
              [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
                tasm->round_l_d(dst, src);
              });
}

template <typename RoundFunc>
void TurboAssembler::RoundFloat(FPURegister dst, FPURegister src,
                                FPURoundingMode mode, RoundFunc round) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = t8;
  if (IsMipsArchVariant(kMips32r6)) {
    cfc1(scratch, FCSR);
    li(at, Operand(mode));
    ctc1(at, FCSR);
    rint_s(dst, src);
    ctc1(scratch, FCSR);
  } else {
    int32_t kFloat32ExponentBias = 127;
    int32_t kFloat32MantissaBits = 23;
    int32_t kFloat32ExponentBits = 8;
    Label done;
    mfc1(scratch, src);
    Ext(at, scratch, kFloat32MantissaBits, kFloat32ExponentBits);
    Branch(USE_DELAY_SLOT, &done, hs, at,
           Operand(kFloat32ExponentBias + kFloat32MantissaBits));
    mov_s(dst, src);
    round(this, dst, src);
    mfc1(at, dst);
    Branch(USE_DELAY_SLOT, &done, ne, at, Operand(zero_reg));
    cvt_s_w(dst, dst);
    srl(at, scratch, 31);
    sll(at, at, 31);
    mtc1(at, dst);
    bind(&done);
  }
}

void TurboAssembler::Floor_s_s(FPURegister dst, FPURegister src) {
  RoundFloat(dst, src, mode_floor,
             [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
               tasm->floor_w_s(dst, src);
             });
}

void TurboAssembler::Ceil_s_s(FPURegister dst, FPURegister src) {
  RoundFloat(dst, src, mode_ceil,
             [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
               tasm->ceil_w_s(dst, src);
             });
}

void TurboAssembler::Trunc_s_s(FPURegister dst, FPURegister src) {
  RoundFloat(dst, src, mode_trunc,
             [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
               tasm->trunc_w_s(dst, src);
             });
}

void TurboAssembler::Round_s_s(FPURegister dst, FPURegister src) {
  RoundFloat(dst, src, mode_round,
             [](TurboAssembler* tasm, FPURegister dst, FPURegister src) {
               tasm->round_w_s(dst, src);
             });
}

void TurboAssembler::Mthc1(Register rt, FPURegister fs) {
  if (IsFp32Mode()) {
    mtc1(rt, fs.high());
  } else {
    DCHECK(IsFp64Mode() || IsFpxxMode());
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
    mthc1(rt, fs);
  }
}

void TurboAssembler::Mfhc1(Register rt, FPURegister fs) {
  if (IsFp32Mode()) {
    mfc1(rt, fs.high());
  } else {
    DCHECK(IsFp64Mode() || IsFpxxMode());
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
    mfhc1(rt, fs);
  }
}

void TurboAssembler::Madd_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  if (IsMipsArchVariant(kMips32r2)) {
    madd_s(fd, fr, fs, ft);
  } else {
    DCHECK(fr != scratch && fs != scratch && ft != scratch);
    mul_s(scratch, fs, ft);
    add_s(fd, fr, scratch);
  }
}

void TurboAssembler::Madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  if (IsMipsArchVariant(kMips32r2)) {
    madd_d(fd, fr, fs, ft);
  } else {
    DCHECK(fr != scratch && fs != scratch && ft != scratch);
    mul_d(scratch, fs, ft);
    add_d(fd, fr, scratch);
  }
}

void TurboAssembler::Msub_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  if (IsMipsArchVariant(kMips32r2)) {
    msub_s(fd, fr, fs, ft);
  } else {
    DCHECK(fr != scratch && fs != scratch && ft != scratch);
    mul_s(scratch, fs, ft);
    sub_s(fd, scratch, fr);
  }
}

void TurboAssembler::Msub_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  if (IsMipsArchVariant(kMips32r2)) {
    msub_d(fd, fr, fs, ft);
  } else {
    DCHECK(fr != scratch && fs != scratch && ft != scratch);
    mul_d(scratch, fs, ft);
    sub_d(fd, scratch, fr);
  }
}

void TurboAssembler::CompareF(SecondaryField sizeField, FPUCondition cc,
                              FPURegister cmp1, FPURegister cmp2) {
  if (IsMipsArchVariant(kMips32r6)) {
    sizeField = sizeField == D ? L : W;
    DCHECK(cmp1 != kDoubleCompareReg && cmp2 != kDoubleCompareReg);
    cmp(cc, sizeField, kDoubleCompareReg, cmp1, cmp2);
  } else {
    c(cc, sizeField, cmp1, cmp2);
  }
}

void TurboAssembler::CompareIsNanF(SecondaryField sizeField, FPURegister cmp1,
                                   FPURegister cmp2) {
  CompareF(sizeField, UN, cmp1, cmp2);
}

void TurboAssembler::BranchTrueShortF(Label* target, BranchDelaySlot bd) {
  if (IsMipsArchVariant(kMips32r6)) {
    bc1nez(target, kDoubleCompareReg);
  } else {
    bc1t(target);
  }
  if (bd == PROTECT) {
    nop();
  }
}

void TurboAssembler::BranchFalseShortF(Label* target, BranchDelaySlot bd) {
  if (IsMipsArchVariant(kMips32r6)) {
    bc1eqz(target, kDoubleCompareReg);
  } else {
    bc1f(target);
  }
  if (bd == PROTECT) {
    nop();
  }
}

void TurboAssembler::BranchTrueF(Label* target, BranchDelaySlot bd) {
  bool long_branch =
      target->is_bound() ? !is_near(target) : is_trampoline_emitted();
  if (long_branch) {
    Label skip;
    BranchFalseShortF(&skip);
    BranchLong(target, bd);
    bind(&skip);
  } else {
    BranchTrueShortF(target, bd);
  }
}

void TurboAssembler::BranchFalseF(Label* target, BranchDelaySlot bd) {
  bool long_branch =
      target->is_bound() ? !is_near(target) : is_trampoline_emitted();
  if (long_branch) {
    Label skip;
    BranchTrueShortF(&skip);
    BranchLong(target, bd);
    bind(&skip);
  } else {
    BranchFalseShortF(target, bd);
  }
}

void TurboAssembler::BranchMSA(Label* target, MSABranchDF df,
                               MSABranchCondition cond, MSARegister wt,
                               BranchDelaySlot bd) {
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);

    if (target) {
      bool long_branch =
          target->is_bound() ? !is_near(target) : is_trampoline_emitted();
      if (long_branch) {
        Label skip;
        MSABranchCondition neg_cond = NegateMSABranchCondition(cond);
        BranchShortMSA(df, &skip, neg_cond, wt, bd);
        BranchLong(target, bd);
        bind(&skip);
      } else {
        BranchShortMSA(df, target, cond, wt, bd);
      }
    }
  }
}

void TurboAssembler::BranchShortMSA(MSABranchDF df, Label* target,
                                    MSABranchCondition cond, MSARegister wt,
                                    BranchDelaySlot bd) {
  if (IsMipsArchVariant(kMips32r6)) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (target) {
      switch (cond) {
        case all_not_zero:
          switch (df) {
            case MSA_BRANCH_D:
              bnz_d(wt, target);
              break;
            case MSA_BRANCH_W:
              bnz_w(wt, target);
              break;
            case MSA_BRANCH_H:
              bnz_h(wt, target);
              break;
            case MSA_BRANCH_B:
            default:
              bnz_b(wt, target);
          }
          break;
        case one_elem_not_zero:
          bnz_v(wt, target);
          break;
        case one_elem_zero:
          switch (df) {
            case MSA_BRANCH_D:
              bz_d(wt, target);
              break;
            case MSA_BRANCH_W:
              bz_w(wt, target);
              break;
            case MSA_BRANCH_H:
              bz_h(wt, target);
              break;
            case MSA_BRANCH_B:
            default:
              bz_b(wt, target);
          }
          break;
        case all_zero:
          bz_v(wt, target);
          break;
        default:
          UNREACHABLE();
      }
    }
  }
  if (bd == PROTECT) {
    nop();
  }
}

void TurboAssembler::FmoveLow(FPURegister dst, Register src_low) {
  if (IsFp32Mode()) {
    mtc1(src_low, dst);
  } else {
    DCHECK(IsFp64Mode() || IsFpxxMode());
    DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(src_low != scratch);
    mfhc1(scratch, dst);
    mtc1(src_low, dst);
    mthc1(scratch, dst);
  }
}

void TurboAssembler::Move(FPURegister dst, uint32_t src) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(static_cast<int32_t>(src)));
  mtc1(scratch, dst);
}

void TurboAssembler::Move(FPURegister dst, uint64_t src) {
  // Handle special values first.
  if (src == bit_cast<uint64_t>(0.0) && has_double_zero_reg_set_) {
    mov_d(dst, kDoubleRegZero);
  } else if (src == bit_cast<uint64_t>(-0.0) && has_double_zero_reg_set_) {
    Neg_d(dst, kDoubleRegZero);
  } else {
    uint32_t lo = src & 0xFFFFFFFF;
    uint32_t hi = src >> 32;
    // Move the low part of the double into the lower of the corresponding FPU
    // register of FPU register pair.
    if (lo != 0) {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(lo));
      mtc1(scratch, dst);
    } else {
      mtc1(zero_reg, dst);
    }
    // Move the high part of the double into the higher of the corresponding FPU
    // register of FPU register pair.
    if (hi != 0) {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(hi));
      Mthc1(scratch, dst);
    } else {
      Mthc1(zero_reg, dst);
    }
    if (dst == kDoubleRegZero) has_double_zero_reg_set_ = true;
  }
}

void TurboAssembler::LoadZeroOnCondition(Register rd, Register rs,
                                         const Operand& rt, Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  switch (cond) {
    case cc_always:
      mov(rd, zero_reg);
      break;
    case eq:
      if (rs == zero_reg) {
        if (rt.is_reg()) {
          LoadZeroIfConditionZero(rd, rt.rm());
        } else {
          if (rt.immediate() == 0) {
            mov(rd, zero_reg);
          } else {
            nop();
          }
        }
      } else if (IsZero(rt)) {
        LoadZeroIfConditionZero(rd, rs);
      } else {
        Subu(t9, rs, rt);
        LoadZeroIfConditionZero(rd, t9);
      }
      break;
    case ne:
      if (rs == zero_reg) {
        if (rt.is_reg()) {
          LoadZeroIfConditionNotZero(rd, rt.rm());
        } else {
          if (rt.immediate() != 0) {
            mov(rd, zero_reg);
          } else {
            nop();
          }
        }
      } else if (IsZero(rt)) {
        LoadZeroIfConditionNotZero(rd, rs);
      } else {
        Subu(t9, rs, rt);
        LoadZeroIfConditionNotZero(rd, t9);
      }
      break;

    // Signed comparison.
    case greater:
      Sgt(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      break;
    case greater_equal:
      Sge(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs >= rt
      break;
    case less:
      Slt(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs < rt
      break;
    case less_equal:
      Sle(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs <= rt
      break;

    // Unsigned comparison.
    case Ugreater:
      Sgtu(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs > rt
      break;

    case Ugreater_equal:
      Sgeu(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs >= rt
      break;
    case Uless:
      Sltu(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs < rt
      break;
    case Uless_equal:
      Sleu(t9, rs, rt);
      LoadZeroIfConditionNotZero(rd, t9);
      // rs <= rt
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::LoadZeroIfConditionNotZero(Register dest,
                                                Register condition) {
  if (IsMipsArchVariant(kMips32r6)) {
    seleqz(dest, dest, condition);
  } else {
    Movn(dest, zero_reg, condition);
  }
}

void TurboAssembler::LoadZeroIfConditionZero(Register dest,
                                             Register condition) {
  if (IsMipsArchVariant(kMips32r6)) {
    selnez(dest, dest, condition);
  } else {
    Movz(dest, zero_reg, condition);
  }
}

void TurboAssembler::LoadZeroIfFPUCondition(Register dest) {
  if (IsMipsArchVariant(kMips32r6)) {
    mfc1(kScratchReg, kDoubleCompareReg);
    LoadZeroIfConditionNotZero(dest, kScratchReg);
  } else {
    Movt(dest, zero_reg);
  }
}

void TurboAssembler::LoadZeroIfNotFPUCondition(Register dest) {
  if (IsMipsArchVariant(kMips32r6)) {
    mfc1(kScratchReg, kDoubleCompareReg);
    LoadZeroIfConditionZero(dest, kScratchReg);
  } else {
    Movf(dest, zero_reg);
  }
}

void TurboAssembler::Movz(Register rd, Register rs, Register rt) {
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    Label done;
    Branch(&done, ne, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movz(rd, rs, rt);
  }
}

void TurboAssembler::Movn(Register rd, Register rs, Register rt) {
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    Label done;
    Branch(&done, eq, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movn(rd, rs, rt);
  }
}

void TurboAssembler::Movt(Register rd, Register rs, uint16_t cc) {
  if (IsMipsArchVariant(kLoongson)) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    // Tests an FP condition code and then conditionally move rs to rd.
    // We do not currently use any FPU cc bit other than bit 0.
    DCHECK_EQ(cc, 0);
    DCHECK(rs != t8 && rd != t8);
    Label done;
    Register scratch = t8;
    // For testing purposes we need to fetch content of the FCSR register and
    // than test its cc (floating point condition code) bit (for cc = 0, it is
    // 24. bit of the FCSR).
    cfc1(scratch, FCSR);
    // For the MIPS I, II and III architectures, the contents of scratch is
    // UNPREDICTABLE for the instruction immediately following CFC1.
    nop();
    srl(scratch, scratch, 16);
    andi(scratch, scratch, 0x0080);
    Branch(&done, eq, scratch, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movt(rd, rs, cc);
  }
}

void TurboAssembler::Movf(Register rd, Register rs, uint16_t cc) {
  if (IsMipsArchVariant(kLoongson)) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    // Tests an FP condition code and then conditionally move rs to rd.
    // We do not currently use any FPU cc bit other than bit 0.
    DCHECK_EQ(cc, 0);
    DCHECK(rs != t8 && rd != t8);
    Label done;
    Register scratch = t8;
    // For testing purposes we need to fetch content of the FCSR register and
    // than test its cc (floating point condition code) bit (for cc = 0, it is
    // 24. bit of the FCSR).
    cfc1(scratch, FCSR);
    // For the MIPS I, II and III architectures, the contents of scratch is
    // UNPREDICTABLE for the instruction immediately following CFC1.
    nop();
    srl(scratch, scratch, 16);
    andi(scratch, scratch, 0x0080);
    Branch(&done, ne, scratch, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movf(rd, rs, cc);
  }
}

void TurboAssembler::Clz(Register rd, Register rs) {
  if (IsMipsArchVariant(kLoongson)) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK(rd != t8 && rd != t9 && rs != t8 && rs != t9);
    Register mask = t8;
    Register scratch = t9;
    Label loop, end;
    {
      UseScratchRegisterScope temps(this);
      Register scratch1 = temps.Acquire();
      mov(scratch1, rs);
      mov(rd, zero_reg);
      lui(mask, 0x8000);
      bind(&loop);
      and_(scratch, scratch1, mask);
    }
    Branch(&end, ne, scratch, Operand(zero_reg));
    addiu(rd, rd, 1);
    Branch(&loop, ne, mask, Operand(zero_reg), USE_DELAY_SLOT);
    srl(mask, mask, 1);
    bind(&end);
  } else {
    clz(rd, rs);
  }
}

void TurboAssembler::Ctz(Register rd, Register rs) {
  if (IsMipsArchVariant(kMips32r6)) {
    // We don't have an instruction to count the number of trailing zeroes.
    // Start by flipping the bits end-for-end so we can count the number of
    // leading zeroes instead.
    Ror(rd, rs, 16);
    wsbh(rd, rd);
    bitswap(rd, rd);
    Clz(rd, rd);
  } else {
    // Convert trailing zeroes to trailing ones, and bits to their left
    // to zeroes.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Addu(scratch, rs, -1);
    Xor(rd, scratch, rs);
    And(rd, rd, scratch);
    // Count number of leading zeroes.
    Clz(rd, rd);
    // Subtract number of leading zeroes from 32 to get number of trailing
    // ones. Remember that the trailing ones were formerly trailing zeroes.
    li(scratch, 32);
    Subu(rd, scratch, rd);
  }
}

void TurboAssembler::Popcnt(Register rd, Register rs) {
  // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  //
  // A generalization of the best bit counting method to integers of
  // bit-widths up to 128 (parameterized by type T) is this:
  //
  // v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
  // v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
  // v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
  // c = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * BITS_PER_BYTE; //count
  //
  // For comparison, for 32-bit quantities, this algorithm can be executed
  // using 20 MIPS instructions (the calls to LoadConst32() generate two
  // machine instructions each for the values being used in this algorithm).
  // A(n unrolled) loop-based algorithm requires 25 instructions.
  //
  // For 64-bit quantities, this algorithm gets executed twice, (once
  // for in_lo, and again for in_hi), but saves a few instructions
  // because the mask values only have to be loaded once. Using this
  // algorithm the count for a 64-bit operand can be performed in 29
  // instructions compared to a loop-based algorithm which requires 47
  // instructions.
  uint32_t B0 = 0x55555555;     // (T)~(T)0/3
  uint32_t B1 = 0x33333333;     // (T)~(T)0/15*3
  uint32_t B2 = 0x0F0F0F0F;     // (T)~(T)0/255*15
  uint32_t value = 0x01010101;  // (T)~(T)0/255
  uint32_t shift = 24;          // (sizeof(T) - 1) * BITS_PER_BYTE
  BlockTrampolinePoolScope block_trampoline_pool(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register scratch2 = t8;
  srl(scratch, rs, 1);
  li(scratch2, B0);
  And(scratch, scratch, scratch2);
  Subu(scratch, rs, scratch);
  li(scratch2, B1);
  And(rd, scratch, scratch2);
  srl(scratch, scratch, 2);
  And(scratch, scratch, scratch2);
  Addu(scratch, rd, scratch);
  srl(rd, scratch, 4);
  Addu(rd, rd, scratch);
  li(scratch2, B2);
  And(rd, rd, scratch2);
  li(scratch, value);
  Mul(rd, rd, scratch);
  srl(rd, rd, shift);
}

void MacroAssembler::EmitFPUTruncate(
    FPURoundingMode rounding_mode, Register result, DoubleRegister double_input,
    Register scratch, DoubleRegister double_scratch, Register except_flag,
    CheckForInexactConversion check_inexact) {
  DCHECK(result != scratch);
  DCHECK(double_input != double_scratch);
  DCHECK(except_flag != scratch);

  Label done;

  // Clear the except flag (0 = no exception)
  mov(except_flag, zero_reg);

  // Test for values that can be exactly represented as a signed 32-bit integer.
  cvt_w_d(double_scratch, double_input);
  mfc1(result, double_scratch);
  cvt_d_w(double_scratch, double_scratch);
  CompareF64(EQ, double_input, double_scratch);
  BranchTrueShortF(&done);

  int32_t except_mask = kFCSRFlagMask;  // Assume interested in all exceptions.

  if (check_inexact == kDontCheckForInexactConversion) {
    // Ignore inexact exceptions.
    except_mask &= ~kFCSRInexactFlagMask;
  }

  // Save FCSR.
  cfc1(scratch, FCSR);
  // Disable FPU exceptions.
  ctc1(zero_reg, FCSR);

  // Do operation based on rounding mode.
  switch (rounding_mode) {
    case kRoundToNearest:
      Round_w_d(double_scratch, double_input);
      break;
    case kRoundToZero:
      Trunc_w_d(double_scratch, double_input);
      break;
    case kRoundToPlusInf:
      Ceil_w_d(double_scratch, double_input);
      break;
    case kRoundToMinusInf:
      Floor_w_d(double_scratch, double_input);
      break;
  }  // End of switch-statement.

  // Retrieve FCSR.
  cfc1(except_flag, FCSR);
  // Restore FCSR.
  ctc1(scratch, FCSR);
  // Move the converted value into the result register.
  mfc1(result, double_scratch);

  // Check for fpu exceptions.
  And(except_flag, except_flag, Operand(except_mask));

  bind(&done);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DoubleRegister single_scratch = kScratchDoubleReg.low();
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register scratch2 = t9;

  // Clear cumulative exception flags and save the FCSR.
  cfc1(scratch2, FCSR);
  ctc1(zero_reg, FCSR);
  // Try a conversion to a signed integer.
  trunc_w_d(single_scratch, double_input);
  mfc1(result, single_scratch);
  // Retrieve and restore the FCSR.
  cfc1(scratch, FCSR);
  ctc1(scratch2, FCSR);
  // Check for overflow and NaNs.
  And(scratch, scratch,
      kFCSROverflowFlagMask | kFCSRUnderflowFlagMask | kFCSRInvalidOpFlagMask);
  // If we had no exceptions we are done.
  Branch(done, eq, scratch, Operand(zero_reg));
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DoubleRegister double_input,
                                       StubCallMode stub_mode) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(ra);
  Subu(sp, sp, Operand(kDoubleSize));  // Put input on stack.
  Sdc1(double_input, MemOperand(sp, 0));

  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }
  lw(result, MemOperand(sp, 0));

  Addu(sp, sp, Operand(kDoubleSize));
  pop(ra);

  bind(&done);
}

// Emulated condtional branches do not emit a nop in the branch delay slot.
//
// BRANCH_ARGS_CHECK checks that conditional jump arguments are correct.
#define BRANCH_ARGS_CHECK(cond, rs, rt)                                  \
  DCHECK((cond == cc_always && rs == zero_reg && rt.rm() == zero_reg) || \
         (cond != cc_always && (rs != zero_reg || rt.rm() != zero_reg)))

void TurboAssembler::Branch(int32_t offset, BranchDelaySlot bdslot) {
  DCHECK(IsMipsArchVariant(kMips32r6) ? is_int26(offset) : is_int16(offset));
  BranchShort(offset, bdslot);
}

void TurboAssembler::Branch(int32_t offset, Condition cond, Register rs,
                            const Operand& rt, BranchDelaySlot bdslot) {
  bool is_near = BranchShortCheck(offset, nullptr, cond, rs, rt, bdslot);
  DCHECK(is_near);
  USE(is_near);
}

void TurboAssembler::Branch(Label* L, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near_branch(L)) {
      BranchShort(L, bdslot);
    } else {
      BranchLong(L, bdslot);
    }
  } else {
    if (is_trampoline_emitted()) {
      BranchLong(L, bdslot);
    } else {
      BranchShort(L, bdslot);
    }
  }
}

void TurboAssembler::Branch(Label* L, Condition cond, Register rs,
                            const Operand& rt, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (!BranchShortCheck(0, L, cond, rs, rt, bdslot)) {
      if (cond != cc_always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L, bdslot);
        bind(&skip);
      } else {
        BranchLong(L, bdslot);
      }
    }
  } else {
    if (is_trampoline_emitted()) {
      if (cond != cc_always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L, bdslot);
        bind(&skip);
      } else {
        BranchLong(L, bdslot);
      }
    } else {
      BranchShort(L, cond, rs, rt, bdslot);
    }
  }
}

void TurboAssembler::Branch(Label* L, Condition cond, Register rs,
                            RootIndex index, BranchDelaySlot bdslot) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  LoadRoot(scratch, index);
  Branch(L, cond, rs, Operand(scratch), bdslot);
}

void TurboAssembler::BranchShortHelper(int16_t offset, Label* L,
                                       BranchDelaySlot bdslot) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset16);
  b(offset);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT) nop();
}

void TurboAssembler::BranchShortHelperR6(int32_t offset, Label* L) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset26);
  bc(offset);
}

void TurboAssembler::BranchShort(int32_t offset, BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
    DCHECK(is_int26(offset));
    BranchShortHelperR6(offset, nullptr);
  } else {
    DCHECK(is_int16(offset));
    BranchShortHelper(offset, nullptr, bdslot);
  }
}

void TurboAssembler::BranchShort(Label* L, BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
    BranchShortHelperR6(0, L);
  } else {
    BranchShortHelper(0, L, bdslot);
  }
}

int32_t TurboAssembler::GetOffset(int32_t offset, Label* L, OffsetSize bits) {
  if (L) {
    offset = branch_offset_helper(L, bits) >> 2;
  } else {
    DCHECK(is_intn(offset, bits));
  }
  return offset;
}

Register TurboAssembler::GetRtAsRegisterHelper(const Operand& rt,
                                               Register scratch) {
  Register r2 = no_reg;
  if (rt.is_reg()) {
    r2 = rt.rm();
  } else {
    r2 = scratch;
    li(r2, rt);
  }

  return r2;
}

bool TurboAssembler::CalculateOffset(Label* L, int32_t* offset,
                                     OffsetSize bits) {
  if (!is_near(L, bits)) return false;
  *offset = GetOffset(*offset, L, bits);
  return true;
}

bool TurboAssembler::CalculateOffset(Label* L, int32_t* offset, OffsetSize bits,
                                     Register* scratch, const Operand& rt) {
  if (!is_near(L, bits)) return false;
  *scratch = GetRtAsRegisterHelper(rt, *scratch);
  *offset = GetOffset(*offset, L, bits);
  return true;
}

bool TurboAssembler::BranchShortHelperR6(int32_t offset, Label* L,
                                         Condition cond, Register rs,
                                         const Operand& rt) {
  DCHECK(L == nullptr || offset == 0);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;

  // Be careful to always use shifted_branch_offset only just before the
  // branch instruction, as the location will be remember for patching the
  // target.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    switch (cond) {
      case cc_always:
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
        bc(offset);
        break;
      case eq:
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          // Pre R6 beq is used here to make the code patchable. Otherwise bc
          // should be used which has no condition field so is not patchable.
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          beq(rs, scratch, offset);
          nop();
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          beqzc(rs, offset);
        } else {
          // We don't want any other register but scratch clobbered.
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          beqc(rs, scratch, offset);
        }
        break;
      case ne:
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          // Pre R6 bne is used here to make the code patchable. Otherwise we
          // should not generate any instruction.
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          bne(rs, scratch, offset);
          nop();
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          bnezc(rs, offset);
        } else {
          // We don't want any other register but scratch clobbered.
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          bnec(rs, scratch, offset);
        }
        break;

      // Signed comparison.
      case greater:
        // rs > rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          break;  // No code needs to be emitted.
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          bltzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
          bgtzc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltc(scratch, rs, offset);
        }
        break;
      case greater_equal:
        // rs >= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          blezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
          bgezc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bgec(rs, scratch, offset);
        }
        break;
      case less:
        // rs < rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          break;  // No code needs to be emitted.
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          bgtzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
          bltzc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltc(rs, scratch, offset);
        }
        break;
      case less_equal:
        // rs <= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          bgezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
          blezc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bgec(scratch, rs, offset);
        }
        break;

      // Unsigned comparison.
      case Ugreater:
        // rs > rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          break;  // No code needs to be emitted.
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21, &scratch, rt))
            return false;
          bnezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          bnezc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltuc(scratch, rs, offset);
        }
        break;
      case Ugreater_equal:
        // rs >= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21, &scratch, rt))
            return false;
          beqzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bgeuc(rs, scratch, offset);
        }
        break;
      case Uless:
        // rs < rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          break;  // No code needs to be emitted.
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21, &scratch, rt))
            return false;
          bnezc(scratch, offset);
        } else if (IsZero(rt)) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltuc(rs, scratch, offset);
        }
        break;
      case Uless_equal:
        // rs <= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset26, &scratch, rt))
            return false;
          bc(offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          beqzc(rs, offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bgeuc(scratch, rs, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  CheckTrampolinePoolQuick(1);
  return true;
}

bool TurboAssembler::BranchShortHelper(int16_t offset, Label* L, Condition cond,
                                       Register rs, const Operand& rt,
                                       BranchDelaySlot bdslot) {
  DCHECK(L == nullptr || offset == 0);
  if (!is_near(L, OffsetSize::kOffset16)) return false;

  UseScratchRegisterScope temps(this);
  Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
  int32_t offset32;

  // Be careful to always use shifted_branch_offset only just before the
  // branch instruction, as the location will be remember for patching the
  // target.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    switch (cond) {
      case cc_always:
        offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
        b(offset32);
        break;
      case eq:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(rs, zero_reg, offset32);
        } else {
          // We don't want any other register but scratch clobbered.
          scratch = GetRtAsRegisterHelper(rt, scratch);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(rs, scratch, offset32);
        }
        break;
      case ne:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(rs, zero_reg, offset32);
        } else {
          // We don't want any other register but scratch clobbered.
          scratch = GetRtAsRegisterHelper(rt, scratch);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(rs, scratch, offset32);
        }
        break;

      // Signed comparison.
      case greater:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bgtz(rs, offset32);
        } else {
          Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(scratch, zero_reg, offset32);
        }
        break;
      case greater_equal:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bgez(rs, offset32);
        } else {
          Slt(scratch, rs, rt);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(scratch, zero_reg, offset32);
        }
        break;
      case less:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bltz(rs, offset32);
        } else {
          Slt(scratch, rs, rt);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(scratch, zero_reg, offset32);
        }
        break;
      case less_equal:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          blez(rs, offset32);
        } else {
          Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(scratch, zero_reg, offset32);
        }
        break;

      // Unsigned comparison.
      case Ugreater:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(rs, zero_reg, offset32);
        } else {
          Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(scratch, zero_reg, offset32);
        }
        break;
      case Ugreater_equal:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          b(offset32);
        } else {
          Sltu(scratch, rs, rt);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(scratch, zero_reg, offset32);
        }
        break;
      case Uless:
        if (IsZero(rt)) {
          return true;  // No code needs to be emitted.
        } else {
          Sltu(scratch, rs, rt);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          bne(scratch, zero_reg, offset32);
        }
        break;
      case Uless_equal:
        if (IsZero(rt)) {
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(rs, zero_reg, offset32);
        } else {
          Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
          offset32 = GetOffset(offset, L, OffsetSize::kOffset16);
          beq(scratch, zero_reg, offset32);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT) nop();

  return true;
}

bool TurboAssembler::BranchShortCheck(int32_t offset, Label* L, Condition cond,
                                      Register rs, const Operand& rt,
                                      BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  if (!L) {
    if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
      DCHECK(is_int26(offset));
      return BranchShortHelperR6(offset, nullptr, cond, rs, rt);
    } else {
      DCHECK(is_int16(offset));
      return BranchShortHelper(offset, nullptr, cond, rs, rt, bdslot);
    }
  } else {
    DCHECK_EQ(offset, 0);
    if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
      return BranchShortHelperR6(0, L, cond, rs, rt);
    } else {
      return BranchShortHelper(0, L, cond, rs, rt, bdslot);
    }
  }
  return false;
}

void TurboAssembler::BranchShort(int32_t offset, Condition cond, Register rs,
                                 const Operand& rt, BranchDelaySlot bdslot) {
  BranchShortCheck(offset, nullptr, cond, rs, rt, bdslot);
}

void TurboAssembler::BranchShort(Label* L, Condition cond, Register rs,
                                 const Operand& rt, BranchDelaySlot bdslot) {
  BranchShortCheck(0, L, cond, rs, rt, bdslot);
}

void TurboAssembler::BranchAndLink(int32_t offset, BranchDelaySlot bdslot) {
  BranchAndLinkShort(offset, bdslot);
}

void TurboAssembler::BranchAndLink(int32_t offset, Condition cond, Register rs,
                                   const Operand& rt, BranchDelaySlot bdslot) {
  bool is_near = BranchAndLinkShortCheck(offset, nullptr, cond, rs, rt, bdslot);
  DCHECK(is_near);
  USE(is_near);
}

void TurboAssembler::BranchAndLink(Label* L, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near_branch(L)) {
      BranchAndLinkShort(L, bdslot);
    } else {
      BranchAndLinkLong(L, bdslot);
    }
  } else {
    if (is_trampoline_emitted()) {
      BranchAndLinkLong(L, bdslot);
    } else {
      BranchAndLinkShort(L, bdslot);
    }
  }
}

void TurboAssembler::BranchAndLink(Label* L, Condition cond, Register rs,
                                   const Operand& rt, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (!BranchAndLinkShortCheck(0, L, cond, rs, rt, bdslot)) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      BranchAndLinkLong(L, bdslot);
      bind(&skip);
    }
  } else {
    if (is_trampoline_emitted()) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      BranchAndLinkLong(L, bdslot);
      bind(&skip);
    } else {
      BranchAndLinkShortCheck(0, L, cond, rs, rt, bdslot);
    }
  }
}

void TurboAssembler::BranchAndLinkShortHelper(int16_t offset, Label* L,
                                              BranchDelaySlot bdslot) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset16);
  bal(offset);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT) nop();
}

void TurboAssembler::BranchAndLinkShortHelperR6(int32_t offset, Label* L) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset26);
  balc(offset);
}

void TurboAssembler::BranchAndLinkShort(int32_t offset,
                                        BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
    DCHECK(is_int26(offset));
    BranchAndLinkShortHelperR6(offset, nullptr);
  } else {
    DCHECK(is_int16(offset));
    BranchAndLinkShortHelper(offset, nullptr, bdslot);
  }
}

void TurboAssembler::BranchAndLinkShort(Label* L, BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
    BranchAndLinkShortHelperR6(0, L);
  } else {
    BranchAndLinkShortHelper(0, L, bdslot);
  }
}

bool TurboAssembler::BranchAndLinkShortHelperR6(int32_t offset, Label* L,
                                                Condition cond, Register rs,
                                                const Operand& rt) {
  DCHECK(L == nullptr || offset == 0);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
  OffsetSize bits = OffsetSize::kOffset16;

  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK((cond == cc_always && is_int26(offset)) || is_int16(offset));
  switch (cond) {
    case cc_always:
      if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
      balc(offset);
      break;
    case eq:
      if (!is_near(L, bits)) return false;
      Subu(scratch, rs, rt);
      offset = GetOffset(offset, L, bits);
      beqzalc(scratch, offset);
      break;
    case ne:
      if (!is_near(L, bits)) return false;
      Subu(scratch, rs, rt);
      offset = GetOffset(offset, L, bits);
      bnezalc(scratch, offset);
      break;

    // Signed comparison.
    case greater:
      // rs > rt
      if (rs.code() == rt.rm().code()) {
        break;  // No code needs to be emitted.
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
          return false;
        bltzalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
        bgtzalc(rs, offset);
      } else {
        if (!is_near(L, bits)) return false;
        Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
        offset = GetOffset(offset, L, bits);
        bnezalc(scratch, offset);
      }
      break;
    case greater_equal:
      // rs >= rt
      if (rs.code() == rt.rm().code()) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
        balc(offset);
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
          return false;
        blezalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
        bgezalc(rs, offset);
      } else {
        if (!is_near(L, bits)) return false;
        Slt(scratch, rs, rt);
        offset = GetOffset(offset, L, bits);
        beqzalc(scratch, offset);
      }
      break;
    case less:
      // rs < rt
      if (rs.code() == rt.rm().code()) {
        break;  // No code needs to be emitted.
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
          return false;
        bgtzalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
        bltzalc(rs, offset);
      } else {
        if (!is_near(L, bits)) return false;
        Slt(scratch, rs, rt);
        offset = GetOffset(offset, L, bits);
        bnezalc(scratch, offset);
      }
      break;
    case less_equal:
      // rs <= r2
      if (rs.code() == rt.rm().code()) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset26)) return false;
        balc(offset);
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16, &scratch, rt))
          return false;
        bgezalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset16)) return false;
        blezalc(rs, offset);
      } else {
        if (!is_near(L, bits)) return false;
        Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
        offset = GetOffset(offset, L, bits);
        beqzalc(scratch, offset);
      }
      break;

    // Unsigned comparison.
    case Ugreater:
      // rs > r2
      if (!is_near(L, bits)) return false;
      Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      offset = GetOffset(offset, L, bits);
      bnezalc(scratch, offset);
      break;
    case Ugreater_equal:
      // rs >= r2
      if (!is_near(L, bits)) return false;
      Sltu(scratch, rs, rt);
      offset = GetOffset(offset, L, bits);
      beqzalc(scratch, offset);
      break;
    case Uless:
      // rs < r2
      if (!is_near(L, bits)) return false;
      Sltu(scratch, rs, rt);
      offset = GetOffset(offset, L, bits);
      bnezalc(scratch, offset);
      break;
    case Uless_equal:
      // rs <= r2
      if (!is_near(L, bits)) return false;
      Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      offset = GetOffset(offset, L, bits);
      beqzalc(scratch, offset);
      break;
    default:
      UNREACHABLE();
  }
  return true;
}

// Pre r6 we need to use a bgezal or bltzal, but they can't be used directly
// with the slt instructions. We could use sub or add instead but we would miss
// overflow cases, so we keep slt and add an intermediate third instruction.
bool TurboAssembler::BranchAndLinkShortHelper(int16_t offset, Label* L,
                                              Condition cond, Register rs,
                                              const Operand& rt,
                                              BranchDelaySlot bdslot) {
  DCHECK(L == nullptr || offset == 0);
  if (!is_near(L, OffsetSize::kOffset16)) return false;

  Register scratch = t8;
  BlockTrampolinePoolScope block_trampoline_pool(this);

  switch (cond) {
    case cc_always:
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bal(offset);
      break;
    case eq:
      bne(rs, GetRtAsRegisterHelper(rt, scratch), 2);
      nop();
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bal(offset);
      break;
    case ne:
      beq(rs, GetRtAsRegisterHelper(rt, scratch), 2);
      nop();
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bal(offset);
      break;

    // Signed comparison.
    case greater:
      Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      Slt(scratch, rs, rt);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bltzal(scratch, offset);
      break;
    case less:
      Slt(scratch, rs, rt);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bgezal(scratch, offset);
      break;
    case less_equal:
      Slt(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      Sltu(scratch, rs, rt);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bltzal(scratch, offset);
      break;
    case Uless:
      Sltu(scratch, rs, rt);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      Sltu(scratch, GetRtAsRegisterHelper(rt, scratch), rs);
      addiu(scratch, scratch, -1);
      offset = GetOffset(offset, L, OffsetSize::kOffset16);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT) nop();

  return true;
}

bool TurboAssembler::BranchAndLinkShortCheck(int32_t offset, Label* L,
                                             Condition cond, Register rs,
                                             const Operand& rt,
                                             BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  if (!L) {
    if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
      DCHECK(is_int26(offset));
      return BranchAndLinkShortHelperR6(offset, nullptr, cond, rs, rt);
    } else {
      DCHECK(is_int16(offset));
      return BranchAndLinkShortHelper(offset, nullptr, cond, rs, rt, bdslot);
    }
  } else {
    DCHECK_EQ(offset, 0);
    if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
      return BranchAndLinkShortHelperR6(0, L, cond, rs, rt);
    } else {
      return BranchAndLinkShortHelper(0, L, cond, rs, rt, bdslot);
    }
  }
  return false;
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  lw(destination,
     FieldMemOperand(destination,
                     FixedArray::kHeaderSize + constant_index * kPointerSize));
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  lw(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    Addu(destination, kRootRegister, offset);
  }
}

void TurboAssembler::Jump(Register target, int16_t offset, Condition cond,
                          Register rs, const Operand& rt, BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(is_int16(offset));
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT) {
    if (cond == cc_always) {
      jic(target, offset);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jic(target, offset);
    }
  } else {
    if (offset != 0) {
      Addu(target, target, offset);
    }
    if (cond == cc_always) {
      jr(target);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jr(target);
    }
    // Emit a nop in the branch delay slot if required.
    if (bd == PROTECT) nop();
  }
}

void TurboAssembler::Jump(Register target, Register base, int16_t offset,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  DCHECK(is_int16(offset));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT) {
    if (cond == cc_always) {
      jic(base, offset);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jic(base, offset);
    }
  } else {
    if (offset != 0) {
      Addu(target, base, offset);
    } else {  // Call through target
      if (target != base) mov(target, base);
    }
    if (cond == cc_always) {
      jr(target);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jr(target);
    }
    // Emit a nop in the branch delay slot if required.
    if (bd == PROTECT) nop();
  }
}

void TurboAssembler::Jump(Register target, const Operand& offset,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT &&
      !is_int16(offset.immediate())) {
    uint32_t aui_offset, jic_offset;
    Assembler::UnpackTargetAddressUnsigned(offset.immediate(), &aui_offset,
                                           &jic_offset);
    RecordRelocInfo(RelocInfo::EXTERNAL_REFERENCE, offset.immediate());
    aui(target, target, aui_offset);
    if (cond == cc_always) {
      jic(target, jic_offset);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jic(target, jic_offset);
    }
  } else {
    if (offset.immediate() != 0) {
      Addu(target, target, offset);
    }
    if (cond == cc_always) {
      jr(target);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jr(target);
    }
    // Emit a nop in the branch delay slot if required.
    if (bd == PROTECT) nop();
  }
}

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label skip;
  if (cond != cc_always) {
    Branch(USE_DELAY_SLOT, &skip, NegateCondition(cond), rs, rt);
  }
  // The first instruction of 'li' may be placed in the delay slot.
  // This is not an issue, t9 is expected to be clobbered anyway.
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT) {
    uint32_t lui_offset, jic_offset;
    UnpackTargetAddressUnsigned(target, &lui_offset, &jic_offset);
    if (MustUseReg(rmode)) {
      RecordRelocInfo(rmode, target);
    }
    lui(t9, lui_offset);
    Jump(t9, jic_offset, al, zero_reg, Operand(zero_reg), bd);
  } else {
    li(t9, Operand(target, rmode));
    Jump(t9, 0, al, zero_reg, Operand(zero_reg), bd);
  }
  bind(&skip);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          Register rs, const Operand& rt, BranchDelaySlot bd) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(static_cast<intptr_t>(target), rmode, cond, rs, rt, bd);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  BlockTrampolinePoolScope block_trampoline_pool(this);

  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);
  if (target_is_isolate_independent_builtin &&
      options().use_pc_relative_calls_and_jumps) {
    int32_t code_target_index = AddCodeTarget(code);
    Label skip;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (cond != cc_always) {
      // By using delay slot, we always execute first instruction of
      // GenPcRelativeJump (which is or_(t8, ra, zero_reg)).
      Branch(USE_DELAY_SLOT, &skip, NegateCondition(cond), rs, rt);
    }
    GenPCRelativeJump(t8, t9, code_target_index,
                      RelocInfo::RELATIVE_CODE_TARGET, bd);
    bind(&skip);
    return;
  } else if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(t9, code);
    Jump(t9, Code::kHeaderSize - kHeapObjectTag, cond, rs, rt, bd);
    return;
  } else if (target_is_isolate_independent_builtin &&
             options().inline_offheap_trampolines) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin_index);
    CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
    EmbeddedData d = EmbeddedData::FromBlob();
    Address entry = d.InstructionStartOfBuiltin(builtin_index);
    li(t9, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
    Jump(t9, 0, cond, rs, rt, bd);
    return;
  }

  Jump(static_cast<intptr_t>(code.address()), rmode, cond, rs, rt, bd);
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  li(t9, reference);
  Jump(t9);
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  if (lower_limit != 0) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Subu(scratch, value, Operand(lower_limit));
    Branch(on_in_range, ls, scratch, Operand(higher_limit - lower_limit));
  } else {
    Branch(on_in_range, ls, value, Operand(higher_limit - lower_limit));
  }
}

// Note: To call gcc-compiled C code on mips, you must call through t9.
void TurboAssembler::Call(Register target, int16_t offset, Condition cond,
                          Register rs, const Operand& rt, BranchDelaySlot bd) {
  DCHECK(is_int16(offset));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT) {
    if (cond == cc_always) {
      jialc(target, offset);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jialc(target, offset);
    }
  } else {
    if (offset != 0) {
      Addu(target, target, offset);
    }
    if (cond == cc_always) {
      jalr(target);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jalr(target);
    }
    // Emit a nop in the branch delay slot if required.
    if (bd == PROTECT) nop();
  }
}

// Note: To call gcc-compiled C code on mips, you must call through t9.
void TurboAssembler::Call(Register target, Register base, int16_t offset,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  DCHECK(is_uint16(offset));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT) {
    if (cond == cc_always) {
      jialc(base, offset);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jialc(base, offset);
    }
  } else {
    if (offset != 0) {
      Addu(target, base, offset);
    } else {  // Call through target
      if (target != base) mov(target, base);
    }
    if (cond == cc_always) {
      jalr(target);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jalr(target);
    }
    // Emit a nop in the branch delay slot if required.
    if (bd == PROTECT) nop();
  }
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode, Condition cond,
                          Register rs, const Operand& rt, BranchDelaySlot bd) {
  CheckBuffer();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int32_t target_int = static_cast<int32_t>(target);
  if (IsMipsArchVariant(kMips32r6) && bd == PROTECT && cond == cc_always) {
    uint32_t lui_offset, jialc_offset;
    UnpackTargetAddressUnsigned(target_int, &lui_offset, &jialc_offset);
    if (MustUseReg(rmode)) {
      RecordRelocInfo(rmode, target_int);
    }
    lui(t9, lui_offset);
    Call(t9, jialc_offset, cond, rs, rt, bd);
  } else {
    li(t9, Operand(target_int, rmode), CONSTANT_SIZE);
    Call(t9, 0, cond, rs, rt, bd);
  }
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);

  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);
  if (target_is_isolate_independent_builtin &&
      options().use_pc_relative_calls_and_jumps) {
    int32_t code_target_index = AddCodeTarget(code);
    Label skip;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (cond != cc_always) {
      Branch(PROTECT, &skip, NegateCondition(cond), rs, rt);
    }
    GenPCRelativeJumpAndLink(t8, code_target_index,
                             RelocInfo::RELATIVE_CODE_TARGET, bd);
    bind(&skip);
    return;
  } else if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(t9, code);
    Call(t9, Code::kHeaderSize - kHeapObjectTag, cond, rs, rt, bd);
    return;
  } else if (target_is_isolate_independent_builtin &&
             options().inline_offheap_trampolines) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin_index);
    CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
    EmbeddedData d = EmbeddedData::FromBlob();
    Address entry = d.InstructionStartOfBuiltin(builtin_index);
    li(t9, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
    Call(t9, 0, cond, rs, rt, bd);
    return;
  }

  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK(code->IsExecutable());
  Call(code.address(), rmode, cond, rs, rt, bd);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  STATIC_ASSERT(kSystemPointerSize == 4);
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  SmiUntag(builtin_index, builtin_index);
  Lsa(builtin_index, kRootRegister, builtin_index, kSystemPointerSizeLog2);
  lw(builtin_index,
     MemOperand(builtin_index, IsolateData::builtin_entry_table_offset()));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  LoadEntryFromBuiltinIndex(builtin_index);
  Call(builtin_index);
}

void TurboAssembler::PatchAndJump(Address target) {
  if (kArchVariant != kMips32r6) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    mov(scratch, ra);
    bal(1);                                  // jump to lw
    nop();                                   // in the delay slot
    lw(t9, MemOperand(ra, kInstrSize * 3));  // ra == pc_
    jr(t9);
    mov(ra, scratch);  // in delay slot
    DCHECK_EQ(reinterpret_cast<uint32_t>(pc_) % 8, 0);
    *reinterpret_cast<uint32_t*>(pc_) = target;
    pc_ += sizeof(uint32_t);
  } else {
    // TODO(mips r6): Implement.
    UNIMPLEMENTED();
  }
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  Assembler::BlockTrampolinePoolScope block_trampoline_pool(this);
  static constexpr int kNumInstructionsToJump = 4;
  Label find_ra;
  // Adjust the value in ra to point to the correct return location, 2nd
  // instruction past the real call into C code (the jalr(t9)), and push it.
  // This is the return address of the exit frame.
  if (kArchVariant >= kMips32r6) {
    addiupc(ra, kNumInstructionsToJump + 1);
  } else {
    // This no-op-and-link sequence saves PC + 8 in ra register on pre-r6 MIPS
    nal();  // nal has branch delay slot.
    Addu(ra, ra, kNumInstructionsToJump * kInstrSize);
  }
  bind(&find_ra);

  // This spot was reserved in EnterExitFrame.
  sw(ra, MemOperand(sp));
  // Stack space reservation moved to the branch delay slot below.
  // Stack is still aligned.

  // Call the C routine.
  mov(t9, target);  // Function pointer to t9 to conform to ABI for PIC.
  jalr(t9);
  // Set up sp in the delay slot.
  addiu(sp, sp, -kCArgsSlotsSize);
  // Make sure the stored 'ra' points to this position.
  DCHECK_EQ(kNumInstructionsToJump, InstructionsGeneratedSince(&find_ra));
}

void TurboAssembler::Ret(Condition cond, Register rs, const Operand& rt,
                         BranchDelaySlot bd) {
  Jump(ra, 0, cond, rs, rt, bd);
}

void TurboAssembler::BranchLong(Label* L, BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT &&
      (!L->is_bound() || is_near_r6(L))) {
    BranchShortHelperR6(0, L);
  } else {
    // Generate position independent long branch.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    int32_t imm32;
    imm32 = branch_long_offset(L);
    GenPCRelativeJump(t8, t9, imm32, RelocInfo::NONE, bdslot);
  }
}

void TurboAssembler::BranchAndLinkLong(Label* L, BranchDelaySlot bdslot) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT &&
      (!L->is_bound() || is_near_r6(L))) {
    BranchAndLinkShortHelperR6(0, L);
  } else {
    // Generate position independent long branch and link.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    int32_t imm32;
    imm32 = branch_long_offset(L);
    GenPCRelativeJumpAndLink(t8, imm32, RelocInfo::NONE, bdslot);
  }
}

void TurboAssembler::DropAndRet(int drop) {
  DCHECK(is_int16(drop * kPointerSize));
  Ret(USE_DELAY_SLOT);
  addiu(sp, sp, drop * kPointerSize);
}

void TurboAssembler::DropAndRet(int drop, Condition cond, Register r1,
                                const Operand& r2) {
  // Both Drop and Ret need to be conditional.
  Label skip;
  if (cond != cc_always) {
    Branch(&skip, NegateCondition(cond), r1, r2);
  }

  Drop(drop);
  Ret();

  if (cond != cc_always) {
    bind(&skip);
  }
}

void TurboAssembler::Drop(int count, Condition cond, Register reg,
                          const Operand& op) {
  if (count <= 0) {
    return;
  }

  Label skip;

  if (cond != al) {
    Branch(&skip, NegateCondition(cond), reg, op);
  }

  Addu(sp, sp, Operand(count * kPointerSize));

  if (cond != al) {
    bind(&skip);
  }
}

void MacroAssembler::Swap(Register reg1, Register reg2, Register scratch) {
  if (scratch == no_reg) {
    Xor(reg1, reg1, Operand(reg2));
    Xor(reg2, reg2, Operand(reg1));
    Xor(reg1, reg1, Operand(reg2));
  } else {
    mov(scratch, reg1);
    mov(reg1, reg2);
    mov(reg2, scratch);
  }
}

void TurboAssembler::Call(Label* target) { BranchAndLink(target); }

void TurboAssembler::LoadAddress(Register dst, Label* target) {
  uint32_t address = jump_address(target);
  li(dst, address);
}

void TurboAssembler::Push(Handle<HeapObject> handle) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(handle));
  push(scratch);
}

void TurboAssembler::Push(Smi smi) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(smi));
  push(scratch);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  li(a1, ExternalReference::debug_restart_fp_address(isolate()));
  lw(a1, MemOperand(a1));
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne, a1, Operand(zero_reg));
}

// ---------------------------------------------------------------------------
// Exception handling.

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  Push(Smi::zero());  // Padding.

  // Link the current handler as the next handler.
  li(t2,
     ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  lw(t1, MemOperand(t2));
  push(t1);

  // Set this new handler as the current one.
  sw(sp, MemOperand(t2));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(a1);
  Addu(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch,
     ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  sw(a1, MemOperand(scratch));
}

void TurboAssembler::FPUCanonicalizeNaN(const DoubleRegister dst,
                                        const DoubleRegister src) {
  sub_d(dst, src, kDoubleRegZero);
}

void TurboAssembler::MovFromFloatResult(DoubleRegister dst) {
  if (IsMipsSoftFloatABI) {
    if (kArchEndian == kLittle) {
      Move(dst, v0, v1);
    } else {
      Move(dst, v1, v0);
    }
  } else {
    Move(dst, f0);  // Reg f0 is o32 ABI FP return value.
  }
}

void TurboAssembler::MovFromFloatParameter(DoubleRegister dst) {
  if (IsMipsSoftFloatABI) {
    if (kArchEndian == kLittle) {
      Move(dst, a0, a1);
    } else {
      Move(dst, a1, a0);
    }
  } else {
    Move(dst, f12);  // Reg f12 is o32 ABI FP first argument value.
  }
}

void TurboAssembler::MovToFloatParameter(DoubleRegister src) {
  if (!IsMipsSoftFloatABI) {
    Move(f12, src);
  } else {
    if (kArchEndian == kLittle) {
      Move(a0, a1, src);
    } else {
      Move(a1, a0, src);
    }
  }
}

void TurboAssembler::MovToFloatResult(DoubleRegister src) {
  if (!IsMipsSoftFloatABI) {
    Move(f0, src);
  } else {
    if (kArchEndian == kLittle) {
      Move(v0, v1, src);
    } else {
      Move(v1, v0, src);
    }
  }
}

void TurboAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (!IsMipsSoftFloatABI) {
    if (src2 == f12) {
      DCHECK(src1 != f14);
      Move(f14, src2);
      Move(f12, src1);
    } else {
      Move(f12, src1);
      Move(f14, src2);
    }
  } else {
    if (kArchEndian == kLittle) {
      Move(a0, a1, src1);
      Move(a2, a3, src2);
    } else {
      Move(a1, a0, src1);
      Move(a3, a2, src2);
    }
  }
}

// -----------------------------------------------------------------------------
// JavaScript invokes.

void TurboAssembler::PrepareForTailCall(Register callee_args_count,
                                        Register caller_args_count,
                                        Register scratch0, Register scratch1) {
  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  Lsa(dst_reg, fp, caller_args_count, kPointerSizeLog2);
  Addu(dst_reg, dst_reg,
       Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  Lsa(src_reg, sp, callee_args_count, kPointerSizeLog2);
  Addu(src_reg, src_reg, Operand(kPointerSize));

  if (FLAG_debug_code) {
    Check(lo, AbortReason::kStackAccessBelowStackPointer, src_reg,
          Operand(dst_reg));
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  lw(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  lw(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop, entry;
  Branch(&entry);
  bind(&loop);
  Subu(src_reg, src_reg, Operand(kPointerSize));
  Subu(dst_reg, dst_reg, Operand(kPointerSize));
  lw(tmp_reg, MemOperand(src_reg));
  sw(tmp_reg, MemOperand(dst_reg));
  bind(&entry);
  Branch(&loop, ne, sp, Operand(src_reg));

  // Leave current frame.
  mov(sp, dst_reg);
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeFlag flag) {
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. The registers
  // are set up according to contract with ArgumentsAdaptorTrampoline:
  //  a0: actual arguments count
  //  a1: function (passed through to callee)
  //  a2: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract.
  DCHECK_EQ(actual_parameter_count, a0);
  DCHECK_EQ(expected_parameter_count, a2);

  Branch(&regular_invoke, eq, expected_parameter_count,
         Operand(actual_parameter_count));

  Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
  if (flag == CALL_FUNCTION) {
    Call(adaptor);
    Branch(done);
  } else {
    Jump(adaptor, RelocInfo::CODE_TARGET);
  }

  bind(&regular_invoke);
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count) {
  Label skip_hook;
  li(t0, ExternalReference::debug_hook_on_function_call_address(isolate()));
  lb(t0, MemOperand(t0));
  Branch(&skip_hook, eq, t0, Operand(zero_reg));

  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    Lsa(at, sp, actual_parameter_count, kPointerSizeLog2);
    lw(t0, MemOperand(at));
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    SmiTag(expected_parameter_count);
    Push(expected_parameter_count);

    SmiTag(actual_parameter_count);
    Push(actual_parameter_count);

    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    Push(t0);
    CallRuntime(Runtime::kDebugOnFunctionCall);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }

    Pop(actual_parameter_count);
    SmiUntag(actual_parameter_count);

    Pop(expected_parameter_count);
    SmiUntag(expected_parameter_count);
  }
  bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());
  DCHECK_EQ(function, a1);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == a3);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected_parameter_count,
                 actual_parameter_count);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(a3, RootIndex::kUndefinedValue);
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, flag);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Register code = kJavaScriptCallCodeStartRegister;
  lw(code, FieldMemOperand(function, JSFunction::kCodeOffset));
  if (flag == CALL_FUNCTION) {
    Addu(code, code, Code::kHeaderSize - kHeapObjectTag);
    Call(code);
  } else {
    DCHECK(flag == JUMP_FUNCTION);
    Addu(code, code, Code::kHeaderSize - kHeapObjectTag);
    Jump(code);
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}

void MacroAssembler::InvokeFunctionWithNewTarget(
    Register function, Register new_target, Register actual_parameter_count,
    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK_EQ(function, a1);
  Register expected_reg = a2;
  Register temp_reg = t0;

  lw(temp_reg, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  lhu(expected_reg,
      FieldMemOperand(temp_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(function, new_target, expected_reg, actual_parameter_count,
                     flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK_EQ(function, a1);

  // Get the function and setup the context.
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  InvokeFunctionCode(a1, no_reg, expected_parameter_count,
                     actual_parameter_count, flag);
}

// ---------------------------------------------------------------------------
// Support functions.

void MacroAssembler::GetObjectType(Register object, Register map,
                                   Register type_reg) {
  LoadMap(map, object);
  lhu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
}

// -----------------------------------------------------------------------------
// Runtime calls.

void TurboAssembler::AddOverflow(Register dst, Register left,
                                 const Operand& right, Register overflow) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = t8;
  if (!right.is_reg()) {
    li(at, Operand(right));
    right_reg = at;
  } else {
    right_reg = right.rm();
  }

  DCHECK(left != scratch && right_reg != scratch && dst != scratch &&
         overflow != scratch);
  DCHECK(overflow != left && overflow != right_reg);

  if (dst == left || dst == right_reg) {
    addu(scratch, left, right_reg);
    xor_(overflow, scratch, left);
    xor_(at, scratch, right_reg);
    and_(overflow, overflow, at);
    mov(dst, scratch);
  } else {
    addu(dst, left, right_reg);
    xor_(overflow, dst, left);
    xor_(at, dst, right_reg);
    and_(overflow, overflow, at);
  }
}

void TurboAssembler::SubOverflow(Register dst, Register left,
                                 const Operand& right, Register overflow) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = t8;
  if (!right.is_reg()) {
    li(at, Operand(right));
    right_reg = at;
  } else {
    right_reg = right.rm();
  }

  DCHECK(left != scratch && right_reg != scratch && dst != scratch &&
         overflow != scratch);
  DCHECK(overflow != left && overflow != right_reg);

  if (dst == left || dst == right_reg) {
    subu(scratch, left, right_reg);
    xor_(overflow, left, scratch);
    xor_(at, left, right_reg);
    and_(overflow, overflow, at);
    mov(dst, scratch);
  } else {
    subu(dst, left, right_reg);
    xor_(overflow, left, dst);
    xor_(at, left, right_reg);
    and_(overflow, overflow, at);
  }
}

void TurboAssembler::MulOverflow(Register dst, Register left,
                                 const Operand& right, Register overflow) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = t8;
  Register scratch2 = t9;
  if (!right.is_reg()) {
    li(at, Operand(right));
    right_reg = at;
  } else {
    right_reg = right.rm();
  }

  DCHECK(left != scratch && right_reg != scratch && dst != scratch &&
         overflow != scratch);
  DCHECK(overflow != left && overflow != right_reg);

  if (dst == left || dst == right_reg) {
    Mul(overflow, scratch2, left, right_reg);
    sra(scratch, scratch2, 31);
    xor_(overflow, overflow, scratch);
    mov(dst, scratch2);
  } else {
    Mul(overflow, dst, left, right_reg);
    sra(scratch, dst, 31);
    xor_(overflow, overflow, scratch);
  }
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack. v0 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  PrepareCEntryArgs(num_arguments);
  PrepareCEntryFunction(ExternalReference::Create(f));
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    PrepareCEntryArgs(function->nargs);
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             BranchDelaySlot bd,
                                             bool builtin_exit_frame) {
  PrepareCEntryFunction(builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg), bd);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  li(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Jump(kOffHeapTrampolineRegister);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  Branch(target_if_cleared, eq, in, Operand(kClearedWeakHeapObjectLower32));

  And(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, ExternalReference::Create(counter));
    lw(scratch1, MemOperand(scratch2));
    Addu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, ExternalReference::Create(counter));
    lw(scratch1, MemOperand(scratch2));
    Subu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}

// -----------------------------------------------------------------------------
// Debugging.

void TurboAssembler::Trap() { stop(); }

void TurboAssembler::Assert(Condition cc, AbortReason reason, Register rs,
                            Operand rt) {
  if (emit_debug_code()) Check(cc, reason, rs, rt);
}

void TurboAssembler::Check(Condition cc, AbortReason reason, Register rs,
                           Operand rt) {
  Label L;
  Branch(&L, cc, rs, rt);
  Abort(reason);
  // Will not return here.
  bind(&L);
}

void TurboAssembler::Abort(AbortReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetAbortReason(reason);
  RecordComment("Abort message: ");
  RecordComment(msg);
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    stop();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NONE);
    PrepareCallCFunction(0, a0);
    li(a0, Operand(static_cast<int>(reason)));
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

  Move(a0, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // Will not return here.
  if (is_trampoline_pool_blocked()) {
    // If the calling code cares about the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the Abort macro constant.
    // Currently in debug mode with debug_code enabled the number of
    // generated instructions is 10, so we use this as a maximum value.
    static const int kExpectedAbortInstructions = 10;
    int abort_instructions = InstructionsGeneratedSince(&abort_start);
    DCHECK_LE(abort_instructions, kExpectedAbortInstructions);
    while (abort_instructions++ < kExpectedAbortInstructions) {
      nop();
    }
  }
}

void MacroAssembler::LoadMap(Register destination, Register object) {
  Lw(destination, FieldMemOperand(object, HeapObject::kMapOffset));
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  LoadMap(dst, cp);
  Lw(dst,
     FieldMemOperand(dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  Lw(dst, MemOperand(dst, Context::SlotOffset(index)));
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(StackFrame::TypeToMarker(type)));
  PushCommonFrame(scratch);
}

void TurboAssembler::Prologue() { PushStandardFrame(a1); }

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int stack_offset = -3 * kPointerSize;
  const int fp_offset = 1 * kPointerSize;
  addiu(sp, sp, stack_offset);
  stack_offset = -stack_offset - kPointerSize;
  sw(ra, MemOperand(sp, stack_offset));
  stack_offset -= kPointerSize;
  sw(fp, MemOperand(sp, stack_offset));
  stack_offset -= kPointerSize;
  li(t9, Operand(StackFrame::TypeToMarker(type)));
  sw(t9, MemOperand(sp, stack_offset));
  // Adjust FP to point to saved FP.
  DCHECK_EQ(stack_offset, 0);
  Addu(fp, sp, Operand(fp_offset));
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  addiu(sp, fp, 2 * kPointerSize);
  lw(ra, MemOperand(fp, 1 * kPointerSize));
  lw(fp, MemOperand(fp, 0 * kPointerSize));
}

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  STATIC_ASSERT(2 * kPointerSize == ExitFrameConstants::kCallerSPDisplacement);
  STATIC_ASSERT(1 * kPointerSize == ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT(0 * kPointerSize == ExitFrameConstants::kCallerFPOffset);

  // This is how the stack will look:
  // fp + 2 (==kCallerSPDisplacement) - old stack's end
  // [fp + 1 (==kCallerPCOffset)] - saved old ra
  // [fp + 0 (==kCallerFPOffset)] - saved old fp
  // [fp - 1 StackFrame::EXIT Smi
  // [fp - 2 (==kSPOffset)] - sp of the called function
  // fp - (2 + stack_space + alignment) == sp == [fp - kSPOffset] - top of the
  //   new stack (will contain saved ra)

  // Save registers and reserve room for saved entry sp.
  addiu(sp, sp, -2 * kPointerSize - ExitFrameConstants::kFixedFrameSizeFromFp);
  sw(ra, MemOperand(sp, 3 * kPointerSize));
  sw(fp, MemOperand(sp, 2 * kPointerSize));
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, Operand(StackFrame::TypeToMarker(frame_type)));
    sw(scratch, MemOperand(sp, 1 * kPointerSize));
  }
  // Set up new frame pointer.
  addiu(fp, sp, ExitFrameConstants::kFixedFrameSizeFromFp);

  if (emit_debug_code()) {
    sw(zero_reg, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }

  // Save the frame pointer and the context in top.
  li(t8,
     ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  sw(fp, MemOperand(t8));
  li(t8,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  sw(cp, MemOperand(t8));

  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (save_doubles) {
    // The stack  must be align to 0 modulo 8 for stores with sdc1.
    DCHECK_EQ(kDoubleSize, frame_alignment);
    if (frame_alignment > 0) {
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      And(sp, sp, Operand(-frame_alignment));  // Align stack.
    }
    int space = FPURegister::kNumRegisters * kDoubleSize;
    Subu(sp, sp, Operand(space));
    // Remember: we only need to save every 2nd double FPU value.
    for (int i = 0; i < FPURegister::kNumRegisters; i += 2) {
      FPURegister reg = FPURegister::from_code(i);
      Sdc1(reg, MemOperand(sp, i * kDoubleSize));
    }
  }

  // Reserve place for the return address, stack space and an optional slot
  // (used by DirectCEntry to hold the return value if a struct is
  // returned) and align the frame preparing for calling the runtime function.
  DCHECK_GE(stack_space, 0);
  Subu(sp, sp, Operand((stack_space + 2) * kPointerSize));
  if (frame_alignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));  // Align stack.
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  addiu(scratch, sp, kPointerSize);
  sw(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool do_return,
                                    bool argument_count_is_length) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Remember: we only need to restore every 2nd double FPU value.
    lw(t8, MemOperand(fp, ExitFrameConstants::kSPOffset));
    for (int i = 0; i < FPURegister::kNumRegisters; i += 2) {
      FPURegister reg = FPURegister::from_code(i);
      Ldc1(reg, MemOperand(t8, i * kDoubleSize + kPointerSize));
    }
  }

  // Clear top frame.
  li(t8,
     ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  sw(zero_reg, MemOperand(t8));

  // Restore current context from top and clear it in debug mode.
  li(t8,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  lw(cp, MemOperand(t8));

#ifdef DEBUG
  li(t8,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  sw(a3, MemOperand(t8));
#endif

  // Pop the arguments, restore registers, and return.
  mov(sp, fp);  // Respect ABI stack constraint.
  lw(fp, MemOperand(sp, ExitFrameConstants::kCallerFPOffset));
  lw(ra, MemOperand(sp, ExitFrameConstants::kCallerPCOffset));

  if (argument_count.is_valid()) {
    if (argument_count_is_length) {
      addu(sp, sp, argument_count);
    } else {
      Lsa(sp, sp, argument_count, kPointerSizeLog2, t8);
    }
  }

  if (do_return) {
    Ret(USE_DELAY_SLOT);
    // If returning, the instruction in the delay slot will be the addiu below.
  }
  addiu(sp, sp, 8);
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_MIPS
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one Mips
  // platform for another Mips platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else   // V8_HOST_ARCH_MIPS
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_MIPS
}

void MacroAssembler::AssertStackIsAligned() {
  if (emit_debug_code()) {
    const int frame_alignment = ActivationFrameAlignment();
    const int frame_alignment_mask = frame_alignment - 1;

    if (frame_alignment > kPointerSize) {
      Label alignment_as_expected;
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      andi(scratch, sp, frame_alignment_mask);
      Branch(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      stop();
      bind(&alignment_as_expected);
    }
  }
}

void TurboAssembler::JumpIfSmi(Register value, Label* smi_label,
                               Register scratch, BranchDelaySlot bd) {
  DCHECK_EQ(0, kSmiTag);
  andi(scratch, value, kSmiTagMask);
  Branch(bd, smi_label, eq, scratch, Operand(zero_reg));
}

void MacroAssembler::JumpIfNotSmi(Register value, Label* not_smi_label,
                                  Register scratch, BranchDelaySlot bd) {
  DCHECK_EQ(0, kSmiTag);
  andi(scratch, value, kSmiTagMask);
  Branch(bd, not_smi_label, ne, scratch, Operand(zero_reg));
}

void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    andi(scratch, object, kSmiTagMask);
    Check(ne, AbortReason::kOperandIsASmi, scratch, Operand(zero_reg));
  }
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    andi(scratch, object, kSmiTagMask);
    Check(eq, AbortReason::kOperandIsASmi, scratch, Operand(zero_reg));
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (emit_debug_code()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    SmiTst(object, t8);
    Check(ne, AbortReason::kOperandIsASmiAndNotAConstructor, t8,
          Operand(zero_reg));

    LoadMap(t8, object);
    lbu(t8, FieldMemOperand(t8, Map::kBitFieldOffset));
    And(t8, t8, Operand(Map::Bits1::IsConstructorBit::kMask));
    Check(ne, AbortReason::kOperandIsNotAConstructor, t8, Operand(zero_reg));
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    SmiTst(object, t8);
    Check(ne, AbortReason::kOperandIsASmiAndNotAFunction, t8,
          Operand(zero_reg));
    GetObjectType(object, t8, t8);
    Check(eq, AbortReason::kOperandIsNotAFunction, t8,
          Operand(JS_FUNCTION_TYPE));
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    SmiTst(object, t8);
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction, t8,
          Operand(zero_reg));
    GetObjectType(object, t8, t8);
    Check(eq, AbortReason::kOperandIsNotABoundFunction, t8,
          Operand(JS_BOUND_FUNCTION_TYPE));
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  BlockTrampolinePoolScope block_trampoline_pool(this);
  STATIC_ASSERT(kSmiTag == 0);
  SmiTst(object, t8);
  Check(ne, AbortReason::kOperandIsASmiAndNotAGeneratorObject, t8,
        Operand(zero_reg));

  GetObjectType(object, t8, t8);

  Label done;

  // Check if JSGeneratorObject
  Branch(&done, eq, t8, Operand(JS_GENERATOR_OBJECT_TYPE));

  // Check if JSAsyncFunctionObject (See MacroAssembler::CompareInstanceType)
  Branch(&done, eq, t8, Operand(JS_ASYNC_FUNCTION_OBJECT_TYPE));

  // Check if JSAsyncGeneratorObject
  Branch(&done, eq, t8, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

  Abort(AbortReason::kOperandIsNotAGeneratorObject);

  bind(&done);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    LoadRoot(scratch, RootIndex::kUndefinedValue);
    Branch(&done_checking, eq, object, Operand(scratch));
    GetObjectType(object, scratch, scratch);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell, scratch,
           Operand(ALLOCATION_SITE_TYPE));
    bind(&done_checking);
  }
}

void TurboAssembler::Float32Max(FPURegister dst, FPURegister src1,
                                FPURegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_s(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF32(src1, src2);
  BranchTrueF(out_of_line);

  if (IsMipsArchVariant(kMips32r6)) {
    max_s(dst, src1, src2);
  } else {
    Label return_left, return_right, done;

    CompareF32(OLT, src1, src2);
    BranchTrueShortF(&return_right);
    CompareF32(OLT, src2, src1);
    BranchTrueShortF(&return_left);

    // Operands are equal, but check for +/-0.
    {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      mfc1(t8, src1);
      Branch(&return_left, eq, t8, Operand(zero_reg));
      Branch(&return_right);
    }

    bind(&return_right);
    if (src2 != dst) {
      Move_s(dst, src2);
    }
    Branch(&done);

    bind(&return_left);
    if (src1 != dst) {
      Move_s(dst, src1);
    }

    bind(&done);
  }
}

void TurboAssembler::Float32MaxOutOfLine(FPURegister dst, FPURegister src1,
                                         FPURegister src2) {
  add_s(dst, src1, src2);
}

void TurboAssembler::Float32Min(FPURegister dst, FPURegister src1,
                                FPURegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_s(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF32(src1, src2);
  BranchTrueF(out_of_line);

  if (IsMipsArchVariant(kMips32r6)) {
    min_s(dst, src1, src2);
  } else {
    Label return_left, return_right, done;

    CompareF32(OLT, src1, src2);
    BranchTrueShortF(&return_left);
    CompareF32(OLT, src2, src1);
    BranchTrueShortF(&return_right);

    // Left equals right => check for -0.
    {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      mfc1(t8, src1);
      Branch(&return_right, eq, t8, Operand(zero_reg));
      Branch(&return_left);
    }

    bind(&return_right);
    if (src2 != dst) {
      Move_s(dst, src2);
    }
    Branch(&done);

    bind(&return_left);
    if (src1 != dst) {
      Move_s(dst, src1);
    }

    bind(&done);
  }
}

void TurboAssembler::Float32MinOutOfLine(FPURegister dst, FPURegister src1,
                                         FPURegister src2) {
  add_s(dst, src1, src2);
}

void TurboAssembler::Float64Max(DoubleRegister dst, DoubleRegister src1,
                                DoubleRegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_d(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF64(src1, src2);
  BranchTrueF(out_of_line);

  if (IsMipsArchVariant(kMips32r6)) {
    max_d(dst, src1, src2);
  } else {
    Label return_left, return_right, done;

    CompareF64(OLT, src1, src2);
    BranchTrueShortF(&return_right);
    CompareF64(OLT, src2, src1);
    BranchTrueShortF(&return_left);

    // Left equals right => check for -0.
    {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Mfhc1(t8, src1);
      Branch(&return_left, eq, t8, Operand(zero_reg));
      Branch(&return_right);
    }

    bind(&return_right);
    if (src2 != dst) {
      Move_d(dst, src2);
    }
    Branch(&done);

    bind(&return_left);
    if (src1 != dst) {
      Move_d(dst, src1);
    }

    bind(&done);
  }
}

void TurboAssembler::Float64MaxOutOfLine(DoubleRegister dst,
                                         DoubleRegister src1,
                                         DoubleRegister src2) {
  add_d(dst, src1, src2);
}

void TurboAssembler::Float64Min(DoubleRegister dst, DoubleRegister src1,
                                DoubleRegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_d(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF64(src1, src2);
  BranchTrueF(out_of_line);

  if (IsMipsArchVariant(kMips32r6)) {
    min_d(dst, src1, src2);
  } else {
    Label return_left, return_right, done;

    CompareF64(OLT, src1, src2);
    BranchTrueShortF(&return_left);
    CompareF64(OLT, src2, src1);
    BranchTrueShortF(&return_right);

    // Left equals right => check for -0.
    {
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Mfhc1(t8, src1);
      Branch(&return_right, eq, t8, Operand(zero_reg));
      Branch(&return_left);
    }

    bind(&return_right);
    if (src2 != dst) {
      Move_d(dst, src2);
    }
    Branch(&done);

    bind(&return_left);
    if (src1 != dst) {
      Move_d(dst, src1);
    }

    bind(&done);
  }
}

void TurboAssembler::Float64MinOutOfLine(DoubleRegister dst,
                                         DoubleRegister src1,
                                         DoubleRegister src2) {
  add_d(dst, src1, src2);
}

static const int kRegisterPassedArguments = 4;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  num_reg_arguments += 2 * num_double_arguments;

  // Up to four simple arguments are passed in registers a0..a3.
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
  stack_passed_words += kCArgSlotCount;
  return stack_passed_words;
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();

  // Up to four simple arguments are passed in registers a0..a3.
  // Those four arguments must have reserved argument slots on the stack for
  // mips, even though those argument slots are not normally used.
  // Remaining arguments are pushed on the stack, above (higher address than)
  // the argument slots.
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    Subu(sp, sp, Operand((stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));
    sw(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Subu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  // Linux/MIPS convention demands that register t9 contains
  // the address of the function being call in case of
  // Position independent code
  BlockTrampolinePoolScope block_trampoline_pool(this);
  li(t9, function);
  CallCFunctionHelper(t9, 0, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, 0, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunctionHelper(Register function_base,
                                         int16_t function_offset,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK_LE(num_reg_arguments + num_double_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
  // The argument stots are presumed to have been set up by
  // PrepareCallCFunction. The C function must be called via t9, for mips ABI.

#if V8_HOST_ARCH_MIPS
  if (emit_debug_code()) {
    int frame_alignment = base::OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      Label alignment_as_expected;
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      And(scratch, sp, Operand(frame_alignment_mask));
      Branch(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop();
      bind(&alignment_as_expected);
    }
  }
#endif  // V8_HOST_ARCH_MIPS

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.

  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (function_base != t9) {
      mov(t9, function_base);
      function_base = t9;
    }

    if (function_offset != 0) {
      addiu(t9, t9, function_offset);
      function_offset = 0;
    }

    // Save the frame pointer and PC so that the stack layout remains iterable,
    // even without an ExitFrame which normally exists between JS and C frames.
    if (isolate() != nullptr) {
      // 't' registers are caller-saved so this is safe as a scratch register.
      Register scratch1 = t4;
      Register scratch2 = t5;
      DCHECK(!AreAliased(scratch1, scratch2, function_base));

      Label get_pc;
      mov(scratch1, ra);
      Call(&get_pc);

      bind(&get_pc);
      mov(scratch2, ra);
      mov(ra, scratch1);

      li(scratch1, ExternalReference::fast_c_call_caller_pc_address(isolate()));
      sw(scratch2, MemOperand(scratch1));
      li(scratch1, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      sw(fp, MemOperand(scratch1));
    }

    Call(function_base, function_offset);

    if (isolate() != nullptr) {
      // We don't unset the PC; the FP is the source of truth.
      Register scratch = t4;
      li(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      sw(zero_reg, MemOperand(scratch));
    }
  }

  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);

  if (base::OS::ActivationFrameAlignment() > kPointerSize) {
    lw(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Addu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}

#undef BRANCH_ARGS_CHECK

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met) {
  And(scratch, object, Operand(~kPageAlignmentMask));
  lw(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  And(scratch, scratch, Operand(mask));
  Branch(condition_met, cc, scratch, Operand(zero_reg));
}

Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2, Register reg3,
                                   Register reg4, Register reg5,
                                   Register reg6) {
  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  // This push on ra and the pop below together ensure that we restore the
  // register ra, which is needed while computing the code start address.
  push(ra);

  // The nal instruction puts the address of the current instruction into
  // the return address (ra) register, which we can use later on.
  if (IsMipsArchVariant(kMips32r6)) {
    addiupc(ra, 1);
  } else {
    nal();
    nop();
  }
  int pc = pc_offset();
  li(dst, pc);
  subu(dst, ra, dst);

  pop(ra);  // Restore ra
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  li(kSpeculationPoisonRegister, -1);
}

void TurboAssembler::CallForDeoptimization(Address target, int deopt_id) {
  NoRootArrayScope no_root_array(this);

  // Save the deipt id in kRootRegister (we don't need the roots array from now
  // on).
  DCHECK_LE(deopt_id, 0xFFFF);
  li(kRootRegister, deopt_id);

  Call(target, RelocInfo::RUNTIME_ENTRY);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
