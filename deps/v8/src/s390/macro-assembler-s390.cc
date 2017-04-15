// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_S390

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"

#include "src/s390/macro-assembler-s390.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size,
                               CodeObjectRequired create_code_object)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<Object>::New(isolate()->heap()->undefined_value(), isolate());
  }
}

void MacroAssembler::Jump(Register target) { b(target); }

void MacroAssembler::JumpToJSEntry(Register target) {
  Move(ip, target);
  Jump(ip);
}

void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, CRegister) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip);

  DCHECK(rmode == RelocInfo::CODE_TARGET || rmode == RelocInfo::RUNTIME_ENTRY);

  mov(ip, Operand(target, rmode));
  b(ip);

  bind(&skip);
}

void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          CRegister cr) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, cr);
}

void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  jump(code, rmode, cond);
}

int MacroAssembler::CallSize(Register target) { return 2; }  // BASR

void MacroAssembler::Call(Register target) {
  Label start;
  bind(&start);

  // Branch to target via indirect branch
  basr(r14, target);

  DCHECK_EQ(CallSize(target), SizeOfCodeGeneratedSince(&start));
}

void MacroAssembler::CallJSEntry(Register target) {
  DCHECK(target.is(ip));
  Call(target);
}

int MacroAssembler::CallSize(Address target, RelocInfo::Mode rmode,
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

void MacroAssembler::Call(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(cond == al);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(target, rmode, cond);
  Label start;
  bind(&start);
#endif

  mov(ip, Operand(reinterpret_cast<intptr_t>(target), rmode));
  basr(r14, ip);

  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}

int MacroAssembler::CallSize(Handle<Code> code, RelocInfo::Mode rmode,
                             TypeFeedbackId ast_id, Condition cond) {
  return 6;  // BRASL
}

void MacroAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id, Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode) && cond == al);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(code, rmode, ast_id, cond);
  Label start;
  bind(&start);
#endif
  call(code, rmode, ast_id);
  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}

void MacroAssembler::Drop(int count) {
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

void MacroAssembler::Drop(Register count, Register scratch) {
  ShiftLeftP(scratch, count, Operand(kPointerSizeLog2));
  AddP(sp, sp, scratch);
}

void MacroAssembler::Call(Label* target) { b(r14, target); }

void MacroAssembler::Push(Handle<Object> handle) {
  mov(r0, Operand(handle));
  push(r0);
}

void MacroAssembler::Move(Register dst, Handle<Object> value) {
  mov(dst, Operand(value));
}

void MacroAssembler::Move(Register dst, Register src, Condition cond) {
  if (!dst.is(src)) {
    LoadRR(dst, src);
  }
}

void MacroAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (!dst.is(src)) {
    ldr(dst, src);
  }
}

void MacroAssembler::MultiPush(RegList regs, Register location) {
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  SubP(location, location, Operand(stack_offset));
  for (int16_t i = Register::kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      StoreP(ToRegister(i), MemOperand(location, stack_offset));
    }
  }
}

void MacroAssembler::MultiPop(RegList regs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < Register::kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      LoadP(ToRegister(i), MemOperand(location, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  AddP(location, location, Operand(stack_offset));
}

void MacroAssembler::MultiPushDoubles(RegList dregs, Register location) {
  int16_t num_to_push = NumberOfBitsSet(dregs);
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

void MacroAssembler::MultiPopDoubles(RegList dregs, Register location) {
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

void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index,
                              Condition) {
  LoadP(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), r0);
}

void MacroAssembler::StoreRoot(Register source, Heap::RootListIndex index,
                               Condition) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(index));
  StoreP(source, MemOperand(kRootRegister, index << kPointerSizeLog2));
}

void MacroAssembler::InNewSpace(Register object, Register scratch,
                                Condition cond, Label* branch) {
  DCHECK(cond == eq || cond == ne);
  CheckPageFlag(object, scratch, MemoryChunk::kIsInNewSpaceMask, cond, branch);
}

void MacroAssembler::RecordWriteField(
    Register object, int offset, Register value, Register dst,
    LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
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
    AndP(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, Label::kNear);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object, dst, value, lr_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 4)));
    mov(dst, Operand(bit_cast<intptr_t>(kZapValue + 8)));
  }
}

// Will clobber 4 registers: object, map, dst, ip.  The
// register 'object' contains a heap object pointer.
void MacroAssembler::RecordWriteForMap(Register object, Register map,
                                       Register dst,
                                       LinkRegisterStatus lr_status,
                                       SaveFPRegsMode fp_mode) {
  if (emit_debug_code()) {
    LoadP(dst, FieldMemOperand(map, HeapObject::kMapOffset));
    CmpP(dst, Operand(isolate()->factory()->meta_map()));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    CmpP(map, FieldMemOperand(object, HeapObject::kMapOffset));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlag(map,
                map,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);

  lay(dst, MemOperand(object, HeapObject::kMapOffset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    AndP(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, Label::kNear);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(r14);
  }
  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r14);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip, dst);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(dst, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(map, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}

// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(
    Register object, Register address, Register value,
    LinkRegisterStatus lr_status, SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  if (emit_debug_code()) {
    CmpP(value, MemOperand(address));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
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

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  }
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(r14);
  }
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r14);
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

void MacroAssembler::RecordWriteCodeEntryField(Register js_function,
                                               Register code_entry,
                                               Register scratch) {
  const int offset = JSFunction::kCodeEntryOffset;

  // Since a code entry (value) is always in old space, we don't need to update
  // remembered set. If incremental marking is off, there is nothing for us to
  // do.
  if (!FLAG_incremental_marking) return;

  DCHECK(js_function.is(r3));
  DCHECK(code_entry.is(r6));
  DCHECK(scratch.is(r7));
  AssertNotSmi(js_function);

  if (emit_debug_code()) {
    AddP(scratch, js_function, Operand(offset - kHeapObjectTag));
    LoadP(ip, MemOperand(scratch));
    CmpP(ip, code_entry);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  CheckPageFlag(code_entry, scratch,
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  CheckPageFlag(js_function, scratch,
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  const Register dst = scratch;
  AddP(dst, js_function, Operand(offset - kHeapObjectTag));

  // Save caller-saved registers.  js_function and code_entry are in the
  // caller-saved register list.
  DCHECK(kJSCallerSaved & js_function.bit());
  // DCHECK(kJSCallerSaved & code_entry.bit());
  MultiPush(kJSCallerSaved | code_entry.bit() | r14.bit());

  int argument_count = 3;
  PrepareCallCFunction(argument_count, code_entry);

  LoadRR(r2, js_function);
  LoadRR(r3, dst);
  mov(r4, Operand(ExternalReference::isolate_address(isolate())));

  {
    AllowExternalCallThatCantCauseGC scope(this);
    CallCFunction(
        ExternalReference::incremental_marking_record_write_code_entry_function(
            isolate()),
        argument_count);
  }

  // Restore caller-saved registers (including js_function and code_entry).
  MultiPop(kJSCallerSaved | code_entry.bit() | r14.bit());

  bind(&done);
}

void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address, Register scratch,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok);
    stop("Remembered set pointer is in new space");
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(ip, Operand(store_buffer));
  LoadP(scratch, MemOperand(ip));
  // Store pointer to buffer and increment buffer top.
  StoreP(address, MemOperand(scratch));
  AddP(scratch, Operand(kPointerSize));
  // Write back new top of buffer.
  StoreP(scratch, MemOperand(ip));
  // Call stub on end of buffer.
  // Check for end of buffer.
  AndP(scratch, Operand(StoreBuffer::kStoreBufferMask));

  if (and_then == kFallThroughAtEnd) {
    bne(&done, Label::kNear);
  } else {
    DCHECK(and_then == kReturnAtEnd);
    bne(&done, Label::kNear);
  }
  push(r14);
  StoreBufferOverflowStub store_buffer_overflow(isolate(), fp_mode);
  CallStub(&store_buffer_overflow);
  pop(r14);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}

void MacroAssembler::PushCommonFrame(Register marker_reg) {
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

void MacroAssembler::PopCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Pop(r14, fp, marker_reg);
  } else {
    Pop(r14, fp);
  }
}

void MacroAssembler::PushStandardFrame(Register function_reg) {
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

void MacroAssembler::RestoreFrameStateForTailCall() {
  // if (FLAG_enable_embedded_constant_pool) {
  //   LoadP(kConstantPoolRegister,
  //         MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
  //   set_constant_pool_available(false);
  // }
  DCHECK(!FLAG_enable_embedded_constant_pool);
  LoadP(r14, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
}

const RegList MacroAssembler::kSafepointSavedRegisters = Register::kAllocatable;
const int MacroAssembler::kNumSafepointSavedRegisters =
    Register::kNumAllocatable;

// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK(num_unsaved >= 0);
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

void MacroAssembler::StoreToSafepointRegisterSlot(Register src, Register dst) {
  StoreP(src, SafepointRegisterSlot(dst));
}

void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  LoadP(dst, SafepointRegisterSlot(src));
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

MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}

MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  // General purpose registers are pushed last on the stack.
  const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
  int doubles_size = config->num_allocatable_double_registers() * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}

void MacroAssembler::CanonicalizeNaN(const DoubleRegister dst,
                                     const DoubleRegister src) {
  // Turn potential sNaN into qNaN
  if (!dst.is(src)) ldr(dst, src);
  lzdr(kDoubleRegZero);
  sdbr(dst, kDoubleRegZero);
}

void MacroAssembler::ConvertIntToDouble(Register src, DoubleRegister dst) {
  cdfbr(dst, src);
}

void MacroAssembler::ConvertUnsignedIntToDouble(Register src,
                                                DoubleRegister dst) {
  if (CpuFeatures::IsSupported(FLOATING_POINT_EXT)) {
    cdlfbr(Condition(5), Condition(0), dst, src);
  } else {
    // zero-extend src
    llgfr(src, src);
    // convert to double
    cdgbr(dst, src);
  }
}

void MacroAssembler::ConvertIntToFloat(Register src, DoubleRegister dst) {
  cefbr(Condition(4), dst, src);
}

void MacroAssembler::ConvertUnsignedIntToFloat(Register src,
                                               DoubleRegister dst) {
  celfbr(Condition(4), Condition(0), dst, src);
}

#if V8_TARGET_ARCH_S390X
void MacroAssembler::ConvertInt64ToDouble(Register src,
                                          DoubleRegister double_dst) {
  cdgbr(double_dst, src);
}

void MacroAssembler::ConvertUnsignedInt64ToFloat(Register src,
                                                 DoubleRegister double_dst) {
  celgbr(Condition(0), Condition(0), double_dst, src);
}

void MacroAssembler::ConvertUnsignedInt64ToDouble(Register src,
                                                  DoubleRegister double_dst) {
  cdlgbr(Condition(0), Condition(0), double_dst, src);
}

void MacroAssembler::ConvertInt64ToFloat(Register src,
                                         DoubleRegister double_dst) {
  cegbr(double_dst, src);
}
#endif

void MacroAssembler::ConvertFloat32ToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_S390X
                                           const Register dst_hi,
#endif
                                           const Register dst,
                                           const DoubleRegister double_dst,
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
  ldgr(double_dst, dst);
#if !V8_TARGET_ARCH_S390X
  srlg(dst_hi, dst, Operand(32));
#endif
}

void MacroAssembler::ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_S390X
                                          const Register dst_hi,
#endif
                                          const Register dst,
                                          const DoubleRegister double_dst,
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
  ldgr(double_dst, dst);
#if !V8_TARGET_ARCH_S390X
  srlg(dst_hi, dst, Operand(32));
#endif
}

void MacroAssembler::ConvertFloat32ToInt32(const DoubleRegister double_input,
                                           const Register dst,
                                           const DoubleRegister double_dst,
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
  cfebr(m, dst, double_input);
  Label done;
  b(Condition(0xe), &done, Label::kNear);  // special case
  LoadImmP(dst, Operand::Zero());
  bind(&done);
  ldgr(double_dst, dst);
}

void MacroAssembler::ConvertFloat32ToUnsignedInt32(
    const DoubleRegister double_input, const Register dst,
    const DoubleRegister double_dst, FPRoundingMode rounding_mode) {
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
  clfebr(m, Condition(0), dst, double_input);
  Label done;
  b(Condition(0xe), &done, Label::kNear);  // special case
  LoadImmP(dst, Operand::Zero());
  bind(&done);
  ldgr(double_dst, dst);
}

#if V8_TARGET_ARCH_S390X
void MacroAssembler::ConvertFloat32ToUnsignedInt64(
    const DoubleRegister double_input, const Register dst,
    const DoubleRegister double_dst, FPRoundingMode rounding_mode) {
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
  clgebr(m, Condition(0), dst, double_input);
  ldgr(double_dst, dst);
}

void MacroAssembler::ConvertDoubleToUnsignedInt64(
    const DoubleRegister double_input, const Register dst,
    const DoubleRegister double_dst, FPRoundingMode rounding_mode) {
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
  ldgr(double_dst, dst);
}

#endif

#if !V8_TARGET_ARCH_S390X
void MacroAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  sldl(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void MacroAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  sldl(r0, r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void MacroAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srdl(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void MacroAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srdl(r0, r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void MacroAssembler::ShiftRightArithPair(Register dst_low, Register dst_high,
                                         Register src_low, Register src_high,
                                         Register scratch, Register shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srda(r0, shift, Operand::Zero());
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}

void MacroAssembler::ShiftRightArithPair(Register dst_low, Register dst_high,
                                         Register src_low, Register src_high,
                                         uint32_t shift) {
  LoadRR(r0, src_high);
  LoadRR(r1, src_low);
  srda(r0, r0, Operand(shift));
  LoadRR(dst_high, r0);
  LoadRR(dst_low, r1);
}
#endif

void MacroAssembler::MovDoubleToInt64(Register dst, DoubleRegister src) {
  lgdr(dst, src);
}

void MacroAssembler::MovInt64ToDouble(DoubleRegister dst, Register src) {
  ldgr(dst, src);
}

void MacroAssembler::StubPrologue(StackFrame::Type type, Register base,
                                  int prologue_offset) {
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(this);
    LoadSmiLiteral(r1, Smi::FromInt(type));
    PushCommonFrame(r1);
  }
}

void MacroAssembler::Prologue(bool code_pre_aging, Register base,
                              int prologue_offset) {
  DCHECK(!base.is(no_reg));
  {
    PredictableCodeSizeScope predictible_code_size_scope(
        this, kNoCodeAgeSequenceLength);
    // The following instructions must remain together and unmodified
    // for code aging to work properly.
    if (code_pre_aging) {
      // Pre-age the code.
      // This matches the code found in PatchPlatformCodeAge()
      Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
      intptr_t target = reinterpret_cast<intptr_t>(stub->instruction_start());
      nop();
      CleanseP(r14);
      Push(r14);
      mov(r2, Operand(target));
      Call(r2);
      for (int i = 0; i < kNoCodeAgeSequenceLength - kCodeAgingSequenceLength;
           i += 2) {
        // TODO(joransiu): Create nop function to pad
        //         (kNoCodeAgeSequenceLength - kCodeAgingSequenceLength) bytes.
        nop();  // 2-byte nops().
      }
    } else {
      // This matches the code found in GetNoCodeAgeSequence()
      PushStandardFrame(r3);
    }
  }
}

void MacroAssembler::EmitLoadFeedbackVector(Register vector) {
  LoadP(vector, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  LoadP(vector, FieldMemOperand(vector, JSFunction::kLiteralsOffset));
  LoadP(vector, FieldMemOperand(vector, LiteralsArray::kFeedbackVectorOffset));
}

void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // We create a stack frame with:
  //    Return Addr <-- old sp
  //    Old FP      <-- new fp
  //    CP
  //    type
  //    CodeObject  <-- new sp

  LoadSmiLiteral(ip, Smi::FromInt(type));
  PushCommonFrame(ip);

  if (type == StackFrame::INTERNAL) {
    mov(r0, Operand(CodeObject()));
    push(r0);
  }
}

int MacroAssembler::LeaveFrame(StackFrame::Type type, int stack_adjustment) {
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer, return address and constant pool pointer.
  LoadP(r14, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  lay(r1, MemOperand(
              fp, StandardFrameConstants::kCallerSPOffset + stack_adjustment));
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  LoadRR(sp, r1);
  int frame_ends = pc_offset();
  return frame_ends;
}

void MacroAssembler::EnterBuiltinFrame(Register context, Register target,
                                       Register argc) {
  CleanseP(r14);
  Push(r14, fp, context, target);
  la(fp, MemOperand(sp, 2 * kPointerSize));
  Push(argc);
}

void MacroAssembler::LeaveBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Pop(argc);
  Pop(r14, fp, context, target);
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
// in the fp register (r11)
// Then - we buy a new frame

// r14
// oldFP <- newFP
// SP
// Code
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
  DCHECK(stack_space > 0);

  // This is an opportunity to build a frame to wrap
  // all of the pushes that have happened inside of V8
  // since we were called from C code
  CleanseP(r14);
  LoadSmiLiteral(r1, Smi::FromInt(frame_type));
  PushCommonFrame(r1);
  // Reserve room for saved entry sp and code object.
  lay(sp, MemOperand(fp, -ExitFrameConstants::kFixedFrameSizeFromFp));

  if (emit_debug_code()) {
    StoreP(MemOperand(fp, ExitFrameConstants::kSPOffset), Operand::Zero(), r1);
  }
  mov(r1, Operand(CodeObject()));
  StoreP(r1, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  mov(r1, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(fp, MemOperand(r1));
  mov(r1, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
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
  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (frame_alignment > 0) {
    DCHECK(frame_alignment == 8);
    ClearRightImm(sp, sp, Operand(3));  // equivalent to &= -8
  }

  lay(sp, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));
  StoreP(MemOperand(sp), Operand::Zero(), r0);
  // Set the exit frame sp value to point just before the return address
  // location.
  lay(r1, MemOperand(sp, kStackFrameSPSlot * kPointerSize));
  StoreP(r1, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

void MacroAssembler::InitializeNewString(Register string, Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1, Register scratch2) {
  SmiTag(scratch1, length);
  LoadRoot(scratch2, map_index);
  StoreP(scratch1, FieldMemOperand(string, String::kLengthOffset));
  StoreP(FieldMemOperand(string, String::kHashFieldSlot),
         Operand(String::kEmptyHashField), scratch1);
  StoreP(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
}

int MacroAssembler::ActivationFrameAlignment() {
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
                                    bool restore_context,
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
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(MemOperand(ip), Operand(0, kRelocInfo_NONEPTR), r0);

  // Restore current context from top and clear it in debug mode.
  if (restore_context) {
    mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
    LoadP(cp, MemOperand(ip));
  }
#ifdef DEBUG
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  StoreP(MemOperand(ip), Operand(0, kRelocInfo_NONEPTR), r0);
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

void MacroAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d0);
}

void MacroAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d0);
}

void MacroAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
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
    Check(lt, kStackAccessBelowStackPointer);
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
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
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
  //  DCHECK(actual.is_immediate() || actual.reg().is(r2));
  //  DCHECK(expected.is_immediate() || expected.reg().is(r4));

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
    Handle<Code> adaptor = isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor);
      call_wrapper.AfterCall();
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
  mov(r6, Operand(debug_hook_avtive));
  LoadB(r6, MemOperand(r6));
  CmpP(r6, Operand::Zero());
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
                                        InvokeFlag flag,
                                        const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(function.is(r3));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(r5));

  if (call_wrapper.NeedsDebugHookCheck()) {
    CheckDebugHook(function, new_target, expected, actual);
  }

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 call_wrapper);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = ip;
    LoadP(code, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      CallJSEntry(code);
      call_wrapper.AfterCall();
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
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(fun.is(r3));

  Register expected_reg = r4;
  Register temp_reg = r6;
  LoadP(temp_reg, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  LoadW(expected_reg,
        FieldMemOperand(temp_reg,
                        SharedFunctionInfo::kFormalParameterCountOffset));
#if !defined(V8_TARGET_ARCH_S390X)
  SmiUntag(expected_reg);
#endif

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(fun, new_target, expected, actual, flag, call_wrapper);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(function.is(r3));

  // Get the function and setup the context.
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  InvokeFunctionCode(r3, no_reg, expected, actual, flag, call_wrapper);
}

void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(r3, function);
  InvokeFunction(r3, expected, actual, flag, call_wrapper);
}

void MacroAssembler::IsObjectJSStringType(Register object, Register scratch,
                                          Label* fail) {
  DCHECK(kNotStringTag != 0);

  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  mov(r0, Operand(kIsNotStringMask));
  AndP(r0, scratch);
  bne(fail);
}

void MacroAssembler::IsObjectNameType(Register object, Register scratch,
                                      Label* fail) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  CmpP(scratch, Operand(LAST_NAME_TYPE));
  bgt(fail);
}

void MacroAssembler::DebugBreak() {
  LoadImmP(r2, Operand::Zero());
  mov(r3,
      Operand(ExternalReference(Runtime::kHandleDebuggerStatement, isolate())));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUGGER_STATEMENT);
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  // Link the current handler as the next handler.
  mov(r7, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));

  // Buy the full stack frame for 5 slots.
  lay(sp, MemOperand(sp, -StackHandlerConstants::kSize));

  // Copy the old handler into the next handler slot.
  mvc(MemOperand(sp, StackHandlerConstants::kNextOffset), MemOperand(r7),
      kPointerSize);
  // Set this new handler as the current one.
  StoreP(sp, MemOperand(r7));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  // Pop the Next Handler into r3 and store it into Handler Address reference.
  Pop(r3);
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));

  StoreP(r3, MemOperand(ip));
}

// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register t0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiUntag(scratch);

  // Xor original key with a seed.
  XorP(t0, scratch);

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  LoadRR(scratch, t0);
  NotP(scratch);
  sll(t0, Operand(15));
  AddP(t0, scratch, t0);
  // hash = hash ^ (hash >> 12);
  ShiftRight(scratch, t0, Operand(12));
  XorP(t0, scratch);
  // hash = hash + (hash << 2);
  ShiftLeft(scratch, t0, Operand(2));
  AddP(t0, t0, scratch);
  // hash = hash ^ (hash >> 4);
  ShiftRight(scratch, t0, Operand(4));
  XorP(t0, scratch);
  // hash = hash * 2057;
  LoadRR(r0, t0);
  ShiftLeft(scratch, t0, Operand(3));
  AddP(t0, t0, scratch);
  ShiftLeft(scratch, r0, Operand(11));
  AddP(t0, t0, scratch);
  // hash = hash ^ (hash >> 16);
  ShiftRight(scratch, t0, Operand(16));
  XorP(t0, scratch);
  // hash & 0x3fffffff
  ExtractBitRange(t0, t0, 29, 0);
}

void MacroAssembler::Allocate(int object_size, Register result,
                              Register scratch1, Register scratch2,
                              Label* gc_required, AllocationFlags flags) {
  DCHECK(object_size <= kMaxRegularHeapObjectSize);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      LoadImmP(result, Operand(0x7091));
      LoadImmP(scratch1, Operand(0x7191));
      LoadImmP(scratch2, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  DCHECK(!AreAliased(result, scratch1, scratch2, ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK_EQ(0, static_cast<int>(object_size & kObjectAlignmentMask));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address register.
  Register top_address = scratch1;
  Register result_end = scratch2;
  mov(top_address, Operand(allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    LoadP(result, MemOperand(top_address));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      CmpP(result, MemOperand(top_address));
      Check(eq, kUnexpectedAllocationTop);
    }
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
// Align the next allocation. Storing the filler map without checking top is
// safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(result_end, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    if ((flags & PRETENURE) != 0) {
      CmpLogicalP(result, MemOperand(top_address, limit - top));
      bge(gc_required);
    }
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(result_end, MemOperand(result));
    AddP(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  AddP(result_end, result, Operand(object_size));

  // Compare with allocation limit.
  CmpLogicalP(result_end, MemOperand(top_address, limit - top));
  bge(gc_required);

  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    StoreP(result_end, MemOperand(top_address));
  }

  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // Prefetch the allocation_top's next cache line in advance to
    // help alleviate potential cache misses.
    // Mode 2 - Prefetch the data into a cache line for store access.
    pfd(r2, MemOperand(result, 256));
  }

  // Tag object.
  la(result, MemOperand(result, kHeapObjectTag));
}

void MacroAssembler::Allocate(Register object_size, Register result,
                              Register result_end, Register scratch,
                              Label* gc_required, AllocationFlags flags) {
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      LoadImmP(result, Operand(0x7091));
      LoadImmP(scratch, Operand(0x7191));
      LoadImmP(result_end, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  // |object_size| and |result_end| may overlap if the DOUBLE_ALIGNMENT flag
  // is not specified. Other registers must not overlap.
  DCHECK(!AreAliased(object_size, result, scratch, ip));
  DCHECK(!AreAliased(result_end, result, scratch, ip));
  DCHECK((flags & DOUBLE_ALIGNMENT) == 0 || !object_size.is(result_end));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address and allocation limit registers.
  Register top_address = scratch;
  mov(top_address, Operand(allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result
    LoadP(result, MemOperand(top_address));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      CmpP(result, MemOperand(top_address));
      Check(eq, kUnexpectedAllocationTop);
    }
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
// Align the next allocation. Storing the filler map without checking top is
// safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(result_end, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    if ((flags & PRETENURE) != 0) {
      CmpLogicalP(result, MemOperand(top_address, limit - top));
      bge(gc_required);
    }
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(result_end, MemOperand(result));
    AddP(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    ShiftLeftP(result_end, object_size, Operand(kPointerSizeLog2));
    AddP(result_end, result, result_end);
  } else {
    AddP(result_end, result, object_size);
  }
  CmpLogicalP(result_end, MemOperand(top_address, limit - top));
  bge(gc_required);

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    AndP(r0, result_end, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace, cr0);
  }
  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    StoreP(result_end, MemOperand(top_address));
  }

  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // Prefetch the allocation_top's next cache line in advance to
    // help alleviate potential cache misses.
    // Mode 2 - Prefetch the data into a cache line for store access.
    pfd(r2, MemOperand(result, 256));
  }

  // Tag object.
  la(result, MemOperand(result, kHeapObjectTag));
}

void MacroAssembler::FastAllocate(Register object_size, Register result,
                                  Register result_end, Register scratch,
                                  AllocationFlags flags) {
  // |object_size| and |result_end| may overlap if the DOUBLE_ALIGNMENT flag
  // is not specified. Other registers must not overlap.
  DCHECK(!AreAliased(object_size, result, scratch, ip));
  DCHECK(!AreAliased(result_end, result, scratch, ip));
  DCHECK((flags & DOUBLE_ALIGNMENT) == 0 || !object_size.is(result_end));

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  Register top_address = scratch;
  mov(top_address, Operand(allocation_top));
  LoadP(result, MemOperand(top_address));

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
// Align the next allocation. Storing the filler map without checking top is
// safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(result_end, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(result_end, MemOperand(result));
    AddP(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top using result. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    ShiftLeftP(result_end, object_size, Operand(kPointerSizeLog2));
    AddP(result_end, result, result_end);
  } else {
    AddP(result_end, result, object_size);
  }

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    AndP(r0, result_end, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace, cr0);
  }
  StoreP(result_end, MemOperand(top_address));

  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // Prefetch the allocation_top's next cache line in advance to
    // help alleviate potential cache misses.
    // Mode 2 - Prefetch the data into a cache line for store access.
    pfd(r2, MemOperand(result, 256));
  }

  // Tag object.
  la(result, MemOperand(result, kHeapObjectTag));
}

void MacroAssembler::FastAllocate(int object_size, Register result,
                                  Register scratch1, Register scratch2,
                                  AllocationFlags flags) {
  DCHECK(object_size <= kMaxRegularHeapObjectSize);
  DCHECK(!AreAliased(result, scratch1, scratch2, ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK_EQ(0, object_size & kObjectAlignmentMask);

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Set up allocation top address register.
  Register top_address = scratch1;
  Register result_end = scratch2;
  mov(top_address, Operand(allocation_top));
  LoadP(result, MemOperand(top_address));

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
// Align the next allocation. Storing the filler map without checking top is
// safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(result_end, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(result_end, MemOperand(result));
    AddP(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

#if V8_TARGET_ARCH_S390X
  // Limit to 64-bit only, as double alignment check above may adjust
  // allocation top by an extra kDoubleSize/2.
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) && is_int8(object_size)) {
    // Update allocation top.
    AddP(MemOperand(top_address), Operand(object_size));
  } else {
    // Calculate new top using result.
    AddP(result_end, result, Operand(object_size));
    // Update allocation top.
    StoreP(result_end, MemOperand(top_address));
  }
#else
  // Calculate new top using result.
  AddP(result_end, result, Operand(object_size));
  // Update allocation top.
  StoreP(result_end, MemOperand(top_address));
#endif

  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // Prefetch the allocation_top's next cache line in advance to
    // help alleviate potential cache misses.
    // Mode 2 - Prefetch the data into a cache line for store access.
    pfd(r2, MemOperand(result, 256));
  }

  // Tag object.
  la(result, MemOperand(result, kHeapObjectTag));
}

void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  const Register temp = type_reg.is(no_reg) ? r0 : type_reg;

  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}

void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  LoadlB(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  CmpP(type_reg, Operand(type));
}

void MacroAssembler::CompareRoot(Register obj, Heap::RootListIndex index) {
  CmpP(obj, MemOperand(kRootRegister, index << kPointerSizeLog2));
}

void MacroAssembler::SmiToDouble(DoubleRegister value, Register smi) {
  SmiUntag(ip, smi);
  ConvertIntToDouble(ip, value);
}

void MacroAssembler::CompareMap(Register obj, Register scratch, Handle<Map> map,
                                Label* early_success) {
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(obj, map, early_success);
}

void MacroAssembler::CompareMap(Register obj_map, Handle<Map> map,
                                Label* early_success) {
  mov(r0, Operand(map));
  CmpP(r0, FieldMemOperand(obj_map, HeapObject::kMapOffset));
}

void MacroAssembler::CheckMap(Register obj, Register scratch, Handle<Map> map,
                              Label* fail, SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, scratch, map, &success);
  bne(fail);
  bind(&success);
}

void MacroAssembler::CheckMap(Register obj, Register scratch,
                              Heap::RootListIndex index, Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareRoot(scratch, index);
  bne(fail);
}

void MacroAssembler::DispatchWeakMap(Register obj, Register scratch1,
                                     Register scratch2, Handle<WeakCell> cell,
                                     Handle<Code> success,
                                     SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  LoadP(scratch1, FieldMemOperand(obj, HeapObject::kMapOffset));
  CmpWeakValue(scratch1, cell, scratch2);
  Jump(success, RelocInfo::CODE_TARGET, eq);
  bind(&fail);
}

void MacroAssembler::CmpWeakValue(Register value, Handle<WeakCell> cell,
                                  Register scratch, CRegister) {
  mov(scratch, Operand(cell));
  CmpP(value, FieldMemOperand(scratch, WeakCell::kValueOffset));
}

void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  mov(value, Operand(cell));
  LoadP(value, FieldMemOperand(value, WeakCell::kValueOffset));
}

void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
}

void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp, Register temp2) {
  Label done, loop;
  LoadP(result, FieldMemOperand(map, Map::kConstructorOrBackPointerOffset));
  bind(&loop);
  JumpIfSmi(result, &done);
  CompareObjectType(result, temp, temp2, MAP_TYPE);
  bne(&done);
  LoadP(result, FieldMemOperand(result, Map::kConstructorOrBackPointerOffset));
  b(&loop);
  bind(&done);
}

void MacroAssembler::TryGetFunctionPrototype(Register function, Register result,
                                             Register scratch, Label* miss) {
  // Get the prototype or initial map from the function.
  LoadP(result,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  CompareRoot(result, Heap::kTheHoleValueRootIndex);
  beq(miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CompareObjectType(result, scratch, scratch, MAP_TYPE);
  bne(&done, Label::kNear);

  // Get the prototype from the initial map.
  LoadP(result, FieldMemOperand(result, Map::kPrototypeOffset));

  // All done.
  bind(&done);
}

void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id,
                              Condition cond) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id, cond);
}

void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}

bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::TestDoubleIsInt32(DoubleRegister double_input,
                                       Register scratch1, Register scratch2,
                                       DoubleRegister double_scratch) {
  TryDoubleToInt32Exact(scratch1, double_input, scratch2, double_scratch);
}

void MacroAssembler::TestDoubleIsMinusZero(DoubleRegister input,
                                           Register scratch1,
                                           Register scratch2) {
  lgdr(scratch1, input);
#if V8_TARGET_ARCH_S390X
  llihf(scratch2, Operand(0x80000000));  // scratch2 = 0x80000000_00000000
  CmpP(scratch1, scratch2);
#else
  Label done;
  CmpP(scratch1, Operand::Zero());
  bne(&done, Label::kNear);

  srlg(scratch1, scratch1, Operand(32));
  CmpP(scratch1, Operand(HeapNumber::kSignMask));
  bind(&done);
#endif
}

void MacroAssembler::TestDoubleSign(DoubleRegister input, Register scratch) {
  lgdr(scratch, input);
  cgfi(scratch, Operand::Zero());
}

void MacroAssembler::TestHeapNumberSign(Register input, Register scratch) {
  LoadlW(scratch, FieldMemOperand(input, HeapNumber::kValueOffset +
                                             Register::kExponentOffset));
  Cmp32(scratch, Operand::Zero());
}

void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DoubleRegister double_input,
                                           Register scratch,
                                           DoubleRegister double_scratch) {
  Label done;
  DCHECK(!double_input.is(double_scratch));

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_S390X
                       scratch,
#endif
                       result, double_scratch);

#if V8_TARGET_ARCH_S390X
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  bne(&done);

  // convert back and compare
  lgdr(scratch, double_scratch);
  cdfbr(double_scratch, scratch);
  cdbr(double_scratch, double_input);
  bind(&done);
}

void MacroAssembler::TryInt32Floor(Register result, DoubleRegister double_input,
                                   Register input_high, Register scratch,
                                   DoubleRegister double_scratch, Label* done,
                                   Label* exact) {
  DCHECK(!result.is(input_high));
  DCHECK(!double_input.is(double_scratch));
  Label exception;

  // Move high word into input_high
  lay(sp, MemOperand(sp, -kDoubleSize));
  StoreDouble(double_input, MemOperand(sp));
  LoadlW(input_high, MemOperand(sp, Register::kExponentOffset));
  la(sp, MemOperand(sp, kDoubleSize));

  // Test for NaN/Inf
  ExtractBitMask(result, input_high, HeapNumber::kExponentMask);
  CmpLogicalP(result, Operand(0x7ff));
  beq(&exception);

  // Convert (rounding to -Inf)
  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_S390X
                       scratch,
#endif
                       result, double_scratch, kRoundToMinusInf);

// Test for overflow
#if V8_TARGET_ARCH_S390X
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  bne(&exception);

  // Test for exactness
  lgdr(scratch, double_scratch);
  cdfbr(double_scratch, scratch);
  cdbr(double_scratch, double_input);
  beq(exact);
  b(done);

  bind(&exception);
}

void MacroAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  DoubleRegister double_scratch = kScratchDoubleReg;
#if !V8_TARGET_ARCH_S390X
  Register scratch = ip;
#endif

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_S390X
                       scratch,
#endif
                       result, double_scratch);

// Test for overflow
#if V8_TARGET_ARCH_S390X
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  beq(done);
}

void MacroAssembler::TruncateDoubleToI(Register result,
                                       DoubleRegister double_input) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(r14);
  // Put input on stack.
  lay(sp, MemOperand(sp, -kDoubleSize));
  StoreDouble(double_input, MemOperand(sp));

  DoubleToIStub stub(isolate(), sp, result, 0, true, true);
  CallStub(&stub);

  la(sp, MemOperand(sp, kDoubleSize));
  pop(r14);

  bind(&done);
}

void MacroAssembler::TruncateHeapNumberToI(Register result, Register object) {
  Label done;
  DoubleRegister double_scratch = kScratchDoubleReg;
  DCHECK(!result.is(object));

  LoadDouble(double_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));
  TryInlineTruncateDoubleToI(result, double_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(r14);
  DoubleToIStub stub(isolate(), object, result,
                     HeapNumber::kValueOffset - kHeapObjectTag, true, true);
  CallStub(&stub);
  pop(r14);

  bind(&done);
}

void MacroAssembler::TruncateNumberToI(Register object, Register result,
                                       Register heap_number_map,
                                       Register scratch1, Label* not_number) {
  Label done;
  DCHECK(!result.is(object));

  UntagAndJumpIfSmi(result, object, &done);
  JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_number);
  TruncateHeapNumberToI(result, object);

  bind(&done);
}

void MacroAssembler::GetLeastBitsFromSmi(Register dst, Register src,
                                         int num_least_bits) {
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // We rotate by kSmiShift amount, and extract the num_least_bits
    risbg(dst, src, Operand(64 - num_least_bits), Operand(63),
          Operand(64 - kSmiShift), true);
  } else {
    SmiUntag(dst, src);
    AndP(dst, Operand((1 << num_least_bits) - 1));
  }
}

void MacroAssembler::GetLeastBitsFromInt32(Register dst, Register src,
                                           int num_least_bits) {
  AndP(dst, src, Operand((1 << num_least_bits) - 1));
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
  mov(r3, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(isolate(),
#if V8_TARGET_ARCH_S390X
                  f->result_size,
#else
                  1,
#endif
                  save_doubles);
  CallStub(&stub);
}

void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  mov(r2, Operand(num_arguments));
  mov(r3, Operand(ext));

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    mov(r2, Operand(function->nargs));
  }
  JumpToExternalReference(ExternalReference(fid, isolate()));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  mov(r3, Operand(builtin));
  CEntryStub stub(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  builtin_exit_frame);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}

void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(value));
    mov(scratch2, Operand(ExternalReference(counter)));
    StoreW(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(ExternalReference(counter)));
    // @TODO(john.yan): can be optimized by asi()
    LoadW(scratch2, MemOperand(scratch1));
    AddP(scratch2, Operand(value));
    StoreW(scratch2, MemOperand(scratch1));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(ExternalReference(counter)));
    // @TODO(john.yan): can be optimized by asi()
    LoadW(scratch2, MemOperand(scratch1));
    AddP(scratch2, Operand(-value));
    StoreW(scratch2, MemOperand(scratch1));
  }
}

void MacroAssembler::Assert(Condition cond, BailoutReason reason,
                            CRegister cr) {
  if (emit_debug_code()) Check(cond, reason, cr);
}

void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    DCHECK(!elements.is(r0));
    Label ok;
    push(elements);
    LoadP(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    CompareRoot(elements, Heap::kFixedArrayMapRootIndex);
    beq(&ok, Label::kNear);
    CompareRoot(elements, Heap::kFixedDoubleArrayMapRootIndex);
    beq(&ok, Label::kNear);
    CompareRoot(elements, Heap::kFixedCOWArrayMapRootIndex);
    beq(&ok, Label::kNear);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
    pop(elements);
  }
}

void MacroAssembler::Check(Condition cond, BailoutReason reason, CRegister cr) {
  Label L;
  b(cond, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void MacroAssembler::Abort(BailoutReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    stop(msg);
    return;
  }
#endif

  // Check if Abort() has already been initialized.
  DCHECK(isolate()->builtins()->Abort()->IsHeapObject());

  LoadSmiLiteral(r3, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(isolate()->builtins()->Abort(), RelocInfo::CODE_TARGET);
  } else {
    Call(isolate()->builtins()->Abort(), RelocInfo::CODE_TARGET);
  }
  // will not return here
}

void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    LoadP(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      LoadP(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    LoadRR(dst, cp);
  }
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  LoadP(dst, NativeContextMemOperand());
  LoadP(dst, ContextMemOperand(dst, index));
}

void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  LoadP(map,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    b(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}

void MacroAssembler::JumpIfNotPowerOfTwoOrZero(
    Register reg, Register scratch, Label* not_power_of_two_or_zero) {
  SubP(scratch, reg, Operand(1));
  CmpP(scratch, Operand::Zero());
  blt(not_power_of_two_or_zero);
  AndP(r0, reg, scratch /*, SetRC*/);  // Should be okay to remove rc
  bne(not_power_of_two_or_zero /*, cr0*/);
}

void MacroAssembler::JumpIfNotPowerOfTwoOrZeroAndNeg(Register reg,
                                                     Register scratch,
                                                     Label* zero_and_neg,
                                                     Label* not_power_of_two) {
  SubP(scratch, reg, Operand(1));
  CmpP(scratch, Operand::Zero());
  blt(zero_and_neg);
  AndP(r0, reg, scratch /*, SetRC*/);  // Should be okay to remove rc
  bne(not_power_of_two /*, cr0*/);
}

#if !V8_TARGET_ARCH_S390X
void MacroAssembler::SmiTagCheckOverflow(Register reg, Register overflow) {
  DCHECK(!reg.is(overflow));
  LoadRR(overflow, reg);  // Save original value.
  SmiTag(reg);
  XorP(overflow, overflow, reg);  // Overflow if (value ^ 2 * value) < 0.
  LoadAndTestRR(overflow, overflow);
}

void MacroAssembler::SmiTagCheckOverflow(Register dst, Register src,
                                         Register overflow) {
  if (dst.is(src)) {
    // Fall back to slower case.
    SmiTagCheckOverflow(dst, overflow);
  } else {
    DCHECK(!dst.is(src));
    DCHECK(!dst.is(overflow));
    DCHECK(!src.is(overflow));
    SmiTag(dst, src);
    XorP(overflow, dst, src);  // Overflow if (value ^ 2 * value) < 0.
    LoadAndTestRR(overflow, overflow);
  }
}
#endif

void MacroAssembler::JumpIfNotBothSmi(Register reg1, Register reg2,
                                      Label* on_not_both_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  OrP(r0, reg1, reg2 /*, LeaveRC*/);  // should be okay to remove LeaveRC
  JumpIfNotSmi(r0, on_not_both_smi);
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

void MacroAssembler::AssertNotNumber(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsANumber, cr0);
    push(object);
    CompareObjectType(object, object, object, HEAP_NUMBER_TYPE);
    pop(object);
    Check(ne, kOperandIsANumber);
  }
}

void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmi, cr0);
  }
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(eq, kOperandIsNotSmi, cr0);
  }
}

void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAString, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(lt, kOperandIsNotAString);
  }
}

void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAName, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, LAST_NAME_TYPE);
    pop(object);
    Check(le, kOperandIsNotAName);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_FUNCTION_TYPE);
    pop(object);
    Check(eq, kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotABoundFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAGeneratorObject, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_GENERATOR_OBJECT_TYPE);
    pop(object);
    Check(eq, kOperandIsNotAGeneratorObject);
  }
}

void MacroAssembler::AssertReceiver(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAReceiver, cr0);
    push(object);
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    CompareObjectType(object, object, object, FIRST_JS_RECEIVER_TYPE);
    pop(object);
    Check(ge, kOperandIsNotAReceiver);
  }
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, Heap::kUndefinedValueRootIndex);
    beq(&done_checking, Label::kNear);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

void MacroAssembler::AssertIsRoot(Register reg, Heap::RootListIndex index) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, kHeapNumberMapRegisterClobbered);
  }
}

void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  CmpP(scratch, heap_number_map);
  bne(on_not_heap_number);
}

void MacroAssembler::JumpIfNonSmisNotBothSequentialOneByteStrings(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential one-byte strings.
  // Assume that they are non-smis.
  LoadP(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  LoadP(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  LoadlB(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  LoadlB(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialOneByte(scratch1, scratch2, scratch1,
                                                 scratch2, failure);
}

void MacroAssembler::JumpIfNotBothSequentialOneByteStrings(Register first,
                                                           Register second,
                                                           Register scratch1,
                                                           Register scratch2,
                                                           Label* failure) {
  // Check that neither is a smi.
  AndP(scratch1, first, second);
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialOneByteStrings(first, second, scratch1,
                                               scratch2, failure);
}

void MacroAssembler::JumpIfNotUniqueNameInstanceType(Register reg,
                                                     Label* not_unique_name) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  AndP(r0, reg, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  beq(&succeed, Label::kNear);
  CmpP(reg, Operand(SYMBOL_TYPE));
  bne(not_unique_name);

  bind(&succeed);
}

// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result, Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* gc_required,
                                        MutableMode mode) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           NO_ALLOCATION_FLAGS);

  Heap::RootListIndex map_index = mode == MUTABLE
                                      ? Heap::kMutableHeapNumberMapRootIndex
                                      : Heap::kHeapNumberMapRootIndex;
  AssertIsRoot(heap_number_map, map_index);

  // Store heap number map in the allocated object.
    StoreP(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
}

void MacroAssembler::AllocateHeapNumberWithValue(
    Register result, DoubleRegister value, Register scratch1, Register scratch2,
    Register heap_number_map, Label* gc_required) {
  AllocateHeapNumber(result, scratch1, scratch2, heap_number_map, gc_required);
  StoreDouble(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}

void MacroAssembler::AllocateJSValue(Register result, Register constructor,
                                     Register value, Register scratch1,
                                     Register scratch2, Label* gc_required) {
  DCHECK(!result.is(constructor));
  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!result.is(value));

  // Allocate JSValue in new space.
  Allocate(JSValue::kSize, result, scratch1, scratch2, gc_required,
           NO_ALLOCATION_FLAGS);

  // Initialize the JSValue.
  LoadGlobalFunctionInitialMap(constructor, scratch1, scratch2);
  StoreP(scratch1, FieldMemOperand(result, HeapObject::kMapOffset), r0);
  LoadRoot(scratch1, Heap::kEmptyFixedArrayRootIndex);
  StoreP(scratch1, FieldMemOperand(result, JSObject::kPropertiesOffset), r0);
  StoreP(scratch1, FieldMemOperand(result, JSObject::kElementsOffset), r0);
  StoreP(value, FieldMemOperand(result, JSValue::kValueOffset), r0);
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
}

void MacroAssembler::InitializeNFieldsWithFiller(Register current_address,
                                                 Register count,
                                                 Register filler) {
  Label loop;
  bind(&loop);
  StoreP(filler, MemOperand(current_address));
  AddP(current_address, current_address, Operand(kPointerSize));
  BranchOnCount(r1, &loop);
}

void MacroAssembler::InitializeFieldsWithFiller(Register current_address,
                                                Register end_address,
                                                Register filler) {
  Label done;
  DCHECK(!filler.is(r1));
  DCHECK(!current_address.is(r1));
  DCHECK(!end_address.is(r1));
  SubP(r1, end_address, current_address /*, LeaveOE, SetRC*/);
  beq(&done, Label::kNear);
  ShiftRightP(r1, r1, Operand(kPointerSizeLog2));
  InitializeNFieldsWithFiller(current_address, r1, filler);
  bind(&done);
}

void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialOneByte(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  if (!scratch1.is(first)) LoadRR(scratch1, first);
  if (!scratch2.is(second)) LoadRR(scratch2, second);
  nilf(scratch1, Operand(kFlatOneByteStringMask));
  CmpP(scratch1, Operand(kFlatOneByteStringTag));
  bne(failure);
  nilf(scratch2, Operand(kFlatOneByteStringMask));
  CmpP(scratch2, Operand(kFlatOneByteStringTag));
  bne(failure);
}

static const int kRegisterPassedArguments = 5;

int MacroAssembler::CalculateStackPassedWords(int num_reg_arguments,
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

void MacroAssembler::EmitSeqStringSetCharCheck(Register string, Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  TestIfSmi(string);
  Check(ne, kNonObject, cr0);

  LoadP(ip, FieldMemOperand(string, HeapObject::kMapOffset));
  LoadlB(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

  AndP(ip, Operand(kStringRepresentationMask | kStringEncodingMask));
  CmpP(ip, Operand(encoding_mask));
  Check(eq, kUnexpectedStringType);

// The index is assumed to be untagged coming in, tag it to compare with the
// string length without using a temp register, it is restored at the end of
// this function.
#if !V8_TARGET_ARCH_S390X
  Label index_tag_ok, index_tag_bad;
  JumpIfNotSmiCandidate(index, r0, &index_tag_bad);
#endif
  SmiTag(index, index);
#if !V8_TARGET_ARCH_S390X
  b(&index_tag_ok);
  bind(&index_tag_bad);
  Abort(kIndexIsTooLarge);
  bind(&index_tag_ok);
#endif

  LoadP(ip, FieldMemOperand(string, String::kLengthOffset));
  CmpP(index, ip);
  Check(lt, kIndexIsTooLarge);

  DCHECK(Smi::kZero == 0);
  CmpP(index, Operand::Zero());
  Check(ge, kIndexIsNegative);

  SmiUntag(index, index);
}

void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
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
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
    StoreP(scratch, MemOperand(sp, (stack_passed_arguments)*kPointerSize));
  } else {
    stack_space += stack_passed_arguments;
  }
  lay(sp, MemOperand(sp, -(stack_space)*kPointerSize));
}

void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void MacroAssembler::MovToFloatParameter(DoubleRegister src) { Move(d0, src); }

void MacroAssembler::MovToFloatResult(DoubleRegister src) { Move(d0, src); }

void MacroAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2.is(d0)) {
    DCHECK(!src1.is(d2));
    Move(d2, src2);
    Move(d0, src1);
  } else {
    Move(d0, src1);
    Move(d2, src2);
  }
}

void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  mov(ip, Operand(function));
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments);
}

void MacroAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}

void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void MacroAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK(has_frame());

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Register dest = function;
  if (ABI_CALL_VIA_IP) {
    Move(ip, function);
    dest = ip;
  }

  Call(dest);

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

void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask, Condition cc, Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));

  if (base::bits::IsPowerOfTwo32(mask)) {
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

void MacroAssembler::JumpIfBlack(Register object, Register scratch0,
                                 Register scratch1, Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 1);  // kBlackBitPattern.
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
}

void MacroAssembler::HasColor(Register object, Register bitmap_scratch,
                              Register mask_scratch, Label* has_color,
                              int first_bit, int second_bit) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, no_reg));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  LoadlW(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Test the first bit
  AndP(r0, ip, mask_scratch /*, SetRC*/);  // Should be okay to remove rc
  b(first_bit == 1 ? eq : ne, &other_color, Label::kNear);
  // Shift left 1
  // May need to load the next cell
  sll(mask_scratch, Operand(1) /*, SetRC*/);
  LoadAndTest32(mask_scratch, mask_scratch);
  beq(&word_boundary, Label::kNear);
  // Test the second bit
  AndP(r0, ip, mask_scratch /*, SetRC*/);  // Should be okay to remove rc
  b(second_bit == 1 ? ne : eq, has_color);
  b(&other_color, Label::kNear);

  bind(&word_boundary);
  LoadlW(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize + kIntSize));
  AndP(r0, ip, Operand(1));
  b(second_bit == 1 ? ne : eq, has_color);
  bind(&other_color);
}

void MacroAssembler::GetMarkBits(Register addr_reg, Register bitmap_reg,
                                 Register mask_reg) {
  DCHECK(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  LoadRR(bitmap_reg, addr_reg);
  nilf(bitmap_reg, Operand(~Page::kPageAlignmentMask));
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  ExtractBitRange(mask_reg, addr_reg, kLowBits - 1, kPointerSizeLog2);
  ExtractBitRange(ip, addr_reg, kPageSizeBits - 1, kLowBits);
  ShiftLeftP(ip, ip, Operand(Bitmap::kBytesPerCellLog2));
  AddP(bitmap_reg, ip);
  LoadRR(ip, mask_reg);  // Have to do some funky reg shuffling as
                         // 31-bit shift left clobbers on s390.
  LoadImmP(mask_reg, Operand(1));
  ShiftLeftP(mask_reg, mask_reg, ip);
}

void MacroAssembler::JumpIfWhite(Register value, Register bitmap_scratch,
                                 Register mask_scratch, Register load_scratch,
                                 Label* value_is_white) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, ip));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  LoadlW(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  LoadRR(r0, load_scratch);
  AndP(r0, mask_scratch);
  beq(value_is_white);
}

// Saturate a value into 8-bit unsigned integer
//   if input_value < 0, output_value is 0
//   if input_value > 255, output_value is 255
//   otherwise output_value is the input_value
void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  int satval = (1 << 8) - 1;

  Label done, negative_label, overflow_label;
  CmpP(input_reg, Operand::Zero());
  blt(&negative_label);

  CmpP(input_reg, Operand(satval));
  bgt(&overflow_label);
  if (!output_reg.is(input_reg)) {
    LoadRR(output_reg, input_reg);
  }
  b(&done);

  bind(&negative_label);
  LoadImmP(output_reg, Operand::Zero());  // set to 0 if negative
  b(&done);

  bind(&overflow_label);  // set to satval if > satval
  LoadImmP(output_reg, Operand(satval));

  bind(&done);
}

void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DoubleRegister input_reg,
                                        DoubleRegister double_scratch) {
  Label above_zero;
  Label done;
  Label in_bounds;

  LoadDoubleLiteral(double_scratch, 0.0, result_reg);
  cdbr(input_reg, double_scratch);
  bgt(&above_zero, Label::kNear);

  // Double value is less than zero, NaN or Inf, return 0.
  LoadIntLiteral(result_reg, 0);
  b(&done, Label::kNear);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  LoadDoubleLiteral(double_scratch, 255.0, result_reg);
  cdbr(input_reg, double_scratch);
  ble(&in_bounds, Label::kNear);
  LoadIntLiteral(result_reg, 255);
  b(&done, Label::kNear);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);

  // round to nearest (default rounding mode)
  cfdbr(ROUND_TO_NEAREST_WITH_TIES_TO_EVEN, result_reg, input_reg);
  bind(&done);
}

void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  LoadP(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}

void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  LoadlW(dst, FieldMemOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}

void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  LoadW(dst, FieldMemOperand(map, Map::kBitField3Offset));
  And(dst, Operand(Map::EnumLengthBits::kMask));
  SmiTag(dst);
}

void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  LoadP(dst, FieldMemOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  LoadP(dst,
        FieldMemOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  const int getterOffset = AccessorPair::kGetterOffset;
  const int setterOffset = AccessorPair::kSetterOffset;
  int offset = ((accessor == ACCESSOR_GETTER) ? getterOffset : setterOffset);
  LoadP(dst, FieldMemOperand(dst, offset));
}

void MacroAssembler::CheckEnumCache(Label* call_runtime) {
  Register null_value = r7;
  Register empty_fixed_array_value = r8;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Label next, start;
  LoadRR(r4, r2);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  LoadP(r3, FieldMemOperand(r4, HeapObject::kMapOffset));

  EnumLength(r5, r3);
  CmpSmiLiteral(r5, Smi::FromInt(kInvalidEnumCacheSentinel), r0);
  beq(call_runtime);

  LoadRoot(null_value, Heap::kNullValueRootIndex);
  b(&start, Label::kNear);

  bind(&next);
  LoadP(r3, FieldMemOperand(r4, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(r5, r3);
  CmpSmiLiteral(r5, Smi::kZero, r0);
  bne(call_runtime);

  bind(&start);

  // Check that there are no elements. Register r4 contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  LoadP(r4, FieldMemOperand(r4, JSObject::kElementsOffset));
  CmpP(r4, empty_fixed_array_value);
  beq(&no_elements, Label::kNear);

  // Second chance, the object may be using the empty slow element dictionary.
  CompareRoot(r5, Heap::kEmptySlowElementDictionaryRootIndex);
  bne(call_runtime);

  bind(&no_elements);
  LoadP(r4, FieldMemOperand(r3, Map::kPrototypeOffset));
  CmpP(r4, null_value);
  bne(&next);
}

////////////////////////////////////////////////////////////////////////////////
//
// New MacroAssembler Interfaces added for S390
//
////////////////////////////////////////////////////////////////////////////////
// Primarily used for loading constants
// This should really move to be in macro-assembler as it
// is really a pseudo instruction
// Some usages of this intend for a FIXED_SEQUENCE to be used
// @TODO - break this dependency so we can optimize mov() in general
// and only use the generic version when we require a fixed sequence
void MacroAssembler::LoadRepresentation(Register dst, const MemOperand& mem,
                                        Representation r, Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    LoadB(dst, mem);
  } else if (r.IsUInteger8()) {
    LoadlB(dst, mem);
  } else if (r.IsInteger16()) {
    LoadHalfWordP(dst, mem, scratch);
  } else if (r.IsUInteger16()) {
    LoadHalfWordP(dst, mem, scratch);
#if V8_TARGET_ARCH_S390X
  } else if (r.IsInteger32()) {
    LoadW(dst, mem, scratch);
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
#if V8_TARGET_ARCH_S390X
  } else if (r.IsInteger32()) {
    StoreW(src, mem, scratch);
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

void MacroAssembler::TestJSArrayForAllocationMemento(Register receiver_reg,
                                                     Register scratch_reg,
                                                     Register scratch2_reg,
                                                     Label* no_memento_found) {
  Label map_check;
  Label top_check;
  ExternalReference new_space_allocation_top_adr =
      ExternalReference::new_space_allocation_top_address(isolate());
  const int kMementoMapOffset = JSArray::kSize - kHeapObjectTag;
  const int kMementoLastWordOffset =
      kMementoMapOffset + AllocationMemento::kSize - kPointerSize;

  DCHECK(!AreAliased(receiver_reg, scratch_reg));

  // Bail out if the object is not in new space.
  JumpIfNotInNewSpace(receiver_reg, scratch_reg, no_memento_found);

  DCHECK((~Page::kPageAlignmentMask & 0xffff) == 0);

  // If the object is in new space, we need to check whether it is on the same
  // page as the current top.
  AddP(scratch_reg, receiver_reg, Operand(kMementoLastWordOffset));
  mov(ip, Operand(new_space_allocation_top_adr));
  LoadP(ip, MemOperand(ip));
  XorP(r0, scratch_reg, ip);
  AndP(r0, r0, Operand(~Page::kPageAlignmentMask));
  beq(&top_check, Label::kNear);
  // The object is on a different page than allocation top. Bail out if the
  // object sits on the page boundary as no memento can follow and we cannot
  // touch the memory following it.
  XorP(r0, scratch_reg, receiver_reg);
  AndP(r0, r0, Operand(~Page::kPageAlignmentMask));
  bne(no_memento_found);
  // Continue with the actual map check.
  b(&map_check, Label::kNear);
  // If top is on the same page as the current object, we need to check whether
  // we are below top.
  bind(&top_check);
  CmpP(scratch_reg, ip);
  bge(no_memento_found);
  // Memento map check.
  bind(&map_check);
  LoadP(scratch_reg, MemOperand(receiver_reg, kMementoMapOffset));
  CmpP(scratch_reg, Operand(isolate()->factory()->allocation_memento_map()));
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

  const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
  return no_reg;
}

void MacroAssembler::mov(Register dst, const Operand& src) {
  if (src.rmode_ != kRelocInfo_NONEPTR) {
    // some form of relocation needed
    RecordRelocInfo(src.rmode_, src.imm_);
  }

#if V8_TARGET_ARCH_S390X
  int64_t value = src.immediate();
  int32_t hi_32 = static_cast<int64_t>(value) >> 32;
  int32_t lo_32 = static_cast<int32_t>(value);

  iihf(dst, Operand(hi_32));
  iilf(dst, Operand(lo_32));
#else
  int value = src.immediate();
  iilf(dst, Operand(value));
#endif
}

void MacroAssembler::Mul32(Register dst, const MemOperand& src1) {
  if (is_uint12(src1.offset())) {
    ms(dst, src1);
  } else if (is_int20(src1.offset())) {
    msy(dst, src1);
  } else {
    UNIMPLEMENTED();
  }
}

void MacroAssembler::Mul32(Register dst, Register src1) { msr(dst, src1); }

void MacroAssembler::Mul32(Register dst, const Operand& src1) {
  msfi(dst, src1);
}

void MacroAssembler::Mul64(Register dst, const MemOperand& src1) {
  if (is_int20(src1.offset())) {
    msg(dst, src1);
  } else {
    UNIMPLEMENTED();
  }
}

void MacroAssembler::Mul64(Register dst, Register src1) { msgr(dst, src1); }

void MacroAssembler::Mul64(Register dst, const Operand& src1) {
  msgfi(dst, src1);
}

void MacroAssembler::Mul(Register dst, Register src1, Register src2) {
  if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
    MulPWithCondition(dst, src1, src2);
  } else {
    if (dst.is(src2)) {
      MulP(dst, src1);
    } else if (dst.is(src1)) {
      MulP(dst, src2);
    } else {
      Move(dst, src1);
      MulP(dst, src2);
    }
  }
}

void MacroAssembler::DivP(Register dividend, Register divider) {
  // have to make sure the src and dst are reg pairs
  DCHECK(dividend.code() % 2 == 0);
#if V8_TARGET_ARCH_S390X
  dsgr(dividend, divider);
#else
  dr(dividend, divider);
#endif
}

void MacroAssembler::MulP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  msgfi(dst, opnd);
#else
  msfi(dst, opnd);
#endif
}

void MacroAssembler::MulP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  msgr(dst, src);
#else
  msr(dst, src);
#endif
}

void MacroAssembler::MulPWithCondition(Register dst, Register src1,
                                       Register src2) {
  CHECK(CpuFeatures::IsSupported(MISC_INSTR_EXT2));
#if V8_TARGET_ARCH_S390X
  msgrkc(dst, src1, src2);
#else
  msrkc(dst, src1, src2);
#endif
}

void MacroAssembler::MulP(Register dst, const MemOperand& opnd) {
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

void MacroAssembler::Sqrt(DoubleRegister result, DoubleRegister input) {
  sqdbr(result, input);
}
void MacroAssembler::Sqrt(DoubleRegister result, const MemOperand& input) {
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
void MacroAssembler::Add32(Register dst, const Operand& opnd) {
  if (is_int16(opnd.immediate()))
    ahi(dst, opnd);
  else
    afi(dst, opnd);
}

// Add Pointer Size (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddP(Register dst, const Operand& opnd) {
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
void MacroAssembler::Add32(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      ahik(dst, src, opnd);
      return;
    }
    lr(dst, src);
  }
  Add32(dst, opnd);
}

// Add Pointer Size (Register dst = Register src + Immediate opnd)
void MacroAssembler::AddP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      AddPImm_RRI(dst, src, opnd);
      return;
    }
    LoadRR(dst, src);
  }
  AddP(dst, opnd);
}

// Add 32-bit (Register dst = Register dst + Register src)
void MacroAssembler::Add32(Register dst, Register src) { ar(dst, src); }

// Add Pointer Size (Register dst = Register dst + Register src)
void MacroAssembler::AddP(Register dst, Register src) { AddRR(dst, src); }

// Add Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) + Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::AddP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  agfr(dst, src);
#else
  ar(dst, src);
#endif
}

// Add 32-bit (Register dst = Register src1 + Register src2)
void MacroAssembler::Add32(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ark(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  ar(dst, src2);
}

// Add Pointer Size (Register dst = Register src1 + Register src2)
void MacroAssembler::AddP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AddP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  AddRR(dst, src2);
}

// Add Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) + Register src1 (ptr) +
//                            Register src2 (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::AddP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (dst.is(src2)) {
    // The source we need to sign extend is the same as result.
    lgfr(dst, src2);
    agr(dst, src1);
  } else {
    if (!dst.is(src1)) LoadRR(dst, src1);
    agfr(dst, src2);
  }
#else
  AddP(dst, src1, src2);
#endif
}

// Add 32-bit (Register-Memory)
void MacroAssembler::Add32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    a(dst, opnd);
  else
    ay(dst, opnd);
}

// Add Pointer Size (Register-Memory)
void MacroAssembler::AddP(Register dst, const MemOperand& opnd) {
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
void MacroAssembler::AddP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  agf(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}

// Add 32-bit (Memory - Immediate)
void MacroAssembler::Add32(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  DCHECK(CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
  asi(opnd, imm);
}

// Add Pointer-sized (Memory - Immediate)
void MacroAssembler::AddP(const MemOperand& opnd, const Operand& imm) {
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
void MacroAssembler::AddLogicalWithCarry32(Register dst, Register src1,
                                           Register src2) {
  if (!dst.is(src2) && !dst.is(src1)) {
    lr(dst, src1);
    alcr(dst, src2);
  } else if (!dst.is(src2)) {
    // dst == src1
    DCHECK(dst.is(src1));
    alcr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst.is(src2));
    alcr(dst, src1);
  }
}

// Add Logical 32-bit (Register dst = Register src1 + Register src2)
void MacroAssembler::AddLogical32(Register dst, Register src1, Register src2) {
  if (!dst.is(src2) && !dst.is(src1)) {
    lr(dst, src1);
    alr(dst, src2);
  } else if (!dst.is(src2)) {
    // dst == src1
    DCHECK(dst.is(src1));
    alr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst.is(src2));
    alr(dst, src1);
  }
}

// Add Logical 32-bit (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddLogical(Register dst, const Operand& imm) {
  alfi(dst, imm);
}

// Add Logical Pointer Size (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddLogicalP(Register dst, const Operand& imm) {
#ifdef V8_TARGET_ARCH_S390X
  algfi(dst, imm);
#else
  AddLogical(dst, imm);
#endif
}

// Add Logical 32-bit (Register-Memory)
void MacroAssembler::AddLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    al_z(dst, opnd);
  else
    aly(dst, opnd);
}

// Add Logical Pointer Size (Register-Memory)
void MacroAssembler::AddLogicalP(Register dst, const MemOperand& opnd) {
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
void MacroAssembler::SubLogicalWithBorrow32(Register dst, Register src1,
                                            Register src2) {
  if (!dst.is(src2) && !dst.is(src1)) {
    lr(dst, src1);
    slbr(dst, src2);
  } else if (!dst.is(src2)) {
    // dst == src1
    DCHECK(dst.is(src1));
    slbr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst.is(src2));
    lr(r0, dst);
    SubLogicalWithBorrow32(dst, src1, r0);
  }
}

// Subtract Logical 32-bit (Register dst = Register src1 - Register src2)
void MacroAssembler::SubLogical32(Register dst, Register src1, Register src2) {
  if (!dst.is(src2) && !dst.is(src1)) {
    lr(dst, src1);
    slr(dst, src2);
  } else if (!dst.is(src2)) {
    // dst == src1
    DCHECK(dst.is(src1));
    slr(dst, src2);
  } else {
    // dst == src2
    DCHECK(dst.is(src2));
    lr(r0, dst);
    SubLogical32(dst, src1, r0);
  }
}

// Subtract 32-bit (Register dst = Register dst - Immediate opnd)
void MacroAssembler::Sub32(Register dst, const Operand& imm) {
  Add32(dst, Operand(-(imm.imm_)));
}

// Subtract Pointer Size (Register dst = Register dst - Immediate opnd)
void MacroAssembler::SubP(Register dst, const Operand& imm) {
  AddP(dst, Operand(-(imm.imm_)));
}

// Subtract 32-bit (Register dst = Register src - Immediate opnd)
void MacroAssembler::Sub32(Register dst, Register src, const Operand& imm) {
  Add32(dst, src, Operand(-(imm.imm_)));
}

// Subtract Pointer Sized (Register dst = Register src - Immediate opnd)
void MacroAssembler::SubP(Register dst, Register src, const Operand& imm) {
  AddP(dst, src, Operand(-(imm.imm_)));
}

// Subtract 32-bit (Register dst = Register dst - Register src)
void MacroAssembler::Sub32(Register dst, Register src) { sr(dst, src); }

// Subtract Pointer Size (Register dst = Register dst - Register src)
void MacroAssembler::SubP(Register dst, Register src) { SubRR(dst, src); }

// Subtract Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) - Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::SubP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  sgfr(dst, src);
#else
  sr(dst, src);
#endif
}

// Subtract 32-bit (Register = Register - Register)
void MacroAssembler::Sub32(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srk(dst, src1, src2);
    return;
  }
  if (!dst.is(src1) && !dst.is(src2)) lr(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
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
void MacroAssembler::SubP(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    SubP_RRR(dst, src1, src2);
    return;
  }
  if (!dst.is(src1) && !dst.is(src2)) LoadRR(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
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
void MacroAssembler::SubP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (!dst.is(src1) && !dst.is(src2)) LoadRR(dst, src1);

  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
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
void MacroAssembler::Sub32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    s(dst, opnd);
  else
    sy(dst, opnd);
}

// Subtract Pointer Sized (Register - Memory)
void MacroAssembler::SubP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  sg(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}

void MacroAssembler::MovIntToFloat(DoubleRegister dst, Register src) {
  sllg(r0, src, Operand(32));
  ldgr(dst, r0);
}

void MacroAssembler::MovFloatToInt(Register dst, DoubleRegister src) {
  lgdr(dst, src);
  srlg(dst, dst, Operand(32));
}

void MacroAssembler::SubP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  sgf(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}

//----------------------------------------------------------------------------
//  Subtract Logical Instructions
//----------------------------------------------------------------------------

// Subtract Logical 32-bit (Register - Memory)
void MacroAssembler::SubLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    sl(dst, opnd);
  else
    sly(dst, opnd);
}

// Subtract Logical Pointer Sized (Register - Memory)
void MacroAssembler::SubLogicalP(Register dst, const MemOperand& opnd) {
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
void MacroAssembler::SubLogicalP_ExtendSrc(Register dst,
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
void MacroAssembler::And(Register dst, Register src) { nr(dst, src); }

// AND Pointer Size - dst = dst & src
void MacroAssembler::AndP(Register dst, Register src) { AndRR(dst, src); }

// Non-clobbering AND 32-bit - dst = src1 & src1
void MacroAssembler::And(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      nrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  And(dst, src2);
}

// Non-clobbering AND pointer size - dst = src1 & src1
void MacroAssembler::AndP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AndP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  AndP(dst, src2);
}

// AND 32-bit (Reg - Mem)
void MacroAssembler::And(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    n(dst, opnd);
  else
    ny(dst, opnd);
}

// AND Pointer Size (Reg - Mem)
void MacroAssembler::AndP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  ng(dst, opnd);
#else
  And(dst, opnd);
#endif
}

// AND 32-bit - dst = dst & imm
void MacroAssembler::And(Register dst, const Operand& opnd) { nilf(dst, opnd); }

// AND Pointer Size - dst = dst & imm
void MacroAssembler::AndP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
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
void MacroAssembler::And(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) lr(dst, src);
  nilf(dst, opnd);
}

// AND Pointer Size - dst = src & imm
void MacroAssembler::AndP(Register dst, Register src, const Operand& opnd) {
  // Try to exploit RISBG first
  intptr_t value = opnd.imm_;
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
    if ((0 != shifted_value) && base::bits::IsPowerOfTwo64(shifted_value + 1)) {
      int startBit =
          base::bits::CountLeadingZeros64(shifted_value) - trailing_zeros;
      int endBit = 63 - trailing_zeros;
      // Start: startBit, End: endBit, Shift = 0, true = zero unselected bits.
      risbg(dst, src, Operand(startBit), Operand(endBit), Operand::Zero(),
            true);
      return;
    } else if (-1 == shifted_value) {
      // A Special case in which all top bits up to MSB are 1's.  In this case,
      // we can set startBit to be 0.
      int endBit = 63 - trailing_zeros;
      risbg(dst, src, Operand::Zero(), Operand(endBit), Operand::Zero(), true);
      return;
    }
  }

  // If we are &'ing zero, we can just whack the dst register and skip copy
  if (!dst.is(src) && (0 != value)) LoadRR(dst, src);
  AndP(dst, opnd);
}

// OR 32-bit - dst = dst & src
void MacroAssembler::Or(Register dst, Register src) { or_z(dst, src); }

// OR Pointer Size - dst = dst & src
void MacroAssembler::OrP(Register dst, Register src) { OrRR(dst, src); }

// Non-clobbering OR 32-bit - dst = src1 & src1
void MacroAssembler::Or(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ork(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  Or(dst, src2);
}

// Non-clobbering OR pointer size - dst = src1 & src1
void MacroAssembler::OrP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      OrP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  OrP(dst, src2);
}

// OR 32-bit (Reg - Mem)
void MacroAssembler::Or(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    o(dst, opnd);
  else
    oy(dst, opnd);
}

// OR Pointer Size (Reg - Mem)
void MacroAssembler::OrP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  og(dst, opnd);
#else
  Or(dst, opnd);
#endif
}

// OR 32-bit - dst = dst & imm
void MacroAssembler::Or(Register dst, const Operand& opnd) { oilf(dst, opnd); }

// OR Pointer Size - dst = dst & imm
void MacroAssembler::OrP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
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
void MacroAssembler::Or(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) lr(dst, src);
  oilf(dst, opnd);
}

// OR Pointer Size - dst = src & imm
void MacroAssembler::OrP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) LoadRR(dst, src);
  OrP(dst, opnd);
}

// XOR 32-bit - dst = dst & src
void MacroAssembler::Xor(Register dst, Register src) { xr(dst, src); }

// XOR Pointer Size - dst = dst & src
void MacroAssembler::XorP(Register dst, Register src) { XorRR(dst, src); }

// Non-clobbering XOR 32-bit - dst = src1 & src1
void MacroAssembler::Xor(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      xrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  Xor(dst, src2);
}

// Non-clobbering XOR pointer size - dst = src1 & src1
void MacroAssembler::XorP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      XorP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  XorP(dst, src2);
}

// XOR 32-bit (Reg - Mem)
void MacroAssembler::Xor(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    x(dst, opnd);
  else
    xy(dst, opnd);
}

// XOR Pointer Size (Reg - Mem)
void MacroAssembler::XorP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  xg(dst, opnd);
#else
  Xor(dst, opnd);
#endif
}

// XOR 32-bit - dst = dst & imm
void MacroAssembler::Xor(Register dst, const Operand& opnd) { xilf(dst, opnd); }

// XOR Pointer Size - dst = dst & imm
void MacroAssembler::XorP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
  xihf(dst, Operand(value >> 32));
  xilf(dst, Operand(value & 0xFFFFFFFF));
#else
  Xor(dst, opnd);
#endif
}

// XOR 32-bit - dst = src & imm
void MacroAssembler::Xor(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) lr(dst, src);
  xilf(dst, opnd);
}

// XOR Pointer Size - dst = src & imm
void MacroAssembler::XorP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) LoadRR(dst, src);
  XorP(dst, opnd);
}

void MacroAssembler::Not32(Register dst, Register src) {
  if (!src.is(no_reg) && !src.is(dst)) lr(dst, src);
  xilf(dst, Operand(0xFFFFFFFF));
}

void MacroAssembler::Not64(Register dst, Register src) {
  if (!src.is(no_reg) && !src.is(dst)) lgr(dst, src);
  xihf(dst, Operand(0xFFFFFFFF));
  xilf(dst, Operand(0xFFFFFFFF));
}

void MacroAssembler::NotP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  Not64(dst, src);
#else
  Not32(dst, src);
#endif
}

// works the same as mov
void MacroAssembler::Load(Register dst, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value)) {
#if V8_TARGET_ARCH_S390X
    lghi(dst, opnd);
#else
    lhi(dst, opnd);
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lgfi(dst, opnd);
#else
    iilf(dst, opnd);
#endif
  }
}

void MacroAssembler::Load(Register dst, const MemOperand& opnd) {
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

void MacroAssembler::LoadPositiveP(Register result, Register input) {
#if V8_TARGET_ARCH_S390X
  lpgr(result, input);
#else
  lpr(result, input);
#endif
}

void MacroAssembler::LoadPositive32(Register result, Register input) {
  lpr(result, input);
  lgfr(result, result);
}

//-----------------------------------------------------------------------------
//  Compare Helpers
//-----------------------------------------------------------------------------

// Compare 32-bit Register vs Register
void MacroAssembler::Cmp32(Register src1, Register src2) { cr_z(src1, src2); }

// Compare Pointer Sized Register vs Register
void MacroAssembler::CmpP(Register src1, Register src2) {
#if V8_TARGET_ARCH_S390X
  cgr(src1, src2);
#else
  Cmp32(src1, src2);
#endif
}

// Compare 32-bit Register vs Immediate
// This helper will set up proper relocation entries if required.
void MacroAssembler::Cmp32(Register dst, const Operand& opnd) {
  if (opnd.rmode_ == kRelocInfo_NONEPTR) {
    intptr_t value = opnd.immediate();
    if (is_int16(value))
      chi(dst, opnd);
    else
      cfi(dst, opnd);
  } else {
    // Need to generate relocation record here
    RecordRelocInfo(opnd.rmode_, opnd.imm_);
    cfi(dst, opnd);
  }
}

// Compare Pointer Sized  Register vs Immediate
// This helper will set up proper relocation entries if required.
void MacroAssembler::CmpP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (opnd.rmode_ == kRelocInfo_NONEPTR) {
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
void MacroAssembler::Cmp32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    c(dst, opnd);
  else
    cy(dst, opnd);
}

// Compare Pointer Size Register vs Memory
void MacroAssembler::CmpP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  cg(dst, opnd);
#else
  Cmp32(dst, opnd);
#endif
}

//-----------------------------------------------------------------------------
// Compare Logical Helpers
//-----------------------------------------------------------------------------

// Compare Logical 32-bit Register vs Register
void MacroAssembler::CmpLogical32(Register dst, Register src) { clr(dst, src); }

// Compare Logical Pointer Sized Register vs Register
void MacroAssembler::CmpLogicalP(Register dst, Register src) {
#ifdef V8_TARGET_ARCH_S390X
  clgr(dst, src);
#else
  CmpLogical32(dst, src);
#endif
}

// Compare Logical 32-bit Register vs Immediate
void MacroAssembler::CmpLogical32(Register dst, const Operand& opnd) {
  clfi(dst, opnd);
}

// Compare Logical Pointer Sized Register vs Immediate
void MacroAssembler::CmpLogicalP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(static_cast<uint32_t>(opnd.immediate() >> 32) == 0);
  clgfi(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}

// Compare Logical 32-bit Register vs Memory
void MacroAssembler::CmpLogical32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    cl(dst, opnd);
  else
    cly(dst, opnd);
}

// Compare Logical Pointer Sized Register vs Memory
void MacroAssembler::CmpLogicalP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  clg(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}

// Compare Logical Byte (Mem - Imm)
void MacroAssembler::CmpLogicalByte(const MemOperand& mem, const Operand& imm) {
  DCHECK(is_uint8(imm.immediate()));
  if (is_uint12(mem.offset()))
    cli(mem, imm);
  else
    cliy(mem, imm);
}

void MacroAssembler::Branch(Condition c, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value))
    brc(c, opnd);
  else
    brcl(c, opnd);
}

// Branch On Count.  Decrement R1, and branch if R1 != 0.
void MacroAssembler::BranchOnCount(Register r1, Label* l) {
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

void MacroAssembler::LoadIntLiteral(Register dst, int value) {
  Load(dst, Operand(value));
}

void MacroAssembler::LoadSmiLiteral(Register dst, Smi* smi) {
  intptr_t value = reinterpret_cast<intptr_t>(smi);
#if V8_TARGET_ARCH_S390X
  DCHECK((value & 0xffffffff) == 0);
  // The smi value is loaded in upper 32-bits.  Lower 32-bit are zeros.
  llihf(dst, Operand(value >> 32));
#else
  llilf(dst, Operand(value));
#endif
}

void MacroAssembler::LoadDoubleLiteral(DoubleRegister result, uint64_t value,
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

void MacroAssembler::LoadDoubleLiteral(DoubleRegister result, double value,
                                       Register scratch) {
  uint64_t int_val = bit_cast<uint64_t, double>(value);
  LoadDoubleLiteral(result, int_val, scratch);
}

void MacroAssembler::LoadFloat32Literal(DoubleRegister result, float value,
                                        Register scratch) {
  uint64_t int_val = static_cast<uint64_t>(bit_cast<uint32_t, float>(value))
                     << 32;
  LoadDoubleLiteral(result, int_val, scratch);
}

void MacroAssembler::CmpSmiLiteral(Register src1, Smi* smi, Register scratch) {
#if V8_TARGET_ARCH_S390X
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    cih(src1, Operand(reinterpret_cast<intptr_t>(smi) >> 32));
  } else {
    LoadSmiLiteral(scratch, smi);
    cgr(src1, scratch);
  }
#else
  // CFI takes 32-bit immediate.
  cfi(src1, Operand(smi));
#endif
}

void MacroAssembler::CmpLogicalSmiLiteral(Register src1, Smi* smi,
                                          Register scratch) {
#if V8_TARGET_ARCH_S390X
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    clih(src1, Operand(reinterpret_cast<intptr_t>(smi) >> 32));
  } else {
    LoadSmiLiteral(scratch, smi);
    clgr(src1, scratch);
  }
#else
  // CLFI takes 32-bit immediate
  clfi(src1, Operand(smi));
#endif
}

void MacroAssembler::AddSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_S390X
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    if (!dst.is(src)) LoadRR(dst, src);
    aih(dst, Operand(reinterpret_cast<intptr_t>(smi) >> 32));
  } else {
    LoadSmiLiteral(scratch, smi);
    AddP(dst, src, scratch);
  }
#else
  AddP(dst, src, Operand(reinterpret_cast<intptr_t>(smi)));
#endif
}

void MacroAssembler::SubSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_S390X
  if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    if (!dst.is(src)) LoadRR(dst, src);
    aih(dst, Operand((-reinterpret_cast<intptr_t>(smi)) >> 32));
  } else {
    LoadSmiLiteral(scratch, smi);
    SubP(dst, src, scratch);
  }
#else
  AddP(dst, src, Operand(-(reinterpret_cast<intptr_t>(smi))));
#endif
}

void MacroAssembler::AndSmiLiteral(Register dst, Register src, Smi* smi) {
  if (!dst.is(src)) LoadRR(dst, src);
#if V8_TARGET_ARCH_S390X
  DCHECK((reinterpret_cast<intptr_t>(smi) & 0xffffffff) == 0);
  int value = static_cast<int>(reinterpret_cast<intptr_t>(smi) >> 32);
  nihf(dst, Operand(value));
#else
  nilf(dst, Operand(reinterpret_cast<int>(smi)));
#endif
}

// Load a "pointer" sized value from the memory location
void MacroAssembler::LoadP(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!scratch.is(no_reg) && !is_int20(offset)) {
    /* cannot use d-form */
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lg(dst, MemOperand(mem.rb(), scratch));
#else
    l(dst, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lg(dst, mem);
#else
    if (is_uint12(offset)) {
      l(dst, mem);
    } else {
      ly(dst, mem);
    }
#endif
  }
}

// Store a "pointer" sized value to the memory location
void MacroAssembler::StoreP(Register src, const MemOperand& mem,
                            Register scratch) {
  if (!is_int20(mem.offset())) {
    DCHECK(!scratch.is(no_reg));
    DCHECK(!scratch.is(r0));
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
void MacroAssembler::StoreP(const MemOperand& mem, const Operand& opnd,
                            Register scratch) {
  // Relocations not supported
  DCHECK(opnd.rmode_ == kRelocInfo_NONEPTR);

  // Try to use MVGHI/MVHI
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) && is_uint12(mem.offset()) &&
      mem.getIndexRegister().is(r0) && is_int16(opnd.imm_)) {
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

void MacroAssembler::LoadMultipleP(Register dst1, Register dst2,
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

void MacroAssembler::StoreMultipleP(Register src1, Register src2,
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

void MacroAssembler::LoadMultipleW(Register dst1, Register dst2,
                                   const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    lm(dst1, dst2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    lmy(dst1, dst2, mem);
  }
}

void MacroAssembler::StoreMultipleW(Register src1, Register src2,
                                    const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    stm(src1, src2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    stmy(src1, src2, mem);
  }
}

// Load 32-bits and sign extend if necessary.
void MacroAssembler::LoadW(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  lgfr(dst, src);
#else
  if (!dst.is(src)) lr(dst, src);
#endif
}

// Load 32-bits and sign extend if necessary.
void MacroAssembler::LoadW(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(!scratch.is(no_reg));
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
void MacroAssembler::LoadlW(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  llgfr(dst, src);
#else
  if (!dst.is(src)) lr(dst, src);
#endif
}

// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void MacroAssembler::LoadlW(Register dst, const MemOperand& mem,
                            Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

#if V8_TARGET_ARCH_S390X
  if (is_int20(offset)) {
    llgf(dst, mem);
  } else if (!scratch.is(no_reg)) {
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
  } else if (!scratch.is(no_reg)) {
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

void MacroAssembler::LoadLogicalHalfWordP(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  llgh(dst, mem);
#else
  llh(dst, mem);
#endif
}

void MacroAssembler::LoadLogicalHalfWordP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  llghr(dst, src);
#else
  llhr(dst, src);
#endif
}

void MacroAssembler::LoadB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  lgb(dst, mem);
#else
  lb(dst, mem);
#endif
}

void MacroAssembler::LoadB(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  lgbr(dst, src);
#else
  lbr(dst, src);
#endif
}

void MacroAssembler::LoadlB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  llgc(dst, mem);
#else
  llc(dst, mem);
#endif
}

void MacroAssembler::LoadLogicalReversedWordP(Register dst,
                                              const MemOperand& mem) {
  lrv(dst, mem);
  LoadlW(dst, dst);
}


void MacroAssembler::LoadLogicalReversedHalfWordP(Register dst,
                                              const MemOperand& mem) {
  lrvh(dst, mem);
  LoadLogicalHalfWordP(dst, dst);
}


// Load And Test (Reg <- Reg)
void MacroAssembler::LoadAndTest32(Register dst, Register src) {
  ltr(dst, src);
}

// Load And Test
//     (Register dst(ptr) = Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::LoadAndTestP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgfr(dst, src);
#else
  ltr(dst, src);
#endif
}

// Load And Test Pointer Sized (Reg <- Reg)
void MacroAssembler::LoadAndTestP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgr(dst, src);
#else
  ltr(dst, src);
#endif
}

// Load And Test 32-bit (Reg <- Mem)
void MacroAssembler::LoadAndTest32(Register dst, const MemOperand& mem) {
  lt_z(dst, mem);
}

// Load And Test Pointer Sized (Reg <- Mem)
void MacroAssembler::LoadAndTestP(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  ltg(dst, mem);
#else
  lt_z(dst, mem);
#endif
}

// Load On Condition Pointer Sized (Reg <- Reg)
void MacroAssembler::LoadOnConditionP(Condition cond, Register dst,
                                      Register src) {
#if V8_TARGET_ARCH_S390X
  locgr(cond, dst, src);
#else
  locr(cond, dst, src);
#endif
}

// Load Double Precision (64-bit) Floating Point number from memory
void MacroAssembler::LoadDouble(DoubleRegister dst, const MemOperand& mem) {
  // for 32bit and 64bit we all use 64bit floating point regs
  if (is_uint12(mem.offset())) {
    ld(dst, mem);
  } else {
    ldy(dst, mem);
  }
}

// Load Single Precision (32-bit) Floating Point number from memory
void MacroAssembler::LoadFloat32(DoubleRegister dst, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    le_z(dst, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    ley(dst, mem);
  }
}

// Load Single Precision (32-bit) Floating Point number from memory,
// and convert to Double Precision (64-bit)
void MacroAssembler::LoadFloat32ConvertToDouble(DoubleRegister dst,
                                                const MemOperand& mem) {
  LoadFloat32(dst, mem);
  ldebr(dst, dst);
}

// Store Double Precision (64-bit) Floating Point number to memory
void MacroAssembler::StoreDouble(DoubleRegister dst, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    std(dst, mem);
  } else {
    stdy(dst, mem);
  }
}

// Store Single Precision (32-bit) Floating Point number to memory
void MacroAssembler::StoreFloat32(DoubleRegister src, const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    ste(src, mem);
  } else {
    stey(src, mem);
  }
}

// Convert Double precision (64-bit) to Single Precision (32-bit)
// and store resulting Float32 to memory
void MacroAssembler::StoreDoubleAsFloat32(DoubleRegister src,
                                          const MemOperand& mem,
                                          DoubleRegister scratch) {
  ledbr(scratch, src);
  StoreFloat32(scratch, mem);
}

// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void MacroAssembler::StoreW(Register src, const MemOperand& mem,
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
  } else if (!scratch.is(no_reg)) {
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

// Loads 16-bits half-word value from memory and sign extends to pointer
// sized register
void MacroAssembler::LoadHalfWordP(Register dst, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(!scratch.is(no_reg));
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
void MacroAssembler::StoreHalfWord(Register src, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    sth(src, mem);
  } else if (is_int20(offset)) {
    sthy(src, mem);
  } else {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
    sth(src, MemOperand(base, scratch));
  }
}

// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreByte(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    stc(src, mem);
  } else if (is_int20(offset)) {
    stcy(src, mem);
  } else {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
    stc(src, MemOperand(base, scratch));
  }
}

// Shift left logical for 32-bit integer types.
void MacroAssembler::ShiftLeft(Register dst, Register src, const Operand& val) {
  if (dst.is(src)) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    lr(dst, src);
    sll(dst, val);
  }
}

// Shift left logical for 32-bit integer types.
void MacroAssembler::ShiftLeft(Register dst, Register src, Register val) {
  if (dst.is(src)) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    DCHECK(!dst.is(val));  // The lr/sll path clobbers val.
    lr(dst, src);
    sll(dst, val);
  }
}

// Shift right logical for 32-bit integer types.
void MacroAssembler::ShiftRight(Register dst, Register src,
                                const Operand& val) {
  if (dst.is(src)) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    lr(dst, src);
    srl(dst, val);
  }
}

// Shift right logical for 32-bit integer types.
void MacroAssembler::ShiftRight(Register dst, Register src, Register val) {
  if (dst.is(src)) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    DCHECK(!dst.is(val));  // The lr/srl path clobbers val.
    lr(dst, src);
    srl(dst, val);
  }
}

// Shift left arithmetic for 32-bit integer types.
void MacroAssembler::ShiftLeftArith(Register dst, Register src,
                                    const Operand& val) {
  if (dst.is(src)) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    lr(dst, src);
    sla(dst, val);
  }
}

// Shift left arithmetic for 32-bit integer types.
void MacroAssembler::ShiftLeftArith(Register dst, Register src, Register val) {
  if (dst.is(src)) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    DCHECK(!dst.is(val));  // The lr/sla path clobbers val.
    lr(dst, src);
    sla(dst, val);
  }
}

// Shift right arithmetic for 32-bit integer types.
void MacroAssembler::ShiftRightArith(Register dst, Register src,
                                     const Operand& val) {
  if (dst.is(src)) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    lr(dst, src);
    sra(dst, val);
  }
}

// Shift right arithmetic for 32-bit integer types.
void MacroAssembler::ShiftRightArith(Register dst, Register src, Register val) {
  if (dst.is(src)) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    DCHECK(!dst.is(val));  // The lr/sra path clobbers val.
    lr(dst, src);
    sra(dst, val);
  }
}

// Clear right most # of bits
void MacroAssembler::ClearRightImm(Register dst, Register src,
                                   const Operand& val) {
  int numBitsToClear = val.imm_ % (kPointerSize * 8);

  // Try to use RISBG if possible
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    int endBit = 63 - numBitsToClear;
    risbg(dst, src, Operand::Zero(), Operand(endBit), Operand::Zero(), true);
    return;
  }

  uint64_t hexMask = ~((1L << numBitsToClear) - 1);

  // S390 AND instr clobbers source.  Make a copy if necessary
  if (!dst.is(src)) LoadRR(dst, src);

  if (numBitsToClear <= 16) {
    nill(dst, Operand(static_cast<uint16_t>(hexMask)));
  } else if (numBitsToClear <= 32) {
    nilf(dst, Operand(static_cast<uint32_t>(hexMask)));
  } else if (numBitsToClear <= 64) {
    nilf(dst, Operand(static_cast<intptr_t>(0)));
    nihf(dst, Operand(hexMask >> 32));
  }
}

void MacroAssembler::Popcnt32(Register dst, Register src) {
  DCHECK(!src.is(r0));
  DCHECK(!dst.is(r0));

  popcnt(dst, src);
  ShiftRight(r0, dst, Operand(16));
  ar(dst, r0);
  ShiftRight(r0, dst, Operand(8));
  ar(dst, r0);
  LoadB(dst, dst);
}

#ifdef V8_TARGET_ARCH_S390X
void MacroAssembler::Popcnt64(Register dst, Register src) {
  DCHECK(!src.is(r0));
  DCHECK(!dst.is(r0));

  popcnt(dst, src);
  ShiftRightP(r0, dst, Operand(32));
  AddP(dst, r0);
  ShiftRightP(r0, dst, Operand(16));
  AddP(dst, r0);
  ShiftRightP(r0, dst, Operand(8));
  AddP(dst, r0);
  LoadB(dst, dst);
}
#endif

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
#endif

CodePatcher::CodePatcher(Isolate* isolate, byte* address, int size,
                         FlushICache flush_cache)
    : address_(address),
      size_(size),
      masm_(isolate, address, size_ + Assembler::kGap, CodeObjectRequired::kNo),
      flush_cache_(flush_cache) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}

CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  if (flush_cache_ == FLUSH) {
    Assembler::FlushICache(masm_.isolate(), address_, size_);
  }

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}

void MacroAssembler::TruncatingDiv(Register result, Register dividend,
                                   int32_t divisor) {
  DCHECK(!dividend.is(result));
  DCHECK(!dividend.is(r0));
  DCHECK(!result.is(r0));
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(static_cast<uint32_t>(divisor));
#ifdef V8_TARGET_ARCH_S390X
  LoadRR(result, dividend);
  MulP(result, Operand(mag.multiplier));
  ShiftRightArithP(result, result, Operand(32));

#else
  lay(sp, MemOperand(sp, -kPointerSize));
  StoreP(r1, MemOperand(sp));

  mov(r1, Operand(mag.multiplier));
  mr_z(r0, dividend);  // r0:r1 = r1 * dividend

  LoadRR(result, r0);
  LoadP(r1, MemOperand(sp));
  la(sp, MemOperand(sp, kPointerSize));
#endif
  bool neg = (mag.multiplier & (static_cast<uint32_t>(1) << 31)) != 0;
  if (divisor > 0 && neg) {
    AddP(result, dividend);
  }
  if (divisor < 0 && !neg && mag.multiplier > 0) {
    SubP(result, dividend);
  }
  if (mag.shift > 0) ShiftRightArith(result, result, Operand(mag.shift));
  ExtractBit(r0, dividend, 31);
  AddP(result, r0);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
