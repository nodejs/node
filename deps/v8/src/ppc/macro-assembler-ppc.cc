// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_PPC

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frames-inl.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"

#include "src/ppc/macro-assembler-ppc.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* isolate, void* buffer, int size,
                               CodeObjectRequired create_code_object)
    : TurboAssembler(isolate, buffer, size, create_code_object) {}

TurboAssembler::TurboAssembler(Isolate* isolate, void* buffer, int buffer_size,
                               CodeObjectRequired create_code_object)
    : Assembler(isolate, buffer, buffer_size), isolate_(isolate) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<HeapObject>::New(isolate->heap()->undefined_value(), isolate);
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
    bytes += kNumCallerSavedDoubles * kDoubleSize;
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
    bytes += kNumCallerSavedDoubles * kDoubleSize;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    MultiPopDoubles(kCallerSavedDoubles);
    bytes += kNumCallerSavedDoubles * kDoubleSize;
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

void TurboAssembler::Jump(Register target) {
  mtctr(target);
  bctr();
}

void MacroAssembler::JumpToJSEntry(Register target) {
  Move(ip, target);
  Jump(ip);
}

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip, cr);

  DCHECK(rmode == RelocInfo::CODE_TARGET || rmode == RelocInfo::RUNTIME_ENTRY);

  mov(ip, Operand(target, rmode));
  mtctr(ip);
  bctr();

  bind(&skip);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          CRegister cr) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, cr);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  // 'code' is always generated ppc code, never THUMB code
  Jump(reinterpret_cast<intptr_t>(code.address()), rmode, cond, cr);
}

int TurboAssembler::CallSize(Register target) { return 2 * kInstrSize; }

void TurboAssembler::Call(Register target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);

  // branch via link register and set LK bit for return point
  mtctr(target);
  bctrl();

  DCHECK_EQ(CallSize(target), SizeOfCodeGeneratedSince(&start));
}

void MacroAssembler::CallJSEntry(Register target) {
  DCHECK(target == ip);
  Call(target);
}

int TurboAssembler::CallSize(Address target, RelocInfo::Mode rmode,
                             Condition cond) {
  Operand mov_operand = Operand(reinterpret_cast<intptr_t>(target), rmode);
  return (2 + instructions_required_for_mov(ip, mov_operand)) * kInstrSize;
}

int MacroAssembler::CallSizeNotPredictableCodeSize(Address target,
                                                   RelocInfo::Mode rmode,
                                                   Condition cond) {
  return (2 + kMovInstructionsNoConstantPool) * kInstrSize;
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(cond == al);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(target, rmode, cond);
  Label start;
  bind(&start);
#endif
  // This can likely be optimized to make use of bc() with 24bit relative
  //
  // RecordRelocInfo(x.rmode_, x.immediate);
  // bc( BA, .... offset, LKset);
  //

  mov(ip, Operand(reinterpret_cast<intptr_t>(target), rmode));
  mtctr(ip);
  bctrl();

  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}

int TurboAssembler::CallSize(Handle<Code> code, RelocInfo::Mode rmode,
                             Condition cond) {
  return CallSize(code.address(), rmode, cond);
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));

  Label start;
  bind(&start);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(code, rmode, cond);
#endif

  Call(code.address(), rmode, cond);
  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}

void TurboAssembler::Drop(int count) {
  if (count > 0) {
    Add(sp, sp, count * kPointerSize, r0);
  }
}

void TurboAssembler::Drop(Register count, Register scratch) {
  ShiftLeftImm(scratch, count, Operand(kPointerSizeLog2));
  add(sp, sp, scratch);
}

void TurboAssembler::Call(Label* target) { b(target, SetLK); }

void TurboAssembler::Push(Handle<HeapObject> handle) {
  mov(r0, Operand(handle));
  push(r0);
}

void TurboAssembler::Push(Smi* smi) {
  mov(r0, Operand(smi));
  push(r0);
}

void TurboAssembler::Move(Register dst, Handle<HeapObject> value) {
  mov(dst, Operand(value));
}

void TurboAssembler::Move(Register dst, Register src, Condition cond) {
  DCHECK(cond == al);
  if (dst != src) {
    mr(dst, src);
  }
}

void TurboAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (dst != src) {
    fmr(dst, src);
  }
}

void TurboAssembler::MultiPush(RegList regs, Register location) {
  int16_t num_to_push = base::bits::CountPopulation(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  subi(location, location, Operand(stack_offset));
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
  addi(location, location, Operand(stack_offset));
}

void TurboAssembler::MultiPushDoubles(RegList dregs, Register location) {
  int16_t num_to_push = base::bits::CountPopulation(dregs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  subi(location, location, Operand(stack_offset));
  for (int16_t i = DoubleRegister::kNumRegisters - 1; i >= 0; i--) {
    if ((dregs & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      stack_offset -= kDoubleSize;
      stfd(dreg, MemOperand(location, stack_offset));
    }
  }
}

void TurboAssembler::MultiPopDoubles(RegList dregs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < DoubleRegister::kNumRegisters; i++) {
    if ((dregs & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      lfd(dreg, MemOperand(location, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addi(location, location, Operand(stack_offset));
}

void TurboAssembler::LoadRoot(Register destination, Heap::RootListIndex index,
                              Condition cond) {
  DCHECK(cond == al);
  LoadP(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), r0);
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

  Add(dst, object, offset - kHeapObjectTag, r0);
  if (emit_debug_code()) {
    Label ok;
    andi(r0, dst, Operand(kPointerSize - 1));
    beq(&ok, cr0);
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

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kRecordWrite);
  RegList registers = callable.descriptor().allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kObject));
  Register slot_parameter(
      callable.descriptor().GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register isolate_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kIsolate));
  Register remembered_set_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kFPMode));

  push(object);
  push(address);

  pop(slot_parameter);
  pop(object_parameter);

  mov(isolate_parameter,
      Operand(ExternalReference::isolate_address(isolate())));
  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(callable.code(), RelocInfo::CODE_TARGET);

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
    LoadP(r0, MemOperand(address));
    cmp(r0, value);
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
    mflr(r0);
    push(r0);
  }
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r0);
    mtlr(r0);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  int fp_delta = 0;
  mflr(r0);
  if (FLAG_enable_embedded_constant_pool) {
    if (marker_reg.is_valid()) {
      Push(r0, fp, kConstantPoolRegister, marker_reg);
      fp_delta = 2;
    } else {
      Push(r0, fp, kConstantPoolRegister);
      fp_delta = 1;
    }
  } else {
    if (marker_reg.is_valid()) {
      Push(r0, fp, marker_reg);
      fp_delta = 1;
    } else {
      Push(r0, fp);
      fp_delta = 0;
    }
  }
  addi(fp, sp, Operand(fp_delta * kPointerSize));
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  int fp_delta = 0;
  mflr(r0);
  if (FLAG_enable_embedded_constant_pool) {
    if (function_reg.is_valid()) {
      Push(r0, fp, kConstantPoolRegister, cp, function_reg);
      fp_delta = 3;
    } else {
      Push(r0, fp, kConstantPoolRegister, cp);
      fp_delta = 2;
    }
  } else {
    if (function_reg.is_valid()) {
      Push(r0, fp, cp, function_reg);
      fp_delta = 2;
    } else {
      Push(r0, fp, cp);
      fp_delta = 1;
    }
  }
  addi(fp, sp, Operand(fp_delta * kPointerSize));
}

void TurboAssembler::RestoreFrameStateForTailCall() {
  if (FLAG_enable_embedded_constant_pool) {
    LoadP(kConstantPoolRegister,
          MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
    set_constant_pool_available(false);
  }
  LoadP(r0, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  mtlr(r0);
}

// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK_GE(num_unsaved, 0);
  if (num_unsaved > 0) {
    subi(sp, sp, Operand(num_unsaved * kPointerSize));
  }
  MultiPush(kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  MultiPop(kSafepointSavedRegisters);
  if (num_unsaved > 0) {
    addi(sp, sp, Operand(num_unsaved * kPointerSize));
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
  // Turn potential sNaN into qNaN.
  fsub(dst, src, kDoubleRegZero);
}

void TurboAssembler::ConvertIntToDouble(Register src, DoubleRegister dst) {
  MovIntToDouble(dst, src, r0);
  fcfid(dst, dst);
}

void TurboAssembler::ConvertUnsignedIntToDouble(Register src,
                                                DoubleRegister dst) {
  MovUnsignedIntToDouble(dst, src, r0);
  fcfid(dst, dst);
}

void TurboAssembler::ConvertIntToFloat(Register src, DoubleRegister dst) {
  MovIntToDouble(dst, src, r0);
  fcfids(dst, dst);
}

void TurboAssembler::ConvertUnsignedIntToFloat(Register src,
                                               DoubleRegister dst) {
  MovUnsignedIntToDouble(dst, src, r0);
  fcfids(dst, dst);
}

#if V8_TARGET_ARCH_PPC64
void TurboAssembler::ConvertInt64ToDouble(Register src,
                                          DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfid(double_dst, double_dst);
}

void TurboAssembler::ConvertUnsignedInt64ToFloat(Register src,
                                                 DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfidus(double_dst, double_dst);
}

void TurboAssembler::ConvertUnsignedInt64ToDouble(Register src,
                                                  DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfidu(double_dst, double_dst);
}

void TurboAssembler::ConvertInt64ToFloat(Register src,
                                         DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfids(double_dst, double_dst);
}
#endif

void TurboAssembler::ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_PPC64
                                          const Register dst_hi,
#endif
                                          const Register dst,
                                          const DoubleRegister double_dst,
                                          FPRoundingMode rounding_mode) {
  if (rounding_mode == kRoundToZero) {
    fctidz(double_dst, double_input);
  } else {
    SetRoundingMode(rounding_mode);
    fctid(double_dst, double_input);
    ResetRoundingMode();
  }

  MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
      dst_hi,
#endif
      dst, double_dst);
}

#if V8_TARGET_ARCH_PPC64
void TurboAssembler::ConvertDoubleToUnsignedInt64(
    const DoubleRegister double_input, const Register dst,
    const DoubleRegister double_dst, FPRoundingMode rounding_mode) {
  if (rounding_mode == kRoundToZero) {
    fctiduz(double_dst, double_input);
  } else {
    SetRoundingMode(rounding_mode);
    fctidu(double_dst, double_input);
    ResetRoundingMode();
  }

  MovDoubleToInt64(dst, double_dst);
}
#endif

#if !V8_TARGET_ARCH_PPC64
void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  DCHECK(!AreAliased(dst_low, dst_high, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  slw(dst_high, src_low, scratch);
  li(dst_low, Operand::Zero());
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  slw(dst_high, src_high, shift);
  srw(scratch, src_low, scratch);
  orx(dst_high, dst_high, scratch);
  slw(dst_low, src_low, shift);
  bind(&done);
}

void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_high, src_low);
    li(dst_low, Operand::Zero());
  } else if (shift > 32) {
    shift &= 0x1F;
    slwi(dst_high, src_low, Operand(shift));
    li(dst_low, Operand::Zero());
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    slwi(dst_high, src_high, Operand(shift));
    rlwimi(dst_high, src_low, shift, 32 - shift, 31);
    slwi(dst_low, src_low, Operand(shift));
  }
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  DCHECK(!AreAliased(dst_low, dst_high, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  srw(dst_low, src_high, scratch);
  li(dst_high, Operand::Zero());
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  srw(dst_low, src_low, shift);
  slw(scratch, src_high, scratch);
  orx(dst_low, dst_low, scratch);
  srw(dst_high, src_high, shift);
  bind(&done);
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_low, src_high);
    li(dst_high, Operand::Zero());
  } else if (shift > 32) {
    shift &= 0x1F;
    srwi(dst_low, src_high, Operand(shift));
    li(dst_high, Operand::Zero());
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    srwi(dst_low, src_low, Operand(shift));
    rlwimi(dst_low, src_high, 32 - shift, 0, shift - 1);
    srwi(dst_high, src_high, Operand(shift));
  }
}

void TurboAssembler::ShiftRightAlgPair(Register dst_low, Register dst_high,
                                       Register src_low, Register src_high,
                                       Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high, shift));
  DCHECK(!AreAliased(dst_high, src_low, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  sraw(dst_low, src_high, scratch);
  srawi(dst_high, src_high, 31);
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  srw(dst_low, src_low, shift);
  slw(scratch, src_high, scratch);
  orx(dst_low, dst_low, scratch);
  sraw(dst_high, src_high, shift);
  bind(&done);
}

void TurboAssembler::ShiftRightAlgPair(Register dst_low, Register dst_high,
                                       Register src_low, Register src_high,
                                       uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_low, src_high);
    srawi(dst_high, src_high, 31);
  } else if (shift > 32) {
    shift &= 0x1F;
    srawi(dst_low, src_high, shift);
    srawi(dst_high, src_high, 31);
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    srwi(dst_low, src_low, Operand(shift));
    rlwimi(dst_low, src_high, 32 - shift, 0, shift - 1);
    srawi(dst_high, src_high, shift);
  }
}
#endif

void MacroAssembler::LoadConstantPoolPointerRegisterFromCodeTargetAddress(
    Register code_target_address) {
  lwz(kConstantPoolRegister,
      MemOperand(code_target_address,
                 Code::kConstantPoolOffset - Code::kHeaderSize));
  add(kConstantPoolRegister, kConstantPoolRegister, code_target_address);
}

void TurboAssembler::LoadConstantPoolPointerRegister(Register base,
                                                     int code_start_delta) {
  add_label_offset(kConstantPoolRegister, base, ConstantPoolPosition(),
                   code_start_delta);
}

void TurboAssembler::LoadConstantPoolPointerRegister() {
  mov_label_addr(kConstantPoolRegister, ConstantPoolPosition());
}

void TurboAssembler::StubPrologue(StackFrame::Type type, Register base,
                                  int prologue_offset) {
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(this);
    mov(r11, Operand(StackFrame::TypeToMarker(type)));
    PushCommonFrame(r11);
  }
  if (FLAG_enable_embedded_constant_pool) {
    if (base != no_reg) {
      // base contains prologue address
      LoadConstantPoolPointerRegister(base, -prologue_offset);
    } else {
      LoadConstantPoolPointerRegister();
    }
    set_constant_pool_available(true);
  }
}

void TurboAssembler::Prologue(Register base, int prologue_offset) {
  DCHECK(base != no_reg);
  PushStandardFrame(r4);
  if (FLAG_enable_embedded_constant_pool) {
    // base contains prologue address
    LoadConstantPoolPointerRegister(base, -prologue_offset);
    set_constant_pool_available(true);
  }
}

void TurboAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  if (FLAG_enable_embedded_constant_pool && load_constant_pool_pointer_reg) {
    // Push type explicitly so we can leverage the constant pool.
    // This path cannot rely on ip containing code entry.
    PushCommonFrame();
    LoadConstantPoolPointerRegister();
    mov(ip, Operand(StackFrame::TypeToMarker(type)));
    push(ip);
  } else {
    mov(ip, Operand(StackFrame::TypeToMarker(type)));
    PushCommonFrame(ip);
  }
  if (type == StackFrame::INTERNAL) {
    mov(r0, Operand(CodeObject()));
    push(r0);
  }
}

int TurboAssembler::LeaveFrame(StackFrame::Type type, int stack_adjustment) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // r3: preserved
  // r4: preserved
  // r5: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller's state.
  int frame_ends;
  LoadP(r0, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadP(ip, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  if (FLAG_enable_embedded_constant_pool) {
    LoadP(kConstantPoolRegister,
          MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
  }
  mtlr(r0);
  frame_ends = pc_offset();
  Add(sp, fp, StandardFrameConstants::kCallerSPOffset + stack_adjustment, r0);
  mr(fp, ip);
  return frame_ends;
}

// ExitFrame layout (probably wrongish.. needs updating)
//
//  SP -> previousSP
//        LK reserved
//        code
//        sp_on_exit (for debug?)
// oldSP->prev SP
//        LK
//        <parameters on stack>

// Prior to calling EnterExitFrame, we've got a bunch of parameters
// on the stack that we need to wrap a real frame around.. so first
// we reserve a slot for LK and push the previous SP which is captured
// in the fp register (r31)
// Then - we buy a new frame

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

  mov(ip, Operand(StackFrame::TypeToMarker(frame_type)));
  PushCommonFrame(ip);
  // Reserve room for saved entry sp and code object.
  subi(sp, fp, Operand(ExitFrameConstants::kFixedFrameSizeFromFp));

  if (emit_debug_code()) {
    li(r8, Operand::Zero());
    StoreP(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }
  if (FLAG_enable_embedded_constant_pool) {
    StoreP(kConstantPoolRegister,
           MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }
  mov(r8, Operand(CodeObject()));
  StoreP(r8, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  mov(r8, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                    isolate())));
  StoreP(fp, MemOperand(r8));
  mov(r8,
      Operand(ExternalReference(IsolateAddressId::kContextAddress, isolate())));
  StoreP(cp, MemOperand(r8));

  // Optionally save all volatile double registers.
  if (save_doubles) {
    MultiPushDoubles(kCallerSavedDoubles);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   kNumCallerSavedDoubles * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
  }

  addi(sp, sp, Operand(-stack_space * kPointerSize));

  // Allocate and align the frame preparing for calling the runtime
  // function.
  const int frame_alignment = ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }
  li(r0, Operand::Zero());
  StorePU(r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));

  // Set the exit frame sp value to point just before the return address
  // location.
  addi(r8, sp, Operand((kStackFrameExtraParamSlot + 1) * kPointerSize));
  StoreP(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

int TurboAssembler::ActivationFrameAlignment() {
#if !defined(USE_SIMULATOR)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one PPC
  // platform for another PPC platform with a different alignment.
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
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int kNumRegs = kNumCallerSavedDoubles;
    const int offset =
        (ExitFrameConstants::kFixedFrameSizeFromFp + kNumRegs * kDoubleSize);
    addi(r6, fp, Operand(-offset));
    MultiPopDoubles(kCallerSavedDoubles, r6);
  }

  // Clear top frame.
  li(r6, Operand::Zero());
  mov(ip, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                    isolate())));
  StoreP(r6, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  mov(ip,
      Operand(ExternalReference(IsolateAddressId::kContextAddress, isolate())));
  LoadP(cp, MemOperand(ip));

#ifdef DEBUG
  mov(r6, Operand(Context::kInvalidContext));
  mov(ip,
      Operand(ExternalReference(IsolateAddressId::kContextAddress, isolate())));
  StoreP(r6, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  LeaveFrame(StackFrame::EXIT);

  if (argument_count.is_valid()) {
    if (!argument_count_is_length) {
      ShiftLeftImm(argument_count, argument_count, Operand(kPointerSizeLog2));
    }
    add(sp, sp, argument_count);
  }
}

void TurboAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d1);
}

void TurboAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d1);
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
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  ShiftLeftImm(dst_reg, caller_args_count_reg, Operand(kPointerSizeLog2));
  add(dst_reg, fp, dst_reg);
  addi(dst_reg, dst_reg,
       Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count_reg;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  if (callee_args_count.is_reg()) {
    ShiftLeftImm(src_reg, callee_args_count.reg(), Operand(kPointerSizeLog2));
    add(src_reg, sp, src_reg);
    addi(src_reg, src_reg, Operand(kPointerSize));
  } else {
    Add(src_reg, sp, (callee_args_count.immediate() + 1) * kPointerSize, r0);
  }

  if (FLAG_debug_code) {
    cmpl(src_reg, dst_reg);
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
    addi(tmp_reg, callee_args_count.reg(), Operand(1));  // +1 for receiver
  } else {
    mov(tmp_reg, Operand(callee_args_count.immediate() + 1));
  }
  mtctr(tmp_reg);
  bind(&loop);
  LoadPU(tmp_reg, MemOperand(src_reg, -kPointerSize));
  StorePU(tmp_reg, MemOperand(dst_reg, -kPointerSize));
  bdnz(&loop);

  // Leave current frame.
  mr(sp, dst_reg);
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
  //  r3: actual arguments count
  //  r4: function (passed through to callee)
  //  r5: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.

  // ARM has some sanity checks as per below, considering add them for PPC
  //  DCHECK(actual.is_immediate() || actual.reg() == r3);
  //  DCHECK(expected.is_immediate() || expected.reg() == r5);

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    mov(r3, Operand(actual.immediate()));
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
        mov(r5, Operand(expected.immediate()));
      }
    }
  } else {
    if (actual.is_immediate()) {
      mov(r3, Operand(actual.immediate()));
      cmpi(expected.reg(), Operand(actual.immediate()));
      beq(&regular_invoke);
    } else {
      cmp(expected.reg(), actual.reg());
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
  ExternalReference debug_hook_avtive =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  mov(r7, Operand(debug_hook_avtive));
  LoadByte(r7, MemOperand(r7), r0);
  extsb(r7, r7);
  CmpSmiLiteral(r7, Smi::kZero, r0);
  beq(&skip_hook);
  {
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
    Push(fun, fun);
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
  DCHECK(function == r4);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == r6);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = ip;
    LoadP(code, FieldMemOperand(function, JSFunction::kCodeOffset));
    addi(code, code, Operand(Code::kHeaderSize - kHeapObjectTag));
    if (flag == CALL_FUNCTION) {
      CallJSEntry(code);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      JumpToJSEntry(code);
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

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK(fun == r4);

  Register expected_reg = r5;
  Register temp_reg = r7;

  LoadP(temp_reg, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
  LoadWordArith(expected_reg,
                FieldMemOperand(
                    temp_reg, SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(fun, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK(function == r4);

  // Get the function and setup the context.
  LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  InvokeFunctionCode(r4, no_reg, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  Move(r4, function);
  InvokeFunction(r4, expected, actual, flag);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  mov(r4, Operand(restart_fp));
  LoadP(r4, MemOperand(r4));
  cmpi(r4, Operand::Zero());
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne);
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  Push(Smi::kZero);  // Padding.

  // Link the current handler as the next handler.
  // Preserve r3-r7.
  mov(r8,
      Operand(ExternalReference(IsolateAddressId::kHandlerAddress, isolate())));
  LoadP(r0, MemOperand(r8));
  push(r0);

  // Set this new handler as the current one.
  StoreP(sp, MemOperand(r8));
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  pop(r4);
  mov(ip,
      Operand(ExternalReference(IsolateAddressId::kHandlerAddress, isolate())));
  StoreP(r4, MemOperand(ip));

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
  lhz(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmpi(type_reg, Operand(type));
}


void MacroAssembler::CompareRoot(Register obj, Heap::RootListIndex index) {
  DCHECK(obj != r0);
  LoadRoot(r0, index);
  cmp(obj, r0);
}

void TurboAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);
  DCHECK(overflow_dst != right);

  bool left_is_right = left == right;
  RCBit xorRC = left_is_right ? SetRC : LeaveRC;

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst == left) {
    mr(scratch, left);            // Preserve left.
    add(dst, left, right);        // Left is overwritten.
    xor_(overflow_dst, dst, scratch, xorRC);  // Original left.
    if (!left_is_right) xor_(scratch, dst, right);
  } else if (dst == right) {
    mr(scratch, right);           // Preserve right.
    add(dst, left, right);        // Right is overwritten.
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, scratch);  // Original right.
  } else {
    add(dst, left, right);
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, right);
  }
  if (!left_is_right) and_(overflow_dst, scratch, overflow_dst, SetRC);
}

void TurboAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            intptr_t right,
                                            Register overflow_dst,
                                            Register scratch) {
  Register original_left = left;
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst == left) {
    // Preserve left.
    original_left = overflow_dst;
    mr(original_left, left);
  }
  Add(dst, left, right, scratch);
  xor_(overflow_dst, dst, original_left);
  if (right >= 0) {
    and_(overflow_dst, overflow_dst, dst, SetRC);
  } else {
    andc(overflow_dst, overflow_dst, dst, SetRC);
  }
}

void TurboAssembler::SubAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);
  DCHECK(overflow_dst != right);

  // C = A-B; C overflows if A/B have diff signs and C has diff sign than A
  if (dst == left) {
    mr(scratch, left);      // Preserve left.
    sub(dst, left, right);  // Left is overwritten.
    xor_(overflow_dst, dst, scratch);
    xor_(scratch, scratch, right);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else if (dst == right) {
    mr(scratch, right);     // Preserve right.
    sub(dst, left, right);  // Right is overwritten.
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, scratch);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else {
    sub(dst, left, right);
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, right);
    and_(overflow_dst, scratch, overflow_dst, SetRC);
  }
}


void MacroAssembler::CallStub(CodeStub* stub, Condition cond) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}

void TurboAssembler::CallStubDelayed(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.

  // Block constant pool for the call instruction sequence.
  ConstantPoolUnavailableScope constant_pool_unavailable(this);

  mov(ip, Operand::EmbeddedCode(stub));
  mtctr(ip);
  bctrl();
}

void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}

bool TurboAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DoubleRegister double_input,
                                           Register scratch,
                                           DoubleRegister double_scratch) {
  Label done;
  DCHECK(double_input != double_scratch);

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch);

#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  bne(&done);

  // convert back and compare
  fcfid(double_scratch, double_scratch);
  fcmpu(double_scratch, double_input);
  bind(&done);
}

void TurboAssembler::TruncateDoubleToIDelayed(Zone* zone, Register result,
                                              DoubleRegister double_input) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  mflr(r0);
  push(r0);
  // Put input on stack.
  stfdu(double_input, MemOperand(sp, -kDoubleSize));

  CallStubDelayed(new (zone) DoubleToIStub(nullptr, result));

  addi(sp, sp, Operand(kDoubleSize));
  pop(r0);
  mtlr(r0);

  bind(&done);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  DoubleRegister double_scratch = kScratchDoubleReg;
#if !V8_TARGET_ARCH_PPC64
  Register scratch = ip;
#endif

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  beq(done);
}

void TurboAssembler::CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                                        SaveFPRegsMode save_doubles) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r3, Operand(f->nargs));
  mov(r4, Operand(ExternalReference(f, isolate())));
  CallStubDelayed(new (zone) CEntryStub(nullptr,
#if V8_TARGET_ARCH_PPC64
                                        f->result_size,
#else
                                        1,
#endif
                                        save_doubles));
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r3 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r3, Operand(num_arguments));
  mov(r4, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(isolate(),
#if V8_TARGET_ARCH_PPC64
                  f->result_size,
#else
                  1,
#endif
                  save_doubles);
  CallStub(&stub);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    mov(r3, Operand(function->nargs));
  }
  JumpToExternalReference(ExternalReference(fid, isolate()));
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  mov(r4, Operand(builtin));
  CEntryStub stub(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  builtin_exit_frame);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    lwz(scratch1, MemOperand(scratch2));
    addi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    lwz(scratch1, MemOperand(scratch2));
    subi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}

void TurboAssembler::Assert(Condition cond, AbortReason reason,
                            CRegister cr) {
  if (emit_debug_code()) Check(cond, reason, cr);
}

void TurboAssembler::Check(Condition cond, AbortReason reason, CRegister cr) {
  Label L;
  b(cond, &L, cr);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::Abort(AbortReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != nullptr) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    stop(msg);
    return;
  }
#endif

  LoadSmiLiteral(r4, Smi::FromInt(static_cast<int>(reason)));

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
  TestBitRange(src, kSmiTagSize - 1, 0, r0);
  SmiUntag(dst, src);
  beq(smi_case, cr0);
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
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmi, cr0);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(eq, AbortReason::kOperandIsNotASmi, cr0);
  }
}

void MacroAssembler::AssertFixedArray(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmiAndNotAFixedArray, cr0);
    push(object);
    CompareObjectType(object, object, object, FIXED_ARRAY_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotAFixedArray);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
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
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  TestIfSmi(object, r0);
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

  // Check if JSAsyncGeneratorObject (See MacroAssembler::CompareInstanceType)
  cmpi(instance_type, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

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
    CompareRoot(object, Heap::kUndefinedValueRootIndex);
    beq(&done_checking);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


static const int kRegisterPassedArguments = 8;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  if (num_double_arguments > DoubleRegister::kNumRegisters) {
    stack_passed_words +=
        2 * (num_double_arguments - DoubleRegister::kNumRegisters);
  }
  // Up to 8 simple arguments are passed in registers r3..r10.
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
    mr(scratch, sp);
    addi(sp, sp, Operand(-(stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
    StoreP(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    // Make room for stack arguments
    stack_space += stack_passed_arguments;
  }

  // Allocate frame with required slots to make ABI work.
  li(r0, Operand::Zero());
  StorePU(r0, MemOperand(sp, -stack_space * kPointerSize));
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void TurboAssembler::MovToFloatParameter(DoubleRegister src) { Move(d1, src); }

void TurboAssembler::MovToFloatResult(DoubleRegister src) { Move(d1, src); }

void TurboAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2 == d1) {
    DCHECK(src1 != d2);
    Move(d2, src2);
    Move(d1, src1);
  } else {
    Move(d1, src1);
    Move(d2, src2);
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  mov(ip, Operand(function));
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

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Register dest = function;
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux uses a function descriptor. When calling C code be
    // aware of this descriptor and pick up values from it
    LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(function, kPointerSize));
    LoadP(ip, MemOperand(function, 0));
    dest = ip;
  } else if (ABI_CALL_VIA_IP) {
    Move(ip, function);
    dest = ip;
  }

  Call(dest);

  // Remove frame bought in PrepareCallCFunction
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (ActivationFrameAlignment() > kPointerSize) {
    LoadP(sp, MemOperand(sp, stack_space * kPointerSize));
  } else {
    addi(sp, sp, Operand(stack_space * kPointerSize));
  }
}


void TurboAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask, Condition cc, Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));
  LoadP(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));

  mov(r0, Operand(mask));
  and_(r0, scratch, r0, SetRC);

  if (cc == ne) {
    bne(condition_met, cr0);
  }
  if (cc == eq) {
    beq(condition_met, cr0);
  }
}

void TurboAssembler::SetRoundingMode(FPRoundingMode RN) { mtfsfi(7, RN); }

void TurboAssembler::ResetRoundingMode() {
  mtfsfi(7, kRoundToNearest);  // reset (default is kRoundToNearest)
}


////////////////////////////////////////////////////////////////////////////////
//
// New MacroAssembler Interfaces added for PPC
//
////////////////////////////////////////////////////////////////////////////////
void TurboAssembler::LoadIntLiteral(Register dst, int value) {
  mov(dst, Operand(value));
}

void TurboAssembler::LoadSmiLiteral(Register dst, Smi* smi) {
  mov(dst, Operand(smi));
}

void TurboAssembler::LoadDoubleLiteral(DoubleRegister result, Double value,
                                       Register scratch) {
  if (FLAG_enable_embedded_constant_pool && is_constant_pool_available() &&
      !(scratch == r0 && ConstantPoolAccessIsInOverflow())) {
    ConstantPoolEntry::Access access = ConstantPoolAddEntry(value);
    if (access == ConstantPoolEntry::OVERFLOWED) {
      addis(scratch, kConstantPoolRegister, Operand::Zero());
      lfd(result, MemOperand(scratch, 0));
    } else {
      lfd(result, MemOperand(kConstantPoolRegister, 0));
    }
    return;
  }

  // avoid gcc strict aliasing error using union cast
  union {
    uint64_t dval;
#if V8_TARGET_ARCH_PPC64
    intptr_t ival;
#else
    intptr_t ival[2];
#endif
  } litVal;

  litVal.dval = value.AsUint64();

#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mov(scratch, Operand(litVal.ival));
    mtfprd(result, scratch);
    return;
  }
#endif

  addi(sp, sp, Operand(-kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  mov(scratch, Operand(litVal.ival));
  std(scratch, MemOperand(sp));
#else
  LoadIntLiteral(scratch, litVal.ival[0]);
  stw(scratch, MemOperand(sp, 0));
  LoadIntLiteral(scratch, litVal.ival[1]);
  stw(scratch, MemOperand(sp, 4));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(result, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovIntToDouble(DoubleRegister dst, Register src,
                                    Register scratch) {
// sign-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprwa(dst, src);
    return;
  }
#endif

  DCHECK(src != scratch);
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  extsw(scratch, src);
  std(scratch, MemOperand(sp, 0));
#else
  srawi(scratch, src, 31);
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovUnsignedIntToDouble(DoubleRegister dst, Register src,
                                            Register scratch) {
// zero-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprwz(dst, src);
    return;
  }
#endif

  DCHECK(src != scratch);
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  clrldi(scratch, src, Operand(32));
  std(scratch, MemOperand(sp, 0));
#else
  li(scratch, Operand::Zero());
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovInt64ToDouble(DoubleRegister dst,
#if !V8_TARGET_ARCH_PPC64
                                      Register src_hi,
#endif
                                      Register src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  std(src, MemOperand(sp, 0));
#else
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}


#if V8_TARGET_ARCH_PPC64
void TurboAssembler::MovInt64ComponentsToDouble(DoubleRegister dst,
                                                Register src_hi,
                                                Register src_lo,
                                                Register scratch) {
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    sldi(scratch, src_hi, Operand(32));
    rldimi(scratch, src_lo, 0, 32);
    mtfprd(dst, scratch);
    return;
  }

  subi(sp, sp, Operand(kDoubleSize));
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src_lo, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}
#endif

void TurboAssembler::InsertDoubleLow(DoubleRegister dst, Register src,
                                     Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 0, 32);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::InsertDoubleHigh(DoubleRegister dst, Register src,
                                      Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 32, 0);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kExponentOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleLowToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprwz(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleHighToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(dst, src);
    srdi(dst, dst, Operand(32));
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kExponentOffset));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
    Register dst_hi,
#endif
    Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
#if V8_TARGET_ARCH_PPC64
  ld(dst, MemOperand(sp, 0));
#else
  lwz(dst_hi, MemOperand(sp, Register::kExponentOffset));
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
#endif
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovIntToFloat(DoubleRegister dst, Register src) {
  subi(sp, sp, Operand(kFloatSize));
  stw(src, MemOperand(sp, 0));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfs(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kFloatSize));
}

void TurboAssembler::MovFloatToInt(Register dst, DoubleRegister src) {
  subi(sp, sp, Operand(kFloatSize));
  stfs(src, MemOperand(sp, 0));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kFloatSize));
}

void TurboAssembler::Add(Register dst, Register src, intptr_t value,
                         Register scratch) {
  if (is_int16(value)) {
    addi(dst, src, Operand(value));
  } else {
    mov(scratch, Operand(value));
    add(dst, src, scratch);
  }
}


void MacroAssembler::Cmpi(Register src1, const Operand& src2, Register scratch,
                          CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmp(src1, scratch, cr);
  }
}

void TurboAssembler::Cmpli(Register src1, const Operand& src2, Register scratch,
                           CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmpli(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmpl(src1, scratch, cr);
  }
}

void TurboAssembler::Cmpwi(Register src1, const Operand& src2, Register scratch,
                           CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmpw(src1, scratch, cr);
  }
}


void MacroAssembler::Cmplwi(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmplwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmplw(src1, scratch, cr);
  }
}


void MacroAssembler::And(Register ra, Register rs, const Operand& rb,
                         RCBit rc) {
  if (rb.is_reg()) {
    and_(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.immediate()) && RelocInfo::IsNone(rb.rmode_) &&
        rc == SetRC) {
      andi(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(rs != r0);
      mov(r0, rb);
      and_(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::Or(Register ra, Register rs, const Operand& rb, RCBit rc) {
  if (rb.is_reg()) {
    orx(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.immediate()) && RelocInfo::IsNone(rb.rmode_) &&
        rc == LeaveRC) {
      ori(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(rs != r0);
      mov(r0, rb);
      orx(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::Xor(Register ra, Register rs, const Operand& rb,
                         RCBit rc) {
  if (rb.is_reg()) {
    xor_(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.immediate()) && RelocInfo::IsNone(rb.rmode_) &&
        rc == LeaveRC) {
      xori(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(rs != r0);
      mov(r0, rb);
      xor_(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::CmpSmiLiteral(Register src1, Smi* smi, Register scratch,
                                   CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  cmp(src1, scratch, cr);
#else
  Cmpi(src1, Operand(smi), scratch, cr);
#endif
}


void MacroAssembler::CmplSmiLiteral(Register src1, Smi* smi, Register scratch,
                                    CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  cmpl(src1, scratch, cr);
#else
  Cmpli(src1, Operand(smi), scratch, cr);
#endif
}


void MacroAssembler::AddSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  add(dst, src, scratch);
#else
  Add(dst, src, reinterpret_cast<intptr_t>(smi), scratch);
#endif
}


void MacroAssembler::SubSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  sub(dst, src, scratch);
#else
  Add(dst, src, -(reinterpret_cast<intptr_t>(smi)), scratch);
#endif
}


void MacroAssembler::AndSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch, RCBit rc) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  and_(dst, src, scratch, rc);
#else
  And(dst, src, Operand(smi), rc);
#endif
}


// Load a "pointer" sized value from the memory location
void TurboAssembler::LoadP(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    LoadPX(dst, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // Todo: enhance to use scratch if dst is unsuitable
      DCHECK(dst != r0);
      addi(dst, mem.ra(), Operand((offset & 3) - 4));
      ld(dst, MemOperand(dst, (offset & ~3) + 4));
    } else {
      ld(dst, mem);
    }
#else
    lwz(dst, mem);
#endif
  }
}

void TurboAssembler::LoadPU(Register dst, const MemOperand& mem,
                            Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    LoadPUX(dst, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    ldu(dst, mem);
#else
    lwzu(dst, mem);
#endif
  }
}

// Store a "pointer" sized value to the memory location
void TurboAssembler::StoreP(Register src, const MemOperand& mem,
                            Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    StorePX(src, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // a suitable scratch is required here
      DCHECK(scratch != no_reg);
      if (scratch == r0) {
        LoadIntLiteral(scratch, offset);
        stdx(src, MemOperand(mem.ra(), scratch));
      } else {
        addi(scratch, mem.ra(), Operand((offset & 3) - 4));
        std(src, MemOperand(scratch, (offset & ~3) + 4));
      }
    } else {
      std(src, mem);
    }
#else
    stw(src, mem);
#endif
  }
}

void TurboAssembler::StorePU(Register src, const MemOperand& mem,
                             Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    StorePUX(src, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    stdu(src, mem);
#else
    stwu(src, mem);
#endif
  }
}

void TurboAssembler::LoadWordArith(Register dst, const MemOperand& mem,
                                   Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    lwax(dst, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // Todo: enhance to use scratch if dst is unsuitable
      DCHECK(dst != r0);
      addi(dst, mem.ra(), Operand((offset & 3) - 4));
      lwa(dst, MemOperand(dst, (offset & ~3) + 4));
    } else {
      lwa(dst, mem);
    }
#else
    lwz(dst, mem);
#endif
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadWord(Register dst, const MemOperand& mem,
                              Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lwzx(dst, MemOperand(base, scratch));
  } else {
    lwz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreWord(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    stwx(src, MemOperand(base, scratch));
  } else {
    stw(src, mem);
  }
}


void MacroAssembler::LoadHalfWordArith(Register dst, const MemOperand& mem,
                                       Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    DCHECK(scratch != no_reg);
    mov(scratch, Operand(offset));
    lhax(dst, MemOperand(mem.ra(), scratch));
  } else {
    lha(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadHalfWord(Register dst, const MemOperand& mem,
                                  Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lhzx(dst, MemOperand(base, scratch));
  } else {
    lhz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreHalfWord(Register src, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    sthx(src, MemOperand(base, scratch));
  } else {
    sth(src, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadByte(Register dst, const MemOperand& mem,
                              Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lbzx(dst, MemOperand(base, scratch));
  } else {
    lbz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreByte(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    stbx(src, MemOperand(base, scratch));
  } else {
    stb(src, mem);
  }
}


void MacroAssembler::LoadRepresentation(Register dst, const MemOperand& mem,
                                        Representation r, Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    LoadByte(dst, mem, scratch);
    extsb(dst, dst);
  } else if (r.IsUInteger8()) {
    LoadByte(dst, mem, scratch);
  } else if (r.IsInteger16()) {
    LoadHalfWordArith(dst, mem, scratch);
  } else if (r.IsUInteger16()) {
    LoadHalfWord(dst, mem, scratch);
#if V8_TARGET_ARCH_PPC64
  } else if (r.IsInteger32()) {
    LoadWordArith(dst, mem, scratch);
#endif
  } else {
    LoadP(dst, mem, scratch);
  }
}


void MacroAssembler::StoreRepresentation(Register src, const MemOperand& mem,
                                         Representation r, Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    StoreByte(src, mem, scratch);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    StoreHalfWord(src, mem, scratch);
#if V8_TARGET_ARCH_PPC64
  } else if (r.IsInteger32()) {
    StoreWord(src, mem, scratch);
#endif
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    StoreP(src, mem, scratch);
  }
}

void TurboAssembler::LoadDouble(DoubleRegister dst, const MemOperand& mem,
                                Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    lfdx(dst, MemOperand(base, scratch));
  } else {
    lfd(dst, mem);
  }
}

void MacroAssembler::LoadDoubleU(DoubleRegister dst, const MemOperand& mem,
                                Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    lfdux(dst, MemOperand(base, scratch));
  } else {
    lfdu(dst, mem);
  }
}

void TurboAssembler::LoadSingle(DoubleRegister dst, const MemOperand& mem,
                                Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    lfsx(dst, MemOperand(base, scratch));
  } else {
    lfs(dst, mem);
  }
}

void TurboAssembler::LoadSingleU(DoubleRegister dst, const MemOperand& mem,
                                 Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    lfsux(dst, MemOperand(base, scratch));
  } else {
    lfsu(dst, mem);
  }
}

void TurboAssembler::StoreDouble(DoubleRegister src, const MemOperand& mem,
                                 Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    stfdx(src, MemOperand(base, scratch));
  } else {
    stfd(src, mem);
  }
}

void TurboAssembler::StoreDoubleU(DoubleRegister src, const MemOperand& mem,
                                  Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    stfdux(src, MemOperand(base, scratch));
  } else {
    stfdu(src, mem);
  }
}

void TurboAssembler::StoreSingle(DoubleRegister src, const MemOperand& mem,
                                 Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    stfsx(src, MemOperand(base, scratch));
  } else {
    stfs(src, mem);
  }
}

void TurboAssembler::StoreSingleU(DoubleRegister src, const MemOperand& mem,
                                  Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    stfsux(src, MemOperand(base, scratch));
  } else {
    stfsu(src, mem);
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

void TurboAssembler::SwapP(Register src, Register dst, Register scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  mr(scratch, src);
  mr(src, dst);
  mr(dst, scratch);
}

void TurboAssembler::SwapP(Register src, MemOperand dst, Register scratch) {
  if (dst.ra() != r0) DCHECK(!AreAliased(src, dst.ra(), scratch));
  if (dst.rb() != r0) DCHECK(!AreAliased(src, dst.rb(), scratch));
  DCHECK(!AreAliased(src, scratch));
  mr(scratch, src);
  LoadP(src, dst);
  StoreP(scratch, dst);
}

void TurboAssembler::SwapP(MemOperand src, MemOperand dst, Register scratch_0,
                           Register scratch_1) {
  if (src.ra() != r0) DCHECK(!AreAliased(src.ra(), scratch_0, scratch_1));
  if (src.rb() != r0) DCHECK(!AreAliased(src.rb(), scratch_0, scratch_1));
  if (dst.ra() != r0) DCHECK(!AreAliased(dst.ra(), scratch_0, scratch_1));
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
  fmr(scratch, src);
  fmr(src, dst);
  fmr(dst, scratch);
}

void TurboAssembler::SwapFloat32(DoubleRegister src, MemOperand dst,
                                 DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  fmr(scratch, src);
  LoadSingle(src, dst);
  StoreSingle(scratch, dst);
}

void TurboAssembler::SwapFloat32(MemOperand src, MemOperand dst,
                                 DoubleRegister scratch_0,
                                 DoubleRegister scratch_1) {
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadSingle(scratch_0, src);
  LoadSingle(scratch_1, dst);
  StoreSingle(scratch_0, dst);
  StoreSingle(scratch_1, src);
}

void TurboAssembler::SwapDouble(DoubleRegister src, DoubleRegister dst,
                                DoubleRegister scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  fmr(scratch, src);
  fmr(src, dst);
  fmr(dst, scratch);
}

void TurboAssembler::SwapDouble(DoubleRegister src, MemOperand dst,
                                DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  fmr(scratch, src);
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

#ifdef DEBUG
bool AreAliased(Register reg1, Register reg2, Register reg3, Register reg4,
                Register reg5, Register reg6, Register reg7, Register reg8,
                Register reg9, Register reg10) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() + reg3.is_valid() +
                        reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
                        reg7.is_valid() + reg8.is_valid() + reg9.is_valid() +
                        reg10.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  if (reg9.is_valid()) regs |= reg9.bit();
  if (reg10.is_valid()) regs |= reg10.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}

bool AreAliased(DoubleRegister reg1, DoubleRegister reg2, DoubleRegister reg3,
                DoubleRegister reg4, DoubleRegister reg5, DoubleRegister reg6,
                DoubleRegister reg7, DoubleRegister reg8, DoubleRegister reg9,
                DoubleRegister reg10) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() + reg3.is_valid() +
                        reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
                        reg7.is_valid() + reg8.is_valid() + reg9.is_valid() +
                        reg10.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  if (reg9.is_valid()) regs |= reg9.bit();
  if (reg10.is_valid()) regs |= reg10.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
