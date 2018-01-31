// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frame-constants.h"
#include "src/frames-inl.h"
#include "src/runtime/runtime.h"

#include "src/ia32/assembler-ia32-inl.h"
#include "src/ia32/macro-assembler-ia32.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MacroAssembler implementation.

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

void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  if (isolate()->heap()->RootCanBeTreatedAsConstant(index)) {
    Handle<Object> object = isolate()->heap()->root_handle(index);
    if (object->IsHeapObject()) {
      mov(destination, Handle<HeapObject>::cast(object));
    } else {
      mov(destination, Immediate(Smi::cast(*object)));
    }
    return;
  }
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(destination, Immediate(index));
  mov(destination, Operand::StaticArray(destination,
                                        times_pointer_size,
                                        roots_array_start));
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
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    cmp(with, Handle<HeapObject>::cast(object));
  } else {
    cmp(with, Immediate(Smi::cast(*object)));
  }
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    cmp(with, Handle<HeapObject>::cast(object));
  } else {
    cmp(with, Immediate(Smi::cast(*object)));
  }
}

void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    Push(Handle<HeapObject>::cast(object));
  } else {
    Push(Smi::cast(*object));
  }
}

static constexpr Register saved_regs[] = {eax, ecx, edx};

static constexpr int kNumberOfSavedRegs = sizeof(saved_regs) / sizeof(Register);

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      bytes += kPointerSize;
    }
  }

  if (fp_mode == kSaveFPRegs) {
    // Count all XMM registers except XMM0.
    bytes += kDoubleSize * (XMMRegister::kNumRegisters - 1);
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      push(reg);
      bytes += kPointerSize;
    }
  }

  if (fp_mode == kSaveFPRegs) {
    // Save all XMM registers except XMM0.
    int delta = kDoubleSize * (XMMRegister::kNumRegisters - 1);
    sub(esp, Immediate(delta));
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(esp, (i - 1) * kDoubleSize), reg);
    }
    bytes += delta;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    // Restore all XMM registers except XMM0.
    int delta = kDoubleSize * (XMMRegister::kNumRegisters - 1);
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(reg, Operand(esp, (i - 1) * kDoubleSize));
    }
    add(esp, Immediate(delta));
    bytes += delta;
  }

  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      pop(reg);
      bytes += kPointerSize;
    }
  }

  return bytes;
}

void TurboAssembler::SlowTruncateToIDelayed(Zone* zone, Register result_reg) {
  CallStubDelayed(new (zone) DoubleToIStub(nullptr, result_reg));
}

void MacroAssembler::DoubleToI(Register result_reg, XMMRegister input_reg,
                               XMMRegister scratch,
                               MinusZeroMode minus_zero_mode,
                               Label* lost_precision, Label* is_nan,
                               Label* minus_zero, Label::Distance dst) {
  DCHECK(input_reg != scratch);
  cvttsd2si(result_reg, Operand(input_reg));
  Cvtsi2sd(scratch, Operand(result_reg));
  ucomisd(scratch, input_reg);
  j(not_equal, lost_precision, dst);
  j(parity_even, is_nan, dst);
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    Label done;
    // The integer converted back is equal to the original. We
    // only have to test if we got -0 as an input.
    test(result_reg, Operand(result_reg));
    j(not_zero, &done, Label::kNear);
    movmskpd(result_reg, input_reg);
    // Bit 0 contains the sign of the double in input_reg.
    // If input was positive, we are ok and return 0, otherwise
    // jump to minus_zero.
    and_(result_reg, 1);
    j(not_zero, minus_zero, dst);
    bind(&done);
  }
}

void TurboAssembler::LoadUint32(XMMRegister dst, const Operand& src) {
  Label done;
  cmp(src, Immediate(0));
  ExternalReference uint32_bias = ExternalReference::address_of_uint32_bias();
  Cvtsi2sd(dst, src);
  j(not_sign, &done, Label::kNear);
  addsd(dst, Operand::StaticVariable(uint32_bias));
  bind(&done);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register dst,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
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
    test_b(dst, Immediate(kPointerSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(dst, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      push(Register::from_code(i));
    }
  }
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = Register::kNumRegisters - 1; i >= 0; --i) {
    if ((registers >> i) & 1u) {
      pop(Register::from_code(i));
    }
  }
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
      Immediate(ExternalReference::isolate_address(isolate())));
  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(callable.code(), RelocInfo::CODE_TARGET);

  RestoreRegisters(registers);
}

void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(object != value);
  DCHECK(object != address);
  DCHECK(value != address);
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

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);

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

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  mov(ebx, Operand::StaticVariable(restart_fp));
  test(ebx, ebx);
  j(not_zero, BUILTIN_CODE(isolate(), FrameDropperTrampoline),
    RelocInfo::CODE_TARGET);
}

void TurboAssembler::Cvtsi2sd(XMMRegister dst, const Operand& src) {
  xorps(dst, dst);
  cvtsi2sd(dst, src);
}

void TurboAssembler::Cvtui2ss(XMMRegister dst, Register src, Register tmp) {
  Label msb_set_src;
  Label jmp_return;
  test(src, src);
  j(sign, &msb_set_src, Label::kNear);
  cvtsi2ss(dst, src);
  jmp(&jmp_return, Label::kNear);
  bind(&msb_set_src);
  mov(tmp, src);
  shr(src, 1);
  // Recover the least significant bit to avoid rounding errors.
  and_(tmp, Immediate(1));
  or_(src, tmp);
  cvtsi2ss(dst, src);
  addss(dst, dst);
  bind(&jmp_return);
}

void TurboAssembler::ShlPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(high, low);
    shl(high, shift - 32);
    xor_(low, low);
  } else {
    shld(high, low, shift);
    shl(low, shift);
  }
}

void TurboAssembler::ShlPair_cl(Register high, Register low) {
  shld_cl(high, low);
  shl_cl(low);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(high, low);
  xor_(low, low);
  bind(&done);
}

void TurboAssembler::ShrPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    shr(low, shift - 32);
    xor_(high, high);
  } else {
    shrd(high, low, shift);
    shr(high, shift);
  }
}

void TurboAssembler::ShrPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  shr_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  xor_(high, high);
  bind(&done);
}

void TurboAssembler::SarPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    sar(low, shift - 32);
    sar(high, 31);
  } else {
    shrd(high, low, shift);
    sar(high, shift);
  }
}

void TurboAssembler::SarPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  sar_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  sar(high, 31);
  bind(&done);
}

void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(equal, kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertFixedArray(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAFixedArray);
    Push(object);
    CmpObjectType(object, FIXED_ARRAY_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotAFixedArray);
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
  if (!emit_debug_code()) return;

  test(object, Immediate(kSmiTagMask));
  Check(not_equal, kOperandIsASmiAndNotAGeneratorObject);

  {
    Push(object);
    Register map = object;

    // Load map
    mov(map, FieldOperand(object, HeapObject::kMapOffset));

    Label do_check;
    // Check if JSGeneratorObject
    CmpInstanceType(map, JS_GENERATOR_OBJECT_TYPE);
    j(equal, &do_check, Label::kNear);

    // Check if JSAsyncGeneratorObject
    CmpInstanceType(map, JS_ASYNC_GENERATOR_OBJECT_TYPE);

    bind(&do_check);
    Pop(object);
  }

  Check(equal, kOperandIsNotAGeneratorObject);
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

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::Prologue() {
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(esi);  // Callee's context.
  push(edi);  // Callee's JS function.
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, esp);
  push(Immediate(StackFrame::TypeToMarker(type)));
  if (type == StackFrame::INTERNAL) {
    push(Immediate(CodeObject()));
  }
  if (emit_debug_code()) {
    cmp(Operand(esp, 0), Immediate(isolate()->factory()->undefined_value()));
    Check(not_equal, kCodeObjectNotProperlyPatched);
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
        Immediate(StackFrame::TypeToMarker(type)));
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
  push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kPointerSize, ExitFrameConstants::kSPOffset);
  push(Immediate(0));  // Saved entry sp, patched before call.
  DCHECK_EQ(-3 * kPointerSize, ExitFrameConstants::kCodeOffset);
  push(Immediate(CodeObject()));  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(IsolateAddressId::kCEntryFPAddress,
                                       isolate());
  ExternalReference context_address(IsolateAddressId::kContextAddress,
                                    isolate());
  ExternalReference c_function_address(IsolateAddressId::kCFunctionAddress,
                                       isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), ebp);
  mov(Operand::StaticVariable(context_address), esi);
  mov(Operand::StaticVariable(c_function_address), ebx);
}


void MacroAssembler::EnterExitFrameEpilogue(int argc, bool save_doubles) {
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space = XMMRegister::kNumRegisters * kDoubleSize + argc * kPointerSize;
    sub(esp, Immediate(space));
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(ebp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else {
    sub(esp, Immediate(argc * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = base::OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(kFrameAlignment));
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
  // Optionally restore all XMM registers.
  if (save_doubles) {
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(reg, Operand(ebp, offset - ((i + 1) * kDoubleSize)));
    }
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

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(IsolateAddressId::kContextAddress,
                                    isolate());
  mov(esi, Operand::StaticVariable(context_address));
#ifdef DEBUG
  mov(Operand::StaticVariable(context_address), Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(IsolateAddressId::kCEntryFPAddress,
                                       isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), Immediate(0));
}

void MacroAssembler::LeaveApiExitFrame() {
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue();
}


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  // Link the current handler as the next handler.
  ExternalReference handler_address(IsolateAddressId::kHandlerAddress,
                                    isolate());
  push(Operand::StaticVariable(handler_address));

  // Set this new handler as the current one.
  mov(Operand::StaticVariable(handler_address), esp);
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(IsolateAddressId::kHandlerAddress,
                                    isolate());
  pop(Operand::StaticVariable(handler_address));
  add(esp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::CallStub(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  call(stub->GetCode(), RelocInfo::CODE_TARGET);
}

void TurboAssembler::CallStubDelayed(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  call(stub);
}

void MacroAssembler::TailCallStub(CodeStub* stub) {
  jmp(stub->GetCode(), RelocInfo::CODE_TARGET);
}

bool TurboAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame() || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
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

void TurboAssembler::CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                                        SaveFPRegsMode save_doubles) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Move(eax, Immediate(f->nargs));
  mov(ebx, Immediate(ExternalReference(f, isolate())));
  CallStubDelayed(new (zone) CEntryStub(nullptr, 1, save_doubles));
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

void TurboAssembler::PrepareForTailCall(
    const ParameterCount& callee_args_count, Register caller_args_count_reg,
    Register scratch0, Register scratch1,
    int number_of_temp_values_after_return_address) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
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
  mov(tmp_reg, Operand(ebp, StandardFrameConstants::kCallerPCOffset));
  mov(Operand(esp, number_of_temp_values_after_return_address * kPointerSize),
      tmp_reg);

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
                                    const ParameterCount& actual, Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance done_near) {
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
      DCHECK(expected.reg() == ebx);
    } else if (expected.reg() != actual.reg()) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmp(expected.reg(), actual.reg());
      j(equal, &invoke);
      DCHECK(actual.reg() == eax);
      DCHECK(expected.reg() == ebx);
    } else {
      definitely_matches = true;
      Move(eax, actual.reg());
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      call(adaptor, RelocInfo::CODE_TARGET);
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
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function == edi);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == edx);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    mov(edx, isolate()->factory()->undefined_value());
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 Label::kNear);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    mov(ecx, FieldOperand(function, JSFunction::kCodeOffset));
    add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    if (flag == CALL_FUNCTION) {
      call(ecx);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      jmp(ecx);
    }
    bind(&done);
  }
}

void MacroAssembler::InvokeFunction(Register fun, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun == edi);
  mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(ebx);
  InvokeFunctionCode(edi, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun == edi);
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  InvokeFunctionCode(edi, no_reg, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  Move(edi, function);
  InvokeFunction(edi, expected, actual, flag);
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

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the lowest encoding,
  // which means that lowest encodings are furthest away from
  // the stack pointer.
  DCHECK(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return kNumSafepointRegisters - reg_code - 1;
}

void TurboAssembler::Ret() { ret(0); }

void TurboAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    pop(scratch);
    add(esp, Immediate(bytes_dropped));
    push(scratch);
    ret(0);
  }
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(esp, Immediate(stack_elements * kPointerSize));
  }
}

void TurboAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Register dst, const Immediate& x) {
  if (!x.is_heap_object_request() && x.is_zero() &&
      RelocInfo::IsNone(x.rmode())) {
    xor_(dst, dst);  // Shorter than mov of 32-bit immediate 0.
  } else {
    mov(dst, x);
  }
}

void TurboAssembler::Move(const Operand& dst, const Immediate& x) {
  mov(dst, x);
}

void TurboAssembler::Move(Register dst, Handle<HeapObject> object) {
  mov(dst, object);
}

void TurboAssembler::Move(XMMRegister dst, uint32_t src) {
  if (src == 0) {
    pxor(dst, dst);
  } else {
    unsigned cnt = base::bits::CountPopulation(src);
    unsigned nlz = base::bits::CountLeadingZeros32(src);
    unsigned ntz = base::bits::CountTrailingZeros32(src);
    if (nlz + cnt + ntz == 32) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrld(dst, 32 - cnt);
      } else {
        pslld(dst, 32 - cnt);
        if (nlz != 0) psrld(dst, nlz);
      }
    } else {
      push(eax);
      mov(eax, Immediate(src));
      movd(dst, Operand(eax));
      pop(eax);
    }
  }
}

void TurboAssembler::Move(XMMRegister dst, uint64_t src) {
  if (src == 0) {
    pxor(dst, dst);
  } else {
    uint32_t lower = static_cast<uint32_t>(src);
    uint32_t upper = static_cast<uint32_t>(src >> 32);
    unsigned cnt = base::bits::CountPopulation(src);
    unsigned nlz = base::bits::CountLeadingZeros64(src);
    unsigned ntz = base::bits::CountTrailingZeros64(src);
    if (nlz + cnt + ntz == 64) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrlq(dst, 64 - cnt);
      } else {
        psllq(dst, 64 - cnt);
        if (nlz != 0) psrlq(dst, nlz);
      }
    } else if (lower == 0) {
      Move(dst, upper);
      psllq(dst, 32);
    } else if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatureScope scope(this, SSE4_1);
      push(eax);
      Move(eax, Immediate(lower));
      movd(dst, Operand(eax));
      Move(eax, Immediate(upper));
      pinsrd(dst, Operand(eax), 1);
      pop(eax);
    } else {
      push(Immediate(upper));
      push(Immediate(lower));
      movsd(dst, Operand(esp, 0));
      add(esp, Immediate(kDoubleSize));
    }
  }
}

void TurboAssembler::Pshuflw(XMMRegister dst, const Operand& src,
                             uint8_t shuffle) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshuflw(dst, src, shuffle);
  } else {
    pshuflw(dst, src, shuffle);
  }
}

void TurboAssembler::Pshufd(XMMRegister dst, const Operand& src,
                            uint8_t shuffle) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufd(dst, src, shuffle);
  } else {
    pshufd(dst, src, shuffle);
  }
}

void TurboAssembler::Psignb(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignb(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignb(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Psignw(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignw(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignw(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Psignd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignd(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignd(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Pshufb(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufb(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    pshufb(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Pextrb(Register dst, XMMRegister src, int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrb(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrb(dst, src, imm8);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Pextrw(Register dst, XMMRegister src, int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrw(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrw(dst, src, imm8);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Pextrd(Register dst, XMMRegister src, int8_t imm8) {
  if (imm8 == 0) {
    Movd(dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrd(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrd(dst, src, imm8);
    return;
  }
  DCHECK_LT(imm8, 4);
  pshufd(xmm0, src, imm8);
  movd(dst, xmm0);
}

void TurboAssembler::Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8,
                            bool is_64_bits) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pinsrd(dst, src, imm8);
    return;
  }
  if (is_64_bits) {
    movd(xmm0, src);
    if (imm8 == 1) {
      punpckldq(dst, xmm0);
    } else {
      DCHECK_EQ(0, imm8);
      psrlq(dst, 32);
      punpckldq(xmm0, dst);
      movaps(dst, xmm0);
    }
  } else {
    DCHECK_LT(imm8, 4);
    push(eax);
    mov(eax, src);
    pinsrw(dst, eax, imm8 * 2);
    shr(eax, 16);
    pinsrw(dst, eax, imm8 * 2 + 1);
    pop(eax);
  }
}

void TurboAssembler::Lzcnt(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsr(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(63));  // 63^31 == 32
  bind(&not_zero_src);
  xor_(dst, Immediate(31));  // for x in [0..31], 31^x == 31-x.
}

void TurboAssembler::Tzcnt(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsf(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(32));  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}

void TurboAssembler::Popcnt(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcnt(dst, src);
    return;
  }
  UNREACHABLE();
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
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
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}


void TurboAssembler::Assert(Condition cc, BailoutReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(BailoutReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void TurboAssembler::Check(Condition cc, BailoutReason reason) {
  Label L;
  j(cc, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}

void TurboAssembler::Abort(BailoutReason reason) {
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != nullptr) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    int3();
    return;
  }
#endif

  Move(edx, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // will not return here
  int3();
}


void TurboAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  if (frame_alignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    sub(esp, Immediate((num_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    and_(esp, -frame_alignment);
    mov(Operand(esp, num_arguments * kPointerSize), scratch);
  } else {
    sub(esp, Immediate(num_arguments * kPointerSize));
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  mov(eax, Immediate(function));
  CallCFunction(eax, num_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  DCHECK_LE(num_arguments, kMaxCParameters);
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


void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  if (scratch == object) {
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

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
