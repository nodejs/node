// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include <stdint.h>

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/builtins/builtins.h"
#include "src/codegen/assembler.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/ia32/assembler-ia32.h"
#include "src/codegen/ia32/register-ia32.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/label.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/reloc-info.h"
#include "src/codegen/turbo-assembler.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/execution/isolate-data.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/factory-inl.h"
#include "src/heap/factory.h"
#include "src/heap/memory-chunk.h"
#include "src/logging/counters.h"
#include "src/objects/code.h"
#include "src/objects/contexts.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-function.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi.h"
#include "src/roots/roots-inl.h"
#include "src/roots/roots.h"
#include "src/runtime/runtime.h"
#include "src/utils/utils.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/ia32/macro-assembler-ia32.h"
#endif

#define __ ACCESS_MASM(masm)

namespace v8 {
namespace internal {

Operand StackArgumentsAccessor::GetArgumentOperand(int index) const {
  DCHECK_GE(index, 0);
  // arg[0] = esp + kPCOnStackSize;
  // arg[i] = arg[0] + i * kSystemPointerSize;
  return Operand(esp, kPCOnStackSize + index * kSystemPointerSize);
}

// -------------------------------------------------------------------------
// MacroAssembler implementation.

void TurboAssembler::InitializeRootRegister() {
  ASM_CODE_COMMENT(this);
  ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
  Move(kRootRegister, Immediate(isolate_root));
}

Operand TurboAssembler::RootAsOperand(RootIndex index) {
  DCHECK(root_array_available());
  return Operand(kRootRegister, RootRegisterOffsetForRootIndex(index));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  ASM_CODE_COMMENT(this);
  if (root_array_available()) {
    mov(destination, RootAsOperand(index));
    return;
  }

  if (RootsTable::IsImmortalImmovable(index)) {
    Handle<Object> object = isolate()->root_handle(index);
    if (object->IsSmi()) {
      mov(destination, Immediate(Smi::cast(*object)));
      return;
    } else {
      DCHECK(object->IsHeapObject());
      mov(destination, Handle<HeapObject>::cast(object));
      return;
    }
  }

  ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
  lea(destination,
      Operand(isolate_root.address(), RelocInfo::EXTERNAL_REFERENCE));
  mov(destination, Operand(destination, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::CompareRoot(Register with, Register scratch,
                                 RootIndex index) {
  ASM_CODE_COMMENT(this);
  if (root_array_available()) {
    CompareRoot(with, index);
  } else {
    ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
    lea(scratch,
        Operand(isolate_root.address(), RelocInfo::EXTERNAL_REFERENCE));
    cmp(with, Operand(scratch, RootRegisterOffsetForRootIndex(index)));
  }
}

void TurboAssembler::CompareRoot(Register with, RootIndex index) {
  ASM_CODE_COMMENT(this);
  if (root_array_available()) {
    cmp(with, RootAsOperand(index));
    return;
  }

  DCHECK(RootsTable::IsImmortalImmovable(index));
  Handle<Object> object = isolate()->root_handle(index);
  if (object->IsHeapObject()) {
    cmp(with, Handle<HeapObject>::cast(object));
  } else {
    cmp(with, Immediate(Smi::cast(*object)));
  }
}

void MacroAssembler::PushRoot(RootIndex index) {
  ASM_CODE_COMMENT(this);
  if (root_array_available()) {
    DCHECK(RootsTable::IsImmortalImmovable(index));
    push(RootAsOperand(index));
    return;
  }

  // TODO(v8:6666): Add a scratch register or remove all uses.
  DCHECK(RootsTable::IsImmortalImmovable(index));
  Handle<Object> object = isolate()->root_handle(index);
  if (object->IsHeapObject()) {
    Push(Handle<HeapObject>::cast(object));
  } else {
    Push(Smi::cast(*object));
  }
}

void MacroAssembler::CompareRange(Register value, unsigned lower_limit,
                                  unsigned higher_limit, Register scratch) {
  ASM_CODE_COMMENT(this);
  DCHECK_LT(lower_limit, higher_limit);
  if (lower_limit != 0) {
    lea(scratch, Operand(value, 0u - lower_limit));
    cmp(scratch, Immediate(higher_limit - lower_limit));
  } else {
    cmp(value, Immediate(higher_limit));
  }
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit, Register scratch,
                                     Label* on_in_range,
                                     Label::Distance near_jump) {
  CompareRange(value, lower_limit, higher_limit, scratch);
  j(below_equal, on_in_range, near_jump);
}

void TurboAssembler::PushArray(Register array, Register size, Register scratch,
                               PushArrayOrder order) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(array, size, scratch));
  Register counter = scratch;
  Label loop, entry;
  if (order == PushArrayOrder::kReverse) {
    mov(counter, 0);
    jmp(&entry);
    bind(&loop);
    Push(Operand(array, counter, times_system_pointer_size, 0));
    inc(counter);
    bind(&entry);
    cmp(counter, size);
    j(less, &loop, Label::kNear);
  } else {
    mov(counter, size);
    jmp(&entry);
    bind(&loop);
    Push(Operand(array, counter, times_system_pointer_size, 0));
    bind(&entry);
    dec(counter);
    j(greater_equal, &loop, Label::kNear);
  }
}

Operand TurboAssembler::ExternalReferenceAsOperand(ExternalReference reference,
                                                   Register scratch) {
  if (root_array_available() && options().enable_root_relative_access) {
    intptr_t delta =
        RootRegisterOffsetForExternalReference(isolate(), reference);
    return Operand(kRootRegister, delta);
  }
  if (root_array_available() && options().isolate_independent_code) {
    if (IsAddressableThroughRootRegister(isolate(), reference)) {
      // Some external references can be efficiently loaded as an offset from
      // kRootRegister.
      intptr_t offset =
          RootRegisterOffsetForExternalReference(isolate(), reference);
      return Operand(kRootRegister, offset);
    } else {
      // Otherwise, do a memory load from the external reference table.
      mov(scratch, Operand(kRootRegister,
                           RootRegisterOffsetForExternalReferenceTableEntry(
                               isolate(), reference)));
      return Operand(scratch, 0);
    }
  }
  Move(scratch, Immediate(reference));
  return Operand(scratch, 0);
}

// TODO(v8:6666): If possible, refactor into a platform-independent function in
// TurboAssembler.
Operand TurboAssembler::ExternalReferenceAddressAsOperand(
    ExternalReference reference) {
  DCHECK(root_array_available());
  DCHECK(options().isolate_independent_code);
  return Operand(
      kRootRegister,
      RootRegisterOffsetForExternalReferenceTableEntry(isolate(), reference));
}

// TODO(v8:6666): If possible, refactor into a platform-independent function in
// TurboAssembler.
Operand TurboAssembler::HeapObjectAsOperand(Handle<HeapObject> object) {
  DCHECK(root_array_available());

  Builtin builtin;
  RootIndex root_index;
  if (isolate()->roots_table().IsRootHandle(object, &root_index)) {
    return RootAsOperand(root_index);
  } else if (isolate()->builtins()->IsBuiltinHandle(object, &builtin)) {
    return Operand(kRootRegister, RootRegisterOffsetForBuiltin(builtin));
  } else if (object.is_identical_to(code_object_) &&
             Builtins::IsBuiltinId(maybe_builtin_)) {
    return Operand(kRootRegister, RootRegisterOffsetForBuiltin(maybe_builtin_));
  } else {
    // Objects in the constants table need an additional indirection, which
    // cannot be represented as a single Operand.
    UNREACHABLE();
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  ASM_CODE_COMMENT(this);
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  mov(destination,
      FieldOperand(destination, FixedArray::OffsetOfElementAt(constant_index)));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  ASM_CODE_COMMENT(this);
  DCHECK(is_int32(offset));
  DCHECK(root_array_available());
  if (offset == 0) {
    mov(destination, kRootRegister);
  } else {
    lea(destination, Operand(kRootRegister, static_cast<int32_t>(offset)));
  }
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  ASM_CODE_COMMENT(this);
  DCHECK(root_array_available());
  mov(destination, Operand(kRootRegister, offset));
}

void TurboAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  // TODO(jgruber): Add support for enable_root_relative_access.
  if (root_array_available() && options().isolate_independent_code) {
    IndirectLoadExternalReference(destination, source);
    return;
  }
  mov(destination, Immediate(source));
}

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion) const {
  int bytes = 0;
  RegList saved_regs = kCallerSaved - exclusion;
  bytes += kSystemPointerSize * saved_regs.Count();

  if (fp_mode == SaveFPRegsMode::kSave) {
    // Count all XMM registers except XMM0.
    bytes += kStackSavedSavedFPSize * (XMMRegister::kNumRegisters - 1);
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                    Register exclusion) {
  ASM_CODE_COMMENT(this);
  // We don't allow a GC in a write barrier slow path so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  int bytes = 0;
  RegList saved_regs = kCallerSaved - exclusion;
  for (Register reg : saved_regs) {
    push(reg);
    bytes += kSystemPointerSize;
  }

  if (fp_mode == SaveFPRegsMode::kSave) {
    // Save all XMM registers except XMM0.
    const int delta = kStackSavedSavedFPSize * (XMMRegister::kNumRegisters - 1);
    AllocateStackSpace(delta);
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
#if V8_ENABLE_WEBASSEMBLY
      Movdqu(Operand(esp, (i - 1) * kStackSavedSavedFPSize), reg);
#else
      Movsd(Operand(esp, (i - 1) * kStackSavedSavedFPSize), reg);
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    bytes += delta;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion) {
  ASM_CODE_COMMENT(this);
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    // Restore all XMM registers except XMM0.
    const int delta = kStackSavedSavedFPSize * (XMMRegister::kNumRegisters - 1);
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
#if V8_ENABLE_WEBASSEMBLY
      Movdqu(reg, Operand(esp, (i - 1) * kStackSavedSavedFPSize));
#else
      Movsd(reg, Operand(esp, (i - 1) * kStackSavedSavedFPSize));
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    add(esp, Immediate(delta));
    bytes += delta;
  }

  RegList saved_regs = kCallerSaved - exclusion;
  for (Register reg : base::Reversed(saved_regs)) {
    pop(reg);
    bytes += kSystemPointerSize;
  }

  return bytes;
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register slot_address,
                                      SaveFPRegsMode save_fp,
                                      SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  lea(slot_address, FieldOperand(object, offset));
  if (v8_flags.debug_code) {
    Label ok;
    test_b(slot_address, Immediate(kTaggedSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, slot_address, value, save_fp, SmiCheck::kOmit);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (v8_flags.debug_code) {
    mov(value, Immediate(base::bit_cast<int32_t>(kZapValue)));
    mov(slot_address, Immediate(base::bit_cast<int32_t>(kZapValue)));
  }
}

void TurboAssembler::MaybeSaveRegisters(RegList registers) {
  for (Register reg : registers) {
    push(reg);
  }
}

void TurboAssembler::MaybeRestoreRegisters(RegList registers) {
  for (Register reg : base::Reversed(registers)) {
    pop(reg);
  }
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object,
                                             Register slot_address,
                                             SaveFPRegsMode fp_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  push(object);
  push(slot_address);
  pop(slot_address_parameter);
  pop(object_parameter);

  Call(isolate()->builtins()->code_handle(
           Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
       RelocInfo::CODE_TARGET);

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStubSaveRegisters(Register object,
                                                      Register slot_address,
                                                      SaveFPRegsMode fp_mode,
                                                      StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  push(object);
  push(slot_address);
  pop(slot_address_parameter);
  pop(object_parameter);

  CallRecordWriteStub(object_parameter, slot_address_parameter, fp_mode, mode);

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(Register object, Register slot_address,
                                         SaveFPRegsMode fp_mode,
                                         StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  // Use CallRecordWriteStubSaveRegisters if the object and slot registers
  // need to be caller saved.
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
#if V8_ENABLE_WEBASSEMBLY
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    // Use {wasm_call} for direct Wasm call within a module.
    auto wasm_target = wasm::WasmCode::GetRecordWriteStub(fp_mode);
    wasm_call(wasm_target, RelocInfo::WASM_STUB_CALL);
#else
  if (false) {
#endif
  } else {
    Builtin builtin = Builtins::GetRecordWriteStub(fp_mode);
    CallBuiltin(builtin);
  }
}

void MacroAssembler::RecordWrite(Register object, Register slot_address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, value, slot_address));
  AssertNotSmi(object);

  if (v8_flags.disable_write_barriers) {
    return;
  }

  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Verify slot_address");
    Label ok;
    cmp(value, Operand(slot_address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  if (smi_check == SmiCheck::kInline) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done, Label::kNear);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
                zero, &done, Label::kNear);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, zero, &done,
                Label::kNear);
  RecordComment("CheckPageFlag]");

  CallRecordWriteStub(object, slot_address, fp_mode);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Clobber slot_address and value");
    mov(slot_address, Immediate(base::bit_cast<int32_t>(kZapValue)));
    mov(value, Immediate(base::bit_cast<int32_t>(kZapValue)));
  }
}

void TurboAssembler::Cvtsi2ss(XMMRegister dst, Operand src) {
  xorps(dst, dst);
  cvtsi2ss(dst, src);
}

void TurboAssembler::Cvtsi2sd(XMMRegister dst, Operand src) {
  xorpd(dst, dst);
  cvtsi2sd(dst, src);
}

void TurboAssembler::Cvtui2ss(XMMRegister dst, Operand src, Register tmp) {
  Label done;
  Register src_reg = src.is_reg_only() ? src.reg() : tmp;
  if (src_reg == tmp) mov(tmp, src);
  cvtsi2ss(dst, src_reg);
  test(src_reg, src_reg);
  j(positive, &done, Label::kNear);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src_reg != tmp) mov(tmp, src_reg);
  shr(tmp, 1);
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  Label msb_not_set;
  j(not_carry, &msb_not_set, Label::kNear);
  or_(tmp, Immediate(1));
  bind(&msb_not_set);
  cvtsi2ss(dst, tmp);
  addss(dst, dst);
  bind(&done);
}

void TurboAssembler::Cvttss2ui(Register dst, Operand src, XMMRegister tmp) {
  Label done;
  cvttss2si(dst, src);
  test(dst, dst);
  j(positive, &done);
  Move(tmp, static_cast<float>(INT32_MIN));
  addss(tmp, src);
  cvttss2si(dst, tmp);
  or_(dst, Immediate(0x80000000));
  bind(&done);
}

void TurboAssembler::Cvtui2sd(XMMRegister dst, Operand src, Register scratch) {
  Label done;
  cmp(src, Immediate(0));
  ExternalReference uint32_bias = ExternalReference::address_of_uint32_bias();
  Cvtsi2sd(dst, src);
  j(not_sign, &done, Label::kNear);
  addsd(dst, ExternalReferenceAsOperand(uint32_bias, scratch));
  bind(&done);
}

void TurboAssembler::Cvttsd2ui(Register dst, Operand src, XMMRegister tmp) {
  Move(tmp, -2147483648.0);
  addsd(tmp, src);
  cvttsd2si(dst, tmp);
  add(dst, Immediate(0x80000000));
}

void TurboAssembler::ShlPair(Register high, Register low, uint8_t shift) {
  DCHECK_GE(63, shift);
  if (shift >= 32) {
    mov(high, low);
    if (shift != 32) shl(high, shift - 32);
    xor_(low, low);
  } else {
    shld(high, low, shift);
    shl(low, shift);
  }
}

void TurboAssembler::ShlPair_cl(Register high, Register low) {
  ASM_CODE_COMMENT(this);
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
  DCHECK_GE(63, shift);
  if (shift >= 32) {
    mov(low, high);
    if (shift != 32) shr(low, shift - 32);
    xor_(high, high);
  } else {
    shrd(low, high, shift);
    shr(high, shift);
  }
}

void TurboAssembler::ShrPair_cl(Register high, Register low) {
  ASM_CODE_COMMENT(this);
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
  ASM_CODE_COMMENT(this);
  DCHECK_GE(63, shift);
  if (shift >= 32) {
    mov(low, high);
    if (shift != 32) sar(low, shift - 32);
    sar(high, 31);
  } else {
    shrd(low, high, shift);
    sar(high, shift);
  }
}

void TurboAssembler::SarPair_cl(Register high, Register low) {
  ASM_CODE_COMMENT(this);
  shrd_cl(low, high);
  sar_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  sar(high, 31);
  bind(&done);
}

void TurboAssembler::LoadMap(Register destination, Register object) {
  mov(destination, FieldOperand(object, HeapObject::kMapOffset));
}

void MacroAssembler::CmpObjectType(Register heap_object, InstanceType type,
                                   Register map) {
  ASM_CODE_COMMENT(this);
  LoadMap(map, heap_object);
  CmpInstanceType(map, type);
}

void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::CmpInstanceTypeRange(Register map,
                                          Register instance_type_out,
                                          Register scratch,
                                          InstanceType lower_limit,
                                          InstanceType higher_limit) {
  ASM_CODE_COMMENT(this);
  DCHECK_LT(lower_limit, higher_limit);
  movzx_w(instance_type_out, FieldOperand(map, Map::kInstanceTypeOffset));
  CompareRange(instance_type_out, lower_limit, higher_limit, scratch);
}

void MacroAssembler::TestCodeTIsMarkedForDeoptimization(Register codet,
                                                        Register scratch) {
  mov(scratch, FieldOperand(codet, Code::kCodeDataContainerOffset));
  test(FieldOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset),
       Immediate(1 << Code::kMarkedForDeoptimizationBit));
}

Immediate MacroAssembler::ClearedValue() const {
  return Immediate(
      static_cast<int32_t>(HeapObjectReference::ClearedValue(isolate()).ptr()));
}

namespace {

void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                               Register optimized_code_entry) {
  // ----------- S t a t e -------------
  //  -- eax : actual argument count
  //  -- edx : new target (preserved for callee if needed, and caller)
  //  -- edi : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  ASM_CODE_COMMENT(masm);
  DCHECK(!AreAliased(edx, edi, optimized_code_entry));

  Register closure = edi;
  __ Push(eax);
  __ Push(edx);

  Label heal_optimized_code_slot;

  // If the optimized code is cleared, go to runtime to update the optimization
  // marker field.
  __ LoadWeakValue(optimized_code_entry, &heal_optimized_code_slot);

  // Check if the optimized code is marked for deopt. If it is, bailout to a
  // given label.
  __ TestCodeTIsMarkedForDeoptimization(optimized_code_entry, eax);
  __ j(not_zero, &heal_optimized_code_slot);

  // Optimized code is good, get it into the closure and link the closure
  // into the optimized functions list, then tail call the optimized code.
  __ Push(optimized_code_entry);
  __ ReplaceClosureCodeWithOptimizedCode(optimized_code_entry, closure, edx,
                                         ecx);
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ Pop(optimized_code_entry);
  __ LoadCodeObjectEntry(ecx, optimized_code_entry);
  __ Pop(edx);
  __ Pop(eax);
  __ jmp(ecx);

  // Optimized code slot contains deoptimized code or code is cleared and
  // optimized code marker isn't updated. Evict the code, update the marker
  // and re-enter the closure's code.
  __ bind(&heal_optimized_code_slot);
  __ Pop(edx);
  __ Pop(eax);
  __ GenerateTailCallToReturnedCode(Runtime::kHealOptimizedCodeSlot);
}

}  // namespace

#ifdef V8_ENABLE_DEBUG_CODE
void MacroAssembler::AssertFeedbackVector(Register object, Register scratch) {
  if (v8_flags.debug_code) {
    CmpObjectType(object, FEEDBACK_VECTOR_TYPE, scratch);
    Assert(equal, AbortReason::kExpectedFeedbackVector);
  }
}
#endif  // V8_ENABLE_DEBUG_CODE

void MacroAssembler::ReplaceClosureCodeWithOptimizedCode(
    Register optimized_code, Register closure, Register value,
    Register slot_address) {
  ASM_CODE_COMMENT(this);
  // Store the optimized code in the closure.
  mov(FieldOperand(closure, JSFunction::kCodeOffset), optimized_code);
  mov(value, optimized_code);  // Write barrier clobbers slot_address below.
  RecordWriteField(closure, JSFunction::kCodeOffset, value, slot_address,
                   SaveFPRegsMode::kIgnore, SmiCheck::kOmit);
}

void MacroAssembler::GenerateTailCallToReturnedCode(
    Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- eax : actual argument count
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  ASM_CODE_COMMENT(this);
  {
    FrameScope scope(this, StackFrame::INTERNAL);
    // Push a copy of the target function, the new target and the actual
    // argument count.
    push(kJavaScriptCallTargetRegister);
    push(kJavaScriptCallNewTargetRegister);
    SmiTag(kJavaScriptCallArgCountRegister);
    push(kJavaScriptCallArgCountRegister);
    // Function is also the parameter to the runtime call.
    push(kJavaScriptCallTargetRegister);

    CallRuntime(function_id, 1);
    mov(ecx, eax);

    // Restore target function, new target and actual argument count.
    pop(kJavaScriptCallArgCountRegister);
    SmiUntag(kJavaScriptCallArgCountRegister);
    pop(kJavaScriptCallNewTargetRegister);
    pop(kJavaScriptCallTargetRegister);
  }

  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  JumpCodeObject(ecx);
}

// Read off the flags in the feedback vector and check if there
// is optimized code or a tiering state that needs to be processed.
// Registers flags and feedback_vector must be aliased.
void MacroAssembler::LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
    Register flags, XMMRegister saved_feedback_vector,
    CodeKind current_code_kind, Label* flags_need_processing) {
  ASM_CODE_COMMENT(this);
  DCHECK(CodeKindCanTierUp(current_code_kind));
  Register feedback_vector = flags;

  // Store feedback_vector. We may need it if we need to load the optimize code
  // slot entry.
  movd(saved_feedback_vector, feedback_vector);
  mov_w(flags, FieldOperand(feedback_vector, FeedbackVector::kFlagsOffset));

  // Check if there is optimized code or a tiering state that needes to be
  // processed.
  uint32_t kFlagsMask = FeedbackVector::kFlagsTieringStateIsAnyRequested |
                        FeedbackVector::kFlagsMaybeHasTurbofanCode |
                        FeedbackVector::kFlagsLogNextExecution;
  if (current_code_kind != CodeKind::MAGLEV) {
    kFlagsMask |= FeedbackVector::kFlagsMaybeHasMaglevCode;
  }
  test_w(flags, Immediate(kFlagsMask));
  j(not_zero, flags_need_processing);
}

void MacroAssembler::MaybeOptimizeCodeOrTailCallOptimizedCodeSlot(
    Register flags, XMMRegister saved_feedback_vector) {
  ASM_CODE_COMMENT(this);
  Label maybe_has_optimized_code, maybe_needs_logging;
  // Check if optimized code is available.
  test(flags, Immediate(FeedbackVector::kFlagsTieringStateIsAnyRequested));
  j(zero, &maybe_needs_logging);

  GenerateTailCallToReturnedCode(Runtime::kCompileOptimized);

  bind(&maybe_needs_logging);
  test(flags, Immediate(FeedbackVector::LogNextExecutionBit::kMask));
  j(zero, &maybe_has_optimized_code);
  GenerateTailCallToReturnedCode(Runtime::kFunctionLogNextExecution);

  bind(&maybe_has_optimized_code);
  Register optimized_code_entry = flags;
  Register feedback_vector = flags;
  movd(feedback_vector, saved_feedback_vector);  // Restore feedback vector.
  mov(optimized_code_entry,
      FieldOperand(feedback_vector, FeedbackVector::kMaybeOptimizedCodeOffset));
  TailCallOptimizedCodeSlot(this, optimized_code_entry);
}

#ifdef V8_ENABLE_DEBUG_CODE
void MacroAssembler::AssertSmi(Register object) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(equal, AbortReason::kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAConstructor);
    Push(object);
    LoadMap(object, object);
    test_b(FieldOperand(object, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsConstructorBit::kMask));
    Pop(object);
    Check(not_zero, AbortReason::kOperandIsNotAConstructor);
  }
}

void MacroAssembler::AssertFunction(Register object, Register scratch) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
    Push(object);
    LoadMap(object, object);
    CmpInstanceTypeRange(object, scratch, scratch, FIRST_JS_FUNCTION_TYPE,
                         LAST_JS_FUNCTION_TYPE);
    Pop(object);
    Check(below_equal, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertCallableFunction(Register object, Register scratch) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
    Push(object);
    LoadMap(object, object);
    CmpInstanceTypeRange(object, scratch, scratch,
                         FIRST_CALLABLE_JS_FUNCTION_TYPE,
                         LAST_CALLABLE_JS_FUNCTION_TYPE);
    Pop(object);
    Check(below_equal, AbortReason::kOperandIsNotACallableFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);

  test(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  {
    Push(object);
    Register map = object;

    LoadMap(map, object);

    // Check if JSGeneratorObject
    CmpInstanceTypeRange(map, map, map, FIRST_JS_GENERATOR_OBJECT_TYPE,
                         LAST_JS_GENERATOR_OBJECT_TYPE);
    Pop(object);
  }

  Check(below_equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, scratch, RootIndex::kUndefinedValue);
    j(equal, &done_checking);
    LoadRoot(scratch, RootIndex::kAllocationSiteWithWeakNextMap);
    cmp(FieldOperand(object, 0), scratch);
    Assert(equal, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

void MacroAssembler::AssertNotSmi(Register object) {
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT(this);
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmi);
  }
}

void TurboAssembler::Assert(Condition cc, AbortReason reason) {
  if (v8_flags.debug_code) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (v8_flags.debug_code) Abort(reason);
}
#endif  // V8_ENABLE_DEBUG_CODE

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::Prologue() {
  ASM_CODE_COMMENT(this);
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(kContextRegister);                 // Callee's context.
  push(kJSFunctionRegister);              // Callee's JS function.
  push(kJavaScriptCallArgCountRegister);  // Actual argument count.
}

void TurboAssembler::DropArguments(Register count, ArgumentsCountType type,
                                   ArgumentsCountMode mode) {
  int receiver_bytes =
      (mode == kCountExcludesReceiver) ? kSystemPointerSize : 0;
  switch (type) {
    case kCountIsInteger: {
      lea(esp, Operand(esp, count, times_system_pointer_size, receiver_bytes));
      break;
    }
    case kCountIsSmi: {
      static_assert(kSmiTagSize == 1 && kSmiTag == 0);
      // SMIs are stored shifted left by 1 byte with the tag being 0.
      // This is equivalent to multiplying by 2. To convert SMIs to bytes we
      // can therefore just multiply the stored value by half the system pointer
      // size.
      lea(esp,
          Operand(esp, count, times_half_system_pointer_size, receiver_bytes));
      break;
    }
    case kCountIsBytes: {
      if (receiver_bytes == 0) {
        add(esp, count);
      } else {
        lea(esp, Operand(esp, count, times_1, receiver_bytes));
      }
      break;
    }
  }
}

void TurboAssembler::DropArguments(Register count, Register scratch,
                                   ArgumentsCountType type,
                                   ArgumentsCountMode mode) {
  DCHECK(!AreAliased(count, scratch));
  PopReturnAddressTo(scratch);
  DropArguments(count, type, mode);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::DropArgumentsAndPushNewReceiver(Register argc,
                                                     Register receiver,
                                                     Register scratch,
                                                     ArgumentsCountType type,
                                                     ArgumentsCountMode mode) {
  DCHECK(!AreAliased(argc, receiver, scratch));
  PopReturnAddressTo(scratch);
  DropArguments(argc, type, mode);
  Push(receiver);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::DropArgumentsAndPushNewReceiver(Register argc,
                                                     Operand receiver,
                                                     Register scratch,
                                                     ArgumentsCountType type,
                                                     ArgumentsCountMode mode) {
  DCHECK(!AreAliased(argc, scratch));
  DCHECK(!receiver.is_reg(scratch));
  PopReturnAddressTo(scratch);
  DropArguments(argc, type, mode);
  Push(receiver);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  push(ebp);
  mov(ebp, esp);
  if (!StackFrame::IsJavaScript(type)) {
    Push(Immediate(StackFrame::TypeToMarker(type)));
  }
#if V8_ENABLE_WEBASSEMBLY
  if (type == StackFrame::WASM) Push(kWasmInstanceRegister);
#endif  // V8_ENABLE_WEBASSEMBLY
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  if (v8_flags.debug_code && !StackFrame::IsJavaScript(type)) {
    cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
        Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
  }
  leave();
}

#ifdef V8_OS_WIN
void TurboAssembler::AllocateStackSpace(Register bytes_scratch) {
  ASM_CODE_COMMENT(this);
  // In windows, we cannot increment the stack size by more than one page
  // (minimum page size is 4KB) without accessing at least one byte on the
  // page. Check this:
  // https://msdn.microsoft.com/en-us/library/aa227153(v=vs.60).aspx.
  Label check_offset;
  Label touch_next_page;
  jmp(&check_offset);
  bind(&touch_next_page);
  sub(esp, Immediate(kStackPageSize));
  // Just to touch the page, before we increment further.
  mov(Operand(esp, 0), Immediate(0));
  sub(bytes_scratch, Immediate(kStackPageSize));

  bind(&check_offset);
  cmp(bytes_scratch, kStackPageSize);
  j(greater_equal, &touch_next_page);

  sub(esp, bytes_scratch);
}

void TurboAssembler::AllocateStackSpace(int bytes) {
  ASM_CODE_COMMENT(this);
  DCHECK_GE(bytes, 0);
  while (bytes >= kStackPageSize) {
    sub(esp, Immediate(kStackPageSize));
    mov(Operand(esp, 0), Immediate(0));
    bytes -= kStackPageSize;
  }
  if (bytes == 0) return;
  sub(esp, Immediate(bytes));
}
#endif

void MacroAssembler::EnterExitFramePrologue(StackFrame::Type frame_type,
                                            Register scratch) {
  ASM_CODE_COMMENT(this);
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  DCHECK_EQ(+2 * kSystemPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(+1 * kSystemPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kSystemPointerSize, ExitFrameConstants::kCallerFPOffset);
  push(ebp);
  mov(ebp, esp);

  // Reserve room for entry stack pointer.
  push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kSystemPointerSize, ExitFrameConstants::kSPOffset);
  push(Immediate(0));  // Saved entry sp, patched before call.

  static_assert(edx == kRuntimeCallFunctionRegister);
  static_assert(esi == kContextRegister);

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  ExternalReference context_address =
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
  ExternalReference c_function_address =
      ExternalReference::Create(IsolateAddressId::kCFunctionAddress, isolate());

  DCHECK(!AreAliased(scratch, ebp, esi, edx));
  mov(ExternalReferenceAsOperand(c_entry_fp_address, scratch), ebp);
  mov(ExternalReferenceAsOperand(context_address, scratch), esi);
  mov(ExternalReferenceAsOperand(c_function_address, scratch), edx);
}

void MacroAssembler::EnterExitFrameEpilogue(int argc, bool save_doubles) {
  ASM_CODE_COMMENT(this);
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space =
        XMMRegister::kNumRegisters * kDoubleSize + argc * kSystemPointerSize;
    AllocateStackSpace(space);
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(ebp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else {
    AllocateStackSpace(argc * kSystemPointerSize);
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
  ASM_CODE_COMMENT(this);
  EnterExitFramePrologue(frame_type, edi);

  // Set up argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kSystemPointerSize;
  mov(edi, eax);
  lea(esi, Operand(ebp, eax, times_system_pointer_size, offset));

  // Reserve space for argc, argv and isolate.
  EnterExitFrameEpilogue(argc, save_doubles);
}

void MacroAssembler::EnterApiExitFrame(int argc, Register scratch) {
  EnterExitFramePrologue(StackFrame::EXIT, scratch);
  EnterExitFrameEpilogue(argc, false);
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, bool pop_arguments) {
  ASM_CODE_COMMENT(this);
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
    mov(ecx, Operand(ebp, 1 * kSystemPointerSize));
    mov(ebp, Operand(ebp, 0 * kSystemPointerSize));

    // Pop the arguments and the receiver from the caller stack.
    lea(esp, Operand(esi, 1 * kSystemPointerSize));

    // Push the return address to get ready to return.
    push(ecx);
  } else {
    // Otherwise just leave the exit frame.
    leave();
  }

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  ASM_CODE_COMMENT(this);
  // Clear the top frame.
  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  mov(ExternalReferenceAsOperand(c_entry_fp_address, esi), Immediate(0));

  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address =
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
  mov(esi, ExternalReferenceAsOperand(context_address, esi));
#ifdef DEBUG
  push(eax);
  mov(ExternalReferenceAsOperand(context_address, eax),
      Immediate(Context::kInvalidContext));
  pop(eax);
#endif
}

void MacroAssembler::LeaveApiExitFrame() {
  ASM_CODE_COMMENT(this);
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue();
}

void MacroAssembler::PushStackHandler(Register scratch) {
  ASM_CODE_COMMENT(this);
  // Adjust this code if not the case.
  static_assert(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  static_assert(StackHandlerConstants::kNextOffset == 0);

  push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  push(ExternalReferenceAsOperand(handler_address, scratch));

  // Set this new handler as the current one.
  mov(ExternalReferenceAsOperand(handler_address, scratch), esp);
}

void MacroAssembler::PopStackHandler(Register scratch) {
  ASM_CODE_COMMENT(this);
  static_assert(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  pop(ExternalReferenceAsOperand(handler_address, scratch));
  add(esp, Immediate(StackHandlerConstants::kSize - kSystemPointerSize));
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  ASM_CODE_COMMENT(this);
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Move(kRuntimeCallArgCountRegister, Immediate(num_arguments));
  Move(kRuntimeCallFunctionRegister, Immediate(ExternalReference::Create(f)));
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
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
  ASM_CODE_COMMENT(this);
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    Move(kRuntimeCallArgCountRegister, Immediate(function->nargs));
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             bool builtin_exit_frame) {
  ASM_CODE_COMMENT(this);
  // Set the entry point and jump to the C entry runtime stub.
  Move(kRuntimeCallFunctionRegister, Immediate(ext));
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, SaveFPRegsMode::kIgnore,
                                          ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToOffHeapInstructionStream(Address entry) {
  jmp(entry, RelocInfo::OFF_HEAP_TARGET);
}

void MacroAssembler::CompareStackLimit(Register with, StackLimitKind kind) {
  ASM_CODE_COMMENT(this);
  DCHECK(root_array_available());
  Isolate* isolate = this->isolate();
  // Address through the root register. No load is needed.
  ExternalReference limit =
      kind == StackLimitKind::kRealStackLimit
          ? ExternalReference::address_of_real_jslimit(isolate)
          : ExternalReference::address_of_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  cmp(with, Operand(kRootRegister, offset));
}

void MacroAssembler::StackOverflowCheck(Register num_args, Register scratch,
                                        Label* stack_overflow,
                                        bool include_receiver) {
  ASM_CODE_COMMENT(this);
  DCHECK_NE(num_args, scratch);
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  ExternalReference real_stack_limit =
      ExternalReference::address_of_real_jslimit(isolate());
  // Compute the space that is left as a negative number in scratch. If
  // we already overflowed, this will be a positive number.
  mov(scratch, ExternalReferenceAsOperand(real_stack_limit, scratch));
  sub(scratch, esp);
  // TODO(victorgomes): Remove {include_receiver} and always require one extra
  // word of the stack space.
  lea(scratch, Operand(scratch, num_args, times_system_pointer_size, 0));
  if (include_receiver) {
    add(scratch, Immediate(kSystemPointerSize));
  }
  // See if we overflowed, i.e. scratch is positive.
  cmp(scratch, Immediate(0));
  // TODO(victorgomes):  Save some bytes in the builtins that use stack checks
  // by jumping to a builtin that throws the exception.
  j(greater, stack_overflow);  // Signed comparison.
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeType type) {
  if (expected_parameter_count == actual_parameter_count) return;
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(actual_parameter_count, eax);
  DCHECK_EQ(expected_parameter_count, ecx);
  Label regular_invoke;

  // If the expected parameter count is equal to the adaptor sentinel, no need
  // to push undefined value as arguments.
  if (kDontAdaptArgumentsSentinel != 0) {
    cmp(expected_parameter_count, Immediate(kDontAdaptArgumentsSentinel));
    j(equal, &regular_invoke, Label::kFar);
  }

  // If overapplication or if the actual argument count is equal to the
  // formal parameter count, no need to push extra undefined values.
  sub(expected_parameter_count, actual_parameter_count);
  j(less_equal, &regular_invoke, Label::kFar);

  // We need to preserve edx, edi, esi and ebx.
  movd(xmm0, edx);
  movd(xmm1, edi);
  movd(xmm2, esi);
  movd(xmm3, ebx);

  Label stack_overflow;
  StackOverflowCheck(expected_parameter_count, edx, &stack_overflow);

  Register scratch = esi;

  // Underapplication. Move the arguments already in the stack, including the
  // receiver and the return address.
  {
    Label copy, check;
    Register src = edx, dest = esp, num = edi, current = ebx;
    mov(src, esp);
    lea(scratch,
        Operand(expected_parameter_count, times_system_pointer_size, 0));
    AllocateStackSpace(scratch);
    // Extra words are the receiver (if not already included in argc) and the
    // return address (if a jump).
    int extra_words = type == InvokeType::kCall ? 0 : 1;
    lea(num, Operand(eax, extra_words));  // Number of words to copy.
    Move(current, 0);
    // Fall-through to the loop body because there are non-zero words to copy.
    bind(&copy);
    mov(scratch, Operand(src, current, times_system_pointer_size, 0));
    mov(Operand(dest, current, times_system_pointer_size, 0), scratch);
    inc(current);
    bind(&check);
    cmp(current, num);
    j(less, &copy);
    lea(edx, Operand(esp, num, times_system_pointer_size, 0));
  }

    // Fill remaining expected arguments with undefined values.
    movd(ebx, xmm3);  // Restore root.
    LoadRoot(scratch, RootIndex::kUndefinedValue);
    {
      Label loop;
      bind(&loop);
      dec(expected_parameter_count);
      mov(Operand(edx, expected_parameter_count, times_system_pointer_size, 0),
          scratch);
      j(greater, &loop, Label::kNear);
    }

    // Restore remaining registers.
    movd(esi, xmm2);
    movd(edi, xmm1);
    movd(edx, xmm0);

    jmp(&regular_invoke);

    bind(&stack_overflow);
    {
      FrameScope frame(
          this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
      CallRuntime(Runtime::kThrowStackOverflow);
      int3();  // This should be unreachable.
    }

    bind(&regular_invoke);
}

void MacroAssembler::CallDebugOnFunctionCall(Register fun, Register new_target,
                                             Register expected_parameter_count,
                                             Register actual_parameter_count) {
  ASM_CODE_COMMENT(this);
  FrameScope frame(
      this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
  SmiTag(expected_parameter_count);
  Push(expected_parameter_count);

  SmiTag(actual_parameter_count);
  Push(actual_parameter_count);
  SmiUntag(actual_parameter_count);

  if (new_target.is_valid()) {
    Push(new_target);
  }
  Push(fun);
  Push(fun);
  // Arguments are located 2 words below the base pointer.
  Operand receiver_op = Operand(ebp, kSystemPointerSize * 2);
  Push(receiver_op);
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

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());
  DCHECK_EQ(function, edi);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == edx);
  DCHECK(expected_parameter_count == ecx || expected_parameter_count == eax);
  DCHECK_EQ(actual_parameter_count, eax);

  // On function call, call into the debugger if necessary.
  Label debug_hook, continue_after_hook;
  {
    ExternalReference debug_hook_active =
        ExternalReference::debug_hook_on_function_call_address(isolate());
    push(eax);
    cmpb(ExternalReferenceAsOperand(debug_hook_active, eax), Immediate(0));
    pop(eax);
    j(not_equal, &debug_hook);
  }
  bind(&continue_after_hook);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    Move(edx, isolate()->factory()->undefined_value());
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, type);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  mov(ecx, FieldOperand(function, JSFunction::kCodeOffset));
  switch (type) {
    case InvokeType::kCall:
      CallCodeObject(ecx);
      break;
    case InvokeType::kJump:
      JumpCodeObject(ecx);
      break;
  }
  jmp(&done, Label::kNear);

  // Deferred debug hook.
  bind(&debug_hook);
  CallDebugOnFunctionCall(function, new_target, expected_parameter_count,
                          actual_parameter_count);
  jmp(&continue_after_hook);

  bind(&done);
}

void MacroAssembler::InvokeFunction(Register fun, Register new_target,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK(type == InvokeType::kJump || has_frame());

  DCHECK(fun == edi);
  mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  movzx_w(ecx,
          FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(edi, new_target, ecx, actual_parameter_count, type);
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  LoadNativeContextSlot(dst, Context::GLOBAL_PROXY_INDEX);
}

void MacroAssembler::LoadNativeContextSlot(Register destination, int index) {
  ASM_CODE_COMMENT(this);
  // Load the native context from the current context.
  LoadMap(destination, esi);
  mov(destination,
      FieldOperand(destination,
                   Map::kConstructorOrBackPointerOrNativeContextOffset));
  // Load the function from the native context.
  mov(destination, Operand(destination, Context::SlotOffset(index)));
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

void TurboAssembler::Push(Immediate value) {
  if (root_array_available() && options().isolate_independent_code) {
    if (value.is_embedded_object()) {
      Push(HeapObjectAsOperand(value.embedded_object()));
      return;
    } else if (value.is_external_reference()) {
      Push(ExternalReferenceAddressAsOperand(value.external_reference()));
      return;
    }
  }
  push(value);
}

void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(esp, Immediate(stack_elements * kSystemPointerSize));
  }
}

void TurboAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Register dst, const Immediate& src) {
  if (!src.is_heap_number_request() && src.is_zero()) {
    xor_(dst, dst);  // Shorter than mov of 32-bit immediate 0.
  } else if (src.is_external_reference()) {
    LoadAddress(dst, src.external_reference());
  } else {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Operand dst, const Immediate& src) {
  // Since there's no scratch register available, take a detour through the
  // stack.
  if (root_array_available() && options().isolate_independent_code) {
    if (src.is_embedded_object() || src.is_external_reference() ||
        src.is_heap_number_request()) {
      Push(src);
      pop(dst);
      return;
    }
  }

  if (src.is_embedded_object()) {
    mov(dst, src.embedded_object());
  } else {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Register dst, Operand src) { mov(dst, src); }

void TurboAssembler::Move(Register dst, Handle<HeapObject> src) {
  if (root_array_available() && options().isolate_independent_code) {
    IndirectLoadConstant(dst, src);
    return;
  }
  mov(dst, src);
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
      if (upper != lower) {
        Move(eax, Immediate(upper));
      }
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

void TurboAssembler::PextrdPreSse41(Register dst, XMMRegister src,
                                    uint8_t imm8) {
  if (imm8 == 0) {
    Movd(dst, src);
    return;
  }
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  AllocateStackSpace(kDoubleSize);
  movsd(Operand(esp, 0), src);
  mov(dst, Operand(esp, imm8 * kUInt32Size));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::PinsrdPreSse41(XMMRegister dst, Operand src, uint8_t imm8,
                                    uint32_t* load_pc_offset) {
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  AllocateStackSpace(kDoubleSize);
  // Write original content of {dst} to the stack.
  movsd(Operand(esp, 0), dst);
  // Overwrite the portion specified in {imm8}.
  if (src.is_reg_only()) {
    mov(Operand(esp, imm8 * kUInt32Size), src.reg());
  } else {
    movss(dst, src);
    movss(Operand(esp, imm8 * kUInt32Size), dst);
  }
  // Load back the full value into {dst}.
  movsd(dst, Operand(esp, 0));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::Lzcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsr(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  mov(dst, 63);  // 63^31 == 32
  bind(&not_zero_src);
  xor_(dst, Immediate(31));  // for x in [0..31], 31^x == 31-x.
}

void TurboAssembler::Tzcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsf(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  mov(dst, 32);  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}

void TurboAssembler::Popcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcnt(dst, src);
    return;
  }
  FATAL("no POPCNT support");
}

void MacroAssembler::LoadWeakValue(Register in_out, Label* target_if_cleared) {
  ASM_CODE_COMMENT(this);
  cmp(in_out, Immediate(kClearedWeakHeapObjectLower32));
  j(equal, target_if_cleared);

  and_(in_out, Immediate(~kWeakHeapObjectMask));
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value,
                                          Register scratch) {
  DCHECK_GT(value, 0);
  if (v8_flags.native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    Operand operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter), scratch);
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}

void MacroAssembler::EmitDecrementCounter(StatsCounter* counter, int value,
                                          Register scratch) {
  DCHECK_GT(value, 0);
  if (v8_flags.native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    Operand operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter), scratch);
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}

void TurboAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
  ASM_CODE_COMMENT(this);
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kSystemPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}

void TurboAssembler::Abort(AbortReason reason) {
  if (v8_flags.code_comments) {
    const char* msg = GetAbortReason(reason);
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    int3();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NO_FRAME_TYPE);
    PrepareCallCFunction(1, eax);
    mov(Operand(esp, 0), Immediate(static_cast<int>(reason)));
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

  Move(edx, Smi::FromInt(static_cast<int>(reason)));

  {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NO_FRAME_TYPE);
    if (root_array_available()) {
      // Generate an indirect call via builtins entry table here in order to
      // ensure that the interpreter_entry_return_pc_offset is the same for
      // InterpreterEntryTrampoline and InterpreterEntryTrampolineForProfiling
      // when v8_flags.debug_code is enabled.
      Call(EntryFromBuiltinAsOperand(Builtin::kAbort));
    } else {
      Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
    }
  }

  // will not return here
  int3();
}

void TurboAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  ASM_CODE_COMMENT(this);
  int frame_alignment = base::OS::ActivationFrameAlignment();
  if (frame_alignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    AllocateStackSpace((num_arguments + 1) * kSystemPointerSize);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    and_(esp, -frame_alignment);
    mov(Operand(esp, num_arguments * kSystemPointerSize), scratch);
  } else {
    AllocateStackSpace(num_arguments * kSystemPointerSize);
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  Move(eax, Immediate(function));
  CallCFunction(eax, num_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  ASM_CODE_COMMENT(this);
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (v8_flags.debug_code) {
    CheckStackAlignment();
  }

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  // Find two caller-saved scratch registers.
  Register pc_scratch = eax;
  Register scratch = ecx;
  if (function == eax) pc_scratch = edx;
  if (function == ecx) scratch = edx;
  PushPC();
  pop(pc_scratch);

  // See x64 code for reasoning about how to address the isolate data fields.
  DCHECK_IMPLIES(!root_array_available(), isolate() != nullptr);
  mov(root_array_available()
          ? Operand(kRootRegister, IsolateData::fast_c_call_caller_pc_offset())
          : ExternalReferenceAsOperand(
                ExternalReference::fast_c_call_caller_pc_address(isolate()),
                scratch),
      pc_scratch);
  mov(root_array_available()
          ? Operand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset())
          : ExternalReferenceAsOperand(
                ExternalReference::fast_c_call_caller_fp_address(isolate()),
                scratch),
      ebp);

  call(function);

  // We don't unset the PC; the FP is the source of truth.
  mov(root_array_available()
          ? Operand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset())
          : ExternalReferenceAsOperand(
                ExternalReference::fast_c_call_caller_fp_address(isolate()),
                scratch),
      Immediate(0));

  if (base::OS::ActivationFrameAlignment() != 0) {
    mov(esp, Operand(esp, num_arguments * kSystemPointerSize));
  } else {
    add(esp, Immediate(num_arguments * kSystemPointerSize));
  }
}

void TurboAssembler::PushPC() {
  // Push the current PC onto the stack as "return address" via calling
  // the next instruction.
  Label get_pc;
  call(&get_pc);
  bind(&get_pc);
}

void TurboAssembler::Call(Handle<Code> code_object, RelocInfo::Mode rmode) {
  ASM_CODE_COMMENT(this);
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code_object));
  Builtin builtin = Builtin::kNoBuiltinId;
  if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin)) {
    CallBuiltin(builtin);
    return;
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  ASM_CODE_COMMENT(this);
  static_assert(kSystemPointerSize == 4);
  static_assert(kSmiShiftSize == 0);
  static_assert(kSmiTagSize == 1);
  static_assert(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below (we use
  // times_half_system_pointer_size instead of times_system_pointer_size since
  // smis are already shifted by one).
  mov(builtin_index,
      Operand(kRootRegister, builtin_index, times_half_system_pointer_size,
              IsolateData::builtin_entry_table_offset()));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  ASM_CODE_COMMENT(this);
  LoadEntryFromBuiltinIndex(builtin_index);
  call(builtin_index);
}

void TurboAssembler::CallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this, CommentForOffHeapTrampoline("call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute: {
      call(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
      break;
    }
    case BuiltinCallJumpMode::kPCRelative:
      UNREACHABLE();
    case BuiltinCallJumpMode::kIndirect:
      call(EntryFromBuiltinAsOperand(builtin));
      break;
    case BuiltinCallJumpMode::kForMksnapshot: {
      Handle<CodeT> code = isolate()->builtins()->code_handle(builtin);
      call(code, RelocInfo::CODE_TARGET);
      break;
    }
  }
}

void TurboAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute: {
      jmp(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
      break;
    }
    case BuiltinCallJumpMode::kPCRelative:
      UNREACHABLE();
    case BuiltinCallJumpMode::kIndirect:
      jmp(EntryFromBuiltinAsOperand(builtin));
      break;
    case BuiltinCallJumpMode::kForMksnapshot: {
      Handle<CodeT> code = isolate()->builtins()->code_handle(builtin);
      jmp(code, RelocInfo::CODE_TARGET);
      break;
    }
  }
}

Operand TurboAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  ASM_CODE_COMMENT(this);
  return Operand(kRootRegister, IsolateData::BuiltinEntrySlotOffset(builtin));
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
  ASM_CODE_COMMENT(this);
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

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.
    test(FieldOperand(code_object, Code::kFlagsOffset),
         Immediate(Code::IsOffHeapTrampoline::kMask));
    j(not_equal, &if_code_is_off_heap);

    // Not an off-heap trampoline, the entry point is at
    // Code::raw_instruction_start().
    Move(destination, code_object);
    add(destination, Immediate(Code::kHeaderSize - kHeapObjectTag));
    jmp(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    mov(destination, FieldOperand(code_object, Code::kBuiltinIndexOffset));
    mov(destination,
        Operand(kRootRegister, destination, times_system_pointer_size,
                IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    Move(destination, code_object);
    add(destination, Immediate(Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  ASM_CODE_COMMENT(this);
  LoadCodeObjectEntry(code_object, code_object);
  call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
  ASM_CODE_COMMENT(this);
  LoadCodeObjectEntry(code_object, code_object);
  switch (jump_mode) {
    case JumpMode::kJump:
      jmp(code_object);
      return;
    case JumpMode::kPushAndReturn:
      push(code_object);
      ret(0);
      return;
  }
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  DCHECK(root_array_available());
  jmp(Operand(kRootRegister, RootRegisterOffsetForExternalReferenceTableEntry(
                                 isolate(), reference)));
}

void TurboAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode) {
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code_object));
  Builtin builtin = Builtin::kNoBuiltinId;
  if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin)) {
    TailCallBuiltin(builtin);
    return;
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  jmp(code_object, rmode);
}

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
  ASM_CODE_COMMENT(this);
  DCHECK(cc == zero || cc == not_zero);
  if (scratch == object) {
    and_(scratch, Immediate(~kPageAlignmentMask));
  } else {
    mov(scratch, Immediate(~kPageAlignmentMask));
    and_(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand(scratch, BasicMemoryChunk::kFlagsOffset), Immediate(mask));
  } else {
    test(Operand(scratch, BasicMemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  ASM_CODE_COMMENT(this);
  // In order to get the address of the current instruction, we first need
  // to use a call and then use a pop, thus pushing the return address to
  // the stack and then popping it into the register.
  Label current;
  call(&current);
  int pc = pc_offset();
  bind(&current);
  pop(dst);
  if (pc != 0) {
    sub(dst, Immediate(pc));
  }
}

void TurboAssembler::CallForDeoptimization(Builtin target, int, Label* exit,
                                           DeoptimizeKind kind, Label* ret,
                                           Label*) {
  ASM_CODE_COMMENT(this);
  CallBuiltin(target);
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void TurboAssembler::Trap() { int3(); }
void TurboAssembler::DebugBreak() { int3(); }

}  // namespace internal
}  // namespace v8

#undef __

#endif  // V8_TARGET_ARCH_IA32
