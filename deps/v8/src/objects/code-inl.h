// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_INL_H_
#define V8_OBJECTS_CODE_INL_H_

#include "src/base/memory.h"
#include "src/baseline/bytecode-offset-iterator.h"
#include "src/codegen/code-desc.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/code.h"
#include "src/objects/dictionary.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/smi-inl.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/code-tq-inl.inc"

OBJECT_CONSTRUCTORS_IMPL(DeoptimizationData, FixedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(BytecodeArray)
OBJECT_CONSTRUCTORS_IMPL(AbstractCode, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(DependentCode, WeakArrayList)
OBJECT_CONSTRUCTORS_IMPL(Code, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(GcSafeCode, HeapObject)

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(GcSafeCode)
CAST_ACCESSOR(InstructionStream)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(DeoptimizationData)
CAST_ACCESSOR(DeoptimizationLiteralArray)

Code GcSafeCode::UnsafeCastToCode() const {
  return Code::unchecked_cast(*this);
}

#define GCSAFE_CODE_FWD_ACCESSOR(ReturnType, Name) \
  ReturnType GcSafeCode::Name() const { return UnsafeCastToCode().Name(); }
GCSAFE_CODE_FWD_ACCESSOR(Address, InstructionStart)
GCSAFE_CODE_FWD_ACCESSOR(Address, InstructionEnd)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_builtin)
GCSAFE_CODE_FWD_ACCESSOR(Builtin, builtin_id)
GCSAFE_CODE_FWD_ACCESSOR(CodeKind, kind)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_interpreter_trampoline_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_baseline_trampoline_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_baseline_leave_frame_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, has_instruction_stream)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_maglevved)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_turbofanned)
GCSAFE_CODE_FWD_ACCESSOR(bool, has_tagged_outgoing_params)
GCSAFE_CODE_FWD_ACCESSOR(bool, marked_for_deoptimization)
GCSAFE_CODE_FWD_ACCESSOR(Object, raw_instruction_stream)
GCSAFE_CODE_FWD_ACCESSOR(int, stack_slots)
GCSAFE_CODE_FWD_ACCESSOR(Address, constant_pool)
GCSAFE_CODE_FWD_ACCESSOR(Address, SafepointTableAddress)
#undef GCSAFE_CODE_FWD_ACCESSOR

int GcSafeCode::GetOffsetFromInstructionStart(Isolate* isolate,
                                              Address pc) const {
  return UnsafeCastToCode().GetOffsetFromInstructionStart(isolate, pc);
}

Address GcSafeCode::InstructionStart(Isolate* isolate, Address pc) const {
  return UnsafeCastToCode().InstructionStart(isolate, pc);
}

Address GcSafeCode::InstructionEnd(Isolate* isolate, Address pc) const {
  return UnsafeCastToCode().InstructionEnd(isolate, pc);
}

bool GcSafeCode::CanDeoptAt(Isolate* isolate, Address pc) const {
  DeoptimizationData deopt_data = DeoptimizationData::unchecked_cast(
      UnsafeCastToCode().unchecked_deoptimization_data());
  Address code_start_address = InstructionStart();
  for (int i = 0; i < deopt_data.DeoptCount(); i++) {
    if (deopt_data.Pc(i).value() == -1) continue;
    Address address = code_start_address + deopt_data.Pc(i).value();
    if (address == pc &&
        deopt_data.GetBytecodeOffset(i) != BytecodeOffset::None()) {
      return true;
    }
  }
  return false;
}

Object GcSafeCode::raw_instruction_stream(
    PtrComprCageBase code_cage_base) const {
  return UnsafeCastToCode().raw_instruction_stream(code_cage_base);
}

int AbstractCode::InstructionSize(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().InstructionSize();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().length();
  }
}

ByteArray AbstractCode::SourcePositionTableInternal(
    PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    Code code = GetCode();
    if (!code.has_instruction_stream()) {
      return GetReadOnlyRoots().empty_byte_array();
    }
    return code.source_position_table(cage_base);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SourcePositionTable(cage_base);
  }
}

ByteArray AbstractCode::SourcePositionTable(Isolate* isolate,
                                            SharedFunctionInfo sfi) {
  Map map_object = map(isolate);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().SourcePositionTable(isolate, sfi);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SourcePositionTable(isolate);
  }
}

int AbstractCode::SizeIncludingMetadata(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().SizeIncludingMetadata();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SizeIncludingMetadata();
  }
}

Address AbstractCode::InstructionStart(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().InstructionStart();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().GetFirstBytecodeAddress();
  }
}

Address AbstractCode::InstructionEnd(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().InstructionEnd();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    BytecodeArray bytecode_array = GetBytecodeArray();
    return bytecode_array.GetFirstBytecodeAddress() + bytecode_array.length();
  }
}

bool AbstractCode::contains(Isolate* isolate, Address inner_pointer) {
  PtrComprCageBase cage_base(isolate);
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().contains(isolate, inner_pointer);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return (address() <= inner_pointer) &&
           (inner_pointer <= address() + Size(cage_base));
  }
}

CodeKind AbstractCode::kind(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().kind();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return CodeKind::INTERPRETED_FUNCTION;
  }
}

Builtin AbstractCode::builtin_id(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().builtin_id();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return Builtin::kNoBuiltinId;
  }
}

bool AbstractCode::has_instruction_stream(PtrComprCageBase cage_base) {
  DCHECK(InstanceTypeChecker::IsCode(map(cage_base)));
  return GetCode().has_instruction_stream();
}

HandlerTable::CatchPrediction AbstractCode::GetBuiltinCatchPrediction(
    PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().GetBuiltinCatchPrediction();
  } else {
    UNREACHABLE();
  }
}

bool AbstractCode::IsCode(PtrComprCageBase cage_base) const {
  return HeapObject::IsCode(cage_base);
}

bool AbstractCode::IsBytecodeArray(PtrComprCageBase cage_base) const {
  return HeapObject::IsBytecodeArray(cage_base);
}

Code AbstractCode::GetCode() { return Code::cast(*this); }

BytecodeArray AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(*this);
}

OBJECT_CONSTRUCTORS_IMPL(InstructionStream, HeapObject)
NEVER_READ_ONLY_SPACE_IMPL(InstructionStream)

INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, metadata_size, kMetadataSizeOffset)
INT_ACCESSORS(Code, handler_table_offset, kHandlerTableOffsetOffset)
INT_ACCESSORS(Code, code_comments_offset, kCodeCommentsOffsetOffset)
INT32_ACCESSORS(Code, unwinding_info_offset, kUnwindingInfoOffsetOffset)
ACCESSORS(Code, relocation_info, ByteArray, kRelocationInfoOffset)
ACCESSORS_CHECKED2(Code, deoptimization_data, FixedArray,
                   kDeoptimizationDataOrInterpreterDataOffset,
                   kind() != CodeKind::BASELINE,
                   kind() != CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, bytecode_or_interpreter_data, HeapObject,
                   kDeoptimizationDataOrInterpreterDataOffset,
                   kind() == CodeKind::BASELINE,
                   kind() == CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, source_position_table, ByteArray, kPositionTableOffset,
                   kind() != CodeKind::BASELINE,
                   kind() != CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, bytecode_offset_table, ByteArray, kPositionTableOffset,
                   kind() == CodeKind::BASELINE,
                   kind() == CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))

// Same as RELEASE_ACQUIRE_ACCESSORS_CHECKED2 macro but with InstructionStream
// as a host and using main_cage_base(kRelaxedLoad) for computing the base.
#define RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS_CHECKED2(              \
    name, type, offset, get_condition, set_condition)                       \
  type InstructionStream::name(AcquireLoadTag tag) const {                  \
    PtrComprCageBase cage_base = main_cage_base(kRelaxedLoad);              \
    return InstructionStream::name(cage_base, tag);                         \
  }                                                                         \
  type InstructionStream::name(PtrComprCageBase cage_base, AcquireLoadTag)  \
      const {                                                               \
    type value = TaggedField<type, offset>::Acquire_Load(cage_base, *this); \
    DCHECK(get_condition);                                                  \
    return value;                                                           \
  }                                                                         \
  void InstructionStream::set_##name(type value, ReleaseStoreTag,           \
                                     WriteBarrierMode mode) {               \
    DCHECK(set_condition);                                                  \
    TaggedField<type, offset>::Release_Store(*this, value);                 \
    CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);                  \
  }

#define RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS(name, type, offset) \
  RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS_CHECKED2(                 \
      name, type, offset, !ObjectInYoungGeneration(value),               \
      !ObjectInYoungGeneration(value))

// Concurrent marker needs to access kind specific flags in code.
RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS(code, Code, kCodeOffset)
RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS(raw_code, HeapObject, kCodeOffset)
#undef RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS
#undef RELEASE_ACQUIRE_INSTRUCTION_STREAM_ACCESSORS_CHECKED2

PtrComprCageBase InstructionStream::main_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  Address cage_base_hi = ReadField<Tagged_t>(kMainCageBaseUpper32BitsOffset);
  return PtrComprCageBase(cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

PtrComprCageBase InstructionStream::main_cage_base(RelaxedLoadTag) const {
#ifdef V8_EXTERNAL_CODE_SPACE
  Address cage_base_hi =
      Relaxed_ReadField<Tagged_t>(kMainCageBaseUpper32BitsOffset);
  return PtrComprCageBase(cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void InstructionStream::set_main_cage_base(Address cage_base, RelaxedStoreTag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Tagged_t cage_base_hi = static_cast<Tagged_t>(cage_base >> 32);
  Relaxed_WriteField<Tagged_t>(kMainCageBaseUpper32BitsOffset, cage_base_hi);
#else
  UNREACHABLE();
#endif
}

Code InstructionStream::GCSafeCode(AcquireLoadTag) const {
  PtrComprCageBase cage_base = main_cage_base(kRelaxedLoad);
  HeapObject object =
      TaggedField<HeapObject, kCodeOffset>::Acquire_Load(cage_base, *this);
  DCHECK(!ObjectInYoungGeneration(object));
  Code code = ForwardingAddress(Code::unchecked_cast(object));
  return code;
}

// Helper functions for converting InstructionStream objects to
// Code and back.
inline Code ToCode(InstructionStream code) { return code.code(kAcquireLoad); }

inline InstructionStream FromCode(Code code) {
  DCHECK(code.has_instruction_stream());
  // Compute the InstructionStream object pointer from the code entry point.
  Address ptr =
      code.code_entry_point() - InstructionStream::kHeaderSize + kHeapObjectTag;
  return InstructionStream::cast(Object(ptr));
}

inline InstructionStream FromCode(Code code, PtrComprCageBase code_cage_base,
                                  RelaxedLoadTag tag) {
  DCHECK(code.has_instruction_stream());
  // Since the code entry point field is not aligned we can't load it atomically
  // and use for InstructionStream object pointer calculation. So, we load and
  // decompress the code field.
  return code.instruction_stream(code_cage_base, tag);
}

inline InstructionStream FromCode(Code code, Isolate* isolate,
                                  RelaxedLoadTag tag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  return FromCode(code, PtrComprCageBase{isolate->code_cage_base()}, tag);
#else
  return FromCode(code, GetPtrComprCageBase(code), tag);
#endif  // V8_EXTERNAL_CODE_SPACE
}

// TODO(jgruber): Remove this method once main_cage_base is gone.
void InstructionStream::WipeOutHeader() {
  WRITE_FIELD(*this, kCodeOffset, Smi::FromInt(0));
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    set_main_cage_base(kNullAddress, kRelaxedStore);
  }
}

void Code::ClearInstructionStreamPadding() {
  // Clear the padding after `body_end`.
  size_t trailing_padding_size =
      CodeSize() - InstructionStream::kHeaderSize - body_size();
  memset(reinterpret_cast<void*>(body_end()), 0, trailing_padding_size);
}

ByteArray Code::SourcePositionTable(Isolate* isolate,
                                    SharedFunctionInfo sfi) const {
  if (!has_instruction_stream()) {
    return GetReadOnlyRoots().empty_byte_array();
  }

  DisallowGarbageCollection no_gc;
  if (kind() == CodeKind::BASELINE) {
    return sfi.GetBytecodeArray(isolate).SourcePositionTable(isolate);
  }
  return source_position_table(isolate);
}

Address Code::body_start() const { return InstructionStart(); }

Address Code::body_end() const { return body_start() + body_size(); }

int Code::body_size() const { return instruction_size() + metadata_size(); }

// TODO(jgruber): Remove instruction_size.
int Code::InstructionSize() const { return instruction_size(); }

Address InstructionStream::instruction_start() const {
  return field_address(kHeaderSize);
}

Address Code::InstructionEnd() const {
  return InstructionStart() + instruction_size();
}

Address Code::metadata_start() const {
  return InstructionStart() + instruction_size();
}

Address Code::InstructionStart(Isolate* isolate, Address pc) const {
  return V8_LIKELY(has_instruction_stream())
             ? code_entry_point()
             : OffHeapInstructionStart(isolate, pc);
}

Address Code::InstructionEnd(Isolate* isolate, Address pc) const {
  return V8_LIKELY(has_instruction_stream())
             ? InstructionEnd()
             : OffHeapInstructionEnd(isolate, pc);
}

int Code::GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const {
  Address instruction_start = InstructionStart(isolate, pc);
  Address offset = pc - instruction_start;
  DCHECK_LE(offset, InstructionSize());
  return static_cast<int>(offset);
}

Address Code::metadata_end() const {
  return metadata_start() + metadata_size();
}

int Code::SizeIncludingMetadata() const {
  int size = CodeSize();
  size += relocation_info().Size();
  if (kind() != CodeKind::BASELINE) {
    size += deoptimization_data().Size();
  }
  return size;
}

Address Code::safepoint_table_address() const {
  return metadata_start() + safepoint_table_offset();
}

Address Code::SafepointTableAddress() const {
  return V8_LIKELY(has_instruction_stream()) ? safepoint_table_address()
                                             : OffHeapSafepointTableAddress();
}

int Code::safepoint_table_size() const {
  return handler_table_offset() - safepoint_table_offset();
}

bool Code::has_safepoint_table() const { return safepoint_table_size() > 0; }

Address Code::handler_table_address() const {
  return metadata_start() + handler_table_offset();
}

Address Code::HandlerTableAddress() const {
  return V8_LIKELY(has_instruction_stream()) ? handler_table_address()
                                             : OffHeapHandlerTableAddress();
}

int Code::handler_table_size() const {
  return constant_pool_offset() - handler_table_offset();
}

bool Code::has_handler_table() const { return handler_table_size() > 0; }

int Code::constant_pool_size() const {
  if V8_UNLIKELY (!has_instruction_stream()) return OffHeapConstantPoolSize();

  const int size = code_comments_offset() - constant_pool_offset();
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    DCHECK_EQ(size, 0);
    return 0;
  }
  DCHECK_GE(size, 0);
  return size;
}

bool Code::has_constant_pool() const { return constant_pool_size() > 0; }

ByteArray Code::unchecked_relocation_info() const {
  return ByteArray::unchecked_cast(
      TaggedField<HeapObject, kRelocationInfoOffset>::load(*this));
}

FixedArray Code::unchecked_deoptimization_data() const {
  return FixedArray::unchecked_cast(
      TaggedField<HeapObject, kDeoptimizationDataOrInterpreterDataOffset>::load(
          *this));
}

Code InstructionStream::unchecked_code() const {
  PtrComprCageBase cage_base = main_cage_base(kRelaxedLoad);
  return Code::unchecked_cast(
      TaggedField<HeapObject, kCodeOffset>::Acquire_Load(cage_base, *this));
}

byte* Code::relocation_start() const {
  return V8_LIKELY(has_instruction_stream())
             ? relocation_info().GetDataStartAddress()
             : nullptr;
}

byte* Code::relocation_end() const {
  return V8_LIKELY(has_instruction_stream())
             ? relocation_info().GetDataEndAddress()
             : nullptr;
}

int Code::relocation_size() const {
  return V8_LIKELY(has_instruction_stream()) ? relocation_info().length() : 0;
}

Address InstructionStream::entry() const { return instruction_start(); }

bool InstructionStream::contains(Isolate* isolate, Address inner_pointer) {
  return (address() <= inner_pointer) &&
         (inner_pointer < address() + CodeSize());
}

bool Code::contains(Isolate* isolate, Address inner_pointer) {
  return has_instruction_stream()
             ? instruction_stream().contains(isolate, inner_pointer)
             : OffHeapBuiltinContains(isolate, inner_pointer);
}

// static
void Code::CopyRelocInfoToByteArray(ByteArray dest, const CodeDesc& desc) {
  DCHECK_EQ(dest.length(), desc.reloc_size);
  CopyBytes(dest.GetDataStartAddress(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));
}

int InstructionStream::CodeSize() const {
  return SizeFor(Code::unchecked_cast(raw_code(kAcquireLoad)).body_size());
}
int Code::CodeSize() const { return InstructionStream::SizeFor(body_size()); }

DEF_GETTER(InstructionStream, Size, int) { return CodeSize(); }

CodeKind Code::kind() const {
  static_assert(FIELD_SIZE(kFlagsOffset) == kInt32Size);
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return KindField::decode(flags);
}

int Code::GetBytecodeOffsetForBaselinePC(Address baseline_pc,
                                         BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK(!is_baseline_trampoline_builtin());
  if (is_baseline_leave_frame_builtin()) return kFunctionExitBytecodeOffset;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  Address pc = baseline_pc - InstructionStart();
  offset_iterator.AdvanceToPCOffset(pc);
  return offset_iterator.current_bytecode_offset();
}

uintptr_t Code::GetBaselinePCForBytecodeOffset(int bytecode_offset,
                                               BytecodeToPCPosition position,
                                               BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  offset_iterator.AdvanceToBytecodeOffset(bytecode_offset);
  uintptr_t pc = 0;
  if (position == kPcAtStartOfBytecode) {
    pc = offset_iterator.current_pc_start_offset();
  } else {
    DCHECK_EQ(position, kPcAtEndOfBytecode);
    pc = offset_iterator.current_pc_end_offset();
  }
  return pc;
}

uintptr_t Code::GetBaselineStartPCForBytecodeOffset(int bytecode_offset,
                                                    BytecodeArray bytecodes) {
  return GetBaselinePCForBytecodeOffset(bytecode_offset, kPcAtStartOfBytecode,
                                        bytecodes);
}

uintptr_t Code::GetBaselineEndPCForBytecodeOffset(int bytecode_offset,
                                                  BytecodeArray bytecodes) {
  return GetBaselinePCForBytecodeOffset(bytecode_offset, kPcAtEndOfBytecode,
                                        bytecodes);
}

uintptr_t Code::GetBaselinePCForNextExecutedBytecode(int bytecode_offset,
                                                     BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  Handle<BytecodeArray> bytecodes_handle(
      reinterpret_cast<Address*>(&bytecodes));
  interpreter::BytecodeArrayIterator bytecode_iterator(bytecodes_handle,
                                                       bytecode_offset);
  interpreter::Bytecode bytecode = bytecode_iterator.current_bytecode();
  if (bytecode == interpreter::Bytecode::kJumpLoop) {
    return GetBaselineStartPCForBytecodeOffset(
        bytecode_iterator.GetJumpTargetOffset(), bytecodes);
  } else {
    DCHECK(!interpreter::Bytecodes::IsJump(bytecode));
    DCHECK(!interpreter::Bytecodes::IsSwitch(bytecode));
    DCHECK(!interpreter::Bytecodes::Returns(bytecode));
    return GetBaselineEndPCForBytecodeOffset(bytecode_offset, bytecodes);
  }
}

inline bool Code::checks_tiering_state() const {
  bool checks_state = (builtin_id() == Builtin::kCompileLazy ||
                       builtin_id() == Builtin::kInterpreterEntryTrampoline ||
                       CodeKindCanTierUp(kind()));
  return checks_state ||
         (CodeKindCanDeoptimize(kind()) && marked_for_deoptimization());
}

inline constexpr bool CodeKindHasTaggedOutgoingParams(CodeKind kind) {
  return kind != CodeKind::JS_TO_WASM_FUNCTION &&
         kind != CodeKind::C_WASM_ENTRY && kind != CodeKind::WASM_FUNCTION;
}

inline bool Code::has_tagged_outgoing_params() const {
#if V8_ENABLE_WEBASSEMBLY
  return CodeKindHasTaggedOutgoingParams(kind()) &&
         builtin_id() != Builtin::kWasmCompileLazy;
#else
  return CodeKindHasTaggedOutgoingParams(kind());
#endif
}

inline bool Code::is_turbofanned() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return IsTurbofannedField::decode(flags);
}

inline bool Code::is_maglevved() const { return kind() == CodeKind::MAGLEV; }

inline bool Code::can_have_weak_objects() const {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  int16_t flags = kind_specific_flags(kRelaxedLoad);
  return CanHaveWeakObjectsField::decode(flags);
}

inline void Code::set_can_have_weak_objects(bool value) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  int16_t previous = kind_specific_flags(kRelaxedLoad);
  int16_t updated = CanHaveWeakObjectsField::update(previous, value);
  set_kind_specific_flags(updated, kRelaxedStore);
}

inline bool Code::is_promise_rejection() const {
  DCHECK_EQ(kind(), CodeKind::BUILTIN);
  int16_t flags = kind_specific_flags(kRelaxedLoad);
  return IsPromiseRejectionField::decode(flags);
}

inline void Code::set_is_promise_rejection(bool value) {
  DCHECK_EQ(kind(), CodeKind::BUILTIN);
  int16_t previous = kind_specific_flags(kRelaxedLoad);
  int16_t updated = IsPromiseRejectionField::update(previous, value);
  set_kind_specific_flags(updated, kRelaxedStore);
}

inline HandlerTable::CatchPrediction
InstructionStream::GetBuiltinCatchPrediction() const {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
  return HandlerTable::UNCAUGHT;
}

inline HandlerTable::CatchPrediction Code::GetBuiltinCatchPrediction() const {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
  return HandlerTable::UNCAUGHT;
}

unsigned Code::inlined_bytecode_size() const {
  unsigned size = RELAXED_READ_UINT_FIELD(*this, kInlinedBytecodeSizeOffset);
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  return size;
}

void Code::set_inlined_bytecode_size(unsigned size) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  RELAXED_WRITE_UINT_FIELD(*this, kInlinedBytecodeSizeOffset, size);
}

BytecodeOffset Code::osr_offset() const {
  return BytecodeOffset(RELAXED_READ_INT32_FIELD(*this, kOsrOffsetOffset));
}

void Code::set_osr_offset(BytecodeOffset offset) {
  RELAXED_WRITE_INT32_FIELD(*this, kOsrOffsetOffset, offset.ToInt());
}

bool Code::uses_safepoint_table() const {
  return is_turbofanned() || is_maglevved() || is_wasm_code();
}

int Code::stack_slots() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  const int slots = StackSlotsField::decode(flags);
  DCHECK_IMPLIES(!uses_safepoint_table(), slots == 0);
  return slots;
}

bool Code::marked_for_deoptimization() const {
  DCHECK(CodeKindCanDeoptimize(kind()));
  int16_t flags = kind_specific_flags(kRelaxedLoad);
  return MarkedForDeoptimizationField::decode(flags);
}

void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK(CodeKindCanDeoptimize(kind()));
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(
                           GetIsolateFromWritableObject(*this)));
  int16_t previous = kind_specific_flags(kRelaxedLoad);
  int16_t updated = MarkedForDeoptimizationField::update(previous, flag);
  set_kind_specific_flags(updated, kRelaxedStore);
}

bool Code::embedded_objects_cleared() const {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  int16_t flags = kind_specific_flags(kRelaxedLoad);
  return Code::EmbeddedObjectsClearedField::decode(flags);
}

void Code::set_embedded_objects_cleared(bool flag) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  DCHECK_IMPLIES(flag, marked_for_deoptimization());
  int16_t previous = kind_specific_flags(kRelaxedLoad);
  int16_t updated = Code::EmbeddedObjectsClearedField::update(previous, flag);
  set_kind_specific_flags(updated, kRelaxedStore);
}

bool Code::is_wasm_code() const { return kind() == CodeKind::WASM_FUNCTION; }

int Code::constant_pool_offset() const {
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    // Redirection needed since the field doesn't exist in this case.
    return code_comments_offset();
  }
  return ReadField<int>(kConstantPoolOffsetOffset);
}

void Code::set_constant_pool_offset(int value) {
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    // Redirection needed since the field doesn't exist in this case.
    return;
  }
  DCHECK_LE(value, metadata_size());
  WriteField<int>(kConstantPoolOffsetOffset, value);
}

Address Code::constant_pool() const {
  if (!has_constant_pool()) return kNullAddress;
  return V8_LIKELY(has_instruction_stream())
             ? metadata_start() + constant_pool_offset()
             : OffHeapConstantPoolAddress();
}

Address Code::code_comments() const {
  return V8_LIKELY(has_instruction_stream())
             ? metadata_start() + code_comments_offset()
             : OffHeapCodeCommentsAddress();
}

int Code::code_comments_size() const {
  return unwinding_info_offset() - code_comments_offset();
}

bool Code::has_code_comments() const { return code_comments_size() > 0; }

Address Code::unwinding_info_start() const {
  return metadata_start() + unwinding_info_offset();
}

Address Code::unwinding_info_end() const { return metadata_end(); }

int Code::unwinding_info_size() const {
  return static_cast<int>(unwinding_info_end() - unwinding_info_start());
}

bool Code::has_unwinding_info() const { return unwinding_info_size() > 0; }

// static
InstructionStream InstructionStream::FromTargetAddress(Address address) {
  {
    // TODO(jgruber,v8:6666): Support embedded builtins here. We'd need to pass
    // in the current isolate.
    Address start =
        reinterpret_cast<Address>(Isolate::CurrentEmbeddedBlobCode());
    Address end = start + Isolate::CurrentEmbeddedBlobCodeSize();
    CHECK(address < start || address >= end);
  }

  HeapObject code =
      HeapObject::FromAddress(address - InstructionStream::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently not being a
  // forwarding pointer.
  return InstructionStream::unchecked_cast(code);
}

// static
Code Code::FromTargetAddress(Address address) {
  return InstructionStream::FromTargetAddress(address).code(kAcquireLoad);
}

// static
InstructionStream InstructionStream::FromEntryAddress(
    Address location_of_address) {
  Address code_entry = base::Memory<Address>(location_of_address);
  HeapObject code =
      HeapObject::FromAddress(code_entry - InstructionStream::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently not being a
  // forwarding pointer.
  return InstructionStream::unchecked_cast(code);
}

bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}

bool Code::IsWeakObject(HeapObject object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}

bool Code::IsWeakObjectInOptimizedCode(HeapObject object) {
  Map map_object = object.map(kAcquireLoad);
  if (InstanceTypeChecker::IsMap(map_object)) {
    return Map::cast(object).CanTransition();
  }
  return InstanceTypeChecker::IsPropertyCell(map_object) ||
         InstanceTypeChecker::IsJSReceiver(map_object) ||
         InstanceTypeChecker::IsContext(map_object);
}

bool Code::IsWeakObjectInDeoptimizationLiteralArray(Object object) {
  // Maps must be strong because they can be used as part of the description for
  // how to materialize an object upon deoptimization, in which case it is
  // possible to reach the code that requires the Map without anything else
  // holding a strong pointer to that Map.
  return object.IsHeapObject() && !object.IsMap() &&
         Code::IsWeakObjectInOptimizedCode(HeapObject::cast(object));
}

void Code::IterateDeoptimizationLiterals(RootVisitor* v) {
  if (kind() == CodeKind::BASELINE) return;

  auto deopt_data = DeoptimizationData::cast(deoptimization_data());
  if (deopt_data.length() == 0) return;

  DeoptimizationLiteralArray literals = deopt_data.LiteralArray();
  const int literals_length = literals.length();
  for (int i = 0; i < literals_length; ++i) {
    MaybeObject maybe_literal = literals.Get(i);
    HeapObject heap_literal;
    if (maybe_literal.GetHeapObject(&heap_literal)) {
      v->VisitRootPointer(Root::kStackRoots, "deoptimization literal",
                          FullObjectSlot(&heap_literal));
    }
  }
}

// This field has to have relaxed atomic accessors because it is accessed in the
// concurrent marker.
static_assert(FIELD_SIZE(Code::kKindSpecificFlagsOffset) == kInt16Size);
RELAXED_UINT16_ACCESSORS(Code, kind_specific_flags, kKindSpecificFlagsOffset)

Object Code::raw_instruction_stream() const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::raw_instruction_stream(cage_base);
}

Object Code::raw_instruction_stream(PtrComprCageBase cage_base) const {
  return ExternalCodeField<Object>::load(cage_base, *this);
}

void Code::set_raw_instruction_stream(Object value, WriteBarrierMode mode) {
  ExternalCodeField<Object>::Release_Store(*this, value);
  CONDITIONAL_WRITE_BARRIER(*this, kInstructionStreamOffset, value, mode);
}

bool Code::has_instruction_stream() const {
  const uint32_t value = ReadField<uint32_t>(kInstructionStreamOffset);
  SLOW_DCHECK(value == 0 || !InReadOnlySpace());
  return value != 0;
}

bool Code::has_instruction_stream(RelaxedLoadTag tag) const {
  const uint32_t value =
      RELAXED_READ_INT32_FIELD(*this, kInstructionStreamOffset);
  SLOW_DCHECK(value == 0 || !InReadOnlySpace());
  return value != 0;
}

PtrComprCageBase Code::code_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  // Only available if the current Code object is not in RO space (otherwise we
  // can't grab the current Isolate from it).
  DCHECK(!InReadOnlySpace());
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  return PtrComprCageBase(isolate->code_cage_base());
#else   // V8_EXTERNAL_CODE_SPACE
  // Without external code space: `code_cage_base == main_cage_base`. We can
  // get the main cage base from any heap object, including objects in RO
  // space.
  return GetPtrComprCageBase(*this);
#endif  // V8_EXTERNAL_CODE_SPACE
}

InstructionStream Code::instruction_stream() const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::instruction_stream(cage_base);
}
InstructionStream Code::instruction_stream(PtrComprCageBase cage_base) const {
  DCHECK(has_instruction_stream());
  return ExternalCodeField<InstructionStream>::load(cage_base, *this);
}

InstructionStream Code::instruction_stream(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::instruction_stream(cage_base, tag);
}

InstructionStream Code::instruction_stream(PtrComprCageBase cage_base,
                                           RelaxedLoadTag tag) const {
  DCHECK(has_instruction_stream());
  return ExternalCodeField<InstructionStream>::Relaxed_Load(cage_base, *this);
}

Object Code::raw_instruction_stream(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::raw_instruction_stream(cage_base, tag);
}

Object Code::raw_instruction_stream(PtrComprCageBase cage_base,
                                    RelaxedLoadTag tag) const {
  return ExternalCodeField<Object>::Relaxed_Load(cage_base, *this);
}

DEF_GETTER(Code, code_entry_point, Address) {
  return ReadField<Address>(kCodeEntryPointOffset);
}

void Code::init_code_entry_point(Isolate* isolate, Address value) {
  set_code_entry_point(isolate, value);
}

void Code::set_code_entry_point(Isolate* isolate, Address value) {
  WriteField<Address>(kCodeEntryPointOffset, value);
}

void Code::SetInstructionStreamAndEntryPoint(Isolate* isolate_for_sandbox,
                                             InstructionStream code,
                                             WriteBarrierMode mode) {
  set_raw_instruction_stream(code, mode);
  set_code_entry_point(isolate_for_sandbox, code.instruction_start());
}

void Code::SetEntryPointForOffHeapBuiltin(Isolate* isolate_for_sandbox,
                                          Address entry) {
  DCHECK(!has_instruction_stream());
  set_code_entry_point(isolate_for_sandbox, entry);
}

void Code::SetCodeEntryPointForSerialization(Isolate* isolate, Address entry) {
  set_code_entry_point(isolate, entry);
}

void Code::UpdateCodeEntryPoint(Isolate* isolate_for_sandbox,
                                InstructionStream code) {
  DCHECK_EQ(raw_instruction_stream(), code);
  set_code_entry_point(isolate_for_sandbox, code.instruction_start());
}

Address Code::InstructionStart() const { return code_entry_point(); }

void Code::clear_padding() {
  memset(reinterpret_cast<void*>(address() + kUnalignedSize), 0,
         kSize - kUnalignedSize);
}

RELAXED_UINT16_ACCESSORS(Code, flags, kFlagsOffset)

// Ensure builtin_id field fits into int16_t, so that we can rely on sign
// extension to convert int16_t{-1} to kNoBuiltinId.
// If the asserts fail, update the code that use kBuiltinIdOffset below.
static_assert(static_cast<int>(Builtin::kNoBuiltinId) == -1);
static_assert(Builtins::kBuiltinCount < std::numeric_limits<int16_t>::max());

void Code::initialize_flags(CodeKind kind, Builtin builtin_id,
                            bool is_turbofanned, int stack_slots) {
  CHECK(0 <= stack_slots && stack_slots < StackSlotsField::kMax);
  DCHECK(!CodeKindIsInterpretedJSFunction(kind));
  uint32_t value = KindField::encode(kind) |
                   IsTurbofannedField::encode(is_turbofanned) |
                   StackSlotsField::encode(stack_slots);
  static_assert(FIELD_SIZE(kFlagsOffset) == kInt32Size);
  RELAXED_WRITE_UINT32_FIELD(*this, kFlagsOffset, value);
  DCHECK_IMPLIES(stack_slots != 0, uses_safepoint_table());
  DCHECK_IMPLIES(!uses_safepoint_table(), stack_slots == 0);

  WriteField<int16_t>(kBuiltinIdOffset, static_cast<int16_t>(builtin_id));
}

Builtin Code::builtin_id() const {
  // Rely on sign-extension when converting int16_t to int to preserve
  // kNoBuiltinId value.
  static_assert(static_cast<int>(static_cast<int16_t>(Builtin::kNoBuiltinId)) ==
                static_cast<int>(Builtin::kNoBuiltinId));
  int value = ReadField<int16_t>(kBuiltinIdOffset);
  return static_cast<Builtin>(value);
}

bool Code::is_builtin() const { return builtin_id() != Builtin::kNoBuiltinId; }

bool Code::is_optimized_code() const {
  return CodeKindIsOptimizedJSFunction(kind());
}

inline bool Code::is_interpreter_trampoline_builtin() const {
  return IsInterpreterTrampolineBuiltin(builtin_id());
}

inline bool Code::is_baseline_trampoline_builtin() const {
  return IsBaselineTrampolineBuiltin(builtin_id());
}

inline bool Code::is_baseline_leave_frame_builtin() const {
  return builtin_id() == Builtin::kBaselineLeaveFrame;
}

byte BytecodeArray::get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return ReadField<byte>(kHeaderSize + index * kCharSize);
}

void BytecodeArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WriteField<byte>(kHeaderSize + index * kCharSize, value);
}

void BytecodeArray::set_frame_size(int32_t frame_size) {
  DCHECK_GE(frame_size, 0);
  DCHECK(IsAligned(frame_size, kSystemPointerSize));
  WriteField<int32_t>(kFrameSizeOffset, frame_size);
}

int32_t BytecodeArray::frame_size() const {
  return ReadField<int32_t>(kFrameSizeOffset);
}

int BytecodeArray::register_count() const {
  return static_cast<int>(frame_size()) / kSystemPointerSize;
}

void BytecodeArray::set_parameter_count(int32_t number_of_parameters) {
  DCHECK_GE(number_of_parameters, 0);
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  WriteField<int32_t>(kParameterSizeOffset,
                      (number_of_parameters << kSystemPointerSizeLog2));
}

interpreter::Register BytecodeArray::incoming_new_target_or_generator_register()
    const {
  int32_t register_operand =
      ReadField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset);
  if (register_operand == 0) {
    return interpreter::Register::invalid_value();
  } else {
    return interpreter::Register::FromOperand(register_operand);
  }
}

void BytecodeArray::set_incoming_new_target_or_generator_register(
    interpreter::Register incoming_new_target_or_generator_register) {
  if (!incoming_new_target_or_generator_register.is_valid()) {
    WriteField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset, 0);
  } else {
    DCHECK(incoming_new_target_or_generator_register.index() <
           register_count());
    DCHECK_NE(0, incoming_new_target_or_generator_register.ToOperand());
    WriteField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset,
                        incoming_new_target_or_generator_register.ToOperand());
  }
}

uint16_t BytecodeArray::bytecode_age() const {
  // Bytecode is aged by the concurrent marker.
  return RELAXED_READ_UINT16_FIELD(*this, kBytecodeAgeOffset);
}

void BytecodeArray::set_bytecode_age(uint16_t age) {
  // Bytecode is aged by the concurrent marker.
  RELAXED_WRITE_UINT16_FIELD(*this, kBytecodeAgeOffset, age);
}

int32_t BytecodeArray::parameter_count() const {
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  return ReadField<int32_t>(kParameterSizeOffset) >> kSystemPointerSizeLog2;
}

void BytecodeArray::clear_padding() {
  int data_size = kHeaderSize + length();
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

Address BytecodeArray::GetFirstBytecodeAddress() {
  return ptr() - kHeapObjectTag + kHeaderSize;
}

bool BytecodeArray::HasSourcePositionTable() const {
  Object maybe_table = source_position_table(kAcquireLoad);
  return !(maybe_table.IsUndefined() || DidSourcePositionGenerationFail());
}

bool BytecodeArray::DidSourcePositionGenerationFail() const {
  return source_position_table(kAcquireLoad).IsException();
}

void BytecodeArray::SetSourcePositionsFailedToCollect() {
  set_source_position_table(GetReadOnlyRoots().exception(), kReleaseStore);
}

DEF_GETTER(BytecodeArray, SourcePositionTable, ByteArray) {
  // WARNING: This function may be called from a background thread, hence
  // changes to how it accesses the heap can easily lead to bugs.
  Object maybe_table = source_position_table(cage_base, kAcquireLoad);
  if (maybe_table.IsByteArray(cage_base)) return ByteArray::cast(maybe_table);
  ReadOnlyRoots roots = GetReadOnlyRoots();
  DCHECK(maybe_table.IsUndefined(roots) || maybe_table.IsException(roots));
  return roots.empty_byte_array();
}

DEF_GETTER(BytecodeArray, raw_constant_pool, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kConstantPoolOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsFixedArray());
  return value;
}

DEF_GETTER(BytecodeArray, raw_handler_table, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kHandlerTableOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsByteArray());
  return value;
}

DEF_GETTER(BytecodeArray, raw_source_position_table, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kSourcePositionTableOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsByteArray() || value.IsUndefined() ||
         value.IsException());
  return value;
}

int BytecodeArray::BytecodeArraySize() const { return SizeFor(this->length()); }

DEF_GETTER(BytecodeArray, SizeIncludingMetadata, int) {
  int size = BytecodeArraySize();
  Object maybe_constant_pool = raw_constant_pool(cage_base);
  if (maybe_constant_pool.IsFixedArray()) {
    size += FixedArray::cast(maybe_constant_pool).Size(cage_base);
  } else {
    DCHECK_EQ(maybe_constant_pool, Smi::zero());
  }
  Object maybe_handler_table = raw_handler_table(cage_base);
  if (maybe_handler_table.IsByteArray()) {
    size += ByteArray::cast(maybe_handler_table).Size();
  } else {
    DCHECK_EQ(maybe_handler_table, Smi::zero());
  }
  Object maybe_table = raw_source_position_table(cage_base);
  if (maybe_table.IsByteArray()) {
    size += ByteArray::cast(maybe_table).Size();
  }
  return size;
}

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, TranslationArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, DeoptimizationLiteralArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
DEFINE_DEOPT_ELEMENT_ACCESSORS(DeoptExitStart, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(EagerDeoptCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LazyDeoptCount, Smi)

DEFINE_DEOPT_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)
#ifdef DEBUG
DEFINE_DEOPT_ENTRY_ACCESSORS(NodeId, Smi)
#endif  // DEBUG

BytecodeOffset DeoptimizationData::GetBytecodeOffset(int i) const {
  return BytecodeOffset(BytecodeOffsetRaw(i).value());
}

void DeoptimizationData::SetBytecodeOffset(int i, BytecodeOffset value) {
  SetBytecodeOffsetRaw(i, Smi::FromInt(value.ToInt()));
}

int DeoptimizationData::DeoptCount() {
  return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
}

inline DeoptimizationLiteralArray::DeoptimizationLiteralArray(Address ptr)
    : WeakFixedArray(ptr) {
  // No type check is possible beyond that for WeakFixedArray.
}

inline Object DeoptimizationLiteralArray::get(int index) const {
  return get(GetPtrComprCageBase(*this), index);
}

inline Object DeoptimizationLiteralArray::get(PtrComprCageBase cage_base,
                                              int index) const {
  MaybeObject maybe = Get(cage_base, index);

  // Slots in the DeoptimizationLiteralArray should only be cleared when there
  // is no possible code path that could need that slot. This works because the
  // weakly-held deoptimization literals are basically local variables that
  // TurboFan has decided not to keep on the stack. Thus, if the deoptimization
  // literal goes away, then whatever code needed it should be unreachable. The
  // exception is currently running InstructionStream: in that case, the
  // deoptimization literals array might be the only thing keeping the target
  // object alive. Thus, when a InstructionStream is running, we strongly mark
  // all of its deoptimization literals.
  CHECK(!maybe.IsCleared());

  return maybe.GetHeapObjectOrSmi();
}

inline void DeoptimizationLiteralArray::set(int index, Object value) {
  MaybeObject maybe = MaybeObject::FromObject(value);
  if (Code::IsWeakObjectInDeoptimizationLiteralArray(value)) {
    maybe = MaybeObject::MakeWeak(maybe);
  }
  Set(index, maybe);
}

// static
template <typename ObjectT>
void DependentCode::DeoptimizeDependencyGroups(Isolate* isolate, ObjectT object,
                                               DependencyGroups groups) {
  // Shared objects are designed to never invalidate code.
  DCHECK(!object.InSharedHeap());
  object.dependent_code().DeoptimizeDependencyGroups(isolate, groups);
}

// static
template <typename ObjectT>
bool DependentCode::MarkCodeForDeoptimization(Isolate* isolate, ObjectT object,
                                              DependencyGroups groups) {
  // Shared objects are designed to never invalidate code.
  DCHECK(!object.InSharedHeap());
  return object.dependent_code().MarkCodeForDeoptimization(isolate, groups);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_INL_H_
