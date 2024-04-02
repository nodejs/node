// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#if V8_TARGET_ARCH_X64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/string-constants.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/sandbox/external-pointer.h"
#include "src/snapshot/snapshot.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/x64/macro-assembler-x64.h"
#endif

namespace v8 {
namespace internal {

Operand StackArgumentsAccessor::GetArgumentOperand(int index) const {
  DCHECK_GE(index, 0);
  // arg[0] = rsp + kPCOnStackSize;
  // arg[i] = arg[0] + i * kSystemPointerSize;
  return Operand(rsp, kPCOnStackSize + index * kSystemPointerSize);
}

void MacroAssembler::Load(Register destination, ExternalReference source) {
  if (root_array_available_ && options().enable_root_relative_access) {
    intptr_t delta = RootRegisterOffsetForExternalReference(isolate(), source);
    if (is_int32(delta)) {
      movq(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  if (destination == rax && !options().isolate_independent_code) {
    load_rax(source);
  } else {
    movq(destination, ExternalReferenceAsOperand(source));
  }
}

void MacroAssembler::Store(ExternalReference destination, Register source) {
  if (root_array_available_ && options().enable_root_relative_access) {
    intptr_t delta =
        RootRegisterOffsetForExternalReference(isolate(), destination);
    if (is_int32(delta)) {
      movq(Operand(kRootRegister, static_cast<int32_t>(delta)), source);
      return;
    }
  }
  // Safe code.
  if (source == rax && !options().isolate_independent_code) {
    store_rax(destination);
  } else {
    movq(ExternalReferenceAsOperand(destination), source);
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadTaggedPointerField(
      destination,
      FieldOperand(destination, FixedArray::OffsetOfElementAt(constant_index)));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  DCHECK(is_int32(offset));
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    leaq(destination, Operand(kRootRegister, static_cast<int32_t>(offset)));
  }
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  movq(destination, Operand(kRootRegister, offset));
}

void TurboAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  if (root_array_available_ && options().enable_root_relative_access) {
    intptr_t delta = RootRegisterOffsetForExternalReference(isolate(), source);
    if (is_int32(delta)) {
      leaq(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadExternalReference(destination, source);
    return;
  }
  Move(destination, source);
}

Operand TurboAssembler::ExternalReferenceAsOperand(ExternalReference reference,
                                                   Register scratch) {
  if (root_array_available_ && options().enable_root_relative_access) {
    int64_t delta =
        RootRegisterOffsetForExternalReference(isolate(), reference);
    if (is_int32(delta)) {
      return Operand(kRootRegister, static_cast<int32_t>(delta));
    }
  }
  if (root_array_available_ && options().isolate_independent_code) {
    if (IsAddressableThroughRootRegister(isolate(), reference)) {
      // Some external references can be efficiently loaded as an offset from
      // kRootRegister.
      intptr_t offset =
          RootRegisterOffsetForExternalReference(isolate(), reference);
      CHECK(is_int32(offset));
      return Operand(kRootRegister, static_cast<int32_t>(offset));
    } else {
      // Otherwise, do a memory load from the external reference table.
      movq(scratch, Operand(kRootRegister,
                            RootRegisterOffsetForExternalReferenceTableEntry(
                                isolate(), reference)));
      return Operand(scratch, 0);
    }
  }
  Move(scratch, reference);
  return Operand(scratch, 0);
}

void MacroAssembler::PushAddress(ExternalReference source) {
  LoadAddress(kScratchRegister, source);
  Push(kScratchRegister);
}

Operand TurboAssembler::RootAsOperand(RootIndex index) {
  DCHECK(root_array_available());
  return Operand(kRootRegister, RootRegisterOffsetForRootIndex(index));
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  DCHECK(root_array_available_);
  movq(destination, RootAsOperand(index));
}

void MacroAssembler::PushRoot(RootIndex index) {
  DCHECK(root_array_available_);
  Push(RootAsOperand(index));
}

void TurboAssembler::CompareRoot(Register with, RootIndex index) {
  DCHECK(root_array_available_);
  if (base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                      RootIndex::kLastStrongOrReadOnlyRoot)) {
    cmp_tagged(with, RootAsOperand(index));
  } else {
    // Some smi roots contain system pointer size values like stack limits.
    cmpq(with, RootAsOperand(index));
  }
}

void TurboAssembler::CompareRoot(Operand with, RootIndex index) {
  DCHECK(root_array_available_);
  DCHECK(!with.AddressUsesRegister(kScratchRegister));
  LoadRoot(kScratchRegister, index);
  if (base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                      RootIndex::kLastStrongOrReadOnlyRoot)) {
    cmp_tagged(with, kScratchRegister);
  } else {
    // Some smi roots contain system pointer size values like stack limits.
    cmpq(with, kScratchRegister);
  }
}

void TurboAssembler::LoadMap(Register destination, Register object) {
  LoadTaggedPointerField(destination,
                         FieldOperand(object, HeapObject::kMapOffset));
#ifdef V8_MAP_PACKING
  UnpackMapWord(destination);
#endif
}

void TurboAssembler::LoadTaggedPointerField(Register destination,
                                            Operand field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(destination, field_operand);
  } else {
    mov_tagged(destination, field_operand);
  }
}

#ifdef V8_MAP_PACKING
void TurboAssembler::UnpackMapWord(Register r) {
  // Clear the top two bytes (which may include metadata). Must be in sync with
  // MapWord::Unpack, and vice versa.
  shlq(r, Immediate(16));
  shrq(r, Immediate(16));
  xorq(r, Immediate(Internals::kMapWordXorMask));
}
#endif

void TurboAssembler::LoadTaggedSignedField(Register destination,
                                           Operand field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedSigned(destination, field_operand);
  } else {
    mov_tagged(destination, field_operand);
  }
}

void TurboAssembler::LoadAnyTaggedField(Register destination,
                                        Operand field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressAnyTagged(destination, field_operand);
  } else {
    mov_tagged(destination, field_operand);
  }
}

void TurboAssembler::PushTaggedPointerField(Operand field_operand,
                                            Register scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(!field_operand.AddressUsesRegister(scratch));
    DecompressTaggedPointer(scratch, field_operand);
    Push(scratch);
  } else {
    Push(field_operand);
  }
}

void TurboAssembler::PushTaggedAnyField(Operand field_operand,
                                        Register scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(!field_operand.AddressUsesRegister(scratch));
    DecompressAnyTagged(scratch, field_operand);
    Push(scratch);
  } else {
    Push(field_operand);
  }
}

void TurboAssembler::SmiUntagField(Register dst, Operand src) {
  SmiUntag(dst, src);
}

void TurboAssembler::StoreTaggedField(Operand dst_field_operand,
                                      Immediate value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst_field_operand, value);
  } else {
    movq(dst_field_operand, value);
  }
}

void TurboAssembler::StoreTaggedField(Operand dst_field_operand,
                                      Register value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst_field_operand, value);
  } else {
    movq(dst_field_operand, value);
  }
}

void TurboAssembler::StoreTaggedSignedField(Operand dst_field_operand,
                                            Smi value) {
  if (SmiValuesAre32Bits()) {
    Move(kScratchRegister, value);
    movq(dst_field_operand, kScratchRegister);
  } else {
    StoreTaggedField(dst_field_operand, Immediate(value));
  }
}

void TurboAssembler::AtomicStoreTaggedField(Operand dst_field_operand,
                                            Register value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(kScratchRegister, value);
    xchgl(kScratchRegister, dst_field_operand);
  } else {
    movq(kScratchRegister, value);
    xchgq(kScratchRegister, dst_field_operand);
  }
}

void TurboAssembler::DecompressTaggedSigned(Register destination,
                                            Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movl(destination, field_operand);
}

void TurboAssembler::DecompressTaggedPointer(Register destination,
                                             Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movl(destination, field_operand);
  addq(destination, kPtrComprCageBaseRegister);
}

void TurboAssembler::DecompressTaggedPointer(Register destination,
                                             Register source) {
  ASM_CODE_COMMENT(this);
  movl(destination, source);
  addq(destination, kPtrComprCageBaseRegister);
}

void TurboAssembler::DecompressAnyTagged(Register destination,
                                         Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movl(destination, field_operand);
  addq(destination, kPtrComprCageBaseRegister);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register slot_address,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, value, slot_address));
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so the offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  leaq(slot_address, FieldOperand(object, offset));
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Debug check slot_address");
    Label ok;
    testb(slot_address, Immediate(kTaggedSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, slot_address, value, save_fp, remembered_set_action,
              SmiCheck::kOmit);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Zap scratch registers");
    Move(value, kZapValue, RelocInfo::NO_INFO);
    Move(slot_address, kZapValue, RelocInfo::NO_INFO);
  }
}

void TurboAssembler::EncodeSandboxedPointer(Register value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_SANDBOXED_POINTERS
  subq(value, kPtrComprCageBaseRegister);
  shlq(value, Immediate(kSandboxedPointerShift));
#else
  UNREACHABLE();
#endif
}

void TurboAssembler::DecodeSandboxedPointer(Register value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_SANDBOXED_POINTERS
  shrq(value, Immediate(kSandboxedPointerShift));
  addq(value, kPtrComprCageBaseRegister);
#else
  UNREACHABLE();
#endif
}

void TurboAssembler::LoadSandboxedPointerField(Register destination,
                                               Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movq(destination, field_operand);
  DecodeSandboxedPointer(destination);
}

void TurboAssembler::StoreSandboxedPointerField(Operand dst_field_operand,
                                                Register value) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(value, kScratchRegister));
  DCHECK(!dst_field_operand.AddressUsesRegister(kScratchRegister));
  movq(kScratchRegister, value);
  EncodeSandboxedPointer(kScratchRegister);
  movq(dst_field_operand, kScratchRegister);
}

void TurboAssembler::LoadExternalPointerField(
    Register destination, Operand field_operand, ExternalPointerTag tag,
    Register scratch, IsolateRootLocation isolateRootLocation) {
  DCHECK(!AreAliased(destination, scratch));
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  DCHECK_NE(kExternalPointerNullTag, tag);
  DCHECK(!field_operand.AddressUsesRegister(scratch));
  if (isolateRootLocation == IsolateRootLocation::kInRootRegister) {
    DCHECK(root_array_available_);
    movq(scratch, Operand(kRootRegister,
                          IsolateData::external_pointer_table_offset() +
                              Internals::kExternalPointerTableBufferOffset));
  } else {
    DCHECK(isolateRootLocation == IsolateRootLocation::kInScratchRegister);
    movq(scratch,
         Operand(scratch, IsolateData::external_pointer_table_offset() +
                              Internals::kExternalPointerTableBufferOffset));
  }
  movl(destination, field_operand);
  shrq(destination, Immediate(kExternalPointerIndexShift));
  movq(destination, Operand(scratch, destination, times_8, 0));
  movq(scratch, Immediate64(~tag));
  andq(destination, scratch);
#else
  movq(destination, field_operand);
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
}

void TurboAssembler::MaybeSaveRegisters(RegList registers) {
  for (Register reg : registers) {
    pushq(reg);
  }
}

void TurboAssembler::MaybeRestoreRegisters(RegList registers) {
  for (Register reg : base::Reversed(registers)) {
    popq(reg);
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
  MovePair(slot_address_parameter, slot_address, object_parameter, object);

  Call(isolate()->builtins()->code_handle(
           Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
       RelocInfo::CODE_TARGET);
  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStubSaveRegisters(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);
  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();
  MovePair(object_parameter, object, slot_address_parameter, slot_address);

  CallRecordWriteStub(object_parameter, slot_address_parameter,
                      remembered_set_action, fp_mode, mode);
  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  // Use CallRecordWriteStubSaveRegisters if the object and slot registers
  // need to be caller saved.
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
#if V8_ENABLE_WEBASSEMBLY
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    // Use {near_call} for direct Wasm call within a module.
    auto wasm_target =
        wasm::WasmCode::GetRecordWriteStub(remembered_set_action, fp_mode);
    near_call(wasm_target, RelocInfo::WASM_STUB_CALL);
#else
  if (false) {
#endif
  } else {
    Builtin builtin =
        Builtins::GetRecordWriteStub(remembered_set_action, fp_mode);
    if (options().inline_offheap_trampolines) {
      CallBuiltin(builtin);
    } else {
      Handle<CodeT> code_target = isolate()->builtins()->code_handle(builtin);
      Call(code_target, RelocInfo::CODE_TARGET);
    }
  }
}

#ifdef V8_IS_TSAN
void TurboAssembler::CallTSANStoreStub(Register address, Register value,
                                       SaveFPRegsMode fp_mode, int size,
                                       StubCallMode mode,
                                       std::memory_order order) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(address, value));
  TSANStoreDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  MaybeSaveRegisters(registers);

  Register address_parameter(
      descriptor.GetRegisterParameter(TSANStoreDescriptor::kAddress));
  Register value_parameter(
      descriptor.GetRegisterParameter(TSANStoreDescriptor::kValue));

  // Prepare argument registers for calling GetTSANStoreStub.
  MovePair(address_parameter, address, value_parameter, value);

  if (isolate()) {
    Builtin builtin = CodeFactory::GetTSANStoreStub(fp_mode, size, order);
    Handle<CodeT> code_target = isolate()->builtins()->code_handle(builtin);
    Call(code_target, RelocInfo::CODE_TARGET);
  }
#if V8_ENABLE_WEBASSEMBLY
  // There are two different kinds of wasm-to-js functions: one lives in the
  // wasm code space, and another one lives on the heap. Both of them have the
  // same CodeKind (WASM_TO_JS_FUNCTION), but depending on where they are they
  // have to either use the wasm stub calls, or call the builtin using the
  // isolate like JS does. In order to know which wasm-to-js function we are
  // compiling right now, we check if the isolate is null.
  // TODO(solanes, v8:11600): Split CodeKind::WASM_TO_JS_FUNCTION into two
  // different CodeKinds and pass the CodeKind as a parameter so that we can use
  // that instead of a nullptr check.
  // NOLINTNEXTLINE(readability/braces)
  else {
    DCHECK_EQ(mode, StubCallMode::kCallWasmRuntimeStub);
    // Use {near_call} for direct Wasm call within a module.
    auto wasm_target = wasm::WasmCode::GetTSANStoreStub(fp_mode, size, order);
    near_call(wasm_target, RelocInfo::WASM_STUB_CALL);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallTSANRelaxedLoadStub(Register address,
                                             SaveFPRegsMode fp_mode, int size,
                                             StubCallMode mode) {
  TSANLoadDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  MaybeSaveRegisters(registers);

  Register address_parameter(
      descriptor.GetRegisterParameter(TSANLoadDescriptor::kAddress));

  // Prepare argument registers for calling TSANRelaxedLoad.
  Move(address_parameter, address);

  if (isolate()) {
    Builtin builtin = CodeFactory::GetTSANRelaxedLoadStub(fp_mode, size);
    Handle<CodeT> code_target = isolate()->builtins()->code_handle(builtin);
    Call(code_target, RelocInfo::CODE_TARGET);
  }
#if V8_ENABLE_WEBASSEMBLY
  // There are two different kinds of wasm-to-js functions: one lives in the
  // wasm code space, and another one lives on the heap. Both of them have the
  // same CodeKind (WASM_TO_JS_FUNCTION), but depending on where they are they
  // have to either use the wasm stub calls, or call the builtin using the
  // isolate like JS does. In order to know which wasm-to-js function we are
  // compiling right now, we check if the isolate is null.
  // TODO(solanes, v8:11600): Split CodeKind::WASM_TO_JS_FUNCTION into two
  // different CodeKinds and pass the CodeKind as a parameter so that we can use
  // that instead of a nullptr check.
  // NOLINTNEXTLINE(readability/braces)
  else {
    DCHECK_EQ(mode, StubCallMode::kCallWasmRuntimeStub);
    // Use {near_call} for direct Wasm call within a module.
    auto wasm_target = wasm::WasmCode::GetTSANRelaxedLoadStub(fp_mode, size);
    near_call(wasm_target, RelocInfo::WASM_STUB_CALL);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  MaybeRestoreRegisters(registers);
}
#endif  // V8_IS_TSAN

void MacroAssembler::RecordWrite(Register object, Register slot_address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address, value));
  AssertNotSmi(object);

  if ((remembered_set_action == RememberedSetAction::kOmit &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  if (FLAG_debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Debug check slot_address");
    Label ok;
    cmp_tagged(value, Operand(slot_address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == SmiCheck::kInline) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);

  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, zero, &done,
                Label::kNear);

  CallRecordWriteStub(object, slot_address, remembered_set_action, fp_mode);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Zap scratch registers");
    Move(slot_address, kZapValue, RelocInfo::NO_INFO);
    Move(value, kZapValue, RelocInfo::NO_INFO);
  }
}

void TurboAssembler::Assert(Condition cc, AbortReason reason) {
  if (FLAG_debug_code) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (FLAG_debug_code) Abort(reason);
}

void TurboAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L, Label::kNear);
  Abort(reason);
  // Control will not return here.
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kSystemPointerSize) {
    ASM_CODE_COMMENT(this);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    Label alignment_as_expected;
    testq(rsp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected, Label::kNear);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}

void TurboAssembler::Abort(AbortReason reason) {
  ASM_CODE_COMMENT(this);
  if (FLAG_code_comments) {
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
    Move(arg_reg_1, static_cast<int>(reason));
    PrepareCallCFunction(1);
    LoadAddress(rax, ExternalReference::abort_with_reason());
    call(rax);
    return;
  }

  Move(rdx, Smi::FromInt(static_cast<int>(reason)));

  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NO_FRAME_TYPE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // Control will not return here.
  int3();
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
  Move(rax, num_arguments);
  LoadAddress(rbx, ExternalReference::Create(f));
  Handle<CodeT> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  // ----------- S t a t e -------------
  //  -- rsp[0]                 : return address
  //  -- rsp[8]                 : argument num_arguments - 1
  //  ...
  //  -- rsp[8 * num_arguments] : argument 0 (receiver)
  //
  //  For runtime functions with variable arguments:
  //  -- rax                    : number of  arguments
  // -----------------------------------
  ASM_CODE_COMMENT(this);
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    Move(rax, function->nargs);
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             bool builtin_exit_frame) {
  ASM_CODE_COMMENT(this);
  // Set the entry point and jump to the C entry runtime stub.
  LoadAddress(rbx, ext);
  Handle<CodeT> code =
      CodeFactory::CEntry(isolate(), 1, SaveFPRegsMode::kIgnore,
                          ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

static constexpr Register saved_regs[] = {rax, rcx, rdx, rbx, rbp, rsi,
                                          rdi, r8,  r9,  r10, r11};

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

  // R12 to r15 are callee save on all platforms.
  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += kStackSavedSavedFPSize * XMMRegister::kNumRegisters;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  ASM_CODE_COMMENT(this);
  // We don't allow a GC in a write barrier slow path so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      pushq(reg);
      bytes += kSystemPointerSize;
    }
  }

  // R12 to r15 are callee save on all platforms.
  if (fp_mode == SaveFPRegsMode::kSave) {
    const int delta = kStackSavedSavedFPSize * XMMRegister::kNumRegisters;
    AllocateStackSpace(delta);
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
#if V8_ENABLE_WEBASSEMBLY
      Movdqu(Operand(rsp, i * kStackSavedSavedFPSize), reg);
#else
      Movsd(Operand(rsp, i * kStackSavedSavedFPSize), reg);
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    bytes += delta;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  ASM_CODE_COMMENT(this);
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
#if V8_ENABLE_WEBASSEMBLY
      Movdqu(reg, Operand(rsp, i * kStackSavedSavedFPSize));
#else
      Movsd(reg, Operand(rsp, i * kStackSavedSavedFPSize));
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    const int delta = kStackSavedSavedFPSize * XMMRegister::kNumRegisters;
    addq(rsp, Immediate(delta));
    bytes += delta;
  }

  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      popq(reg);
      bytes += kSystemPointerSize;
    }
  }

  return bytes;
}

void TurboAssembler::Movq(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void TurboAssembler::Movq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void TurboAssembler::Pextrq(Register dst, XMMRegister src, int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpextrq(dst, src, imm8);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrq(dst, src, imm8);
  }
}

void TurboAssembler::Cvtss2sd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, src, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void TurboAssembler::Cvtss2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, dst, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void TurboAssembler::Cvtsd2ss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, src, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void TurboAssembler::Cvtsd2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, dst, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void TurboAssembler::Cvtlsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtlsi2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtlsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtlsi2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtqsi2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtlui2ss(XMMRegister dst, Register src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2ss(dst, kScratchRegister);
}

void TurboAssembler::Cvtlui2ss(XMMRegister dst, Operand src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2ss(dst, kScratchRegister);
}

void TurboAssembler::Cvtlui2sd(XMMRegister dst, Register src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2sd(dst, kScratchRegister);
}

void TurboAssembler::Cvtlui2sd(XMMRegister dst, Operand src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2sd(dst, kScratchRegister);
}

void TurboAssembler::Cvtqui2ss(XMMRegister dst, Register src) {
  Label done;
  Cvtqsi2ss(dst, src);
  testq(src, src);
  j(positive, &done, Label::kNear);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src != kScratchRegister) movq(kScratchRegister, src);
  shrq(kScratchRegister, Immediate(1));
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  Label msb_not_set;
  j(not_carry, &msb_not_set, Label::kNear);
  orq(kScratchRegister, Immediate(1));
  bind(&msb_not_set);
  Cvtqsi2ss(dst, kScratchRegister);
  Addss(dst, dst);
  bind(&done);
}

void TurboAssembler::Cvtqui2ss(XMMRegister dst, Operand src) {
  movq(kScratchRegister, src);
  Cvtqui2ss(dst, kScratchRegister);
}

void TurboAssembler::Cvtqui2sd(XMMRegister dst, Register src) {
  Label done;
  Cvtqsi2sd(dst, src);
  testq(src, src);
  j(positive, &done, Label::kNear);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src != kScratchRegister) movq(kScratchRegister, src);
  shrq(kScratchRegister, Immediate(1));
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  Label msb_not_set;
  j(not_carry, &msb_not_set, Label::kNear);
  orq(kScratchRegister, Immediate(1));
  bind(&msb_not_set);
  Cvtqsi2sd(dst, kScratchRegister);
  Addsd(dst, dst);
  bind(&done);
}

void TurboAssembler::Cvtqui2sd(XMMRegister dst, Operand src) {
  movq(kScratchRegister, src);
  Cvtqui2sd(dst, kScratchRegister);
}

void TurboAssembler::Cvttss2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void TurboAssembler::Cvttss2si(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void TurboAssembler::Cvttsd2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void TurboAssembler::Cvttsd2si(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void TurboAssembler::Cvttss2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void TurboAssembler::Cvttss2siq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void TurboAssembler::Cvttsd2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}

void TurboAssembler::Cvttsd2siq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}

namespace {
template <typename OperandOrXMMRegister, bool is_double>
void ConvertFloatToUint64(TurboAssembler* tasm, Register dst,
                          OperandOrXMMRegister src, Label* fail) {
  Label success;
  // There does not exist a native float-to-uint instruction, so we have to use
  // a float-to-int, and postprocess the result.
  if (is_double) {
    tasm->Cvttsd2siq(dst, src);
  } else {
    tasm->Cvttss2siq(dst, src);
  }
  // If the result of the conversion is positive, we are already done.
  tasm->testq(dst, dst);
  tasm->j(positive, &success);
  // The result of the first conversion was negative, which means that the
  // input value was not within the positive int64 range. We subtract 2^63
  // and convert it again to see if it is within the uint64 range.
  if (is_double) {
    tasm->Move(kScratchDoubleReg, -9223372036854775808.0);
    tasm->Addsd(kScratchDoubleReg, src);
    tasm->Cvttsd2siq(dst, kScratchDoubleReg);
  } else {
    tasm->Move(kScratchDoubleReg, -9223372036854775808.0f);
    tasm->Addss(kScratchDoubleReg, src);
    tasm->Cvttss2siq(dst, kScratchDoubleReg);
  }
  tasm->testq(dst, dst);
  // The only possible negative value here is 0x80000000000000000, which is
  // used on x64 to indicate an integer overflow.
  tasm->j(negative, fail ? fail : &success);
  // The input value is within uint64 range and the second conversion worked
  // successfully, but we still have to undo the subtraction we did
  // earlier.
  tasm->Move(kScratchRegister, 0x8000000000000000);
  tasm->orq(dst, kScratchRegister);
  tasm->bind(&success);
}
}  // namespace

void TurboAssembler::Cvttsd2uiq(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint64<Operand, true>(this, dst, src, fail);
}

void TurboAssembler::Cvttsd2uiq(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint64<XMMRegister, true>(this, dst, src, fail);
}

void TurboAssembler::Cvttss2uiq(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint64<Operand, false>(this, dst, src, fail);
}

void TurboAssembler::Cvttss2uiq(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint64<XMMRegister, false>(this, dst, src, fail);
}

void TurboAssembler::Cmpeqss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vcmpeqss(dst, src);
  } else {
    cmpeqss(dst, src);
  }
}

void TurboAssembler::Cmpeqsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vcmpeqsd(dst, src);
  } else {
    cmpeqsd(dst, src);
  }
}

// ----------------------------------------------------------------------------
// Smi tagging, untagging and tag detection.

Register TurboAssembler::GetSmiConstant(Smi source) {
  Move(kScratchRegister, source);
  return kScratchRegister;
}

void TurboAssembler::Cmp(Register dst, int32_t src) {
  if (src == 0) {
    testl(dst, dst);
  } else {
    cmpl(dst, Immediate(src));
  }
}

void TurboAssembler::SmiTag(Register reg) {
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  if (COMPRESS_POINTERS_BOOL) {
    shll(reg, Immediate(kSmiShift));
  } else {
    shlq(reg, Immediate(kSmiShift));
  }
}

void TurboAssembler::SmiTag(Register dst, Register src) {
  DCHECK(dst != src);
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst, src);
  } else {
    movq(dst, src);
  }
  SmiTag(dst);
}

void TurboAssembler::SmiUntag(Register reg) {
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  // TODO(v8:7703): Is there a way to avoid this sign extension when pointer
  // compression is enabled?
  if (COMPRESS_POINTERS_BOOL) {
    movsxlq(reg, reg);
  }
  sarq(reg, Immediate(kSmiShift));
}

void TurboAssembler::SmiUntag(Register dst, Register src) {
  DCHECK(dst != src);
  if (COMPRESS_POINTERS_BOOL) {
    movsxlq(dst, src);
  } else {
    movq(dst, src);
  }
  // TODO(v8:7703): Call SmiUntag(reg) if we can find a way to avoid the extra
  // mov when pointer compression is enabled.
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  sarq(dst, Immediate(kSmiShift));
}

void TurboAssembler::SmiUntag(Register dst, Operand src) {
  if (SmiValuesAre32Bits()) {
    movl(dst, Operand(src, kSmiShift / kBitsPerByte));
    // Sign extend to 64-bit.
    movsxlq(dst, dst);
  } else {
    DCHECK(SmiValuesAre31Bits());
    if (COMPRESS_POINTERS_BOOL) {
      movsxlq(dst, src);
    } else {
      movq(dst, src);
    }
    sarq(dst, Immediate(kSmiShift));
  }
}

void TurboAssembler::SmiToInt32(Register reg) {
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  if (COMPRESS_POINTERS_BOOL) {
    sarl(reg, Immediate(kSmiShift));
  } else {
    shrq(reg, Immediate(kSmiShift));
  }
}

void TurboAssembler::SmiCompare(Register smi1, Register smi2) {
  AssertSmi(smi1);
  AssertSmi(smi2);
  cmp_tagged(smi1, smi2);
}

void TurboAssembler::SmiCompare(Register dst, Smi src) {
  AssertSmi(dst);
  Cmp(dst, src);
}

void TurboAssembler::Cmp(Register dst, Smi src) {
  if (src.value() == 0) {
    test_tagged(dst, dst);
  } else {
    DCHECK_NE(dst, kScratchRegister);
    Register constant_reg = GetSmiConstant(src);
    cmp_tagged(dst, constant_reg);
  }
}

void TurboAssembler::SmiCompare(Register dst, Operand src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmp_tagged(dst, src);
}

void TurboAssembler::SmiCompare(Operand dst, Register src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmp_tagged(dst, src);
}

void TurboAssembler::SmiCompare(Operand dst, Smi src) {
  AssertSmi(dst);
  if (SmiValuesAre32Bits()) {
    cmpl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(src.value()));
  } else {
    DCHECK(SmiValuesAre31Bits());
    cmpl(dst, Immediate(src));
  }
}

void TurboAssembler::Cmp(Operand dst, Smi src) {
  // The Operand cannot use the smi register.
  Register smi_reg = GetSmiConstant(src);
  DCHECK(!dst.AddressUsesRegister(smi_reg));
  cmp_tagged(dst, smi_reg);
}

Condition TurboAssembler::CheckSmi(Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

Condition TurboAssembler::CheckSmi(Operand src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

void TurboAssembler::JumpIfSmi(Register src, Label* on_smi,
                               Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(smi, on_smi, near_jump);
}

void TurboAssembler::JumpIfNotSmi(Register src, Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
}

void TurboAssembler::JumpIfNotSmi(Operand src, Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
}

void TurboAssembler::SmiAddConstant(Operand dst, Smi constant) {
  if (constant.value() != 0) {
    if (SmiValuesAre32Bits()) {
      addl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(constant.value()));
    } else {
      DCHECK(SmiValuesAre31Bits());
      if (kTaggedSize == kInt64Size) {
        // Sign-extend value after addition
        movl(kScratchRegister, dst);
        addl(kScratchRegister, Immediate(constant));
        movsxlq(kScratchRegister, kScratchRegister);
        movq(dst, kScratchRegister);
      } else {
        DCHECK_EQ(kTaggedSize, kInt32Size);
        addl(dst, Immediate(constant));
      }
    }
  }
}

SmiIndex TurboAssembler::SmiToIndex(Register dst, Register src, int shift) {
  if (SmiValuesAre32Bits()) {
    DCHECK(is_uint6(shift));
    // There is a possible optimization if shift is in the range 60-63, but that
    // will (and must) never happen.
    if (dst != src) {
      movq(dst, src);
    }
    if (shift < kSmiShift) {
      sarq(dst, Immediate(kSmiShift - shift));
    } else {
      shlq(dst, Immediate(shift - kSmiShift));
    }
    return SmiIndex(dst, times_1);
  } else {
    DCHECK(SmiValuesAre31Bits());
    // We have to sign extend the index register to 64-bit as the SMI might
    // be negative.
    movsxlq(dst, src);
    if (shift < kSmiShift) {
      sarq(dst, Immediate(kSmiShift - shift));
    } else if (shift != kSmiShift) {
      if (shift - kSmiShift <= static_cast<int>(times_8)) {
        return SmiIndex(dst, static_cast<ScaleFactor>(shift - kSmiShift));
      }
      shlq(dst, Immediate(shift - kSmiShift));
    }
    return SmiIndex(dst, times_1);
  }
}

void TurboAssembler::Push(Smi source) {
  intptr_t smi = static_cast<intptr_t>(source.ptr());
  if (is_int32(smi)) {
    Push(Immediate(static_cast<int32_t>(smi)));
    return;
  }
  int first_byte_set = base::bits::CountTrailingZeros64(smi) / 8;
  int last_byte_set = (63 - base::bits::CountLeadingZeros64(smi)) / 8;
  if (first_byte_set == last_byte_set) {
    // This sequence has only 7 bytes, compared to the 12 bytes below.
    Push(Immediate(0));
    movb(Operand(rsp, first_byte_set),
         Immediate(static_cast<int8_t>(smi >> (8 * first_byte_set))));
    return;
  }
  Register constant = GetSmiConstant(source);
  Push(constant);
}

// ----------------------------------------------------------------------------

void TurboAssembler::Move(Register dst, Smi source) {
  STATIC_ASSERT(kSmiTag == 0);
  int value = source.value();
  if (value == 0) {
    xorl(dst, dst);
  } else if (SmiValuesAre32Bits()) {
    Move(dst, source.ptr(), RelocInfo::NO_INFO);
  } else {
    uint32_t uvalue = static_cast<uint32_t>(source.ptr());
    Move(dst, uvalue);
  }
}

void TurboAssembler::Move(Operand dst, intptr_t x) {
  if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else {
    Move(kScratchRegister, x);
    movq(dst, kScratchRegister);
  }
}

void TurboAssembler::Move(Register dst, ExternalReference ext) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadExternalReference(dst, ext);
    return;
  }
  movq(dst, Immediate64(ext.address(), RelocInfo::EXTERNAL_REFERENCE));
}

void TurboAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    movq(dst, src);
  }
}

void TurboAssembler::Move(Register dst, Operand src) { movq(dst, src); }
void TurboAssembler::Move(Register dst, Immediate src) {
  if (src.rmode() == RelocInfo::Mode::NO_INFO) {
    Move(dst, src.value());
  } else {
    movl(dst, src);
  }
}

void TurboAssembler::Move(XMMRegister dst, XMMRegister src) {
  if (dst != src) {
    Movaps(dst, src);
  }
}

void TurboAssembler::MovePair(Register dst0, Register src0, Register dst1,
                              Register src1) {
  if (dst0 != src1) {
    // Normal case: Writing to dst0 does not destroy src1.
    Move(dst0, src0);
    Move(dst1, src1);
  } else if (dst1 != src0) {
    // Only dst0 and src1 are the same register,
    // but writing to dst1 does not destroy src0.
    Move(dst1, src1);
    Move(dst0, src0);
  } else {
    // dst0 == src1, and dst1 == src0, a swap is required:
    // dst0 \/ src0
    // dst1 /\ src1
    xchgq(dst0, dst1);
  }
}

void TurboAssembler::MoveNumber(Register dst, double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) {
    Move(dst, Smi::FromInt(smi));
  } else {
    movq_heap_number(dst, value);
  }
}

void TurboAssembler::Move(XMMRegister dst, uint32_t src) {
  if (src == 0) {
    Xorps(dst, dst);
  } else {
    unsigned nlz = base::bits::CountLeadingZeros(src);
    unsigned ntz = base::bits::CountTrailingZeros(src);
    unsigned pop = base::bits::CountPopulation(src);
    DCHECK_NE(0u, pop);
    if (pop + ntz + nlz == 32) {
      Pcmpeqd(dst, dst);
      if (ntz) Pslld(dst, static_cast<byte>(ntz + nlz));
      if (nlz) Psrld(dst, static_cast<byte>(nlz));
    } else {
      movl(kScratchRegister, Immediate(src));
      Movd(dst, kScratchRegister);
    }
  }
}

void TurboAssembler::Move(XMMRegister dst, uint64_t src) {
  if (src == 0) {
    Xorpd(dst, dst);
  } else {
    unsigned nlz = base::bits::CountLeadingZeros(src);
    unsigned ntz = base::bits::CountTrailingZeros(src);
    unsigned pop = base::bits::CountPopulation(src);
    DCHECK_NE(0u, pop);
    if (pop + ntz + nlz == 64) {
      Pcmpeqd(dst, dst);
      if (ntz) Psllq(dst, static_cast<byte>(ntz + nlz));
      if (nlz) Psrlq(dst, static_cast<byte>(nlz));
    } else {
      uint32_t lower = static_cast<uint32_t>(src);
      uint32_t upper = static_cast<uint32_t>(src >> 32);
      if (upper == 0) {
        Move(dst, lower);
      } else {
        movq(kScratchRegister, src);
        Movq(dst, kScratchRegister);
      }
    }
  }
}

void TurboAssembler::Move(XMMRegister dst, uint64_t high, uint64_t low) {
  if (high == low) {
    Move(dst, low);
    Punpcklqdq(dst, dst);
    return;
  }

  Move(dst, low);
  movq(kScratchRegister, high);
  Pinsrq(dst, dst, kScratchRegister, uint8_t{1});
}

// ----------------------------------------------------------------------------

void MacroAssembler::Cmp(Register dst, Handle<Object> source) {
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    Move(kScratchRegister, Handle<HeapObject>::cast(source));
    cmp_tagged(dst, kScratchRegister);
  }
}

void MacroAssembler::Cmp(Operand dst, Handle<Object> source) {
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    Move(kScratchRegister, Handle<HeapObject>::cast(source));
    cmp_tagged(dst, kScratchRegister);
  }
}

void MacroAssembler::CompareRange(Register value, unsigned lower_limit,
                                  unsigned higher_limit) {
  ASM_CODE_COMMENT(this);
  DCHECK_LT(lower_limit, higher_limit);
  if (lower_limit != 0) {
    leal(kScratchRegister, Operand(value, 0u - lower_limit));
    cmpl(kScratchRegister, Immediate(higher_limit - lower_limit));
  } else {
    cmpl(value, Immediate(higher_limit));
  }
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit, Label* on_in_range,
                                     Label::Distance near_jump) {
  CompareRange(value, lower_limit, higher_limit);
  j(below_equal, on_in_range, near_jump);
}

void TurboAssembler::Push(Handle<HeapObject> source) {
  Move(kScratchRegister, source);
  Push(kScratchRegister);
}

void TurboAssembler::PushArray(Register array, Register size, Register scratch,
                               PushArrayOrder order) {
  DCHECK(!AreAliased(array, size, scratch));
  Register counter = scratch;
  Label loop, entry;
  if (order == PushArrayOrder::kReverse) {
    Move(counter, 0);
    jmp(&entry);
    bind(&loop);
    Push(Operand(array, counter, times_system_pointer_size, 0));
    incq(counter);
    bind(&entry);
    cmpq(counter, size);
    j(less, &loop, Label::kNear);
  } else {
    movq(counter, size);
    jmp(&entry);
    bind(&loop);
    Push(Operand(array, counter, times_system_pointer_size, 0));
    bind(&entry);
    decq(counter);
    j(greater_equal, &loop, Label::kNear);
  }
}

void TurboAssembler::Move(Register result, Handle<HeapObject> object,
                          RelocInfo::Mode rmode) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    // TODO(v8:9706): Fix-it! This load will always uncompress the value
    // even when we are loading a compressed embedded object.
    IndirectLoadConstant(result, object);
  } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
    EmbeddedObjectIndex index = AddEmbeddedObject(object);
    DCHECK(is_uint32(index));
    movl(result, Immediate(static_cast<int>(index), rmode));
  } else {
    DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
    movq(result, Immediate64(object.address(), rmode));
  }
}

void TurboAssembler::Move(Operand dst, Handle<HeapObject> object,
                          RelocInfo::Mode rmode) {
  Move(kScratchRegister, object, rmode);
  movq(dst, kScratchRegister);
}

void TurboAssembler::MoveStringConstant(Register result,
                                        const StringConstantBase* string,
                                        RelocInfo::Mode rmode) {
  movq_string(result, string);
}

void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    addq(rsp, Immediate(stack_elements * kSystemPointerSize));
  }
}

void MacroAssembler::DropUnderReturnAddress(int stack_elements,
                                            Register scratch) {
  DCHECK_GT(stack_elements, 0);
  if (stack_elements == 1) {
    popq(MemOperand(rsp, 0));
    return;
  }

  PopReturnAddressTo(scratch);
  Drop(stack_elements);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::DropArguments(Register count, ArgumentsCountType type,
                                   ArgumentsCountMode mode) {
  int receiver_bytes =
      (mode == kCountExcludesReceiver) ? kSystemPointerSize : 0;
  switch (type) {
    case kCountIsInteger: {
      leaq(rsp, Operand(rsp, count, times_system_pointer_size, receiver_bytes));
      break;
    }
    case kCountIsSmi: {
      SmiIndex index = SmiToIndex(count, count, kSystemPointerSizeLog2);
      leaq(rsp, Operand(rsp, index.reg, index.scale, receiver_bytes));
      break;
    }
    case kCountIsBytes: {
      if (receiver_bytes == 0) {
        addq(rsp, count);
      } else {
        leaq(rsp, Operand(rsp, count, times_1, receiver_bytes));
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
  DCHECK(!receiver.AddressUsesRegister(scratch));
  PopReturnAddressTo(scratch);
  DropArguments(argc, type, mode);
  Push(receiver);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::Push(Register src) { pushq(src); }

void TurboAssembler::Push(Operand src) { pushq(src); }

void MacroAssembler::PushQuad(Operand src) { pushq(src); }

void TurboAssembler::Push(Immediate value) { pushq(value); }

void MacroAssembler::PushImm32(int32_t imm32) { pushq_imm32(imm32); }

void MacroAssembler::Pop(Register dst) { popq(dst); }

void MacroAssembler::Pop(Operand dst) { popq(dst); }

void MacroAssembler::PopQuad(Operand dst) { popq(dst); }

void TurboAssembler::Jump(const ExternalReference& reference) {
  DCHECK(root_array_available());
  jmp(Operand(kRootRegister, RootRegisterOffsetForExternalReferenceTableEntry(
                                 isolate(), reference)));
}

void TurboAssembler::Jump(Operand op) { jmp(op); }

void TurboAssembler::Jump(Address destination, RelocInfo::Mode rmode) {
  Move(kScratchRegister, destination, rmode);
  jmp(kScratchRegister);
}

void TurboAssembler::Jump(Handle<CodeT> code_object, RelocInfo::Mode rmode,
                          Condition cc) {
  DCHECK_IMPLIES(
      options().isolate_independent_code,
      Builtins::IsIsolateIndependentBuiltin(FromCodeT(*code_object)));
  if (options().inline_offheap_trampolines) {
    Builtin builtin = Builtin::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin)) {
      Label skip;
      if (cc != always) {
        if (cc == never) return;
        j(NegateCondition(cc), &skip, Label::kNear);
      }
      TailCallBuiltin(builtin);
      bind(&skip);
      return;
    }
  }
  j(cc, code_object, rmode);
}

void MacroAssembler::JumpToOffHeapInstructionStream(Address entry) {
  Move(kOffHeapTrampolineRegister, entry, RelocInfo::OFF_HEAP_TARGET);
  jmp(kOffHeapTrampolineRegister);
}

void TurboAssembler::Call(ExternalReference ext) {
  LoadAddress(kScratchRegister, ext);
  call(kScratchRegister);
}

void TurboAssembler::Call(Operand op) {
  if (!CpuFeatures::IsSupported(INTEL_ATOM)) {
    call(op);
  } else {
    movq(kScratchRegister, op);
    call(kScratchRegister);
  }
}

void TurboAssembler::Call(Address destination, RelocInfo::Mode rmode) {
  Move(kScratchRegister, destination, rmode);
  call(kScratchRegister);
}

void TurboAssembler::Call(Handle<CodeT> code_object, RelocInfo::Mode rmode) {
  // TODO(v8:11880): avoid roundtrips between cdc and code.
  DCHECK_IMPLIES(
      options().isolate_independent_code,
      Builtins::IsIsolateIndependentBuiltin(FromCodeT(*code_object)));
  if (options().inline_offheap_trampolines) {
    Builtin builtin = Builtin::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin)) {
      // Inline the trampoline.
      CallBuiltin(builtin);
      return;
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
}

Operand TurboAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  DCHECK(root_array_available());
  return Operand(kRootRegister, IsolateData::BuiltinEntrySlotOffset(builtin));
}

Operand TurboAssembler::EntryFromBuiltinIndexAsOperand(Register builtin_index) {
  if (SmiValuesAre32Bits()) {
    // The builtin_index register contains the builtin index as a Smi.
    SmiUntag(builtin_index);
    return Operand(kRootRegister, builtin_index, times_system_pointer_size,
                   IsolateData::builtin_entry_table_offset());
  } else {
    DCHECK(SmiValuesAre31Bits());

    // The builtin_index register contains the builtin index as a Smi.
    // Untagging is folded into the indexing operand below (we use
    // times_half_system_pointer_size since smis are already shifted by one).
    return Operand(kRootRegister, builtin_index, times_half_system_pointer_size,
                   IsolateData::builtin_entry_table_offset());
  }
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  Call(EntryFromBuiltinIndexAsOperand(builtin_index));
}

void TurboAssembler::CallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this, CommentForOffHeapTrampoline("call", builtin));
  if (options().short_builtin_calls) {
    call(BuiltinEntry(builtin), RelocInfo::RUNTIME_ENTRY);
  } else {
    Move(kScratchRegister, BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
    call(kScratchRegister);
  }
}

void TurboAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));
  if (options().short_builtin_calls) {
    jmp(BuiltinEntry(builtin), RelocInfo::RUNTIME_ENTRY);
  } else {
    Jump(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
  }
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
  ASM_CODE_COMMENT(this);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    LoadExternalPointerField(
        destination,
        FieldOperand(code_object, CodeDataContainer::kCodeEntryPointOffset),
        kCodeEntryPointTag, kScratchRegister);
    return;
  }

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
    testl(FieldOperand(code_object, Code::kFlagsOffset),
          Immediate(Code::IsOffHeapTrampoline::kMask));
    j(not_equal, &if_code_is_off_heap);

    // Not an off-heap trampoline, the entry point is at
    // Code::raw_instruction_start().
    Move(destination, code_object);
    addq(destination, Immediate(Code::kHeaderSize - kHeapObjectTag));
    jmp(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    movl(destination, FieldOperand(code_object, Code::kBuiltinIndexOffset));
    movq(destination,
         Operand(kRootRegister, destination, times_system_pointer_size,
                 IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    Move(destination, code_object);
    addq(destination, Immediate(Code::kHeaderSize - kHeapObjectTag));
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
      pushq(code_object);
      Ret();
      return;
  }
}

void TurboAssembler::LoadCodeDataContainerEntry(
    Register destination, Register code_data_container_object) {
  ASM_CODE_COMMENT(this);
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  LoadExternalPointerField(
      destination,
      FieldOperand(code_data_container_object,
                   CodeDataContainer::kCodeEntryPointOffset),
      kCodeEntryPointTag, kScratchRegister);
}

void TurboAssembler::LoadCodeDataContainerCodeNonBuiltin(
    Register destination, Register code_data_container_object) {
  ASM_CODE_COMMENT(this);
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  // Given the fields layout we can read the Code reference as a full word.
  STATIC_ASSERT(!V8_EXTERNAL_CODE_SPACE_BOOL ||
                (CodeDataContainer::kCodeCageBaseUpper32BitsOffset ==
                 CodeDataContainer::kCodeOffset + kTaggedSize));
  movq(destination, FieldOperand(code_data_container_object,
                                 CodeDataContainer::kCodeOffset));
}

void TurboAssembler::CallCodeDataContainerObject(
    Register code_data_container_object) {
  LoadCodeDataContainerEntry(code_data_container_object,
                             code_data_container_object);
  call(code_data_container_object);
}

void TurboAssembler::JumpCodeDataContainerObject(
    Register code_data_container_object, JumpMode jump_mode) {
  LoadCodeDataContainerEntry(code_data_container_object,
                             code_data_container_object);
  switch (jump_mode) {
    case JumpMode::kJump:
      jmp(code_data_container_object);
      return;
    case JumpMode::kPushAndReturn:
      pushq(code_data_container_object);
      Ret();
      return;
  }
}

void TurboAssembler::LoadCodeTEntry(Register destination, Register code) {
  ASM_CODE_COMMENT(this);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    LoadCodeDataContainerEntry(destination, code);
  } else {
    leaq(destination, Operand(code, Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeTObject(Register code) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    CallCodeDataContainerObject(code);
  } else {
    CallCodeObject(code);
  }
}

void TurboAssembler::JumpCodeTObject(Register code, JumpMode jump_mode) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    JumpCodeDataContainerObject(code, jump_mode);
  } else {
    JumpCodeObject(code, jump_mode);
  }
}

void TurboAssembler::PextrdPreSse41(Register dst, XMMRegister src,
                                    uint8_t imm8) {
  if (imm8 == 0) {
    Movd(dst, src);
    return;
  }
  DCHECK_EQ(1, imm8);
  movq(dst, src);
  shrq(dst, Immediate(32));
}

namespace {
template <typename Op>
void PinsrdPreSse41Helper(TurboAssembler* tasm, XMMRegister dst, Op src,
                          uint8_t imm8, uint32_t* load_pc_offset) {
  tasm->Movd(kScratchDoubleReg, src);
  if (load_pc_offset) *load_pc_offset = tasm->pc_offset();
  if (imm8 == 1) {
    tasm->punpckldq(dst, kScratchDoubleReg);
  } else {
    DCHECK_EQ(0, imm8);
    tasm->Movss(dst, kScratchDoubleReg);
  }
}
}  // namespace

void TurboAssembler::PinsrdPreSse41(XMMRegister dst, Register src, uint8_t imm8,
                                    uint32_t* load_pc_offset) {
  PinsrdPreSse41Helper(this, dst, src, imm8, load_pc_offset);
}

void TurboAssembler::PinsrdPreSse41(XMMRegister dst, Operand src, uint8_t imm8,
                                    uint32_t* load_pc_offset) {
  PinsrdPreSse41Helper(this, dst, src, imm8, load_pc_offset);
}

void TurboAssembler::Pinsrq(XMMRegister dst, XMMRegister src1, Register src2,
                            uint8_t imm8, uint32_t* load_pc_offset) {
  PinsrHelper(this, &Assembler::vpinsrq, &Assembler::pinsrq, dst, src1, src2,
              imm8, load_pc_offset, {SSE4_1});
}

void TurboAssembler::Pinsrq(XMMRegister dst, XMMRegister src1, Operand src2,
                            uint8_t imm8, uint32_t* load_pc_offset) {
  PinsrHelper(this, &Assembler::vpinsrq, &Assembler::pinsrq, dst, src1, src2,
              imm8, load_pc_offset, {SSE4_1});
}

void TurboAssembler::Lzcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsrl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 63);  // 63^31 == 32
  bind(&not_zero_src);
  xorl(dst, Immediate(31));  // for x in [0..31], 31^x == 31 - x
}

void TurboAssembler::Lzcntl(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsrl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 63);  // 63^31 == 32
  bind(&not_zero_src);
  xorl(dst, Immediate(31));  // for x in [0..31], 31^x == 31 - x
}

void TurboAssembler::Lzcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsrq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 127);  // 127^63 == 64
  bind(&not_zero_src);
  xorl(dst, Immediate(63));  // for x in [0..63], 63^x == 63 - x
}

void TurboAssembler::Lzcntq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsrq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 127);  // 127^63 == 64
  bind(&not_zero_src);
  xorl(dst, Immediate(63));  // for x in [0..63], 63^x == 63 - x
}

void TurboAssembler::Tzcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsfq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  // Define the result of tzcnt(0) separately, because bsf(0) is undefined.
  Move(dst, 64);
  bind(&not_zero_src);
}

void TurboAssembler::Tzcntq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsfq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  // Define the result of tzcnt(0) separately, because bsf(0) is undefined.
  Move(dst, 64);
  bind(&not_zero_src);
}

void TurboAssembler::Tzcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsfl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 32);  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}

void TurboAssembler::Tzcntl(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsfl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, 32);  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}

void TurboAssembler::Popcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntl(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  Push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  Push(ExternalReferenceAsOperand(handler_address));

  // Set this new handler as the current one.
  movq(ExternalReferenceAsOperand(handler_address), rsp);
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  Pop(ExternalReferenceAsOperand(handler_address));
  addq(rsp, Immediate(StackHandlerConstants::kSize - kSystemPointerSize));
}

void TurboAssembler::Ret() { ret(0); }

void TurboAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    PopReturnAddressTo(scratch);
    addq(rsp, Immediate(bytes_dropped));
    PushReturnAddressFrom(scratch);
    ret(0);
  }
}

void TurboAssembler::IncsspqIfSupported(Register number_of_words,
                                        Register scratch) {
  // Optimized code can validate at runtime whether the cpu supports the
  // incsspq instruction, so it shouldn't use this method.
  CHECK(isolate()->IsGeneratingEmbeddedBuiltins());
  DCHECK_NE(number_of_words, scratch);
  Label not_supported;
  ExternalReference supports_cetss =
      ExternalReference::supports_cetss_address();
  Operand supports_cetss_operand =
      ExternalReferenceAsOperand(supports_cetss, scratch);
  cmpb(supports_cetss_operand, Immediate(0));
  j(equal, &not_supported, Label::kNear);
  incsspq(number_of_words);
  bind(&not_supported);
}

void MacroAssembler::CmpObjectType(Register heap_object, InstanceType type,
                                   Register map) {
  LoadMap(map, heap_object);
  CmpInstanceType(map, type);
}

void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::CmpInstanceTypeRange(Register map,
                                          Register instance_type_out,
                                          InstanceType lower_limit,
                                          InstanceType higher_limit) {
  DCHECK_LT(lower_limit, higher_limit);
  movzxwl(instance_type_out, FieldOperand(map, Map::kInstanceTypeOffset));
  CompareRange(instance_type_out, lower_limit, higher_limit);
}

void TurboAssembler::AssertNotSmi(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(NegateCondition(is_smi), AbortReason::kOperandIsASmi);
}

void TurboAssembler::AssertSmi(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(is_smi, AbortReason::kOperandIsNotASmi);
}

void TurboAssembler::AssertSmi(Operand object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(is_smi, AbortReason::kOperandIsNotASmi);
}

void TurboAssembler::AssertZeroExtended(Register int32_register) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  DCHECK_NE(int32_register, kScratchRegister);
  movq(kScratchRegister, int64_t{0x0000000100000000});
  cmpq(kScratchRegister, int32_register);
  Check(above, AbortReason::k32BitValueInRegisterIsNotZeroExtended);
}

void MacroAssembler::AssertCodeT(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsNotACodeT);
  Push(object);
  LoadMap(object, object);
  CmpInstanceType(object, CODET_TYPE);
  Pop(object);
  Check(equal, AbortReason::kOperandIsNotACodeT);
}

void MacroAssembler::AssertConstructor(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAConstructor);
  Push(object);
  LoadMap(object, object);
  testb(FieldOperand(object, Map::kBitFieldOffset),
        Immediate(Map::Bits1::IsConstructorBit::kMask));
  Pop(object);
  Check(not_zero, AbortReason::kOperandIsNotAConstructor);
}

void MacroAssembler::AssertFunction(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
  Push(object);
  LoadMap(object, object);
  CmpInstanceTypeRange(object, object, FIRST_JS_FUNCTION_TYPE,
                       LAST_JS_FUNCTION_TYPE);
  Pop(object);
  Check(below_equal, AbortReason::kOperandIsNotAFunction);
}

void MacroAssembler::AssertCallableFunction(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
  Push(object);
  LoadMap(object, object);
  CmpInstanceTypeRange(object, object, FIRST_CALLABLE_JS_FUNCTION_TYPE,
                       LAST_CALLABLE_JS_FUNCTION_TYPE);
  Pop(object);
  Check(below_equal, AbortReason::kOperandIsNotACallableFunction);
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
  Push(object);
  CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
  Pop(object);
  Check(equal, AbortReason::kOperandIsNotABoundFunction);
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  Register map = object;
  Push(object);
  LoadMap(map, object);

  Label do_check;
  // Check if JSGeneratorObject
  CmpInstanceType(map, JS_GENERATOR_OBJECT_TYPE);
  j(equal, &do_check);

  // Check if JSAsyncFunctionObject
  CmpInstanceType(map, JS_ASYNC_FUNCTION_OBJECT_TYPE);
  j(equal, &do_check);

  // Check if JSAsyncGeneratorObject
  CmpInstanceType(map, JS_ASYNC_GENERATOR_OBJECT_TYPE);

  bind(&do_check);
  // Restore generator object to register and perform assertion
  Pop(object);
  Check(equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  Label done_checking;
  AssertNotSmi(object);
  Cmp(object, isolate()->factory()->undefined_value());
  j(equal, &done_checking);
  Register map = object;
  Push(object);
  LoadMap(map, object);
  Cmp(map, isolate()->factory()->allocation_site_map());
  Pop(object);
  Assert(equal, AbortReason::kExpectedUndefinedOrCell);
  bind(&done_checking);
}

void MacroAssembler::LoadWeakValue(Register in_out, Label* target_if_cleared) {
  cmpl(in_out, Immediate(kClearedWeakHeapObjectLower32));
  j(equal, target_if_cleared);

  andq(in_out, Immediate(~static_cast<int32_t>(kWeakHeapObjectMask)));
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    Operand counter_operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    if (value == 1) {
      incl(counter_operand);
    } else {
      addl(counter_operand, Immediate(value));
    }
  }
}

void MacroAssembler::EmitDecrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    ASM_CODE_COMMENT(this);
    Operand counter_operand =
        ExternalReferenceAsOperand(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    if (value == 1) {
      decl(counter_operand);
    } else {
      subl(counter_operand, Immediate(value));
    }
  }
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  ASM_CODE_COMMENT(this);
  LoadTaggedPointerField(
      rbx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  movzxwq(rbx,
          FieldOperand(rbx, SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunction(function, new_target, rbx, actual_parameter_count, type);
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  DCHECK_EQ(function, rdi);
  LoadTaggedPointerField(rsi,
                         FieldOperand(function, JSFunction::kContextOffset));
  InvokeFunctionCode(rdi, new_target, expected_parameter_count,
                     actual_parameter_count, type);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());
  DCHECK_EQ(function, rdi);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == rdx);

  // On function call, call into the debugger if necessary.
  Label debug_hook, continue_after_hook;
  {
    ExternalReference debug_hook_active =
        ExternalReference::debug_hook_on_function_call_address(isolate());
    Operand debug_hook_active_operand =
        ExternalReferenceAsOperand(debug_hook_active);
    cmpb(debug_hook_active_operand, Immediate(0));
    j(not_equal, &debug_hook);
  }
  bind(&continue_after_hook);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(rdx, RootIndex::kUndefinedValue);
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, type);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  LoadTaggedPointerField(rcx, FieldOperand(function, JSFunction::kCodeOffset));
  switch (type) {
    case InvokeType::kCall:
      CallCodeTObject(rcx);
      break;
    case InvokeType::kJump:
      JumpCodeTObject(rcx);
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

Operand MacroAssembler::StackLimitAsOperand(StackLimitKind kind) {
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
  return Operand(kRootRegister, static_cast<int32_t>(offset));
}

void MacroAssembler::StackOverflowCheck(
    Register num_args, Label* stack_overflow,
    Label::Distance stack_overflow_distance) {
  ASM_CODE_COMMENT(this);
  DCHECK_NE(num_args, kScratchRegister);
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  movq(kScratchRegister, rsp);
  // Make kScratchRegister the space we have left. The stack might already be
  // overflowed here which will cause kScratchRegister to become negative.
  subq(kScratchRegister, StackLimitAsOperand(StackLimitKind::kRealStackLimit));
  // TODO(victorgomes): Use ia32 approach with leaq, since it requires less
  // instructions.
  sarq(kScratchRegister, Immediate(kSystemPointerSizeLog2));
  // Check if the arguments will overflow the stack.
  cmpq(kScratchRegister, num_args);
  // Signed comparison.
  // TODO(victorgomes):  Save some bytes in the builtins that use stack checks
  // by jumping to a builtin that throws the exception.
  j(less_equal, stack_overflow, stack_overflow_distance);
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeType type) {
    ASM_CODE_COMMENT(this);
    if (expected_parameter_count == actual_parameter_count) {
      Move(rax, actual_parameter_count);
      return;
    }
    Label regular_invoke;
    // If the expected parameter count is equal to the adaptor sentinel, no need
    // to push undefined value as arguments.
    if (kDontAdaptArgumentsSentinel != 0) {
      cmpl(expected_parameter_count, Immediate(kDontAdaptArgumentsSentinel));
      j(equal, &regular_invoke, Label::kFar);
    }

    // If overapplication or if the actual argument count is equal to the
    // formal parameter count, no need to push extra undefined values.
    subq(expected_parameter_count, actual_parameter_count);
    j(less_equal, &regular_invoke, Label::kFar);

    Label stack_overflow;
    StackOverflowCheck(expected_parameter_count, &stack_overflow);

    // Underapplication. Move the arguments already in the stack, including the
    // receiver and the return address.
    {
      Label copy, check;
      Register src = r8, dest = rsp, num = r9, current = r11;
      movq(src, rsp);
      leaq(kScratchRegister,
           Operand(expected_parameter_count, times_system_pointer_size, 0));
      AllocateStackSpace(kScratchRegister);
      // Extra words are for the return address (if a jump).
      int extra_words =
          type == InvokeType::kCall ? 0 : kReturnAddressStackSlotCount;

      leaq(num, Operand(rax, extra_words));  // Number of words to copy.
      Move(current, 0);
      // Fall-through to the loop body because there are non-zero words to copy.
      bind(&copy);
      movq(kScratchRegister,
           Operand(src, current, times_system_pointer_size, 0));
      movq(Operand(dest, current, times_system_pointer_size, 0),
           kScratchRegister);
      incq(current);
      bind(&check);
      cmpq(current, num);
      j(less, &copy);
      leaq(r8, Operand(rsp, num, times_system_pointer_size, 0));
    }
    // Fill remaining expected arguments with undefined values.
    LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
    {
      Label loop;
      bind(&loop);
      decq(expected_parameter_count);
      movq(Operand(r8, expected_parameter_count, times_system_pointer_size, 0),
           kScratchRegister);
      j(greater, &loop, Label::kNear);
    }
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
  Operand receiver_op = Operand(rbp, kSystemPointerSize * 2);
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

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  pushq(rbp);  // Caller's frame pointer.
  movq(rbp, rsp);
  Push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::Prologue() {
  ASM_CODE_COMMENT(this);
  pushq(rbp);  // Caller's frame pointer.
  movq(rbp, rsp);
  Push(kContextRegister);                 // Callee's context.
  Push(kJSFunctionRegister);              // Callee's JS function.
  Push(kJavaScriptCallArgCountRegister);  // Actual argument count.
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  pushq(rbp);
  movq(rbp, rsp);
  if (!StackFrame::IsJavaScript(type)) {
    Push(Immediate(StackFrame::TypeToMarker(type)));
  }
#if V8_ENABLE_WEBASSEMBLY
  if (type == StackFrame::WASM) Push(kWasmInstanceRegister);
#endif  // V8_ENABLE_WEBASSEMBLY
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  // TODO(v8:11429): Consider passing BASELINE instead, and checking for
  // IsJSFrame or similar. Could then unify with manual frame leaves in the
  // interpreter too.
  if (FLAG_debug_code && !StackFrame::IsJavaScript(type)) {
    cmpq(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
  }
  movq(rsp, rbp);
  popq(rbp);
}

#if defined(V8_TARGET_OS_WIN) || defined(V8_TARGET_OS_MACOS)
void TurboAssembler::AllocateStackSpace(Register bytes_scratch) {
  ASM_CODE_COMMENT(this);
  // On Windows and on macOS, we cannot increment the stack size by more than
  // one page (minimum page size is 4KB) without accessing at least one byte on
  // the page. Check this:
  // https://msdn.microsoft.com/en-us/library/aa227153(v=vs.60).aspx.
  Label check_offset;
  Label touch_next_page;
  jmp(&check_offset);
  bind(&touch_next_page);
  subq(rsp, Immediate(kStackPageSize));
  // Just to touch the page, before we increment further.
  movb(Operand(rsp, 0), Immediate(0));
  subq(bytes_scratch, Immediate(kStackPageSize));

  bind(&check_offset);
  cmpq(bytes_scratch, Immediate(kStackPageSize));
  j(greater_equal, &touch_next_page);

  subq(rsp, bytes_scratch);
}

void TurboAssembler::AllocateStackSpace(int bytes) {
  ASM_CODE_COMMENT(this);
  DCHECK_GE(bytes, 0);
  while (bytes >= kStackPageSize) {
    subq(rsp, Immediate(kStackPageSize));
    movb(Operand(rsp, 0), Immediate(0));
    bytes -= kStackPageSize;
  }
  if (bytes == 0) return;
  subq(rsp, Immediate(bytes));
}
#endif

void MacroAssembler::EnterExitFramePrologue(Register saved_rax_reg,
                                            StackFrame::Type frame_type) {
  ASM_CODE_COMMENT(this);
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  // All constants are relative to the frame pointer of the exit frame.
  DCHECK_EQ(kFPOnStackSize + kPCOnStackSize,
            ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(kFPOnStackSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kSystemPointerSize, ExitFrameConstants::kCallerFPOffset);
  pushq(rbp);
  movq(rbp, rsp);

  // Reserve room for entry stack pointer.
  Push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kSystemPointerSize, ExitFrameConstants::kSPOffset);
  Push(Immediate(0));  // Saved entry sp, patched before call.

  // Save the frame pointer and the context in top.
  if (saved_rax_reg != no_reg) {
    movq(saved_rax_reg, rax);  // Backup rax in callee-save register.
  }

  Store(
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()),
      rbp);
  Store(ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()),
        rsi);
  Store(
      ExternalReference::Create(IsolateAddressId::kCFunctionAddress, isolate()),
      rbx);
}

#ifdef V8_TARGET_OS_WIN
static const int kRegisterPassedArguments = 4;
#else
static const int kRegisterPassedArguments = 6;
#endif

void MacroAssembler::EnterExitFrameEpilogue(int arg_stack_space,
                                            bool save_doubles) {
  ASM_CODE_COMMENT(this);
#ifdef V8_TARGET_OS_WIN
  arg_stack_space += kRegisterPassedArguments;
#endif
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space = XMMRegister::kNumRegisters * kDoubleSize +
                arg_stack_space * kSystemPointerSize;
    AllocateStackSpace(space);
    int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    const RegisterConfiguration* config = RegisterConfiguration::Default();
    for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
      DoubleRegister reg =
          DoubleRegister::from_code(config->GetAllocatableDoubleCode(i));
      Movsd(Operand(rbp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else if (arg_stack_space > 0) {
    AllocateStackSpace(arg_stack_space * kSystemPointerSize);
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = base::OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(kFrameAlignment));
    DCHECK(is_int8(kFrameAlignment));
    andq(rsp, Immediate(-kFrameAlignment));
  }

  // Patch the saved entry sp.
  movq(Operand(rbp, ExitFrameConstants::kSPOffset), rsp);
}

void MacroAssembler::EnterExitFrame(int arg_stack_space, bool save_doubles,
                                    StackFrame::Type frame_type) {
  ASM_CODE_COMMENT(this);
  Register saved_rax_reg = r12;
  EnterExitFramePrologue(saved_rax_reg, frame_type);

  // Set up argv in callee-saved register r15. It is reused in LeaveExitFrame,
  // so it must be retained across the C-call.
  int offset = StandardFrameConstants::kCallerSPOffset - kSystemPointerSize;
  leaq(r15, Operand(rbp, saved_rax_reg, times_system_pointer_size, offset));

  EnterExitFrameEpilogue(arg_stack_space, save_doubles);
}

void MacroAssembler::EnterApiExitFrame(int arg_stack_space) {
  ASM_CODE_COMMENT(this);
  EnterExitFramePrologue(no_reg, StackFrame::EXIT);
  EnterExitFrameEpilogue(arg_stack_space, false);
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, bool pop_arguments) {
  ASM_CODE_COMMENT(this);
  // Registers:
  // r15 : argv
  if (save_doubles) {
    int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    const RegisterConfiguration* config = RegisterConfiguration::Default();
    for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
      DoubleRegister reg =
          DoubleRegister::from_code(config->GetAllocatableDoubleCode(i));
      Movsd(reg, Operand(rbp, offset - ((i + 1) * kDoubleSize)));
    }
  }

  if (pop_arguments) {
    // Get the return address from the stack and restore the frame pointer.
    movq(rcx, Operand(rbp, kFPOnStackSize));
    movq(rbp, Operand(rbp, 0 * kSystemPointerSize));

    // Drop everything up to and including the arguments and the receiver
    // from the caller stack.
    leaq(rsp, Operand(r15, 1 * kSystemPointerSize));

    PushReturnAddressFrom(rcx);
  } else {
    // Otherwise just leave the exit frame.
    leave();
  }

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveApiExitFrame() {
  ASM_CODE_COMMENT(this);
  movq(rsp, rbp);
  popq(rbp);

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  ASM_CODE_COMMENT(this);
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address =
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
  Operand context_operand = ExternalReferenceAsOperand(context_address);
  movq(rsi, context_operand);
#ifdef DEBUG
  Move(context_operand, Context::kInvalidContext);
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  Operand c_entry_fp_operand = ExternalReferenceAsOperand(c_entry_fp_address);
  Move(c_entry_fp_operand, 0);
}

void MacroAssembler::LoadNativeContextSlot(Register dst, int index) {
  ASM_CODE_COMMENT(this);
  // Load native context.
  LoadMap(dst, rsi);
  LoadTaggedPointerField(
      dst,
      FieldOperand(dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  // Load value from native context.
  LoadTaggedPointerField(dst, Operand(dst, Context::SlotOffset(index)));
}

int TurboAssembler::ArgumentStackSlotsForCFunctionCall(int num_arguments) {
  // On Windows 64 stack slots are reserved by the caller for all arguments
  // including the ones passed in registers, and space is always allocated for
  // the four register arguments even if the function takes fewer than four
  // arguments.
  // On AMD64 ABI (Linux/Mac) the first six arguments are passed in registers
  // and the caller does not reserve stack slots for them.
  DCHECK_GE(num_arguments, 0);
#ifdef V8_TARGET_OS_WIN
  const int kMinimumStackSlots = kRegisterPassedArguments;
  if (num_arguments < kMinimumStackSlots) return kMinimumStackSlots;
  return num_arguments;
#else
  if (num_arguments < kRegisterPassedArguments) return 0;
  return num_arguments - kRegisterPassedArguments;
#endif
}

void TurboAssembler::PrepareCallCFunction(int num_arguments) {
  ASM_CODE_COMMENT(this);
  int frame_alignment = base::OS::ActivationFrameAlignment();
  DCHECK_NE(frame_alignment, 0);
  DCHECK_GE(num_arguments, 0);

  // Make stack end at alignment and allocate space for arguments and old rsp.
  movq(kScratchRegister, rsp);
  DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  AllocateStackSpace((argument_slots_on_stack + 1) * kSystemPointerSize);
  andq(rsp, Immediate(-frame_alignment));
  movq(Operand(rsp, argument_slots_on_stack * kSystemPointerSize),
       kScratchRegister);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  ASM_CODE_COMMENT(this);
  LoadAddress(rax, function);
  CallCFunction(rax, num_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  ASM_CODE_COMMENT(this);
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (FLAG_debug_code) {
    CheckStackAlignment();
  }

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  Label get_pc;
  DCHECK(!AreAliased(kScratchRegister, function));
  leaq(kScratchRegister, Operand(&get_pc, 0));
  bind(&get_pc);

  // Addressing the following external references is tricky because we need
  // this to work in three situations:
  // 1. In wasm compilation, the isolate is nullptr and thus no
  //    ExternalReference can be created, but we can construct the address
  //    directly using the root register and a static offset.
  // 2. In normal JIT (and builtin) compilation, the external reference is
  //    usually addressed through the root register, so we can use the direct
  //    offset directly in most cases.
  // 3. In regexp compilation, the external reference is embedded into the reloc
  //    info.
  // The solution here is to use root register offsets wherever possible in
  // which case we can construct it directly. When falling back to external
  // references we need to ensure that the scratch register does not get
  // accidentally overwritten. If we run into more such cases in the future, we
  // should implement a more general solution.
  if (root_array_available()) {
    movq(Operand(kRootRegister, IsolateData::fast_c_call_caller_pc_offset()),
         kScratchRegister);
    movq(Operand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset()),
         rbp);
  } else {
    DCHECK_NOT_NULL(isolate());
    // Use alternative scratch register in order not to overwrite
    // kScratchRegister.
    Register scratch = r12;
    pushq(scratch);

    movq(ExternalReferenceAsOperand(
             ExternalReference::fast_c_call_caller_pc_address(isolate()),
             scratch),
         kScratchRegister);
    movq(ExternalReferenceAsOperand(
             ExternalReference::fast_c_call_caller_fp_address(isolate())),
         rbp);

    popq(scratch);
  }

  call(function);

  // We don't unset the PC; the FP is the source of truth.
  if (root_array_available()) {
    movq(Operand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset()),
         Immediate(0));
  } else {
    DCHECK_NOT_NULL(isolate());
    movq(ExternalReferenceAsOperand(
             ExternalReference::fast_c_call_caller_fp_address(isolate())),
         Immediate(0));
  }

  DCHECK_NE(base::OS::ActivationFrameAlignment(), 0);
  DCHECK_GE(num_arguments, 0);
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  movq(rsp, Operand(rsp, argument_slots_on_stack * kSystemPointerSize));
}

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
  ASM_CODE_COMMENT(this);
  DCHECK(cc == zero || cc == not_zero);
  if (scratch == object) {
    andq(scratch, Immediate(~kPageAlignmentMask));
  } else {
    movq(scratch, Immediate(~kPageAlignmentMask));
    andq(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    testb(Operand(scratch, BasicMemoryChunk::kFlagsOffset),
          Immediate(static_cast<uint8_t>(mask)));
  } else {
    testl(Operand(scratch, BasicMemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  Label current;
  bind(&current);
  int pc = pc_offset();
  // Load effective address to get the address of the current instruction.
  leaq(dst, Operand(&current, -pc));
}

// Check if the code object is marked for deoptimization. If it is, then it
// jumps to the CompileLazyDeoptimizedCode builtin. In order to do this we need
// to:
//    1. read from memory the word that contains that bit, which can be found in
//       the flags in the referenced {CodeDataContainer} object;
//    2. test kMarkedForDeoptimizationBit in those flags; and
//    3. if it is not zero then it jumps to the builtin.
void TurboAssembler::BailoutIfDeoptimized(Register scratch) {
  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  LoadTaggedPointerField(scratch,
                         Operand(kJavaScriptCallCodeStartRegister, offset));
  testl(FieldOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset),
        Immediate(1 << Code::kMarkedForDeoptimizationBit));
  Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
       RelocInfo::CODE_TARGET, not_zero);
}

void TurboAssembler::CallForDeoptimization(Builtin target, int, Label* exit,
                                           DeoptimizeKind kind, Label* ret,
                                           Label*) {
  ASM_CODE_COMMENT(this);
  // Note: Assembler::call is used here on purpose to guarantee fixed-size
  // exits even on Atom CPUs; see TurboAssembler::Call for Atom-specific
  // performance tuning which emits a different instruction sequence.
  call(EntryFromBuiltinAsOperand(target));
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void TurboAssembler::Trap() { int3(); }
void TurboAssembler::DebugBreak() { int3(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
