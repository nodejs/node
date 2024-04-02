// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_RISCV64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/heap-number.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-code-manager.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/riscv64/macro-assembler-riscv64.h"
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

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  bytes += list.Count() * kSystemPointerSize;

  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += kCallerSavedFPU.Count() * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  int bytes = 0;

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  MultiPush(list);
  bytes += list.Count() * kSystemPointerSize;

  if (fp_mode == SaveFPRegsMode::kSave) {
    MultiPushFPU(kCallerSavedFPU);
    bytes += kCallerSavedFPU.Count() * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    MultiPopFPU(kCallerSavedFPU);
    bytes += kCallerSavedFPU.Count() * kDoubleSize;
  }

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  MultiPop(list);
  bytes += list.Count() * kSystemPointerSize;

  return bytes;
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  Ld(destination,
     MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition cond, Register src1,
                              const Operand& src2) {
  Label skip;
  BranchShort(&skip, NegateCondition(cond), src1, src2);
  Ld(destination,
     MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
  bind(&skip);
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Push(ra, fp, marker_reg);
    Add64(fp, sp, Operand(kSystemPointerSize));
  } else {
    Push(ra, fp);
    Mv(fp, sp);
  }
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  int offset = -StandardFrameConstants::kContextOffset;
  if (function_reg.is_valid()) {
    Push(ra, fp, cp, function_reg, kJavaScriptCallArgCountRegister);
    offset += 2 * kSystemPointerSize;
  } else {
    Push(ra, fp, cp, kJavaScriptCallArgCountRegister);
    offset += kSystemPointerSize;
  }
  Add64(fp, sp, Operand(offset));
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
                                      Register value, RAStatus ra_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  DCHECK(!AreAliased(object, value));
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  if (FLAG_debug_code) {
    Label ok;
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(!AreAliased(object, value, scratch));
    Add64(scratch, object, offset - kHeapObjectTag);
    And(scratch, scratch, Operand(kTaggedSize - 1));
    BranchShort(&ok, eq, scratch, Operand(zero_reg));
    Abort(AbortReason::kUnalignedCellInWriteBarrier);
    bind(&ok);
  }

  RecordWrite(object, Operand(offset - kHeapObjectTag), value, ra_status,
              save_fp, remembered_set_action, SmiCheck::kOmit);

  bind(&done);
}

void TurboAssembler::MaybeSaveRegisters(RegList registers) {
  if (registers.is_empty()) return;
  MultiPush(registers);
}

void TurboAssembler::MaybeRestoreRegisters(RegList registers) {
  if (registers.is_empty()) return;
  MultiPop(registers);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object,
                                             Register slot_address,
                                             SaveFPRegsMode fp_mode) {
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  Push(object);
  Push(slot_address);
  Pop(slot_address_parameter);
  Pop(object_parameter);

  Call(isolate()->builtins()->code_handle(
           Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
       RelocInfo::CODE_TARGET);
  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStubSaveRegisters(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  Push(object);
  Push(slot_address);
  Pop(slot_address_parameter);
  Pop(object_parameter);

  CallRecordWriteStub(object_parameter, slot_address_parameter,
                      remembered_set_action, fp_mode, mode);

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  // Use CallRecordWriteStubSaveRegisters if the object and slot registers
  // need to be caller saved.
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    auto wasm_target =
        wasm::WasmCode::GetRecordWriteStub(remembered_set_action, fp_mode);
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
  } else {
    auto builtin = Builtins::GetRecordWriteStub(remembered_set_action, fp_mode);
    if (options().inline_offheap_trampolines) {
      // Inline the trampoline. //qj
      RecordCommentForOffHeapTrampoline(builtin);

      UseScratchRegisterScope temps(this);
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
      Call(scratch);
      RecordComment("]");
    } else {
      Handle<Code> code_target = isolate()->builtins()->code_handle(builtin);
      Call(code_target, RelocInfo::CODE_TARGET);
    }
  }
}

// Clobbers object, address, value, and ra, if (ra_status == kRAHasBeenSaved)
// The register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object, Operand offset,
                                 Register value, RAStatus ra_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(!AreAliased(object, value));

  if (FLAG_debug_code) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.Acquire();
    DCHECK(!AreAliased(object, value, temp));
    Add64(temp, object, offset);
    LoadTaggedPointerField(temp, MemOperand(temp));
    Assert(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite, temp,
           Operand(value));
  }

  if ((remembered_set_action == RememberedSetAction::kOmit &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == SmiCheck::kInline) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  {
    UseScratchRegisterScope temps(this);
    Register temp = temps.Acquire();
    CheckPageFlag(value,
                  temp,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask,
                  eq,  // In RISC-V, it uses cc for a comparison with 0, so if
                       // no bits are set, and cc is eq, it will branch to done
                  &done);

    CheckPageFlag(object,
                  temp,  // Used as scratch.
                  MemoryChunk::kPointersFromHereAreInterestingMask,
                  eq,  // In RISC-V, it uses cc for a comparison with 0, so if
                       // no bits are set, and cc is eq, it will branch to done
                  &done);
  }
  // Record the actual write.
  if (ra_status == kRAHasNotBeenSaved) {
    push(ra);
  }
  Register slot_address = WriteBarrierDescriptor::SlotAddressRegister();
  DCHECK(!AreAliased(object, slot_address, value));
  // TODO(cbruni): Turn offset into int.
  DCHECK(offset.IsImmediate());
  Add64(slot_address, object, offset);
  CallRecordWriteStub(object, slot_address, remembered_set_action, fp_mode);
  if (ra_status == kRAHasNotBeenSaved) {
    pop(ra);
  }
  if (FLAG_debug_code) li(slot_address, Operand(kZapValue));

  bind(&done);
}

// ---------------------------------------------------------------------------
// Instruction macros.

void TurboAssembler::Add32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_addw(rd, rt.rm());
    } else {
      addw(rd, rs, rt.rm());
    }
  } else {
    if (FLAG_riscv_c_extension && is_int6(rt.immediate()) &&
        (rd.code() == rs.code()) && (rd != zero_reg) &&
        !MustUseReg(rt.rmode())) {
      c_addiw(rd, static_cast<int8_t>(rt.immediate()));
    } else if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiw(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else if ((-4096 <= rt.immediate() && rt.immediate() <= -2049) ||
               (2048 <= rt.immediate() && rt.immediate() <= 4094)) {
      addiw(rd, rs, rt.immediate() / 2);
      addiw(rd, rd, rt.immediate() - (rt.immediate() / 2));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, rt.immediate());
      addw(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Add64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        (rt.rm() != zero_reg) && (rs != zero_reg)) {
      c_add(rd, rt.rm());
    } else {
      add(rd, rs, rt.rm());
    }
  } else {
    if (FLAG_riscv_c_extension && is_int6(rt.immediate()) &&
        (rd.code() == rs.code()) && (rd != zero_reg) && (rt.immediate() != 0) &&
        !MustUseReg(rt.rmode())) {
      c_addi(rd, static_cast<int8_t>(rt.immediate()));
    } else if (FLAG_riscv_c_extension && is_int10(rt.immediate()) &&
               (rt.immediate() != 0) && ((rt.immediate() & 0xf) == 0) &&
               (rd.code() == rs.code()) && (rd == sp) &&
               !MustUseReg(rt.rmode())) {
      c_addi16sp(static_cast<int16_t>(rt.immediate()));
    } else if (FLAG_riscv_c_extension && ((rd.code() & 0b11000) == 0b01000) &&
               (rs == sp) && is_uint10(rt.immediate()) &&
               (rt.immediate() != 0) && !MustUseReg(rt.rmode())) {
      c_addi4spn(rd, static_cast<uint16_t>(rt.immediate()));
    } else if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      addi(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else if ((-4096 <= rt.immediate() && rt.immediate() <= -2049) ||
               (2048 <= rt.immediate() && rt.immediate() <= 4094)) {
      addi(rd, rs, rt.immediate() / 2);
      addi(rd, rd, rt.immediate() - (rt.immediate() / 2));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Li(scratch, rt.immediate());
      add(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Sub32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_subw(rd, rt.rm());
    } else {
      subw(rd, rs, rt.rm());
    }
  } else {
    DCHECK(is_int32(rt.immediate()));
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        (rd != zero_reg) && is_int6(-rt.immediate()) &&
        !MustUseReg(rt.rmode())) {
      c_addiw(
          rd,
          static_cast<int8_t>(
              -rt.immediate()));  // No c_subiw instr, use c_addiw(x, y, -imm).
    } else if (is_int12(-rt.immediate()) && !MustUseReg(rt.rmode())) {
      addiw(rd, rs,
            static_cast<int32_t>(
                -rt.immediate()));  // No subiw instr, use addiw(x, y, -imm).
    } else if ((-4096 <= -rt.immediate() && -rt.immediate() <= -2049) ||
               (2048 <= -rt.immediate() && -rt.immediate() <= 4094)) {
      addiw(rd, rs, -rt.immediate() / 2);
      addiw(rd, rd, -rt.immediate() - (-rt.immediate() / 2));
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      if (-rt.immediate() >> 12 == 0 && !MustUseReg(rt.rmode())) {
        // Use load -imm and addu when loading -imm generates one instruction.
        Li(scratch, -rt.immediate());
        addw(rd, rs, scratch);
      } else {
        // li handles the relocation.
        Li(scratch, rt.immediate());
        subw(rd, rs, scratch);
      }
    }
  }
}

void TurboAssembler::Sub64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_sub(rd, rt.rm());
    } else {
      sub(rd, rs, rt.rm());
    }
  } else if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
             (rd != zero_reg) && is_int6(-rt.immediate()) &&
             (rt.immediate() != 0) && !MustUseReg(rt.rmode())) {
    c_addi(rd,
           static_cast<int8_t>(
               -rt.immediate()));  // No c_subi instr, use c_addi(x, y, -imm).

  } else if (FLAG_riscv_c_extension && is_int10(-rt.immediate()) &&
             (rt.immediate() != 0) && ((rt.immediate() & 0xf) == 0) &&
             (rd.code() == rs.code()) && (rd == sp) &&
             !MustUseReg(rt.rmode())) {
    c_addi16sp(static_cast<int16_t>(-rt.immediate()));
  } else if (is_int12(-rt.immediate()) && !MustUseReg(rt.rmode())) {
    addi(rd, rs,
         static_cast<int32_t>(
             -rt.immediate()));  // No subi instr, use addi(x, y, -imm).
  } else if ((-4096 <= -rt.immediate() && -rt.immediate() <= -2049) ||
             (2048 <= -rt.immediate() && -rt.immediate() <= 4094)) {
    addi(rd, rs, -rt.immediate() / 2);
    addi(rd, rd, -rt.immediate() - (-rt.immediate() / 2));
  } else {
    int li_count = InstrCountForLi64Bit(rt.immediate());
    int li_neg_count = InstrCountForLi64Bit(-rt.immediate());
    if (li_neg_count < li_count && !MustUseReg(rt.rmode())) {
      // Use load -imm and add when loading -imm generates one instruction.
      DCHECK(rt.immediate() != std::numeric_limits<int32_t>::min());
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, -rt.immediate());
      add(rd, rs, scratch);
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, rt.immediate());
      sub(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Mul32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mulw(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    mulw(rd, rs, scratch);
  }
}

void TurboAssembler::Mulh32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mul(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    mul(rd, rs, scratch);
  }
  srai(rd, rd, 32);
}

void TurboAssembler::Mulhu32(Register rd, Register rs, const Operand& rt,
                             Register rsz, Register rtz) {
  slli(rsz, rs, 32);
  if (rt.is_reg()) {
    slli(rtz, rt.rm(), 32);
  } else {
    Li(rtz, rt.immediate() << 32);
  }
  mulhu(rd, rsz, rtz);
  srai(rd, rd, 32);
}

void TurboAssembler::Mul64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mul(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    mul(rd, rs, scratch);
  }
}

void TurboAssembler::Mulh64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mulh(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    mulh(rd, rs, scratch);
  }
}

void TurboAssembler::Div32(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divw(res, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    divw(res, rs, scratch);
  }
}

void TurboAssembler::Mod32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    remw(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    remw(rd, rs, scratch);
  }
}

void TurboAssembler::Modu32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    remuw(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    remuw(rd, rs, scratch);
  }
}

void TurboAssembler::Div64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    div(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    div(rd, rs, scratch);
  }
}

void TurboAssembler::Divu32(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divuw(res, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    divuw(res, rs, scratch);
  }
}

void TurboAssembler::Divu64(Register res, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divu(res, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    divu(res, rs, scratch);
  }
}

void TurboAssembler::Mod64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    rem(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    rem(rd, rs, scratch);
  }
}

void TurboAssembler::Modu64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    remu(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Li(scratch, rt.immediate());
    remu(rd, rs, scratch);
  }
}

void TurboAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_and(rd, rt.rm());
    } else {
      and_(rd, rs, rt.rm());
    }
  } else {
    if (FLAG_riscv_c_extension && is_int6(rt.immediate()) &&
        !MustUseReg(rt.rmode()) && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000)) {
      c_andi(rd, static_cast<int8_t>(rt.immediate()));
    } else if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      andi(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, rt.immediate());
      and_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Or(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_or(rd, rt.rm());
    } else {
      or_(rd, rs, rt.rm());
    }
  } else {
    if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      ori(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, rt.immediate());
      or_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Xor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        ((rd.code() & 0b11000) == 0b01000) &&
        ((rt.rm().code() & 0b11000) == 0b01000)) {
      c_xor(rd, rt.rm());
    } else {
      xor_(rd, rs, rt.rm());
    }
  } else {
    if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      xori(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Li(scratch, rt.immediate());
      xor_(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Nor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    or_(rd, rs, rt.rm());
    not_(rd, rd);
  } else {
    Or(rd, rs, rt);
    not_(rd, rd);
  }
}

void TurboAssembler::Neg(Register rs, const Operand& rt) {
  DCHECK(rt.is_reg());
  neg(rs, rt.rm());
}

void TurboAssembler::Seqz(Register rd, const Operand& rt) {
  if (rt.is_reg()) {
    seqz(rd, rt.rm());
  } else {
    li(rd, rt.immediate() == 0);
  }
}

void TurboAssembler::Snez(Register rd, const Operand& rt) {
  if (rt.is_reg()) {
    snez(rd, rt.rm());
  } else {
    li(rd, rt.immediate() != 0);
  }
}

void TurboAssembler::Seq(Register rd, Register rs, const Operand& rt) {
  if (rs == zero_reg) {
    Seqz(rd, rt);
  } else if (IsZero(rt)) {
    seqz(rd, rs);
  } else {
    Sub64(rd, rs, rt);
    seqz(rd, rd);
  }
}

void TurboAssembler::Sne(Register rd, Register rs, const Operand& rt) {
  if (rs == zero_reg) {
    Snez(rd, rt);
  } else if (IsZero(rt)) {
    snez(rd, rs);
  } else {
    Sub64(rd, rs, rt);
    snez(rd, rd);
  }
}

void TurboAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      slti(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Li(scratch, rt.immediate());
      slt(rd, rs, scratch);
    }
  }
}

void TurboAssembler::Sltu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rs, rt.rm());
  } else {
    if (is_int12(rt.immediate()) && !MustUseReg(rt.rmode())) {
      sltiu(rd, rs, static_cast<int32_t>(rt.immediate()));
    } else {
      // li handles the relocation.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Li(scratch, rt.immediate());
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
    Register scratch = temps.Acquire();
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Li(scratch, rt.immediate());
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
    Register scratch = temps.Acquire();
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Li(scratch, rt.immediate());
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
    Register scratch = temps.Acquire();
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Li(scratch, rt.immediate());
    slt(rd, scratch, rs);
  }
}

void TurboAssembler::Sgtu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rt.rm(), rs);
  } else {
    // li handles the relocation.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    BlockTrampolinePoolScope block_trampoline_pool(this);
    Li(scratch, rt.immediate());
    sltu(rd, scratch, rs);
  }
}

void TurboAssembler::Sll32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sllw(rd, rs, rt.rm());
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    slliw(rd, rs, shamt);
  }
}

void TurboAssembler::Sra32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sraw(rd, rs, rt.rm());
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    sraiw(rd, rs, shamt);
  }
}

void TurboAssembler::Srl32(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    srlw(rd, rs, rt.rm());
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    srliw(rd, rs, shamt);
  }
}

void TurboAssembler::Sra64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sra(rd, rs, rt.rm());
  } else if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
             ((rd.code() & 0b11000) == 0b01000) && is_int6(rt.immediate())) {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    c_srai(rd, shamt);
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    srai(rd, rs, shamt);
  }
}

void TurboAssembler::Srl64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    srl(rd, rs, rt.rm());
  } else if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
             ((rd.code() & 0b11000) == 0b01000) && is_int6(rt.immediate())) {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    c_srli(rd, shamt);
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    srli(rd, rs, shamt);
  }
}

void TurboAssembler::Sll64(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sll(rd, rs, rt.rm());
  } else {
    uint8_t shamt = static_cast<uint8_t>(rt.immediate());
    if (FLAG_riscv_c_extension && (rd.code() == rs.code()) &&
        (rd != zero_reg) && (shamt != 0) && is_uint6(shamt)) {
      c_slli(rd, shamt);
    } else {
      slli(rd, rs, shamt);
    }
  }
}

void TurboAssembler::Li(Register rd, int64_t imm) {
  if (FLAG_riscv_c_extension && (rd != zero_reg) && is_int6(imm)) {
    c_li(rd, imm);
  } else {
    RV_li(rd, imm);
  }
}

void TurboAssembler::Mv(Register rd, const Operand& rt) {
  if (FLAG_riscv_c_extension && (rd != zero_reg) && (rt.rm() != zero_reg)) {
    c_mv(rd, rt.rm());
  } else {
    mv(rd, rt.rm());
  }
}

void TurboAssembler::Ror(Register rd, Register rs, const Operand& rt) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (rt.is_reg()) {
    negw(scratch, rt.rm());
    sllw(scratch, rs, scratch);
    srlw(rd, rs, rt.rm());
    or_(rd, scratch, rd);
    sext_w(rd, rd);
  } else {
    int64_t ror_value = rt.immediate() % 32;
    if (ror_value == 0) {
      Mv(rd, rs);
      return;
    } else if (ror_value < 0) {
      ror_value += 32;
    }
    srliw(scratch, rs, ror_value);
    slliw(rd, rs, 32 - ror_value);
    or_(rd, scratch, rd);
    sext_w(rd, rd);
  }
}

void TurboAssembler::Dror(Register rd, Register rs, const Operand& rt) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (rt.is_reg()) {
    negw(scratch, rt.rm());
    sll(scratch, rs, scratch);
    srl(rd, rs, rt.rm());
    or_(rd, scratch, rd);
  } else {
    int64_t dror_value = rt.immediate() % 64;
    if (dror_value == 0) {
      Mv(rd, rs);
      return;
    } else if (dror_value < 0) {
      dror_value += 64;
    }
    srli(scratch, rs, dror_value);
    slli(rd, rs, 64 - dror_value);
    or_(rd, scratch, rd);
  }
}

void TurboAssembler::CalcScaledAddress(Register rd, Register rt, Register rs,
                                       uint8_t sa) {
  DCHECK(sa >= 1 && sa <= 31);
  UseScratchRegisterScope temps(this);
  Register tmp = rd == rt ? temps.Acquire() : rd;
  DCHECK(tmp != rt);
  slli(tmp, rs, sa);
  Add64(rd, rt, tmp);
}

// ------------Pseudo-instructions-------------
// Change endianness
void TurboAssembler::ByteSwap(Register rd, Register rs, int operand_size,
                              Register scratch) {
  DCHECK_NE(scratch, rs);
  DCHECK_NE(scratch, rd);
  DCHECK(operand_size == 4 || operand_size == 8);
  if (operand_size == 4) {
    // Uint32_t x1 = 0x00FF00FF;
    // x0 = (x0 << 16 | x0 >> 16);
    // x0 = (((x0 & x1) << 8)  | ((x0 & (x1 << 8)) >> 8));
    UseScratchRegisterScope temps(this);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK((rd != t6) && (rs != t6));
    Register x0 = temps.Acquire();
    Register x1 = temps.Acquire();
    Register x2 = scratch;
    li(x1, 0x00FF00FF);
    slliw(x0, rs, 16);
    srliw(rd, rs, 16);
    or_(x0, rd, x0);   // x0 <- x0 << 16 | x0 >> 16
    and_(x2, x0, x1);  // x2 <- x0 & 0x00FF00FF
    slliw(x2, x2, 8);  // x2 <- (x0 & x1) << 8
    slliw(x1, x1, 8);  // x1 <- 0xFF00FF00
    and_(rd, x0, x1);  // x0 & 0xFF00FF00
    srliw(rd, rd, 8);
    or_(rd, rd, x2);  // (((x0 & x1) << 8)  | ((x0 & (x1 << 8)) >> 8))
  } else {
    // uinx24_t x1 = 0x0000FFFF0000FFFFl;
    // uinx24_t x1 = 0x00FF00FF00FF00FFl;
    // x0 = (x0 << 32 | x0 >> 32);
    // x0 = (x0 & x1) << 16 | (x0 & (x1 << 16)) >> 16;
    // x0 = (x0 & x1) << 8  | (x0 & (x1 << 8)) >> 8;
    UseScratchRegisterScope temps(this);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    DCHECK((rd != t6) && (rs != t6));
    Register x0 = temps.Acquire();
    Register x1 = temps.Acquire();
    Register x2 = scratch;
    li(x1, 0x0000FFFF0000FFFFl);
    slli(x0, rs, 32);
    srli(rd, rs, 32);
    or_(x0, rd, x0);   // x0 <- x0 << 32 | x0 >> 32
    and_(x2, x0, x1);  // x2 <- x0 & 0x0000FFFF0000FFFF
    slli(x2, x2, 16);  // x2 <- (x0 & 0x0000FFFF0000FFFF) << 16
    slli(x1, x1, 16);  // x1 <- 0xFFFF0000FFFF0000
    and_(rd, x0, x1);  // rd <- x0 & 0xFFFF0000FFFF0000
    srli(rd, rd, 16);  // rd <- x0 & (x1 << 16)) >> 16
    or_(x0, rd, x2);   // (x0 & x1) << 16 | (x0 & (x1 << 16)) >> 16;
    li(x1, 0x00FF00FF00FF00FFl);
    and_(x2, x0, x1);  // x2 <- x0 & 0x00FF00FF00FF00FF
    slli(x2, x2, 8);   // x2 <- (x0 & x1) << 8
    slli(x1, x1, 8);   // x1 <- 0xFF00FF00FF00FF00
    and_(rd, x0, x1);
    srli(rd, rd, 8);  // rd <- (x0 & (x1 << 8)) >> 8
    or_(rd, rd, x2);  // (((x0 & x1) << 8)  | ((x0 & (x1 << 8)) >> 8))
  }
}

template <int NBYTES, bool LOAD_SIGNED>
void TurboAssembler::LoadNBytes(Register rd, const MemOperand& rs,
                                Register scratch) {
  DCHECK(rd != rs.rm() && rd != scratch);
  DCHECK_LE(NBYTES, 8);

  // load the most significant byte
  if (LOAD_SIGNED) {
    lb(rd, rs.rm(), rs.offset() + (NBYTES - 1));
  } else {
    lbu(rd, rs.rm(), rs.offset() + (NBYTES - 1));
  }

  // load remaining (nbytes-1) bytes from higher to lower
  slli(rd, rd, 8 * (NBYTES - 1));
  for (int i = (NBYTES - 2); i >= 0; i--) {
    lbu(scratch, rs.rm(), rs.offset() + i);
    if (i) slli(scratch, scratch, i * 8);
    or_(rd, rd, scratch);
  }
}

template <int NBYTES, bool LOAD_SIGNED>
void TurboAssembler::LoadNBytesOverwritingBaseReg(const MemOperand& rs,
                                                  Register scratch0,
                                                  Register scratch1) {
  // This function loads nbytes from memory specified by rs and into rs.rm()
  DCHECK(rs.rm() != scratch0 && rs.rm() != scratch1 && scratch0 != scratch1);
  DCHECK_LE(NBYTES, 8);

  // load the most significant byte
  if (LOAD_SIGNED) {
    lb(scratch0, rs.rm(), rs.offset() + (NBYTES - 1));
  } else {
    lbu(scratch0, rs.rm(), rs.offset() + (NBYTES - 1));
  }

  // load remaining (nbytes-1) bytes from higher to lower
  slli(scratch0, scratch0, 8 * (NBYTES - 1));
  for (int i = (NBYTES - 2); i >= 0; i--) {
    lbu(scratch1, rs.rm(), rs.offset() + i);
    if (i) {
      slli(scratch1, scratch1, i * 8);
      or_(scratch0, scratch0, scratch1);
    } else {
      // write to rs.rm() when processing the last byte
      or_(rs.rm(), scratch0, scratch1);
    }
  }
}

template <int NBYTES, bool IS_SIGNED>
void TurboAssembler::UnalignedLoadHelper(Register rd, const MemOperand& rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  UseScratchRegisterScope temps(this);

  if (NeedAdjustBaseAndOffset(rs, OffsetAccessType::TWO_ACCESSES, NBYTES - 1)) {
    // Adjust offset for two accesses and check if offset + 3 fits into int12.
    MemOperand source = rs;
    Register scratch_base = temps.Acquire();
    DCHECK(scratch_base != rs.rm());
    AdjustBaseAndOffset(&source, scratch_base, OffsetAccessType::TWO_ACCESSES,
                        NBYTES - 1);

    // Since source.rm() is scratch_base, assume rd != source.rm()
    DCHECK(rd != source.rm());
    Register scratch_other = temps.Acquire();
    LoadNBytes<NBYTES, IS_SIGNED>(rd, source, scratch_other);
  } else {
    // no need to adjust base-and-offset
    if (rd != rs.rm()) {
      Register scratch = temps.Acquire();
      LoadNBytes<NBYTES, IS_SIGNED>(rd, rs, scratch);
    } else {  // rd == rs.rm()
      Register scratch = temps.Acquire();
      Register scratch2 = temps.Acquire();
      LoadNBytesOverwritingBaseReg<NBYTES, IS_SIGNED>(rs, scratch, scratch2);
    }
  }
}

template <int NBYTES>
void TurboAssembler::UnalignedFLoadHelper(FPURegister frd, const MemOperand& rs,
                                          Register scratch_base) {
  DCHECK(NBYTES == 4 || NBYTES == 8);
  DCHECK_NE(scratch_base, rs.rm());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  MemOperand source = rs;
  if (NeedAdjustBaseAndOffset(rs, OffsetAccessType::TWO_ACCESSES, NBYTES - 1)) {
    // Adjust offset for two accesses and check if offset + 3 fits into int12.
    DCHECK(scratch_base != rs.rm());
    AdjustBaseAndOffset(&source, scratch_base, OffsetAccessType::TWO_ACCESSES,
                        NBYTES - 1);
  }
  UseScratchRegisterScope temps(this);
  Register scratch_other = temps.Acquire();
  Register scratch = temps.Acquire();
  DCHECK(scratch != rs.rm() && scratch_other != scratch &&
         scratch_other != rs.rm());
  LoadNBytes<NBYTES, true>(scratch, source, scratch_other);
  if (NBYTES == 4)
    fmv_w_x(frd, scratch);
  else
    fmv_d_x(frd, scratch);
}

template <int NBYTES>
void TurboAssembler::UnalignedStoreHelper(Register rd, const MemOperand& rs,
                                          Register scratch_other) {
  DCHECK(scratch_other != rs.rm());
  DCHECK_LE(NBYTES, 8);
  MemOperand source = rs;
  UseScratchRegisterScope temps(this);
  Register scratch_base = temps.Acquire();
  // Adjust offset for two accesses and check if offset + 3 fits into int12.
  if (NeedAdjustBaseAndOffset(rs, OffsetAccessType::TWO_ACCESSES, NBYTES - 1)) {
    DCHECK(scratch_base != rd && scratch_base != rs.rm());
    AdjustBaseAndOffset(&source, scratch_base, OffsetAccessType::TWO_ACCESSES,
                        NBYTES - 1);
  }

  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (scratch_other == no_reg) {
    if (temps.hasAvailable()) {
      scratch_other = temps.Acquire();
    } else {
      push(t2);
      scratch_other = t2;
    }
  }

  DCHECK(scratch_other != rd && scratch_other != rs.rm() &&
         scratch_other != source.rm());

  sb(rd, source.rm(), source.offset());
  for (size_t i = 1; i <= (NBYTES - 1); i++) {
    srli(scratch_other, rd, i * 8);
    sb(scratch_other, source.rm(), source.offset() + i);
  }
  if (scratch_other == t2) {
    pop(t2);
  }
}

template <int NBYTES>
void TurboAssembler::UnalignedFStoreHelper(FPURegister frd,
                                           const MemOperand& rs,
                                           Register scratch) {
  DCHECK(NBYTES == 8 || NBYTES == 4);
  DCHECK_NE(scratch, rs.rm());
  if (NBYTES == 4) {
    fmv_x_w(scratch, frd);
  } else {
    fmv_x_d(scratch, frd);
  }
  UnalignedStoreHelper<NBYTES>(scratch, rs);
}

template <typename Reg_T, typename Func>
void TurboAssembler::AlignedLoadHelper(Reg_T target, const MemOperand& rs,
                                       Func generator) {
  MemOperand source = rs;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (NeedAdjustBaseAndOffset(source)) {
    Register scratch = temps.Acquire();
    DCHECK(scratch != rs.rm());
    AdjustBaseAndOffset(&source, scratch);
  }
  generator(target, source);
}

template <typename Reg_T, typename Func>
void TurboAssembler::AlignedStoreHelper(Reg_T value, const MemOperand& rs,
                                        Func generator) {
  MemOperand source = rs;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (NeedAdjustBaseAndOffset(source)) {
    Register scratch = temps.Acquire();
    // make sure scratch does not overwrite value
    if (std::is_same<Reg_T, Register>::value)
      DCHECK(scratch.code() != value.code());
    DCHECK(scratch != rs.rm());
    AdjustBaseAndOffset(&source, scratch);
  }
  generator(value, source);
}

void TurboAssembler::Ulw(Register rd, const MemOperand& rs) {
  UnalignedLoadHelper<4, true>(rd, rs);
}

void TurboAssembler::Ulwu(Register rd, const MemOperand& rs) {
  UnalignedLoadHelper<4, false>(rd, rs);
}

void TurboAssembler::Usw(Register rd, const MemOperand& rs) {
  UnalignedStoreHelper<4>(rd, rs);
}

void TurboAssembler::Ulh(Register rd, const MemOperand& rs) {
  UnalignedLoadHelper<2, true>(rd, rs);
}

void TurboAssembler::Ulhu(Register rd, const MemOperand& rs) {
  UnalignedLoadHelper<2, false>(rd, rs);
}

void TurboAssembler::Ush(Register rd, const MemOperand& rs) {
  UnalignedStoreHelper<2>(rd, rs);
}

void TurboAssembler::Uld(Register rd, const MemOperand& rs) {
  UnalignedLoadHelper<8, true>(rd, rs);
}

// Load consequent 32-bit word pair in 64-bit reg. and put first word in low
// bits,
// second word in high bits.
void MacroAssembler::LoadWordPair(Register rd, const MemOperand& rs) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Lwu(rd, rs);
  Lw(scratch, MemOperand(rs.rm(), rs.offset() + kSystemPointerSize / 2));
  slli(scratch, scratch, 32);
  Add64(rd, rd, scratch);
}

void TurboAssembler::Usd(Register rd, const MemOperand& rs) {
  UnalignedStoreHelper<8>(rd, rs);
}

// Do 64-bit store as two consequent 32-bit stores to unaligned address.
void MacroAssembler::StoreWordPair(Register rd, const MemOperand& rs) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Sw(rd, rs);
  srai(scratch, rd, 32);
  Sw(scratch, MemOperand(rs.rm(), rs.offset() + kSystemPointerSize / 2));
}

void TurboAssembler::ULoadFloat(FPURegister fd, const MemOperand& rs,
                                Register scratch) {
  DCHECK_NE(scratch, rs.rm());
  UnalignedFLoadHelper<4>(fd, rs, scratch);
}

void TurboAssembler::UStoreFloat(FPURegister fd, const MemOperand& rs,
                                 Register scratch) {
  DCHECK_NE(scratch, rs.rm());
  UnalignedFStoreHelper<4>(fd, rs, scratch);
}

void TurboAssembler::ULoadDouble(FPURegister fd, const MemOperand& rs,
                                 Register scratch) {
  DCHECK_NE(scratch, rs.rm());
  UnalignedFLoadHelper<8>(fd, rs, scratch);
}

void TurboAssembler::UStoreDouble(FPURegister fd, const MemOperand& rs,
                                  Register scratch) {
  DCHECK_NE(scratch, rs.rm());
  UnalignedFStoreHelper<8>(fd, rs, scratch);
}

void TurboAssembler::Lb(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    this->lb(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Lbu(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    this->lbu(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Sb(Register rd, const MemOperand& rs) {
  auto fn = [this](Register value, const MemOperand& source) {
    this->sb(value, source.rm(), source.offset());
  };
  AlignedStoreHelper(rd, rs, fn);
}

void TurboAssembler::Lh(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    this->lh(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Lhu(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    this->lhu(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Sh(Register rd, const MemOperand& rs) {
  auto fn = [this](Register value, const MemOperand& source) {
    this->sh(value, source.rm(), source.offset());
  };
  AlignedStoreHelper(rd, rs, fn);
}

void TurboAssembler::Lw(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((target.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint7(source.offset()) && ((source.offset() & 0x3) == 0)) {
      this->c_lw(target, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (target != zero_reg) &&
               is_uint8(source.offset()) && (source.rm() == sp) &&
               ((source.offset() & 0x3) == 0)) {
      this->c_lwsp(target, source.offset());
    } else {
      this->lw(target, source.rm(), source.offset());
    }
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Lwu(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    this->lwu(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Sw(Register rd, const MemOperand& rs) {
  auto fn = [this](Register value, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((value.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint7(source.offset()) && ((source.offset() & 0x3) == 0)) {
      this->c_sw(value, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (source.rm() == sp) &&
               is_uint8(source.offset()) && (((source.offset() & 0x3) == 0))) {
      this->c_swsp(value, source.offset());
    } else {
      this->sw(value, source.rm(), source.offset());
    }
  };
  AlignedStoreHelper(rd, rs, fn);
}

void TurboAssembler::Ld(Register rd, const MemOperand& rs) {
  auto fn = [this](Register target, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((target.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint8(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_ld(target, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (target != zero_reg) &&
               is_uint9(source.offset()) && (source.rm() == sp) &&
               ((source.offset() & 0x7) == 0)) {
      this->c_ldsp(target, source.offset());
    } else {
      this->ld(target, source.rm(), source.offset());
    }
  };
  AlignedLoadHelper(rd, rs, fn);
}

void TurboAssembler::Sd(Register rd, const MemOperand& rs) {
  auto fn = [this](Register value, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((value.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint8(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_sd(value, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (source.rm() == sp) &&
               is_uint9(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_sdsp(value, source.offset());
    } else {
      this->sd(value, source.rm(), source.offset());
    }
  };
  AlignedStoreHelper(rd, rs, fn);
}

void TurboAssembler::LoadFloat(FPURegister fd, const MemOperand& src) {
  auto fn = [this](FPURegister target, const MemOperand& source) {
    this->flw(target, source.rm(), source.offset());
  };
  AlignedLoadHelper(fd, src, fn);
}

void TurboAssembler::StoreFloat(FPURegister fs, const MemOperand& src) {
  auto fn = [this](FPURegister value, const MemOperand& source) {
    this->fsw(value, source.rm(), source.offset());
  };
  AlignedStoreHelper(fs, src, fn);
}

void TurboAssembler::LoadDouble(FPURegister fd, const MemOperand& src) {
  auto fn = [this](FPURegister target, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((target.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint8(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_fld(target, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (source.rm() == sp) &&
               is_uint9(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_fldsp(target, source.offset());
    } else {
      this->fld(target, source.rm(), source.offset());
    }
  };
  AlignedLoadHelper(fd, src, fn);
}

void TurboAssembler::StoreDouble(FPURegister fs, const MemOperand& src) {
  auto fn = [this](FPURegister value, const MemOperand& source) {
    if (FLAG_riscv_c_extension && ((value.code() & 0b11000) == 0b01000) &&
        ((source.rm().code() & 0b11000) == 0b01000) &&
        is_uint8(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_fsd(value, source.rm(), source.offset());
    } else if (FLAG_riscv_c_extension && (source.rm() == sp) &&
               is_uint9(source.offset()) && ((source.offset() & 0x7) == 0)) {
      this->c_fsdsp(value, source.offset());
    } else {
      this->fsd(value, source.rm(), source.offset());
    }
  };
  AlignedStoreHelper(fs, src, fn);
}

void TurboAssembler::Ll(Register rd, const MemOperand& rs) {
  bool is_one_instruction = rs.offset() == 0;
  if (is_one_instruction) {
    lr_w(false, false, rd, rs.rm());
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs.rm(), rs.offset());
    lr_w(false, false, rd, scratch);
  }
}

void TurboAssembler::Lld(Register rd, const MemOperand& rs) {
  bool is_one_instruction = rs.offset() == 0;
  if (is_one_instruction) {
    lr_d(false, false, rd, rs.rm());
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs.rm(), rs.offset());
    lr_d(false, false, rd, scratch);
  }
}

void TurboAssembler::Sc(Register rd, const MemOperand& rs) {
  bool is_one_instruction = rs.offset() == 0;
  if (is_one_instruction) {
    sc_w(false, false, rd, rs.rm(), rd);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs.rm(), rs.offset());
    sc_w(false, false, rd, scratch, rd);
  }
}

void TurboAssembler::Scd(Register rd, const MemOperand& rs) {
  bool is_one_instruction = rs.offset() == 0;
  if (is_one_instruction) {
    sc_d(false, false, rd, rs.rm(), rd);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs.rm(), rs.offset());
    sc_d(false, false, rd, scratch, rd);
  }
}

void TurboAssembler::li(Register dst, Handle<HeapObject> value,
                        RelocInfo::Mode rmode) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(dst, value);
    return;
  } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
    EmbeddedObjectIndex index = AddEmbeddedObject(value);
    DCHECK(is_uint32(index));
    li(dst, Operand(index, rmode));
  } else {
    DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
    li(dst, Operand(value.address(), rmode));
  }
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

static inline int InstrCountForLiLower32Bit(int64_t value) {
  int64_t Hi20 = ((value + 0x800) >> 12);
  int64_t Lo12 = value << 52 >> 52;
  if (Hi20 == 0 || Lo12 == 0) {
    return 1;
  }
  return 2;
}

int TurboAssembler::InstrCountForLi64Bit(int64_t value) {
  if (is_int32(value + 0x800)) {
    return InstrCountForLiLower32Bit(value);
  } else {
    return li_estimate(value);
  }
  UNREACHABLE();
  return INT_MAX;
}

void TurboAssembler::li_optimized(Register rd, Operand j, LiFlags mode) {
  DCHECK(!j.is_reg());
  DCHECK(!MustUseReg(j.rmode()));
  DCHECK(mode == OPTIMIZE_SIZE);
  Li(rd, j.immediate());
}

void TurboAssembler::li(Register rd, Operand j, LiFlags mode) {
  DCHECK(!j.is_reg());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (!MustUseReg(j.rmode()) && mode == OPTIMIZE_SIZE) {
    UseScratchRegisterScope temps(this);
    int count = li_estimate(j.immediate(), temps.hasAvailable());
    int reverse_count = li_estimate(~j.immediate(), temps.hasAvailable());
    if (FLAG_riscv_constant_pool && count >= 4 && reverse_count >= 4) {
      // Ld a Address from a constant pool.
      RecordEntry((uint64_t)j.immediate(), j.rmode());
      auipc(rd, 0);
      // Record a value into constant pool.
      ld(rd, rd, 0);
    } else {
      if ((count - reverse_count) > 1) {
        Li(rd, ~j.immediate());
        not_(rd, rd);
      } else {
        Li(rd, j.immediate());
      }
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
    li_ptr(rd, immediate);
  } else if (mode == ADDRESS_LOAD) {
    // We always need the same number of instructions as we may need to patch
    // this code to load another value which may need all 6 instructions.
    RecordRelocInfo(j.rmode());
    li_ptr(rd, j.immediate());
  } else {  // Always emit the same 48 bit instruction
            // sequence.
    li_ptr(rd, j.immediate());
  }
}

static RegList t_regs = {t0, t1, t2, t3, t4, t5, t6};
static RegList a_regs = {a0, a1, a2, a3, a4, a5, a6, a7};
static RegList s_regs = {s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

void TurboAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = regs.Count();
  int16_t stack_offset = num_to_push * kSystemPointerSize;

#define TEST_AND_PUSH_REG(reg)             \
  if (regs.has(reg)) {                     \
    stack_offset -= kSystemPointerSize;    \
    Sd(reg, MemOperand(sp, stack_offset)); \
    regs.clear(reg);                       \
  }

#define T_REGS(V) V(t6) V(t5) V(t4) V(t3) V(t2) V(t1) V(t0)
#define A_REGS(V) V(a7) V(a6) V(a5) V(a4) V(a3) V(a2) V(a1) V(a0)
#define S_REGS(V) \
  V(s11) V(s10) V(s9) V(s8) V(s7) V(s6) V(s5) V(s4) V(s3) V(s2) V(s1)

  Sub64(sp, sp, Operand(stack_offset));

  // Certain usage of MultiPush requires that registers are pushed onto the
  // stack in a particular: ra, fp, sp, gp, .... (basically in the decreasing
  // order of register numbers according to MIPS register numbers)
  TEST_AND_PUSH_REG(ra);
  TEST_AND_PUSH_REG(fp);
  TEST_AND_PUSH_REG(sp);
  TEST_AND_PUSH_REG(gp);
  TEST_AND_PUSH_REG(tp);
  if (!(regs & s_regs).is_empty()) {
    S_REGS(TEST_AND_PUSH_REG)
  }
  if (!(regs & a_regs).is_empty()) {
    A_REGS(TEST_AND_PUSH_REG)
  }
  if (!(regs & t_regs).is_empty()) {
    T_REGS(TEST_AND_PUSH_REG)
  }

  DCHECK(regs.is_empty());

#undef TEST_AND_PUSH_REG
#undef T_REGS
#undef A_REGS
#undef S_REGS
}

void TurboAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

#define TEST_AND_POP_REG(reg)              \
  if (regs.has(reg)) {                     \
    Ld(reg, MemOperand(sp, stack_offset)); \
    stack_offset += kSystemPointerSize;    \
    regs.clear(reg);                       \
  }

#define T_REGS(V) V(t0) V(t1) V(t2) V(t3) V(t4) V(t5) V(t6)
#define A_REGS(V) V(a0) V(a1) V(a2) V(a3) V(a4) V(a5) V(a6) V(a7)
#define S_REGS(V) \
  V(s1) V(s2) V(s3) V(s4) V(s5) V(s6) V(s7) V(s8) V(s9) V(s10) V(s11)

  // MultiPop pops from the stack in reverse order as MultiPush
  if (!(regs & t_regs).is_empty()) {
    T_REGS(TEST_AND_POP_REG)
  }
  if (!(regs & a_regs).is_empty()) {
    A_REGS(TEST_AND_POP_REG)
  }
  if (!(regs & s_regs).is_empty()) {
    S_REGS(TEST_AND_POP_REG)
  }
  TEST_AND_POP_REG(tp);
  TEST_AND_POP_REG(gp);
  TEST_AND_POP_REG(sp);
  TEST_AND_POP_REG(fp);
  TEST_AND_POP_REG(ra);

  DCHECK(regs.is_empty());

  addi(sp, sp, stack_offset);

#undef TEST_AND_POP_REG
#undef T_REGS
#undef S_REGS
#undef A_REGS
}

void TurboAssembler::MultiPushFPU(DoubleRegList regs) {
  int16_t num_to_push = regs.Count();
  int16_t stack_offset = num_to_push * kDoubleSize;

  Sub64(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs.bits() & (1 << i)) != 0) {
      stack_offset -= kDoubleSize;
      StoreDouble(FPURegister::from_code(i), MemOperand(sp, stack_offset));
    }
  }
}

void TurboAssembler::MultiPopFPU(DoubleRegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs.bits() & (1 << i)) != 0) {
      LoadDouble(FPURegister::from_code(i), MemOperand(sp, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addi(sp, sp, stack_offset);
}

void TurboAssembler::ExtractBits(Register rt, Register rs, uint16_t pos,
                                 uint16_t size, bool sign_extend) {
  DCHECK(pos < 64 && 0 < size && size <= 64 && 0 < pos + size &&
         pos + size <= 64);
  slli(rt, rs, 64 - (pos + size));
  if (sign_extend) {
    srai(rt, rt, 64 - size);
  } else {
    srli(rt, rt, 64 - size);
  }
}

void TurboAssembler::InsertBits(Register dest, Register source, Register pos,
                                int size) {
  DCHECK_LT(size, 64);
  UseScratchRegisterScope temps(this);
  Register mask = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register source_ = temps.Acquire();
  // Create a mask of the length=size.
  li(mask, 1);
  slli(mask, mask, size);
  addi(mask, mask, -1);
  and_(source_, mask, source);
  sll(source_, source_, pos);
  // Make a mask containing 0's. 0's start at "pos" with length=size.
  sll(mask, mask, pos);
  not_(mask, mask);
  // cut area for insertion of source.
  and_(dest, mask, dest);
  // insert source
  or_(dest, dest, source_);
}

void TurboAssembler::Neg_s(FPURegister fd, FPURegister fs) { fneg_s(fd, fs); }

void TurboAssembler::Neg_d(FPURegister fd, FPURegister fs) { fneg_d(fd, fs); }

void TurboAssembler::Cvt_d_uw(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_d_wu(fd, rs);
}

void TurboAssembler::Cvt_d_w(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_d_w(fd, rs);
}

void TurboAssembler::Cvt_d_ul(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_d_lu(fd, rs);
}

void TurboAssembler::Cvt_s_uw(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_s_wu(fd, rs);
}

void TurboAssembler::Cvt_s_w(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_s_w(fd, rs);
}

void TurboAssembler::Cvt_s_ul(FPURegister fd, Register rs) {
  // Convert rs to a FP value in fd.
  fcvt_s_lu(fd, rs);
}

template <typename CvtFunc>
void TurboAssembler::RoundFloatingPointToInteger(Register rd, FPURegister fs,
                                                 Register result,
                                                 CvtFunc fcvt_generator) {
  // Save csr_fflags to scratch & clear exception flags
  if (result.is_valid()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();

    int exception_flags = kInvalidOperation;
    csrrci(scratch, csr_fflags, exception_flags);

    // actual conversion instruction
    fcvt_generator(this, rd, fs);

    // check kInvalidOperation flag (out-of-range, NaN)
    // set result to 1 if normal, otherwise set result to 0 for abnormal
    frflags(result);
    andi(result, result, exception_flags);
    seqz(result, result);  // result <-- 1 (normal), result <-- 0 (abnormal)

    // restore csr_fflags
    csrw(csr_fflags, scratch);
  } else {
    // actual conversion instruction
    fcvt_generator(this, rd, fs);
  }
}

void TurboAssembler::Clear_if_nan_d(Register rd, FPURegister fs) {
  Label no_nan;
  feq_d(kScratchReg, fs, fs);
  bnez(kScratchReg, &no_nan);
  Move(rd, zero_reg);
  bind(&no_nan);
}

void TurboAssembler::Clear_if_nan_s(Register rd, FPURegister fs) {
  Label no_nan;
  feq_s(kScratchReg, fs, fs);
  bnez(kScratchReg, &no_nan);
  Move(rd, zero_reg);
  bind(&no_nan);
}

void TurboAssembler::Trunc_uw_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_wu_d(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_w_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_d(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_uw_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_wu_s(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_w_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_s(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_ul_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_lu_d(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_l_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_l_d(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_ul_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_lu_s(dst, src, RTZ);
      });
}

void TurboAssembler::Trunc_l_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_l_s(dst, src, RTZ);
      });
}

void TurboAssembler::Round_w_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_s(dst, src, RNE);
      });
}

void TurboAssembler::Round_w_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_d(dst, src, RNE);
      });
}

void TurboAssembler::Ceil_w_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_s(dst, src, RUP);
      });
}

void TurboAssembler::Ceil_w_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_d(dst, src, RUP);
      });
}

void TurboAssembler::Floor_w_s(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_s(dst, src, RDN);
      });
}

void TurboAssembler::Floor_w_d(Register rd, FPURegister fs, Register result) {
  RoundFloatingPointToInteger(
      rd, fs, result, [](TurboAssembler* tasm, Register dst, FPURegister src) {
        tasm->fcvt_w_d(dst, src, RDN);
      });
}

// According to JS ECMA specification, for floating-point round operations, if
// the input is NaN, +/-infinity, or +/-0, the same input is returned as the
// rounded result; this differs from behavior of RISCV fcvt instructions (which
// round out-of-range values to the nearest max or min value), therefore special
// handling is needed by NaN, +/-Infinity, +/-0
template <typename F>
void TurboAssembler::RoundHelper(FPURegister dst, FPURegister src,
                                 FPURegister fpu_scratch, RoundingMode frm) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.Acquire();

  DCHECK((std::is_same<float, F>::value) || (std::is_same<double, F>::value));
  // Need at least two FPRs, so check against dst == src == fpu_scratch
  DCHECK(!(dst == src && dst == fpu_scratch));

  const int kFloatMantissaBits =
      sizeof(F) == 4 ? kFloat32MantissaBits : kFloat64MantissaBits;
  const int kFloatExponentBits =
      sizeof(F) == 4 ? kFloat32ExponentBits : kFloat64ExponentBits;
  const int kFloatExponentBias =
      sizeof(F) == 4 ? kFloat32ExponentBias : kFloat64ExponentBias;
  Label done;

  {
    UseScratchRegisterScope temps2(this);
    Register scratch = temps2.Acquire();
    // extract exponent value of the source floating-point to scratch
    if (std::is_same<F, double>::value) {
      fmv_x_d(scratch, src);
    } else {
      fmv_x_w(scratch, src);
    }
    ExtractBits(scratch2, scratch, kFloatMantissaBits, kFloatExponentBits);
  }

  // if src is NaN/+-Infinity/+-Zero or if the exponent is larger than # of bits
  // in mantissa, the result is the same as src, so move src to dest  (to avoid
  // generating another branch)
  if (dst != src) {
    if (std::is_same<F, double>::value) {
      fmv_d(dst, src);
    } else {
      fmv_s(dst, src);
    }
  }
  {
    Label not_NaN;
    UseScratchRegisterScope temps2(this);
    Register scratch = temps2.Acquire();
    // According to the wasm spec
    // (https://webassembly.github.io/spec/core/exec/numerics.html#aux-nans)
    // if input is canonical NaN, then output is canonical NaN, and if input is
    // any other NaN, then output is any NaN with most significant bit of
    // payload is 1. In RISC-V, feq_d will set scratch to 0 if src is a NaN. If
    // src is not a NaN, branch to the label and do nothing, but if it is,
    // fmin_d will set dst to the canonical NaN.
    if (std::is_same<F, double>::value) {
      feq_d(scratch, src, src);
      bnez(scratch, &not_NaN);
      fmin_d(dst, src, src);
    } else {
      feq_s(scratch, src, src);
      bnez(scratch, &not_NaN);
      fmin_s(dst, src, src);
    }
    bind(&not_NaN);
  }

  // If real exponent (i.e., scratch2 - kFloatExponentBias) is greater than
  // kFloat32MantissaBits, it means the floating-point value has no fractional
  // part, thus the input is already rounded, jump to done. Note that, NaN and
  // Infinity in floating-point representation sets maximal exponent value, so
  // they also satisfy (scratch2 - kFloatExponentBias >= kFloatMantissaBits),
  // and JS round semantics specify that rounding of NaN (Infinity) returns NaN
  // (Infinity), so NaN and Infinity are considered rounded value too.
  Branch(&done, greater_equal, scratch2,
         Operand(kFloatExponentBias + kFloatMantissaBits));

  // Actual rounding is needed along this path

  // old_src holds the original input, needed for the case of src == dst
  FPURegister old_src = src;
  if (src == dst) {
    DCHECK(fpu_scratch != dst);
    Move(fpu_scratch, src);
    old_src = fpu_scratch;
  }

  // Since only input whose real exponent value is less than kMantissaBits
  // (i.e., 23 or 52-bits) falls into this path, the value range of the input
  // falls into that of 23- or 53-bit integers. So we round the input to integer
  // values, then convert them back to floating-point.
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (std::is_same<F, double>::value) {
      fcvt_l_d(scratch, src, frm);
      fcvt_d_l(dst, scratch, frm);
    } else {
      fcvt_w_s(scratch, src, frm);
      fcvt_s_w(dst, scratch, frm);
    }
  }
  // A special handling is needed if the input is a very small positive/negative
  // number that rounds to zero. JS semantics requires that the rounded result
  // retains the sign of the input, so a very small positive (negative)
  // floating-point number should be rounded to positive (negative) 0.
  // Therefore, we use sign-bit injection to produce +/-0 correctly. Instead of
  // testing for zero w/ a branch, we just insert sign-bit for everyone on this
  // path (this is where old_src is needed)
  if (std::is_same<F, double>::value) {
    fsgnj_d(dst, dst, old_src);
  } else {
    fsgnj_s(dst, dst, old_src);
  }

  bind(&done);
}

// According to JS ECMA specification, for floating-point round operations, if
// the input is NaN, +/-infinity, or +/-0, the same input is returned as the
// rounded result; this differs from behavior of RISCV fcvt instructions (which
// round out-of-range values to the nearest max or min value), therefore special
// handling is needed by NaN, +/-Infinity, +/-0
template <typename F>
void TurboAssembler::RoundHelper(VRegister dst, VRegister src, Register scratch,
                                 VRegister v_scratch, RoundingMode frm) {
  VU.set(scratch, std::is_same<F, float>::value ? E32 : E64, m1);
  // if src is NaN/+-Infinity/+-Zero or if the exponent is larger than # of bits
  // in mantissa, the result is the same as src, so move src to dest  (to avoid
  // generating another branch)

  // If real exponent (i.e., scratch2 - kFloatExponentBias) is greater than
  // kFloat32MantissaBits, it means the floating-point value has no fractional
  // part, thus the input is already rounded, jump to done. Note that, NaN and
  // Infinity in floating-point representation sets maximal exponent value, so
  // they also satisfy (scratch2 - kFloatExponentBias >= kFloatMantissaBits),
  // and JS round semantics specify that rounding of NaN (Infinity) returns NaN
  // (Infinity), so NaN and Infinity are considered rounded value too.
  const int kFloatMantissaBits =
      sizeof(F) == 4 ? kFloat32MantissaBits : kFloat64MantissaBits;
  const int kFloatExponentBits =
      sizeof(F) == 4 ? kFloat32ExponentBits : kFloat64ExponentBits;
  const int kFloatExponentBias =
      sizeof(F) == 4 ? kFloat32ExponentBias : kFloat64ExponentBias;

  // slli(rt, rs, 64 - (pos + size));
  // if (sign_extend) {
  //   srai(rt, rt, 64 - size);
  // } else {
  //   srli(rt, rt, 64 - size);
  // }

  li(scratch, 64 - kFloatMantissaBits - kFloatExponentBits);
  vsll_vx(v_scratch, src, scratch);
  li(scratch, 64 - kFloatExponentBits);
  vsrl_vx(v_scratch, v_scratch, scratch);
  li(scratch, kFloatExponentBias + kFloatMantissaBits);
  vmslt_vx(v0, v_scratch, scratch);

  VU.set(frm);
  vmv_vv(dst, src);
  if (dst == src) {
    vmv_vv(v_scratch, src);
  }
  vfcvt_x_f_v(dst, src, MaskType::Mask);
  vfcvt_f_x_v(dst, dst, MaskType::Mask);

  // A special handling is needed if the input is a very small positive/negative
  // number that rounds to zero. JS semantics requires that the rounded result
  // retains the sign of the input, so a very small positive (negative)
  // floating-point number should be rounded to positive (negative) 0.
  if (dst == src) {
    vfsngj_vv(dst, dst, v_scratch);
  } else {
    vfsngj_vv(dst, dst, src);
  }
}

void TurboAssembler::Ceil_f(VRegister vdst, VRegister vsrc, Register scratch,
                            VRegister v_scratch) {
  RoundHelper<float>(vdst, vsrc, scratch, v_scratch, RUP);
}

void TurboAssembler::Ceil_d(VRegister vdst, VRegister vsrc, Register scratch,
                            VRegister v_scratch) {
  RoundHelper<double>(vdst, vsrc, scratch, v_scratch, RUP);
}

void TurboAssembler::Floor_f(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<float>(vdst, vsrc, scratch, v_scratch, RDN);
}

void TurboAssembler::Floor_d(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<double>(vdst, vsrc, scratch, v_scratch, RDN);
}

void TurboAssembler::Trunc_d(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<double>(vdst, vsrc, scratch, v_scratch, RTZ);
}

void TurboAssembler::Trunc_f(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<float>(vdst, vsrc, scratch, v_scratch, RTZ);
}

void TurboAssembler::Round_f(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<float>(vdst, vsrc, scratch, v_scratch, RNE);
}

void TurboAssembler::Round_d(VRegister vdst, VRegister vsrc, Register scratch,
                             VRegister v_scratch) {
  RoundHelper<double>(vdst, vsrc, scratch, v_scratch, RNE);
}

void TurboAssembler::Floor_d_d(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<double>(dst, src, fpu_scratch, RDN);
}

void TurboAssembler::Ceil_d_d(FPURegister dst, FPURegister src,
                              FPURegister fpu_scratch) {
  RoundHelper<double>(dst, src, fpu_scratch, RUP);
}

void TurboAssembler::Trunc_d_d(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<double>(dst, src, fpu_scratch, RTZ);
}

void TurboAssembler::Round_d_d(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<double>(dst, src, fpu_scratch, RNE);
}

void TurboAssembler::Floor_s_s(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<float>(dst, src, fpu_scratch, RDN);
}

void TurboAssembler::Ceil_s_s(FPURegister dst, FPURegister src,
                              FPURegister fpu_scratch) {
  RoundHelper<float>(dst, src, fpu_scratch, RUP);
}

void TurboAssembler::Trunc_s_s(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<float>(dst, src, fpu_scratch, RTZ);
}

void TurboAssembler::Round_s_s(FPURegister dst, FPURegister src,
                               FPURegister fpu_scratch) {
  RoundHelper<float>(dst, src, fpu_scratch, RNE);
}

void MacroAssembler::Madd_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft) {
  fmadd_s(fd, fs, ft, fr);
}

void MacroAssembler::Madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft) {
  fmadd_d(fd, fs, ft, fr);
}

void MacroAssembler::Msub_s(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft) {
  fmsub_s(fd, fs, ft, fr);
}

void MacroAssembler::Msub_d(FPURegister fd, FPURegister fr, FPURegister fs,
                            FPURegister ft) {
  fmsub_d(fd, fs, ft, fr);
}

void TurboAssembler::CompareF32(Register rd, FPUCondition cc, FPURegister cmp1,
                                FPURegister cmp2) {
  switch (cc) {
    case EQ:
      feq_s(rd, cmp1, cmp2);
      break;
    case NE:
      feq_s(rd, cmp1, cmp2);
      NegateBool(rd, rd);
      break;
    case LT:
      flt_s(rd, cmp1, cmp2);
      break;
    case GE:
      fle_s(rd, cmp2, cmp1);
      break;
    case LE:
      fle_s(rd, cmp1, cmp2);
      break;
    case GT:
      flt_s(rd, cmp2, cmp1);
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::CompareF64(Register rd, FPUCondition cc, FPURegister cmp1,
                                FPURegister cmp2) {
  switch (cc) {
    case EQ:
      feq_d(rd, cmp1, cmp2);
      break;
    case NE:
      feq_d(rd, cmp1, cmp2);
      NegateBool(rd, rd);
      break;
    case LT:
      flt_d(rd, cmp1, cmp2);
      break;
    case GE:
      fle_d(rd, cmp2, cmp1);
      break;
    case LE:
      fle_d(rd, cmp1, cmp2);
      break;
    case GT:
      flt_d(rd, cmp2, cmp1);
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::CompareIsNotNanF32(Register rd, FPURegister cmp1,
                                        FPURegister cmp2) {
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = temps.Acquire();

  feq_s(rd, cmp1, cmp1);       // rd <- !isNan(cmp1)
  feq_s(scratch, cmp2, cmp2);  // scratch <- !isNaN(cmp2)
  And(rd, rd, scratch);        // rd <- !isNan(cmp1) && !isNan(cmp2)
}

void TurboAssembler::CompareIsNotNanF64(Register rd, FPURegister cmp1,
                                        FPURegister cmp2) {
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = temps.Acquire();

  feq_d(rd, cmp1, cmp1);       // rd <- !isNan(cmp1)
  feq_d(scratch, cmp2, cmp2);  // scratch <- !isNaN(cmp2)
  And(rd, rd, scratch);        // rd <- !isNan(cmp1) && !isNan(cmp2)
}

void TurboAssembler::CompareIsNanF32(Register rd, FPURegister cmp1,
                                     FPURegister cmp2) {
  CompareIsNotNanF32(rd, cmp1, cmp2);  // rd <- !isNan(cmp1) && !isNan(cmp2)
  Xor(rd, rd, 1);                      // rd <- isNan(cmp1) || isNan(cmp2)
}

void TurboAssembler::CompareIsNanF64(Register rd, FPURegister cmp1,
                                     FPURegister cmp2) {
  CompareIsNotNanF64(rd, cmp1, cmp2);  // rd <- !isNan(cmp1) && !isNan(cmp2)
  Xor(rd, rd, 1);                      // rd <- isNan(cmp1) || isNan(cmp2)
}

void TurboAssembler::BranchTrueShortF(Register rs, Label* target) {
  Branch(target, not_equal, rs, Operand(zero_reg));
}

void TurboAssembler::BranchFalseShortF(Register rs, Label* target) {
  Branch(target, equal, rs, Operand(zero_reg));
}

void TurboAssembler::BranchTrueF(Register rs, Label* target) {
  bool long_branch =
      target->is_bound() ? !is_near(target) : is_trampoline_emitted();
  if (long_branch) {
    Label skip;
    BranchFalseShortF(rs, &skip);
    BranchLong(target);
    bind(&skip);
  } else {
    BranchTrueShortF(rs, target);
  }
}

void TurboAssembler::BranchFalseF(Register rs, Label* target) {
  bool long_branch =
      target->is_bound() ? !is_near(target) : is_trampoline_emitted();
  if (long_branch) {
    Label skip;
    BranchTrueShortF(rs, &skip);
    BranchLong(target);
    bind(&skip);
  } else {
    BranchFalseShortF(rs, target);
  }
}

void TurboAssembler::InsertHighWordF64(FPURegister dst, Register src_high) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);

  DCHECK(src_high != scratch2 && src_high != scratch);

  fmv_x_d(scratch, dst);
  slli(scratch2, src_high, 32);
  slli(scratch, scratch, 32);
  srli(scratch, scratch, 32);
  or_(scratch, scratch, scratch2);
  fmv_d_x(dst, scratch);
}

void TurboAssembler::InsertLowWordF64(FPURegister dst, Register src_low) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);

  DCHECK(src_low != scratch && src_low != scratch2);
  fmv_x_d(scratch, dst);
  slli(scratch2, src_low, 32);
  srli(scratch2, scratch2, 32);
  srli(scratch, scratch, 32);
  slli(scratch, scratch, 32);
  or_(scratch, scratch, scratch2);
  fmv_d_x(dst, scratch);
}

void TurboAssembler::LoadFPRImmediate(FPURegister dst, uint32_t src) {
  // Handle special values first.
  if (src == bit_cast<uint32_t>(0.0f) && has_single_zero_reg_set_) {
    if (dst != kDoubleRegZero) fmv_s(dst, kDoubleRegZero);
  } else if (src == bit_cast<uint32_t>(-0.0f) && has_single_zero_reg_set_) {
    Neg_s(dst, kDoubleRegZero);
  } else {
    if (dst == kDoubleRegZero) {
      DCHECK(src == bit_cast<uint32_t>(0.0f));
      fmv_w_x(dst, zero_reg);
      has_single_zero_reg_set_ = true;
      has_double_zero_reg_set_ = false;
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(static_cast<int32_t>(src)));
      fmv_w_x(dst, scratch);
    }
  }
}

void TurboAssembler::LoadFPRImmediate(FPURegister dst, uint64_t src) {
  // Handle special values first.
  if (src == bit_cast<uint64_t>(0.0) && has_double_zero_reg_set_) {
    if (dst != kDoubleRegZero) fmv_d(dst, kDoubleRegZero);
  } else if (src == bit_cast<uint64_t>(-0.0) && has_double_zero_reg_set_) {
    Neg_d(dst, kDoubleRegZero);
  } else {
    if (dst == kDoubleRegZero) {
      DCHECK(src == bit_cast<uint64_t>(0.0));
      fmv_d_x(dst, zero_reg);
      has_double_zero_reg_set_ = true;
      has_single_zero_reg_set_ = false;
    } else {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, Operand(src));
      fmv_d_x(dst, scratch);
    }
  }
}

void TurboAssembler::CompareI(Register rd, Register rs, const Operand& rt,
                              Condition cond) {
  switch (cond) {
    case eq:
      Seq(rd, rs, rt);
      break;
    case ne:
      Sne(rd, rs, rt);
      break;

    // Signed comparison.
    case greater:
      Sgt(rd, rs, rt);
      break;
    case greater_equal:
      Sge(rd, rs, rt);  // rs >= rt
      break;
    case less:
      Slt(rd, rs, rt);  // rs < rt
      break;
    case less_equal:
      Sle(rd, rs, rt);  // rs <= rt
      break;

    // Unsigned comparison.
    case Ugreater:
      Sgtu(rd, rs, rt);  // rs > rt
      break;
    case Ugreater_equal:
      Sgeu(rd, rs, rt);  // rs >= rt
      break;
    case Uless:
      Sltu(rd, rs, rt);  // rs < rt
      break;
    case Uless_equal:
      Sleu(rd, rs, rt);  // rs <= rt
      break;
    case cc_always:
      UNREACHABLE();
    default:
      UNREACHABLE();
  }
}

// dest <- (condition != 0 ? zero : dest)
void TurboAssembler::LoadZeroIfConditionNotZero(Register dest,
                                                Register condition) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  seqz(scratch, condition);
  // neg + and may be more efficient than mul(dest, dest, scratch)
  neg(scratch, scratch);  // 0 is still 0, 1 becomes all 1s
  and_(dest, dest, scratch);
}

// dest <- (condition == 0 ? 0 : dest)
void TurboAssembler::LoadZeroIfConditionZero(Register dest,
                                             Register condition) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  snez(scratch, condition);
  //  neg + and may be more efficient than mul(dest, dest, scratch);
  neg(scratch, scratch);  // 0 is still 0, 1 becomes all 1s
  and_(dest, dest, scratch);
}

void TurboAssembler::Clz32(Register rd, Register xx) {
  // 32 bit unsigned in lower word: count number of leading zeros.
  //  int n = 32;
  //  unsigned y;

  //  y = x >>16; if (y != 0) { n = n -16; x = y; }
  //  y = x >> 8; if (y != 0) { n = n - 8; x = y; }
  //  y = x >> 4; if (y != 0) { n = n - 4; x = y; }
  //  y = x >> 2; if (y != 0) { n = n - 2; x = y; }
  //  y = x >> 1; if (y != 0) {rd = n - 2; return;}
  //  rd = n - x;

  Label L0, L1, L2, L3, L4;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register x = rd;
  Register y = temps.Acquire();
  Register n = temps.Acquire();
  DCHECK(xx != y && xx != n);
  Move(x, xx);
  li(n, Operand(32));
  srliw(y, x, 16);
  BranchShort(&L0, eq, y, Operand(zero_reg));
  Move(x, y);
  addiw(n, n, -16);
  bind(&L0);
  srliw(y, x, 8);
  BranchShort(&L1, eq, y, Operand(zero_reg));
  addiw(n, n, -8);
  Move(x, y);
  bind(&L1);
  srliw(y, x, 4);
  BranchShort(&L2, eq, y, Operand(zero_reg));
  addiw(n, n, -4);
  Move(x, y);
  bind(&L2);
  srliw(y, x, 2);
  BranchShort(&L3, eq, y, Operand(zero_reg));
  addiw(n, n, -2);
  Move(x, y);
  bind(&L3);
  srliw(y, x, 1);
  subw(rd, n, x);
  BranchShort(&L4, eq, y, Operand(zero_reg));
  addiw(rd, n, -2);
  bind(&L4);
}

void TurboAssembler::Clz64(Register rd, Register xx) {
  // 64 bit: count number of leading zeros.
  //  int n = 64;
  //  unsigned y;

  //  y = x >>32; if (y != 0) { n = n - 32; x = y; }
  //  y = x >>16; if (y != 0) { n = n - 16; x = y; }
  //  y = x >> 8; if (y != 0) { n = n - 8; x = y; }
  //  y = x >> 4; if (y != 0) { n = n - 4; x = y; }
  //  y = x >> 2; if (y != 0) { n = n - 2; x = y; }
  //  y = x >> 1; if (y != 0) {rd = n - 2; return;}
  //  rd = n - x;

  Label L0, L1, L2, L3, L4, L5;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register x = rd;
  Register y = temps.Acquire();
  Register n = temps.Acquire();
  DCHECK(xx != y && xx != n);
  Move(x, xx);
  li(n, Operand(64));
  srli(y, x, 32);
  BranchShort(&L0, eq, y, Operand(zero_reg));
  addiw(n, n, -32);
  Move(x, y);
  bind(&L0);
  srli(y, x, 16);
  BranchShort(&L1, eq, y, Operand(zero_reg));
  addiw(n, n, -16);
  Move(x, y);
  bind(&L1);
  srli(y, x, 8);
  BranchShort(&L2, eq, y, Operand(zero_reg));
  addiw(n, n, -8);
  Move(x, y);
  bind(&L2);
  srli(y, x, 4);
  BranchShort(&L3, eq, y, Operand(zero_reg));
  addiw(n, n, -4);
  Move(x, y);
  bind(&L3);
  srli(y, x, 2);
  BranchShort(&L4, eq, y, Operand(zero_reg));
  addiw(n, n, -2);
  Move(x, y);
  bind(&L4);
  srli(y, x, 1);
  subw(rd, n, x);
  BranchShort(&L5, eq, y, Operand(zero_reg));
  addiw(rd, n, -2);
  bind(&L5);
}

void TurboAssembler::Ctz32(Register rd, Register rs) {
  // Convert trailing zeroes to trailing ones, and bits to their left
  // to zeroes.

  BlockTrampolinePoolScope block_trampoline_pool(this);
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs, -1);
    Xor(rd, scratch, rs);
    And(rd, rd, scratch);
    // Count number of leading zeroes.
  }
  Clz32(rd, rd);
  {
    // Subtract number of leading zeroes from 32 to get number of trailing
    // ones. Remember that the trailing ones were formerly trailing zeroes.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, 32);
    Sub32(rd, scratch, rd);
  }
}

void TurboAssembler::Ctz64(Register rd, Register rs) {
  // Convert trailing zeroes to trailing ones, and bits to their left
  // to zeroes.

  BlockTrampolinePoolScope block_trampoline_pool(this);
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add64(scratch, rs, -1);
    Xor(rd, scratch, rs);
    And(rd, rd, scratch);
    // Count number of leading zeroes.
  }
  Clz64(rd, rd);
  {
    // Subtract number of leading zeroes from 64 to get number of trailing
    // ones. Remember that the trailing ones were formerly trailing zeroes.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, 64);
    Sub64(rd, scratch, rd);
  }
}

void TurboAssembler::Popcnt32(Register rd, Register rs, Register scratch) {
  DCHECK_NE(scratch, rs);
  DCHECK_NE(scratch, rd);
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
  // There are algorithms which are faster in the cases where very few
  // bits are set but the algorithm here attempts to minimize the total
  // number of instructions executed even when a large number of bits
  // are set.
  // The number of instruction is 20.
  // uint32_t B0 = 0x55555555;     // (T)~(T)0/3
  // uint32_t B1 = 0x33333333;     // (T)~(T)0/15*3
  // uint32_t B2 = 0x0F0F0F0F;     // (T)~(T)0/255*15
  // uint32_t value = 0x01010101;  // (T)~(T)0/255

  uint32_t shift = 24;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch2 = temps.Acquire();
  Register value = temps.Acquire();
  DCHECK((rd != value) && (rs != value));
  li(value, 0x01010101);     // value = 0x01010101;
  li(scratch2, 0x55555555);  // B0 = 0x55555555;
  Srl32(scratch, rs, 1);
  And(scratch, scratch, scratch2);
  Sub32(scratch, rs, scratch);
  li(scratch2, 0x33333333);  // B1 = 0x33333333;
  slli(rd, scratch2, 4);
  or_(scratch2, scratch2, rd);
  And(rd, scratch, scratch2);
  Srl32(scratch, scratch, 2);
  And(scratch, scratch, scratch2);
  Add32(scratch, rd, scratch);
  srliw(rd, scratch, 4);
  Add32(rd, rd, scratch);
  li(scratch2, 0xF);
  Mul32(scratch2, value, scratch2);  // B2 = 0x0F0F0F0F;
  And(rd, rd, scratch2);
  Mul32(rd, rd, value);
  Srl32(rd, rd, shift);
}

void TurboAssembler::Popcnt64(Register rd, Register rs, Register scratch) {
  DCHECK_NE(scratch, rs);
  DCHECK_NE(scratch, rd);
  // uint64_t B0 = 0x5555555555555555l;     // (T)~(T)0/3
  // uint64_t B1 = 0x3333333333333333l;     // (T)~(T)0/15*3
  // uint64_t B2 = 0x0F0F0F0F0F0F0F0Fl;     // (T)~(T)0/255*15
  // uint64_t value = 0x0101010101010101l;  // (T)~(T)0/255
  // uint64_t shift = 24;                   // (sizeof(T) - 1) * BITS_PER_BYTE

  uint64_t shift = 24;
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch2 = temps.Acquire();
  Register value = temps.Acquire();
  DCHECK((rd != value) && (rs != value));
  li(value, 0x1111111111111111l);  // value = 0x1111111111111111l;
  li(scratch2, 5);
  Mul64(scratch2, value, scratch2);  // B0 = 0x5555555555555555l;
  Srl64(scratch, rs, 1);
  And(scratch, scratch, scratch2);
  Sub64(scratch, rs, scratch);
  li(scratch2, 3);
  Mul64(scratch2, value, scratch2);  // B1 = 0x3333333333333333l;
  And(rd, scratch, scratch2);
  Srl64(scratch, scratch, 2);
  And(scratch, scratch, scratch2);
  Add64(scratch, rd, scratch);
  Srl64(rd, scratch, 4);
  Add64(rd, rd, scratch);
  li(scratch2, 0xF);
  li(value, 0x0101010101010101l);    // value = 0x0101010101010101l;
  Mul64(scratch2, value, scratch2);  // B2 = 0x0F0F0F0F0F0F0F0Fl;
  And(rd, rd, scratch2);
  Mul64(rd, rd, value);
  srli(rd, rd, 32 + shift);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  // if scratch == 1, exception happens during truncation
  Trunc_w_d(result, double_input, scratch);
  // If we had no exceptions (i.e., scratch==1) we are done.
  Branch(done, eq, scratch, Operand(1));
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DoubleRegister double_input,
                                       StubCallMode stub_mode) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub
  // instead.
  push(ra);
  Sub64(sp, sp, Operand(kDoubleSize));  // Put input on stack.
  fsd(double_input, sp, 0);

  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }
  ld(result, sp, 0);

  Add64(sp, sp, Operand(kDoubleSize));
  pop(ra);

  bind(&done);
}

// BRANCH_ARGS_CHECK checks that conditional jump arguments are correct.
#define BRANCH_ARGS_CHECK(cond, rs, rt)                                  \
  DCHECK((cond == cc_always && rs == zero_reg && rt.rm() == zero_reg) || \
         (cond != cc_always && (rs != zero_reg || rt.rm() != zero_reg)))

void TurboAssembler::Branch(int32_t offset) {
  DCHECK(is_int21(offset));
  BranchShort(offset);
}

void TurboAssembler::Branch(int32_t offset, Condition cond, Register rs,
                            const Operand& rt, Label::Distance near_jump) {
  bool is_near = BranchShortCheck(offset, nullptr, cond, rs, rt);
  DCHECK(is_near);
  USE(is_near);
}

void TurboAssembler::Branch(Label* L) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchShort(L);
    } else {
      BranchLong(L);
    }
  } else {
    if (is_trampoline_emitted()) {
      BranchLong(L);
    } else {
      BranchShort(L);
    }
  }
}

void TurboAssembler::Branch(Label* L, Condition cond, Register rs,
                            const Operand& rt, Label::Distance near_jump) {
  if (L->is_bound()) {
    if (!BranchShortCheck(0, L, cond, rs, rt)) {
      if (cond != cc_always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L);
        bind(&skip);
      } else {
        BranchLong(L);
        EmitConstPoolWithJumpIfNeeded();
      }
    }
  } else {
    if (is_trampoline_emitted() && near_jump == Label::Distance::kFar) {
      if (cond != cc_always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L);
        bind(&skip);
      } else {
        BranchLong(L);
        EmitConstPoolWithJumpIfNeeded();
      }
    } else {
      BranchShort(L, cond, rs, rt);
    }
  }
}

void TurboAssembler::Branch(Label* L, Condition cond, Register rs,
                            RootIndex index) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  LoadRoot(scratch, index);
  Branch(L, cond, rs, Operand(scratch));
}

void TurboAssembler::BranchShortHelper(int32_t offset, Label* L) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset21);
  j(offset);
}

void TurboAssembler::BranchShort(int32_t offset) {
  DCHECK(is_int21(offset));
  BranchShortHelper(offset, nullptr);
}

void TurboAssembler::BranchShort(Label* L) { BranchShortHelper(0, L); }

int32_t TurboAssembler::GetOffset(int32_t offset, Label* L, OffsetSize bits) {
  if (L) {
    offset = branch_offset_helper(L, bits);
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

bool TurboAssembler::BranchShortHelper(int32_t offset, Label* L, Condition cond,
                                       Register rs, const Operand& rt) {
  DCHECK(L == nullptr || offset == 0);
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = no_reg;
  if (!rt.is_reg()) {
    scratch = temps.Acquire();
    li(scratch, rt);
  } else {
    scratch = rt.rm();
  }
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    switch (cond) {
      case cc_always:
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
        j(offset);
        EmitConstPoolWithJumpIfNeeded();
        break;
      case eq:
        // rs == rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          beq(rs, scratch, offset);
        }
        break;
      case ne:
        // rs != rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bne(rs, scratch, offset);
        }
        break;

      // Signed comparison.
      case greater:
        // rs > rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bgt(rs, scratch, offset);
        }
        break;
      case greater_equal:
        // rs >= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bge(rs, scratch, offset);
        }
        break;
      case less:
        // rs < rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          blt(rs, scratch, offset);
        }
        break;
      case less_equal:
        // rs <= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          ble(rs, scratch, offset);
        }
        break;

      // Unsigned comparison.
      case Ugreater:
        // rs > rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bgtu(rs, scratch, offset);
        }
        break;
      case Ugreater_equal:
        // rs >= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bgeu(rs, scratch, offset);
        }
        break;
      case Uless:
        // rs < rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bltu(rs, scratch, offset);
        }
        break;
      case Uless_equal:
        // rs <= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bleu(rs, scratch, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }

  CheckTrampolinePoolQuick(1);
  return true;
}

bool TurboAssembler::BranchShortCheck(int32_t offset, Label* L, Condition cond,
                                      Register rs, const Operand& rt) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  if (!L) {
    DCHECK(is_int13(offset));
    return BranchShortHelper(offset, nullptr, cond, rs, rt);
  } else {
    DCHECK_EQ(offset, 0);
    return BranchShortHelper(0, L, cond, rs, rt);
  }
}

void TurboAssembler::BranchShort(int32_t offset, Condition cond, Register rs,
                                 const Operand& rt) {
  BranchShortCheck(offset, nullptr, cond, rs, rt);
}

void TurboAssembler::BranchShort(Label* L, Condition cond, Register rs,
                                 const Operand& rt) {
  BranchShortCheck(0, L, cond, rs, rt);
}

void TurboAssembler::BranchAndLink(int32_t offset) {
  BranchAndLinkShort(offset);
}

void TurboAssembler::BranchAndLink(int32_t offset, Condition cond, Register rs,
                                   const Operand& rt) {
  bool is_near = BranchAndLinkShortCheck(offset, nullptr, cond, rs, rt);
  DCHECK(is_near);
  USE(is_near);
}

void TurboAssembler::BranchAndLink(Label* L) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchAndLinkShort(L);
    } else {
      BranchAndLinkLong(L);
    }
  } else {
    if (is_trampoline_emitted()) {
      BranchAndLinkLong(L);
    } else {
      BranchAndLinkShort(L);
    }
  }
}

void TurboAssembler::BranchAndLink(Label* L, Condition cond, Register rs,
                                   const Operand& rt) {
  if (L->is_bound()) {
    if (!BranchAndLinkShortCheck(0, L, cond, rs, rt)) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      BranchAndLinkLong(L);
      bind(&skip);
    }
  } else {
    if (is_trampoline_emitted()) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      BranchAndLinkLong(L);
      bind(&skip);
    } else {
      BranchAndLinkShortCheck(0, L, cond, rs, rt);
    }
  }
}

void TurboAssembler::BranchAndLinkShortHelper(int32_t offset, Label* L) {
  DCHECK(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset21);
  jal(offset);
}

void TurboAssembler::BranchAndLinkShort(int32_t offset) {
  DCHECK(is_int21(offset));
  BranchAndLinkShortHelper(offset, nullptr);
}

void TurboAssembler::BranchAndLinkShort(Label* L) {
  BranchAndLinkShortHelper(0, L);
}

// Pre r6 we need to use a bgezal or bltzal, but they can't be used directly
// with the slt instructions. We could use sub or add instead but we would miss
// overflow cases, so we keep slt and add an intermediate third instruction.
bool TurboAssembler::BranchAndLinkShortHelper(int32_t offset, Label* L,
                                              Condition cond, Register rs,
                                              const Operand& rt) {
  DCHECK(L == nullptr || offset == 0);
  if (!is_near(L, OffsetSize::kOffset21)) return false;

  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);

  if (cond == cc_always) {
    offset = GetOffset(offset, L, OffsetSize::kOffset21);
    jal(offset);
  } else {
    Branch(kInstrSize * 2, NegateCondition(cond), rs,
           Operand(GetRtAsRegisterHelper(rt, scratch)));
    offset = GetOffset(offset, L, OffsetSize::kOffset21);
    jal(offset);
  }

  return true;
}

bool TurboAssembler::BranchAndLinkShortCheck(int32_t offset, Label* L,
                                             Condition cond, Register rs,
                                             const Operand& rt) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  if (!L) {
    DCHECK(is_int21(offset));
    return BranchAndLinkShortHelper(offset, nullptr, cond, rs, rt);
  } else {
    DCHECK_EQ(offset, 0);
    return BranchAndLinkShortHelper(0, L, cond, rs, rt);
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadTaggedPointerField(
      destination, FieldMemOperand(destination, FixedArray::OffsetOfElementAt(
                                                    constant_index)));
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  Ld(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    Add64(destination, kRootRegister, Operand(offset));
  }
}

void TurboAssembler::Jump(Register target, Condition cond, Register rs,
                          const Operand& rt) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (cond == cc_always) {
    jr(target);
    ForceConstantPoolEmissionWithoutJump();
  } else {
    BRANCH_ARGS_CHECK(cond, rs, rt);
    Branch(kInstrSize * 2, NegateCondition(cond), rs, rt);
    jr(target);
  }
}

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt) {
  Label skip;
  if (cond != cc_always) {
    Branch(&skip, NegateCondition(cond), rs, rt);
  }
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    li(t6, Operand(target, rmode));
    Jump(t6, al, zero_reg, Operand(zero_reg));
    EmitConstPoolWithJumpIfNeeded();
    bind(&skip);
  }
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          Register rs, const Operand& rt) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(static_cast<intptr_t>(target), rmode, cond, rs, rt);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));

  BlockTrampolinePoolScope block_trampoline_pool(this);
  Builtin builtin = Builtin::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin) &&
      Builtins::IsIsolateIndependent(builtin);
  if (target_is_isolate_independent_builtin &&
      options().use_pc_relative_calls_and_jumps) {
    int32_t code_target_index = AddCodeTarget(code);
    Label skip;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (cond != al) {
      Branch(&skip, NegateCondition(cond), rs, rt);
    }
    RecordRelocInfo(RelocInfo::RELATIVE_CODE_TARGET);
    GenPCRelativeJump(t6, code_target_index);
    bind(&skip);
    return;
  } else if (root_array_available_ && options().isolate_independent_code &&
             target_is_isolate_independent_builtin) {
    int offset = static_cast<int>(code->builtin_id()) * kSystemPointerSize +
                 IsolateData::builtin_entry_table_offset();
    Ld(t6, MemOperand(kRootRegister, offset));
    Jump(t6, cond, rs, rt);
    return;
  } else if (options().inline_offheap_trampolines &&
             target_is_isolate_independent_builtin) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin);
    li(t6, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
    Jump(t6, cond, rs, rt);
    RecordComment("]");
    return;
  }

  int32_t target_index = AddCodeTarget(code);
  Jump(static_cast<intptr_t>(target_index), rmode, cond, rs, rt);
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  li(t6, reference);
  Jump(t6);
}

// Note: To call gcc-compiled C code on riscv64, you must call through t6.
void TurboAssembler::Call(Register target, Condition cond, Register rs,
                          const Operand& rt) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (cond == cc_always) {
    jalr(ra, target, 0);
  } else {
    BRANCH_ARGS_CHECK(cond, rs, rt);
    Branch(kInstrSize * 2, NegateCondition(cond), rs, rt);
    jalr(ra, target, 0);
  }
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  if (lower_limit != 0) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Sub64(scratch, value, Operand(lower_limit));
    Branch(on_in_range, Uless_equal, scratch,
           Operand(higher_limit - lower_limit));
  } else {
    Branch(on_in_range, Uless_equal, value,
           Operand(higher_limit - lower_limit));
  }
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode, Condition cond,
                          Register rs, const Operand& rt) {
  li(t6, Operand(static_cast<int64_t>(target), rmode), ADDRESS_LOAD);
  Call(t6, cond, rs, rt);
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, Register rs, const Operand& rt) {
  Builtin builtin = Builtin::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin) &&
      Builtins::IsIsolateIndependent(builtin);
  if (target_is_isolate_independent_builtin &&
      options().use_pc_relative_calls_and_jumps) {
    int32_t code_target_index = AddCodeTarget(code);
    Label skip;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    RecordCommentForOffHeapTrampoline(builtin);
    if (cond != al) {
      Branch(&skip, NegateCondition(cond), rs, rt);
    }
    RecordRelocInfo(RelocInfo::RELATIVE_CODE_TARGET);
    GenPCRelativeJumpAndLink(t6, code_target_index);
    bind(&skip);
    RecordComment("]");
    return;
  } else if (root_array_available_ && options().isolate_independent_code &&
             target_is_isolate_independent_builtin) {
    int offset = static_cast<int>(code->builtin_id()) * kSystemPointerSize +
                 IsolateData::builtin_entry_table_offset();
    LoadRootRelative(t6, offset);
    Call(t6, cond, rs, rt);
    return;
  } else if (options().inline_offheap_trampolines &&
             target_is_isolate_independent_builtin) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin);
    li(t6, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
    Call(t6, cond, rs, rt);
    RecordComment("]");
    return;
  }

  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK(code->IsExecutable());
  int32_t target_index = AddCodeTarget(code);
  Call(static_cast<Address>(target_index), rmode, cond, rs, rt);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin) {
  STATIC_ASSERT(kSystemPointerSize == 8);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin register contains the builtin index as a Smi.
  SmiUntag(builtin, builtin);
  CalcScaledAddress(builtin, kRootRegister, builtin, kSystemPointerSizeLog2);
  Ld(builtin, MemOperand(builtin, IsolateData::builtin_entry_table_offset()));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin) {
  LoadEntryFromBuiltinIndex(builtin);
  Call(builtin);
}

void TurboAssembler::CallBuiltin(Builtin builtin) {
  RecordCommentForOffHeapTrampoline(builtin);
  if (options().short_builtin_calls) {
    Call(BuiltinEntry(builtin), RelocInfo::RUNTIME_ENTRY);
  } else {
    Call(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
  }
  RecordComment("]");
}

void TurboAssembler::TailCallBuiltin(Builtin builtin) {
  RecordCommentForOffHeapTrampoline(builtin);
  if (options().short_builtin_calls) {
    Jump(BuiltinEntry(builtin), RelocInfo::RUNTIME_ENTRY);
  } else {
    Jump(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
  }
  RecordComment("]");
}

void TurboAssembler::LoadEntryFromBuiltin(Builtin builtin,
                                          Register destination) {
  Ld(destination, EntryFromBuiltinAsOperand(builtin));
}

MemOperand TurboAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  DCHECK(root_array_available());
  return MemOperand(kRootRegister,
                    IsolateData::BuiltinEntrySlotOffset(builtin));
}

void TurboAssembler::PatchAndJump(Address target) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  auipc(scratch, 0);  // Load PC into scratch
  Ld(t6, MemOperand(scratch, kInstrSize * 4));
  jr(t6);
  nop();  // For alignment
  DCHECK_EQ(reinterpret_cast<uint64_t>(pc_) % 8, 0);
  *reinterpret_cast<uint64_t*>(pc_) = target;  // pc_ should be align.
  pc_ += sizeof(uint64_t);
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.
  //
  // Compute the return address in lr to return to after the jump below. The
  // pc is already at '+ 8' from the current instruction; but return is after
  // three instructions, so add another 4 to pc to get the return address.
  //
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(this);
  int kNumInstructionsToJump = 5;
  if (FLAG_riscv_c_extension) kNumInstructionsToJump = 4;
  Label find_ra;
  // Adjust the value in ra to point to the correct return location, one
  // instruction past the real call into C code (the jalr(t6)), and push it.
  // This is the return address of the exit frame.
  auipc(ra, 0);  // Set ra the current PC
  bind(&find_ra);
  addi(ra, ra,
       (kNumInstructionsToJump + 1) *
           kInstrSize);  // Set ra to insn after the call

  // This spot was reserved in EnterExitFrame.
  Sd(ra, MemOperand(sp));
  addi(sp, sp, -kCArgsSlotsSize);
  // Stack is still aligned.

  // Call the C routine.
  Mv(t6,
     target);  // Function pointer to t6 to conform to ABI for PIC.
  jalr(t6);
  // Make sure the stored 'ra' points to this position.
  DCHECK_EQ(kNumInstructionsToJump, InstructionsGeneratedSince(&find_ra));
}

void TurboAssembler::Ret(Condition cond, Register rs, const Operand& rt) {
  Jump(ra, cond, rs, rt);
  if (cond == al) {
    ForceConstantPoolEmissionWithoutJump();
  }
}


void TurboAssembler::BranchLong(Label* L) {
  // Generate position independent long branch.
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int64_t imm64;
  imm64 = branch_long_offset(L);
  GenPCRelativeJump(t6, imm64);
  EmitConstPoolWithJumpIfNeeded();
}

void TurboAssembler::BranchAndLinkLong(Label* L) {
  // Generate position independent long branch and link.
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int64_t imm64;
  imm64 = branch_long_offset(L);
  GenPCRelativeJumpAndLink(t6, imm64);
}

void TurboAssembler::DropAndRet(int drop) {
  Add64(sp, sp, drop * kSystemPointerSize);
  Ret();
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

  Add64(sp, sp, Operand(count * kSystemPointerSize));

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
    Mv(scratch, reg1);
    Mv(reg1, reg2);
    Mv(reg2, scratch);
  }
}

void TurboAssembler::Call(Label* target) { BranchAndLink(target); }

void TurboAssembler::LoadAddress(Register dst, Label* target,
                                 RelocInfo::Mode rmode) {
  int32_t offset;
  if (CalculateOffset(target, &offset, OffsetSize::kOffset32)) {
    CHECK(is_int32(offset + 0x800));
    int32_t Hi20 = (((int32_t)offset + 0x800) >> 12);
    int32_t Lo12 = (int32_t)offset << 20 >> 20;
    BlockTrampolinePoolScope block_trampoline_pool(this);
    auipc(dst, Hi20);
    addi(dst, dst, Lo12);
  } else {
    uint64_t address = jump_address(target);
    li(dst, Operand(address, rmode), ADDRESS_LOAD);
  }
}

void TurboAssembler::Push(Smi smi) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(smi));
  push(scratch);
}

void TurboAssembler::PushArray(Register array, Register size,
                               PushArrayOrder order) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  Label loop, entry;
  if (order == PushArrayOrder::kReverse) {
    Mv(scratch, zero_reg);
    jmp(&entry);
    bind(&loop);
    CalcScaledAddress(scratch2, array, scratch, kSystemPointerSizeLog2);
    Ld(scratch2, MemOperand(scratch2));
    push(scratch2);
    Add64(scratch, scratch, Operand(1));
    bind(&entry);
    Branch(&loop, less, scratch, Operand(size));
  } else {
    Mv(scratch, size);
    jmp(&entry);
    bind(&loop);
    CalcScaledAddress(scratch2, array, scratch, kSystemPointerSizeLog2);
    Ld(scratch2, MemOperand(scratch2));
    push(scratch2);
    bind(&entry);
    Add64(scratch, scratch, Operand(-1));
    Branch(&loop, greater_equal, scratch, Operand(zero_reg));
  }
}

void TurboAssembler::Push(Handle<HeapObject> handle) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(handle));
  push(scratch);
}

// ---------------------------------------------------------------------------
// Exception handling.

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kSystemPointerSize);

  Push(Smi::zero());  // Padding.

  // Link the current handler as the next handler.
  UseScratchRegisterScope temps(this);
  Register handler_address = temps.Acquire();
  li(handler_address,
     ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  Register handler = temps.Acquire();
  Ld(handler, MemOperand(handler_address));
  push(handler);

  // Set this new handler as the current one.
  Sd(sp, MemOperand(handler_address));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(a1);
  Add64(sp, sp,
        Operand(static_cast<int64_t>(StackHandlerConstants::kSize -
                                     kSystemPointerSize)));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch,
     ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  Sd(a1, MemOperand(scratch));
}

void TurboAssembler::FPUCanonicalizeNaN(const DoubleRegister dst,
                                        const DoubleRegister src) {
  // Subtracting 0.0 preserves all inputs except for signalling NaNs, which
  // become quiet NaNs. We use fsub rather than fadd because fsub preserves -0.0
  // inputs: -0.0 + 0.0 = 0.0, but -0.0 - 0.0 = -0.0.
  fsub_d(dst, src, kDoubleRegZero);
}

void TurboAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, fa0);  // Reg fa0 is FP return value.
}

void TurboAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, fa0);  // Reg fa0 is FP first argument value.
}

void TurboAssembler::MovToFloatParameter(DoubleRegister src) { Move(fa0, src); }

void TurboAssembler::MovToFloatResult(DoubleRegister src) { Move(fa0, src); }

void TurboAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  const DoubleRegister fparg2 = fa1;
  if (src2 == fa0) {
    DCHECK(src1 != fparg2);
    Move(fparg2, src2);
    Move(fa0, src1);
  } else {
    Move(fa0, src1);
    Move(fparg2, src2);
  }
}

// -----------------------------------------------------------------------------
// JavaScript invokes.

void MacroAssembler::LoadStackLimit(Register destination, StackLimitKind kind) {
  DCHECK(root_array_available());
  Isolate* isolate = this->isolate();
  ExternalReference limit =
      kind == StackLimitKind::kRealStackLimit
          ? ExternalReference::address_of_real_jslimit(isolate)
          : ExternalReference::address_of_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  CHECK(is_int32(offset));
  Ld(destination, MemOperand(kRootRegister, static_cast<int32_t>(offset)));
}

void MacroAssembler::StackOverflowCheck(Register num_args, Register scratch1,
                                        Register scratch2,
                                        Label* stack_overflow, Label* done) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  DCHECK(stack_overflow != nullptr || done != nullptr);
  LoadStackLimit(scratch1, StackLimitKind::kRealStackLimit);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  Sub64(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  Sll64(scratch2, num_args, kSystemPointerSizeLog2);
  // Signed comparison.
  if (stack_overflow != nullptr) {
    Branch(stack_overflow, le, scratch1, Operand(scratch2));
  } else if (done != nullptr) {
    Branch(done, gt, scratch1, Operand(scratch2));
  } else {
    UNREACHABLE();
  }
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeType type) {
  Label regular_invoke;

  //  a0: actual arguments count
  //  a1: function (passed through to callee)
  //  a2: expected arguments count

  DCHECK_EQ(actual_parameter_count, a0);
  DCHECK_EQ(expected_parameter_count, a2);

  // If the expected parameter count is equal to the adaptor sentinel, no need
  // to push undefined value as arguments.
  if (kDontAdaptArgumentsSentinel != 0) {
    Branch(&regular_invoke, eq, expected_parameter_count,
           Operand(kDontAdaptArgumentsSentinel));
  }
  // If overapplication or if the actual argument count is equal to the
  // formal parameter count, no need to push extra undefined values.
  Sub64(expected_parameter_count, expected_parameter_count,
        actual_parameter_count);
  Branch(&regular_invoke, le, expected_parameter_count, Operand(zero_reg));

  Label stack_overflow;
  {
    UseScratchRegisterScope temps(this);
    StackOverflowCheck(expected_parameter_count, temps.Acquire(),
                       temps.Acquire(), &stack_overflow);
  }
  // Underapplication. Move the arguments already in the stack, including the
  // receiver and the return address.
  {
    Label copy;
    Register src = a6, dest = a7;
    Move(src, sp);
    Sll64(t0, expected_parameter_count, kSystemPointerSizeLog2);
    Sub64(sp, sp, Operand(t0));
    // Update stack pointer.
    Move(dest, sp);
    Move(t0, actual_parameter_count);
    bind(&copy);
    Ld(t1, MemOperand(src, 0));
    Sd(t1, MemOperand(dest, 0));
    Sub64(t0, t0, Operand(1));
    Add64(src, src, Operand(kSystemPointerSize));
    Add64(dest, dest, Operand(kSystemPointerSize));
    Branch(&copy, gt, t0, Operand(zero_reg));
  }

  // Fill remaining expected arguments with undefined values.
  LoadRoot(t0, RootIndex::kUndefinedValue);
  {
    Label loop;
    bind(&loop);
    Sd(t0, MemOperand(a7, 0));
    Sub64(expected_parameter_count, expected_parameter_count, Operand(1));
    Add64(a7, a7, Operand(kSystemPointerSize));
    Branch(&loop, gt, expected_parameter_count, Operand(zero_reg));
  }
  Branch(&regular_invoke);

  bind(&stack_overflow);
  {
    FrameScope frame(
        this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
    CallRuntime(Runtime::kThrowStackOverflow);
    break_(0xCC);
  }
  bind(&regular_invoke);
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count) {
  Label skip_hook;
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch,
       ExternalReference::debug_hook_on_function_call_address(isolate()));
    Lb(scratch, MemOperand(scratch));
    Branch(&skip_hook, eq, scratch, Operand(zero_reg));
  }
  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    UseScratchRegisterScope temps(this);
    Register receiver = temps.Acquire();
    LoadReceiver(receiver, actual_parameter_count);

    FrameScope frame(
        this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
    SmiTag(expected_parameter_count);
    Push(expected_parameter_count);

    SmiTag(actual_parameter_count);
    Push(actual_parameter_count);

    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    Push(receiver);
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
                                        InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());
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
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, type);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Register code = kJavaScriptCallCodeStartRegister;
  LoadTaggedPointerField(code,
                         FieldMemOperand(function, JSFunction::kCodeOffset));
  switch (type) {
    case InvokeType::kCall:
      CallCodeObject(code);
      break;
    case InvokeType::kJump:
      JumpCodeObject(code);
      break;
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}

void MacroAssembler::InvokeFunctionWithNewTarget(
    Register function, Register new_target, Register actual_parameter_count,
    InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK_EQ(function, a1);
  Register expected_parameter_count = a2;
  {
    UseScratchRegisterScope temps(this);
    Register temp_reg = temps.Acquire();
    LoadTaggedPointerField(
        temp_reg,
        FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    LoadTaggedPointerField(
        cp, FieldMemOperand(function, JSFunction::kContextOffset));
    // The argument count is stored as uint16_t
    Lhu(expected_parameter_count,
        FieldMemOperand(temp_reg,
                        SharedFunctionInfo::kFormalParameterCountOffset));
  }
  InvokeFunctionCode(function, new_target, expected_parameter_count,
                     actual_parameter_count, type);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());

  // Contract with called JS functions requires that function is passed in a1.
  DCHECK_EQ(function, a1);

  // Get the function and setup the context.
  LoadTaggedPointerField(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  InvokeFunctionCode(a1, no_reg, expected_parameter_count,
                     actual_parameter_count, type);
}

// ---------------------------------------------------------------------------
// Support functions.

void MacroAssembler::GetObjectType(Register object, Register map,
                                   Register type_reg) {
  LoadMap(map, object);
  Lhu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
}

void MacroAssembler::GetInstanceTypeRange(Register map, Register type_reg,
                                          InstanceType lower_limit,
                                          Register range) {
  Lhu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Sub64(range, type_reg, Operand(lower_limit));
}
//------------------------------------------------------------------------------
// Wasm
void TurboAssembler::WasmRvvEq(VRegister dst, VRegister lhs, VRegister rhs,
                               VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmseq_vv(v0, lhs, rhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvNe(VRegister dst, VRegister lhs, VRegister rhs,
                               VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmsne_vv(v0, lhs, rhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvGeS(VRegister dst, VRegister lhs, VRegister rhs,
                                VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmsle_vv(v0, rhs, lhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvGeU(VRegister dst, VRegister lhs, VRegister rhs,
                                VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmsleu_vv(v0, rhs, lhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvGtS(VRegister dst, VRegister lhs, VRegister rhs,
                                VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmslt_vv(v0, rhs, lhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvGtU(VRegister dst, VRegister lhs, VRegister rhs,
                                VSew sew, Vlmul lmul) {
  VU.set(kScratchReg, sew, lmul);
  vmsltu_vv(v0, rhs, lhs);
  li(kScratchReg, -1);
  vmv_vx(dst, zero_reg);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::WasmRvvS128const(VRegister dst, const uint8_t imms[16]) {
  uint64_t imm1 = *(reinterpret_cast<const uint64_t*>(imms));
  uint64_t imm2 = *((reinterpret_cast<const uint64_t*>(imms)) + 1);
  VU.set(kScratchReg, VSew::E64, Vlmul::m1);
  li(kScratchReg, 1);
  vmv_vx(v0, kScratchReg);
  li(kScratchReg, imm1);
  vmerge_vx(dst, kScratchReg, dst);
  li(kScratchReg, imm2);
  vsll_vi(v0, v0, 1);
  vmerge_vx(dst, kScratchReg, dst);
}

void TurboAssembler::LoadLane(int ts, VRegister dst, uint8_t laneidx,
                              MemOperand src) {
  if (ts == 8) {
    Lbu(kScratchReg2, src);
    VU.set(kScratchReg, E64, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    VU.set(kScratchReg, E8, m1);
    vmerge_vx(dst, kScratchReg2, dst);
  } else if (ts == 16) {
    Lhu(kScratchReg2, src);
    VU.set(kScratchReg, E16, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst, kScratchReg2, dst);
  } else if (ts == 32) {
    Lwu(kScratchReg2, src);
    VU.set(kScratchReg, E32, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst, kScratchReg2, dst);
  } else if (ts == 64) {
    Ld(kScratchReg2, src);
    VU.set(kScratchReg, E64, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst, kScratchReg2, dst);
  } else {
    UNREACHABLE();
  }
}

void TurboAssembler::StoreLane(int sz, VRegister src, uint8_t laneidx,
                               MemOperand dst) {
  if (sz == 8) {
    VU.set(kScratchReg, E8, m1);
    vslidedown_vi(kSimd128ScratchReg, src, laneidx);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sb(kScratchReg, dst);
  } else if (sz == 16) {
    VU.set(kScratchReg, E16, m1);
    vslidedown_vi(kSimd128ScratchReg, src, laneidx);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sh(kScratchReg, dst);
  } else if (sz == 32) {
    VU.set(kScratchReg, E32, m1);
    vslidedown_vi(kSimd128ScratchReg, src, laneidx);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sw(kScratchReg, dst);
  } else {
    DCHECK_EQ(sz, 64);
    VU.set(kScratchReg, E64, m1);
    vslidedown_vi(kSimd128ScratchReg, src, laneidx);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sd(kScratchReg, dst);
  }
}
// -----------------------------------------------------------------------------
// Runtime calls.

void TurboAssembler::AddOverflow64(Register dst, Register left,
                                   const Operand& right, Register overflow) {
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  if (!right.is_reg()) {
    li(scratch, Operand(right));
    right_reg = scratch;
  } else {
    right_reg = right.rm();
  }
  DCHECK(left != scratch2 && right_reg != scratch2 && dst != scratch2 &&
         overflow != scratch2);
  DCHECK(overflow != left && overflow != right_reg);
  if (dst == left || dst == right_reg) {
    add(scratch2, left, right_reg);
    xor_(overflow, scratch2, left);
    xor_(scratch, scratch2, right_reg);
    and_(overflow, overflow, scratch);
    Mv(dst, scratch2);
  } else {
    add(dst, left, right_reg);
    xor_(overflow, dst, left);
    xor_(scratch, dst, right_reg);
    and_(overflow, overflow, scratch);
  }
}

void TurboAssembler::SubOverflow64(Register dst, Register left,
                                   const Operand& right, Register overflow) {
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  if (!right.is_reg()) {
    li(scratch, Operand(right));
    right_reg = scratch;
  } else {
    right_reg = right.rm();
  }

  DCHECK(left != scratch2 && right_reg != scratch2 && dst != scratch2 &&
         overflow != scratch2);
  DCHECK(overflow != left && overflow != right_reg);

  if (dst == left || dst == right_reg) {
    sub(scratch2, left, right_reg);
    xor_(overflow, left, scratch2);
    xor_(scratch, left, right_reg);
    and_(overflow, overflow, scratch);
    Mv(dst, scratch2);
  } else {
    sub(dst, left, right_reg);
    xor_(overflow, left, dst);
    xor_(scratch, left, right_reg);
    and_(overflow, overflow, scratch);
  }
}

void TurboAssembler::MulOverflow32(Register dst, Register left,
                                   const Operand& right, Register overflow) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register right_reg = no_reg;
  Register scratch = temps.Acquire();
  Register scratch2 = temps.Acquire();
  if (!right.is_reg()) {
    li(scratch, Operand(right));
    right_reg = scratch;
  } else {
    right_reg = right.rm();
  }

  DCHECK(left != scratch2 && right_reg != scratch2 && dst != scratch2 &&
         overflow != scratch2);
  DCHECK(overflow != left && overflow != right_reg);
  sext_w(overflow, left);
  sext_w(scratch2, right_reg);

  mul(overflow, overflow, scratch2);
  sext_w(dst, overflow);
  xor_(overflow, overflow, dst);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  ASM_CODE_COMMENT(this);
  // All parameters are on the stack. a0 has the return value after call.

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
  ASM_CODE_COMMENT(this);
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    PrepareCEntryArgs(function->nargs);
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  ASM_CODE_COMMENT(this);
  PrepareCEntryFunction(builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, SaveFPRegsMode::kIgnore,
                                          ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg));
}

void MacroAssembler::JumpToOffHeapInstructionStream(Address entry) {
  // Ld a Address from a constant pool.
  // Record a value into constant pool.
  ASM_CODE_COMMENT(this);
  if (!FLAG_riscv_constant_pool) {
    li(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  } else {
    RecordEntry(entry, RelocInfo::OFF_HEAP_TARGET);
    RecordRelocInfo(RelocInfo::OFF_HEAP_TARGET, entry);
    auipc(kOffHeapTrampolineRegister, 0);
    ld(kOffHeapTrampolineRegister, kOffHeapTrampolineRegister, 0);
  }
  Jump(kOffHeapTrampolineRegister);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  ASM_CODE_COMMENT(this);
  Branch(target_if_cleared, eq, in, Operand(kClearedWeakHeapObjectLower32));
  And(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value,
                                          Register scratch1,
                                          Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t
    // dummy_stats_counter_ field.
    li(scratch2, ExternalReference::Create(counter));
    Lw(scratch1, MemOperand(scratch2));
    Add32(scratch1, scratch1, Operand(value));
    Sw(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::EmitDecrementCounter(StatsCounter* counter, int value,
                                          Register scratch1,
                                          Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t
    // dummy_stats_counter_ field.
    li(scratch2, ExternalReference::Create(counter));
    Lw(scratch1, MemOperand(scratch2));
    Sub32(scratch1, scratch1, Operand(value));
    Sw(scratch1, MemOperand(scratch2));
  }
}

// -----------------------------------------------------------------------------
// Debugging.

void TurboAssembler::Trap() { stop(); }
void TurboAssembler::DebugBreak() { stop(); }

void TurboAssembler::Assert(Condition cc, AbortReason reason, Register rs,
                            Operand rt) {
  if (FLAG_debug_code) Check(cc, reason, rs, rt);
}

void TurboAssembler::Check(Condition cc, AbortReason reason, Register rs,
                           Operand rt) {
  Label L;
  BranchShort(&L, cc, rs, rt);
  Abort(reason);
  // Will not return here.
  bind(&L);
}

void TurboAssembler::Abort(AbortReason reason) {
  Label abort_start;
  bind(&abort_start);
  if (FLAG_code_comments) {
    const char* msg = GetAbortReason(reason);
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    ebreak();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NO_FRAME_TYPE);
    PrepareCallCFunction(0, a0);
    li(a0, Operand(static_cast<int64_t>(reason)));
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

  Move(a0, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NO_FRAME_TYPE);
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

void TurboAssembler::LoadMap(Register destination, Register object) {
  ASM_CODE_COMMENT(this);
  LoadTaggedPointerField(destination,
                         FieldMemOperand(object, HeapObject::kMapOffset));
}

void MacroAssembler::LoadNativeContextSlot(Register dst, int index) {
  ASM_CODE_COMMENT(this);
  LoadMap(dst, cp);
  LoadTaggedPointerField(
      dst, FieldMemOperand(
               dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  LoadTaggedPointerField(dst, MemOperand(dst, Context::SlotOffset(index)));
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, Operand(StackFrame::TypeToMarker(type)));
  PushCommonFrame(scratch);
}

void TurboAssembler::Prologue() { PushStandardFrame(a1); }

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Push(ra, fp);
  Move(fp, sp);
  if (!StackFrame::IsJavaScript(type)) {
    li(scratch, Operand(StackFrame::TypeToMarker(type)));
    Push(scratch);
  }
#if V8_ENABLE_WEBASSEMBLY
  if (type == StackFrame::WASM) Push(kWasmInstanceRegister);
#endif  // V8_ENABLE_WEBASSEMBLY
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  addi(sp, fp, 2 * kSystemPointerSize);
  Ld(ra, MemOperand(fp, 1 * kSystemPointerSize));
  Ld(fp, MemOperand(fp, 0 * kSystemPointerSize));
}

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
  ASM_CODE_COMMENT(this);
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  STATIC_ASSERT(2 * kSystemPointerSize ==
                ExitFrameConstants::kCallerSPDisplacement);
  STATIC_ASSERT(1 * kSystemPointerSize == ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT(0 * kSystemPointerSize == ExitFrameConstants::kCallerFPOffset);

  // This is how the stack will look:
  // fp + 2 (==kCallerSPDisplacement) - old stack's end
  // [fp + 1 (==kCallerPCOffset)] - saved old ra
  // [fp + 0 (==kCallerFPOffset)] - saved old fp
  // [fp - 1 StackFrame::EXIT Smi
  // [fp - 2 (==kSPOffset)] - sp of the called function
  // fp - (2 + stack_space + alignment) == sp == [fp - kSPOffset] - top of the
  //   new stack (will contain saved ra)

  // Save registers and reserve room for saved entry sp.
  addi(sp, sp,
       -2 * kSystemPointerSize - ExitFrameConstants::kFixedFrameSizeFromFp);
  Sd(ra, MemOperand(sp, 3 * kSystemPointerSize));
  Sd(fp, MemOperand(sp, 2 * kSystemPointerSize));
  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    li(scratch, Operand(StackFrame::TypeToMarker(frame_type)));
    Sd(scratch, MemOperand(sp, 1 * kSystemPointerSize));
  }
  // Set up new frame pointer.
  addi(fp, sp, ExitFrameConstants::kFixedFrameSizeFromFp);

  if (FLAG_debug_code) {
    Sd(zero_reg, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }

  {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    BlockTrampolinePoolScope block_trampoline_pool(this);
    // Save the frame pointer and the context in top.
    li(scratch, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          isolate()));
    Sd(fp, MemOperand(scratch));
    li(scratch,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
    Sd(cp, MemOperand(scratch));
  }

  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (save_doubles) {
    // The stack is already aligned to 0 modulo 8 for stores with sdc1.
    int space = kNumCallerSavedFPU * kDoubleSize;
    Sub64(sp, sp, Operand(space));
    int count = 0;
    for (int i = 0; i < kNumFPURegisters; i++) {
      if (kCallerSavedFPU.bits() & (1 << i)) {
        FPURegister reg = FPURegister::from_code(i);
        StoreDouble(reg, MemOperand(sp, count * kDoubleSize));
        count++;
      }
    }
  }

  // Reserve place for the return address, stack space and an optional slot
  // (used by DirectCEntry to hold the return value if a struct is
  // returned) and align the frame preparing for calling the runtime function.
  DCHECK_GE(stack_space, 0);
  Sub64(sp, sp, Operand((stack_space + 2) * kSystemPointerSize));
  if (frame_alignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));  // Align stack.
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  addi(scratch, sp, kSystemPointerSize);
  Sd(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool do_return,
                                    bool argument_count_is_length) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Remember: we only need to restore kCallerSavedFPU.
    Sub64(scratch, fp,
          Operand(ExitFrameConstants::kFixedFrameSizeFromFp +
                  kNumCallerSavedFPU * kDoubleSize));
    int cout = 0;
    for (int i = 0; i < kNumFPURegisters; i++) {
      if (kCalleeSavedFPU.bits() & (1 << i)) {
        FPURegister reg = FPURegister::from_code(i);
        LoadDouble(reg, MemOperand(scratch, cout * kDoubleSize));
        cout++;
      }
    }
  }

  // Clear top frame.
  li(scratch,
     ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  Sd(zero_reg, MemOperand(scratch));

  // Restore current context from top and clear it in debug mode.
  li(scratch,
     ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Ld(cp, MemOperand(scratch));

  if (FLAG_debug_code) {
    UseScratchRegisterScope temp(this);
    Register scratch2 = temp.Acquire();
    li(scratch2, Operand(Context::kInvalidContext));
    Sd(scratch2, MemOperand(scratch));
  }

  // Pop the arguments, restore registers, and return.
  Mv(sp, fp);  // Respect ABI stack constraint.
  Ld(fp, MemOperand(sp, ExitFrameConstants::kCallerFPOffset));
  Ld(ra, MemOperand(sp, ExitFrameConstants::kCallerPCOffset));

  if (argument_count.is_valid()) {
    if (argument_count_is_length) {
      add(sp, sp, argument_count);
    } else {
      CalcScaledAddress(sp, sp, argument_count, kSystemPointerSizeLog2);
    }
  }

  addi(sp, sp, 2 * kSystemPointerSize);

  if (do_return) {
    Ret();
  }
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_RISCV64
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one RISC-V
  // platform for another RISC-V platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else   // V8_HOST_ARCH_RISCV64
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_RISCV64
}

void MacroAssembler::AssertStackIsAligned() {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    const int frame_alignment = ActivationFrameAlignment();
    const int frame_alignment_mask = frame_alignment - 1;

    if (frame_alignment > kSystemPointerSize) {
      Label alignment_as_expected;
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        andi(scratch, sp, frame_alignment_mask);
        BranchShort(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      }
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      ebreak();
      bind(&alignment_as_expected);
    }
  }
}

void TurboAssembler::SmiUntag(Register dst, const MemOperand& src) {
  ASM_CODE_COMMENT(this);
  if (SmiValuesAre32Bits()) {
    Lw(dst, MemOperand(src.rm(), SmiWordOffset(src.offset())));
  } else {
    DCHECK(SmiValuesAre31Bits());
    if (COMPRESS_POINTERS_BOOL) {
      Lw(dst, src);
    } else {
      Ld(dst, src);
    }
    SmiUntag(dst);
  }
}

void TurboAssembler::SmiToInt32(Register smi) {
  ASM_CODE_COMMENT(this);
  if (FLAG_enable_slow_asserts) {
    AssertSmi(smi);
  }
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  SmiUntag(smi);
}

void TurboAssembler::JumpIfSmi(Register value, Label* smi_label) {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(0, kSmiTag);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  andi(scratch, value, kSmiTagMask);
  Branch(smi_label, eq, scratch, Operand(zero_reg));
}

void MacroAssembler::JumpIfNotSmi(Register value, Label* not_smi_label) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  DCHECK_EQ(0, kSmiTag);
  andi(scratch, value, kSmiTagMask);
  Branch(not_smi_label, ne, scratch, Operand(zero_reg));
}

void TurboAssembler::AssertNotSmi(Register object, AbortReason reason) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK(object != kScratchReg);
    andi(kScratchReg, object, kSmiTagMask);
    Check(ne, reason, kScratchReg, Operand(zero_reg));
  }
}

void TurboAssembler::AssertSmi(Register object, AbortReason reason) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK(object != kScratchReg);
    andi(kScratchReg, object, kSmiTagMask);
    Check(eq, reason, kScratchReg, Operand(zero_reg));
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    DCHECK(object != kScratchReg);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    SmiTst(object, kScratchReg);
    Check(ne, AbortReason::kOperandIsASmiAndNotAConstructor, kScratchReg,
          Operand(zero_reg));

    LoadMap(kScratchReg, object);
    Lbu(kScratchReg, FieldMemOperand(kScratchReg, Map::kBitFieldOffset));
    And(kScratchReg, kScratchReg, Operand(Map::Bits1::IsConstructorBit::kMask));
    Check(ne, AbortReason::kOperandIsNotAConstructor, kScratchReg,
          Operand(zero_reg));
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK(object != kScratchReg);
    SmiTst(object, kScratchReg);
    Check(ne, AbortReason::kOperandIsASmiAndNotAFunction, kScratchReg,
          Operand(zero_reg));
    push(object);
    LoadMap(object, object);
    UseScratchRegisterScope temps(this);
    Register range = temps.Acquire();
    GetInstanceTypeRange(object, object, FIRST_JS_FUNCTION_TYPE, range);
    Check(Uless_equal, AbortReason::kOperandIsNotAFunction, range,
          Operand(LAST_JS_FUNCTION_TYPE - FIRST_JS_FUNCTION_TYPE));
    pop(object);
  }
}

void MacroAssembler::AssertCallableFunction(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  STATIC_ASSERT(kSmiTag == 0);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAFunction);
  push(object);
  LoadMap(object, object);
  UseScratchRegisterScope temps(this);
  Register range = temps.Acquire();
  GetInstanceTypeRange(object, object, FIRST_CALLABLE_JS_FUNCTION_TYPE, range);
  Check(Uless_equal, AbortReason::kOperandIsNotACallableFunction, range,
        Operand(LAST_CALLABLE_JS_FUNCTION_TYPE -
                FIRST_CALLABLE_JS_FUNCTION_TYPE));
  pop(object);
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    BlockTrampolinePoolScope block_trampoline_pool(this);
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK(object != kScratchReg);
    SmiTst(object, kScratchReg);
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction, kScratchReg,
          Operand(zero_reg));
    GetObjectType(object, kScratchReg, kScratchReg);
    Check(eq, AbortReason::kOperandIsNotABoundFunction, kScratchReg,
          Operand(JS_BOUND_FUNCTION_TYPE));
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK(object != kScratchReg);
  SmiTst(object, kScratchReg);
  Check(ne, AbortReason::kOperandIsASmiAndNotAGeneratorObject, kScratchReg,
        Operand(zero_reg));

  GetObjectType(object, kScratchReg, kScratchReg);

  Label done;

  // Check if JSGeneratorObject
  BranchShort(&done, eq, kScratchReg, Operand(JS_GENERATOR_OBJECT_TYPE));

  // Check if JSAsyncFunctionObject (See MacroAssembler::CompareInstanceType)
  BranchShort(&done, eq, kScratchReg, Operand(JS_ASYNC_FUNCTION_OBJECT_TYPE));

  // Check if JSAsyncGeneratorObject
  BranchShort(&done, eq, kScratchReg, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

  Abort(AbortReason::kOperandIsNotAGeneratorObject);

  bind(&done);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT(this);
    Label done_checking;
    AssertNotSmi(object);
    LoadRoot(scratch, RootIndex::kUndefinedValue);
    BranchShort(&done_checking, eq, object, Operand(scratch));
    GetObjectType(object, scratch, scratch);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell, scratch,
           Operand(ALLOCATION_SITE_TYPE));
    bind(&done_checking);
  }
}

template <typename F_TYPE>
void TurboAssembler::FloatMinMaxHelper(FPURegister dst, FPURegister src1,
                                       FPURegister src2, MaxMinKind kind) {
  DCHECK((std::is_same<F_TYPE, float>::value) ||
         (std::is_same<F_TYPE, double>::value));

  if (src1 == src2 && dst != src1) {
    if (std::is_same<float, F_TYPE>::value) {
      fmv_s(dst, src1);
    } else {
      fmv_d(dst, src1);
    }
    return;
  }

  Label done, nan;

  // For RISCV, fmin_s returns the other non-NaN operand as result if only one
  // operand is NaN; but for JS, if any operand is NaN, result is Nan. The
  // following handles the discrepency between handling of NaN between ISA and
  // JS semantics
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (std::is_same<float, F_TYPE>::value) {
    CompareIsNotNanF32(scratch, src1, src2);
  } else {
    CompareIsNotNanF64(scratch, src1, src2);
  }
  BranchFalseF(scratch, &nan);

  if (kind == MaxMinKind::kMax) {
    if (std::is_same<float, F_TYPE>::value) {
      fmax_s(dst, src1, src2);
    } else {
      fmax_d(dst, src1, src2);
    }
  } else {
    if (std::is_same<float, F_TYPE>::value) {
      fmin_s(dst, src1, src2);
    } else {
      fmin_d(dst, src1, src2);
    }
  }
  j(&done);

  bind(&nan);
  // if any operand is NaN, return NaN (fadd returns NaN if any operand is NaN)
  if (std::is_same<float, F_TYPE>::value) {
    fadd_s(dst, src1, src2);
  } else {
    fadd_d(dst, src1, src2);
  }

  bind(&done);
}

void TurboAssembler::Float32Max(FPURegister dst, FPURegister src1,
                                FPURegister src2) {
  ASM_CODE_COMMENT(this);
  FloatMinMaxHelper<float>(dst, src1, src2, MaxMinKind::kMax);
}

void TurboAssembler::Float32Min(FPURegister dst, FPURegister src1,
                                FPURegister src2) {
  ASM_CODE_COMMENT(this);
  FloatMinMaxHelper<float>(dst, src1, src2, MaxMinKind::kMin);
}

void TurboAssembler::Float64Max(FPURegister dst, FPURegister src1,
                                FPURegister src2) {
  ASM_CODE_COMMENT(this);
  FloatMinMaxHelper<double>(dst, src1, src2, MaxMinKind::kMax);
}

void TurboAssembler::Float64Min(FPURegister dst, FPURegister src1,
                                FPURegister src2) {
  ASM_CODE_COMMENT(this);
  FloatMinMaxHelper<double>(dst, src1, src2, MaxMinKind::kMin);
}

static const int kRegisterPassedArguments = 8;

int TurboAssembler::CalculateStackPassedDWords(int num_gp_arguments,
                                               int num_fp_arguments) {
  int stack_passed_dwords = 0;

  // Up to eight integer arguments are passed in registers a0..a7 and
  // up to eight floating point arguments are passed in registers fa0..fa7
  if (num_gp_arguments > kRegisterPassedArguments) {
    stack_passed_dwords += num_gp_arguments - kRegisterPassedArguments;
  }
  if (num_fp_arguments > kRegisterPassedArguments) {
    stack_passed_dwords += num_fp_arguments - kRegisterPassedArguments;
  }
  stack_passed_dwords += kCArgSlotCount;
  return stack_passed_dwords;
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  ASM_CODE_COMMENT(this);
  int frame_alignment = ActivationFrameAlignment();

  // Up to eight simple arguments in a0..a7, fa0..fa7.
  // Remaining arguments are pushed on the stack (arg slot calculation handled
  // by CalculateStackPassedDWords()).
  int stack_passed_arguments =
      CalculateStackPassedDWords(num_reg_arguments, num_double_arguments);
  if (frame_alignment > kSystemPointerSize) {
    // Make stack end at alignment and make room for stack arguments and the
    // original value of sp.
    Mv(scratch, sp);
    Sub64(sp, sp, Operand((stack_passed_arguments + 1) * kSystemPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));
    Sd(scratch, MemOperand(sp, stack_passed_arguments * kSystemPointerSize));
  } else {
    Sub64(sp, sp, Operand(stack_passed_arguments * kSystemPointerSize));
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
  li(t6, function);
  CallCFunctionHelper(t6, num_reg_arguments, num_double_arguments);
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
  ASM_CODE_COMMENT(this);
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
  // The argument stots are presumed to have been set up by
  // PrepareCallCFunction.

#if V8_HOST_ARCH_RISCV64
  if (FLAG_debug_code) {
    int frame_alignment = base::OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kSystemPointerSize) {
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      Label alignment_as_expected;
      {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        And(scratch, sp, Operand(frame_alignment_mask));
        BranchShort(&alignment_as_expected, eq, scratch, Operand(zero_reg));
      }
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      ebreak();
      bind(&alignment_as_expected);
    }
  }
#endif  // V8_HOST_ARCH_RISCV64

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  {
    if (function != t6) {
      Mv(t6, function);
      function = t6;
    }

    // Save the frame pointer and PC so that the stack layout remains
    // iterable, even without an ExitFrame which normally exists between JS
    // and C frames.
    // 't' registers are caller-saved so this is safe as a scratch register.
    Register pc_scratch = t1;
    Register scratch = t2;

    auipc(pc_scratch, 0);
    // See x64 code for reasoning about how to address the isolate data fields.
    if (root_array_available()) {
      Sd(pc_scratch, MemOperand(kRootRegister,
                                IsolateData::fast_c_call_caller_pc_offset()));
      Sd(fp, MemOperand(kRootRegister,
                        IsolateData::fast_c_call_caller_fp_offset()));
    } else {
      DCHECK_NOT_NULL(isolate());
      li(scratch, ExternalReference::fast_c_call_caller_pc_address(isolate()));
      Sd(pc_scratch, MemOperand(scratch));
      li(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      Sd(fp, MemOperand(scratch));
    }

    Call(function);

    if (isolate() != nullptr) {
      // We don't unset the PC; the FP is the source of truth.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      li(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
      Sd(zero_reg, MemOperand(scratch));
    }
  }

  int stack_passed_arguments =
      CalculateStackPassedDWords(num_reg_arguments, num_double_arguments);

  if (base::OS::ActivationFrameAlignment() > kSystemPointerSize) {
    Ld(sp, MemOperand(sp, stack_passed_arguments * kSystemPointerSize));
  } else {
    Add64(sp, sp, Operand(stack_passed_arguments * kSystemPointerSize));
  }
}

#undef BRANCH_ARGS_CHECK

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met) {
  And(scratch, object, Operand(~kPageAlignmentMask));
  Ld(scratch, MemOperand(scratch, BasicMemoryChunk::kFlagsOffset));
  And(scratch, scratch, Operand(mask));
  Branch(condition_met, cc, scratch, Operand(zero_reg));
}

Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2, Register reg3,
                                   Register reg4, Register reg5,
                                   Register reg6) {
  RegList regs = {reg1, reg2, reg3, reg4, reg5, reg6};

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs.has(candidate)) continue;
    return candidate;
  }
  UNREACHABLE();
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  // This push on ra and the pop below together ensure that we restore the
  // register ra, which is needed while computing the code start address.
  push(ra);

  auipc(ra, 0);
  addi(ra, ra, kInstrSize * 2);  // ra = address of li
  int pc = pc_offset();
  li(dst, Operand(pc));
  Sub64(dst, ra, dst);

  pop(ra);  // Restore ra
}

void TurboAssembler::CallForDeoptimization(Builtin target, int, Label* exit,
                                           DeoptimizeKind kind, Label* ret,
                                           Label*) {
  ASM_CODE_COMMENT(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Ld(t6,
     MemOperand(kRootRegister, IsolateData::BuiltinEntrySlotOffset(target)));
  Call(t6);
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
  // Code objects are called differently depending on whether we are generating
  // builtin code (which will later be embedded into the binary) or compiling
  // user JS code at runtime.
  // * Builtin code runs in --jitless mode and thus must not call into on-heap
  //   Code targets. Instead, we dispatch through the builtins entry table.
  // * Codegen at runtime does not have this restriction and we can use the
  //   shorter, branchless instruction sequence. The assumption here is that
  //   targets are usually generated code and not builtin Code objects.
  ASM_CODE_COMMENT(this);
  if (options().isolate_independent_code) {
    DCHECK(root_array_available());
    Label if_code_is_off_heap, out;

    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();

    DCHECK(!AreAliased(destination, scratch));
    DCHECK(!AreAliased(code_object, scratch));

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.

    Lw(scratch, FieldMemOperand(code_object, Code::kFlagsOffset));
    And(scratch, scratch, Operand(Code::IsOffHeapTrampoline::kMask));
    Branch(&if_code_is_off_heap, ne, scratch, Operand(zero_reg));
    // Not an off-heap trampoline object, the entry point is at
    // Code::raw_instruction_start().
    Add64(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
    Branch(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    Lw(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset));
    slli(destination, scratch, kSystemPointerSizeLog2);
    Add64(destination, destination, kRootRegister);
    Ld(destination,
       MemOperand(destination, IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    Add64(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  ASM_CODE_COMMENT(this);
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(JumpMode::kJump, jump_mode);
  LoadCodeObjectEntry(code_object, code_object);
  Jump(code_object);
}

void TurboAssembler::LoadTaggedPointerField(const Register& destination,
                                            const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(destination, field_operand);
  } else {
    Ld(destination, field_operand);
  }
}

void TurboAssembler::LoadAnyTaggedField(const Register& destination,
                                        const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressAnyTagged(destination, field_operand);
  } else {
    Ld(destination, field_operand);
  }
}

void TurboAssembler::LoadTaggedSignedField(const Register& destination,
                                           const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedSigned(destination, field_operand);
  } else {
    Ld(destination, field_operand);
  }
}

void TurboAssembler::SmiUntagField(Register dst, const MemOperand& src) {
  SmiUntag(dst, src);
}

void TurboAssembler::StoreTaggedField(const Register& value,
                                      const MemOperand& dst_field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    Sw(value, dst_field_operand);
  } else {
    Sd(value, dst_field_operand);
  }
}

void TurboAssembler::DecompressTaggedSigned(const Register& destination,
                                            const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Lwu(destination, field_operand);
  if (FLAG_debug_code) {
    // Corrupt the top 32 bits. Made up of 16 fixed bits and 16 pc offset bits.
    Add64(destination, destination,
          Operand(((kDebugZapValue << 16) | (pc_offset() & 0xffff)) << 32));
  }
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Lwu(destination, field_operand);
  Add64(destination, kPtrComprCageBaseRegister, destination);
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const Register& source) {
  ASM_CODE_COMMENT(this);
  And(destination, source, Operand(0xFFFFFFFF));
  Add64(destination, kPtrComprCageBaseRegister, Operand(destination));
}

void TurboAssembler::DecompressAnyTagged(const Register& destination,
                                         const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Lwu(destination, field_operand);
  Add64(destination, kPtrComprCageBaseRegister, destination);
}

void MacroAssembler::DropArguments(Register count, ArgumentsCountType type,
                                   ArgumentsCountMode mode, Register scratch) {
  switch (type) {
    case kCountIsInteger: {
      CalcScaledAddress(sp, sp, count, kPointerSizeLog2);
      break;
    }
    case kCountIsSmi: {
      STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
      DCHECK_NE(scratch, no_reg);
      SmiScale(scratch, count, kPointerSizeLog2);
      Add64(sp, sp, scratch);
      break;
    }
    case kCountIsBytes: {
      Add64(sp, sp, count);
      break;
    }
  }
  if (mode == kCountExcludesReceiver) {
    Add64(sp, sp, kSystemPointerSize);
  }
}

void MacroAssembler::DropArgumentsAndPushNewReceiver(Register argc,
                                                     Register receiver,
                                                     ArgumentsCountType type,
                                                     ArgumentsCountMode mode,
                                                     Register scratch) {
  DCHECK(!AreAliased(argc, receiver));
  if (mode == kCountExcludesReceiver) {
    // Drop arguments without receiver and override old receiver.
    DropArguments(argc, type, kCountIncludesReceiver, scratch);
    Sd(receiver, MemOperand(sp));
  } else {
    DropArguments(argc, type, mode, scratch);
    push(receiver);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64
