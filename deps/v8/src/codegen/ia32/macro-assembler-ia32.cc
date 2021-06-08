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
#include "src/codegen/interface-descriptors.h"
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
#include "src/snapshot/embedded/embedded-data.h"
#include "src/utils/utils.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/ia32/macro-assembler-ia32.h"
#endif

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
  ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
  Move(kRootRegister, Immediate(isolate_root));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  if (root_array_available()) {
    mov(destination,
        Operand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
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
  if (root_array_available()) {
    cmp(with, Operand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
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
  if (root_array_available()) {
    DCHECK(RootsTable::IsImmortalImmovable(index));
    push(Operand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
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

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit, Register scratch,
                                     Label* on_in_range,
                                     Label::Distance near_jump) {
  if (lower_limit != 0) {
    lea(scratch, Operand(value, 0u - lower_limit));
    cmp(scratch, Immediate(higher_limit - lower_limit));
  } else {
    cmp(value, Immediate(higher_limit));
  }
  j(below_equal, on_in_range, near_jump);
}

void TurboAssembler::PushArray(Register array, Register size, Register scratch,
                               PushArrayOrder order) {
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
  // TODO(jgruber): Add support for enable_root_array_delta_access.
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

  int builtin_index;
  RootIndex root_index;
  if (isolate()->roots_table().IsRootHandle(object, &root_index)) {
    return Operand(kRootRegister, RootRegisterOffsetForRootIndex(root_index));
  } else if (isolate()->builtins()->IsBuiltinHandle(object, &builtin_index)) {
    return Operand(kRootRegister,
                   RootRegisterOffsetForBuiltinIndex(builtin_index));
  } else if (object.is_identical_to(code_object_) &&
             Builtins::IsBuiltinId(maybe_builtin_index_)) {
    return Operand(kRootRegister,
                   RootRegisterOffsetForBuiltinIndex(maybe_builtin_index_));
  } else {
    // Objects in the constants table need an additional indirection, which
    // cannot be represented as a single Operand.
    UNREACHABLE();
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  mov(destination,
      FieldOperand(destination, FixedArray::OffsetOfElementAt(constant_index)));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  DCHECK(is_int32(offset));
  DCHECK(root_array_available());
  if (offset == 0) {
    mov(destination, kRootRegister);
  } else {
    lea(destination, Operand(kRootRegister, static_cast<int32_t>(offset)));
  }
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  DCHECK(root_array_available());
  mov(destination, Operand(kRootRegister, offset));
}

void TurboAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  // TODO(jgruber): Add support for enable_root_array_delta_access.
  if (root_array_available() && options().isolate_independent_code) {
    IndirectLoadExternalReference(destination, source);
    return;
  }
  mov(destination, Immediate(source));
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
      bytes += kSystemPointerSize;
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
      bytes += kSystemPointerSize;
    }
  }

  if (fp_mode == kSaveFPRegs) {
    // Save all XMM registers except XMM0.
    int delta = kDoubleSize * (XMMRegister::kNumRegisters - 1);
    AllocateStackSpace(delta);
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
      bytes += kSystemPointerSize;
    }
  }

  return bytes;
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
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  lea(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    test_b(dst, Immediate(kTaggedSize - 1));
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

  push(object);
  push(address);

  pop(slot_parameter);
  pop(object_parameter);

  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(isolate()->builtins()->builtin_handle(Builtins::kEphemeronKeyBarrier),
       RelocInfo::CODE_TARGET);

  RestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode,
                      Builtins::kRecordWrite, kNullAddress);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    Address wasm_target) {
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode,
                      Builtins::kNoBuiltinId, wasm_target);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    int builtin_index, Address wasm_target) {
  DCHECK_NE(builtin_index == Builtins::kNoBuiltinId,
            wasm_target == kNullAddress);
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

  push(object);
  push(address);

  pop(slot_parameter);
  pop(object_parameter);

  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  if (builtin_index == Builtins::kNoBuiltinId) {
    // Use {wasm_call} for direct Wasm call within a module.
    wasm_call(wasm_target, RelocInfo::WASM_STUB_CALL);
  } else if (options().inline_offheap_trampolines) {
    CallBuiltin(builtin_index);
  } else {
    Handle<Code> code_target =
        isolate()->builtins()->builtin_handle(Builtins::kRecordWrite);
    Call(code_target, RelocInfo::CODE_TARGET);
  }

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

  if ((remembered_set_action == OMIT_REMEMBERED_SET &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
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
                MemoryChunk::kPointersFromHereAreInterestingMask, zero, &done,
                Label::kNear);

  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  Label dont_drop;
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  mov(eax, ExternalReferenceAsOperand(restart_fp, eax));
  test(eax, eax);
  j(zero, &dont_drop, Label::kNear);

  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET);
  bind(&dont_drop);
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

void TurboAssembler::Pmulhrsw(XMMRegister dst, XMMRegister src1,
                              XMMRegister src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpmulhrsw(dst, src1, src2);
  } else {
    if (dst != src1) {
      movaps(dst, src1);
    }
    CpuFeatureScope sse_scope(this, SSSE3);
    pmulhrsw(dst, src2);
  }
}

void TurboAssembler::I16x8Q15MulRSatS(XMMRegister dst, XMMRegister src1,
                                      XMMRegister src2, XMMRegister scratch) {
  // k = i16x8.splat(0x8000)
  Pcmpeqd(scratch, scratch);
  Psllw(scratch, scratch, byte{15});

  Pmulhrsw(dst, src1, src2);
  Pcmpeqw(scratch, dst);
  Pxor(dst, scratch);
}

void TurboAssembler::I8x16Popcnt(XMMRegister dst, XMMRegister src,
                                 XMMRegister tmp1, XMMRegister tmp2,
                                 Register scratch) {
  DCHECK_NE(dst, tmp1);
  DCHECK_NE(src, tmp1);
  DCHECK_NE(dst, tmp2);
  DCHECK_NE(src, tmp2);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovdqa(tmp1, ExternalReferenceAsOperand(
                      ExternalReference::address_of_wasm_i8x16_splat_0x0f(),
                      scratch));
    vpandn(tmp2, tmp1, src);
    vpand(dst, tmp1, src);
    vmovdqa(tmp1, ExternalReferenceAsOperand(
                      ExternalReference::address_of_wasm_i8x16_popcnt_mask(),
                      scratch));
    vpsrlw(tmp2, tmp2, 4);
    vpshufb(dst, tmp1, dst);
    vpshufb(tmp2, tmp1, tmp2);
    vpaddb(dst, dst, tmp2);
  } else if (CpuFeatures::IsSupported(ATOM)) {
    // Pre-Goldmont low-power Intel microarchitectures have very slow
    // PSHUFB instruction, thus use PSHUFB-free divide-and-conquer
    // algorithm on these processors. ATOM CPU feature captures exactly
    // the right set of processors.
    movaps(tmp1, src);
    psrlw(tmp1, 1);
    if (dst != src) {
      movaps(dst, src);
    }
    andps(tmp1,
          ExternalReferenceAsOperand(
              ExternalReference::address_of_wasm_i8x16_splat_0x55(), scratch));
    psubb(dst, tmp1);
    Operand splat_0x33 = ExternalReferenceAsOperand(
        ExternalReference::address_of_wasm_i8x16_splat_0x33(), scratch);
    movaps(tmp1, dst);
    andps(dst, splat_0x33);
    psrlw(tmp1, 2);
    andps(tmp1, splat_0x33);
    paddb(dst, tmp1);
    movaps(tmp1, dst);
    psrlw(dst, 4);
    paddb(dst, tmp1);
    andps(dst,
          ExternalReferenceAsOperand(
              ExternalReference::address_of_wasm_i8x16_splat_0x0f(), scratch));
  } else {
    CpuFeatureScope sse_scope(this, SSSE3);
    movaps(tmp1,
           ExternalReferenceAsOperand(
               ExternalReference::address_of_wasm_i8x16_splat_0x0f(), scratch));
    Operand mask = ExternalReferenceAsOperand(
        ExternalReference::address_of_wasm_i8x16_popcnt_mask(), scratch);
    if (tmp2 != tmp1) {
      movaps(tmp2, tmp1);
    }
    andps(tmp1, src);
    andnps(tmp2, src);
    psrlw(tmp2, 4);
    movaps(dst, mask);
    pshufb(dst, tmp1);
    movaps(tmp1, mask);
    pshufb(tmp1, tmp2);
    paddb(dst, tmp1);
  }
}

void TurboAssembler::F64x2ConvertLowI32x4U(XMMRegister dst, XMMRegister src,
                                           Register tmp) {
  // dst = [ src_low, 0x43300000, src_high, 0x4330000 ];
  // 0x43300000'00000000 is a special double where the significand bits
  // precisely represents all uint32 numbers.
  if (!CpuFeatures::IsSupported(AVX) && dst != src) {
    movaps(dst, src);
    src = dst;
  }
  Unpcklps(dst, src,
           ExternalReferenceAsOperand(
               ExternalReference::
                   address_of_wasm_f64x2_convert_low_i32x4_u_int_mask(),
               tmp));
  Subpd(dst, dst,
        ExternalReferenceAsOperand(
            ExternalReference::address_of_wasm_double_2_power_52(), tmp));
}

void TurboAssembler::I32x4TruncSatF64x2SZero(XMMRegister dst, XMMRegister src,
                                             XMMRegister scratch,
                                             Register tmp) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    XMMRegister original_dst = dst;
    // Make sure we don't overwrite src.
    if (dst == src) {
      DCHECK_NE(scratch, src);
      dst = scratch;
    }
    // dst = 0 if src == NaN, else all ones.
    vcmpeqpd(dst, src, src);
    // dst = 0 if src == NaN, else INT32_MAX as double.
    vandpd(dst, dst,
           ExternalReferenceAsOperand(
               ExternalReference::address_of_wasm_int32_max_as_double(), tmp));
    // dst = 0 if src == NaN, src is saturated to INT32_MAX as double.
    vminpd(dst, src, dst);
    // Values > INT32_MAX already saturated, values < INT32_MIN raises an
    // exception, which is masked and returns 0x80000000.
    vcvttpd2dq(dst, dst);

    if (original_dst != dst) {
      vmovaps(original_dst, dst);
    }
  } else {
    if (dst != src) {
      movaps(dst, src);
    }
    movaps(scratch, dst);
    cmpeqpd(scratch, dst);
    andps(scratch,
          ExternalReferenceAsOperand(
              ExternalReference::address_of_wasm_int32_max_as_double(), tmp));
    minpd(dst, scratch);
    cvttpd2dq(dst, dst);
  }
}

void TurboAssembler::I32x4TruncSatF64x2UZero(XMMRegister dst, XMMRegister src,
                                             XMMRegister scratch,
                                             Register tmp) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vxorpd(scratch, scratch, scratch);
    // Saturate to 0.
    vmaxpd(dst, src, scratch);
    // Saturate to UINT32_MAX.
    vminpd(dst, dst,
           ExternalReferenceAsOperand(
               ExternalReference::address_of_wasm_uint32_max_as_double(), tmp));
    // Truncate.
    vroundpd(dst, dst, kRoundToZero);
    // Add to special double where significant bits == uint32.
    vaddpd(dst, dst,
           ExternalReferenceAsOperand(
               ExternalReference::address_of_wasm_double_2_power_52(), tmp));
    // Extract low 32 bits of each double's significand, zero top lanes.
    // dst = [dst[0], dst[2], 0, 0]
    vshufps(dst, dst, scratch, 0x88);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst != src) {
      movaps(dst, src);
    }

    xorps(scratch, scratch);
    maxpd(dst, scratch);
    minpd(dst,
          ExternalReferenceAsOperand(
              ExternalReference::address_of_wasm_uint32_max_as_double(), tmp));
    roundpd(dst, dst, kRoundToZero);
    addpd(dst,
          ExternalReferenceAsOperand(
              ExternalReference::address_of_wasm_double_2_power_52(), tmp));
    shufps(dst, scratch, 0x88);
  }
}

void TurboAssembler::I16x8ExtAddPairwiseI8x16S(XMMRegister dst, XMMRegister src,
                                               XMMRegister tmp,
                                               Register scratch) {
  // pmaddubsw treats the first operand as unsigned, so pass the external
  // reference to as the first operand.
  Operand op = ExternalReferenceAsOperand(
      ExternalReference::address_of_wasm_i8x16_splat_0x01(), scratch);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovdqa(tmp, op);
    vpmaddubsw(dst, tmp, src);
  } else {
    CpuFeatureScope sse_scope(this, SSSE3);
    if (dst == src) {
      movaps(tmp, op);
      pmaddubsw(tmp, src);
      movaps(dst, tmp);
    } else {
      movaps(dst, op);
      pmaddubsw(dst, src);
    }
  }
}

void TurboAssembler::I16x8ExtAddPairwiseI8x16U(XMMRegister dst, XMMRegister src,
                                               Register scratch) {
  Operand op = ExternalReferenceAsOperand(
      ExternalReference::address_of_wasm_i8x16_splat_0x01(), scratch);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpmaddubsw(dst, src, op);
  } else {
    CpuFeatureScope sse_scope(this, SSSE3);
    movaps(dst, src);
    pmaddubsw(dst, op);
  }
}

void TurboAssembler::I32x4ExtAddPairwiseI16x8S(XMMRegister dst, XMMRegister src,
                                               Register scratch) {
  Operand op = ExternalReferenceAsOperand(
      ExternalReference::address_of_wasm_i16x8_splat_0x0001(), scratch);
  // pmaddwd multiplies signed words in src and op, producing
  // signed doublewords, then adds pairwise.
  // src = |a|b|c|d|e|f|g|h|
  // dst = | a*1 + b*1 | c*1 + d*1 | e*1 + f*1 | g*1 + h*1 |
  Pmaddwd(dst, src, op);
}

void TurboAssembler::I32x4ExtAddPairwiseI16x8U(XMMRegister dst, XMMRegister src,
                                               XMMRegister tmp) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    // src = |a|b|c|d|e|f|g|h| (low)
    // scratch = |0|a|0|c|0|e|0|g|
    vpsrld(tmp, src, 16);
    // dst = |0|b|0|d|0|f|0|h|
    vpblendw(dst, src, tmp, 0xAA);
    // dst = |a+b|c+d|e+f|g+h|
    vpaddd(dst, tmp, dst);
  } else if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    // There is a potentially better lowering if we get rip-relative constants,
    // see https://github.com/WebAssembly/simd/pull/380.
    movaps(tmp, src);
    psrld(tmp, 16);
    if (dst != src) {
      movaps(dst, src);
    }
    pblendw(dst, tmp, 0xAA);
    paddd(dst, tmp);
  } else {
    // src = |a|b|c|d|e|f|g|h|
    // tmp = i32x4.splat(0x0000FFFF)
    pcmpeqd(tmp, tmp);
    psrld(tmp, byte{16});
    // tmp =|0|b|0|d|0|f|0|h|
    andps(tmp, src);
    // dst = |0|a|0|c|0|e|0|g|
    if (dst != src) {
      movaps(dst, src);
    }
    psrld(dst, byte{16});
    // dst = |a+b|c+d|e+f|g+h|
    paddd(dst, tmp);
  }
}

void TurboAssembler::I8x16Swizzle(XMMRegister dst, XMMRegister src,
                                  XMMRegister mask, XMMRegister scratch,
                                  Register tmp, bool omit_add) {
  if (omit_add) {
    Pshufb(dst, src, mask);
    return;
  }

  // Out-of-range indices should return 0, add 112 so that any value > 15
  // saturates to 128 (top bit set), so pshufb will zero that lane.
  Operand op = ExternalReferenceAsOperand(
      ExternalReference::address_of_wasm_i8x16_swizzle_mask(), tmp);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpaddusb(scratch, mask, op);
    vpshufb(dst, src, scratch);
  } else {
    CpuFeatureScope sse_scope(this, SSSE3);
    movaps(scratch, op);
    if (dst != src) {
      movaps(dst, src);
    }
    paddusb(scratch, mask);
    pshufb(dst, scratch);
  }
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
  LoadMap(map, heap_object);
  CmpInstanceType(map, type);
}

void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::CmpInstanceTypeRange(Register map, Register scratch,
                                          InstanceType lower_limit,
                                          InstanceType higher_limit) {
  DCHECK_LT(lower_limit, higher_limit);
  movzx_w(scratch, FieldOperand(map, Map::kInstanceTypeOffset));
  lea(scratch, Operand(scratch, 0u - lower_limit));
  cmp(scratch, Immediate(higher_limit - lower_limit));
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(equal, AbortReason::kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (emit_debug_code()) {
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
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
    Push(object);
    LoadMap(object, object);
    CmpInstanceTypeRange(object, scratch, FIRST_JS_FUNCTION_TYPE,
                         LAST_JS_FUNCTION_TYPE);
    Pop(object);
    Check(below_equal, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;

  test(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  {
    Push(object);
    Register map = object;

    LoadMap(map, object);

    Label do_check;
    // Check if JSGeneratorObject
    CmpInstanceType(map, JS_GENERATOR_OBJECT_TYPE);
    j(equal, &do_check, Label::kNear);

    // Check if JSAsyncFunctionObject.
    CmpInstanceType(map, JS_ASYNC_FUNCTION_OBJECT_TYPE);
    j(equal, &do_check, Label::kNear);

    // Check if JSAsyncGeneratorObject
    CmpInstanceType(map, JS_ASYNC_GENERATOR_OBJECT_TYPE);

    bind(&do_check);
    Pop(object);
  }

  Check(equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
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
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmi);
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
  push(kContextRegister);                 // Callee's context.
  push(kJSFunctionRegister);              // Callee's JS function.
  push(kJavaScriptCallArgCountRegister);  // Actual argument count.
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, esp);
  if (!StackFrame::IsJavaScript(type)) {
    Push(Immediate(StackFrame::TypeToMarker(type)));
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code() && !StackFrame::IsJavaScript(type)) {
    cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
        Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
  }
  leave();
}

#ifdef V8_OS_WIN
void TurboAssembler::AllocateStackSpace(Register bytes_scratch) {
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
  j(greater, &touch_next_page);

  sub(esp, bytes_scratch);
}

void TurboAssembler::AllocateStackSpace(int bytes) {
  DCHECK_GE(bytes, 0);
  while (bytes > kStackPageSize) {
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

  STATIC_ASSERT(edx == kRuntimeCallFunctionRegister);
  STATIC_ASSERT(esi == kContextRegister);

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
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue();
}

void MacroAssembler::PushStackHandler(Register scratch) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  push(ExternalReferenceAsOperand(handler_address, scratch));

  // Set this new handler as the current one.
  mov(ExternalReferenceAsOperand(handler_address, scratch), esp);
}

void MacroAssembler::PopStackHandler(Register scratch) {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  pop(ExternalReferenceAsOperand(handler_address, scratch));
  add(esp, Immediate(StackHandlerConstants::kSize - kSystemPointerSize));
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
  // Set the entry point and jump to the C entry runtime stub.
  Move(kRuntimeCallFunctionRegister, Immediate(ext));
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  jmp(entry, RelocInfo::OFF_HEAP_TARGET);
}

void TurboAssembler::PrepareForTailCall(
    Register callee_args_count, Register caller_args_count, Register scratch0,
    Register scratch1, int number_of_temp_values_after_return_address) {
  DCHECK(!AreAliased(callee_args_count, caller_args_count, scratch0, scratch1));

  // Calculate the destination address where we will put the return address
  // after we drop current frame.
  Register new_sp_reg = scratch0;
  sub(caller_args_count, callee_args_count);
  lea(new_sp_reg, Operand(ebp, caller_args_count, times_system_pointer_size,
                          StandardFrameConstants::kCallerPCOffset -
                              number_of_temp_values_after_return_address *
                                  kSystemPointerSize));

  if (FLAG_debug_code) {
    cmp(esp, new_sp_reg);
    Check(below, AbortReason::kStackAccessBelowStackPointer);
  }

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch1;
  mov(tmp_reg, Operand(ebp, StandardFrameConstants::kCallerPCOffset));
  mov(Operand(esp,
              number_of_temp_values_after_return_address * kSystemPointerSize),
      tmp_reg);

  // Restore caller's frame pointer now as it could be overwritten by
  // the copying loop.
  mov(ebp, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // +2 here is to copy both receiver and return address.
  Register count_reg = caller_args_count;
  lea(count_reg, Operand(callee_args_count,
                         2 + number_of_temp_values_after_return_address));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  dec(count_reg);
  mov(tmp_reg, Operand(esp, count_reg, times_system_pointer_size, 0));
  mov(Operand(new_sp_reg, count_reg, times_system_pointer_size, 0), tmp_reg);
  bind(&entry);
  cmp(count_reg, Immediate(0));
  j(not_equal, &loop, Label::kNear);

  // Leave current frame.
  mov(esp, new_sp_reg);
}

void MacroAssembler::CompareStackLimit(Register with, StackLimitKind kind) {
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
                                    Label* done, InvokeFlag flag) {
  if (expected_parameter_count != actual_parameter_count) {
    DCHECK_EQ(actual_parameter_count, eax);
    DCHECK_EQ(expected_parameter_count, ecx);
    Label regular_invoke;

    // If the expected parameter count is equal to the adaptor sentinel, no need
    // to push undefined value as arguments.
    cmp(expected_parameter_count, Immediate(kDontAdaptArgumentsSentinel));
    j(equal, &regular_invoke, Label::kFar);

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
      // Extra words are the receiver and the return address (if a jump).
      int extra_words = flag == CALL_FUNCTION ? 1 : 2;
      lea(num, Operand(eax, extra_words));  // Number of words to copy.
      Set(current, 0);
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
      FrameScope frame(this,
                       has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
      CallRuntime(Runtime::kThrowStackOverflow);
      int3();  // This should be unreachable.
    }

    bind(&regular_invoke);
  }
}

void MacroAssembler::CallDebugOnFunctionCall(Register fun, Register new_target,
                                             Register expected_parameter_count,
                                             Register actual_parameter_count) {
  FrameScope frame(this, has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
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
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());
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
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, flag);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  mov(ecx, FieldOperand(function, JSFunction::kCodeOffset));
  if (flag == CALL_FUNCTION) {
    CallCodeObject(ecx);
  } else {
    DCHECK(flag == JUMP_FUNCTION);
    JumpCodeObject(ecx);
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
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun == edi);
  mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  movzx_w(ecx,
          FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(edi, new_target, ecx, actual_parameter_count, flag);
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  LoadNativeContextSlot(dst, Context::GLOBAL_PROXY_INDEX);
}

void MacroAssembler::LoadNativeContextSlot(Register destination, int index) {
  // Load the native context from the current context.
  LoadMap(destination, esi);
  mov(destination,
      FieldOperand(destination,
                   Map::kConstructorOrBackPointerOrNativeContextOffset));
  // Load the function from the native context.
  mov(destination, Operand(destination, Context::SlotOffset(index)));
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
  if (!src.is_heap_object_request() && src.is_zero()) {
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
        src.is_heap_object_request()) {
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

void TurboAssembler::Pshufb(XMMRegister dst, XMMRegister src, Operand mask) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufb(dst, src, mask);
    return;
  }

  // Make sure these are different so that we won't overwrite mask.
  DCHECK(!mask.is_reg(dst));
  CpuFeatureScope sse_scope(this, SSSE3);
  if (dst != src) {
    movaps(dst, src);
  }
  pshufb(dst, mask);
}

void TurboAssembler::Pblendw(XMMRegister dst, Operand src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpblendw(dst, dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pblendw(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE4.1 support");
}

void TurboAssembler::Palignr(XMMRegister dst, Operand src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpalignr(dst, dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    palignr(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Pextrd(Register dst, XMMRegister src, uint8_t imm8) {
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
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  AllocateStackSpace(kDoubleSize);
  movsd(Operand(esp, 0), src);
  mov(dst, Operand(esp, imm8 * kUInt32Size));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::Pinsrb(XMMRegister dst, Operand src, int8_t imm8) {
  Pinsrb(dst, dst, src, imm8);
}

void TurboAssembler::Pinsrb(XMMRegister dst, XMMRegister src1, Operand src2,
                            int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrb(dst, src1, src2, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    if (dst != src1) {
      movaps(dst, src1);
    }
    pinsrb(dst, src2, imm8);
    return;
  }
  FATAL("no AVX or SSE4.1 support");
}

void TurboAssembler::Pinsrd(XMMRegister dst, XMMRegister src1, Operand src2,
                            uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrd(dst, src1, src2, imm8);
    return;
  }
  if (dst != src1) {
    movaps(dst, src1);
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pinsrd(dst, src2, imm8);
    return;
  }
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  AllocateStackSpace(kDoubleSize);
  // Write original content of {dst} to the stack.
  movsd(Operand(esp, 0), dst);
  // Overwrite the portion specified in {imm8}.
  if (src2.is_reg_only()) {
    mov(Operand(esp, imm8 * kUInt32Size), src2.reg());
  } else {
    movss(dst, src2);
    movss(Operand(esp, imm8 * kUInt32Size), dst);
  }
  // Load back the full value into {dst}.
  movsd(dst, Operand(esp, 0));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::Pinsrd(XMMRegister dst, Operand src, uint8_t imm8) {
  Pinsrd(dst, dst, src, imm8);
}

void TurboAssembler::Pinsrw(XMMRegister dst, Operand src, int8_t imm8) {
  Pinsrw(dst, dst, src, imm8);
}

void TurboAssembler::Pinsrw(XMMRegister dst, XMMRegister src1, Operand src2,
                            int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrw(dst, src1, src2, imm8);
    return;
  } else {
    if (dst != src1) {
      movaps(dst, src1);
    }
    pinsrw(dst, src2, imm8);
    return;
  }
}

void TurboAssembler::Vbroadcastss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vbroadcastss(dst, src);
    return;
  }
  movss(dst, src);
  shufps(dst, dst, static_cast<byte>(0));
}

void TurboAssembler::Shufps(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                            uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vshufps(dst, src1, src2, imm8);
  } else {
    if (dst != src1) {
      movaps(dst, src1);
    }
    shufps(dst, src2, imm8);
  }
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
  cmp(in_out, Immediate(kClearedWeakHeapObjectLower32));
  j(equal, target_if_cleared);

  and_(in_out, Immediate(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter), scratch);
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter), scratch);
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}

void TurboAssembler::Assert(Condition cc, AbortReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void TurboAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
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
#ifdef DEBUG
  const char* msg = GetAbortReason(reason);
  RecordComment("Abort message: ");
  RecordComment(msg);
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    int3();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NONE);
    PrepareCallCFunction(1, eax);
    mov(Operand(esp, 0), Immediate(static_cast<int>(reason)));
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

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
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
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
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code_object));
  if (options().inline_offheap_trampolines) {
    int builtin_index = Builtins::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin_index)) {
      // Inline the trampoline.
      CallBuiltin(builtin_index);
      return;
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  STATIC_ASSERT(kSystemPointerSize == 4);
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below (we use
  // times_half_system_pointer_size instead of times_system_pointer_size since
  // smis are already shifted by one).
  mov(builtin_index,
      Operand(kRootRegister, builtin_index, times_half_system_pointer_size,
              IsolateData::builtin_entry_table_offset()));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  LoadEntryFromBuiltinIndex(builtin_index);
  call(builtin_index);
}

void TurboAssembler::CallBuiltin(int builtin_index) {
  DCHECK(Builtins::IsBuiltinId(builtin_index));
  RecordCommentForOffHeapTrampoline(builtin_index);
  CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
  EmbeddedData d = EmbeddedData::FromBlob();
  Address entry = d.InstructionStartOfBuiltin(builtin_index);
  call(entry, RelocInfo::OFF_HEAP_TARGET);
}

Operand TurboAssembler::EntryFromBuiltinIndexAsOperand(
    Builtins::Name builtin_index) {
  return Operand(kRootRegister,
                 IsolateData::builtin_entry_slot_offset(builtin_index));
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
  LoadCodeObjectEntry(code_object, code_object);
  call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
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
  if (options().inline_offheap_trampolines) {
    int builtin_index = Builtins::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin_index)) {
      // Inline the trampoline.
      RecordCommentForOffHeapTrampoline(builtin_index);
      CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
      EmbeddedData d = EmbeddedData::FromBlob();
      Address entry = d.InstructionStartOfBuiltin(builtin_index);
      jmp(entry, RelocInfo::OFF_HEAP_TARGET);
      return;
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  jmp(code_object, rmode);
}

void TurboAssembler::RetpolineCall(Register reg) {
  Label setup_return, setup_target, inner_indirect_branch, capture_spec;

  jmp(&setup_return);  // Jump past the entire retpoline below.

  bind(&inner_indirect_branch);
  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), reg);
  ret(0);

  bind(&setup_return);
  call(&inner_indirect_branch);  // Callee will return after this instruction.
}

void TurboAssembler::RetpolineCall(Address destination, RelocInfo::Mode rmode) {
  Label setup_return, setup_target, inner_indirect_branch, capture_spec;

  jmp(&setup_return);  // Jump past the entire retpoline below.

  bind(&inner_indirect_branch);
  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), destination, rmode);
  ret(0);

  bind(&setup_return);
  call(&inner_indirect_branch);  // Callee will return after this instruction.
}

void TurboAssembler::RetpolineJump(Register reg) {
  Label setup_target, capture_spec;

  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), reg);
  ret(0);
}

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
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

void TurboAssembler::CallForDeoptimization(Builtins::Name target, int,
                                           Label* exit, DeoptimizeKind kind,
                                           Label* ret, Label*) {
  CallBuiltin(target);
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy)
                ? Deoptimizer::kLazyDeoptExitSize
                : Deoptimizer::kNonLazyDeoptExitSize);

  if (kind == DeoptimizeKind::kEagerWithResume) {
    bool old_predictable_code_size = predictable_code_size();
    set_predictable_code_size(true);

    jmp(ret);
    DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
              Deoptimizer::kEagerWithResumeBeforeArgsSize);
    set_predictable_code_size(old_predictable_code_size);
  }
}

void TurboAssembler::Trap() { int3(); }
void TurboAssembler::DebugBreak() { int3(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
