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
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/sandbox/external-pointer.h"
#include "src/snapshot/snapshot.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/x64/macro-assembler-x64.h"
#endif

#define __ ACCESS_MASM(masm)

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

void MacroAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadTaggedField(
      destination,
      FieldOperand(destination, FixedArray::OffsetOfElementAt(constant_index)));
}

void MacroAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  DCHECK(is_int32(offset));
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    leaq(destination, Operand(kRootRegister, static_cast<int32_t>(offset)));
  }
}

void MacroAssembler::LoadRootRelative(Register destination, int32_t offset) {
  movq(destination, Operand(kRootRegister, offset));
}

void MacroAssembler::LoadAddress(Register destination,
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

Operand MacroAssembler::ExternalReferenceAsOperand(ExternalReference reference,
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

Operand MacroAssembler::RootAsOperand(RootIndex index) {
  DCHECK(root_array_available());
  return Operand(kRootRegister, RootRegisterOffsetForRootIndex(index));
}

void MacroAssembler::LoadTaggedRoot(Register destination, RootIndex index) {
  static_assert(!CanBeImmediate(RootIndex::kUndefinedValue) ||
                std::is_same<Tagged_t, uint32_t>::value);
  if (CanBeImmediate(index)) {
    mov_tagged(destination,
               Immediate(static_cast<uint32_t>(ReadOnlyRootPtr(index))));
    return;
  }
  DCHECK(root_array_available_);
  movq(destination, RootAsOperand(index));
}

void MacroAssembler::LoadRoot(Register destination, RootIndex index) {
  if (CanBeImmediate(index)) {
    DecompressTagged(destination,
                     static_cast<uint32_t>(ReadOnlyRootPtr(index)));
    return;
  }
  DCHECK(root_array_available_);
  movq(destination, RootAsOperand(index));
}

void MacroAssembler::PushRoot(RootIndex index) {
  DCHECK(root_array_available_);
  Push(RootAsOperand(index));
}

void MacroAssembler::CompareRoot(Register with, RootIndex index) {
  if (CanBeImmediate(index)) {
    cmp_tagged(with, Immediate(static_cast<uint32_t>(ReadOnlyRootPtr(index))));
    return;
  }
  DCHECK(root_array_available_);
  if (base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                      RootIndex::kLastStrongOrReadOnlyRoot)) {
    cmp_tagged(with, RootAsOperand(index));
  } else {
    // Some smi roots contain system pointer size values like stack limits.
    cmpq(with, RootAsOperand(index));
  }
}

void MacroAssembler::CompareRoot(Operand with, RootIndex index) {
  if (CanBeImmediate(index)) {
    cmp_tagged(with, Immediate(static_cast<uint32_t>(ReadOnlyRootPtr(index))));
    return;
  }
  DCHECK(root_array_available_);
  DCHECK(!with.AddressUsesRegister(kScratchRegister));
  if (base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                      RootIndex::kLastStrongOrReadOnlyRoot)) {
    mov_tagged(kScratchRegister, RootAsOperand(index));
    cmp_tagged(with, kScratchRegister);
  } else {
    // Some smi roots contain system pointer size values like stack limits.
    movq(kScratchRegister, RootAsOperand(index));
    cmpq(with, kScratchRegister);
  }
}

void MacroAssembler::LoadCompressedMap(Register destination, Register object) {
  CHECK(COMPRESS_POINTERS_BOOL);
  mov_tagged(destination, FieldOperand(object, HeapObject::kMapOffset));
}

void MacroAssembler::LoadMap(Register destination, Register object) {
  LoadTaggedField(destination, FieldOperand(object, HeapObject::kMapOffset));
#ifdef V8_MAP_PACKING
  UnpackMapWord(destination);
#endif
}

void MacroAssembler::LoadTaggedField(Register destination,
                                     Operand field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTagged(destination, field_operand);
  } else {
    mov_tagged(destination, field_operand);
  }
}

void MacroAssembler::LoadTaggedField(TaggedRegister destination,
                                     Operand field_operand) {
  LoadTaggedFieldWithoutDecompressing(destination.reg(), field_operand);
}

void MacroAssembler::LoadTaggedFieldWithoutDecompressing(
    Register destination, Operand field_operand) {
  mov_tagged(destination, field_operand);
}

#ifdef V8_MAP_PACKING
void MacroAssembler::UnpackMapWord(Register r) {
  // Clear the top two bytes (which may include metadata). Must be in sync with
  // MapWord::Unpack, and vice versa.
  shlq(r, Immediate(16));
  shrq(r, Immediate(16));
  xorq(r, Immediate(Internals::kMapWordXorMask));
}
#endif

void MacroAssembler::LoadTaggedSignedField(Register destination,
                                           Operand field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedSigned(destination, field_operand);
  } else {
    mov_tagged(destination, field_operand);
  }
}

void MacroAssembler::PushTaggedField(Operand field_operand, Register scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(!field_operand.AddressUsesRegister(scratch));
    DecompressTagged(scratch, field_operand);
    Push(scratch);
  } else {
    Push(field_operand);
  }
}

void MacroAssembler::SmiUntagField(Register dst, Operand src) {
  SmiUntag(dst, src);
}

void MacroAssembler::SmiUntagFieldUnsigned(Register dst, Operand src) {
  SmiUntagUnsigned(dst, src);
}

void MacroAssembler::StoreTaggedField(Operand dst_field_operand,
                                      Immediate value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst_field_operand, value);
  } else {
    movq(dst_field_operand, value);
  }
}

void MacroAssembler::StoreTaggedField(Operand dst_field_operand,
                                      Register value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst_field_operand, value);
  } else {
    movq(dst_field_operand, value);
  }
}

void MacroAssembler::StoreTaggedSignedField(Operand dst_field_operand,
                                            Smi value) {
  if (SmiValuesAre32Bits()) {
    Move(kScratchRegister, value);
    movq(dst_field_operand, kScratchRegister);
  } else {
    StoreTaggedField(dst_field_operand, Immediate(value));
  }
}

void MacroAssembler::AtomicStoreTaggedField(Operand dst_field_operand,
                                            Register value) {
  if (COMPRESS_POINTERS_BOOL) {
    movl(kScratchRegister, value);
    xchgl(kScratchRegister, dst_field_operand);
  } else {
    movq(kScratchRegister, value);
    xchgq(kScratchRegister, dst_field_operand);
  }
}

void MacroAssembler::DecompressTaggedSigned(Register destination,
                                            Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movl(destination, field_operand);
}

void MacroAssembler::DecompressTagged(Register destination,
                                      Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movl(destination, field_operand);
  addq(destination, kPtrComprCageBaseRegister);
}

void MacroAssembler::DecompressTagged(Register destination, Register source) {
  ASM_CODE_COMMENT(this);
  movl(destination, source);
  addq(destination, kPtrComprCageBaseRegister);
}

void MacroAssembler::DecompressTagged(Register destination,
                                      Tagged_t immediate) {
  ASM_CODE_COMMENT(this);
  leaq(destination,
       Operand(kPtrComprCageBaseRegister, static_cast<int32_t>(immediate)));
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register slot_address,
                                      SaveFPRegsMode save_fp,
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
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Debug check slot_address");
    Label ok;
    testb(slot_address, Immediate(kTaggedSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, slot_address, value, save_fp, SmiCheck::kOmit);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Zap scratch registers");
    Move(value, kZapValue, RelocInfo::NO_INFO);
    Move(slot_address, kZapValue, RelocInfo::NO_INFO);
  }
}

void MacroAssembler::EncodeSandboxedPointer(Register value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_ENABLE_SANDBOX
  subq(value, kPtrComprCageBaseRegister);
  shlq(value, Immediate(kSandboxedPointerShift));
#else
  UNREACHABLE();
#endif
}

void MacroAssembler::DecodeSandboxedPointer(Register value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_ENABLE_SANDBOX
  shrq(value, Immediate(kSandboxedPointerShift));
  addq(value, kPtrComprCageBaseRegister);
#else
  UNREACHABLE();
#endif
}

void MacroAssembler::LoadSandboxedPointerField(Register destination,
                                               Operand field_operand) {
  ASM_CODE_COMMENT(this);
  movq(destination, field_operand);
  DecodeSandboxedPointer(destination);
}

void MacroAssembler::StoreSandboxedPointerField(Operand dst_field_operand,
                                                Register value) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(value, kScratchRegister));
  DCHECK(!dst_field_operand.AddressUsesRegister(kScratchRegister));
  movq(kScratchRegister, value);
  EncodeSandboxedPointer(kScratchRegister);
  movq(dst_field_operand, kScratchRegister);
}

void MacroAssembler::LoadExternalPointerField(
    Register destination, Operand field_operand, ExternalPointerTag tag,
    Register scratch, IsolateRootLocation isolateRootLocation) {
  DCHECK(!AreAliased(destination, scratch));
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  DCHECK(!IsSharedExternalPointerType(tag));
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
#endif  // V8_ENABLE_SANDBOX
}

void MacroAssembler::CallEphemeronKeyBarrier(Register object,
                                             Register slot_address,
                                             SaveFPRegsMode fp_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  PushAll(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();
  MovePair(slot_address_parameter, slot_address, object_parameter, object);

  Call(isolate()->builtins()->code_handle(
            Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
        RelocInfo::CODE_TARGET);
  PopAll(registers);
}

void MacroAssembler::CallRecordWriteStubSaveRegisters(Register object,
                                                      Register slot_address,
                                                      SaveFPRegsMode fp_mode,
                                                      StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  PushAll(registers);
  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();
  MovePair(object_parameter, object, slot_address_parameter, slot_address);

  CallRecordWriteStub(object_parameter, slot_address_parameter, fp_mode, mode);
  PopAll(registers);
}

void MacroAssembler::CallRecordWriteStub(Register object, Register slot_address,
                                         SaveFPRegsMode fp_mode,
                                         StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  // Use CallRecordWriteStubSaveRegisters if the object and slot registers
  // need to be caller saved.
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
#if V8_ENABLE_WEBASSEMBLY
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    // Use {near_call} for direct Wasm call within a module.
    auto wasm_target = wasm::WasmCode::GetRecordWriteStub(fp_mode);
    near_call(wasm_target, RelocInfo::WASM_STUB_CALL);
#else
  if (false) {
#endif
  } else {
    Builtin builtin = Builtins::GetRecordWriteStub(fp_mode);
    CallBuiltin(builtin);
  }
}

#ifdef V8_IS_TSAN
void MacroAssembler::CallTSANStoreStub(Register address, Register value,
                                       SaveFPRegsMode fp_mode, int size,
                                       StubCallMode mode,
                                       std::memory_order order) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(address, value));
  TSANStoreDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  PushAll(registers);

  Register address_parameter(
      descriptor.GetRegisterParameter(TSANStoreDescriptor::kAddress));
  Register value_parameter(
      descriptor.GetRegisterParameter(TSANStoreDescriptor::kValue));

  // Prepare argument registers for calling GetTSANStoreStub.
  MovePair(address_parameter, address, value_parameter, value);

  if (isolate()) {
    Builtin builtin = CodeFactory::GetTSANStoreStub(fp_mode, size, order);
    Handle<Code> code_target = isolate()->builtins()->code_handle(builtin);
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

  PopAll(registers);
}

void MacroAssembler::CallTSANRelaxedLoadStub(Register address,
                                             SaveFPRegsMode fp_mode, int size,
                                             StubCallMode mode) {
  TSANLoadDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  PushAll(registers);

  Register address_parameter(
      descriptor.GetRegisterParameter(TSANLoadDescriptor::kAddress));

  // Prepare argument registers for calling TSANRelaxedLoad.
  Move(address_parameter, address);

  if (isolate()) {
    Builtin builtin = CodeFactory::GetTSANRelaxedLoadStub(fp_mode, size);
    Handle<Code> code_target = isolate()->builtins()->code_handle(builtin);
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

  PopAll(registers);
}
#endif  // V8_IS_TSAN

void MacroAssembler::RecordWrite(Register object, Register slot_address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, slot_address, value));
  AssertNotSmi(object);

  if (v8_flags.disable_write_barriers) {
    return;
  }

  if (v8_flags.debug_code) {
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

  CallRecordWriteStub(object, slot_address, fp_mode);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Zap scratch registers");
    Move(slot_address, kZapValue, RelocInfo::NO_INFO);
    Move(value, kZapValue, RelocInfo::NO_INFO);
  }
}

void MacroAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L, Label::kNear);
  Abort(reason);
  // Control will not return here.
  bind(&L);
}

void MacroAssembler::CheckStackAlignment() {
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

void MacroAssembler::Abort(AbortReason reason) {
  ASM_CODE_COMMENT(this);
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
    Move(arg_reg_1, static_cast<int>(reason));
    PrepareCallCFunction(1);
    LoadAddress(rax, ExternalReference::abort_with_reason());
    call(rax);
    return;
  }

  Move(rdx, Smi::FromInt(static_cast<int>(reason)));

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

  // Control will not return here.
  int3();
}

void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments) {
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
  Handle<Code> code = CodeFactory::CEntry(isolate(), f->result_size);
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
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), 1, ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

namespace {

void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                               Register optimized_code_entry, Register closure,
                               Register scratch1, Register scratch2,
                               JumpMode jump_mode) {
  // ----------- S t a t e -------------
  //  rax : actual argument count
  //  rdx : new target (preserved for callee if needed, and caller)
  //  rsi : current context, used for the runtime call
  //  rdi : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  ASM_CODE_COMMENT(masm);
  DCHECK_EQ(closure, kJSFunctionRegister);
  DCHECK(!AreAliased(rax, rdx, closure, rsi, optimized_code_entry, scratch1,
                     scratch2));

  Label heal_optimized_code_slot;

  // If the optimized code is cleared, go to runtime to update the optimization
  // marker field.
  __ LoadWeakValue(optimized_code_entry, &heal_optimized_code_slot);

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  __ AssertCode(optimized_code_entry);
  __ TestCodeIsMarkedForDeoptimization(optimized_code_entry);
  __ j(not_zero, &heal_optimized_code_slot);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  __ ReplaceClosureCodeWithOptimizedCode(optimized_code_entry, closure,
                                         scratch1, scratch2);
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  __ Move(rcx, optimized_code_entry);
  __ JumpCodeObject(rcx, jump_mode);

  // Optimized code slot contains deoptimized code or code is cleared and
  // optimized code marker isn't updated. Evict the code, update the marker
  // and re-enter the closure's code.
  __ bind(&heal_optimized_code_slot);
  __ GenerateTailCallToReturnedCode(Runtime::kHealOptimizedCodeSlot, jump_mode);
}

}  // namespace

#ifdef V8_ENABLE_DEBUG_CODE
void MacroAssembler::AssertFeedbackVector(Register object) {
  if (v8_flags.debug_code) {
    IsObjectType(object, FEEDBACK_VECTOR_TYPE, kScratchRegister);
    Assert(equal, AbortReason::kExpectedFeedbackVector);
  }
}
#endif  // V8_ENABLE_DEBUG_CODE

void MacroAssembler::GenerateTailCallToReturnedCode(
    Runtime::FunctionId function_id, JumpMode jump_mode) {
  // ----------- S t a t e -------------
  //  -- rax : actual argument count
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  ASM_CODE_COMMENT(this);
  {
    FrameScope scope(this, StackFrame::INTERNAL);
    // Push a copy of the target function, the new target and the actual
    // argument count.
    Push(kJavaScriptCallTargetRegister);
    Push(kJavaScriptCallNewTargetRegister);
    SmiTag(kJavaScriptCallArgCountRegister);
    Push(kJavaScriptCallArgCountRegister);
    // Function is also the parameter to the runtime call.
    Push(kJavaScriptCallTargetRegister);

    CallRuntime(function_id, 1);
    movq(rcx, rax);

    // Restore target function, new target and actual argument count.
    Pop(kJavaScriptCallArgCountRegister);
    SmiUntagUnsigned(kJavaScriptCallArgCountRegister);
    Pop(kJavaScriptCallNewTargetRegister);
    Pop(kJavaScriptCallTargetRegister);
  }
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  JumpCodeObject(rcx, jump_mode);
}

void MacroAssembler::ReplaceClosureCodeWithOptimizedCode(
    Register optimized_code, Register closure, Register scratch1,
    Register slot_address) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(optimized_code, closure, scratch1, slot_address));
  DCHECK_EQ(closure, kJSFunctionRegister);
  // Store the optimized code in the closure.
  AssertCode(optimized_code);
  StoreTaggedField(FieldOperand(closure, JSFunction::kCodeOffset),
                   optimized_code);
  // Write barrier clobbers scratch1 below.
  Register value = scratch1;
  movq(value, optimized_code);

  RecordWriteField(closure, JSFunction::kCodeOffset, value, slot_address,
                   SaveFPRegsMode::kIgnore, SmiCheck::kOmit);
}

// Read off the flags in the feedback vector and check if there
// is optimized code or a tiering state that needs to be processed.
void MacroAssembler::LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
    Register flags, Register feedback_vector, CodeKind current_code_kind,
    Label* flags_need_processing) {
  ASM_CODE_COMMENT(this);
  DCHECK(CodeKindCanTierUp(current_code_kind));
  movzxwl(flags, FieldOperand(feedback_vector, FeedbackVector::kFlagsOffset));
  uint32_t kFlagsMask = FeedbackVector::kFlagsTieringStateIsAnyRequested |
                        FeedbackVector::kFlagsMaybeHasTurbofanCode |
                        FeedbackVector::kFlagsLogNextExecution;
  if (current_code_kind != CodeKind::MAGLEV) {
    kFlagsMask |= FeedbackVector::kFlagsMaybeHasMaglevCode;
  }
  testw(flags, Immediate(kFlagsMask));
  j(not_zero, flags_need_processing);
}

void MacroAssembler::OptimizeCodeOrTailCallOptimizedCodeSlot(
    Register flags, Register feedback_vector, Register closure,
    JumpMode jump_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(flags, feedback_vector, closure));
  Label maybe_has_optimized_code, maybe_needs_logging;
  // Check if optimized code is available.
  testl(flags, Immediate(FeedbackVector::kFlagsTieringStateIsAnyRequested));
  j(zero, &maybe_needs_logging);

  GenerateTailCallToReturnedCode(Runtime::kCompileOptimized, jump_mode);

  bind(&maybe_needs_logging);
  testl(flags, Immediate(FeedbackVector::LogNextExecutionBit::kMask));
  j(zero, &maybe_has_optimized_code);
  GenerateTailCallToReturnedCode(Runtime::kFunctionLogNextExecution, jump_mode);

  bind(&maybe_has_optimized_code);
  Register optimized_code_entry = flags;
  LoadTaggedField(
      optimized_code_entry,
      FieldOperand(feedback_vector, FeedbackVector::kMaybeOptimizedCodeOffset));
  TailCallOptimizedCodeSlot(this, optimized_code_entry, closure, r9,
                            WriteBarrierDescriptor::SlotAddressRegister(),
                            jump_mode);
}

int MacroAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion) const {
  int bytes = 0;
  RegList saved_regs = kCallerSaved - exclusion;
  bytes += kSystemPointerSize * saved_regs.Count();

  // R12 to r15 are callee save on all platforms.
  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += kStackSavedSavedFPSize * kAllocatableDoubleRegisters.Count();
  }

  return bytes;
}

int MacroAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                    Register exclusion) {
  ASM_CODE_COMMENT(this);
  int bytes = 0;
  bytes += PushAll(kCallerSaved - exclusion);
  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += PushAll(kAllocatableDoubleRegisters);
  }

  return bytes;
}

int MacroAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion) {
  ASM_CODE_COMMENT(this);
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += PopAll(kAllocatableDoubleRegisters);
  }
  bytes += PopAll(kCallerSaved - exclusion);

  return bytes;
}

int MacroAssembler::PushAll(RegList registers) {
  int bytes = 0;
  for (Register reg : registers) {
    pushq(reg);
    bytes += kSystemPointerSize;
  }
  return bytes;
}

int MacroAssembler::PopAll(RegList registers) {
  int bytes = 0;
  for (Register reg : base::Reversed(registers)) {
    popq(reg);
    bytes += kSystemPointerSize;
  }
  return bytes;
}

int MacroAssembler::PushAll(DoubleRegList registers, int stack_slot_size) {
  if (registers.is_empty()) return 0;
  const int delta = stack_slot_size * registers.Count();
  AllocateStackSpace(delta);
  int slot = 0;
  for (XMMRegister reg : registers) {
    if (stack_slot_size == kDoubleSize) {
      Movsd(Operand(rsp, slot), reg);
    } else {
      DCHECK_EQ(stack_slot_size, 2 * kDoubleSize);
      Movdqu(Operand(rsp, slot), reg);
    }
    slot += stack_slot_size;
  }
  DCHECK_EQ(slot, delta);
  return delta;
}

int MacroAssembler::PopAll(DoubleRegList registers, int stack_slot_size) {
  if (registers.is_empty()) return 0;
  int slot = 0;
  for (XMMRegister reg : registers) {
    if (stack_slot_size == kDoubleSize) {
      Movsd(reg, Operand(rsp, slot));
    } else {
      DCHECK_EQ(stack_slot_size, 2 * kDoubleSize);
      Movdqu(reg, Operand(rsp, slot));
    }
    slot += stack_slot_size;
  }
  DCHECK_EQ(slot, stack_slot_size * registers.Count());
  addq(rsp, Immediate(slot));
  return slot;
}

void MacroAssembler::Movq(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void MacroAssembler::Movq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void MacroAssembler::Pextrq(Register dst, XMMRegister src, int8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpextrq(dst, src, imm8);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrq(dst, src, imm8);
  }
}

void MacroAssembler::Cvtss2sd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, src, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void MacroAssembler::Cvtss2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, dst, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void MacroAssembler::Cvtsd2ss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, src, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void MacroAssembler::Cvtsd2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, dst, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void MacroAssembler::Cvtlsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void MacroAssembler::Cvtlsi2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void MacroAssembler::Cvtlsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void MacroAssembler::Cvtlsi2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtlsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void MacroAssembler::Cvtqsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void MacroAssembler::Cvtqsi2ss(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2ss(dst, kScratchDoubleReg, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void MacroAssembler::Cvtqsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void MacroAssembler::Cvtqsi2sd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtqsi2sd(dst, kScratchDoubleReg, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void MacroAssembler::Cvtlui2ss(XMMRegister dst, Register src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2ss(dst, kScratchRegister);
}

void MacroAssembler::Cvtlui2ss(XMMRegister dst, Operand src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2ss(dst, kScratchRegister);
}

void MacroAssembler::Cvtlui2sd(XMMRegister dst, Register src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2sd(dst, kScratchRegister);
}

void MacroAssembler::Cvtlui2sd(XMMRegister dst, Operand src) {
  // Zero-extend the 32 bit value to 64 bit.
  movl(kScratchRegister, src);
  Cvtqsi2sd(dst, kScratchRegister);
}

void MacroAssembler::Cvtqui2ss(XMMRegister dst, Register src) {
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

void MacroAssembler::Cvtqui2ss(XMMRegister dst, Operand src) {
  movq(kScratchRegister, src);
  Cvtqui2ss(dst, kScratchRegister);
}

void MacroAssembler::Cvtqui2sd(XMMRegister dst, Register src) {
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

void MacroAssembler::Cvtqui2sd(XMMRegister dst, Operand src) {
  movq(kScratchRegister, src);
  Cvtqui2sd(dst, kScratchRegister);
}

void MacroAssembler::Cvttss2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void MacroAssembler::Cvttss2si(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void MacroAssembler::Cvttsd2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void MacroAssembler::Cvttsd2si(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void MacroAssembler::Cvttss2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void MacroAssembler::Cvttss2siq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void MacroAssembler::Cvttsd2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}

void MacroAssembler::Cvttsd2siq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}

namespace {
template <typename OperandOrXMMRegister, bool is_double>
void ConvertFloatToUint64(MacroAssembler* masm, Register dst,
                          OperandOrXMMRegister src, Label* fail) {
  Label success;
  // There does not exist a native float-to-uint instruction, so we have to use
  // a float-to-int, and postprocess the result.
  if (is_double) {
    masm->Cvttsd2siq(dst, src);
  } else {
    masm->Cvttss2siq(dst, src);
  }
  // If the result of the conversion is positive, we are already done.
  masm->testq(dst, dst);
  masm->j(positive, &success);
  // The result of the first conversion was negative, which means that the
  // input value was not within the positive int64 range. We subtract 2^63
  // and convert it again to see if it is within the uint64 range.
  if (is_double) {
    masm->Move(kScratchDoubleReg, -9223372036854775808.0);
    masm->Addsd(kScratchDoubleReg, src);
    masm->Cvttsd2siq(dst, kScratchDoubleReg);
  } else {
    masm->Move(kScratchDoubleReg, -9223372036854775808.0f);
    masm->Addss(kScratchDoubleReg, src);
    masm->Cvttss2siq(dst, kScratchDoubleReg);
  }
  masm->testq(dst, dst);
  // The only possible negative value here is 0x8000000000000000, which is
  // used on x64 to indicate an integer overflow.
  masm->j(negative, fail ? fail : &success);
  // The input value is within uint64 range and the second conversion worked
  // successfully, but we still have to undo the subtraction we did
  // earlier.
  masm->Move(kScratchRegister, 0x8000000000000000);
  masm->orq(dst, kScratchRegister);
  masm->bind(&success);
}

template <typename OperandOrXMMRegister, bool is_double>
void ConvertFloatToUint32(MacroAssembler* masm, Register dst,
                          OperandOrXMMRegister src, Label* fail) {
  Label success;
  // There does not exist a native float-to-uint instruction, so we have to use
  // a float-to-int, and postprocess the result.
  if (is_double) {
    masm->Cvttsd2si(dst, src);
  } else {
    masm->Cvttss2si(dst, src);
  }
  // If the result of the conversion is positive, we are already done.
  masm->testl(dst, dst);
  masm->j(positive, &success);
  // The result of the first conversion was negative, which means that the
  // input value was not within the positive int32 range. We subtract 2^31
  // and convert it again to see if it is within the uint32 range.
  if (is_double) {
    masm->Move(kScratchDoubleReg, -2147483648.0);
    masm->Addsd(kScratchDoubleReg, src);
    masm->Cvttsd2si(dst, kScratchDoubleReg);
  } else {
    masm->Move(kScratchDoubleReg, -2147483648.0f);
    masm->Addss(kScratchDoubleReg, src);
    masm->Cvttss2si(dst, kScratchDoubleReg);
  }
  masm->testl(dst, dst);
  // The only possible negative value here is 0x80000000, which is
  // used on x64 to indicate an integer overflow.
  masm->j(negative, fail ? fail : &success);
  // The input value is within uint32 range and the second conversion worked
  // successfully, but we still have to undo the subtraction we did
  // earlier.
  masm->Move(kScratchRegister, 0x80000000);
  masm->orl(dst, kScratchRegister);
  masm->bind(&success);
}
}  // namespace

void MacroAssembler::Cvttsd2uiq(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint64<Operand, true>(this, dst, src, fail);
}

void MacroAssembler::Cvttsd2uiq(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint64<XMMRegister, true>(this, dst, src, fail);
}

void MacroAssembler::Cvttsd2ui(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint32<Operand, true>(this, dst, src, fail);
}

void MacroAssembler::Cvttsd2ui(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint32<XMMRegister, true>(this, dst, src, fail);
}

void MacroAssembler::Cvttss2uiq(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint64<Operand, false>(this, dst, src, fail);
}

void MacroAssembler::Cvttss2uiq(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint64<XMMRegister, false>(this, dst, src, fail);
}

void MacroAssembler::Cvttss2ui(Register dst, Operand src, Label* fail) {
  ConvertFloatToUint32<Operand, false>(this, dst, src, fail);
}

void MacroAssembler::Cvttss2ui(Register dst, XMMRegister src, Label* fail) {
  ConvertFloatToUint32<XMMRegister, false>(this, dst, src, fail);
}

void MacroAssembler::Cmpeqss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vcmpeqss(dst, src);
  } else {
    cmpeqss(dst, src);
  }
}

void MacroAssembler::Cmpeqsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vcmpeqsd(dst, src);
  } else {
    cmpeqsd(dst, src);
  }
}

// ----------------------------------------------------------------------------
// Smi tagging, untagging and tag detection.

Register MacroAssembler::GetSmiConstant(Smi source) {
  Move(kScratchRegister, source);
  return kScratchRegister;
}

void MacroAssembler::Cmp(Register dst, int32_t src) {
  if (src == 0) {
    testl(dst, dst);
  } else {
    cmpl(dst, Immediate(src));
  }
}

void MacroAssembler::SmiTag(Register reg) {
  static_assert(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK_EQ(kSmiShift, 1);
    addl(reg, reg);
  } else {
    shlq(reg, Immediate(kSmiShift));
  }
}

void MacroAssembler::SmiTag(Register dst, Register src) {
  DCHECK(dst != src);
  if (COMPRESS_POINTERS_BOOL) {
    movl(dst, src);
  } else {
    movq(dst, src);
  }
  SmiTag(dst);
}

void MacroAssembler::SmiUntag(Register reg) {
  static_assert(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  // TODO(v8:7703): Is there a way to avoid this sign extension when pointer
  // compression is enabled?
  if (COMPRESS_POINTERS_BOOL) {
    movsxlq(reg, reg);
  }
  sarq(reg, Immediate(kSmiShift));
}

void MacroAssembler::SmiUntagUnsigned(Register reg) {
  static_assert(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  if (COMPRESS_POINTERS_BOOL) {
    AssertSignedBitOfSmiIsZero(reg);
    shrl(reg, Immediate(kSmiShift));
  } else {
    shrq(reg, Immediate(kSmiShift));
  }
}

void MacroAssembler::SmiUntag(Register dst, Register src) {
  DCHECK(dst != src);
  if (COMPRESS_POINTERS_BOOL) {
    movsxlq(dst, src);
  } else {
    movq(dst, src);
  }
  // TODO(v8:7703): Call SmiUntag(reg) if we can find a way to avoid the extra
  // mov when pointer compression is enabled.
  static_assert(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  sarq(dst, Immediate(kSmiShift));
}

void MacroAssembler::SmiUntag(Register dst, Operand src) {
  if (SmiValuesAre32Bits()) {
    // Sign extend to 64-bit.
    movsxlq(dst, Operand(src, kSmiShift / kBitsPerByte));
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

void MacroAssembler::SmiUntagUnsigned(Register dst, Operand src) {
  if (SmiValuesAre32Bits()) {
    // Zero extend to 64-bit.
    movl(dst, Operand(src, kSmiShift / kBitsPerByte));
  } else {
    DCHECK(SmiValuesAre31Bits());
    if (COMPRESS_POINTERS_BOOL) {
      movl(dst, src);
      AssertSignedBitOfSmiIsZero(dst);
      shrl(dst, Immediate(kSmiShift));
    } else {
      movq(dst, src);
      shrq(dst, Immediate(kSmiShift));
    }
  }
}

void MacroAssembler::SmiToInt32(Register reg) {
  static_assert(kSmiTag == 0);
  DCHECK(SmiValuesAre32Bits() || SmiValuesAre31Bits());
  if (COMPRESS_POINTERS_BOOL) {
    sarl(reg, Immediate(kSmiShift));
  } else {
    shrq(reg, Immediate(kSmiShift));
  }
}

void MacroAssembler::SmiToInt32(Register dst, Register src) {
  DCHECK(dst != src);
  mov_tagged(dst, src);
  SmiToInt32(dst);
}

void MacroAssembler::SmiCompare(Register smi1, Register smi2) {
  AssertSmi(smi1);
  AssertSmi(smi2);
  cmp_tagged(smi1, smi2);
}

void MacroAssembler::SmiCompare(Register dst, Smi src) {
  AssertSmi(dst);
  Cmp(dst, src);
}

void MacroAssembler::Cmp(Register dst, Smi src) {
  if (src.value() == 0) {
    test_tagged(dst, dst);
  } else if (COMPRESS_POINTERS_BOOL) {
    cmp_tagged(dst, Immediate(src));
  } else {
    DCHECK_NE(dst, kScratchRegister);
    Register constant_reg = GetSmiConstant(src);
    cmp_tagged(dst, constant_reg);
  }
}

void MacroAssembler::SmiCompare(Register dst, Operand src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmp_tagged(dst, src);
}

void MacroAssembler::SmiCompare(Operand dst, Register src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmp_tagged(dst, src);
}

void MacroAssembler::SmiCompare(Operand dst, Smi src) {
  AssertSmi(dst);
  if (SmiValuesAre32Bits()) {
    cmpl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(src.value()));
  } else {
    DCHECK(SmiValuesAre31Bits());
    cmpl(dst, Immediate(src));
  }
}

void MacroAssembler::Cmp(Operand dst, Smi src) {
  // The Operand cannot use the smi register.
  Register smi_reg = GetSmiConstant(src);
  DCHECK(!dst.AddressUsesRegister(smi_reg));
  cmp_tagged(dst, smi_reg);
}

Condition MacroAssembler::CheckSmi(Register src) {
  static_assert(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

Condition MacroAssembler::CheckSmi(Operand src) {
  static_assert(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

void MacroAssembler::JumpIfSmi(Register src, Label* on_smi,
                               Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(smi, on_smi, near_jump);
}

void MacroAssembler::JumpIfNotSmi(Register src, Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
}

void MacroAssembler::JumpIfNotSmi(Operand src, Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
}

void MacroAssembler::SmiAddConstant(Operand dst, Smi constant) {
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

SmiIndex MacroAssembler::SmiToIndex(Register dst, Register src, int shift) {
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

void MacroAssembler::Switch(Register scratch, Register reg, int case_value_base,
                            Label** labels, int num_labels) {
  Register table = scratch;
  Label fallthrough, jump_table;
  if (case_value_base != 0) {
    subq(reg, Immediate(case_value_base));
  }
  cmpq(reg, Immediate(num_labels));
  j(above_equal, &fallthrough);
  leaq(table, MemOperand(&jump_table));
  jmp(MemOperand(table, reg, times_8, 0));
  // Emit the jump table inline, under the assumption that it's not too big.
  Align(kSystemPointerSize);
  bind(&jump_table);
  for (int i = 0; i < num_labels; ++i) {
    dq(labels[i]);
  }
  bind(&fallthrough);
}

void MacroAssembler::Push(Smi source) {
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

void MacroAssembler::Move(Register dst, Smi source) {
  static_assert(kSmiTag == 0);
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

void MacroAssembler::Move(Operand dst, intptr_t x) {
  if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else {
    Move(kScratchRegister, x);
    movq(dst, kScratchRegister);
  }
}

void MacroAssembler::Move(Register dst, ExternalReference ext) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadExternalReference(dst, ext);
    return;
  }
  movq(dst, Immediate64(ext.address(), RelocInfo::EXTERNAL_REFERENCE));
}

void MacroAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    movq(dst, src);
  }
}

void MacroAssembler::Move(Register dst, Operand src) { movq(dst, src); }
void MacroAssembler::Move(Register dst, Immediate src) {
  if (src.rmode() == RelocInfo::Mode::NO_INFO) {
    Move(dst, src.value());
  } else {
    movl(dst, src);
  }
}

void MacroAssembler::Move(XMMRegister dst, XMMRegister src) {
  if (dst != src) {
    Movaps(dst, src);
  }
}

void MacroAssembler::MovePair(Register dst0, Register src0, Register dst1,
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

void MacroAssembler::MoveNumber(Register dst, double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) {
    Move(dst, Smi::FromInt(smi));
  } else {
    movq_heap_number(dst, value);
  }
}

void MacroAssembler::Move(XMMRegister dst, uint32_t src) {
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

void MacroAssembler::Move(XMMRegister dst, uint64_t src) {
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

void MacroAssembler::Move(XMMRegister dst, uint64_t high, uint64_t low) {
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
  } else if (root_array_available_ && options().isolate_independent_code) {
    // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
    // non-isolate-independent code. In many cases it might be cheaper than
    // embedding the relocatable value.
    // TODO(v8:9706): Fix-it! This load will always uncompress the value
    // even when we are loading a compressed embedded object.
    IndirectLoadConstant(kScratchRegister, Handle<HeapObject>::cast(source));
    cmp_tagged(dst, kScratchRegister);
  } else if (COMPRESS_POINTERS_BOOL) {
    EmbeddedObjectIndex index =
        AddEmbeddedObject(Handle<HeapObject>::cast(source));
    DCHECK(is_uint32(index));
    cmpl(dst, Immediate(static_cast<int>(index),
                        RelocInfo::COMPRESSED_EMBEDDED_OBJECT));
  } else {
    movq(kScratchRegister,
         Immediate64(source.address(), RelocInfo::FULL_EMBEDDED_OBJECT));
    cmpq(dst, kScratchRegister);
  }
}

void MacroAssembler::Cmp(Operand dst, Handle<Object> source) {
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else if (root_array_available_ && options().isolate_independent_code) {
    // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
    // non-isolate-independent code. In many cases it might be cheaper than
    // embedding the relocatable value.
    // TODO(v8:9706): Fix-it! This load will always uncompress the value
    // even when we are loading a compressed embedded object.
    IndirectLoadConstant(kScratchRegister, Handle<HeapObject>::cast(source));
    cmp_tagged(dst, kScratchRegister);
  } else if (COMPRESS_POINTERS_BOOL) {
    EmbeddedObjectIndex index =
        AddEmbeddedObject(Handle<HeapObject>::cast(source));
    DCHECK(is_uint32(index));
    cmpl(dst, Immediate(static_cast<int>(index),
                        RelocInfo::COMPRESSED_EMBEDDED_OBJECT));
  } else {
    Move(kScratchRegister, Handle<HeapObject>::cast(source),
         RelocInfo::FULL_EMBEDDED_OBJECT);
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

void MacroAssembler::Push(Handle<HeapObject> source) {
  Move(kScratchRegister, source);
  Push(kScratchRegister);
}

void MacroAssembler::PushArray(Register array, Register size, Register scratch,
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

void MacroAssembler::Move(Register result, Handle<HeapObject> object,
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

void MacroAssembler::Move(Operand dst, Handle<HeapObject> object,
                          RelocInfo::Mode rmode) {
  Move(kScratchRegister, object, rmode);
  movq(dst, kScratchRegister);
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

void MacroAssembler::DropArguments(Register count, ArgumentsCountType type,
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

void MacroAssembler::DropArguments(Register count, Register scratch,
                                   ArgumentsCountType type,
                                   ArgumentsCountMode mode) {
  DCHECK(!AreAliased(count, scratch));
  PopReturnAddressTo(scratch);
  DropArguments(count, type, mode);
  PushReturnAddressFrom(scratch);
}

void MacroAssembler::DropArgumentsAndPushNewReceiver(Register argc,
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

void MacroAssembler::DropArgumentsAndPushNewReceiver(Register argc,
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

void MacroAssembler::Push(Register src) { pushq(src); }

void MacroAssembler::Push(Operand src) { pushq(src); }

void MacroAssembler::PushQuad(Operand src) { pushq(src); }

void MacroAssembler::Push(Immediate value) { pushq(value); }

void MacroAssembler::PushImm32(int32_t imm32) { pushq_imm32(imm32); }

void MacroAssembler::Pop(Register dst) { popq(dst); }

void MacroAssembler::Pop(Operand dst) { popq(dst); }

void MacroAssembler::PopQuad(Operand dst) { popq(dst); }

void MacroAssembler::Jump(const ExternalReference& reference) {
  DCHECK(root_array_available());
  jmp(Operand(kRootRegister, RootRegisterOffsetForExternalReferenceTableEntry(
                                 isolate(), reference)));
}

void MacroAssembler::Jump(Operand op) { jmp(op); }

void MacroAssembler::Jump(Operand op, Condition cc) {
  Label skip;
  j(NegateCondition(cc), &skip, Label::kNear);
  Jump(op);
  bind(&skip);
}

void MacroAssembler::Jump(Address destination, RelocInfo::Mode rmode) {
  Move(kScratchRegister, destination, rmode);
  jmp(kScratchRegister);
}

void MacroAssembler::Jump(Address destination, RelocInfo::Mode rmode,
                          Condition cc) {
  Label skip;
  j(NegateCondition(cc), &skip, Label::kNear);
  Jump(destination, rmode);
  bind(&skip);
}

void MacroAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode) {
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

void MacroAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode,
                          Condition cc) {
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code_object));
  Builtin builtin = Builtin::kNoBuiltinId;
  if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin)) {
    TailCallBuiltin(builtin, cc);
    return;
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  j(cc, code_object, rmode);
}

void MacroAssembler::Call(ExternalReference ext) {
  LoadAddress(kScratchRegister, ext);
  call(kScratchRegister);
}

void MacroAssembler::Call(Operand op) {
  if (!CpuFeatures::IsSupported(INTEL_ATOM)) {
    call(op);
  } else {
    movq(kScratchRegister, op);
    call(kScratchRegister);
  }
}

void MacroAssembler::Call(Address destination, RelocInfo::Mode rmode) {
  Move(kScratchRegister, destination, rmode);
  call(kScratchRegister);
}

void MacroAssembler::Call(Handle<Code> code_object, RelocInfo::Mode rmode) {
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

Operand MacroAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  DCHECK(root_array_available());
  return Operand(kRootRegister, IsolateData::BuiltinEntrySlotOffset(builtin));
}

Operand MacroAssembler::EntryFromBuiltinIndexAsOperand(Register builtin_index) {
  if (SmiValuesAre32Bits()) {
    // The builtin_index register contains the builtin index as a Smi.
    SmiUntagUnsigned(builtin_index);
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

void MacroAssembler::CallBuiltinByIndex(Register builtin_index) {
  Call(EntryFromBuiltinIndexAsOperand(builtin_index));
}

void MacroAssembler::CallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this, CommentForOffHeapTrampoline("call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute:
      Call(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
      break;
    case BuiltinCallJumpMode::kPCRelative:
      near_call(static_cast<intptr_t>(builtin), RelocInfo::NEAR_BUILTIN_ENTRY);
      break;
    case BuiltinCallJumpMode::kIndirect:
      Call(EntryFromBuiltinAsOperand(builtin));
      break;
    case BuiltinCallJumpMode::kForMksnapshot: {
      Handle<Code> code = isolate()->builtins()->code_handle(builtin);
      call(code, RelocInfo::CODE_TARGET);
      break;
    }
  }
}

void MacroAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute:
      Jump(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET);
      break;
    case BuiltinCallJumpMode::kPCRelative:
      near_jmp(static_cast<intptr_t>(builtin), RelocInfo::NEAR_BUILTIN_ENTRY);
      break;
    case BuiltinCallJumpMode::kIndirect:
      Jump(EntryFromBuiltinAsOperand(builtin));
      break;
    case BuiltinCallJumpMode::kForMksnapshot: {
      Handle<Code> code = isolate()->builtins()->code_handle(builtin);
      jmp(code, RelocInfo::CODE_TARGET);
      break;
    }
  }
}

void MacroAssembler::TailCallBuiltin(Builtin builtin, Condition cc) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute:
      Jump(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET, cc);
      break;
    case BuiltinCallJumpMode::kPCRelative:
      near_j(cc, static_cast<intptr_t>(builtin), RelocInfo::NEAR_BUILTIN_ENTRY);
      break;
    case BuiltinCallJumpMode::kIndirect:
      Jump(EntryFromBuiltinAsOperand(builtin), cc);
      break;
    case BuiltinCallJumpMode::kForMksnapshot: {
      Handle<Code> code = isolate()->builtins()->code_handle(builtin);
      j(cc, code, RelocInfo::CODE_TARGET);
      break;
    }
  }
}

void MacroAssembler::LoadCodeEntry(Register destination, Register code_object) {
  ASM_CODE_COMMENT(this);
  movq(destination, FieldOperand(code_object, Code::kCodeEntryPointOffset));
}

void MacroAssembler::CallCodeObject(Register code_object) {
  LoadCodeEntry(code_object, code_object);
  call(code_object);
}

void MacroAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
  LoadCodeEntry(code_object, code_object);
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

void MacroAssembler::PextrdPreSse41(Register dst, XMMRegister src,
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
void PinsrdPreSse41Helper(MacroAssembler* masm, XMMRegister dst, Op src,
                          uint8_t imm8, uint32_t* load_pc_offset) {
  masm->Movd(kScratchDoubleReg, src);
  if (load_pc_offset) *load_pc_offset = masm->pc_offset();
  if (imm8 == 1) {
    masm->punpckldq(dst, kScratchDoubleReg);
  } else {
    DCHECK_EQ(0, imm8);
    masm->Movss(dst, kScratchDoubleReg);
  }
}
}  // namespace

void MacroAssembler::PinsrdPreSse41(XMMRegister dst, Register src, uint8_t imm8,
                                    uint32_t* load_pc_offset) {
  PinsrdPreSse41Helper(this, dst, src, imm8, load_pc_offset);
}

void MacroAssembler::PinsrdPreSse41(XMMRegister dst, Operand src, uint8_t imm8,
                                    uint32_t* load_pc_offset) {
  PinsrdPreSse41Helper(this, dst, src, imm8, load_pc_offset);
}

void MacroAssembler::Pinsrq(XMMRegister dst, XMMRegister src1, Register src2,
                            uint8_t imm8, uint32_t* load_pc_offset) {
  PinsrHelper(this, &Assembler::vpinsrq, &Assembler::pinsrq, dst, src1, src2,
              imm8, load_pc_offset, {SSE4_1});
}

void MacroAssembler::Pinsrq(XMMRegister dst, XMMRegister src1, Operand src2,
                            uint8_t imm8, uint32_t* load_pc_offset) {
  PinsrHelper(this, &Assembler::vpinsrq, &Assembler::pinsrq, dst, src1, src2,
              imm8, load_pc_offset, {SSE4_1});
}

void MacroAssembler::Lzcntl(Register dst, Register src) {
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

void MacroAssembler::Lzcntl(Register dst, Operand src) {
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

void MacroAssembler::Lzcntq(Register dst, Register src) {
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

void MacroAssembler::Lzcntq(Register dst, Operand src) {
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

void MacroAssembler::Tzcntq(Register dst, Register src) {
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

void MacroAssembler::Tzcntq(Register dst, Operand src) {
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

void MacroAssembler::Tzcntl(Register dst, Register src) {
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

void MacroAssembler::Tzcntl(Register dst, Operand src) {
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

void MacroAssembler::Popcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void MacroAssembler::Popcntl(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void MacroAssembler::Popcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}

void MacroAssembler::Popcntq(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  static_assert(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  static_assert(StackHandlerConstants::kNextOffset == 0);

  Push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  Push(ExternalReferenceAsOperand(handler_address));

  // Set this new handler as the current one.
  movq(ExternalReferenceAsOperand(handler_address), rsp);
}

void MacroAssembler::PopStackHandler() {
  static_assert(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  Pop(ExternalReferenceAsOperand(handler_address));
  addq(rsp, Immediate(StackHandlerConstants::kSize - kSystemPointerSize));
}

void MacroAssembler::Ret() { ret(0); }

void MacroAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    PopReturnAddressTo(scratch);
    addq(rsp, Immediate(bytes_dropped));
    PushReturnAddressFrom(scratch);
    ret(0);
  }
}

void MacroAssembler::IncsspqIfSupported(Register number_of_words,
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

void MacroAssembler::IsObjectType(Register heap_object, InstanceType type,
                                  Register map) {
  if (V8_STATIC_ROOTS_BOOL) {
    if (base::Optional<RootIndex> expected =
            InstanceTypeChecker::UniqueMapOfInstanceType(type)) {
      LoadCompressedMap(map, heap_object);
      cmp_tagged(map, Immediate(ReadOnlyRootPtr(*expected)));
      return;
    }
  }
  CmpObjectType(heap_object, type, map);
}

void MacroAssembler::JumpIfJSAnyIsNotPrimitive(Register heap_object,
                                               Register scratch, Label* target,
                                               Label::Distance distance,
                                               Condition cc) {
  CHECK(cc == Condition::kUnsignedLessThan ||
        cc == Condition::kUnsignedGreaterThanEqual);
  if (V8_STATIC_ROOTS_BOOL) {
#ifdef DEBUG
    Label ok;
    LoadMap(scratch, heap_object);
    CmpInstanceTypeRange(scratch, scratch, FIRST_JS_RECEIVER_TYPE,
                         LAST_JS_RECEIVER_TYPE);
    j(Condition::kUnsignedLessThanEqual, &ok, Label::Distance::kNear);
    LoadMap(scratch, heap_object);
    CmpInstanceTypeRange(scratch, scratch, FIRST_PRIMITIVE_HEAP_OBJECT_TYPE,
                         LAST_PRIMITIVE_HEAP_OBJECT_TYPE);
    j(Condition::kUnsignedLessThanEqual, &ok, Label::Distance::kNear);
    Abort(AbortReason::kInvalidReceiver);
    bind(&ok);
#endif  // DEBUG

    // All primitive object's maps are allocated at the start of the read only
    // heap. Thus JS_RECEIVER's must have maps with larger (compressed)
    // addresses.
    LoadCompressedMap(scratch, heap_object);
    cmp_tagged(scratch, Immediate(InstanceTypeChecker::kNonJsReceiverMapLimit));
  } else {
    static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    CmpObjectType(heap_object, FIRST_JS_RECEIVER_TYPE, scratch);
  }
  j(cc, target, distance);
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

void MacroAssembler::TestCodeIsMarkedForDeoptimization(Register code) {
  testl(FieldOperand(code, Code::kKindSpecificFlagsOffset),
        Immediate(1 << Code::kMarkedForDeoptimizationBit));
}

Immediate MacroAssembler::ClearedValue() const {
  return Immediate(
      static_cast<int32_t>(HeapObjectReference::ClearedValue(isolate()).ptr()));
}

#ifdef V8_ENABLE_DEBUG_CODE
void MacroAssembler::AssertNotSmi(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(NegateCondition(is_smi), AbortReason::kOperandIsASmi);
}

void MacroAssembler::AssertSmi(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(is_smi, AbortReason::kOperandIsNotASmi);
}

void MacroAssembler::AssertSmi(Operand object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  Condition is_smi = CheckSmi(object);
  Check(is_smi, AbortReason::kOperandIsNotASmi);
}

void MacroAssembler::AssertZeroExtended(Register int32_register) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  DCHECK_NE(int32_register, kScratchRegister);
  movl(kScratchRegister, Immediate(kMaxUInt32));  // zero-extended
  cmpq(int32_register, kScratchRegister);
  Check(below_equal, AbortReason::k32BitValueInRegisterIsNotZeroExtended);
}

void MacroAssembler::AssertSignedBitOfSmiIsZero(Register smi_register) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  DCHECK(COMPRESS_POINTERS_BOOL);
  testl(smi_register, Immediate(int32_t{0x10000000}));
  Check(zero, AbortReason::kSignedBitOfSmiIsNotZero);
}

void MacroAssembler::AssertMap(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsNotAMap);
  Push(object);
  LoadMap(object, object);
  CmpInstanceType(object, MAP_TYPE);
  popq(object);
  Check(equal, AbortReason::kOperandIsNotAMap);
}

void MacroAssembler::AssertCode(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsNotACode);
  Push(object);
  LoadMap(object, object);
  CmpInstanceType(object, CODE_TYPE);
  popq(object);
  Check(equal, AbortReason::kOperandIsNotACode);
}

void MacroAssembler::AssertConstructor(Register object) {
  if (!v8_flags.debug_code) return;
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
  if (!v8_flags.debug_code) return;
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
  if (!v8_flags.debug_code) return;
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
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
  Push(object);
  IsObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
  Pop(object);
  Check(equal, AbortReason::kOperandIsNotABoundFunction);
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  Register map = object;
  Push(object);
  LoadMap(map, object);

  // Check if JSGeneratorObject
  CmpInstanceTypeRange(map, kScratchRegister, FIRST_JS_GENERATOR_OBJECT_TYPE,
                       LAST_JS_GENERATOR_OBJECT_TYPE);
  // Restore generator object to register and perform assertion
  Pop(object);
  Check(below_equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (!v8_flags.debug_code) return;
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

void MacroAssembler::Assert(Condition cc, AbortReason reason) {
  if (v8_flags.debug_code) Check(cc, reason);
}

void MacroAssembler::AssertUnreachable(AbortReason reason) {
  if (v8_flags.debug_code) Abort(reason);
}
#endif  // V8_ENABLE_DEBUG_CODE

void MacroAssembler::LoadWeakValue(Register in_out, Label* target_if_cleared) {
  cmpl(in_out, Immediate(kClearedWeakHeapObjectLower32));
  j(equal, target_if_cleared);

  andq(in_out, Immediate(~static_cast<int32_t>(kWeakHeapObjectMask)));
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
  if (v8_flags.native_code_counters && counter->Enabled()) {
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
  if (v8_flags.native_code_counters && counter->Enabled()) {
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
  LoadTaggedField(
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
  LoadTaggedField(rsi, FieldOperand(function, JSFunction::kContextOffset));
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

  AssertFunction(function);

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
  InvokePrologue(expected_parameter_count, actual_parameter_count, type);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  LoadTaggedField(rcx, FieldOperand(function, JSFunction::kCodeOffset));
  switch (type) {
    case InvokeType::kCall:
      CallCodeObject(rcx);
      break;
    case InvokeType::kJump:
      JumpCodeObject(rcx);
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
  DCHECK(MacroAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      MacroAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
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
                                    InvokeType type) {
    ASM_CODE_COMMENT(this);
    if (expected_parameter_count == actual_parameter_count) {
      Move(rax, actual_parameter_count);
      return;
    }
    Label regular_invoke;

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

void MacroAssembler::StubPrologue(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  pushq(rbp);  // Caller's frame pointer.
  movq(rbp, rsp);
  Push(Immediate(StackFrame::TypeToMarker(type)));
}

void MacroAssembler::Prologue() {
  ASM_CODE_COMMENT(this);
  pushq(rbp);  // Caller's frame pointer.
  movq(rbp, rsp);
  Push(kContextRegister);                 // Callee's context.
  Push(kJSFunctionRegister);              // Callee's JS function.
  Push(kJavaScriptCallArgCountRegister);  // Actual argument count.
}

void MacroAssembler::EnterFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  pushq(rbp);
  movq(rbp, rsp);
  if (!StackFrame::IsJavaScript(type)) {
    static_assert(CommonFrameConstants::kContextOrFrameTypeOffset ==
                  -kSystemPointerSize);
    Push(Immediate(StackFrame::TypeToMarker(type)));
  }
#if V8_ENABLE_WEBASSEMBLY
  if (type == StackFrame::WASM) Push(kWasmInstanceRegister);
#endif  // V8_ENABLE_WEBASSEMBLY
}

void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  // TODO(v8:11429): Consider passing BASELINE instead, and checking for
  // IsJSFrame or similar. Could then unify with manual frame leaves in the
  // interpreter too.
  if (v8_flags.debug_code && !StackFrame::IsJavaScript(type)) {
    cmpq(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
  }
  movq(rsp, rbp);
  popq(rbp);
}

#if defined(V8_TARGET_OS_WIN) || defined(V8_TARGET_OS_MACOS)
void MacroAssembler::AllocateStackSpace(Register bytes_scratch) {
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

void MacroAssembler::AllocateStackSpace(int bytes) {
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

void MacroAssembler::EnterExitFrame(int reserved_stack_slots,
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

  Push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kSystemPointerSize, ExitFrameConstants::kSPOffset);
  Push(Immediate(0));  // Saved entry sp, patched below.

  using ER = ExternalReference;
  Store(ER::Create(IsolateAddressId::kCEntryFPAddress, isolate()), rbp);
  Store(ER::Create(IsolateAddressId::kContextAddress, isolate()), rsi);
  Store(ER::Create(IsolateAddressId::kCFunctionAddress, isolate()), rbx);

#ifdef V8_TARGET_OS_WIN
  // Note this is only correct under the assumption that the caller hasn't
  // considered home stack slots already.
  // TODO(jgruber): This is a bit hacky since the caller in most cases still
  // needs to know about the home stack slots in order to address reserved
  // slots. Consider moving this fully into caller code.
  reserved_stack_slots += kWindowsHomeStackSlots;
#endif
  AllocateStackSpace(reserved_stack_slots * kSystemPointerSize);

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

void MacroAssembler::LeaveExitFrame() {
  ASM_CODE_COMMENT(this);

  leave();

  // Restore the current context from top and clear it in debug mode.
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
  LoadTaggedField(
      dst,
      FieldOperand(dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  // Load value from native context.
  LoadTaggedField(dst, Operand(dst, Context::SlotOffset(index)));
}

void MacroAssembler::TryLoadOptimizedOsrCode(Register scratch_and_result,
                                             Register feedback_vector,
                                             FeedbackSlot slot,
                                             Label* on_result,
                                             Label::Distance distance) {
  Label fallthrough;
  LoadTaggedField(
      scratch_and_result,
      FieldOperand(feedback_vector,
                   FeedbackVector::OffsetOfElementAt(slot.ToInt())));
  LoadWeakValue(scratch_and_result, &fallthrough);

  // Is it marked_for_deoptimization? If yes, clear the slot.
  {
    TestCodeIsMarkedForDeoptimization(scratch_and_result);
    j(equal, on_result, distance);
    StoreTaggedField(
        FieldOperand(feedback_vector,
                     FeedbackVector::OffsetOfElementAt(slot.ToInt())),
        ClearedValue());
  }

  bind(&fallthrough);
  Move(scratch_and_result, 0);
}

int MacroAssembler::ArgumentStackSlotsForCFunctionCall(int num_arguments) {
  DCHECK_GE(num_arguments, 0);
#ifdef V8_TARGET_OS_WIN
  return std::max(num_arguments, kWindowsHomeStackSlots);
#else
  return std::max(num_arguments - kRegisterPassedArguments, 0);
#endif
}

void MacroAssembler::PrepareCallCFunction(int num_arguments) {
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

void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments,
                                   SetIsolateDataSlots set_isolate_data_slots) {
  ASM_CODE_COMMENT(this);
  LoadAddress(rax, function);
  CallCFunction(rax, num_arguments, set_isolate_data_slots);
}

void MacroAssembler::CallCFunction(Register function, int num_arguments,
                                   SetIsolateDataSlots set_isolate_data_slots) {
  ASM_CODE_COMMENT(this);
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (v8_flags.debug_code) {
    CheckStackAlignment();
  }

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  Label get_pc;
  DCHECK(!AreAliased(kScratchRegister, function));
  leaq(kScratchRegister, Operand(&get_pc, 0));
  bind(&get_pc);

  if (set_isolate_data_slots == SetIsolateDataSlots::kYes) {
    // Addressing the following external references is tricky because we need
    // this to work in three situations:
    // 1. In wasm compilation, the isolate is nullptr and thus no
    //    ExternalReference can be created, but we can construct the address
    //    directly using the root register and a static offset.
    // 2. In normal JIT (and builtin) compilation, the external reference is
    //    usually addressed through the root register, so we can use the direct
    //    offset directly in most cases.
    // 3. In regexp compilation, the external reference is embedded into the
    // reloc
    //    info.
    // The solution here is to use root register offsets wherever possible in
    // which case we can construct it directly. When falling back to external
    // references we need to ensure that the scratch register does not get
    // accidentally overwritten. If we run into more such cases in the future,
    // we should implement a more general solution.
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
  }

  call(function);

  if (set_isolate_data_slots == SetIsolateDataSlots::kYes) {
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
  }

  DCHECK_NE(base::OS::ActivationFrameAlignment(), 0);
  DCHECK_GE(num_arguments, 0);
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  movq(rsp, Operand(rsp, argument_slots_on_stack * kSystemPointerSize));
}

void MacroAssembler::CheckPageFlag(Register object, Register scratch, int mask,
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

void MacroAssembler::ComputeCodeStartAddress(Register dst) {
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
//       the flags in the referenced {Code} object;
//    2. test kMarkedForDeoptimizationBit in those flags; and
//    3. if it is not zero then it jumps to the builtin.
void MacroAssembler::BailoutIfDeoptimized(Register scratch) {
  int offset = InstructionStream::kCodeOffset - InstructionStream::kHeaderSize;
  LoadTaggedField(scratch, Operand(kJavaScriptCallCodeStartRegister, offset));
  testl(FieldOperand(scratch, Code::kKindSpecificFlagsOffset),
        Immediate(1 << Code::kMarkedForDeoptimizationBit));
  Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
       RelocInfo::CODE_TARGET, not_zero);
}

void MacroAssembler::CallForDeoptimization(Builtin target, int, Label* exit,
                                           DeoptimizeKind kind, Label* ret,
                                           Label*) {
  ASM_CODE_COMMENT(this);
  // Note: Assembler::call is used here on purpose to guarantee fixed-size
  // exits even on Atom CPUs; see MacroAssembler::Call for Atom-specific
  // performance tuning which emits a different instruction sequence.
  call(EntryFromBuiltinAsOperand(target));
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void MacroAssembler::Trap() { int3(); }
void MacroAssembler::DebugBreak() { int3(); }

}  // namespace internal
}  // namespace v8

#undef __

#endif  // V8_TARGET_ARCH_X64
