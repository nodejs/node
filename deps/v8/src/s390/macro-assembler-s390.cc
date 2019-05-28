// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_S390

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-factory.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frames-inl.h"
#include "src/heap/heap-inl.h"  // For MemoryChunk.
#include "src/macro-assembler.h"
#include "src/objects/smi.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/embedded-data.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-code-manager.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/s390/macro-assembler-s390.h"
#endif

namespace v8 {
namespace internal {

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
    bytes += NumRegs(kCallerSavedDoubles) * kDoubleSize;
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
    MultiPushDoubles(kCallerSavedDoubles);
    bytes += NumRegs(kCallerSavedDoubles) * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    MultiPopDoubles(kCallerSavedDoubles);
    bytes += NumRegs(kCallerSavedDoubles) * kDoubleSize;
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

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));

  const uint32_t offset =
      FixedArray::kHeaderSize + constant_index * kPointerSize - kHeapObjectTag;

  CHECK(is_uint19(offset));
  DCHECK_NE(destination, r0);
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadP(destination, MemOperand(destination, offset), r1);
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  LoadP(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    LoadRR(destination, kRootRegister);
  } else if (is_uint12(offset)) {
    la(destination, MemOperand(kRootRegister, offset));
  } else {
    DCHECK(is_int20(offset));
    lay(destination, MemOperand(kRootRegister, offset));
  }
}

void TurboAssembler::Jump(Register target, Condition cond) { b(cond, target); }

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip);

  DCHECK(rmode == RelocInfo::CODE_TARGET || rmode == RelocInfo::RUNTIME_ENTRY);

  mov(ip, Operand(target, rmode));
  b(ip);

  bind(&skip);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(static_cast<intptr_t>(target), rmode, cond);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);

  if (options().inline_offheap_trampolines &&
             target_is_isolate_independent_builtin) {
    Label skip;
    if (cond != al) {
      b(NegateCondition(cond), &skip, Label::kNear);
    }
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin_index);
    CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
    EmbeddedData d = EmbeddedData::FromBlob();
    Address entry = d.InstructionStartOfBuiltin(builtin_index);
    mov(ip, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
    b(ip);
    bind(&skip);
    return;
  }
  jump(code, RelocInfo::RELATIVE_CODE_TARGET, cond);
}

void TurboAssembler::Call(Register target) {
  // Branch to target via indirect branch
  basr(r14, target);
}

void MacroAssembler::CallJSEntry(Register target) {
  DCHECK(target == r4);
  Call(target);
}

int MacroAssembler::CallSizeNotPredictableCodeSize(Address target,
                                                   RelocInfo::Mode rmode,
                                                   Condition cond) {
  // S390 Assembler::move sequence is IILF / IIHF
  int size;
#if V8_TARGET_ARCH_S390X
  size = 14;  // IILF + IIHF + BASR
#else
  size = 8;  // IILF + BASR
#endif
  return size;
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(cond == al);

  mov(ip, Operand(target, rmode));
  basr(r14, ip);
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode) && cond == al);

  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);

  if (options().inline_offheap_trampolines &&
             target_is_isolate_independent_builtin) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin_index);
    CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
    EmbeddedData d = EmbeddedData::FromBlob();
    Address entry = d.InstructionStartOfBuiltin(builtin_index);
    mov(ip, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
    Call(ip);
    return;
  }
  call(code, rmode);
}

void TurboAssembler::Drop(int count) {
  if (count > 0) {
    int total = count * kPointerSize;
    if (is_uint12(total)) {
      la(sp, MemOperand(sp, total));
    } else if (is_int20(total)) {
      lay(sp, MemOperand(sp, total));
    } else {
      AddP(sp, Operand(total));
    }
  }
}

void TurboAssembler::Drop(Register count, Register scratch) {
  ShiftLeftP(scratch, count, Operand(kPointerSizeLog2));
  AddP(sp, sp, scratch);
}

void TurboAssembler::Call(Label* target) { b(r14, target); }

void TurboAssembler::Push(Handle<HeapObject> handle) {
  mov(r0, Operand(handle));
  push(r0);
}

void TurboAssembler::Push(Smi smi) {
  mov(r0, Operand(smi));
  push(r0);
}

void TurboAssembler::Move(Register dst, Handle<HeapObject> value) {
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadConstant(dst, value);
      return;
    }
  }
  mov(dst, Operand(value));
}

void TurboAssembler::Move(Register dst, ExternalReference reference) {
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadExternalReference(dst, reference);
      return;
    }
  }
  mov(dst, Operand(reference));
}

void TurboAssembler::Move(Register dst, Register src, Condition cond) {
  if (dst != src) {
    if (cond == al) {
      LoadRR(dst, src);
    } else {
      LoadOnConditionP(cond, dst, src);
    }
  }
}

void TurboAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (dst != src) {
    ldr(dst, src);
  }
}

// Wrapper around Assembler::mvc (SS-a format)
void TurboAssembler::MoveChar(const MemOperand& opnd1,
                              const MemOperand& opnd2,
                              const Operand& length) {
  mvc(opnd1, opnd2, Operand(static_cast<intptr_t>(length.immediate() - 1)));
}

// Wrapper around Assembler::clc (SS-a format)
void TurboAssembler::CompareLogicalChar(const MemOperand& opnd1,
                                        const MemOperand& opnd2,
                                        const Operand& length) {
  clc(opnd1, opnd2, Operand(static_cast<intptr_t>(length.immediate() - 1)));
}

// Wrapper around Assembler::xc (SS-a format)
void TurboAssembler::ExclusiveOrChar(const MemOperand& opnd1,
                                     const MemOperand& opnd2,
                                     const Operand& length) {
  xc(opnd1, opnd2, Operand(static_cast<intptr_t>(length.immediate() - 1)));
}

// Wrapper around Assembler::risbg(n) (RIE-f)
void TurboAssembler::RotateInsertSelectBits(Register dst, Register src,
                               const Operand& startBit, const Operand& endBit,
                               const Operand& shiftAmt, bool zeroBits) {
  if (zeroBits)
    // High tag the top bit of I4/EndBit to zero out any unselected bits
    risbg(dst, src, startBit,
          Operand(static_cast<intptr_t>(endBit.immediate() | 0x80)), shiftAmt);
  else
    risbg(dst, src, startBit, endBit, shiftAmt);
}

void TurboAssembler::BranchRelativeOnIdxHighP(Register dst, Register inc,
                                              Label* L) {
#if V8_TARGET_ARCH_S390X
  brxhg(dst, inc, L);
#else
  brxh(dst, inc, L);
#endif // V8_TARGET_ARCH_S390X
}

void TurboAssembler::MultiPush(RegList regs, Register location) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  SubP(location, location, Operand(stack_offset));
  for (int16_t i = Register::kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      StoreP(ToRegister(i), MemOperand(location, stack_offset));
    }
  }
}

void TurboAssembler::MultiPop(RegList regs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < Register::kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      LoadP(ToRegister(i), MemOperand(location, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  AddP(location, location, Operand(stack_offset));
}

void TurboAssembler::MultiPushDoubles(RegList dregs, Register location) {
  int16_t num_to_push = base::bits::CountPopulation(dregs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  SubP(location, location, Operand(stack_offset));
  for (int16_t i = DoubleRegister::kNumRegisters - 1; i >= 0; i--) {
    if ((dregs & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      stack_offset -= kDoubleSize;
      StoreDouble(dreg, MemOperand(location, stack_offset));
    }
  }
}

void TurboAssembler::MultiPopDoubles(RegList dregs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < DoubleRegister::kNumRegisters; i++) {
    if ((dregs & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      LoadDouble(dreg, MemOperand(location, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  AddP(location, location, Operand(stack_offset));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition) {
  LoadP(destination,
        MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)), r0);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register dst,
                                      LinkRegisterStatus lr_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
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

  lay(dst, MemOperand(object, offset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    AndP(r0, dst, Operand(kPointerSize - 1));
    beq(&ok, Label::kNear);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object, dst, value, lr_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 4)));
    mov(dst, Operand(bit_cast<intptr_t>(kZapValue + 8)));
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

// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(object != value);
  if (emit_debug_code()) {
    CmpP(value, MemOperand(address));
    Check(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite);
  }

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }
  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(r14);
  }
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r14);
  }

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  int fp_delta = 0;
  CleanseP(r14);
  if (marker_reg.is_valid()) {
    Push(r14, fp, marker_reg);
    fp_delta = 1;
  } else {
    Push(r14, fp);
    fp_delta = 0;
  }
  la(fp, MemOperand(sp, fp_delta * kPointerSize));
}

void TurboAssembler::PopCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Pop(r14, fp, marker_reg);
  } else {
    Pop(r14, fp);
  }
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  int fp_delta = 0;
  CleanseP(r14);
  if (function_reg.is_valid()) {
    Push(r14, fp, cp, function_reg);
    fp_delta = 2;
  } else {
    Push(r14, fp, cp);
    fp_delta = 1;
  }
  la(fp, MemOperand(sp, fp_delta * kPointerSize));
}

void TurboAssembler::RestoreFrameStateForTailCall() {
  // if (FLAG_enable_embedded_constant_pool) {
  //   LoadP(kConstantPoolRegister,
  //         MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
  //   set_constant_pool_available(false);
  // }
  DCHECK(!FLAG_enable_embedded_constant_pool);
  LoadP(r14, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
}

// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK_GE(num_unsaved, 0);
  if (num_unsaved > 0) {
    lay(sp, MemOperand(sp, -(num_unsaved * kPointerSize)));
  }
  MultiPush(kSafepointSavedRegisters);
}

void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  MultiPop(kSafepointSavedRegisters);
  if (num_unsaved > 0) {
    la(sp, MemOperand(sp, num_unsaved * kPointerSize));
  }
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  RegList regs = kSafepointSavedRegisters;
  int index = 0;

  DCHECK(reg_code >= 0 && reg_code < kNumRegisters);

  for (int16_t i = 0; i < reg_code; i++) {
    if ((regs & (1 << i)) != 0) {
      index++;
    }
  }

  return index;
}

void TurboAssembler::CanonicalizeNaN(const DoubleRegister dst,
                                     const DoubleRegister src) {
  // Turn potential sNaN into qNaN
  if (dst != src) ldr(dst, src);
  lzdr(kDoubleRegZero);
  sdbr(dst, kDoubleRegZero);
}

void TurboAssembler::ConvertIntToDouble(DoubleRegister dst, Register src) {
  cdfbr(dst, src);
}

void TurboAssembler::ConvertUnsignedIntToDouble(DoubleRegister dst,
                                                Register src) {
  if (CpuFeatures::IsSupported(FLOATING_POINT_EXT)) {
    cdlfbr(Condition(5), Condition(0), dst, src);
  } else {
    // zero-extend src
    llgfr(src, src);
    // convert to double
    cdgbr(dst, src);
  }
}

void TurboAssembler::ConvertIntToFloat(DoubleRegister dst, Register src) {
  cefbra(Condition(4), dst, src);
}

void TurboAssembler::ConvertUnsignedIntToFloat(DoubleRegister dst,
                                               Register src) {
  celfbr(Condition(4), Condition(0), dst, src);
}

void TurboAssembler::ConvertInt64ToFloat(DoubleRegister double_dst,
                                         Register src) {
  cegbr(double_dst, src);
}

void TurboAssembler::ConvertInt64ToDouble(DoubleRegister double_dst,
                                          Register src) {
  cdgbr(double_dst, src);
}

void TurboAssembler::ConvertUnsignedInt64ToFloat(DoubleRegister double_dst,
                                                 Register src) {
  celgbr(Condition(0), Condition(0), double_dst, src);
}

void TurboAssembler::ConvertUnsignedInt64ToDouble(DoubleRegister double_dst,
                                                  Register src) {
  cdlgbr(Condition(0), Condition(0), double_dst, src);
}

void TurboAssembler::ConvertFloat32ToInt64(const Register dst,
                                           const DoubleRegister double_input,
                                           FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  cgebr(m, dst, double_input);
}

void TurboAssembler::ConvertDoubleToInt64(const Register dst,
                                          const DoubleRegister double_input,
                                          FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  cgdbr(m, dst, double_input);
}

void TurboAssembler::ConvertDoubleToInt32(const Register dst,
                                          const DoubleRegister double_input,
                                          FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      m = Condition(4);
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
#ifdef V8_TARGET_ARCH_S390X
  lghi(dst, Operand::Zero());
#endif
  cfdbr(m, dst, double_input);
}

void TurboAssembler::ConvertFloat32ToInt32(const Register result,
                                           const DoubleRegister double_input,
                                           FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      m = Condition(4);
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
#ifdef V8_TARGET_ARCH_S390X
  lghi(result, Operand::Zero());
#endif
  cfebr(m, result, double_input);
}

void TurboAssembler::ConvertFloat32ToUnsignedInt32(
    const Register result, const DoubleRegister double_input,
    FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
#ifdef V8_TARGET_ARCH_S390X
  lghi(result, Operand::Zero());
#endif
  clfebr(m, Condition(0), result, double_input);
}

void TurboAssembler::ConvertFloat32ToUnsignedInt64(
    const Register result, const DoubleRegister double_input,
    FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  clgebr(m, Condition(0), result, double_input);
}

void TurboAssembler::ConvertDoubleToUnsignedInt64(
    const Register dst, const DoubleRegister double_input,
    FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  clgdbr(m, Condition(0), dst, double_input);
}

void TurboAssembler::ConvertDoubleToUnsignedInt32(
    const Register dst, const DoubleRegister double_input,
    FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
#ifdef V8_TARGET_ARCH_S390X
  lghi(dst, Operand::Zero());
#endif
  clfdbr(m, Condition(0), dst, double_input);
}

#if !V8_TARGET_ARCH_S390X
void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  sldl(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  sldl(r0, r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srdl(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srdl(r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void TurboAssembler::ShiftRightArithPair(Register dst_low, Register dst_high,
                                         Register src_low, Register src_high,
                                         Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srda(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void TurboAssembler::ShiftRightArithPair(Register dst_low, Register dst_high,
                                         Register src_low, Register src_high,
                                         uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srda(r0, r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}
#endif

void TurboAssembler::MovDoubleToInt64(Register dst, DoubleRegister src) {
  lgdr(dst, src);
}

void TurboAssembler::MovInt64ToDouble(DoubleRegister dst, Register src) {
  ldgr(dst, src);
}

void TurboAssembler::StubPrologue(StackFrame::Type type, Register base,
                                  int prologue_offset) {
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(this);
    Load(r1, Operand(StackFrame::TypeToMarker(type)));
    PushCommonFrame(r1);
  }
}

void TurboAssembler::Prologue(Register base, int prologue_offset) {
  DCHECK(base != no_reg);
  PushStandardFrame(r3);
}

void TurboAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // We create a stack frame with:
  //    Return Addr <-- old sp
  //    Old FP      <-- new fp
  //    CP
  //    type
  //    CodeObject  <-- new sp

  Load(ip, Operand(StackFrame::TypeToMarker(type)));
  PushCommonFrame(ip);
}

int TurboAssembler::LeaveFrame(StackFrame::Type type, int stack_adjustment) {
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer, return address and constant pool pointer.
  LoadP(r14, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  if (is_int20(StandardFrameConstants::kCallerSPOffset + stack_adjustment)) {
    lay(r1, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                               stack_adjustment));
  } else {
    AddP(r1, fp,
         Operand(StandardFrameConstants::kCallerSPOffset + stack_adjustment));
  }
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  LoadRR(sp, r1);
  int frame_ends = pc_offset();
  return frame_ends;
}

// ExitFrame layout (probably wrongish.. needs updating)
//
//  SP -> previousSP
//        LK reserved
//        sp_on_exit (for debug?)
// oldSP->prev SP
//        LK
//        <parameters on stack>

// Prior to calling EnterExitFrame, we've got a bunch of parameters
// on the stack that we need to wrap a real frame around.. so first
// we reserve a slot for LK and push the previous SP which is captured
// in the fp register (r11)
// Then - we buy a new frame

// r14
// oldFP <- newFP
// SP
// Floats
// gaps
// Args
// ABIRes <- newSP
void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);
  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  DCHECK_GT(stack_space, 0);

  // This is an opportunity to build a frame to wrap
  // all of the pushes that have happened inside of V8
  // since we were called from C code
  CleanseP(r14);
  Load(r1, Operand(StackFrame::TypeToMarker(frame_type)));
  PushCommonFrame(r1);
  // Reserve room for saved entry sp.
  lay(sp, MemOperand(fp, -ExitFrameConstants::kFixedFrameSizeFromFp));

  if (emit_debug_code()) {
    StoreP(MemOperand(fp, ExitFrameConstants::kSPOffset), Operand::Zero(), r1);
  }

  // Save the frame pointer and the context in top.
  Move(r1, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                     isolate()));
  StoreP(fp, MemOperand(r1));
  Move(r1,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  StoreP(cp, MemOperand(r1));

  // Optionally save all volatile double registers.
  if (save_doubles) {
    MultiPushDoubles(kCallerSavedDoubles);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   kNumCallerSavedDoubles * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
  }

  lay(sp, MemOperand(sp, -stack_space * kPointerSize));

  // Allocate and align the frame preparing for calling the runtime
  // function.
  const int frame_alignment = TurboAssembler::ActivationFrameAlignment();
  if (frame_alignment > 0) {
    DCHECK_EQ(frame_alignment, 8);
    ClearRightImm(sp, sp, Operand(3));  // equivalent to &= -8
  }

  lay(sp, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));
  StoreP(MemOperand(sp), Operand::Zero(), r0);
  // Set the exit frame sp value to point just before the return address
  // location.
  lay(r1, MemOperand(sp, kStackFrameSPSlot * kPointerSize));
  StoreP(r1, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

int TurboAssembler::ActivationFrameAlignment() {
#if !defined(USE_SIMULATOR)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one S390
  // platform for another S390 platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // Simulated
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool argument_count_is_length) {
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int kNumRegs = kNumCallerSavedDoubles;
    lay(r5, MemOperand(fp, -(ExitFrameConstants::kFixedFrameSizeFromFp +
                             kNumRegs * kDoubleSize)));
    MultiPopDoubles(kCallerSavedDoubles, r5);
  }

  // Clear top frame.
  Move(ip, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                     isolate()));
  StoreP(MemOperand(ip), Operand(0, RelocInfo::NONE), r0);

  // Restore current context from top and clear it in debug mode.
  Move(ip,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  LoadP(cp, MemOperand(ip));

#ifdef DEBUG
  mov(r1, Operand(Context::kInvalidContext));
  Move(ip,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  StoreP(r1, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  LeaveFrame(StackFrame::EXIT);

  if (argument_count.is_valid()) {
    if (!argument_count_is_length) {
      ShiftLeftP(argument_count, argument_count, Operand(kPointerSizeLog2));
    }
    la(sp, MemOperand(sp, argument_count));
  }
}

void TurboAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d0);
}

void TurboAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d0);
}

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
  // after we drop current frame. We AddP kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  ShiftLeftP(dst_reg, caller_args_count_reg, Operand(kPointerSizeLog2));
  AddP(dst_reg, fp, dst_reg);
  AddP(dst_reg, dst_reg,
       Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count_reg;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  if (callee_args_count.is_reg()) {
    ShiftLeftP(src_reg, callee_args_count.reg(), Operand(kPointerSizeLog2));
    AddP(src_reg, sp, src_reg);
    AddP(src_reg, src_reg, Operand(kPointerSize));
  } else {
    mov(src_reg, Operand((callee_args_count.immediate() + 1) * kPointerSize));
    AddP(src_reg, src_reg, sp);
  }

  if (FLAG_debug_code) {
    CmpLogicalP(src_reg, dst_reg);
    Check(lt, AbortReason::kStackAccessBelowStackPointer);
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  RestoreFrameStateForTailCall();

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop;
  if (callee_args_count.is_reg()) {
    AddP(tmp_reg, callee_args_count.reg(), Operand(1));  // +1 for receiver
  } else {
    mov(tmp_reg, Operand(callee_args_count.immediate() + 1));
  }
  LoadRR(r1, tmp_reg);
  bind(&loop);
  LoadP(tmp_reg, MemOperand(src_reg, -kPointerSize));
  StoreP(tmp_reg, MemOperand(dst_reg, -kPointerSize));
  lay(src_reg, MemOperand(src_reg, -kPointerSize));
  lay(dst_reg, MemOperand(dst_reg, -kPointerSize));
  BranchOnCount(r1, &loop);

  // Leave current frame.
  LoadRR(sp, dst_reg);
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
  //  r2: actual arguments count
  //  r3: function (passed through to callee)
  //  r4: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.

  // ARM has some sanity checks as per below, considering add them for S390
  DCHECK(actual.is_immediate() || actual.reg() == r2);
  DCHECK(expected.is_immediate() || expected.reg() == r4);

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    mov(r2, Operand(actual.immediate()));
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
        mov(r4, Operand(expected.immediate()));
      }
    }
  } else {
    if (actual.is_immediate()) {
      mov(r2, Operand(actual.immediate()));
      CmpPH(expected.reg(), Operand(actual.immediate()));
      beq(&regular_invoke);
    } else {
      CmpP(expected.reg(), actual.reg());
      beq(&regular_invoke);
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor);
      if (!*definitely_mismatches) {
        b(done);
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

  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  Move(r6, debug_hook_active);
  tm(MemOperand(r6), Operand::Zero());
  bne(&skip_hook);

  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    if (actual.is_reg()) {
      LoadRR(r6, actual.reg());
    } else {
      mov(r6, Operand(actual.immediate()));
    }
    ShiftLeftP(r6, r6, Operand(kPointerSizeLog2));
    LoadP(r6, MemOperand(sp, r6));
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
    Push(fun, fun, r6);
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

  DCHECK(function == r3);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == r5);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r5, RootIndex::kUndefinedValue);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = kJavaScriptCallCodeStartRegister;
    LoadP(code, FieldMemOperand(function, JSFunction::kCodeOffset));
    if (flag == CALL_FUNCTION) {
      CallCodeObject(code);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      JumpCodeObject(code);
    }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}

void MacroAssembler::InvokeFunction(Register fun, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(fun == r3);

  Register expected_reg = r4;
  Register temp_reg = r6;
  LoadP(temp_reg, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  LoadLogicalHalfWordP(
      expected_reg,
      FieldMemOperand(temp_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(fun, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(function == r3);

  // Get the function and setup the context.
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  InvokeFunctionCode(r3, no_reg, expected, actual, flag);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  Move(r3, restart_fp);
  LoadP(r3, MemOperand(r3));
  CmpP(r3, Operand::Zero());
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne);
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  // Link the current handler as the next handler.
  Move(r7, ExternalReference::Create(IsolateAddressId::kHandlerAddress,
                                            isolate()));

  // Buy the full stack frame for 5 slots.
  lay(sp, MemOperand(sp, -StackHandlerConstants::kSize));

  // Store padding.
  lghi(r0, Operand::Zero());
  StoreP(r0, MemOperand(sp));  // Padding.

  // Copy the old handler into the next handler slot.
  MoveChar(MemOperand(sp, StackHandlerConstants::kNextOffset), MemOperand(r7),
      Operand(kPointerSize));
  // Set this new handler as the current one.
  StoreP(sp, MemOperand(r7));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  // Pop the Next Handler into r3 and store it into Handler Address reference.
  Pop(r3);
  Move(ip, ExternalReference::Create(IsolateAddressId::kHandlerAddress,
                                            isolate()));
  StoreP(r3, MemOperand(ip));

  Drop(1);  // Drop padding.
}

void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  const Register temp = type_reg == no_reg ? r0 : type_reg;

  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}

void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE <= 0xFFFF);
  LoadHalfWordP(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  CmpP(type_reg, Operand(type));
}

void MacroAssembler::CompareRoot(Register obj, RootIndex index) {
  CmpP(obj, MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  if (lower_limit != 0) {
    Register scratch = r0;
    LoadRR(scratch, value);
    slgfi(scratch, Operand(lower_limit));
    CmpLogicalP(scratch, Operand(higher_limit - lower_limit));
  } else {
    CmpLogicalP(value, Operand(higher_limit));
  }
  ble(on_in_range);
}

void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DoubleRegister double_input,
                                           Register scratch,
                                           DoubleRegister double_scratch) {
  Label done;
  DCHECK(double_input != double_scratch);

  ConvertDoubleToInt64(result, double_input);

  TestIfInt32(result);
  bne(&done);

  // convert back and compare
  cdfbr(double_scratch, result);
  cdbr(double_scratch, double_input);
  bind(&done);
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DoubleRegister double_input,
                                       StubCallMode stub_mode) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(r14);
  // Put input on stack.
  lay(sp, MemOperand(sp, -kDoubleSize));
  StoreDouble(double_input, MemOperand(sp));

  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }

  LoadP(result, MemOperand(sp, 0));
  la(sp, MemOperand(sp, kDoubleSize));
  pop(r14);

  bind(&done);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  ConvertDoubleToInt64(result, double_input);

  // Test for overflow
  TestIfInt32(result);
  beq(done);
}

void TurboAssembler::CallRuntimeWithCEntry(Runtime::FunctionId fid,
                                           Register centry) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  mov(r2, Operand(f->nargs));
  Move(r3, ExternalReference::Create(f));
  DCHECK(!AreAliased(centry, r2, r3));
  CallCodeObject(centry);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r2 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r2, Operand(num_arguments));
  Move(r3, ExternalReference::Create(f));
#if V8_TARGET_ARCH_S390X
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
#else
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, save_doubles);
#endif

  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    mov(r2, Operand(function->nargs));
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  Move(r3, builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  mov(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Jump(kOffHeapTrampolineRegister);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  Cmp32(in, Operand(kClearedWeakHeapObjectLower32));
  beq(target_if_cleared);

  AndP(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    Move(scratch2, ExternalReference::Create(counter));
    // @TODO(john.yan): can be optimized by asi()
    LoadW(scratch1, MemOperand(scratch2));
    AddP(scratch1, Operand(value));
    StoreW(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    Move(scratch2, ExternalReference::Create(counter));
    // @TODO(john.yan): can be optimized by asi()
    LoadW(scratch1, MemOperand(scratch2));
    AddP(scratch1, Operand(-value));
    StoreW(scratch1, MemOperand(scratch2));
  }
}

void TurboAssembler::Assert(Condition cond, AbortReason reason, CRegister cr) {
  if (emit_debug_code()) Check(cond, reason, cr);
}

void TurboAssembler::Check(Condition cond, AbortReason reason, CRegister cr) {
  Label L;
  b(cond, &L);
  Abort(reason);
  // will not return here
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
    lgfi(r2, Operand(static_cast<int>(reason)));
    PrepareCallCFunction(1, 0, r3);
    Move(r3, ExternalReference::abort_with_reason());
    // Use Call directly to avoid any unneeded overhead. The function won't
    // return anyway.
    Call(r3);
    return;
  }

  LoadSmiLiteral(r3, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // will not return here
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  LoadP(dst, NativeContextMemOperand());
  LoadP(dst, ContextMemOperand(dst, index));
}

void MacroAssembler::UntagAndJumpIfSmi(Register dst, Register src,
                                       Label* smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // this won't work if src == dst
  DCHECK(src.code() != dst.code());
  SmiUntag(dst, src);
  TestIfSmi(src);
  beq(smi_case);
}

void MacroAssembler::JumpIfEitherSmi(Register reg1, Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  JumpIfSmi(reg1, on_either_smi);
  JumpIfSmi(reg2, on_either_smi);
}

void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, AbortReason::kOperandIsASmi, cr0);
  }
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(eq, AbortReason::kOperandIsNotASmi, cr0);
  }
}

void MacroAssembler::AssertConstructor(Register object, Register scratch) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, AbortReason::kOperandIsASmiAndNotAConstructor);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    tm(FieldMemOperand(scratch, Map::kBitFieldOffset),
       Operand(Map::IsConstructorBit::kMask));
    Check(ne, AbortReason::kOperandIsNotAConstructor);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, AbortReason::kOperandIsASmiAndNotAFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  TestIfSmi(object);
  Check(ne, AbortReason::kOperandIsASmiAndNotAGeneratorObject, cr0);

  // Load map
  Register map = object;
  push(object);
  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));

  // Check if JSGeneratorObject
  Label do_check;
  Register instance_type = object;
  CompareInstanceType(map, instance_type, JS_GENERATOR_OBJECT_TYPE);
  beq(&do_check);

  // Check if JSAsyncFunctionObject (See MacroAssembler::CompareInstanceType)
  CmpP(instance_type, Operand(JS_ASYNC_FUNCTION_OBJECT_TYPE));
  beq(&do_check);

  // Check if JSAsyncGeneratorObject (See MacroAssembler::CompareInstanceType)
  CmpP(instance_type, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

  bind(&do_check);
  // Restore generator object to register and perform assertion
  pop(object);
  Check(eq, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, RootIndex::kUndefinedValue);
    beq(&done_checking, Label::kNear);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(scratch, scratch, ALLOCATION_SITE_TYPE);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

static const int kRegisterPassedArguments = 5;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  if (num_double_arguments > DoubleRegister::kNumRegisters) {
    stack_passed_words +=
        2 * (num_double_arguments - DoubleRegister::kNumRegisters);
  }
  // Up to five simple arguments are passed in registers r2..r6
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
  return stack_passed_words;
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots;
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for stack arguments
    // -- preserving original value of sp.
    LoadRR(scratch, sp);
    lay(sp, MemOperand(sp, -(stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
    StoreP(scratch, MemOperand(sp, (stack_passed_arguments)*kPointerSize));
  } else {
    stack_space += stack_passed_arguments;
  }
  lay(sp, MemOperand(sp, (-stack_space) * kPointerSize));
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void TurboAssembler::MovToFloatParameter(DoubleRegister src) { Move(d0, src); }

void TurboAssembler::MovToFloatResult(DoubleRegister src) { Move(d0, src); }

void TurboAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2 == d0) {
    DCHECK(src1 != d2);
    Move(d2, src2);
    Move(d0, src1);
  } else {
    Move(d0, src1);
    Move(d2, src2);
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  Move(ip, function);
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments);
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

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  if (isolate() != nullptr) {
    Register scratch = r6;
    push(scratch);

    Move(scratch, ExternalReference::fast_c_call_caller_pc_address(isolate()));
    LoadPC(r0);
    StoreP(r0, MemOperand(scratch));
    Move(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    StoreP(fp, MemOperand(scratch));
    pop(scratch);
  }

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Register dest = function;
  if (ABI_CALL_VIA_IP) {
    Move(ip, function);
    dest = ip;
  }

  Call(dest);


  if (isolate() != nullptr) {
    // We don't unset the PC; the FP is the source of truth.
    Register scratch1 = r6;
    Register scratch2 = r7;
    Push(scratch1, scratch2);
    Move(scratch1, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    lghi(scratch2, Operand::Zero());
    StoreP(scratch2, MemOperand(scratch1));
    Pop(scratch1, scratch2);
  }

  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (ActivationFrameAlignment() > kPointerSize) {
    // Load the original stack pointer (pre-alignment) from the stack
    LoadP(sp, MemOperand(sp, stack_space * kPointerSize));
  } else {
    la(sp, MemOperand(sp, stack_space * kPointerSize));
  }
}

void TurboAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask, Condition cc, Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));

  if (base::bits::IsPowerOfTwo(mask)) {
    // If it's a power of two, we can use Test-Under-Mask Memory-Imm form
    // which allows testing of a single byte in memory.
    int32_t byte_offset = 4;
    uint32_t shifted_mask = mask;
    // Determine the byte offset to be tested
    if (mask <= 0x80) {
      byte_offset = kPointerSize - 1;
    } else if (mask < 0x8000) {
      byte_offset = kPointerSize - 2;
      shifted_mask = mask >> 8;
    } else if (mask < 0x800000) {
      byte_offset = kPointerSize - 3;
      shifted_mask = mask >> 16;
    } else {
      byte_offset = kPointerSize - 4;
      shifted_mask = mask >> 24;
    }
#if V8_TARGET_LITTLE_ENDIAN
    // Reverse the byte_offset if emulating on little endian platform
    byte_offset = kPointerSize - byte_offset - 1;
#endif
    tm(MemOperand(scratch, MemoryChunk::kFlagsOffset + byte_offset),
       Operand(shifted_mask));
  } else {
    LoadP(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
    AndP(r0, scratch, Operand(mask));
  }
  // Should be okay to remove rc

  if (cc == ne) {
    bne(condition_met);
  }
  if (cc == eq) {
    beq(condition_met);
  }
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

void TurboAssembler::mov(Register dst, const Operand& src) {
#if V8_TARGET_ARCH_S390X
  int64_t value;
#else
  int value;
#endif
  if (src.is_heap_object_request()) {
    RequestHeapObject(src.heap_object_request());
    value = 0;
  } else {
    value = src.immediate();
  }

  if (src.rmode() != RelocInfo::NONE) {
    // some form of relocation needed
    RecordRelocInfo(src.rmode(), value);
  }

#if V8_TARGET_ARCH_S390X
  int32_t hi_32 = static_cast<int64_t>(value) >> 32;
  int32_t lo_32 = static_cast<int32_t>(value);

  iihf(dst, Operand(hi_32));
  iilf(dst, Operand(lo_32));
#else
  iilf(dst, Operand(value));
#endif
}

void TurboAssembler::Mul32(Register dst, const MemOperand& src1) {
  if (is_uint12(src1.offset())) {
    ms(dst, src1);
  } else if (is_int20(src1.offset())) {
    msy(dst, src1);
  } else {
    UNIMPLEMENTED();
  }
}

void TurboAssembler::Mul32(Register dst, Register src1) { msr(dst, src1); }

void TurboAssembler::Mul32(Register dst, const Operand& src1) {
  msfi(dst, src1);
}

#define Generate_MulHigh32(instr) \
  {                               \
    lgfr(dst, src1);              \
    instr(dst, src2);             \
    srlg(dst, dst, Operand(32));  \
  }

void TurboAssembler::MulHigh32(Register dst, Register src1,
                               const MemOperand& src2) {
  Generate_MulHigh32(msgf);
}

void TurboAssembler::MulHigh32(Register dst, Register src1, Register src2) {
  if (dst == src2) {
    std::swap(src1, src2);
  }
  Generate_MulHigh32(msgfr);
}

void TurboAssembler::MulHigh32(Register dst, Register src1,
                               const Operand& src2) {
  Generate_MulHigh32(msgfi);
}

#undef Generate_MulHigh32

#define Generate_MulHighU32(instr) \
  {                                \
    lr(r1, src1);                  \
    instr(r0, src2);               \
    LoadlW(dst, r0);               \
  }

void TurboAssembler::MulHighU32(Register dst, Register src1,
                                const MemOperand& src2) {
  Generate_MulHighU32(ml);
}

void TurboAssembler::MulHighU32(Register dst, Register src1, Register src2) {
  Generate_MulHighU32(mlr);
}

void TurboAssembler::MulHighU32(Register dst, Register src1,
                                const Operand& src2) {
  USE(dst);
  USE(src1);
  USE(src2);
  UNREACHABLE();
}

#undef Generate_MulHighU32

#define Generate_Mul32WithOverflowIfCCUnequal(instr) \
  {                                                  \
    lgfr(dst, src1);                                 \
    instr(dst, src2);                                \
    cgfr(dst, dst);                                  \
  }

void TurboAssembler::Mul32WithOverflowIfCCUnequal(Register dst, Register src1,
                                                  const MemOperand& src2) {
  Register result = dst;
  if (src2.rx() == dst || src2.rb() == dst) dst = r0;
  Generate_Mul32WithOverflowIfCCUnequal(msgf);
  if (result != dst) llgfr(result, dst);
}

void TurboAssembler::Mul32WithOverflowIfCCUnequal(Register dst, Register src1,
                                                  Register src2) {
  if (dst == src2) {
    std::swap(src1, src2);
  }
  Generate_Mul32WithOverflowIfCCUnequal(msgfr);
}

void TurboAssembler::Mul32WithOverflowIfCCUnequal(Register dst, Register src1,
                                                  const Operand& src2) {
  Generate_Mul32WithOverflowIfCCUnequal(msgfi);
}

#undef Generate_Mul32WithOverflowIfCCUnequal

void TurboAssembler::Mul64(Register dst, const MemOperand& src1) {
  if (is_int20(src1.offset())) {
    msg(dst, src1);
  } else {
    UNIMPLEMENTED();
  }
}

void TurboAssembler::Mul64(Register dst, Register src1) { msgr(dst, src1); }

void TurboAssembler::Mul64(Register dst, const Operand& src1) {
  msgfi(dst, src1);
}

void TurboAssembler::Mul(Register dst, Register src1, Register src2) {
  if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
    MulPWithCondition(dst, src1, src2);
  } else {
    if (dst == src2) {
      MulP(dst, src1);
    } else if (dst == src1) {
      MulP(dst, src2);
    } else {
      Move(dst, src1);
      MulP(dst, src2);
    }
  }
}

void TurboAssembler::DivP(Register dividend, Register divider) {
  // have to make sure the src and dst are reg pairs
  DCHECK_EQ(dividend.code() % 2, 0);
#if V8_TARGET_ARCH_S390X
  dsgr(dividend, divider);
#else
  dr(dividend, divider);
#endif
}

#define Generate_Div32(instr) \
  {                           \
    lgfr(r1, src1);           \
    instr(r0, src2);          \
    LoadlW(dst, r1);          \
  }

void TurboAssembler::Div32(Register dst, Register src1,
                           const MemOperand& src2) {
  Generate_Div32(dsgf);
}

void TurboAssembler::Div32(Register dst, Register src1, Register src2) {
  Generate_Div32(dsgfr);
}

#undef Generate_Div32

#define Generate_DivU32(instr) \
  {                            \
    lr(r0, src1);              \
    srdl(r0, Operand(32)); \
    instr(r0, src2);           \
    LoadlW(dst, r1);           \
  }

void TurboAssembler::DivU32(Register dst, Register src1,
                            const MemOperand& src2) {
  Generate_DivU32(dl);
}

void TurboAssembler::DivU32(Register dst, Register src1, Register src2) {
  Generate_DivU32(dlr);
}

#undef Generate_DivU32

#define Generate_Div64(instr) \
  {                           \
    lgr(r1, src1);            \
    instr(r0, src2);          \
    lgr(dst, r1);             \
  }

void TurboAssembler::Div64(Register dst, Register src1,
                           const MemOperand& src2) {
  Generate_Div64(dsg);
}

void TurboAssembler::Div64(Register dst, Register src1, Register src2) {
  Generate_Div64(dsgr);
}

#undef Generate_Div64

#define Generate_DivU64(instr) \
  {                            \
    lgr(r1, src1);             \
    lghi(r0, Operand::Zero()); \
    instr(r0, src2);           \
    lgr(dst, r1);              \
  }

void TurboAssembler::DivU64(Register dst, Register src1,
                            const MemOperand& src2) {
  Generate_DivU64(dlg);
}

void TurboAssembler::DivU64(Register dst, Register src1, Register src2) {
  Generate_DivU64(dlgr);
}

#undef Generate_DivU64

#define Generate_Mod32(instr) \
  {                           \
    lgfr(r1, src1);           \
    instr(r0, src2);          \
    LoadlW(dst, r0);          \
  }

void TurboAssembler::Mod32(Register dst, Register src1,
                           const MemOperand& src2) {
  Generate_Mod32(dsgf);
}

void TurboAssembler::Mod32(Register dst, Register src1, Register src2) {
  Generate_Mod32(dsgfr);
}

#undef Generate_Mod32

#define Generate_ModU32(instr) \
  {                            \
    lr(r0, src1);              \
    srdl(r0, Operand(32)); \
    instr(r0, src2);           \
    LoadlW(dst, r0);           \
  }

void TurboAssembler::ModU32(Register dst, Register src1,
                            const MemOperand& src2) {
  Generate_ModU32(dl);
}

void TurboAssembler::ModU32(Register dst, Register src1, Register src2) {
  Generate_ModU32(dlr);
}

#undef Generate_ModU32

#define Generate_Mod64(instr) \
  {                           \
    lgr(r1, src1);            \
    instr(r0, src2);          \
    lgr(dst, r0);             \
  }

void TurboAssembler::Mod64(Register dst, Register src1,
                           const MemOperand& src2) {
  Generate_Mod64(dsg);
}

void TurboAssembler::Mod64(Register dst, Register src1, Register src2) {
  Generate_Mod64(dsgr);
}

#undef Generate_Mod64

#define Generate_ModU64(instr) \
  {                            \
    lgr(r1, src1);             \
    lghi(r0, Operand::Zero()); \
    instr(r0, src2);           \
    lgr(dst, r0);              \
  }

void TurboAssembler::ModU64(Register dst, Register src1,
                            const MemOperand& src2) {
  Generate_ModU64(dlg);
}

void TurboAssembler::ModU64(Register dst, Register src1, Register src2) {
  Generate_ModU64(dlgr);
}

#undef Generate_ModU64

void TurboAssembler::MulP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  msgfi(dst, opnd);
#else
  msfi(dst, opnd);
#endif
}

void TurboAssembler::MulP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  msgr(dst, src);
#else
  msr(dst, src);
#endif
}

void TurboAssembler::MulPWithCondition(Register dst, Register src1,
                                       Register src2) {
  CHECK(CpuFeatures::IsSupported(MISC_INSTR_EXT2));
#if V8_TARGET_ARCH_S390X
  msgrkc(dst, src1, src2);
#else
  msrkc(dst, src1, src2);
#endif
}

void TurboAssembler::MulP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (is_uint16(opnd.offset())) {
    ms(dst, opnd);
  } else if (is_int20(opnd.offset())) {
    msy(dst, opnd);
  } else {
    UNIMPLEMENTED();
  }
#else
  if (is_int20(opnd.offset())) {
    msg(dst, opnd);
  } else {
    UNIMPLEMENTED();
  }
#endif
}

void TurboAssembler::Sqrt(DoubleRegister result, DoubleRegister input) {
  sqdbr(result, input);
}
void TurboAssembler::Sqrt(DoubleRegister result, const MemOperand& input) {
  if (is_uint12(input.offset())) {
    sqdb(result, input);
  } else {
    ldy(result, input);
    sqdbr(result, result);
  }
}
//----------------------------------------------------------------------------
//  Add Instructions
//----------------------------------------------------------------------------

// Add 32-bit (Register dst = Register dst + Immediate opnd)
void TurboAssembler::Add32(Register dst, const Operand& opnd) {
  if (is_int16(opnd.immediate()))
    ahi(dst, opnd);
  else
    afi(dst, opnd);
}

// Add 32-bit (Register dst = Register dst + Immediate opnd)
void TurboAssembler::Add32_RI(Register dst, const Operand& opnd) {
  // Just a wrapper for above
  Add32(dst, opnd);
}

// Add Pointer Size (Register dst = Register dst + Immediate opnd)
void TurboAssembler::AddP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (is_int16(opnd.immediate()))
    aghi(dst, opnd);
  else
    agfi(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}

// Add 32-bit (Register dst = Register src + Immediate opnd)
void TurboAssembler::Add32(Register dst, Register src, const Operand& opnd) {
  if (dst != src) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      ahik(dst, src, opnd);
      return;
    }
    lr(dst, src);
  }
  Add32(dst, opnd);
}

// Add 32-bit (Register dst = Register src + Immediate opnd)
void TurboAssembler::Add32_RRI(Register dst, Register src,
                               const Operand& opnd) {
  // Just a wrapper for above
  Add32(dst, src, opnd);
}

// Add Pointer Size (Register dst = Register src + Immediate opnd)
void TurboAssembler::AddP(Register dst, Register src, const Operand& opnd) {
  if (dst != src) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      AddPImm_RRI(dst, src, opnd);
      return;
    }
    LoadRR(dst, src);
  }
  AddP(dst, opnd);
}

// Add 32-bit (Register dst = Register dst + Register src)
void TurboAssembler::Add32(Register dst, Register src) { ar(dst, src); }

// Add Pointer Size (Register dst = Register dst + Register src)
void TurboAssembler::AddP(Register dst, Register src) { AddRR(dst, src); }

// Add Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) + Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::AddP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  agfr(dst, src);
#else
  ar(dst, src);
#endif
}

// Add 32-bit (Register dst = Register src1 + Register src2)
void TurboAssembler::Add32(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ark(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  ar(dst, src2);
}

// Add Pointer Size (Register dst = Register src1 + Register src2)
void TurboAssembler::AddP(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AddP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  AddRR(dst, src2);
}

// Add Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) + Register src1 (ptr) +
//                            Register src2 (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::AddP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (dst == src2) {
    // The source we need to sign extend is the same as result.
    lgfr(dst, src2);
    agr(dst, src1);
  } else {
    if (dst != src1) LoadRR(dst, src1);
    agfr(dst, src2);
  }
#else
  AddP(dst, src1, src2);
#endif
}

// Add 32-bit (Register-Memory)
void TurboAssembler::Add32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    a(dst, opnd);
  else
    ay(dst, opnd);
}

// Add Pointer Size (Register-Memory)
void TurboAssembler::AddP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  ag(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}

// Add Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) + Mem opnd (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::AddP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  agf(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}

// Add 32-bit (Memory - Immediate)
void TurboAssembler::Add32(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  DCHECK(CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
  asi(opnd, imm);
}

// Add Pointer-sized (Memory - Immediate)
void TurboAssembler::AddP(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  DCHECK(CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
#if V8_TARGET_ARCH_S390X
  agsi(opnd, imm);
#else
  asi(opnd, imm);
#endif
}

//----------------------------------------------------------------------------
//  Add Logical Instructions
//----------------------------------------------------------------------------

// Add Logical With Carry 32-bit (Register dst = Register src1 + Register src2)
void TurboAssembler::AddLogicalWithCarry32(Register dst, Register src1,
                                           Register src2) {
  if (dst != src2 && dst != src1) {
    lr(dst, src1);
    alcr(dst, src2);
  } else if (dst != src2) {
    // dst == src1
    DCHECK(dst == src1);
    alcr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst == src2);
    alcr(dst, src1);
  }
}

// Add Logical 32-bit (Register dst = Register src1 + Register src2)
void TurboAssembler::AddLogical32(Register dst, Register src1, Register src2) {
  if (dst != src2 && dst != src1) {
    lr(dst, src1);
    alr(dst, src2);
  } else if (dst != src2) {
    // dst == src1
    DCHECK(dst == src1);
    alr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst == src2);
    alr(dst, src1);
  }
}

// Add Logical 32-bit (Register dst = Register dst + Immediate opnd)
void TurboAssembler::AddLogical(Register dst, const Operand& imm) {
  alfi(dst, imm);
}

// Add Logical Pointer Size (Register dst = Register dst + Immediate opnd)
void TurboAssembler::AddLogicalP(Register dst, const Operand& imm) {
#ifdef V8_TARGET_ARCH_S390X
  algfi(dst, imm);
#else
  AddLogical(dst, imm);
#endif
}

// Add Logical 32-bit (Register-Memory)
void TurboAssembler::AddLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    al_z(dst, opnd);
  else
    aly(dst, opnd);
}

// Add Logical Pointer Size (Register-Memory)
void TurboAssembler::AddLogicalP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  alg(dst, opnd);
#else
  AddLogical(dst, opnd);
#endif
}

//----------------------------------------------------------------------------
//  Subtract Instructions
//----------------------------------------------------------------------------

// Subtract Logical With Carry 32-bit (Register dst = Register src1 - Register
// src2)
void TurboAssembler::SubLogicalWithBorrow32(Register dst, Register src1,
                                            Register src2) {
  if (dst != src2 && dst != src1) {
    lr(dst, src1);
    slbr(dst, src2);
  } else if (dst != src2) {
    // dst == src1
    DCHECK(dst == src1);
    slbr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst == src2);
    lr(r0, dst);
    SubLogicalWithBorrow32(dst, src1, r0);
  }
}

// Subtract Logical 32-bit (Register dst = Register src1 - Register src2)
void TurboAssembler::SubLogical32(Register dst, Register src1, Register src2) {
  if (dst != src2 && dst != src1) {
    lr(dst, src1);
    slr(dst, src2);
  } else if (dst != src2) {
    // dst == src1
    DCHECK(dst == src1);
    slr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst == src2);
    lr(r0, dst);
    SubLogical32(dst, src1, r0);
  }
}

// Subtract 32-bit (Register dst = Register dst - Immediate opnd)
void TurboAssembler::Sub32(Register dst, const Operand& imm) {
  Add32(dst, Operand(-(imm.immediate())));
}

// Subtract Pointer Size (Register dst = Register dst - Immediate opnd)
void TurboAssembler::SubP(Register dst, const Operand& imm) {
  AddP(dst, Operand(-(imm.immediate())));
}

// Subtract 32-bit (Register dst = Register src - Immediate opnd)
void TurboAssembler::Sub32(Register dst, Register src, const Operand& imm) {
  Add32(dst, src, Operand(-(imm.immediate())));
}

// Subtract Pointer Sized (Register dst = Register src - Immediate opnd)
void TurboAssembler::SubP(Register dst, Register src, const Operand& imm) {
  AddP(dst, src, Operand(-(imm.immediate())));
}

// Subtract 32-bit (Register dst = Register dst - Register src)
void TurboAssembler::Sub32(Register dst, Register src) { sr(dst, src); }

// Subtract Pointer Size (Register dst = Register dst - Register src)
void TurboAssembler::SubP(Register dst, Register src) { SubRR(dst, src); }

// Subtract Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) - Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::SubP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  sgfr(dst, src);
#else
  sr(dst, src);
#endif
}

// Subtract 32-bit (Register = Register - Register)
void TurboAssembler::Sub32(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srk(dst, src1, src2);
    return;
  }
  if (dst != src1 && dst != src2) lr(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (dst != src1 && dst == src2) {
    Label done;
    lcr(dst, dst);  // dst = -dst
    b(overflow, &done);
    ar(dst, src1);  // dst = dst + src
    bind(&done);
  } else {
    sr(dst, src2);
  }
}

// Subtract Pointer Sized (Register = Register - Register)
void TurboAssembler::SubP(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    SubP_RRR(dst, src1, src2);
    return;
  }
  if (dst != src1 && dst != src2) LoadRR(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (dst != src1 && dst == src2) {
    Label done;
    LoadComplementRR(dst, dst);  // dst = -dst
    b(overflow, &done);
    AddP(dst, src1);  // dst = dst + src
    bind(&done);
  } else {
    SubP(dst, src2);
  }
}

// Subtract Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) - Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::SubP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (dst != src1 && dst != src2) LoadRR(dst, src1);

  // In scenario where we have dst = src - dst, we need to swap and negate
  if (dst != src1 && dst == src2) {
    lgfr(dst, dst);              // Sign extend this operand first.
    LoadComplementRR(dst, dst);  // dst = -dst
    AddP(dst, src1);             // dst = -dst + src
  } else {
    sgfr(dst, src2);
  }
#else
  SubP(dst, src1, src2);
#endif
}

// Subtract 32-bit (Register-Memory)
void TurboAssembler::Sub32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    s(dst, opnd);
  else
    sy(dst, opnd);
}

// Subtract Pointer Sized (Register - Memory)
void TurboAssembler::SubP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  sg(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}

void TurboAssembler::MovIntToFloat(DoubleRegister dst, Register src) {
  sllg(r0, src, Operand(32));
  ldgr(dst, r0);
}

void TurboAssembler::MovFloatToInt(Register dst, DoubleRegister src) {
  lgdr(dst, src);
  srlg(dst, dst, Operand(32));
}

void TurboAssembler::SubP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  sgf(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}

// Load And Subtract 32-bit (similar to laa/lan/lao/lax)
void TurboAssembler::LoadAndSub32(Register dst, Register src,
                                  const MemOperand& opnd) {
  lcr(dst, src);
  laa(dst, dst, opnd);
}

void TurboAssembler::LoadAndSub64(Register dst, Register src,
                                  const MemOperand& opnd) {
  lcgr(dst, src);
  laag(dst, dst, opnd);
}

//----------------------------------------------------------------------------
//  Subtract Logical Instructions
//----------------------------------------------------------------------------

// Subtract Logical 32-bit (Register - Memory)
void TurboAssembler::SubLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    sl(dst, opnd);
  else
    sly(dst, opnd);
}

// Subtract Logical Pointer Sized (Register - Memory)
void TurboAssembler::SubLogicalP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  slgf(dst, opnd);
#else
  SubLogical(dst, opnd);
#endif
}

// Subtract Logical Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) - Mem opnd (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::SubLogicalP_ExtendSrc(Register dst,
                                           const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  slgf(dst, opnd);
#else
  SubLogical(dst, opnd);
#endif
}

//----------------------------------------------------------------------------
//  Bitwise Operations
//----------------------------------------------------------------------------

// AND 32-bit - dst = dst & src
void TurboAssembler::And(Register dst, Register src) { nr(dst, src); }

// AND Pointer Size - dst = dst & src
void TurboAssembler::AndP(Register dst, Register src) { AndRR(dst, src); }

// Non-clobbering AND 32-bit - dst = src1 & src1
void TurboAssembler::And(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      nrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  And(dst, src2);
}

// Non-clobbering AND pointer size - dst = src1 & src1
void TurboAssembler::AndP(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AndP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  AndP(dst, src2);
}

// AND 32-bit (Reg - Mem)
void TurboAssembler::And(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    n(dst, opnd);
  else
    ny(dst, opnd);
}

// AND Pointer Size (Reg - Mem)
void TurboAssembler::AndP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  ng(dst, opnd);
#else
  And(dst, opnd);
#endif
}

// AND 32-bit - dst = dst & imm
void TurboAssembler::And(Register dst, const Operand& opnd) { nilf(dst, opnd); }

// AND Pointer Size - dst = dst & imm
void TurboAssembler::AndP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.immediate();
  if (value >> 32 != -1) {
    // this may not work b/c condition code won't be set correctly
    nihf(dst, Operand(value >> 32));
  }
  nilf(dst, Operand(value & 0xFFFFFFFF));
#else
  And(dst, opnd);
#endif
}

// AND 32-bit - dst = src & imm
void TurboAssembler::And(Register dst, Register src, const Operand& opnd) {
  if (dst != src) lr(dst, src);
  nilf(dst, opnd);
}

// AND Pointer Size - dst = src & imm
void TurboAssembler::AndP(Register dst, Register src, const Operand& opnd) {
  // Try to exploit RISBG first
  intptr_t value = opnd.immediate();
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    intptr_t shifted_value = value;
    int trailing_zeros = 0;

    // We start checking how many trailing zeros are left at the end.
    while ((0 != shifted_value) && (0 == (shifted_value & 1))) {
      trailing_zeros++;
      shifted_value >>= 1;
    }

    // If temp (value with right-most set of zeros shifted out) is 1 less
    // than power of 2, we have consecutive bits of 1.
    // Special case: If shift_value is zero, we cannot use RISBG, as it requires
    //               selection of at least 1 bit.
    if ((0 != shifted_value) && base::bits::IsPowerOfTwo(shifted_value + 1)) {
      int startBit =
          base::bits::CountLeadingZeros64(shifted_value) - trailing_zeros;
      int endBit = 63 - trailing_zeros;
      // Start: startBit, End: endBit, Shift = 0, true = zero unselected bits.
      RotateInsertSelectBits(dst, src, Operand(startBit), Operand(endBit),
            Operand::Zero(), true);
      return;
    } else if (-1 == shifted_value) {
      // A Special case in which all top bits up to MSB are 1's.  In this case,
      // we can set startBit to be 0.
      int endBit = 63 - trailing_zeros;
      RotateInsertSelectBits(dst, src, Operand::Zero(), Operand(endBit),
            Operand::Zero(), true);
      return;
    }
  }

  // If we are &'ing zero, we can just whack the dst register and skip copy
  if (dst != src && (0 != value)) LoadRR(dst, src);
  AndP(dst, opnd);
}

// OR 32-bit - dst = dst & src
void TurboAssembler::Or(Register dst, Register src) { or_z(dst, src); }

// OR Pointer Size - dst = dst & src
void TurboAssembler::OrP(Register dst, Register src) { OrRR(dst, src); }

// Non-clobbering OR 32-bit - dst = src1 & src1
void TurboAssembler::Or(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ork(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  Or(dst, src2);
}

// Non-clobbering OR pointer size - dst = src1 & src1
void TurboAssembler::OrP(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      OrP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  OrP(dst, src2);
}

// OR 32-bit (Reg - Mem)
void TurboAssembler::Or(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    o(dst, opnd);
  else
    oy(dst, opnd);
}

// OR Pointer Size (Reg - Mem)
void TurboAssembler::OrP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  og(dst, opnd);
#else
  Or(dst, opnd);
#endif
}

// OR 32-bit - dst = dst & imm
void TurboAssembler::Or(Register dst, const Operand& opnd) { oilf(dst, opnd); }

// OR Pointer Size - dst = dst & imm
void TurboAssembler::OrP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.immediate();
  if (value >> 32 != 0) {
    // this may not work b/c condition code won't be set correctly
    oihf(dst, Operand(value >> 32));
  }
  oilf(dst, Operand(value & 0xFFFFFFFF));
#else
  Or(dst, opnd);
#endif
}

// OR 32-bit - dst = src & imm
void TurboAssembler::Or(Register dst, Register src, const Operand& opnd) {
  if (dst != src) lr(dst, src);
  oilf(dst, opnd);
}

// OR Pointer Size - dst = src & imm
void TurboAssembler::OrP(Register dst, Register src, const Operand& opnd) {
  if (dst != src) LoadRR(dst, src);
  OrP(dst, opnd);
}

// XOR 32-bit - dst = dst & src
void TurboAssembler::Xor(Register dst, Register src) { xr(dst, src); }

// XOR Pointer Size - dst = dst & src
void TurboAssembler::XorP(Register dst, Register src) { XorRR(dst, src); }

// Non-clobbering XOR 32-bit - dst = src1 & src1
void TurboAssembler::Xor(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      xrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  Xor(dst, src2);
}

// Non-clobbering XOR pointer size - dst = src1 & src1
void TurboAssembler::XorP(Register dst, Register src1, Register src2) {
  if (dst != src1 && dst != src2) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      XorP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst == src2) {
    src2 = src1;
  }
  XorP(dst, src2);
}

// XOR 32-bit (Reg - Mem)
void TurboAssembler::Xor(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    x(dst, opnd);
  else
    xy(dst, opnd);
}

// XOR Pointer Size (Reg - Mem)
void TurboAssembler::XorP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  xg(dst, opnd);
#else
  Xor(dst, opnd);
#endif
}

// XOR 32-bit - dst = dst & imm
void TurboAssembler::Xor(Register dst, const Operand& opnd) { xilf(dst, opnd); }

// XOR Pointer Size - dst = dst & imm
void TurboAssembler::XorP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.immediate();
  xihf(dst, Operand(value >> 32));
  xilf(dst, Operand(value & 0xFFFFFFFF));
#else
  Xor(dst, opnd);
#endif
}

// XOR 32-bit - dst = src & imm
void TurboAssembler::Xor(Register dst, Register src, const Operand& opnd) {
  if (dst != src) lr(dst, src);
  xilf(dst, opnd);
}

// XOR Pointer Size - dst = src & imm
void TurboAssembler::XorP(Register dst, Register src, const Operand& opnd) {
  if (dst != src) LoadRR(dst, src);
  XorP(dst, opnd);
}

void TurboAssembler::Not32(Register dst, Register src) {
  if (src != no_reg && src != dst) lr(dst, src);
  xilf(dst, Operand(0xFFFFFFFF));
}

void TurboAssembler::Not64(Register dst, Register src) {
  if (src != no_reg && src != dst) lgr(dst, src);
  xihf(dst, Operand(0xFFFFFFFF));
  xilf(dst, Operand(0xFFFFFFFF));
}

void TurboAssembler::NotP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  Not64(dst, src);
#else
  Not32(dst, src);
#endif
}

// works the same as mov
void TurboAssembler::Load(Register dst, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value)) {
#if V8_TARGET_ARCH_S390X
    lghi(dst, opnd);
#else
    lhi(dst, opnd);
#endif
  } else if (is_int32(value)) {
#if V8_TARGET_ARCH_S390X
    lgfi(dst, opnd);
#else
    iilf(dst, opnd);
#endif
  } else if (is_uint32(value)) {
#if V8_TARGET_ARCH_S390X
    llilf(dst, opnd);
#else
    iilf(dst, opnd);
#endif
  } else {
    int32_t hi_32 = static_cast<int64_t>(value) >> 32;
    int32_t lo_32 = static_cast<int32_t>(value);

    iihf(dst, Operand(hi_32));
    iilf(dst, Operand(lo_32));
  }
}

void TurboAssembler::Load(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  lgf(dst, opnd);  // 64<-32
#else
  if (is_uint12(opnd.offset())) {
    l(dst, opnd);
  } else {
    ly(dst, opnd);
  }
#endif
}

void TurboAssembler::LoadPositiveP(Register result, Register input) {
#if V8_TARGET_ARCH_S390X
  lpgr(result, input);
#else
  lpr(result, input);
#endif
}

void TurboAssembler::LoadPositive32(Register result, Register input) {
  lpr(result, input);
  lgfr(result, result);
}

//-----------------------------------------------------------------------------
//  Compare Helpers
//-----------------------------------------------------------------------------

// Compare 32-bit Register vs Register
void TurboAssembler::Cmp32(Register src1, Register src2) { cr_z(src1, src2); }

// Compare Pointer Sized Register vs Register
void TurboAssembler::CmpP(Register src1, Register src2) {
#if V8_TARGET_ARCH_S390X
  cgr(src1, src2);
#else
  Cmp32(src1, src2);
#endif
}

// Compare 32-bit Register vs Immediate
// This helper will set up proper relocation entries if required.
void TurboAssembler::Cmp32(Register dst, const Operand& opnd) {
  if (opnd.rmode() == RelocInfo::NONE) {
    intptr_t value = opnd.immediate();
    if (is_int16(value))
      chi(dst, opnd);
    else
      cfi(dst, opnd);
  } else {
    // Need to generate relocation record here
    RecordRelocInfo(opnd.rmode(), opnd.immediate());
    cfi(dst, opnd);
  }
}

// Compare Pointer Sized  Register vs Immediate
// This helper will set up proper relocation entries if required.
void TurboAssembler::CmpP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (opnd.rmode() == RelocInfo::NONE) {
    cgfi(dst, opnd);
  } else {
    mov(r0, opnd);  // Need to generate 64-bit relocation
    cgr(dst, r0);
  }
#else
  Cmp32(dst, opnd);
#endif
}

// Compare 32-bit Register vs Memory
void TurboAssembler::Cmp32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    c(dst, opnd);
  else
    cy(dst, opnd);
}

// Compare Pointer Size Register vs Memory
void TurboAssembler::CmpP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  cg(dst, opnd);
#else
  Cmp32(dst, opnd);
#endif
}

// Using cs or scy based on the offset
void TurboAssembler::CmpAndSwap(Register old_val, Register new_val,
                                const MemOperand& opnd) {
  if (is_uint12(opnd.offset())) {
    cs(old_val, new_val, opnd);
  } else {
    csy(old_val, new_val, opnd);
  }
}

void TurboAssembler::CmpAndSwap64(Register old_val, Register new_val,
                                  const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  csg(old_val, new_val, opnd);
}

//-----------------------------------------------------------------------------
// Compare Logical Helpers
//-----------------------------------------------------------------------------

// Compare Logical 32-bit Register vs Register
void TurboAssembler::CmpLogical32(Register dst, Register src) { clr(dst, src); }

// Compare Logical Pointer Sized Register vs Register
void TurboAssembler::CmpLogicalP(Register dst, Register src) {
#ifdef V8_TARGET_ARCH_S390X
  clgr(dst, src);
#else
  CmpLogical32(dst, src);
#endif
}

// Compare Logical 32-bit Register vs Immediate
void TurboAssembler::CmpLogical32(Register dst, const Operand& opnd) {
  clfi(dst, opnd);
}

// Compare Logical Pointer Sized Register vs Immediate
void TurboAssembler::CmpLogicalP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK_EQ(static_cast<uint32_t>(opnd.immediate() >> 32), 0);
  clgfi(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}

// Compare Logical 32-bit Register vs Memory
void TurboAssembler::CmpLogical32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    cl(dst, opnd);
  else
    cly(dst, opnd);
}

// Compare Logical Pointer Sized Register vs Memory
void TurboAssembler::CmpLogicalP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  clg(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}

// Compare Logical Byte (Mem - Imm)
void TurboAssembler::CmpLogicalByte(const MemOperand& mem, const Operand& imm) {
  DCHECK(is_uint8(imm.immediate()));
  if (is_uint12(mem.offset()))
    cli(mem, imm);
  else
    cliy(mem, imm);
}

void TurboAssembler::Branch(Condition c, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value))
    brc(c, opnd);
  else
    brcl(c, opnd);
}

// Branch On Count.  Decrement R1, and branch if R1 != 0.
void TurboAssembler::BranchOnCount(Register r1, Label* l) {
  int32_t offset = branch_offset(l);
  if (is_int16(offset)) {
#if V8_TARGET_ARCH_S390X
    brctg(r1, Operand(offset));
#else
    brct(r1, Operand(offset));
#endif
  } else {
    AddP(r1, Operand(-1));
    Branch(ne, Operand(offset));
  }
}

void TurboAssembler::LoadIntLiteral(Register dst, int value) {
  Load(dst, Operand(value));
}

void TurboAssembler::LoadSmiLiteral(Register dst, Smi smi) {
  intptr_t value = static_cast<intptr_t>(smi.ptr());
#if V8_TARGET_ARCH_S390X
  DCHECK_EQ(value & 0xFFFFFFFF, 0);
  // The smi value is loaded in upper 32-bits.  Lower 32-bit are zeros.
  llihf(dst, Operand(value >> 32));
#else
  llilf(dst, Operand(value));
#endif
}

void TurboAssembler::LoadDoubleLiteral(DoubleRegister result, uint64_t value,
                                       Register scratch) {
  uint32_t hi_32 = value >> 32;
  uint32_t lo_32 = static_cast<uint32_t>(value);

  // Load the 64-bit value into a GPR, then transfer it to FPR via LDGR
  if (value == 0) {
    lzdr(result);
  } else if (lo_32 == 0) {
    llihf(scratch, Operand(hi_32));
    ldgr(result, scratch);
  } else {
    iihf(scratch, Operand(hi_32));
    iilf(scratch, Operand(lo_32));
    ldgr(result, scratch);
  }
}

void TurboAssembler::LoadDoubleLiteral(DoubleRegister result, double value,
                                       Register scratch) {
  uint64_t int_val = bit_cast<uint64_t, double>(value);
  LoadDoubleLiteral(result, int_val, scratch);
}

void TurboAssembler::LoadFloat32Literal(DoubleRegister result, float value,
                                        Register scratch) {
  uint64_t int_val = static_cast<uint64_t>(bit_cast<uint32_t, float>(value))
                     << 32;
  LoadDoubleLiteral(result, int_val, scratch);
}

void TurboAssembler::CmpSmiLiteral(Register src1, Smi smi, Register scratch) {
#if V8_TARGET_ARCH_S390X
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    cih(src1, Operand(static_cast<intptr_t>(smi.ptr()) >> 32));
  } else {
    LoadSmiLiteral(scratch, smi);
    cgr(src1, scratch);
  }
#else
  // CFI takes 32-bit immediate.
  cfi(src1, Operand(smi));
#endif
}

// Load a "pointer" sized value from the memory location
void TurboAssembler::LoadP(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

#if V8_TARGET_ARCH_S390X
  MemOperand src = mem;
  if (!is_int20(offset)) {
    DCHECK(scratch != no_reg && scratch != r0 && mem.rx() == r0);
    DCHECK(scratch != mem.rb());
    LoadIntLiteral(scratch, offset);
    src = MemOperand(mem.rb(), scratch);
  }
  lg(dst, src);
#else
  if (is_uint12(offset)) {
    l(dst, mem);
  } else if (is_int20(offset)) {
    ly(dst, mem);
  } else {
    DCHECK(scratch != no_reg && scratch != r0 && mem.rx() == r0);
    DCHECK(scratch != mem.rb());
    LoadIntLiteral(scratch, offset);
    l(dst, MemOperand(mem.rb(), scratch));
  }
#endif
}

// Store a "pointer" sized value to the memory location
void TurboAssembler::StoreP(Register src, const MemOperand& mem,
                            Register scratch) {
  if (!is_int20(mem.offset())) {
    DCHECK(scratch != no_reg);
    DCHECK(scratch != r0);
    LoadIntLiteral(scratch, mem.offset());
#if V8_TARGET_ARCH_S390X
    stg(src, MemOperand(mem.rb(), scratch));
#else
    st(src, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    stg(src, mem);
#else
    // StoreW will try to generate ST if offset fits, otherwise
    // it'll generate STY.
    StoreW(src, mem);
#endif
  }
}

// Store a "pointer" sized constant to the memory location
void TurboAssembler::StoreP(const MemOperand& mem, const Operand& opnd,
                            Register scratch) {
  // Relocations not supported
  DCHECK_EQ(opnd.rmode(), RelocInfo::NONE);

  // Try to use MVGHI/MVHI
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) && is_uint12(mem.offset()) &&
      mem.getIndexRegister() == r0 && is_int16(opnd.immediate())) {
#if V8_TARGET_ARCH_S390X
    mvghi(mem, opnd);
#else
    mvhi(mem, opnd);
#endif
  } else {
    LoadImmP(scratch, opnd);
    StoreP(scratch, mem);
  }
}

void TurboAssembler::LoadMultipleP(Register dst1, Register dst2,
                                   const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(mem.offset()));
  lmg(dst1, dst2, mem);
#else
  if (is_uint12(mem.offset())) {
    lm(dst1, dst2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    lmy(dst1, dst2, mem);
  }
#endif
}

void TurboAssembler::StoreMultipleP(Register src1, Register src2,
                                    const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(mem.offset()));
  stmg(src1, src2, mem);
#else
  if (is_uint12(mem.offset())) {
    stm(src1, src2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    stmy(src1, src2, mem);
  }
#endif
}

void TurboAssembler::LoadMultipleW(Register dst1, Register dst2,
                                   const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    lm(dst1, dst2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    lmy(dst1, dst2, mem);
  }
}

void TurboAssembler::StoreMultipleW(Register src1, Register src2,
                                    const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    stm(src1, src2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    stmy(src1, src2, mem);
  }
}

// Load 32-bits and sign extend if necessary.
void TurboAssembler::LoadW(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  lgfr(dst, src);
#else
  if (dst != src) lr(dst, src);
#endif
}

// Load 32-bits and sign extend if necessary.
void TurboAssembler::LoadW(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(scratch != no_reg);
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lgf(dst, MemOperand(mem.rb(), scratch));
#else
    l(dst, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lgf(dst, mem);
#else
    if (is_uint12(offset)) {
      l(dst, mem);
    } else {
      ly(dst, mem);
    }
#endif
  }
}

// Load 32-bits and zero extend if necessary.
void TurboAssembler::LoadlW(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  llgfr(dst, src);
#else
  if (dst != src) lr(dst, src);
#endif
}

// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void TurboAssembler::LoadlW(Register dst, const MemOperand& mem,
                            Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

#if V8_TARGET_ARCH_S390X
  if (is_int20(offset)) {
    llgf(dst, mem);
  } else if (scratch != no_reg) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
    llgf(dst, MemOperand(base, scratch));
  } else {
    DCHECK(false);
  }
#else
  bool use_RXform = false;
  bool use_RXYform = false;
  if (is_uint12(offset)) {
    // RX-format supports unsigned 12-bits offset.
    use_RXform = true;
  } else if (is_int20(offset)) {
    // RXY-format supports signed 20-bits offset.
    use_RXYform = true;
  } else if (scratch != no_reg) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
  } else {
    DCHECK(false);
  }

  if (use_RXform) {
    l(dst, mem);
  } else if (use_RXYform) {
    ly(dst, mem);
  } else {
    ly(dst, MemOperand(base, scratch));
  }
#endif
}

void TurboAssembler::LoadLogicalHalfWordP(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  llgh(dst, mem);
#else
  llh(dst, mem);
#endif
}

void TurboAssembler::LoadLogicalHalfWordP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  llghr(dst, src);
#else
  llhr(dst, src);
#endif
}

void TurboAssembler::LoadB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  lgb(dst, mem);
#else
  lb(dst, mem);
#endif
}

void TurboAssembler::LoadB(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  lgbr(dst, src);
#else
  lbr(dst, src);
#endif
}

void TurboAssembler::LoadlB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  llgc(dst, mem);
#else
  llc(dst, mem);
#endif
}

void TurboAssembler::LoadlB(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  llgcr(dst, src);
#else
  llcr(dst, src);
#endif
}

void TurboAssembler::LoadLogicalReversedWordP(Register dst,
                                              const MemOperand& mem) {
  lrv(dst, mem);
  LoadlW(dst, dst);
}

void TurboAssembler::LoadLogicalReversedHalfWordP(Register dst,
                                                  const MemOperand& mem) {
  lrvh(dst, mem);
  LoadLogicalHalfWordP(dst, dst);
}


// Load And Test (Reg <- Reg)
void TurboAssembler::LoadAndTest32(Register dst, Register src) {
  ltr(dst, src);
}

// Load And Test
//     (Register dst(ptr) = Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void TurboAssembler::LoadAndTestP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgfr(dst, src);
#else
  ltr(dst, src);
#endif
}

// Load And Test Pointer Sized (Reg <- Reg)
void TurboAssembler::LoadAndTestP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgr(dst, src);
#else
  ltr(dst, src);
#endif
}

// Load And Test 32-bit (Reg <- Mem)
void TurboAssembler::LoadAndTest32(Register dst, const MemOperand& mem) {
  lt_z(dst, mem);
}

// Load And Test Pointer Sized (Reg <- Mem)
void TurboAssembler::LoadAndTestP(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  ltg(dst, mem);
#else
  lt_z(dst, mem);
#endif
}

// Load On Condition Pointer Sized (Reg <- Reg)
void TurboAssembler::LoadOnConditionP(Condition cond, Register dst,
                                      Register src) {
#if V8_TARGET_ARCH_S390X
  locgr(cond, dst, src);
#else
  locr(cond, dst, src);
#endif
}

// Load Double Precision (64-bit) Floating Point number from memory
void TurboAssembler::LoadDouble(DoubleRegister dst, const MemOperand& mem) {
  // for 32bit and 64bit we all use 64bit floating point regs
  if (is_uint12(mem.offset())) {
    ld(dst, mem);
  } else {
    ldy(dst, mem);
  }
}

// Load Single Precision (32-bit) Floating Point number from memory
void TurboAssembler::LoadFloat32(DoubleRegister dst, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    le_z(dst, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    ley(dst, mem);
  }
}

// Load Single Precision (32-bit) Floating Point number from memory,
// and convert to Double Precision (64-bit)
void TurboAssembler::LoadFloat32ConvertToDouble(DoubleRegister dst,
                                                const MemOperand& mem) {
  LoadFloat32(dst, mem);
  ldebr(dst, dst);
}

// Store Double Precision (64-bit) Floating Point number to memory
void TurboAssembler::StoreDouble(DoubleRegister dst, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    std(dst, mem);
  } else {
    stdy(dst, mem);
  }
}

// Store Single Precision (32-bit) Floating Point number to memory
void TurboAssembler::StoreFloat32(DoubleRegister src, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    ste(src, mem);
  } else {
    stey(src, mem);
  }
}

// Convert Double precision (64-bit) to Single Precision (32-bit)
// and store resulting Float32 to memory
void TurboAssembler::StoreDoubleAsFloat32(DoubleRegister src,
                                          const MemOperand& mem,
                                          DoubleRegister scratch) {
  ledbr(scratch, src);
  StoreFloat32(scratch, mem);
}

void TurboAssembler::AddFloat32(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    aeb(dst, opnd);
  } else {
    ley(scratch, opnd);
    aebr(dst, scratch);
  }
}

void TurboAssembler::AddFloat64(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    adb(dst, opnd);
  } else {
    ldy(scratch, opnd);
    adbr(dst, scratch);
  }
}

void TurboAssembler::SubFloat32(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    seb(dst, opnd);
  } else {
    ley(scratch, opnd);
    sebr(dst, scratch);
  }
}

void TurboAssembler::SubFloat64(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    sdb(dst, opnd);
  } else {
    ldy(scratch, opnd);
    sdbr(dst, scratch);
  }
}

void TurboAssembler::MulFloat32(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    meeb(dst, opnd);
  } else {
    ley(scratch, opnd);
    meebr(dst, scratch);
  }
}

void TurboAssembler::MulFloat64(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    mdb(dst, opnd);
  } else {
    ldy(scratch, opnd);
    mdbr(dst, scratch);
  }
}

void TurboAssembler::DivFloat32(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    deb(dst, opnd);
  } else {
    ley(scratch, opnd);
    debr(dst, scratch);
  }
}

void TurboAssembler::DivFloat64(DoubleRegister dst, const MemOperand& opnd,
                                DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    ddb(dst, opnd);
  } else {
    ldy(scratch, opnd);
    ddbr(dst, scratch);
  }
}

void TurboAssembler::LoadFloat32ToDouble(DoubleRegister dst,
                                         const MemOperand& opnd,
                                         DoubleRegister scratch) {
  if (is_uint12(opnd.offset())) {
    ldeb(dst, opnd);
  } else {
    ley(scratch, opnd);
    ldebr(dst, scratch);
  }
}

// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void TurboAssembler::StoreW(Register src, const MemOperand& mem,
                            Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  bool use_RXform = false;
  bool use_RXYform = false;

  if (is_uint12(offset)) {
    // RX-format supports unsigned 12-bits offset.
    use_RXform = true;
  } else if (is_int20(offset)) {
    // RXY-format supports signed 20-bits offset.
    use_RXYform = true;
  } else if (scratch != no_reg) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
  } else {
    // scratch is no_reg
    DCHECK(false);
  }

  if (use_RXform) {
    st(src, mem);
  } else if (use_RXYform) {
    sty(src, mem);
  } else {
    StoreW(src, MemOperand(base, scratch));
  }
}

void TurboAssembler::LoadHalfWordP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  lghr(dst, src);
#else
  lhr(dst, src);
#endif
}

// Loads 16-bits half-word value from memory and sign extends to pointer
// sized register
void TurboAssembler::LoadHalfWordP(Register dst, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(scratch != no_reg);
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lgh(dst, MemOperand(base, scratch));
#else
    lh(dst, MemOperand(base, scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lgh(dst, mem);
#else
    if (is_uint12(offset)) {
      lh(dst, mem);
    } else {
      lhy(dst, mem);
    }
#endif
  }
}

// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void TurboAssembler::StoreHalfWord(Register src, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    sth(src, mem);
  } else if (is_int20(offset)) {
    sthy(src, mem);
  } else {
    DCHECK(scratch != no_reg);
    LoadIntLiteral(scratch, offset);
    sth(src, MemOperand(base, scratch));
  }
}

// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void TurboAssembler::StoreByte(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    stc(src, mem);
  } else if (is_int20(offset)) {
    stcy(src, mem);
  } else {
    DCHECK(scratch != no_reg);
    LoadIntLiteral(scratch, offset);
    stc(src, MemOperand(base, scratch));
  }
}

// Shift left logical for 32-bit integer types.
void TurboAssembler::ShiftLeft(Register dst, Register src, const Operand& val) {
  if (dst == src) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    lr(dst, src);
    sll(dst, val);
  }
}

// Shift left logical for 32-bit integer types.
void TurboAssembler::ShiftLeft(Register dst, Register src, Register val) {
  if (dst == src) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    DCHECK(dst != val);  // The lr/sll path clobbers val.
    lr(dst, src);
    sll(dst, val);
  }
}

// Shift right logical for 32-bit integer types.
void TurboAssembler::ShiftRight(Register dst, Register src,
                                const Operand& val) {
  if (dst == src) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    lr(dst, src);
    srl(dst, val);
  }
}

// Shift right logical for 32-bit integer types.
void TurboAssembler::ShiftRight(Register dst, Register src, Register val) {
  if (dst == src) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    DCHECK(dst != val);  // The lr/srl path clobbers val.
    lr(dst, src);
    srl(dst, val);
  }
}

// Shift left arithmetic for 32-bit integer types.
void TurboAssembler::ShiftLeftArith(Register dst, Register src,
                                    const Operand& val) {
  if (dst == src) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    lr(dst, src);
    sla(dst, val);
  }
}

// Shift left arithmetic for 32-bit integer types.
void TurboAssembler::ShiftLeftArith(Register dst, Register src, Register val) {
  if (dst == src) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    DCHECK(dst != val);  // The lr/sla path clobbers val.
    lr(dst, src);
    sla(dst, val);
  }
}

// Shift right arithmetic for 32-bit integer types.
void TurboAssembler::ShiftRightArith(Register dst, Register src,
                                     const Operand& val) {
  if (dst == src) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    lr(dst, src);
    sra(dst, val);
  }
}

// Shift right arithmetic for 32-bit integer types.
void TurboAssembler::ShiftRightArith(Register dst, Register src, Register val) {
  if (dst == src) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    DCHECK(dst != val);  // The lr/sra path clobbers val.
    lr(dst, src);
    sra(dst, val);
  }
}

// Clear right most # of bits
void TurboAssembler::ClearRightImm(Register dst, Register src,
                                   const Operand& val) {
  int numBitsToClear = val.immediate() % (kPointerSize * 8);

  // Try to use RISBG if possible
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    int endBit = 63 - numBitsToClear;
    RotateInsertSelectBits(dst, src, Operand::Zero(), Operand(endBit),
          Operand::Zero(), true);
    return;
  }

  uint64_t hexMask = ~((1L << numBitsToClear) - 1);

  // S390 AND instr clobbers source.  Make a copy if necessary
  if (dst != src) LoadRR(dst, src);

  if (numBitsToClear <= 16) {
    nill(dst, Operand(static_cast<uint16_t>(hexMask)));
  } else if (numBitsToClear <= 32) {
    nilf(dst, Operand(static_cast<uint32_t>(hexMask)));
  } else if (numBitsToClear <= 64) {
    nilf(dst, Operand(static_cast<intptr_t>(0)));
    nihf(dst, Operand(hexMask >> 32));
  }
}

void TurboAssembler::Popcnt32(Register dst, Register src) {
  DCHECK(src != r0);
  DCHECK(dst != r0);

  popcnt(dst, src);
  ShiftRight(r0, dst, Operand(16));
  ar(dst, r0);
  ShiftRight(r0, dst, Operand(8));
  ar(dst, r0);
  llgcr(dst, dst);
}

#ifdef V8_TARGET_ARCH_S390X
void TurboAssembler::Popcnt64(Register dst, Register src) {
  DCHECK(src != r0);
  DCHECK(dst != r0);

  popcnt(dst, src);
  ShiftRightP(r0, dst, Operand(32));
  AddP(dst, r0);
  ShiftRightP(r0, dst, Operand(16));
  AddP(dst, r0);
  ShiftRightP(r0, dst, Operand(8));
  AddP(dst, r0);
  LoadlB(dst, dst);
}
#endif

void TurboAssembler::SwapP(Register src, Register dst, Register scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  LoadRR(scratch, src);
  LoadRR(src, dst);
  LoadRR(dst, scratch);
}

void TurboAssembler::SwapP(Register src, MemOperand dst, Register scratch) {
  if (dst.rx() != r0) DCHECK(!AreAliased(src, dst.rx(), scratch));
  if (dst.rb() != r0) DCHECK(!AreAliased(src, dst.rb(), scratch));
  DCHECK(!AreAliased(src, scratch));
  LoadRR(scratch, src);
  LoadP(src, dst);
  StoreP(scratch, dst);
}

void TurboAssembler::SwapP(MemOperand src, MemOperand dst, Register scratch_0,
                           Register scratch_1) {
  if (src.rx() != r0) DCHECK(!AreAliased(src.rx(), scratch_0, scratch_1));
  if (src.rb() != r0) DCHECK(!AreAliased(src.rb(), scratch_0, scratch_1));
  if (dst.rx() != r0) DCHECK(!AreAliased(dst.rx(), scratch_0, scratch_1));
  if (dst.rb() != r0) DCHECK(!AreAliased(dst.rb(), scratch_0, scratch_1));
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadP(scratch_0, src);
  LoadP(scratch_1, dst);
  StoreP(scratch_0, dst);
  StoreP(scratch_1, src);
}

void TurboAssembler::SwapFloat32(DoubleRegister src, DoubleRegister dst,
                                 DoubleRegister scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  ldr(scratch, src);
  ldr(src, dst);
  ldr(dst, scratch);
}

void TurboAssembler::SwapFloat32(DoubleRegister src, MemOperand dst,
                                 DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  ldr(scratch, src);
  LoadFloat32(src, dst);
  StoreFloat32(scratch, dst);
}

void TurboAssembler::SwapFloat32(MemOperand src, MemOperand dst,
                                 DoubleRegister scratch_0,
                                 DoubleRegister scratch_1) {
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadFloat32(scratch_0, src);
  LoadFloat32(scratch_1, dst);
  StoreFloat32(scratch_0, dst);
  StoreFloat32(scratch_1, src);
}

void TurboAssembler::SwapDouble(DoubleRegister src, DoubleRegister dst,
                                DoubleRegister scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  ldr(scratch, src);
  ldr(src, dst);
  ldr(dst, scratch);
}

void TurboAssembler::SwapDouble(DoubleRegister src, MemOperand dst,
                                DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  ldr(scratch, src);
  LoadDouble(src, dst);
  StoreDouble(scratch, dst);
}

void TurboAssembler::SwapDouble(MemOperand src, MemOperand dst,
                                DoubleRegister scratch_0,
                                DoubleRegister scratch_1) {
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadDouble(scratch_0, src);
  LoadDouble(scratch_1, dst);
  StoreDouble(scratch_0, dst);
  StoreDouble(scratch_1, src);
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  mov(kSpeculationPoisonRegister, Operand(-1));
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  larl(dst, Operand(-pc_offset() / 2));
}

void TurboAssembler::LoadPC(Register dst) {
  Label current_pc;
  larl(dst, &current_pc);
  bind(&current_pc);
}

void TurboAssembler::JumpIfEqual(Register x, int32_t y, Label* dest) {
  Cmp32(x, Operand(y));
  beq(dest);
}

void TurboAssembler::JumpIfLessThan(Register x, int32_t y, Label* dest) {
  Cmp32(x, Operand(y));
  blt(dest);
}

void TurboAssembler::CallBuiltinPointer(Register builtin_pointer) {
  STATIC_ASSERT(kSystemPointerSize == 8);
  STATIC_ASSERT(kSmiShiftSize == 31);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_pointer register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below.
  ShiftRightArithP(builtin_pointer, builtin_pointer,
                   Operand(kSmiShift - kSystemPointerSizeLog2));
  AddP(builtin_pointer, builtin_pointer,
      Operand(IsolateData::builtin_entry_table_offset()));
  LoadP(builtin_pointer, MemOperand(kRootRegister, builtin_pointer));
  Call(builtin_pointer);
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

  if (options().isolate_independent_code) {
    DCHECK(root_array_available());
    Label if_code_is_off_heap, out;

    Register scratch = r1;

    DCHECK(!AreAliased(destination, scratch));
    DCHECK(!AreAliased(code_object, scratch));

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.
    LoadW(scratch, FieldMemOperand(code_object, Code::kFlagsOffset));
    tmlh(scratch, Operand(Code::IsOffHeapTrampoline::kMask >> 16));
    bne(&if_code_is_off_heap);

    // Not an off-heap trampoline, the entry point is at
    // Code::raw_instruction_start().
    AddP(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
    b(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    LoadW(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset));
    ShiftLeftP(destination, scratch, Operand(kSystemPointerSizeLog2));
    AddP(destination, destination, kRootRegister);
    LoadP(destination,
        MemOperand(destination, IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    AddP(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Jump(code_object);
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  Label return_label;
  larl(r14, &return_label);  // Generate the return addr of call later.
  StoreP(r14, MemOperand(sp, kStackFrameRASlot * kPointerSize));

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  b(target);
  bind(&return_label);
}

void TurboAssembler::CallForDeoptimization(Address target, int deopt_id) {
  NoRootArrayScope no_root_array(this);

  // Save the deopt id in r10 (we don't need the roots array from now on).
  DCHECK_LE(deopt_id, 0xFFFF);
  lghi(r10, Operand(deopt_id));
  Call(target, RelocInfo::RUNTIME_ENTRY);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
