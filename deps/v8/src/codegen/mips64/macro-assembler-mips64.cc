// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_MIPS64

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
#include "src/codegen/mips64/macro-assembler-mips64.h"
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
  Ld(destination, MemOperand(s6, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition cond, Register src1,
                              const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  Ld(destination, MemOperand(s6, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Push(ra, fp, marker_reg);
    Daddu(fp, sp, Operand(kPointerSize));
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
  Daddu(fp, sp, Operand(offset));
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

  Daddu(dst, object, Operand(offset - kHeapObjectTag));
  if (emit_debug_code()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Label ok;
    And(t8, dst, Operand(kPointerSize - 1));
    Branch(&ok, eq, t8, Operand(zero_reg));
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object, dst, value, ra_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    li(value, Operand(bit_cast<int64_t>(kZapValue + 4)));
    li(dst, Operand(bit_cast<int64_t>(kZapValue + 8)));
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
    Ld(scratch, MemOperand(address));
    Assert(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite, scratch,
           Operand(value));
  }

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
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
    li(address, Operand(bit_cast<int64_t>(kZapValue + 12)));
    li(value, Operand(bit_cast<int64_t>(kZapValue + 16)));
  }
}

// ---------------------------------------------------------------------------
// Instruction macros.

void TurboAssembler::Addu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    addu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiu(rd, rs, static_cast<int32_t>(rt.immediate()));
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

void TurboAssembler::Daddu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    daddu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      daddiu(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      daddu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Subu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    subu(rd, rs, rt.rm());
  } else {
    DCHECK(is_int32(rt.immediate()));
    if (is_int16(-rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiu(rd, rs,
            static_cast<int32_t>(
                -rt.immediate()));  // No subiu instr, use addiu(x, y, -imm).
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      if (-rt.immediate() >> 16 == 0 && !MustUseReg(rt.rmode())) {
        // Use load -imm and addu when loading -imm generates one instruction.
        li(scratch, -rt.immediate());
        addu(rd, rs, scratch);
      } else {
        // li handles the relocation.
        li(scratch, rt);
        subu(rd, rs, scratch);
      }
    }
  }
}

void TurboAssembler::Dsubu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    dsubu(rd, rs, rt.rm());
  } else if (is_int16(-rt.immediate()) && !MustUseReg(rt.rmode())) {
    daddiu(rd, rs,
           static_cast<int32_t>(
               -rt.immediate()));  // No dsubiu instr, use daddiu(x, y, -imm).
  } else {
    DCHECK(rs != at);
    int li_count = InstrCountForLi64Bit(rt.immediate());
    int li_neg_count = InstrCountForLi64Bit(-rt.immediate());
    if (li_neg_count < li_count && !MustUseReg(rt.rmode())) {
      // Use load -imm and daddu when loading -imm generates one instruction.
      DCHECK(rt.immediate() != std::numeric_limits<int32_t>::min());
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(-rt.immediate()));
      Daddu(rd, rs, scratch);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, rt);
      dsubu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mul(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    mul(rd, rs, scratch);
  }
}

void TurboAssembler::Mulh(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      mult(rs, scratch);
      mfhi(rd);
    } else {
      muh(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mulhu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      multu(rs, scratch);
      mfhi(rd);
    } else {
      muhu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Dmul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant == kMips64r6) {
      dmul(rd, rs, rt.rm());
    } else {
      dmult(rs, rt.rm());
      mflo(rd);
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (kArchVariant == kMips64r6) {
      dmul(rd, rs, scratch);
    } else {
      dmult(rs, scratch);
      mflo(rd);
    }
  }
}

void TurboAssembler::Dmulh(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant == kMips64r6) {
      dmuh(rd, rs, rt.rm());
    } else {
      dmult(rs, rt.rm());
      mfhi(rd);
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (kArchVariant == kMips64r6) {
      dmuh(rd, rs, scratch);
    } else {
      dmult(rs, scratch);
      mfhi(rd);
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

void TurboAssembler::Dmult(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    dmult(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    dmult(rs, scratch);
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

void TurboAssembler::Dmultu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    dmultu(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    dmultu(rs, scratch);
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

void TurboAssembler::Div(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      div(rs, scratch);
      mflo(res);
    } else {
      div(res, rs, scratch);
    }
  }
}

void TurboAssembler::Mod(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      div(rs, scratch);
      mfhi(rd);
    } else {
      mod(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Modu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      divu(rs, scratch);
      mfhi(rd);
    } else {
      modu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Ddiv(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    ddiv(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    ddiv(rs, scratch);
  }
}

void TurboAssembler::Ddiv(Register rd, Register rs, const Operand& rt) {
  if (kArchVariant != kMips64r6) {
    if (rt.is_reg()) {
      ddiv(rs, rt.rm());
      mflo(rd);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      ddiv(rs, scratch);
      mflo(rd);
    }
  } else {
    if (rt.is_reg()) {
      ddiv(rd, rs, rt.rm());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      ddiv(rd, rs, scratch);
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
    if (kArchVariant != kMips64r6) {
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
    if (kArchVariant != kMips64r6) {
      divu(rs, scratch);
      mflo(res);
    } else {
      divu(res, rs, scratch);
    }
  }
}

void TurboAssembler::Ddivu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    ddivu(rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    ddivu(rs, scratch);
  }
}

void TurboAssembler::Ddivu(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant != kMips64r6) {
      ddivu(rs, rt.rm());
      mflo(res);
    } else {
      ddivu(res, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(rs != scratch);
    li(scratch, rt);
    if (kArchVariant != kMips64r6) {
      ddivu(rs, scratch);
      mflo(res);
    } else {
      ddivu(res, rs, scratch);
    }
  }
}

void TurboAssembler::Dmod(Register rd, Register rs, const Operand& rt) {
  if (kArchVariant != kMips64r6) {
    if (rt.is_reg()) {
      ddiv(rs, rt.rm());
      mfhi(rd);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      ddiv(rs, scratch);
      mfhi(rd);
    }
  } else {
    if (rt.is_reg()) {
      dmod(rd, rs, rt.rm());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      dmod(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Dmodu(Register rd, Register rs, const Operand& rt) {
  if (kArchVariant != kMips64r6) {
    if (rt.is_reg()) {
      ddivu(rs, rt.rm());
      mfhi(rd);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      ddivu(rs, scratch);
      mfhi(rd);
    }
  } else {
    if (rt.is_reg()) {
      dmodu(rd, rs, rt.rm());
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      DCHECK(rs != scratch);
      li(scratch, rt);
      dmodu(rd, rs, scratch);
    }
  }
}

void TurboAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    and_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      andi(rd, rs, static_cast<int32_t>(rt.immediate()));
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
      ori(rd, rs, static_cast<int32_t>(rt.immediate()));
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
      xori(rd, rs, static_cast<int32_t>(rt.immediate()));
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
  dsubu(rs, zero_reg, rt.rm());
}

void TurboAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.immediate()) && !MustUseReg(rt.rmode())) {
      slti(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
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
    const uint64_t int16_min = std::numeric_limits<int16_t>::min();
    if (is_uint15(rt.immediate()) && !MustUseReg(rt.rmode())) {
      // Imm range is: [0, 32767].
      sltiu(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else if (is_uint15(rt.immediate() - int16_min) &&
               !MustUseReg(rt.rmode())) {
      // Imm range is: [max_unsigned-32767,max_unsigned].
      sltiu(rd, rs, static_cast<uint16_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
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
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    BlockTrampolinePoolScope block_trampoline_pool(this);
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
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    BlockTrampolinePoolScope block_trampoline_pool(this);
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
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    BlockTrampolinePoolScope block_trampoline_pool(this);
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
    UseScratchRegisterScope temps(this);
    Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK(rs != scratch);
    li(scratch, rt);
    sltu(rd, scratch, rs);
  }
}

void TurboAssembler::Ror(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    rotrv(rd, rs, rt.rm());
  } else {
    int64_t ror_value = rt.immediate() % 32;
    if (ror_value < 0) {
      ror_value += 32;
    }
    rotr(rd, rs, ror_value);
  }
}

void TurboAssembler::Dror(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    drotrv(rd, rs, rt.rm());
  } else {
    int64_t dror_value = rt.immediate() % 64;
    if (dror_value < 0) dror_value += 64;
    if (dror_value <= 31) {
      drotr(rd, rs, dror_value);
    } else {
      drotr32(rd, rs, dror_value - 32);
    }
  }
}

void MacroAssembler::Pref(int32_t hint, const MemOperand& rs) {
  pref(hint, rs);
}

void TurboAssembler::Lsa(Register rd, Register rt, Register rs, uint8_t sa,
                         Register scratch) {
  DCHECK(sa >= 1 && sa <= 31);
  if (kArchVariant == kMips64r6 && sa <= 4) {
    lsa(rd, rt, rs, sa - 1);
  } else {
    Register tmp = rd == rt ? scratch : rd;
    DCHECK(tmp != rt);
    sll(tmp, rs, sa);
    Addu(rd, rt, tmp);
  }
}

void TurboAssembler::Dlsa(Register rd, Register rt, Register rs, uint8_t sa,
                          Register scratch) {
  DCHECK(sa >= 1 && sa <= 31);
  if (kArchVariant == kMips64r6 && sa <= 4) {
    dlsa(rd, rt, rs, sa - 1);
  } else {
    Register tmp = rd == rt ? scratch : rd;
    DCHECK(tmp != rt);
    dsll(tmp, rs, sa);
    Daddu(rd, rt, tmp);
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

// Change endianness
void TurboAssembler::ByteSwapSigned(Register dest, Register src,
                                    int operand_size) {
  DCHECK(operand_size == 2 || operand_size == 4 || operand_size == 8);
  DCHECK(kArchVariant == kMips64r6 || kArchVariant == kMips64r2);
  if (operand_size == 2) {
    wsbh(dest, src);
    seh(dest, dest);
  } else if (operand_size == 4) {
    wsbh(dest, src);
    rotr(dest, dest, 16);
  } else {
    dsbh(dest, src);
    dshd(dest, dest);
  }
}

void TurboAssembler::ByteSwapUnsigned(Register dest, Register src,
                                      int operand_size) {
  DCHECK(operand_size == 2 || operand_size == 4);
  if (operand_size == 2) {
    wsbh(dest, src);
    andi(dest, dest, 0xFFFF);
  } else {
    wsbh(dest, src);
    rotr(dest, dest, 16);
    dinsu_(dest, zero_reg, 32, 32);
  }
}

void TurboAssembler::Ulw(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (kArchVariant == kMips64r6) {
    Lw(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(kMipsLwrOffset <= 3 && kMipsLwlOffset <= 3);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 3 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 3);
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

void TurboAssembler::Ulwu(Register rd, const MemOperand& rs) {
  if (kArchVariant == kMips64r6) {
    Lwu(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    Ulw(rd, rs);
    Dext(rd, rd, 0, 32);
  }
}

void TurboAssembler::Usw(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  DCHECK(rd != rs.rm());
  if (kArchVariant == kMips64r6) {
    Sw(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(kMipsSwrOffset <= 3 && kMipsSwlOffset <= 3);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 3 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 3);
    swr(rd, MemOperand(source.rm(), source.offset() + kMipsSwrOffset));
    swl(rd, MemOperand(source.rm(), source.offset() + kMipsSwlOffset));
  }
}

void TurboAssembler::Ulh(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (kArchVariant == kMips64r6) {
    Lh(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 1);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (source.rm() == scratch) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      Lb(rd, MemOperand(source.rm(), source.offset() + 1));
      Lbu(scratch, source);
#elif defined(V8_TARGET_BIG_ENDIAN)
      Lb(rd, source);
      Lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
#endif
    } else {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      Lbu(scratch, source);
      Lb(rd, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
      Lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
      Lb(rd, source);
#endif
    }
    dsll(rd, rd, 8);
    or_(rd, rd, scratch);
  }
}

void TurboAssembler::Ulhu(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (kArchVariant == kMips64r6) {
    Lhu(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 1);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (source.rm() == scratch) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      Lbu(rd, MemOperand(source.rm(), source.offset() + 1));
      Lbu(scratch, source);
#elif defined(V8_TARGET_BIG_ENDIAN)
      Lbu(rd, source);
      Lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
#endif
    } else {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      Lbu(scratch, source);
      Lbu(rd, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
      Lbu(scratch, MemOperand(source.rm(), source.offset() + 1));
      Lbu(rd, source);
#endif
    }
    dsll(rd, rd, 8);
    or_(rd, rd, scratch);
  }
}

void TurboAssembler::Ush(Register rd, const MemOperand& rs, Register scratch) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  DCHECK(rs.rm() != scratch);
  DCHECK(scratch != at);
  if (kArchVariant == kMips64r6) {
    Sh(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 1 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 1);

    if (scratch != rd) {
      mov(scratch, rd);
    }

#if defined(V8_TARGET_LITTLE_ENDIAN)
    Sb(scratch, source);
    srl(scratch, scratch, 8);
    Sb(scratch, MemOperand(source.rm(), source.offset() + 1));
#elif defined(V8_TARGET_BIG_ENDIAN)
    Sb(scratch, MemOperand(source.rm(), source.offset() + 1));
    srl(scratch, scratch, 8);
    Sb(scratch, source);
#endif
  }
}

void TurboAssembler::Uld(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (kArchVariant == kMips64r6) {
    Ld(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(kMipsLdrOffset <= 7 && kMipsLdlOffset <= 7);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 7 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 7);
    if (rd != source.rm()) {
      ldr(rd, MemOperand(source.rm(), source.offset() + kMipsLdrOffset));
      ldl(rd, MemOperand(source.rm(), source.offset() + kMipsLdlOffset));
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      ldr(scratch, MemOperand(rs.rm(), rs.offset() + kMipsLdrOffset));
      ldl(scratch, MemOperand(rs.rm(), rs.offset() + kMipsLdlOffset));
      mov(rd, scratch);
    }
  }
}

// Load consequent 32-bit word pair in 64-bit reg. and put first word in low
// bits,
// second word in high bits.
void MacroAssembler::LoadWordPair(Register rd, const MemOperand& rs,
                                  Register scratch) {
  Lwu(rd, rs);
  Lw(scratch, MemOperand(rs.rm(), rs.offset() + kPointerSize / 2));
  dsll32(scratch, scratch, 0);
  Daddu(rd, rd, scratch);
}

void TurboAssembler::Usd(Register rd, const MemOperand& rs) {
  DCHECK(rd != at);
  DCHECK(rs.rm() != at);
  if (kArchVariant == kMips64r6) {
    Sd(rd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(kMipsSdrOffset <= 7 && kMipsSdlOffset <= 7);
    MemOperand source = rs;
    // Adjust offset for two accesses and check if offset + 7 fits into int16_t.
    AdjustBaseAndOffset(source, OffsetAccessType::TWO_ACCESSES, 7);
    sdr(rd, MemOperand(source.rm(), source.offset() + kMipsSdrOffset));
    sdl(rd, MemOperand(source.rm(), source.offset() + kMipsSdlOffset));
  }
}

// Do 64-bit store as two consequent 32-bit stores to unaligned address.
void MacroAssembler::StoreWordPair(Register rd, const MemOperand& rs,
                                   Register scratch) {
  Sw(rd, rs);
  dsrl32(scratch, rd, 0);
  Sw(scratch, MemOperand(rs.rm(), rs.offset() + kPointerSize / 2));
}

void TurboAssembler::Ulwc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  if (kArchVariant == kMips64r6) {
    Lwc1(fd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    Ulw(scratch, rs);
    mtc1(scratch, fd);
  }
}

void TurboAssembler::Uswc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  if (kArchVariant == kMips64r6) {
    Swc1(fd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    mfc1(scratch, fd);
    Usw(scratch, rs);
  }
}

void TurboAssembler::Uldc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  DCHECK(scratch != at);
  if (kArchVariant == kMips64r6) {
    Ldc1(fd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    Uld(scratch, rs);
    dmtc1(scratch, fd);
  }
}

void TurboAssembler::Usdc1(FPURegister fd, const MemOperand& rs,
                           Register scratch) {
  DCHECK(scratch != at);
  if (kArchVariant == kMips64r6) {
    Sdc1(fd, rs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    dmfc1(scratch, fd);
    Usd(scratch, rs);
  }
}

void TurboAssembler::Lb(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lb(rd, source);
}

void TurboAssembler::Lbu(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lbu(rd, source);
}

void TurboAssembler::Sb(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  sb(rd, source);
}

void TurboAssembler::Lh(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lh(rd, source);
}

void TurboAssembler::Lhu(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lhu(rd, source);
}

void TurboAssembler::Sh(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  sh(rd, source);
}

void TurboAssembler::Lw(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lw(rd, source);
}

void TurboAssembler::Lwu(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  lwu(rd, source);
}

void TurboAssembler::Sw(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  sw(rd, source);
}

void TurboAssembler::Ld(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  ld(rd, source);
}

void TurboAssembler::Sd(Register rd, const MemOperand& rs) {
  MemOperand source = rs;
  AdjustBaseAndOffset(source);
  sd(rd, source);
}

void TurboAssembler::Lwc1(FPURegister fd, const MemOperand& src) {
  MemOperand tmp = src;
  AdjustBaseAndOffset(tmp);
  lwc1(fd, tmp);
}

void TurboAssembler::Swc1(FPURegister fs, const MemOperand& src) {
  MemOperand tmp = src;
  AdjustBaseAndOffset(tmp);
  swc1(fs, tmp);
}

void TurboAssembler::Ldc1(FPURegister fd, const MemOperand& src) {
  MemOperand tmp = src;
  AdjustBaseAndOffset(tmp);
  ldc1(fd, tmp);
}

void TurboAssembler::Sdc1(FPURegister fs, const MemOperand& src) {
  MemOperand tmp = src;
  AdjustBaseAndOffset(tmp);
  sdc1(fs, tmp);
}

void TurboAssembler::Ll(Register rd, const MemOperand& rs) {
  bool is_one_instruction = (kArchVariant == kMips64r6) ? is_int9(rs.offset())
                                                        : is_int16(rs.offset());
  if (is_one_instruction) {
    ll(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    daddu(scratch, scratch, rs.rm());
    ll(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::Lld(Register rd, const MemOperand& rs) {
  bool is_one_instruction = (kArchVariant == kMips64r6) ? is_int9(rs.offset())
                                                        : is_int16(rs.offset());
  if (is_one_instruction) {
    lld(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    daddu(scratch, scratch, rs.rm());
    lld(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::Sc(Register rd, const MemOperand& rs) {
  bool is_one_instruction = (kArchVariant == kMips64r6) ? is_int9(rs.offset())
                                                        : is_int16(rs.offset());
  if (is_one_instruction) {
    sc(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    daddu(scratch, scratch, rs.rm());
    sc(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::Scd(Register rd, const MemOperand& rs) {
  bool is_one_instruction = (kArchVariant == kMips64r6) ? is_int9(rs.offset())
                                                        : is_int16(rs.offset());
  if (is_one_instruction) {
    scd(rd, rs);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, rs.offset());
    daddu(scratch, scratch, rs.rm());
    scd(rd, MemOperand(scratch, 0));
  }
}

void TurboAssembler::li(Register dst, Handle<HeapObject> value, LiFlags mode) {
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadConstant(dst, value);
      return;
    }
  }
  li(dst, Operand(value), mode);
}

void TurboAssembler::li(Register dst, ExternalReference value, LiFlags mode) {
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadExternalReference(dst, value);
      return;
    }
  }
  li(dst, Operand(value), mode);
}

void TurboAssembler::li(Register dst, const StringConstantBase* string,
                        LiFlags mode) {
  li(dst, Operand::EmbeddedStringConstant(string), mode);
}

static inline int InstrCountForLiLower32Bit(int64_t value) {
  if (!is_int16(static_cast<int32_t>(value)) && (value & kUpper16MaskOf64) &&
      (value & kImm16Mask)) {
    return 2;
  } else {
    return 1;
  }
}

void TurboAssembler::LiLower32BitHelper(Register rd, Operand j) {
  if (is_int16(static_cast<int32_t>(j.immediate()))) {
    daddiu(rd, zero_reg, (j.immediate() & kImm16Mask));
  } else if (!(j.immediate() & kUpper16MaskOf64)) {
    ori(rd, zero_reg, j.immediate() & kImm16Mask);
  } else {
    lui(rd, j.immediate() >> kLuiShift & kImm16Mask);
    if (j.immediate() & kImm16Mask) {
      ori(rd, rd, j.immediate() & kImm16Mask);
    }
  }
}

static inline int InstrCountForLoadReplicatedConst32(int64_t value) {
  uint32_t x = static_cast<uint32_t>(value);
  uint32_t y = static_cast<uint32_t>(value >> 32);

  if (x == y) {
    return (is_uint16(x) || is_int16(x) || (x & kImm16Mask) == 0) ? 2 : 3;
  }

  return INT_MAX;
}

int TurboAssembler::InstrCountForLi64Bit(int64_t value) {
  if (is_int32(value)) {
    return InstrCountForLiLower32Bit(value);
  } else {
    int bit31 = value >> 31 & 0x1;
    if ((value & kUpper16MaskOf64) == 0 && is_int16(value >> 32) &&
        kArchVariant == kMips64r6) {
      return 2;
    } else if ((value & (kHigher16MaskOf64 | kUpper16MaskOf64)) == 0 &&
               kArchVariant == kMips64r6) {
      return 2;
    } else if ((value & kImm16Mask) == 0 && is_int16((value >> 32) + bit31) &&
               kArchVariant == kMips64r6) {
      return 2;
    } else if ((value & kImm16Mask) == 0 &&
               ((value >> 31) & 0x1FFFF) == ((0x20000 - bit31) & 0x1FFFF) &&
               kArchVariant == kMips64r6) {
      return 2;
    } else if (is_int16(static_cast<int32_t>(value)) &&
               is_int16((value >> 32) + bit31) && kArchVariant == kMips64r6) {
      return 2;
    } else if (is_int16(static_cast<int32_t>(value)) &&
               ((value >> 31) & 0x1FFFF) == ((0x20000 - bit31) & 0x1FFFF) &&
               kArchVariant == kMips64r6) {
      return 2;
    } else if (base::bits::IsPowerOfTwo(value + 1) ||
               value == std::numeric_limits<int64_t>::max()) {
      return 2;
    } else {
      int shift_cnt = base::bits::CountTrailingZeros64(value);
      int rep32_count = InstrCountForLoadReplicatedConst32(value);
      int64_t tmp = value >> shift_cnt;
      if (is_uint16(tmp)) {
        return 2;
      } else if (is_int16(tmp)) {
        return 2;
      } else if (rep32_count < 3) {
        return 2;
      } else if (is_int32(tmp)) {
        return 3;
      } else {
        shift_cnt = 16 + base::bits::CountTrailingZeros64(value >> 16);
        tmp = value >> shift_cnt;
        if (is_uint16(tmp)) {
          return 3;
        } else if (is_int16(tmp)) {
          return 3;
        } else if (rep32_count < 4) {
          return 3;
        } else if (kArchVariant == kMips64r6) {
          int64_t imm = value;
          int count = InstrCountForLiLower32Bit(imm);
          imm = (imm >> 32) + bit31;
          if (imm & kImm16Mask) {
            count++;
          }
          imm = (imm >> 16) + (imm >> 15 & 0x1);
          if (imm & kImm16Mask) {
            count++;
          }
          return count;
        } else {
          if (is_int48(value)) {
            int64_t k = value >> 16;
            int count = InstrCountForLiLower32Bit(k) + 1;
            if (value & kImm16Mask) {
              count++;
            }
            return count;
          } else {
            int64_t k = value >> 32;
            int count = InstrCountForLiLower32Bit(k);
            if ((value >> 16) & kImm16Mask) {
              count += 3;
              if (value & kImm16Mask) {
                count++;
              }
            } else {
              count++;
              if (value & kImm16Mask) {
                count++;
              }
            }
            return count;
          }
        }
      }
    }
  }
  UNREACHABLE();
  return INT_MAX;
}

// All changes to if...else conditions here must be added to
// InstrCountForLi64Bit as well.
void TurboAssembler::li_optimized(Register rd, Operand j, LiFlags mode) {
  DCHECK(!j.is_reg());
  DCHECK(!MustUseReg(j.rmode()));
  DCHECK(mode == OPTIMIZE_SIZE);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Normal load of an immediate value which does not need Relocation Info.
  if (is_int32(j.immediate())) {
    LiLower32BitHelper(rd, j);
  } else {
    int bit31 = j.immediate() >> 31 & 0x1;
    if ((j.immediate() & kUpper16MaskOf64) == 0 &&
        is_int16(j.immediate() >> 32) && kArchVariant == kMips64r6) {
      // 64-bit value which consists of an unsigned 16-bit value in its
      // least significant 32-bits, and a signed 16-bit value in its
      // most significant 32-bits.
      ori(rd, zero_reg, j.immediate() & kImm16Mask);
      dahi(rd, j.immediate() >> 32 & kImm16Mask);
    } else if ((j.immediate() & (kHigher16MaskOf64 | kUpper16MaskOf64)) == 0 &&
               kArchVariant == kMips64r6) {
      // 64-bit value which consists of an unsigned 16-bit value in its
      // least significant 48-bits, and a signed 16-bit value in its
      // most significant 16-bits.
      ori(rd, zero_reg, j.immediate() & kImm16Mask);
      dati(rd, j.immediate() >> 48 & kImm16Mask);
    } else if ((j.immediate() & kImm16Mask) == 0 &&
               is_int16((j.immediate() >> 32) + bit31) &&
               kArchVariant == kMips64r6) {
      // 16 LSBs (Least Significant Bits) all set to zero.
      // 48 MSBs (Most Significant Bits) hold a signed 32-bit value.
      lui(rd, j.immediate() >> kLuiShift & kImm16Mask);
      dahi(rd, ((j.immediate() >> 32) + bit31) & kImm16Mask);
    } else if ((j.immediate() & kImm16Mask) == 0 &&
               ((j.immediate() >> 31) & 0x1FFFF) ==
                   ((0x20000 - bit31) & 0x1FFFF) &&
               kArchVariant == kMips64r6) {
      // 16 LSBs all set to zero.
      // 48 MSBs hold a signed value which can't be represented by signed
      // 32-bit number, and the middle 16 bits are all zero, or all one.
      lui(rd, j.immediate() >> kLuiShift & kImm16Mask);
      dati(rd, ((j.immediate() >> 48) + bit31) & kImm16Mask);
    } else if (is_int16(static_cast<int32_t>(j.immediate())) &&
               is_int16((j.immediate() >> 32) + bit31) &&
               kArchVariant == kMips64r6) {
      // 32 LSBs contain a signed 16-bit number.
      // 32 MSBs contain a signed 16-bit number.
      daddiu(rd, zero_reg, j.immediate() & kImm16Mask);
      dahi(rd, ((j.immediate() >> 32) + bit31) & kImm16Mask);
    } else if (is_int16(static_cast<int32_t>(j.immediate())) &&
               ((j.immediate() >> 31) & 0x1FFFF) ==
                   ((0x20000 - bit31) & 0x1FFFF) &&
               kArchVariant == kMips64r6) {
      // 48 LSBs contain an unsigned 16-bit number.
      // 16 MSBs contain a signed 16-bit number.
      daddiu(rd, zero_reg, j.immediate() & kImm16Mask);
      dati(rd, ((j.immediate() >> 48) + bit31) & kImm16Mask);
    } else if (base::bits::IsPowerOfTwo(j.immediate() + 1) ||
               j.immediate() == std::numeric_limits<int64_t>::max()) {
      // 64-bit values which have their "n" LSBs set to one, and their
      // "64-n" MSBs set to zero. "n" must meet the restrictions 0 < n < 64.
      int shift_cnt = 64 - base::bits::CountTrailingZeros64(j.immediate() + 1);
      daddiu(rd, zero_reg, -1);
      if (shift_cnt < 32) {
        dsrl(rd, rd, shift_cnt);
      } else {
        dsrl32(rd, rd, shift_cnt & 31);
      }
    } else {
      int shift_cnt = base::bits::CountTrailingZeros64(j.immediate());
      int rep32_count = InstrCountForLoadReplicatedConst32(j.immediate());
      int64_t tmp = j.immediate() >> shift_cnt;
      if (is_uint16(tmp)) {
        // Value can be computed by loading a 16-bit unsigned value, and
        // then shifting left.
        ori(rd, zero_reg, tmp & kImm16Mask);
        if (shift_cnt < 32) {
          dsll(rd, rd, shift_cnt);
        } else {
          dsll32(rd, rd, shift_cnt & 31);
        }
      } else if (is_int16(tmp)) {
        // Value can be computed by loading a 16-bit signed value, and
        // then shifting left.
        daddiu(rd, zero_reg, static_cast<int32_t>(tmp));
        if (shift_cnt < 32) {
          dsll(rd, rd, shift_cnt);
        } else {
          dsll32(rd, rd, shift_cnt & 31);
        }
      } else if (rep32_count < 3) {
        // Value being loaded has 32 LSBs equal to the 32 MSBs, and the
        // value loaded into the 32 LSBs can be loaded with a single
        // MIPS instruction.
        LiLower32BitHelper(rd, j);
        Dins(rd, rd, 32, 32);
      } else if (is_int32(tmp)) {
        // Loads with 3 instructions.
        // Value can be computed by loading a 32-bit signed value, and
        // then shifting left.
        lui(rd, tmp >> kLuiShift & kImm16Mask);
        ori(rd, rd, tmp & kImm16Mask);
        if (shift_cnt < 32) {
          dsll(rd, rd, shift_cnt);
        } else {
          dsll32(rd, rd, shift_cnt & 31);
        }
      } else {
        shift_cnt = 16 + base::bits::CountTrailingZeros64(j.immediate() >> 16);
        tmp = j.immediate() >> shift_cnt;
        if (is_uint16(tmp)) {
          // Value can be computed by loading a 16-bit unsigned value,
          // shifting left, and "or"ing in another 16-bit unsigned value.
          ori(rd, zero_reg, tmp & kImm16Mask);
          if (shift_cnt < 32) {
            dsll(rd, rd, shift_cnt);
          } else {
            dsll32(rd, rd, shift_cnt & 31);
          }
          ori(rd, rd, j.immediate() & kImm16Mask);
        } else if (is_int16(tmp)) {
          // Value can be computed by loading a 16-bit signed value,
          // shifting left, and "or"ing in a 16-bit unsigned value.
          daddiu(rd, zero_reg, static_cast<int32_t>(tmp));
          if (shift_cnt < 32) {
            dsll(rd, rd, shift_cnt);
          } else {
            dsll32(rd, rd, shift_cnt & 31);
          }
          ori(rd, rd, j.immediate() & kImm16Mask);
        } else if (rep32_count < 4) {
          // Value being loaded has 32 LSBs equal to the 32 MSBs, and the
          // value in the 32 LSBs requires 2 MIPS instructions to load.
          LiLower32BitHelper(rd, j);
          Dins(rd, rd, 32, 32);
        } else if (kArchVariant == kMips64r6) {
          // Loads with 3-4 instructions.
          // Catch-all case to get any other 64-bit values which aren't
          // handled by special cases above.
          int64_t imm = j.immediate();
          LiLower32BitHelper(rd, j);
          imm = (imm >> 32) + bit31;
          if (imm & kImm16Mask) {
            dahi(rd, imm & kImm16Mask);
          }
          imm = (imm >> 16) + (imm >> 15 & 0x1);
          if (imm & kImm16Mask) {
            dati(rd, imm & kImm16Mask);
          }
        } else {
          if (is_int48(j.immediate())) {
            Operand k = Operand(j.immediate() >> 16);
            LiLower32BitHelper(rd, k);
            dsll(rd, rd, 16);
            if (j.immediate() & kImm16Mask) {
              ori(rd, rd, j.immediate() & kImm16Mask);
            }
          } else {
            Operand k = Operand(j.immediate() >> 32);
            LiLower32BitHelper(rd, k);
            if ((j.immediate() >> 16) & kImm16Mask) {
              dsll(rd, rd, 16);
              ori(rd, rd, (j.immediate() >> 16) & kImm16Mask);
              dsll(rd, rd, 16);
              if (j.immediate() & kImm16Mask) {
                ori(rd, rd, j.immediate() & kImm16Mask);
              }
            } else {
              dsll32(rd, rd, 0);
              if (j.immediate() & kImm16Mask) {
                ori(rd, rd, j.immediate() & kImm16Mask);
              }
            }
          }
        }
      }
    }
  }
}

void TurboAssembler::li(Register rd, Operand j, LiFlags mode) {
  DCHECK(!j.is_reg());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (!MustUseReg(j.rmode()) && mode == OPTIMIZE_SIZE) {
    int li_count = InstrCountForLi64Bit(j.immediate());
    int li_neg_count = InstrCountForLi64Bit(-j.immediate());
    int li_not_count = InstrCountForLi64Bit(~j.immediate());
    // Loading -MIN_INT64 could cause problems, but loading MIN_INT64 takes only
    // two instructions so no need to check for this.
    if (li_neg_count <= li_not_count && li_neg_count < li_count - 1) {
      DCHECK(j.immediate() != std::numeric_limits<int64_t>::min());
      li_optimized(rd, Operand(-j.immediate()), mode);
      Dsubu(rd, zero_reg, rd);
    } else if (li_neg_count > li_not_count && li_not_count < li_count - 1) {
      DCHECK(j.immediate() != std::numeric_limits<int64_t>::min());
      li_optimized(rd, Operand(~j.immediate()), mode);
      nor(rd, rd, rd);
    } else {
      li_optimized(rd, j, mode);
    }
  } else if (MustUseReg(j.rmode())) {
    int64_t immediate;
    if (j.IsHeapObjectRequest()) {
      RequestHeapObject(j.heap_object_request());
      immediate = 0;
    } else {
      immediate = j.immediate();
    }

    RecordRelocInfo(j.rmode(), immediate);
    lui(rd, (immediate >> 32) & kImm16Mask);
    ori(rd, rd, (immediate >> 16) & kImm16Mask);
    dsll(rd, rd, 16);
    ori(rd, rd, immediate & kImm16Mask);
  } else if (mode == ADDRESS_LOAD) {
    // We always need the same number of instructions as we may need to patch
    // this code to load another value which may need all 4 instructions.
    lui(rd, (j.immediate() >> 32) & kImm16Mask);
    ori(rd, rd, (j.immediate() >> 16) & kImm16Mask);
    dsll(rd, rd, 16);
    ori(rd, rd, j.immediate() & kImm16Mask);
  } else {  // mode == CONSTANT_SIZE - always emit the same instruction
            // sequence.
    if (kArchVariant == kMips64r6) {
      int64_t imm = j.immediate();
      lui(rd, imm >> kLuiShift & kImm16Mask);
      ori(rd, rd, (imm & kImm16Mask));
      imm = (imm >> 32) + ((imm >> 31) & 0x1);
      dahi(rd, imm & kImm16Mask & kImm16Mask);
      imm = (imm >> 16) + ((imm >> 15) & 0x1);
      dati(rd, imm & kImm16Mask & kImm16Mask);
    } else {
      lui(rd, (j.immediate() >> 48) & kImm16Mask);
      ori(rd, rd, (j.immediate() >> 32) & kImm16Mask);
      dsll(rd, rd, 16);
      ori(rd, rd, (j.immediate() >> 16) & kImm16Mask);
      dsll(rd, rd, 16);
      ori(rd, rd, j.immediate() & kImm16Mask);
    }
  }
}

void TurboAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  Dsubu(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      Sd(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}

void TurboAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      Ld(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  daddiu(sp, sp, stack_offset);
}

void TurboAssembler::MultiPushFPU(RegList regs) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  Dsubu(sp, sp, Operand(stack_offset));
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
  daddiu(sp, sp, stack_offset);
}

void TurboAssembler::Ext(Register rt, Register rs, uint16_t pos,
                         uint16_t size) {
  DCHECK_LT(pos, 32);
  DCHECK_LT(pos + size, 33);
  ext_(rt, rs, pos, size);
}

void TurboAssembler::Dext(Register rt, Register rs, uint16_t pos,
                          uint16_t size) {
  DCHECK(pos < 64 && 0 < size && size <= 64 && 0 < pos + size &&
         pos + size <= 64);
  if (size > 32) {
    dextm_(rt, rs, pos, size);
  } else if (pos >= 32) {
    dextu_(rt, rs, pos, size);
  } else {
    dext_(rt, rs, pos, size);
  }
}

void TurboAssembler::Ins(Register rt, Register rs, uint16_t pos,
                         uint16_t size) {
  DCHECK_LT(pos, 32);
  DCHECK_LE(pos + size, 32);
  DCHECK_NE(size, 0);
  ins_(rt, rs, pos, size);
}

void TurboAssembler::Dins(Register rt, Register rs, uint16_t pos,
                          uint16_t size) {
  DCHECK(pos < 64 && 0 < size && size <= 64 && 0 < pos + size &&
         pos + size <= 64);
  if (pos + size <= 32) {
    dins_(rt, rs, pos, size);
  } else if (pos < 32) {
    dinsm_(rt, rs, pos, size);
  } else {
    dinsu_(rt, rs, pos, size);
  }
}

void TurboAssembler::ExtractBits(Register dest, Register source, Register pos,
                                 int size, bool sign_extend) {
  dsrav(dest, source, pos);
  Dext(dest, dest, 0, size);
  if (sign_extend) {
    switch (size) {
      case 8:
        seb(dest, dest);
        break;
      case 16:
        seh(dest, dest);
        break;
      case 32:
        // sign-extend word
        sll(dest, dest, 0);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void TurboAssembler::InsertBits(Register dest, Register source, Register pos,
                                int size) {
  Dror(dest, dest, pos);
  Dins(dest, source, 0, size);
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Dsubu(scratch, zero_reg, pos);
    Dror(dest, dest, scratch);
  }
}

void TurboAssembler::Neg_s(FPURegister fd, FPURegister fs) {
  if (kArchVariant == kMips64r6) {
    // r6 neg_s changes the sign for NaN-like operands as well.
    neg_s(fd, fs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
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
  if (kArchVariant == kMips64r6) {
    // r6 neg_d changes the sign for NaN-like operands as well.
    neg_d(fd, fs);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
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
    dmfc1(scratch1, fs);
    li(scratch2, Double::kSignMask);
    Xor(scratch1, scratch1, scratch2);
    dmtc1(scratch1, fd);
    bind(&done);
  }
}

void TurboAssembler::Cvt_d_uw(FPURegister fd, FPURegister fs) {
  // Move the data from fs to t8.
  BlockTrampolinePoolScope block_trampoline_pool(this);
  mfc1(t8, fs);
  Cvt_d_uw(fd, t8);
}

void TurboAssembler::Cvt_d_uw(FPURegister fd, Register rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);

  // Convert rs to a FP value in fd.
  DCHECK(rs != t9);
  DCHECK(rs != at);

  // Zero extend int32 in rs.
  Dext(t9, rs, 0, 32);
  dmtc1(t9, fd);
  cvt_d_l(fd, fd);
}

void TurboAssembler::Cvt_d_ul(FPURegister fd, FPURegister fs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Move the data from fs to t8.
  dmfc1(t8, fs);
  Cvt_d_ul(fd, t8);
}

void TurboAssembler::Cvt_d_ul(FPURegister fd, Register rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Convert rs to a FP value in fd.

  DCHECK(rs != t9);
  DCHECK(rs != at);

  Label msb_clear, conversion_done;

  Branch(&msb_clear, ge, rs, Operand(zero_reg));

  // Rs >= 2^63
  andi(t9, rs, 1);
  dsrl(rs, rs, 1);
  or_(t9, t9, rs);
  dmtc1(t9, fd);
  cvt_d_l(fd, fd);
  Branch(USE_DELAY_SLOT, &conversion_done);
  add_d(fd, fd, fd);  // In delay slot.

  bind(&msb_clear);
  // Rs < 2^63, we can do simple conversion.
  dmtc1(rs, fd);
  cvt_d_l(fd, fd);

  bind(&conversion_done);
}

void TurboAssembler::Cvt_s_uw(FPURegister fd, FPURegister fs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Move the data from fs to t8.
  mfc1(t8, fs);
  Cvt_s_uw(fd, t8);
}

void TurboAssembler::Cvt_s_uw(FPURegister fd, Register rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Convert rs to a FP value in fd.
  DCHECK(rs != t9);
  DCHECK(rs != at);

  // Zero extend int32 in rs.
  Dext(t9, rs, 0, 32);
  dmtc1(t9, fd);
  cvt_s_l(fd, fd);
}

void TurboAssembler::Cvt_s_ul(FPURegister fd, FPURegister fs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Move the data from fs to t8.
  dmfc1(t8, fs);
  Cvt_s_ul(fd, t8);
}

void TurboAssembler::Cvt_s_ul(FPURegister fd, Register rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Convert rs to a FP value in fd.

  DCHECK(rs != t9);
  DCHECK(rs != at);

  Label positive, conversion_done;

  Branch(&positive, ge, rs, Operand(zero_reg));

  // Rs >= 2^31.
  andi(t9, rs, 1);
  dsrl(rs, rs, 1);
  or_(t9, t9, rs);
  dmtc1(t9, fd);
  cvt_s_l(fd, fd);
  Branch(USE_DELAY_SLOT, &conversion_done);
  add_s(fd, fd, fd);  // In delay slot.

  bind(&positive);
  // Rs < 2^31, we can do simple conversion.
  dmtc1(rs, fd);
  cvt_s_l(fd, fd);

  bind(&conversion_done);
}

void MacroAssembler::Round_l_d(FPURegister fd, FPURegister fs) {
  round_l_d(fd, fs);
}

void MacroAssembler::Floor_l_d(FPURegister fd, FPURegister fs) {
  floor_l_d(fd, fs);
}

void MacroAssembler::Ceil_l_d(FPURegister fd, FPURegister fs) {
  ceil_l_d(fd, fs);
}

void MacroAssembler::Trunc_l_d(FPURegister fd, FPURegister fs) {
  trunc_l_d(fd, fs);
}

void MacroAssembler::Trunc_l_ud(FPURegister fd, FPURegister fs,
                                FPURegister scratch) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Load to GPR.
  dmfc1(t8, fs);
  // Reset sign bit.
  {
    UseScratchRegisterScope temps(this);
    Register scratch1 = temps.Acquire();
    li(scratch1, 0x7FFFFFFFFFFFFFFF);
    and_(t8, t8, scratch1);
  }
  dmtc1(t8, fs);
  trunc_l_d(fd, fs);
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

void TurboAssembler::Trunc_ul_d(FPURegister fd, FPURegister fs,
                                FPURegister scratch, Register result) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Trunc_ul_d(t8, fs, scratch, result);
  dmtc1(t8, fd);
}

void TurboAssembler::Trunc_ul_s(FPURegister fd, FPURegister fs,
                                FPURegister scratch, Register result) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Trunc_ul_s(t8, fs, scratch, result);
  dmtc1(t8, fd);
}

void MacroAssembler::Trunc_w_d(FPURegister fd, FPURegister fs) {
  trunc_w_d(fd, fs);
}

void MacroAssembler::Round_w_d(FPURegister fd, FPURegister fs) {
  round_w_d(fd, fs);
}

void MacroAssembler::Floor_w_d(FPURegister fd, FPURegister fs) {
  floor_w_d(fd, fs);
}

void MacroAssembler::Ceil_w_d(FPURegister fd, FPURegister fs) {
  ceil_w_d(fd, fs);
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
    mthc1(scratch1, scratch);
  }
  // Test if scratch > fd.
  // If fd < 2^31 we can convert it normally.
  Label simple_convert;
  CompareF64(OLT, fs, scratch);
  BranchTrueShortF(&simple_convert);

  // First we subtract 2^31 from fd, then trunc it to rs
  // and add 2^31 to rs.
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

void TurboAssembler::Trunc_ul_d(Register rd, FPURegister fs,
                                FPURegister scratch, Register result) {
  DCHECK(fs != scratch);
  DCHECK(result.is_valid() ? !AreAliased(rd, result, at) : !AreAliased(rd, at));

  Label simple_convert, done, fail;
  if (result.is_valid()) {
    mov(result, zero_reg);
    Move(scratch, -1.0);
    // If fd =< -1 or unordered, then the conversion fails.
    CompareF64(OLE, fs, scratch);
    BranchTrueShortF(&fail);
    CompareIsNanF64(fs, scratch);
    BranchTrueShortF(&fail);
  }

  // Load 2^63 into scratch as its double representation.
  li(at, 0x43E0000000000000);
  dmtc1(at, scratch);

  // Test if scratch > fs.
  // If fs < 2^63 we can convert it normally.
  CompareF64(OLT, fs, scratch);
  BranchTrueShortF(&simple_convert);

  // First we subtract 2^63 from fs, then trunc it to rd
  // and add 2^63 to rd.
  sub_d(scratch, fs, scratch);
  trunc_l_d(scratch, scratch);
  dmfc1(rd, scratch);
  Or(rd, rd, Operand(1UL << 63));
  Branch(&done);

  // Simple conversion.
  bind(&simple_convert);
  trunc_l_d(scratch, fs);
  dmfc1(rd, scratch);

  bind(&done);
  if (result.is_valid()) {
    // Conversion is failed if the result is negative.
    {
      UseScratchRegisterScope temps(this);
      Register scratch1 = temps.Acquire();
      addiu(scratch1, zero_reg, -1);
      dsrl(scratch1, scratch1, 1);  // Load 2^62.
      dmfc1(result, scratch);
      xor_(result, result, scratch1);
    }
    Slt(result, zero_reg, result);
  }

  bind(&fail);
}

void TurboAssembler::Trunc_ul_s(Register rd, FPURegister fs,
                                FPURegister scratch, Register result) {
  DCHECK(fs != scratch);
  DCHECK(result.is_valid() ? !AreAliased(rd, result, at) : !AreAliased(rd, at));

  Label simple_convert, done, fail;
  if (result.is_valid()) {
    mov(result, zero_reg);
    Move(scratch, -1.0f);
    // If fd =< -1 or unordered, then the conversion fails.
    CompareF32(OLE, fs, scratch);
    BranchTrueShortF(&fail);
    CompareIsNanF32(fs, scratch);
    BranchTrueShortF(&fail);
  }

  {
    // Load 2^63 into scratch as its float representation.
    UseScratchRegisterScope temps(this);
    Register scratch1 = temps.Acquire();
    li(scratch1, 0x5F000000);
    mtc1(scratch1, scratch);
  }

  // Test if scratch > fs.
  // If fs < 2^63 we can convert it normally.
  CompareF32(OLT, fs, scratch);
  BranchTrueShortF(&simple_convert);

  // First we subtract 2^63 from fs, then trunc it to rd
  // and add 2^63 to rd.
  sub_s(scratch, fs, scratch);
  trunc_l_s(scratch, scratch);
  dmfc1(rd, scratch);
  Or(rd, rd, Operand(1UL << 63));
  Branch(&done);

  // Simple conversion.
  bind(&simple_convert);
  trunc_l_s(scratch, fs);
  dmfc1(rd, scratch);

  bind(&done);
  if (result.is_valid()) {
    // Conversion is failed if the result is negative or unordered.
    {
      UseScratchRegisterScope temps(this);
      Register scratch1 = temps.Acquire();
      addiu(scratch1, zero_reg, -1);
      dsrl(scratch1, scratch1, 1);  // Load 2^62.
      dmfc1(result, scratch);
      xor_(result, result, scratch1);
    }
    Slt(result, zero_reg, result);
  }

  bind(&fail);
}

template <typename RoundFunc>
void TurboAssembler::RoundDouble(FPURegister dst, FPURegister src,
                                 FPURoundingMode mode, RoundFunc round) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = t8;
  if (kArchVariant == kMips64r6) {
    cfc1(scratch, FCSR);
    li(at, Operand(mode));
    ctc1(at, FCSR);
    rint_d(dst, src);
    ctc1(scratch, FCSR);
  } else {
    Label done;
    mfhc1(scratch, src);
    Ext(at, scratch, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
    Branch(USE_DELAY_SLOT, &done, hs, at,
           Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits));
    mov_d(dst, src);
    round(this, dst, src);
    dmfc1(at, dst);
    Branch(USE_DELAY_SLOT, &done, ne, at, Operand(zero_reg));
    cvt_d_l(dst, dst);
    srl(at, scratch, 31);
    sll(at, at, 31);
    mthc1(at, dst);
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
  if (kArchVariant == kMips64r6) {
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

void MacroAssembler::Madd_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  DCHECK(fr != scratch && fs != scratch && ft != scratch);
  mul_s(scratch, fs, ft);
  add_s(fd, fr, scratch);
}

void MacroAssembler::Madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  DCHECK(fr != scratch && fs != scratch && ft != scratch);
  mul_d(scratch, fs, ft);
  add_d(fd, fr, scratch);
}

void MacroAssembler::Msub_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  DCHECK(fr != scratch && fs != scratch && ft != scratch);
  mul_s(scratch, fs, ft);
  sub_s(fd, scratch, fr);
}

void MacroAssembler::Msub_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft, FPURegister scratch) {
  DCHECK(fr != scratch && fs != scratch && ft != scratch);
  mul_d(scratch, fs, ft);
  sub_d(fd, scratch, fr);
}

void TurboAssembler::CompareF(SecondaryField sizeField, FPUCondition cc,
                              FPURegister cmp1, FPURegister cmp2) {
  if (kArchVariant == kMips64r6) {
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
  if (kArchVariant == kMips64r6) {
    bc1nez(target, kDoubleCompareReg);
  } else {
    bc1t(target);
  }
  if (bd == PROTECT) {
    nop();
  }
}

void TurboAssembler::BranchFalseShortF(Label* target, BranchDelaySlot bd) {
  if (kArchVariant == kMips64r6) {
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
  if (kArchVariant == kMips64r6) {
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
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  DCHECK(src_low != scratch);
  mfhc1(scratch, dst);
  mtc1(src_low, dst);
  mthc1(scratch, dst);
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
      mthc1(scratch, dst);
    } else {
      mthc1(zero_reg, dst);
    }
    if (dst == kDoubleRegZero) has_double_zero_reg_set_ = true;
  }
}

void TurboAssembler::Movz(Register rd, Register rs, Register rt) {
  if (kArchVariant == kMips64r6) {
    Label done;
    Branch(&done, ne, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movz(rd, rs, rt);
  }
}

void TurboAssembler::Movn(Register rd, Register rs, Register rt) {
  if (kArchVariant == kMips64r6) {
    Label done;
    Branch(&done, eq, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movn(rd, rs, rt);
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
        Dsubu(t9, rs, rt);
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
        Dsubu(t9, rs, rt);
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
  if (kArchVariant == kMips64r6) {
    seleqz(dest, dest, condition);
  } else {
    Movn(dest, zero_reg, condition);
  }
}

void TurboAssembler::LoadZeroIfConditionZero(Register dest,
                                             Register condition) {
  if (kArchVariant == kMips64r6) {
    selnez(dest, dest, condition);
  } else {
    Movz(dest, zero_reg, condition);
  }
}

void TurboAssembler::LoadZeroIfFPUCondition(Register dest) {
  if (kArchVariant == kMips64r6) {
    dmfc1(kScratchReg, kDoubleCompareReg);
    LoadZeroIfConditionNotZero(dest, kScratchReg);
  } else {
    Movt(dest, zero_reg);
  }
}

void TurboAssembler::LoadZeroIfNotFPUCondition(Register dest) {
  if (kArchVariant == kMips64r6) {
    dmfc1(kScratchReg, kDoubleCompareReg);
    LoadZeroIfConditionZero(dest, kScratchReg);
  } else {
    Movf(dest, zero_reg);
  }
}

void TurboAssembler::Movt(Register rd, Register rs, uint16_t cc) {
  movt(rd, rs, cc);
}

void TurboAssembler::Movf(Register rd, Register rs, uint16_t cc) {
  movf(rd, rs, cc);
}

void TurboAssembler::Clz(Register rd, Register rs) { clz(rd, rs); }

void TurboAssembler::Ctz(Register rd, Register rs) {
  if (kArchVariant == kMips64r6) {
    // We don't have an instruction to count the number of trailing zeroes.
    // Start by flipping the bits end-for-end so we can count the number of
    // leading zeroes instead.
    rotr(rd, rs, 16);
    wsbh(rd, rd);
    bitswap(rd, rd);
    Clz(rd, rd);
  } else {
    // Convert trailing zeroes to trailing ones, and bits to their left
    // to zeroes.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Daddu(scratch, rs, -1);
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

void TurboAssembler::Dctz(Register rd, Register rs) {
  if (kArchVariant == kMips64r6) {
    // We don't have an instruction to count the number of trailing zeroes.
    // Start by flipping the bits end-for-end so we can count the number of
    // leading zeroes instead.
    dsbh(rd, rs);
    dshd(rd, rd);
    dbitswap(rd, rd);
    dclz(rd, rd);
  } else {
    // Convert trailing zeroes to trailing ones, and bits to their left
    // to zeroes.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Daddu(scratch, rs, -1);
    Xor(rd, scratch, rs);
    And(rd, rd, scratch);
    // Count number of leading zeroes.
    dclz(rd, rd);
    // Subtract number of leading zeroes from 64 to get number of trailing
    // ones. Remember that the trailing ones were formerly trailing zeroes.
    li(scratch, 64);
    Dsubu(rd, scratch, rd);
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
  // For a 64-bit operand this can be performed in 24 instructions compared
  // to a(n unrolled) loop based algorithm which requires 38 instructions.
  //
  // There are algorithms which are faster in the cases where very few
  // bits are set but the algorithm here attempts to minimize the total
  // number of instructions executed even when a large number of bits
  // are set.
  uint32_t B0 = 0x55555555;     // (T)~(T)0/3
  uint32_t B1 = 0x33333333;     // (T)~(T)0/15*3
  uint32_t B2 = 0x0F0F0F0F;     // (T)~(T)0/255*15
  uint32_t value = 0x01010101;  // (T)~(T)0/255
  uint32_t shift = 24;          // (sizeof(T) - 1) * BITS_PER_BYTE

  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
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

void TurboAssembler::Dpopcnt(Register rd, Register rs) {
  uint64_t B0 = 0x5555555555555555l;     // (T)~(T)0/3
  uint64_t B1 = 0x3333333333333333l;     // (T)~(T)0/15*3
  uint64_t B2 = 0x0F0F0F0F0F0F0F0Fl;     // (T)~(T)0/255*15
  uint64_t value = 0x0101010101010101l;  // (T)~(T)0/255
  uint64_t shift = 24;                   // (sizeof(T) - 1) * BITS_PER_BYTE

  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = temps.Acquire();
  Register scratch2 = t8;
  dsrl(scratch, rs, 1);
  li(scratch2, B0);
  And(scratch, scratch, scratch2);
  Dsubu(scratch, rs, scratch);
  li(scratch2, B1);
  And(rd, scratch, scratch2);
  dsrl(scratch, scratch, 2);
  And(scratch, scratch, scratch2);
  Daddu(scratch, rd, scratch);
  dsrl(rd, scratch, 4);
  Daddu(rd, rd, scratch);
  li(scratch2, B2);
  And(rd, rd, scratch2);
  li(scratch, value);
  Dmul(rd, rd, scratch);
  dsrl32(rd, rd, shift);
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
  DoubleRegister single_scratch = kScratchDoubleReg.low();
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
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
  Dsubu(sp, sp, Operand(kDoubleSize));  // Put input on stack.
  Sdc1(double_input, MemOperand(sp, 0));

  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }
  Ld(result, MemOperand(sp, 0));

  Daddu(sp, sp, Operand(kDoubleSize));
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
  DCHECK_EQ(kArchVariant, kMips64r6 ? is_int26(offset) : is_int16(offset));
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
  if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
    DCHECK(is_int26(offset));
    BranchShortHelperR6(offset, nullptr);
  } else {
    DCHECK(is_int16(offset));
    BranchShortHelper(offset, nullptr, bdslot);
  }
}

void TurboAssembler::BranchShort(Label* L, BranchDelaySlot bdslot) {
  if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
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

bool TurboAssembler::CalculateOffset(Label* L, int32_t& offset,
                                     OffsetSize bits) {
  if (!is_near(L, bits)) return false;
  offset = GetOffset(offset, L, bits);
  return true;
}

bool TurboAssembler::CalculateOffset(Label* L, int32_t& offset, OffsetSize bits,
                                     Register& scratch, const Operand& rt) {
  if (!is_near(L, bits)) return false;
  scratch = GetRtAsRegisterHelper(rt, scratch);
  offset = GetOffset(offset, L, bits);
  return true;
}

bool TurboAssembler::BranchShortHelperR6(int32_t offset, Label* L,
                                         Condition cond, Register rs,
                                         const Operand& rt) {
  DCHECK(L == nullptr || offset == 0);
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = temps.hasAvailable() ? temps.Acquire() : t8;

  // Be careful to always use shifted_branch_offset only just before the
  // branch instruction, as the location will be remember for patching the
  // target.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    switch (cond) {
      case cc_always:
        if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
        bc(offset);
        break;
      case eq:
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          // Pre R6 beq is used here to make the code patchable. Otherwise bc
          // should be used which has no condition field so is not patchable.
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          beq(rs, scratch, offset);
          nop();
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21)) return false;
          beqzc(rs, offset);
        } else {
          // We don't want any other register but scratch clobbered.
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          beqc(rs, scratch, offset);
        }
        break;
      case ne:
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          // Pre R6 bne is used here to make the code patchable. Otherwise we
          // should not generate any instruction.
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          bne(rs, scratch, offset);
          nop();
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21)) return false;
          bnezc(rs, offset);
        } else {
          // We don't want any other register but scratch clobbered.
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
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
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          bltzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
          bgtzc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltc(scratch, rs, offset);
        }
        break;
      case greater_equal:
        // rs >= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          blezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
          bgezc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
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
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          bgtzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
          bltzc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltc(rs, scratch, offset);
        }
        break;
      case less_equal:
        // rs <= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          bgezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
          blezc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
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
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21, scratch, rt))
            return false;
          bnezc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21)) return false;
          bnezc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltuc(scratch, rs, offset);
        }
        break;
      case Ugreater_equal:
        // rs >= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21, scratch, rt))
            return false;
          beqzc(scratch, offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
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
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21, scratch, rt))
            return false;
          bnezc(scratch, offset);
        } else if (IsZero(rt)) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
            return false;
          DCHECK(rs != scratch);
          bltuc(rs, scratch, offset);
        }
        break;
      case Uless_equal:
        // rs <= rt
        if (rt.is_reg() && rs.code() == rt.rm().code()) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
          bc(offset);
        } else if (rs == zero_reg) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset26, scratch, rt))
            return false;
          bc(offset);
        } else if (IsZero(rt)) {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset21)) return false;
          beqzc(rs, offset);
        } else {
          if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
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
  BlockTrampolinePoolScope block_trampoline_pool(this);
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
    if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
      DCHECK(is_int26(offset));
      return BranchShortHelperR6(offset, nullptr, cond, rs, rt);
    } else {
      DCHECK(is_int16(offset));
      return BranchShortHelper(offset, nullptr, cond, rs, rt, bdslot);
    }
  } else {
    DCHECK_EQ(offset, 0);
    if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
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
  if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
    DCHECK(is_int26(offset));
    BranchAndLinkShortHelperR6(offset, nullptr);
  } else {
    DCHECK(is_int16(offset));
    BranchAndLinkShortHelper(offset, nullptr, bdslot);
  }
}

void TurboAssembler::BranchAndLinkShort(Label* L, BranchDelaySlot bdslot) {
  if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
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
      if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
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
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
          return false;
        bltzalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
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
        if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
        balc(offset);
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
          return false;
        blezalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
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
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
          return false;
        bgtzalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
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
        if (!CalculateOffset(L, offset, OffsetSize::kOffset26)) return false;
        balc(offset);
      } else if (rs == zero_reg) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16, scratch, rt))
          return false;
        bgezalc(scratch, offset);
      } else if (IsZero(rt)) {
        if (!CalculateOffset(L, offset, OffsetSize::kOffset16)) return false;
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
    if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
      DCHECK(is_int26(offset));
      return BranchAndLinkShortHelperR6(offset, nullptr, cond, rs, rt);
    } else {
      DCHECK(is_int16(offset));
      return BranchAndLinkShortHelper(offset, nullptr, cond, rs, rt, bdslot);
    }
  } else {
    DCHECK_EQ(offset, 0);
    if (kArchVariant == kMips64r6 && bdslot == PROTECT) {
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
  Ld(destination,
     FieldMemOperand(destination,
                     FixedArray::kHeaderSize + constant_index * kPointerSize));
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  Ld(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    Daddu(destination, kRootRegister, Operand(offset));
  }
}

void TurboAssembler::Jump(Register target, Condition cond, Register rs,
                          const Operand& rt, BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (kArchVariant == kMips64r6 && bd == PROTECT) {
    if (cond == cc_always) {
      jic(target, 0);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jic(target, 0);
    }
  } else {
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
  Label skip;
  if (cond != cc_always) {
    Branch(USE_DELAY_SLOT, &skip, NegateCondition(cond), rs, rt);
  }
  // The first instruction of 'li' may be placed in the delay slot.
  // This is not an issue, t9 is expected to be clobbered anyway.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    li(t9, Operand(target, rmode));
    Jump(t9, al, zero_reg, Operand(zero_reg), bd);
    bind(&skip);
  }
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
  if (FLAG_embedded_builtins) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadConstant(t9, code);
      Daddu(t9, t9, Operand(Code::kHeaderSize - kHeapObjectTag));
      Jump(t9, cond, rs, rt, bd);
      return;
    } else if (options().inline_offheap_trampolines) {
      int builtin_index = Builtins::kNoBuiltinId;
      if (isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
          Builtins::IsIsolateIndependent(builtin_index)) {
        // Inline the trampoline.
        RecordCommentForOffHeapTrampoline(builtin_index);
        CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
        EmbeddedData d = EmbeddedData::FromBlob();
        Address entry = d.InstructionStartOfBuiltin(builtin_index);
        li(t9, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
        Jump(t9, cond, rs, rt, bd);
        return;
      }
    }
  }
  Jump(static_cast<intptr_t>(code.address()), rmode, cond, rs, rt, bd);
}

// Note: To call gcc-compiled C code on mips, you must call through t9.
void TurboAssembler::Call(Register target, Condition cond, Register rs,
                          const Operand& rt, BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (kArchVariant == kMips64r6 && bd == PROTECT) {
    if (cond == cc_always) {
      jialc(target, 0);
    } else {
      BRANCH_ARGS_CHECK(cond, rs, rt);
      Branch(2, NegateCondition(cond), rs, rt);
      jialc(target, 0);
    }
  } else {
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

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  if (lower_limit != 0) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Dsubu(scratch, value, Operand(lower_limit));
    Branch(on_in_range, ls, scratch, Operand(higher_limit - lower_limit));
  } else {
    Branch(on_in_range, ls, value, Operand(higher_limit - lower_limit));
  }
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode, Condition cond,
                          Register rs, const Operand& rt, BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  li(t9, Operand(static_cast<int64_t>(target), rmode), ADDRESS_LOAD);
  Call(t9, cond, rs, rt, bd);
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadConstant(t9, code);
      Daddu(t9, t9, Operand(Code::kHeaderSize - kHeapObjectTag));
      Call(t9, cond, rs, rt, bd);
      return;
    } else if (options().inline_offheap_trampolines) {
      int builtin_index = Builtins::kNoBuiltinId;
      if (isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
          Builtins::IsIsolateIndependent(builtin_index)) {
        // Inline the trampoline.
        RecordCommentForOffHeapTrampoline(builtin_index);
        CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
        EmbeddedData d = EmbeddedData::FromBlob();
        Address entry = d.InstructionStartOfBuiltin(builtin_index);
        li(t9, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
        Call(t9, cond, rs, rt, bd);
        return;
      }
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  Call(code.address(), rmode, cond, rs, rt, bd);
}

void TurboAssembler::CallBuiltinPointer(Register builtin_pointer) {
  STATIC_ASSERT(kSystemPointerSize == 8);
  STATIC_ASSERT(kSmiShiftSize == 31);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_pointer register contains the builtin index as a Smi.
  SmiUntag(builtin_pointer, builtin_pointer);
  Dlsa(builtin_pointer, kRootRegister, builtin_pointer, kSystemPointerSizeLog2);
  Ld(builtin_pointer,
     MemOperand(builtin_pointer, IsolateData::builtin_entry_table_offset()));
  Call(builtin_pointer);
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  // Compute the return address in lr to return to after the jump below. The pc
  // is already at '+ 8' from the current instruction; but return is after three
  // instructions, so add another 4 to pc to get the return address.

  Assembler::BlockTrampolinePoolScope block_trampoline_pool(this);
  static constexpr int kNumInstructionsToJump = 4;
  Label find_ra;
  // Adjust the value in ra to point to the correct return location, 2nd
  // instruction past the real call into C code (the jalr(t9)), and push it.
  // This is the return address of the exit frame.
  if (kArchVariant >= kMips64r6) {
    addiupc(ra, kNumInstructionsToJump + 1);
  } else {
    // This no-op-and-link sequence saves PC + 8 in ra register on pre-r6 MIPS
    nal();  // nal has branch delay slot.
    Daddu(ra, ra, kNumInstructionsToJump * kInstrSize);
  }
  bind(&find_ra);

  // This spot was reserved in EnterExitFrame.
  Sd(ra, MemOperand(sp));
  // Stack space reservation moved to the branch delay slot below.
  // Stack is still aligned.

  // Call the C routine.
  mov(t9, target);  // Function pointer to t9 to conform to ABI for PIC.
  jalr(t9);
  // Set up sp in the delay slot.
  daddiu(sp, sp, -kCArgsSlotsSize);
  // Make sure the stored 'ra' points to this position.
  DCHECK_EQ(kNumInstructionsToJump, InstructionsGeneratedSince(&find_ra));
}

void TurboAssembler::Ret(Condition cond, Register rs, const Operand& rt,
                         BranchDelaySlot bd) {
  Jump(ra, cond, rs, rt, bd);
}

void TurboAssembler::BranchLong(Label* L, BranchDelaySlot bdslot) {
  if (kArchVariant == kMips64r6 && bdslot == PROTECT &&
      (!L->is_bound() || is_near_r6(L))) {
    BranchShortHelperR6(0, L);
  } else {
    // Generate position independent long branch.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    int64_t imm64;
    imm64 = branch_long_offset(L);
    DCHECK(is_int32(imm64));
    or_(t8, ra, zero_reg);
    nal();                                        // Read PC into ra register.
    lui(t9, (imm64 & kHiMaskOf32) >> kLuiShift);  // Branch delay slot.
    ori(t9, t9, (imm64 & kImm16Mask));
    daddu(t9, ra, t9);
    if (bdslot == USE_DELAY_SLOT) {
      or_(ra, t8, zero_reg);
    }
    jr(t9);
    // Emit a or_ in the branch delay slot if it's protected.
    if (bdslot == PROTECT) or_(ra, t8, zero_reg);
  }
}

void TurboAssembler::BranchAndLinkLong(Label* L, BranchDelaySlot bdslot) {
  if (kArchVariant == kMips64r6 && bdslot == PROTECT &&
      (!L->is_bound() || is_near_r6(L))) {
    BranchAndLinkShortHelperR6(0, L);
  } else {
    // Generate position independent long branch and link.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    int64_t imm64;
    imm64 = branch_long_offset(L);
    DCHECK(is_int32(imm64));
    lui(t8, (imm64 & kHiMaskOf32) >> kLuiShift);
    nal();                              // Read PC into ra register.
    ori(t8, t8, (imm64 & kImm16Mask));  // Branch delay slot.
    daddu(t8, ra, t8);
    jalr(t8);
    // Emit a nop in the branch delay slot if required.
    if (bdslot == PROTECT) nop();
  }
}

void TurboAssembler::DropAndRet(int drop) {
  DCHECK(is_int16(drop * kPointerSize));
  Ret(USE_DELAY_SLOT);
  daddiu(sp, sp, drop * kPointerSize);
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

  Daddu(sp, sp, Operand(count * kPointerSize));

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

void TurboAssembler::Push(Smi smi) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(smi));
  push(scratch);
}

void TurboAssembler::Push(Handle<HeapObject> handle) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(handle));
  push(scratch);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  li(a1, ExternalReference::debug_restart_fp_address(isolate()));
  Ld(a1, MemOperand(a1));
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
  Ld(t1, MemOperand(t2));
  push(t1);

  // Set this new handler as the current one.
  Sd(sp, MemOperand(t2));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(a1);
  Daddu(sp, sp,
        Operand(
            static_cast<int64_t>(StackHandlerConstants::kSize - kPointerSize)));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch,
     ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  Sd(a1, MemOperand(scratch));
}

void TurboAssembler::FPUCanonicalizeNaN(const DoubleRegister dst,
                                        const DoubleRegister src) {
  sub_d(dst, src, kDoubleRegZero);
}

void TurboAssembler::MovFromFloatResult(const DoubleRegister dst) {
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

void TurboAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  if (IsMipsSoftFloatABI) {
    if (kArchEndian == kLittle) {
      Move(dst, a0, a1);
    } else {
      Move(dst, a1, a0);
    }
  } else {
    Move(dst, f12);  // Reg f12 is n64 ABI FP first argument value.
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
    const DoubleRegister fparg2 = f13;
    if (src2 == f12) {
      DCHECK(src1 != fparg2);
      Move(fparg2, src2);
      Move(f12, src1);
    } else {
      Move(f12, src1);
      Move(fparg2, src2);
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

void TurboAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
                                        Register caller_args_count_reg,
                                        Register scratch0, Register scratch1) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
#endif

  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  Dlsa(dst_reg, fp, caller_args_count_reg, kPointerSizeLog2);
  Daddu(dst_reg, dst_reg,
        Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count_reg;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  if (callee_args_count.is_reg()) {
    Dlsa(src_reg, sp, callee_args_count.reg(), kPointerSizeLog2);
    Daddu(src_reg, src_reg, Operand(kPointerSize));
  } else {
    Daddu(src_reg, sp,
          Operand((callee_args_count.immediate() + 1) * kPointerSize));
  }

  if (FLAG_debug_code) {
    Check(lo, AbortReason::kStackAccessBelowStackPointer, src_reg,
          Operand(dst_reg));
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  Ld(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  Ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop, entry;
  Branch(&entry);
  bind(&loop);
  Dsubu(src_reg, src_reg, Operand(kPointerSize));
  Dsubu(dst_reg, dst_reg, Operand(kPointerSize));
  Ld(tmp_reg, MemOperand(src_reg));
  Sd(tmp_reg, MemOperand(dst_reg));
  bind(&entry);
  Branch(&loop, ne, sp, Operand(src_reg));

  // Leave current frame.
  mov(sp, dst_reg);
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual, Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  a0: actual arguments count
  //  a1: function (passed through to callee)
  //  a2: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  DCHECK(actual.is_immediate() || actual.reg() == a0);
  DCHECK(expected.is_immediate() || expected.reg() == a2);

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    li(a0, Operand(actual.immediate()));
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        li(a2, Operand(expected.immediate()));
      }
    }
  } else if (actual.is_immediate()) {
    li(a0, Operand(actual.immediate()));
    Branch(&regular_invoke, eq, expected.reg(), Operand(a0));
  } else {
    Branch(&regular_invoke, eq, expected.reg(), Operand(actual.reg()));
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor);
      if (!*definitely_mismatches) {
        Branch(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;

  li(t0, ExternalReference::debug_hook_on_function_call_address(isolate()));
  Lb(t0, MemOperand(t0));
  Branch(&skip_hook, eq, t0, Operand(zero_reg));

  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    if (actual.is_reg()) {
      mov(t0, actual.reg());
    } else {
      li(t0, actual.immediate());
    }
    Dlsa(t0, sp, t0, kPointerSizeLog2);
    Ld(t0, MemOperand(t0));
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    if (expected.is_reg()) {
      SmiTag(expected.reg());
      Push(expected.reg());
    }
    if (actual.is_reg()) {
      SmiTag(actual.reg());
      Push(actual.reg());
    }
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
    if (actual.is_reg()) {
      Pop(actual.reg());
      SmiUntag(actual.reg());
    }
    if (expected.is_reg()) {
      Pop(expected.reg());
      SmiUntag(expected.reg());
    }
  }
  bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function == a1);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == a3);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(a3, RootIndex::kUndefinedValue);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = kJavaScriptCallCodeStartRegister;
    Ld(code, FieldMemOperand(function, JSFunction::kCodeOffset));
    if (flag == CALL_FUNCTION) {
      Daddu(code, code, Operand(Code::kHeaderSize - kHeapObjectTag));
      Call(code);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      Daddu(code, code, Operand(Code::kHeaderSize - kHeapObjectTag));
      Jump(code);
    }
    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK(function == a1);
  Register expected_reg = a2;
  Register temp_reg = t0;
  Ld(temp_reg, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // The argument count is stored as uint16_t
  Lhu(expected_reg,
      FieldMemOperand(temp_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount expected(expected_reg);
  InvokeFunctionCode(a1, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK(function == a1);

  // Get the function and setup the context.
  Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  InvokeFunctionCode(a1, no_reg, expected, actual, flag);
}

// ---------------------------------------------------------------------------
// Support functions.

void MacroAssembler::GetObjectType(Register object, Register map,
                                   Register type_reg) {
  Ld(map, FieldMemOperand(object, HeapObject::kMapOffset));
  Lhu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
}

// -----------------------------------------------------------------------------
// Runtime calls.

void TurboAssembler::DaddOverflow(Register dst, Register left,
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
    daddu(scratch, left, right_reg);
    xor_(overflow, scratch, left);
    xor_(at, scratch, right_reg);
    and_(overflow, overflow, at);
    mov(dst, scratch);
  } else {
    daddu(dst, left, right_reg);
    xor_(overflow, dst, left);
    xor_(at, dst, right_reg);
    and_(overflow, overflow, at);
  }
}

void TurboAssembler::DsubOverflow(Register dst, Register left,
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
    dsubu(scratch, left, right_reg);
    xor_(overflow, left, scratch);
    xor_(at, left, right_reg);
    and_(overflow, overflow, at);
    mov(dst, scratch);
  } else {
    dsubu(dst, left, right_reg);
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
    Mul(scratch, left, right_reg);
    Mulh(overflow, left, right_reg);
    mov(dst, scratch);
  } else {
    Mul(dst, left, right_reg);
    Mulh(overflow, left, right_reg);
  }

  dsra32(scratch, dst, 0);
  xor_(overflow, overflow, scratch);
}

void TurboAssembler::CallRuntimeWithCEntry(Runtime::FunctionId fid,
                                           Register centry) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  PrepareCEntryArgs(f->nargs);
  PrepareCEntryFunction(ExternalReference::Create(f));
  DCHECK(!AreAliased(centry, a0, a1));
  Daddu(centry, centry, Operand(Code::kHeaderSize - kHeapObjectTag));
  Call(centry);
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
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    li(scratch2, ExternalReference::Create(counter));
    Lw(scratch1, MemOperand(scratch2));
    Addu(scratch1, scratch1, Operand(value));
    Sw(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    li(scratch2, ExternalReference::Create(counter));
    Lw(scratch1, MemOperand(scratch2));
    Subu(scratch1, scratch1, Operand(value));
    Sw(scratch1, MemOperand(scratch2));
  }
}

// -----------------------------------------------------------------------------
// Debugging.

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
  const char* msg = GetAbortReason(reason);
#ifdef DEBUG
  RecordComment("Abort message: ");
  RecordComment(msg);
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    stop(msg);
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
  if (!has_frame()) {
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

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  Ld(dst, NativeContextMemOperand());
  Ld(dst, ContextMemOperand(dst, index));
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
  daddiu(sp, sp, stack_offset);
  stack_offset = -stack_offset - kPointerSize;
  Sd(ra, MemOperand(sp, stack_offset));
  stack_offset -= kPointerSize;
  Sd(fp, MemOperand(sp, stack_offset));
  stack_offset -= kPointerSize;
  li(t9, Operand(StackFrame::TypeToMarker(type)));
  Sd(t9, MemOperand(sp, stack_offset));
  // Adjust FP to point to saved FP.
  DCHECK_EQ(stack_offset, 0);
  Daddu(fp, sp, Operand(fp_offset));
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  daddiu(sp, fp, 2 * kPointerSize);
  Ld(ra, MemOperand(fp, 1 * kPointerSize));
  Ld(fp, MemOperand(fp, 0 * kPointerSize));
}

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
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
  daddiu(sp, sp, -2 * kPointerSize - ExitFrameConstants::kFixedFrameSizeFromFp);
  Sd(ra, MemOperand(sp, 3 * kPointerSize));
  Sd(fp, MemOperand(sp, 2 * kPointerSize));
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, Operand(StackFrame::TypeToMarker(frame_type)));
    Sd(scratch, MemOperand(sp, 1 * kPointerSize));
  }
  // Set up new frame pointer.
  daddiu(fp, sp, ExitFrameConstants::kFixedFrameSizeFromFp);

  if (emit_debug_code()) {
    Sd(zero_reg, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }

  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    // Save the frame pointer and the context in top.
    li(t8, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                     isolate()));
    Sd(fp, MemOperand(t8));
    li(t8,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
    Sd(cp, MemOperand(t8));
  }

  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (save_doubles) {
    // The stack is already aligned to 0 modulo 8 for stores with sdc1.
    int kNumOfSavedRegisters = FPURegister::kNumRegisters / 2;
    int space = kNumOfSavedRegisters * kDoubleSize;
    Dsubu(sp, sp, Operand(space));
    // Remember: we only need to save every 2nd double FPU value.
    for (int i = 0; i < kNumOfSavedRegisters; i++) {
      FPURegister reg = FPURegister::from_code(2 * i);
      Sdc1(reg, MemOperand(sp, i * kDoubleSize));
    }
  }

  // Reserve place for the return address, stack space and an optional slot
  // (used by DirectCEntry to hold the return value if a struct is
  // returned) and align the frame preparing for calling the runtime function.
  DCHECK_GE(stack_space, 0);
  Dsubu(sp, sp, Operand((stack_space + 2) * kPointerSize));
  if (frame_alignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));  // Align stack.
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  daddiu(scratch, sp, kPointerSize);
  Sd(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool do_return,
                                    bool argument_count_is_length) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Remember: we only need to restore every 2nd double FPU value.
    int kNumOfSavedRegisters = FPURegister::kNumRegisters / 2;
    Dsubu(t8, fp,
          Operand(ExitFrameConstants::kFixedFrameSizeFromFp +
                  kNumOfSavedRegisters * kDoubleSize));
    for (int i = 0; i < kNumOfSavedRegisters; i++) {
      FPURegister reg = FPURegister::from_code(2 * i);
      Ldc1(reg, MemOperand(t8, i * kDoubleSize));
    }
  }

  // Clear top frame.
  li(t8,
     ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  Sd(zero_reg, MemOperand(t8));

  // Restore current context from top and clear it in debug mode.
  li(t8,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Ld(cp, MemOperand(t8));

#ifdef DEBUG
  li(t8,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Sd(a3, MemOperand(t8));
#endif

  // Pop the arguments, restore registers, and return.
  mov(sp, fp);  // Respect ABI stack constraint.
  Ld(fp, MemOperand(sp, ExitFrameConstants::kCallerFPOffset));
  Ld(ra, MemOperand(sp, ExitFrameConstants::kCallerPCOffset));

  if (argument_count.is_valid()) {
    if (argument_count_is_length) {
      daddu(sp, sp, argument_count);
    } else {
      Dlsa(sp, sp, argument_count, kPointerSizeLog2, t8);
    }
  }

  if (do_return) {
    Ret(USE_DELAY_SLOT);
    // If returning, the instruction in the delay slot will be the addiu below.
  }
  daddiu(sp, sp, 2 * kPointerSize);
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64
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
      {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        andi(scratch, sp, frame_alignment_mask);
        Branch(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      }
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      stop("Unexpected stack alignment");
      bind(&alignment_as_expected);
    }
  }
}

void TurboAssembler::SmiUntag(Register dst, const MemOperand& src) {
  if (SmiValuesAre32Bits()) {
    Lw(dst, MemOperand(src.rm(), SmiWordOffset(src.offset())));
  } else {
    DCHECK(SmiValuesAre31Bits());
    Lw(dst, src);
    SmiUntag(dst);
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

    ld(t8, FieldMemOperand(object, HeapObject::kMapOffset));
    Lbu(t8, FieldMemOperand(t8, Map::kBitFieldOffset));
    And(t8, t8, Operand(Map::IsConstructorBit::kMask));
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

  if (kArchVariant >= kMips64r6) {
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
      dsll32(t8, t8, 0);
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

  if (kArchVariant >= kMips64r6) {
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
      dsll32(t8, t8, 0);
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

void TurboAssembler::Float64Max(FPURegister dst, FPURegister src1,
                                FPURegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_d(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF64(src1, src2);
  BranchTrueF(out_of_line);

  if (kArchVariant >= kMips64r6) {
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
      dmfc1(t8, src1);
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

void TurboAssembler::Float64MaxOutOfLine(FPURegister dst, FPURegister src1,
                                         FPURegister src2) {
  add_d(dst, src1, src2);
}

void TurboAssembler::Float64Min(FPURegister dst, FPURegister src1,
                                FPURegister src2, Label* out_of_line) {
  if (src1 == src2) {
    Move_d(dst, src1);
    return;
  }

  // Check if one of operands is NaN.
  CompareIsNanF64(src1, src2);
  BranchTrueF(out_of_line);

  if (kArchVariant >= kMips64r6) {
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
      dmfc1(t8, src1);
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

void TurboAssembler::Float64MinOutOfLine(FPURegister dst, FPURegister src1,
                                         FPURegister src2) {
  add_d(dst, src1, src2);
}

static const int kRegisterPassedArguments = 8;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  num_reg_arguments += 2 * num_double_arguments;

  // O32: Up to four simple arguments are passed in registers a0..a3.
  // N64: Up to eight simple arguments are passed in registers a0..a7.
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

  // n64: Up to eight simple arguments in a0..a3, a4..a7, No argument slots.
  // O32: Up to four simple arguments are passed in registers a0..a3.
  // Those four arguments must have reserved argument slots on the stack for
  // mips, even though those argument slots are not normally used.
  // Both ABIs: Remaining arguments are pushed on the stack, above (higher
  // address than) the (O32) argument slots. (arg slot calculation handled by
  // CalculateStackPassedWords()).
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    Dsubu(sp, sp, Operand((stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));
    Sd(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Dsubu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  li(t9, function);
  CallCFunctionHelper(t9, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK_LE(num_reg_arguments + num_double_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
  // The argument stots are presumed to have been set up by
  // PrepareCallCFunction. The C function must be called via t9, for mips ABI.

#if V8_HOST_ARCH_MIPS || V8_HOST_ARCH_MIPS64
  if (emit_debug_code()) {
    int frame_alignment = base::OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      Label alignment_as_expected;
      {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        And(scratch, sp, Operand(frame_alignment_mask));
        Branch(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      }
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop("Unexpected alignment in CallCFunction");
      bind(&alignment_as_expected);
    }
  }
#endif  // V8_HOST_ARCH_MIPS

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (function != t9) {
      mov(t9, function);
      function = t9;
    }

    // Save the frame pointer and PC so that the stack layout remains iterable,
    // even without an ExitFrame which normally exists between JS and C frames.
    if (isolate() != nullptr) {
      // 't' registers are caller-saved so this is safe as a scratch register.
      Register scratch1 = t1;
      Register scratch2 = t2;
      DCHECK(!AreAliased(scratch1, scratch2, function));

      Label get_pc;
      mov(scratch1, ra);
      Call(&get_pc);

      bind(&get_pc);
      mov(scratch2, ra);
      mov(ra, scratch1);

      li(scratch1, ExternalReference::fast_c_call_caller_pc_address(isolate()));
      Sd(scratch2, MemOperand(scratch1));
      li(scratch1, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      Sd(fp, MemOperand(scratch1));
    }

    Call(function);

    if (isolate() != nullptr) {
      // We don't unset the PC; the FP is the source of truth.
      Register scratch = t1;
      li(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      Sd(zero_reg, MemOperand(scratch));
    }
  }

  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);

  if (base::OS::ActivationFrameAlignment() > kPointerSize) {
    Ld(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Daddu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}

#undef BRANCH_ARGS_CHECK

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met) {
  And(scratch, object, Operand(~kPageAlignmentMask));
  Ld(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
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
  if (kArchVariant == kMips64r6) {
    addiupc(ra, 1);
  } else {
    nal();
    nop();
  }
  int pc = pc_offset();
  li(dst, Operand(pc));
  Dsubu(dst, ra, dst);

  pop(ra);  // Restore ra
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  li(kSpeculationPoisonRegister, -1);
}

void TurboAssembler::CallForDeoptimization(Address target, int deopt_id) {
  NoRootArrayScope no_root_array(this);

  // Save the deopt id in kRootRegister (we don't need the roots array from now
  // on).
  DCHECK_LE(deopt_id, 0xFFFF);
  li(kRootRegister, deopt_id);
  Call(target, RelocInfo::RUNTIME_ENTRY);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
