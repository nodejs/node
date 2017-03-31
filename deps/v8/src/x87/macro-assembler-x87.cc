// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/runtime/runtime.h"
#include "src/x87/frames-x87.h"
#include "src/x87/macro-assembler-x87.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MacroAssembler implementation.

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


void MacroAssembler::Load(Register dst, const Operand& src, Representation r) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    movsx_b(dst, src);
  } else if (r.IsUInteger8()) {
    movzx_b(dst, src);
  } else if (r.IsInteger16()) {
    movsx_w(dst, src);
  } else if (r.IsUInteger16()) {
    movzx_w(dst, src);
  } else {
    mov(dst, src);
  }
}


void MacroAssembler::Store(Register src, const Operand& dst, Representation r) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    mov_b(dst, src);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    mov_w(dst, src);
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    mov(dst, src);
  }
}


void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  if (isolate()->heap()->RootCanBeTreatedAsConstant(index)) {
    mov(destination, isolate()->heap()->root_handle(index));
    return;
  }
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(destination, Immediate(index));
  mov(destination, Operand::StaticArray(destination,
                                        times_pointer_size,
                                        roots_array_start));
}


void MacroAssembler::StoreRoot(Register source,
                               Register scratch,
                               Heap::RootListIndex index) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(index));
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(scratch, Immediate(index));
  mov(Operand::StaticArray(scratch, times_pointer_size, roots_array_start),
      source);
}


void MacroAssembler::CompareRoot(Register with,
                                 Register scratch,
                                 Heap::RootListIndex index) {
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(scratch, Immediate(index));
  cmp(with, Operand::StaticArray(scratch,
                                times_pointer_size,
                                roots_array_start));
}


void MacroAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  cmp(with, isolate()->heap()->root_handle(index));
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  cmp(with, isolate()->heap()->root_handle(index));
}


void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Push(isolate()->heap()->root_handle(index));
}

#define REG(Name) \
  { Register::kCode_##Name }

static const Register saved_regs[] = {REG(eax), REG(ecx), REG(edx)};

#undef REG

static const int kNumberOfSavedRegs = sizeof(saved_regs) / sizeof(Register);

void MacroAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                     Register exclusion1, Register exclusion2,
                                     Register exclusion3) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (!reg.is(exclusion1) && !reg.is(exclusion2) && !reg.is(exclusion3)) {
      push(reg);
    }
  }
  if (fp_mode == kSaveFPRegs) {
    // Save FPU state in m108byte.
    sub(esp, Immediate(108));
    fnsave(Operand(esp, 0));
  }
}

void MacroAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  if (fp_mode == kSaveFPRegs) {
    // Restore FPU state in m108byte.
    frstor(Operand(esp, 0));
    add(esp, Immediate(108));
  }

  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (!reg.is(exclusion1) && !reg.is(exclusion2) && !reg.is(exclusion3)) {
      pop(reg);
    }
  }
}

void MacroAssembler::InNewSpace(Register object, Register scratch, Condition cc,
                                Label* condition_met,
                                Label::Distance distance) {
  CheckPageFlag(object, scratch, MemoryChunk::kIsInNewSpaceMask, cc,
                condition_met, distance);
}


void MacroAssembler::RememberedSetHelper(
    Register object,  // Only used for debug checks.
    Register addr, Register scratch, SaveFPRegsMode save_fp,
    MacroAssembler::RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok, Label::kNear);
    int3();
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(scratch, Operand::StaticVariable(store_buffer));
  // Store pointer to buffer.
  mov(Operand(scratch, 0), addr);
  // Increment buffer top.
  add(scratch, Immediate(kPointerSize));
  // Write back new top of buffer.
  mov(Operand::StaticVariable(store_buffer), scratch);
  // Call stub on end of buffer.
  // Check for end of buffer.
  test(scratch, Immediate(StoreBuffer::kStoreBufferMask));
  if (and_then == kReturnAtEnd) {
    Label buffer_overflowed;
    j(equal, &buffer_overflowed, Label::kNear);
    ret(0);
    bind(&buffer_overflowed);
  } else {
    DCHECK(and_then == kFallThroughAtEnd);
    j(not_equal, &done, Label::kNear);
  }
  StoreBufferOverflowStub store_buffer_overflow(isolate(), save_fp);
  CallStub(&store_buffer_overflow);
  if (and_then == kReturnAtEnd) {
    ret(0);
  } else {
    DCHECK(and_then == kFallThroughAtEnd);
    bind(&done);
  }
}


void MacroAssembler::ClampTOSToUint8(Register result_reg) {
  Label done, conv_failure;
  sub(esp, Immediate(kPointerSize));
  fnclex();
  fist_s(Operand(esp, 0));
  pop(result_reg);
  X87CheckIA();
  j(equal, &conv_failure, Label::kNear);
  test(result_reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  setcc(sign, result_reg);
  sub(result_reg, Immediate(1));
  and_(result_reg, Immediate(255));
  jmp(&done, Label::kNear);
  bind(&conv_failure);
  fnclex();
  fldz();
  fld(1);
  FCmp();
  setcc(below, result_reg);  // 1 if negative, 0 if positive.
  dec_b(result_reg);         // 0 if negative, 255 if positive.
  bind(&done);
}


void MacroAssembler::ClampUint8(Register reg) {
  Label done;
  test(reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  setcc(negative, reg);  // 1 if negative, 0 if positive.
  dec_b(reg);  // 0 if negative, 255 if positive.
  bind(&done);
}


void MacroAssembler::SlowTruncateToI(Register result_reg,
                                     Register input_reg,
                                     int offset) {
  DoubleToIStub stub(isolate(), input_reg, result_reg, offset, true);
  call(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::TruncateX87TOSToI(Register result_reg) {
  sub(esp, Immediate(kDoubleSize));
  fst_d(MemOperand(esp, 0));
  SlowTruncateToI(result_reg, esp, 0);
  add(esp, Immediate(kDoubleSize));
}


void MacroAssembler::X87TOSToI(Register result_reg,
                               MinusZeroMode minus_zero_mode,
                               Label* lost_precision, Label* is_nan,
                               Label* minus_zero, Label::Distance dst) {
  Label done;
  sub(esp, Immediate(kPointerSize));
  fld(0);
  fist_s(MemOperand(esp, 0));
  fild_s(MemOperand(esp, 0));
  pop(result_reg);
  FCmp();
  j(not_equal, lost_precision, dst);
  j(parity_even, is_nan, dst);
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    test(result_reg, Operand(result_reg));
    j(not_zero, &done, Label::kNear);
    // To check for minus zero, we load the value again as float, and check
    // if that is still 0.
    sub(esp, Immediate(kPointerSize));
    fst_s(MemOperand(esp, 0));
    pop(result_reg);
    test(result_reg, Operand(result_reg));
    j(not_zero, minus_zero, dst);
  }
  bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result_reg,
                                           Register input_reg) {
  Label done, slow_case;

  SlowTruncateToI(result_reg, input_reg);
  bind(&done);
}


void MacroAssembler::LoadUint32NoSSE2(const Operand& src) {
  Label done;
  push(src);
  fild_s(Operand(esp, 0));
  cmp(src, Immediate(0));
  j(not_sign, &done, Label::kNear);
  ExternalReference uint32_bias =
        ExternalReference::address_of_uint32_bias();
  fld_d(Operand::StaticVariable(uint32_bias));
  faddp(1);
  bind(&done);
  add(esp, Immediate(kPointerSize));
}


void MacroAssembler::RecordWriteArray(
    Register object, Register value, Register index, SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    DCHECK_EQ(0, kSmiTag);
    test(value, Immediate(kSmiTagMask));
    j(zero, &done);
  }

  // Array access: calculate the destination address in the same manner as
  // KeyedStoreIC::GenerateGeneric.  Multiply a smi by 2 to get an offset
  // into an array of words.
  Register dst = index;
  lea(dst, Operand(object, index, times_half_pointer_size,
                   FixedArray::kHeaderSize - kHeapObjectTag));

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(index, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWriteField(
    Register object, int offset, Register value, Register dst,
    SaveFPRegsMode save_fp, RememberedSetAction remembered_set_action,
    SmiCheck smi_check, PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done, Label::kNear);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  lea(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    test_b(dst, Immediate((1 << kPointerSizeLog2) - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(dst, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWriteForMap(Register object, Handle<Map> map,
                                       Register scratch1, Register scratch2,
                                       SaveFPRegsMode save_fp) {
  Label done;

  Register address = scratch1;
  Register value = scratch2;
  if (emit_debug_code()) {
    Label ok;
    lea(address, FieldOperand(object, HeapObject::kMapOffset));
    test_b(address, Immediate((1 << kPointerSizeLog2) - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  DCHECK(!object.is(value));
  DCHECK(!object.is(address));
  DCHECK(!value.is(address));
  AssertNotSmi(object);

  if (!FLAG_incremental_marking) {
    return;
  }

  // Compute the address.
  lea(address, FieldOperand(object, HeapObject::kMapOffset));

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  DCHECK(!isolate()->heap()->InNewSpace(*map));
  CheckPageFlagForMap(map,
                      MemoryChunk::kPointersToHereAreInterestingMask,
                      zero,
                      &done,
                      Label::kNear);

  RecordWriteStub stub(isolate(), object, value, address, OMIT_REMEMBERED_SET,
                       save_fp);
  CallStub(&stub);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(scratch1, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(scratch2, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWrite(
    Register object, Register address, Register value, SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  DCHECK(!object.is(address));
  DCHECK(!value.is(address));
  AssertNotSmi(object);

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    cmp(value, Operand(address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done, Label::kNear);
  }

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask,
                  zero,
                  &done,
                  Label::kNear);
  }
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
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

  DCHECK(!js_function.is(code_entry));
  DCHECK(!js_function.is(scratch));
  DCHECK(!code_entry.is(scratch));
  AssertNotSmi(js_function);

  if (emit_debug_code()) {
    Label ok;
    lea(scratch, FieldOperand(js_function, offset));
    cmp(code_entry, Operand(scratch, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  CheckPageFlag(code_entry, scratch,
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);
  CheckPageFlag(js_function, scratch,
                MemoryChunk::kPointersFromHereAreInterestingMask, zero, &done,
                Label::kNear);

  // Save input registers.
  push(js_function);
  push(code_entry);

  const Register dst = scratch;
  lea(dst, FieldOperand(js_function, offset));

  // Save caller-saved registers.
  PushCallerSaved(kDontSaveFPRegs, js_function, code_entry);

  int argument_count = 3;
  PrepareCallCFunction(argument_count, code_entry);
  mov(Operand(esp, 0 * kPointerSize), js_function);
  mov(Operand(esp, 1 * kPointerSize), dst);  // Slot.
  mov(Operand(esp, 2 * kPointerSize),
      Immediate(ExternalReference::isolate_address(isolate())));

  {
    AllowExternalCallThatCantCauseGC scope(this);
    CallCFunction(
        ExternalReference::incremental_marking_record_write_code_entry_function(
            isolate()),
        argument_count);
  }

  // Restore caller-saved registers.
  PopCallerSaved(kDontSaveFPRegs, js_function, code_entry);

  // Restore input registers.
  pop(code_entry);
  pop(js_function);

  bind(&done);
}

void MacroAssembler::DebugBreak() {
  Move(eax, Immediate(0));
  mov(ebx, Immediate(ExternalReference(Runtime::kHandleDebuggerStatement,
                                       isolate())));
  CEntryStub ces(isolate(), 1);
  call(ces.GetCode(), RelocInfo::DEBUGGER_STATEMENT);
}

void MacroAssembler::ShlPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(high, low);
    shl(high, shift - 32);
    xor_(low, low);
  } else {
    shld(high, low, shift);
    shl(low, shift);
  }
}

void MacroAssembler::ShlPair_cl(Register high, Register low) {
  shld_cl(high, low);
  shl_cl(low);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(high, low);
  xor_(low, low);
  bind(&done);
}

void MacroAssembler::ShrPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    shr(low, shift - 32);
    xor_(high, high);
  } else {
    shrd(high, low, shift);
    shr(high, shift);
  }
}

void MacroAssembler::ShrPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  shr_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  xor_(high, high);
  bind(&done);
}

void MacroAssembler::SarPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    sar(low, shift - 32);
    sar(high, 31);
  } else {
    shrd(high, low, shift);
    sar(high, shift);
  }
}

void MacroAssembler::SarPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  sar_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  sar(high, 31);
  bind(&done);
}

bool MacroAssembler::IsUnsafeImmediate(const Immediate& x) {
  static const int kMaxImmediateBits = 17;
  if (!RelocInfo::IsNone(x.rmode_)) return false;
  return !is_intn(x.x_, kMaxImmediateBits);
}


void MacroAssembler::SafeMove(Register dst, const Immediate& x) {
  if (IsUnsafeImmediate(x) && jit_cookie() != 0) {
    Move(dst, Immediate(x.x_ ^ jit_cookie()));
    xor_(dst, jit_cookie());
  } else {
    Move(dst, x);
  }
}


void MacroAssembler::SafePush(const Immediate& x) {
  if (IsUnsafeImmediate(x) && jit_cookie() != 0) {
    push(Immediate(x.x_ ^ jit_cookie()));
    xor_(Operand(esp, 0), Immediate(jit_cookie()));
  } else {
    push(x);
  }
}


void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpb(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::CompareMap(Register obj, Handle<Map> map) {
  cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
}


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  CompareMap(obj, map);
  j(not_equal, fail);
}


void MacroAssembler::DispatchWeakMap(Register obj, Register scratch1,
                                     Register scratch2, Handle<WeakCell> cell,
                                     Handle<Code> success,
                                     SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  mov(scratch1, FieldOperand(obj, HeapObject::kMapOffset));
  CmpWeakValue(scratch1, cell, scratch2);
  j(equal, success);

  bind(&fail);
}


Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  test(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


Condition MacroAssembler::IsObjectNameType(Register heap_object,
                                           Register map,
                                           Register instance_type) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  cmpb(instance_type, Immediate(LAST_NAME_TYPE));
  return below_equal;
}


void MacroAssembler::FCmp() {
  fucompp();
  push(eax);
  fnstsw_ax();
  sahf();
  pop(eax);
}


void MacroAssembler::FXamMinusZero() {
  fxam();
  push(eax);
  fnstsw_ax();
  and_(eax, Immediate(0x4700));
  // For minus zero, C3 == 1 && C1 == 1.
  cmp(eax, Immediate(0x4200));
  pop(eax);
  fstp(0);
}


void MacroAssembler::FXamSign() {
  fxam();
  push(eax);
  fnstsw_ax();
  // For negative value (including -0.0), C1 == 1.
  and_(eax, Immediate(0x0200));
  pop(eax);
  fstp(0);
}


void MacroAssembler::X87CheckIA() {
  push(eax);
  fnstsw_ax();
  // For #IA, IE == 1 && SF == 0.
  and_(eax, Immediate(0x0041));
  cmp(eax, Immediate(0x0001));
  pop(eax);
}


// rc=00B, round to nearest.
// rc=01B, round down.
// rc=10B, round up.
// rc=11B, round toward zero.
void MacroAssembler::X87SetRC(int rc) {
  sub(esp, Immediate(kPointerSize));
  fnstcw(MemOperand(esp, 0));
  and_(MemOperand(esp, 0), Immediate(0xF3FF));
  or_(MemOperand(esp, 0), Immediate(rc));
  fldcw(MemOperand(esp, 0));
  add(esp, Immediate(kPointerSize));
}


void MacroAssembler::X87SetFPUCW(int cw) {
  RecordComment("-- X87SetFPUCW start --");
  push(Immediate(cw));
  fldcw(MemOperand(esp, 0));
  add(esp, Immediate(kPointerSize));
  RecordComment("-- X87SetFPUCW end--");
}


void MacroAssembler::AssertNumber(Register object) {
  if (emit_debug_code()) {
    Label ok;
    JumpIfSmi(object, &ok);
    cmp(FieldOperand(object, HeapObject::kMapOffset),
        isolate()->factory()->heap_number_map());
    Check(equal, kOperandNotANumber);
    bind(&ok);
  }
}

void MacroAssembler::AssertNotNumber(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsANumber);
    cmp(FieldOperand(object, HeapObject::kMapOffset),
        isolate()->factory()->heap_number_map());
    Check(not_equal, kOperandIsANumber);
  }
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(equal, kOperandIsNotASmi);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAString);
    push(object);
    mov(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(below, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAName);
    push(object);
    mov(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, LAST_NAME_TYPE);
    pop(object);
    Check(below_equal, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAFunction);
    Push(object);
    CmpObjectType(object, JS_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAGeneratorObject);
    Push(object);
    CmpObjectType(object, JS_GENERATOR_OBJECT_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotAGeneratorObject);
  }
}

void MacroAssembler::AssertReceiver(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAReceiver);
    Push(object);
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    CmpObjectType(object, FIRST_JS_RECEIVER_TYPE, object);
    Pop(object);
    Check(above_equal, kOperandIsNotAReceiver);
  }
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    cmp(object, isolate()->factory()->undefined_value());
    j(equal, &done_checking);
    cmp(FieldOperand(object, 0),
        Immediate(isolate()->factory()->allocation_site_map()));
    Assert(equal, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmi);
  }
}

void MacroAssembler::StubPrologue(StackFrame::Type type) {
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(Immediate(Smi::FromInt(type)));
}


void MacroAssembler::Prologue(bool code_pre_aging) {
  PredictableCodeSizeScope predictible_code_size_scope(this,
      kNoCodeAgeSequenceLength);
  if (code_pre_aging) {
      // Pre-age the code.
    call(isolate()->builtins()->MarkCodeAsExecutedOnce(),
        RelocInfo::CODE_AGE_SEQUENCE);
    Nop(kNoCodeAgeSequenceLength - Assembler::kCallInstructionLength);
  } else {
    push(ebp);  // Caller's frame pointer.
    mov(ebp, esp);
    push(esi);  // Callee's context.
    push(edi);  // Callee's JS function.
  }
}

void MacroAssembler::EmitLoadFeedbackVector(Register vector) {
  mov(vector, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  mov(vector, FieldOperand(vector, JSFunction::kLiteralsOffset));
  mov(vector, FieldOperand(vector, LiteralsArray::kFeedbackVectorOffset));
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // Out-of-line constant pool not implemented on x87.
  UNREACHABLE();
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, esp);
  push(Immediate(Smi::FromInt(type)));
  if (type == StackFrame::INTERNAL) {
    push(Immediate(CodeObject()));
  }
  if (emit_debug_code()) {
    cmp(Operand(esp, 0), Immediate(isolate()->factory()->undefined_value()));
    Check(not_equal, kCodeObjectNotProperlyPatched);
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
        Immediate(Smi::FromInt(type)));
    Check(equal, kStackFrameTypesMustMatch);
  }
  leave();
}

void MacroAssembler::EnterBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Push(ebp);
  Move(ebp, esp);
  Push(context);
  Push(target);
  Push(argc);
}

void MacroAssembler::LeaveBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Pop(argc);
  Pop(target);
  Pop(context);
  leave();
}

void MacroAssembler::EnterExitFramePrologue(StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  DCHECK_EQ(+2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(+1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  push(ebp);
  mov(ebp, esp);

  // Reserve room for entry stack pointer and push the code object.
  push(Immediate(Smi::FromInt(frame_type)));
  DCHECK_EQ(-2 * kPointerSize, ExitFrameConstants::kSPOffset);
  push(Immediate(0));  // Saved entry sp, patched before call.
  DCHECK_EQ(-3 * kPointerSize, ExitFrameConstants::kCodeOffset);
  push(Immediate(CodeObject()));  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress, isolate());
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  ExternalReference c_function_address(Isolate::kCFunctionAddress, isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), ebp);
  mov(Operand::StaticVariable(context_address), esi);
  mov(Operand::StaticVariable(c_function_address), ebx);
}


void MacroAssembler::EnterExitFrameEpilogue(int argc, bool save_doubles) {
  // Optionally save FPU state.
  if (save_doubles) {
    // Store FPU state to m108byte.
    int space = 108 + argc * kPointerSize;
    sub(esp, Immediate(space));
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    fnsave(MemOperand(ebp, offset - 108));
  } else {
    sub(esp, Immediate(argc * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = base::OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo32(kFrameAlignment));
    and_(esp, -kFrameAlignment);
  }

  // Patch the saved entry sp.
  mov(Operand(ebp, ExitFrameConstants::kSPOffset), esp);
}

void MacroAssembler::EnterExitFrame(int argc, bool save_doubles,
                                    StackFrame::Type frame_type) {
  EnterExitFramePrologue(frame_type);

  // Set up argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  mov(edi, eax);
  lea(esi, Operand(ebp, eax, times_4, offset));

  // Reserve space for argc, argv and isolate.
  EnterExitFrameEpilogue(argc, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int argc) {
  EnterExitFramePrologue(StackFrame::EXIT);
  EnterExitFrameEpilogue(argc, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles, bool pop_arguments) {
  // Optionally restore FPU state.
  if (save_doubles) {
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    frstor(MemOperand(ebp, offset - 108));
  }

  if (pop_arguments) {
    // Get the return address from the stack and restore the frame pointer.
    mov(ecx, Operand(ebp, 1 * kPointerSize));
    mov(ebp, Operand(ebp, 0 * kPointerSize));

    // Pop the arguments and the receiver from the caller stack.
    lea(esp, Operand(esi, 1 * kPointerSize));

    // Push the return address to get ready to return.
    push(ecx);
  } else {
    // Otherwise just leave the exit frame.
    leave();
  }

  LeaveExitFrameEpilogue(true);
}


void MacroAssembler::LeaveExitFrameEpilogue(bool restore_context) {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  if (restore_context) {
    mov(esi, Operand::StaticVariable(context_address));
  }
#ifdef DEBUG
  mov(Operand::StaticVariable(context_address), Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress,
                                       isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), Immediate(0));
}


void MacroAssembler::LeaveApiExitFrame(bool restore_context) {
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue(restore_context);
}


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  // Link the current handler as the next handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  push(Operand::StaticVariable(handler_address));

  // Set this new handler as the current one.
  mov(Operand::StaticVariable(handler_address), esp);
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  pop(Operand::StaticVariable(handler_address));
  add(esp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
//
// Note: r0 will contain hash code
void MacroAssembler::GetNumberHash(Register r0, Register scratch) {
  // Xor original key with a seed.
  if (serializer_enabled()) {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    mov(scratch, Immediate(Heap::kHashSeedRootIndex));
    mov(scratch,
        Operand::StaticArray(scratch, times_pointer_size, roots_array_start));
    SmiUntag(scratch);
    xor_(r0, scratch);
  } else {
    int32_t seed = isolate()->heap()->HashSeed();
    xor_(r0, Immediate(seed));
  }

  // hash = ~hash + (hash << 15);
  mov(scratch, r0);
  not_(r0);
  shl(scratch, 15);
  add(r0, scratch);
  // hash = hash ^ (hash >> 12);
  mov(scratch, r0);
  shr(scratch, 12);
  xor_(r0, scratch);
  // hash = hash + (hash << 2);
  lea(r0, Operand(r0, r0, times_4, 0));
  // hash = hash ^ (hash >> 4);
  mov(scratch, r0);
  shr(scratch, 4);
  xor_(r0, scratch);
  // hash = hash * 2057;
  imul(r0, r0, 2057);
  // hash = hash ^ (hash >> 16);
  mov(scratch, r0);
  shr(scratch, 16);
  xor_(r0, scratch);
  and_(r0, 0x3fffffff);
}

void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    DCHECK(scratch.is(no_reg));
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    cmp(result, Operand::StaticVariable(allocation_top));
    Check(equal, kUnexpectedAllocationTop);
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available.
  if (scratch.is(no_reg)) {
    mov(result, Operand::StaticVariable(allocation_top));
  } else {
    mov(scratch, Immediate(allocation_top));
    mov(result, Operand(scratch, 0));
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch,
                                               AllocationFlags flags) {
  if (emit_debug_code()) {
    test(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, kUnalignedAllocationInNewSpace);
  }

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Update new top. Use scratch if available.
  if (scratch.is(no_reg)) {
    mov(Operand::StaticVariable(allocation_top), result_end);
  } else {
    mov(Operand(scratch, 0), result_end);
  }
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  DCHECK(object_size <= kMaxRegularHeapObjectSize);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      if (result_end.is_valid()) {
        mov(result_end, Immediate(0x7191));
      }
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
    }
    jmp(gc_required);
    return;
  }
  DCHECK(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  // Align the next allocation. Storing the filler map without checking top is
  // safe in new-space because the limit of the heap is aligned there.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    if ((flags & PRETENURE) != 0) {
      cmp(result, Operand::StaticVariable(allocation_limit));
      j(above_equal, gc_required);
    }
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  // Calculate new top and bail out if space is exhausted.
  Register top_reg = result_end.is_valid() ? result_end : result;

  if (!top_reg.is(result)) {
    mov(top_reg, result);
  }
  add(top_reg, Immediate(object_size));
  cmp(top_reg, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    UpdateAllocationTopHelper(top_reg, scratch, flags);
  }

  if (top_reg.is(result)) {
    sub(result, Immediate(object_size - kHeapObjectTag));
  } else {
    // Tag the result.
    DCHECK(kHeapObjectTag == 1);
    inc(result);
  }
}


void MacroAssembler::Allocate(int header_size,
                              ScaleFactor element_size,
                              Register element_count,
                              RegisterValueType element_count_type,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK((flags & SIZE_IN_WORDS) == 0);
  DCHECK((flags & ALLOCATION_FOLDING_DOMINATOR) == 0);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // Register element_count is not modified by the function.
    }
    jmp(gc_required);
    return;
  }
  DCHECK(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  // Align the next allocation. Storing the filler map without checking top is
  // safe in new-space because the limit of the heap is aligned there.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    if ((flags & PRETENURE) != 0) {
      cmp(result, Operand::StaticVariable(allocation_limit));
      j(above_equal, gc_required);
    }
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  // Calculate new top and bail out if space is exhausted.
  // We assume that element_count*element_size + header_size does not
  // overflow.
  if (element_count_type == REGISTER_VALUE_IS_SMI) {
    STATIC_ASSERT(static_cast<ScaleFactor>(times_2 - 1) == times_1);
    STATIC_ASSERT(static_cast<ScaleFactor>(times_4 - 1) == times_2);
    STATIC_ASSERT(static_cast<ScaleFactor>(times_8 - 1) == times_4);
    DCHECK(element_size >= times_2);
    DCHECK(kSmiTagSize == 1);
    element_size = static_cast<ScaleFactor>(element_size - 1);
  } else {
    DCHECK(element_count_type == REGISTER_VALUE_IS_INT32);
  }
  lea(result_end, Operand(element_count, element_size, header_size));
  add(result_end, result);
  j(carry, gc_required);
  cmp(result_end, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  // Tag result.
  DCHECK(kHeapObjectTag == 1);
  inc(result);

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch, flags);
}

void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // object_size is left unchanged by this function.
    }
    jmp(gc_required);
    return;
  }
  DCHECK(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  // Align the next allocation. Storing the filler map without checking top is
  // safe in new-space because the limit of the heap is aligned there.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    if ((flags & PRETENURE) != 0) {
      cmp(result, Operand::StaticVariable(allocation_limit));
      j(above_equal, gc_required);
    }
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  // Calculate new top and bail out if space is exhausted.
  if (!object_size.is(result_end)) {
    mov(result_end, object_size);
  }
  add(result_end, result);
  cmp(result_end, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  // Tag result.
  DCHECK(kHeapObjectTag == 1);
  inc(result);

  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    UpdateAllocationTopHelper(result_end, scratch, flags);
  }
}

void MacroAssembler::FastAllocate(int object_size, Register result,
                                  Register result_end, AllocationFlags flags) {
  DCHECK(!result.is(result_end));
  // Load address of new object into result.
  LoadAllocationTopHelper(result, no_reg, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  lea(result_end, Operand(result, object_size));
  UpdateAllocationTopHelper(result_end, no_reg, flags);

  DCHECK(kHeapObjectTag == 1);
  inc(result);
}

void MacroAssembler::FastAllocate(Register object_size, Register result,
                                  Register result_end, AllocationFlags flags) {
  DCHECK(!result.is(result_end));
  // Load address of new object into result.
  LoadAllocationTopHelper(result, no_reg, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  lea(result_end, Operand(result, object_size, times_1, 0));
  UpdateAllocationTopHelper(result_end, no_reg, flags);

  DCHECK(kHeapObjectTag == 1);
  inc(result);
}

void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        MutableMode mode) {
  // Allocate heap number in new space.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           NO_ALLOCATION_FLAGS);

  Handle<Map> map = mode == MUTABLE
      ? isolate()->factory()->mutable_heap_number_map()
      : isolate()->factory()->heap_number_map();

  // Set the map.
  mov(FieldOperand(result, HeapObject::kMapOffset), Immediate(map));
}

void MacroAssembler::AllocateJSValue(Register result, Register constructor,
                                     Register value, Register scratch,
                                     Label* gc_required) {
  DCHECK(!result.is(constructor));
  DCHECK(!result.is(scratch));
  DCHECK(!result.is(value));

  // Allocate JSValue in new space.
  Allocate(JSValue::kSize, result, scratch, no_reg, gc_required,
           NO_ALLOCATION_FLAGS);

  // Initialize the JSValue.
  LoadGlobalFunctionInitialMap(constructor, scratch);
  mov(FieldOperand(result, HeapObject::kMapOffset), scratch);
  LoadRoot(scratch, Heap::kEmptyFixedArrayRootIndex);
  mov(FieldOperand(result, JSObject::kPropertiesOffset), scratch);
  mov(FieldOperand(result, JSObject::kElementsOffset), scratch);
  mov(FieldOperand(result, JSValue::kValueOffset), value);
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
}

void MacroAssembler::InitializeFieldsWithFiller(Register current_address,
                                                Register end_address,
                                                Register filler) {
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  mov(Operand(current_address, 0), filler);
  add(current_address, Immediate(kPointerSize));
  bind(&entry);
  cmp(current_address, end_address);
  j(below, &loop, Label::kNear);
}


void MacroAssembler::BooleanBitTest(Register object,
                                    int field_offset,
                                    int bit_index) {
  bit_index += kSmiTagSize + kSmiShiftSize;
  DCHECK(base::bits::IsPowerOfTwo32(kBitsPerByte));
  int byte_index = bit_index / kBitsPerByte;
  int byte_bit_index = bit_index & (kBitsPerByte - 1);
  test_b(FieldOperand(object, field_offset + byte_index),
         Immediate(1 << byte_bit_index));
}



void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op,
                                      Label* then_label) {
  Label ok;
  test(result, result);
  j(not_zero, &ok, Label::kNear);
  test(op, op);
  j(sign, then_label, Label::kNear);
  bind(&ok);
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op1,
                                      Register op2,
                                      Register scratch,
                                      Label* then_label) {
  Label ok;
  test(result, result);
  j(not_zero, &ok, Label::kNear);
  mov(scratch, op1);
  or_(scratch, op2);
  j(sign, then_label, Label::kNear);
  bind(&ok);
}


void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp) {
  Label done, loop;
  mov(result, FieldOperand(map, Map::kConstructorOrBackPointerOffset));
  bind(&loop);
  JumpIfSmi(result, &done, Label::kNear);
  CmpObjectType(result, MAP_TYPE, temp);
  j(not_equal, &done, Label::kNear);
  mov(result, FieldOperand(result, Map::kConstructorOrBackPointerOffset));
  jmp(&loop);
  bind(&done);
}


void MacroAssembler::TryGetFunctionPrototype(Register function, Register result,
                                             Register scratch, Label* miss) {
  // Get the prototype or initial map from the function.
  mov(result,
      FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  cmp(result, Immediate(isolate()->factory()->the_hole_value()));
  j(equal, miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CmpObjectType(result, MAP_TYPE, scratch);
  j(not_equal, &done, Label::kNear);

  // Get the prototype from the initial map.
  mov(result, FieldOperand(result, Map::kPrototypeOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  jmp(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  DCHECK(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Move(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ExternalReference(f, isolate())));
  CEntryStub ces(isolate(), 1, save_doubles);
  CallStub(&ces);
}


void MacroAssembler::CallExternalReference(ExternalReference ref,
                                           int num_arguments) {
  mov(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ref));

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  // ----------- S t a t e -------------
  //  -- esp[0]                 : return address
  //  -- esp[8]                 : argument num_arguments - 1
  //  ...
  //  -- esp[8 * num_arguments] : argument 0 (receiver)
  //
  //  For runtime functions with variable arguments:
  //  -- eax                    : number of  arguments
  // -----------------------------------

  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    mov(eax, Immediate(function->nargs));
  }
  JumpToExternalReference(ExternalReference(fid, isolate()));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             bool builtin_exit_frame) {
  // Set the entry point and jump to the C entry runtime stub.
  mov(ebx, Immediate(ext));
  CEntryStub ces(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                 builtin_exit_frame);
  jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}

void MacroAssembler::PrepareForTailCall(
    const ParameterCount& callee_args_count, Register caller_args_count_reg,
    Register scratch0, Register scratch1, ReturnAddressState ra_state,
    int number_of_temp_values_after_return_address) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
  DCHECK(ra_state != ReturnAddressState::kNotOnStack ||
         number_of_temp_values_after_return_address == 0);
#endif

  // Calculate the destination address where we will put the return address
  // after we drop current frame.
  Register new_sp_reg = scratch0;
  if (callee_args_count.is_reg()) {
    sub(caller_args_count_reg, callee_args_count.reg());
    lea(new_sp_reg,
        Operand(ebp, caller_args_count_reg, times_pointer_size,
                StandardFrameConstants::kCallerPCOffset -
                    number_of_temp_values_after_return_address * kPointerSize));
  } else {
    lea(new_sp_reg, Operand(ebp, caller_args_count_reg, times_pointer_size,
                            StandardFrameConstants::kCallerPCOffset -
                                (callee_args_count.immediate() +
                                 number_of_temp_values_after_return_address) *
                                    kPointerSize));
  }

  if (FLAG_debug_code) {
    cmp(esp, new_sp_reg);
    Check(below, kStackAccessBelowStackPointer);
  }

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch1;
  if (ra_state == ReturnAddressState::kOnStack) {
    mov(tmp_reg, Operand(ebp, StandardFrameConstants::kCallerPCOffset));
    mov(Operand(esp, number_of_temp_values_after_return_address * kPointerSize),
        tmp_reg);
  } else {
    DCHECK(ReturnAddressState::kNotOnStack == ra_state);
    DCHECK_EQ(0, number_of_temp_values_after_return_address);
    Push(Operand(ebp, StandardFrameConstants::kCallerPCOffset));
  }

  // Restore caller's frame pointer now as it could be overwritten by
  // the copying loop.
  mov(ebp, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // +2 here is to copy both receiver and return address.
  Register count_reg = caller_args_count_reg;
  if (callee_args_count.is_reg()) {
    lea(count_reg, Operand(callee_args_count.reg(),
                           2 + number_of_temp_values_after_return_address));
  } else {
    mov(count_reg, Immediate(callee_args_count.immediate() + 2 +
                             number_of_temp_values_after_return_address));
    // TODO(ishell): Unroll copying loop for small immediate values.
  }

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  dec(count_reg);
  mov(tmp_reg, Operand(esp, count_reg, times_pointer_size, 0));
  mov(Operand(new_sp_reg, count_reg, times_pointer_size, 0), tmp_reg);
  bind(&entry);
  cmp(count_reg, Immediate(0));
  j(not_equal, &loop, Label::kNear);

  // Leave current frame.
  mov(esp, new_sp_reg);
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance done_near,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label invoke;
  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    mov(eax, actual.immediate());
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
        mov(ebx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      mov(eax, actual.immediate());
      cmp(expected.reg(), actual.immediate());
      j(equal, &invoke);
      DCHECK(expected.reg().is(ebx));
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmp(expected.reg(), actual.reg());
      j(equal, &invoke);
      DCHECK(actual.reg().is(eax));
      DCHECK(expected.reg().is(ebx));
    } else {
      Move(eax, actual.reg());
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor, RelocInfo::CODE_TARGET));
      call(adaptor, RelocInfo::CODE_TARGET);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        jmp(done, done_near);
      }
    } else {
      jmp(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;
  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  cmpb(Operand::StaticVariable(debug_hook_active), Immediate(0));
  j(equal, &skip_hook);
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
    Push(fun);
    Push(fun);
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
  DCHECK(function.is(edi));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(edx));

  if (call_wrapper.NeedsDebugHookCheck()) {
    CheckDebugHook(function, new_target, expected, actual);
  }

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    mov(edx, isolate()->factory()->undefined_value());
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 Label::kNear, call_wrapper);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Operand code = FieldOperand(function, JSFunction::kCodeEntryOffset);
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      call(code);
      call_wrapper.AfterCall();
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      jmp(code);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun.is(edi));
  mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kFormalParameterCountOffset));
  SmiUntag(ebx);

  ParameterCount expected(ebx);
  InvokeFunctionCode(edi, new_target, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun.is(edi));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  InvokeFunctionCode(edi, no_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  LoadHeapObject(edi, function);
  InvokeFunction(edi, expected, actual, flag, call_wrapper);
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    mov(dst, Operand(esi, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      mov(dst, Operand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    mov(dst, esi);
  }

  // We should not have found a with context by walking the context chain
  // (i.e., the static scope chain and runtime context chain do not agree).
  // A variable occurring in such a scope should have slot type LOOKUP and
  // not CONTEXT.
  if (emit_debug_code()) {
    cmp(FieldOperand(dst, HeapObject::kMapOffset),
        isolate()->factory()->with_context_map());
    Check(not_equal, kVariableResolvedToWithContext);
  }
}


void MacroAssembler::LoadGlobalProxy(Register dst) {
  mov(dst, NativeContextOperand());
  mov(dst, ContextOperand(dst, Context::GLOBAL_PROXY_INDEX));
}

void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the native context from the current context.
  mov(function, NativeContextOperand());
  // Load the function from the native context.
  mov(function, ContextOperand(function, index));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  mov(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, isolate()->factory()->meta_map(), &fail, DO_SMI_CHECK);
    jmp(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}


// Store the value in register src in the safepoint register stack
// slot for register dst.
void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Register src) {
  mov(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Immediate src) {
  mov(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  mov(dst, SafepointRegisterSlot(src));
}


Operand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return Operand(esp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the lowest encoding,
  // which means that lowest encodings are furthest away from
  // the stack pointer.
  DCHECK(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return kNumSafepointRegisters - reg_code - 1;
}


void MacroAssembler::LoadHeapObject(Register result,
                                    Handle<HeapObject> object) {
  mov(result, object);
}


void MacroAssembler::CmpHeapObject(Register reg, Handle<HeapObject> object) {
  cmp(reg, object);
}

void MacroAssembler::PushHeapObject(Handle<HeapObject> object) { Push(object); }

void MacroAssembler::CmpWeakValue(Register value, Handle<WeakCell> cell,
                                  Register scratch) {
  mov(scratch, cell);
  cmp(value, FieldOperand(scratch, WeakCell::kValueOffset));
}


void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  mov(value, cell);
  mov(value, FieldOperand(value, WeakCell::kValueOffset));
}


void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    pop(scratch);
    add(esp, Immediate(bytes_dropped));
    push(scratch);
    ret(0);
  }
}


void MacroAssembler::VerifyX87StackDepth(uint32_t depth) {
  // Turn off the stack depth check when serializer is enabled to reduce the
  // code size.
  if (serializer_enabled()) return;
  // Make sure the floating point stack is either empty or has depth items.
  DCHECK(depth <= 7);
  // This is very expensive.
  DCHECK(FLAG_debug_code && FLAG_enable_slow_asserts);

  // The top-of-stack (tos) is 7 if there is one item pushed.
  int tos = (8 - depth) % 8;
  const int kTopMask = 0x3800;
  push(eax);
  fwait();
  fnstsw_ax();
  and_(eax, kTopMask);
  shr(eax, 11);
  cmp(eax, Immediate(tos));
  Check(equal, kUnexpectedFPUStackDepthAfterInstruction);
  fnclex();
  pop(eax);
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(esp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    mov(dst, src);
  }
}


void MacroAssembler::Move(Register dst, const Immediate& x) {
  if (x.is_zero() && RelocInfo::IsNone(x.rmode_)) {
    xor_(dst, dst);  // Shorter than mov of 32-bit immediate 0.
  } else {
    mov(dst, x);
  }
}


void MacroAssembler::Move(const Operand& dst, const Immediate& x) {
  mov(dst, x);
}


void MacroAssembler::Lzcnt(Register dst, const Operand& src) {
  // TODO(intel): Add support for LZCNT (with ABM/BMI1).
  Label not_zero_src;
  bsr(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(63));  // 63^31 == 32
  bind(&not_zero_src);
  xor_(dst, Immediate(31));  // for x in [0..31], 31^x == 31-x.
}


void MacroAssembler::Tzcnt(Register dst, const Operand& src) {
  // TODO(intel): Add support for TZCNT (with ABM/BMI1).
  Label not_zero_src;
  bsf(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(32));  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}


void MacroAssembler::Popcnt(Register dst, const Operand& src) {
  // TODO(intel): Add support for POPCNT (with POPCNT)
  // if (CpuFeatures::IsSupported(POPCNT)) {
  //   CpuFeatureScope scope(this, POPCNT);
  //   popcnt(dst, src);
  //   return;
  // }
  UNREACHABLE();
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(Operand::StaticVariable(ExternalReference(counter)), Immediate(value));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}


void MacroAssembler::IncrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    IncrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::DecrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    DecrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::Assert(Condition cc, BailoutReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    Factory* factory = isolate()->factory();
    Label ok;
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_array_map()));
    j(equal, &ok);
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_double_array_map()));
    j(equal, &ok);
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_cow_array_map()));
    j(equal, &ok);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
  }
}


void MacroAssembler::Check(Condition cc, BailoutReason reason) {
  Label L;
  j(cc, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}


void MacroAssembler::CheckStackAlignment() {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}


void MacroAssembler::Abort(BailoutReason reason) {
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    int3();
    return;
  }
#endif

  // Check if Abort() has already been initialized.
  DCHECK(isolate()->builtins()->Abort()->IsHeapObject());

  Move(edx, Smi::FromInt(static_cast<int>(reason)));

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
  int3();
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  mov(descriptors, FieldOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  mov(dst, FieldOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  mov(dst, FieldOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  mov(dst, FieldOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  int offset = accessor == ACCESSOR_GETTER ? AccessorPair::kGetterOffset
                                           : AccessorPair::kSetterOffset;
  mov(dst, FieldOperand(dst, offset));
}

void MacroAssembler::JumpIfNotBothSequentialOneByteStrings(Register object1,
                                                           Register object2,
                                                           Register scratch1,
                                                           Register scratch2,
                                                           Label* failure) {
  // Check that both objects are not smis.
  STATIC_ASSERT(kSmiTag == 0);
  mov(scratch1, object1);
  and_(scratch1, object2);
  JumpIfSmi(scratch1, failure);

  // Load instance type for both strings.
  mov(scratch1, FieldOperand(object1, HeapObject::kMapOffset));
  mov(scratch2, FieldOperand(object2, HeapObject::kMapOffset));
  movzx_b(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzx_b(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat one-byte strings.
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  // Interleave bits from both instance types and compare them in one check.
  DCHECK_EQ(0, kFlatOneByteStringMask & (kFlatOneByteStringMask << 3));
  and_(scratch1, kFlatOneByteStringMask);
  and_(scratch2, kFlatOneByteStringMask);
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmp(scratch1, kFlatOneByteStringTag | (kFlatOneByteStringTag << 3));
  j(not_equal, failure);
}


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Operand operand,
                                                     Label* not_unique_name,
                                                     Label::Distance distance) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  test(operand, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  j(zero, &succeed);
  cmpb(operand, Immediate(SYMBOL_TYPE));
  j(not_equal, not_unique_name, distance);

  bind(&succeed);
}


void MacroAssembler::EmitSeqStringSetCharCheck(Register string,
                                               Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  JumpIfNotSmi(string, &is_object, Label::kNear);
  Abort(kNonObject);
  bind(&is_object);

  push(value);
  mov(value, FieldOperand(string, HeapObject::kMapOffset));
  movzx_b(value, FieldOperand(value, Map::kInstanceTypeOffset));

  and_(value, Immediate(kStringRepresentationMask | kStringEncodingMask));
  cmp(value, Immediate(encoding_mask));
  pop(value);
  Check(equal, kUnexpectedStringType);

  // The index is assumed to be untagged coming in, tag it to compare with the
  // string length without using a temp register, it is restored at the end of
  // this function.
  SmiTag(index);
  Check(no_overflow, kIndexIsTooLarge);

  cmp(index, FieldOperand(string, String::kLengthOffset));
  Check(less, kIndexIsTooLarge);

  cmp(index, Immediate(Smi::kZero));
  Check(greater_equal, kIndexIsNegative);

  // Restore the index
  SmiUntag(index);
}


void MacroAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  if (frame_alignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    sub(esp, Immediate((num_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    and_(esp, -frame_alignment);
    mov(Operand(esp, num_arguments * kPointerSize), scratch);
  } else {
    sub(esp, Immediate(num_arguments * kPointerSize));
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  mov(eax, Immediate(function));
  CallCFunction(eax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  DCHECK(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  if (base::OS::ActivationFrameAlignment() != 0) {
    mov(esp, Operand(esp, num_arguments * kPointerSize));
  } else {
    add(esp, Immediate(num_arguments * kPointerSize));
  }
}


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3,
                Register reg4,
                Register reg5,
                Register reg6,
                Register reg7,
                Register reg8) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() +
      reg3.is_valid() + reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
      reg7.is_valid() + reg8.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


CodePatcher::CodePatcher(Isolate* isolate, byte* address, int size)
    : address_(address),
      size_(size),
      masm_(isolate, address, size + Assembler::kGap, CodeObjectRequired::kNo) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  Assembler::FlushICache(masm_.isolate(), address_, size_);

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  if (scratch.is(object)) {
    and_(scratch, Immediate(~Page::kPageAlignmentMask));
  } else {
    mov(scratch, Immediate(~Page::kPageAlignmentMask));
    and_(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  } else {
    test(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::CheckPageFlagForMap(
    Handle<Map> map,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  Page* page = Page::FromAddress(map->address());
  DCHECK(!serializer_enabled());  // Serializer cannot match page_flags.
  ExternalReference reference(ExternalReference::page_flags(page));
  // The inlined static address check of the page's flags relies
  // on maps never being compacted.
  DCHECK(!isolate()->heap()->mark_compact_collector()->
         IsOnEvacuationCandidate(*map));
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand::StaticVariable(reference), Immediate(mask));
  } else {
    test(Operand::StaticVariable(reference), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black,
                                 Label::Distance on_black_near) {
  HasColor(object, scratch0, scratch1, on_black, on_black_near, 1,
           1);  // kBlackBitPattern.
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              Label::Distance has_color_distance,
                              int first_bit,
                              int second_bit) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, ecx));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(first_bit == 1 ? zero : not_zero, &other_color, Label::kNear);
  add(mask_scratch, mask_scratch);  // Shift left 1 by adding.
  j(zero, &word_boundary, Label::kNear);
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(second_bit == 1 ? not_zero : zero, has_color, has_color_distance);
  jmp(&other_color, Label::kNear);

  bind(&word_boundary);
  test_b(Operand(bitmap_scratch, MemoryChunk::kHeaderSize + kPointerSize),
         Immediate(1));

  j(second_bit == 1 ? not_zero : zero, has_color, has_color_distance);
  bind(&other_color);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  DCHECK(!AreAliased(addr_reg, mask_reg, bitmap_reg, ecx));
  mov(bitmap_reg, Immediate(~Page::kPageAlignmentMask));
  and_(bitmap_reg, addr_reg);
  mov(ecx, addr_reg);
  int shift =
      Bitmap::kBitsPerCellLog2 + kPointerSizeLog2 - Bitmap::kBytesPerCellLog2;
  shr(ecx, shift);
  and_(ecx,
       (Page::kPageAlignmentMask >> shift) & ~(Bitmap::kBytesPerCell - 1));

  add(bitmap_reg, ecx);
  mov(ecx, addr_reg);
  shr(ecx, kPointerSizeLog2);
  and_(ecx, (1 << Bitmap::kBitsPerCellLog2) - 1);
  mov(mask_reg, Immediate(1));
  shl_cl(mask_reg);
}


void MacroAssembler::JumpIfWhite(Register value, Register bitmap_scratch,
                                 Register mask_scratch, Label* value_is_white,
                                 Label::Distance distance) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, ecx));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(zero, value_is_white, Label::kNear);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  mov(dst, FieldOperand(map, Map::kBitField3Offset));
  and_(dst, Immediate(Map::EnumLengthBits::kMask));
  SmiTag(dst);
}


void MacroAssembler::CheckEnumCache(Label* call_runtime) {
  Label next, start;
  mov(ecx, eax);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));

  EnumLength(edx, ebx);
  cmp(edx, Immediate(Smi::FromInt(kInvalidEnumCacheSentinel)));
  j(equal, call_runtime);

  jmp(&start);

  bind(&next);
  mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(edx, ebx);
  cmp(edx, Immediate(Smi::kZero));
  j(not_equal, call_runtime);

  bind(&start);

  // Check that there are no elements. Register rcx contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  mov(ecx, FieldOperand(ecx, JSObject::kElementsOffset));
  cmp(ecx, isolate()->factory()->empty_fixed_array());
  j(equal, &no_elements);

  // Second chance, the object may be using the empty slow element dictionary.
  cmp(ecx, isolate()->factory()->empty_slow_element_dictionary());
  j(not_equal, call_runtime);

  bind(&no_elements);
  mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  cmp(ecx, isolate()->factory()->null_value());
  j(not_equal, &next);
}


void MacroAssembler::TestJSArrayForAllocationMemento(
    Register receiver_reg,
    Register scratch_reg,
    Label* no_memento_found) {
  Label map_check;
  Label top_check;
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  const int kMementoMapOffset = JSArray::kSize - kHeapObjectTag;
  const int kMementoLastWordOffset =
      kMementoMapOffset + AllocationMemento::kSize - kPointerSize;

  // Bail out if the object is not in new space.
  JumpIfNotInNewSpace(receiver_reg, scratch_reg, no_memento_found);
  // If the object is in new space, we need to check whether it is on the same
  // page as the current top.
  lea(scratch_reg, Operand(receiver_reg, kMementoLastWordOffset));
  xor_(scratch_reg, Operand::StaticVariable(new_space_allocation_top));
  test(scratch_reg, Immediate(~Page::kPageAlignmentMask));
  j(zero, &top_check);
  // The object is on a different page than allocation top. Bail out if the
  // object sits on the page boundary as no memento can follow and we cannot
  // touch the memory following it.
  lea(scratch_reg, Operand(receiver_reg, kMementoLastWordOffset));
  xor_(scratch_reg, receiver_reg);
  test(scratch_reg, Immediate(~Page::kPageAlignmentMask));
  j(not_zero, no_memento_found);
  // Continue with the actual map check.
  jmp(&map_check);
  // If top is on the same page as the current object, we need to check whether
  // we are below top.
  bind(&top_check);
  lea(scratch_reg, Operand(receiver_reg, kMementoLastWordOffset));
  cmp(scratch_reg, Operand::StaticVariable(new_space_allocation_top));
  j(greater_equal, no_memento_found);
  // Memento map check.
  bind(&map_check);
  mov(scratch_reg, Operand(receiver_reg, kMementoMapOffset));
  cmp(scratch_reg, Immediate(isolate()->factory()->allocation_memento_map()));
}

void MacroAssembler::TruncatingDiv(Register dividend, int32_t divisor) {
  DCHECK(!dividend.is(eax));
  DCHECK(!dividend.is(edx));
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(static_cast<uint32_t>(divisor));
  mov(eax, Immediate(mag.multiplier));
  imul(dividend);
  bool neg = (mag.multiplier & (static_cast<uint32_t>(1) << 31)) != 0;
  if (divisor > 0 && neg) add(edx, dividend);
  if (divisor < 0 && !neg && mag.multiplier > 0) sub(edx, dividend);
  if (mag.shift > 0) sar(edx, mag.shift);
  mov(eax, dividend);
  shr(eax, 31);
  add(edx, eax);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
