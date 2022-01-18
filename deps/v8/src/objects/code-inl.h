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
OBJECT_CONSTRUCTORS_IMPL(DependentCode, WeakFixedArray)
OBJECT_CONSTRUCTORS_IMPL(CodeDataContainer, HeapObject)

NEVER_READ_ONLY_SPACE_IMPL(AbstractCode)

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(CodeDataContainer)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(DeoptimizationData)

int AbstractCode::raw_instruction_size() {
  if (IsCode()) {
    return GetCode().raw_instruction_size();
  } else {
    return GetBytecodeArray().length();
  }
}

int AbstractCode::InstructionSize() {
  if (IsCode()) {
    return GetCode().InstructionSize();
  } else {
    return GetBytecodeArray().length();
  }
}

ByteArray AbstractCode::SourcePositionTableInternal() {
  if (IsCode()) {
    DCHECK_NE(GetCode().kind(), CodeKind::BASELINE);
    return GetCode().source_position_table();
  } else {
    return GetBytecodeArray().SourcePositionTable();
  }
}

ByteArray AbstractCode::SourcePositionTable(SharedFunctionInfo sfi) {
  if (IsCode()) {
    return GetCode().SourcePositionTable(sfi);
  } else {
    return GetBytecodeArray().SourcePositionTable();
  }
}

int AbstractCode::SizeIncludingMetadata() {
  if (IsCode()) {
    return GetCode().SizeIncludingMetadata();
  } else {
    return GetBytecodeArray().SizeIncludingMetadata();
  }
}

Address AbstractCode::raw_instruction_start() {
  if (IsCode()) {
    return GetCode().raw_instruction_start();
  } else {
    return GetBytecodeArray().GetFirstBytecodeAddress();
  }
}

Address AbstractCode::InstructionStart() {
  if (IsCode()) {
    return GetCode().InstructionStart();
  } else {
    return GetBytecodeArray().GetFirstBytecodeAddress();
  }
}

Address AbstractCode::raw_instruction_end() {
  if (IsCode()) {
    return GetCode().raw_instruction_end();
  } else {
    return GetBytecodeArray().GetFirstBytecodeAddress() +
           GetBytecodeArray().length();
  }
}

Address AbstractCode::InstructionEnd() {
  if (IsCode()) {
    return GetCode().InstructionEnd();
  } else {
    return GetBytecodeArray().GetFirstBytecodeAddress() +
           GetBytecodeArray().length();
  }
}

bool AbstractCode::contains(Address inner_pointer) {
  return (address() <= inner_pointer) && (inner_pointer <= address() + Size());
}

CodeKind AbstractCode::kind() {
  return IsCode() ? GetCode().kind() : CodeKind::INTERPRETED_FUNCTION;
}

Code AbstractCode::GetCode() { return Code::cast(*this); }

BytecodeArray AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(*this);
}

DependentCode DependentCode::next_link() {
  return DependentCode::cast(Get(kNextLinkIndex)->GetHeapObjectAssumeStrong());
}

void DependentCode::set_next_link(DependentCode next) {
  Set(kNextLinkIndex, HeapObjectReference::Strong(next));
}

int DependentCode::flags() { return Smi::ToInt(Get(kFlagsIndex)->ToSmi()); }

void DependentCode::set_flags(int flags) {
  Set(kFlagsIndex, MaybeObject::FromObject(Smi::FromInt(flags)));
}

int DependentCode::count() { return CountField::decode(flags()); }

void DependentCode::set_count(int value) {
  set_flags(CountField::update(flags(), value));
}

DependentCode::DependencyGroup DependentCode::group() {
  return static_cast<DependencyGroup>(GroupField::decode(flags()));
}

void DependentCode::set_object_at(int i, MaybeObject object) {
  Set(kCodesStartIndex + i, object);
}

MaybeObject DependentCode::object_at(int i) {
  return Get(kCodesStartIndex + i);
}

void DependentCode::clear_at(int i) {
  Set(kCodesStartIndex + i,
      HeapObjectReference::Strong(GetReadOnlyRoots().undefined_value()));
}

void DependentCode::copy(int from, int to) {
  Set(kCodesStartIndex + to, Get(kCodesStartIndex + from));
}

OBJECT_CONSTRUCTORS_IMPL(Code, HeapObject)
NEVER_READ_ONLY_SPACE_IMPL(Code)

INT_ACCESSORS(Code, raw_instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, raw_metadata_size, kMetadataSizeOffset)
INT_ACCESSORS(Code, handler_table_offset, kHandlerTableOffsetOffset)
INT_ACCESSORS(Code, code_comments_offset, kCodeCommentsOffsetOffset)
INT32_ACCESSORS(Code, unwinding_info_offset, kUnwindingInfoOffsetOffset)

// Same as ACCESSORS_CHECKED2 macro but with Code as a host and using
// main_cage_base() for computing the base.
#define CODE_ACCESSORS_CHECKED2(name, type, offset, get_condition,  \
                                set_condition)                      \
  type Code::name() const {                                         \
    PtrComprCageBase cage_base = main_cage_base();                  \
    return Code::name(cage_base);                                   \
  }                                                                 \
  type Code::name(PtrComprCageBase cage_base) const {               \
    type value = TaggedField<type, offset>::load(cage_base, *this); \
    DCHECK(get_condition);                                          \
    return value;                                                   \
  }                                                                 \
  void Code::set_##name(type value, WriteBarrierMode mode) {        \
    DCHECK(set_condition);                                          \
    TaggedField<type, offset>::store(*this, value);                 \
    CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);          \
  }

// Same as RELEASE_ACQUIRE_ACCESSORS_CHECKED2 macro but with Code as a host and
// using main_cage_base() for computing the base.
#define RELEASE_ACQUIRE_CODE_ACCESSORS_CHECKED2(name, type, offset,           \
                                                get_condition, set_condition) \
  type Code::name(AcquireLoadTag tag) const {                                 \
    PtrComprCageBase cage_base = main_cage_base();                            \
    return Code::name(cage_base, tag);                                        \
  }                                                                           \
  type Code::name(PtrComprCageBase cage_base, AcquireLoadTag) const {         \
    type value = TaggedField<type, offset>::Acquire_Load(cage_base, *this);   \
    DCHECK(get_condition);                                                    \
    return value;                                                             \
  }                                                                           \
  void Code::set_##name(type value, ReleaseStoreTag, WriteBarrierMode mode) { \
    DCHECK(set_condition);                                                    \
    TaggedField<type, offset>::Release_Store(*this, value);                   \
    CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);                    \
  }

#define CODE_ACCESSORS(name, type, offset) \
  CODE_ACCESSORS_CHECKED2(name, type, offset, true, true)

#define RELEASE_ACQUIRE_CODE_ACCESSORS(name, type, offset)                 \
  RELEASE_ACQUIRE_CODE_ACCESSORS_CHECKED2(name, type, offset,              \
                                          !ObjectInYoungGeneration(value), \
                                          !ObjectInYoungGeneration(value))

CODE_ACCESSORS(relocation_info, ByteArray, kRelocationInfoOffset)

CODE_ACCESSORS_CHECKED2(deoptimization_data, FixedArray,
                        kDeoptimizationDataOrInterpreterDataOffset,
                        kind() != CodeKind::BASELINE,
                        kind() != CodeKind::BASELINE &&
                            !ObjectInYoungGeneration(value))
CODE_ACCESSORS_CHECKED2(bytecode_or_interpreter_data, HeapObject,
                        kDeoptimizationDataOrInterpreterDataOffset,
                        kind() == CodeKind::BASELINE,
                        kind() == CodeKind::BASELINE &&
                            !ObjectInYoungGeneration(value))

CODE_ACCESSORS_CHECKED2(source_position_table, ByteArray, kPositionTableOffset,
                        kind() != CodeKind::BASELINE,
                        kind() != CodeKind::BASELINE &&
                            !ObjectInYoungGeneration(value))
CODE_ACCESSORS_CHECKED2(bytecode_offset_table, ByteArray, kPositionTableOffset,
                        kind() == CodeKind::BASELINE,
                        kind() == CodeKind::BASELINE &&
                            !ObjectInYoungGeneration(value))

// Concurrent marker needs to access kind specific flags in code data container.
RELEASE_ACQUIRE_CODE_ACCESSORS(code_data_container, CodeDataContainer,
                               kCodeDataContainerOffset)
#undef CODE_ACCESSORS
#undef CODE_ACCESSORS_CHECKED2
#undef RELEASE_ACQUIRE_CODE_ACCESSORS
#undef RELEASE_ACQUIRE_CODE_ACCESSORS_CHECKED2

PtrComprCageBase Code::main_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  Address cage_base_hi = ReadField<Tagged_t>(kMainCageBaseUpper32BitsOffset);
  return PtrComprCageBase(cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void Code::set_main_cage_base(Address cage_base) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Tagged_t cage_base_hi = static_cast<Tagged_t>(cage_base >> 32);
  WriteField<Tagged_t>(kMainCageBaseUpper32BitsOffset, cage_base_hi);
#else
  UNREACHABLE();
#endif
}

CodeDataContainer Code::GCSafeCodeDataContainer(AcquireLoadTag) const {
  PtrComprCageBase cage_base = main_cage_base();
  HeapObject object =
      TaggedField<HeapObject, kCodeDataContainerOffset>::Acquire_Load(cage_base,
                                                                      *this);
  DCHECK(!ObjectInYoungGeneration(object));
  CodeDataContainer code_data_container =
      ForwardingAddress(CodeDataContainer::unchecked_cast(object));
  return code_data_container;
}

// Helper functions for converting Code objects to CodeDataContainer and back
// when V8_EXTERNAL_CODE_SPACE is enabled.
inline CodeT ToCodeT(Code code) {
#if V8_EXTERNAL_CODE_SPACE
  return code.code_data_container(kAcquireLoad);
#else
  return code;
#endif
}

inline Code FromCodeT(CodeT code) {
#if V8_EXTERNAL_CODE_SPACE
  return code.code();
#else
  return code;
#endif
}

inline Code FromCodeT(CodeT code, RelaxedLoadTag) {
#if V8_EXTERNAL_CODE_SPACE
  return code.code(kRelaxedLoad);
#else
  return code;
#endif
}

inline CodeDataContainer CodeDataContainerFromCodeT(CodeT code) {
#if V8_EXTERNAL_CODE_SPACE
  return code;
#else
  return code.code_data_container(kAcquireLoad);
#endif
}

void Code::WipeOutHeader() {
  WRITE_FIELD(*this, kRelocationInfoOffset, Smi::FromInt(0));
  WRITE_FIELD(*this, kDeoptimizationDataOrInterpreterDataOffset,
              Smi::FromInt(0));
  WRITE_FIELD(*this, kPositionTableOffset, Smi::FromInt(0));
  WRITE_FIELD(*this, kCodeDataContainerOffset, Smi::FromInt(0));
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    set_main_cage_base(kNullAddress);
  }
}

void Code::clear_padding() {
  // Clear the padding between the header and `raw_body_start`.
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }

  // Clear the padding after `raw_body_end`.
  size_t trailing_padding_size =
      CodeSize() - Code::kHeaderSize - raw_body_size();
  memset(reinterpret_cast<void*>(raw_body_end()), 0, trailing_padding_size);
}

ByteArray Code::SourcePositionTable(SharedFunctionInfo sfi) const {
  DisallowGarbageCollection no_gc;
  if (kind() == CodeKind::BASELINE) {
    return sfi.GetBytecodeArray(sfi.GetIsolate()).SourcePositionTable();
  }
  return source_position_table();
}

Object Code::next_code_link() const {
  return code_data_container(kAcquireLoad).next_code_link();
}

void Code::set_next_code_link(Object value) {
  code_data_container(kAcquireLoad).set_next_code_link(value);
}

Address Code::raw_body_start() const { return raw_instruction_start(); }

Address Code::raw_body_end() const {
  return raw_body_start() + raw_body_size();
}

int Code::raw_body_size() const {
  return raw_instruction_size() + raw_metadata_size();
}

int Code::InstructionSize() const {
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapInstructionSize()
                                               : raw_instruction_size();
}

Address Code::raw_instruction_start() const {
  return field_address(kHeaderSize);
}

Address Code::InstructionStart() const {
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapInstructionStart()
                                               : raw_instruction_start();
}

Address Code::raw_instruction_end() const {
  return raw_instruction_start() + raw_instruction_size();
}

Address Code::InstructionEnd() const {
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapInstructionEnd()
                                               : raw_instruction_end();
}

Address Code::raw_metadata_start() const {
  return raw_instruction_start() + raw_instruction_size();
}

Address Code::InstructionStart(Isolate* isolate, Address pc) const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapInstructionStart(isolate, pc)
             : raw_instruction_start();
}

Address Code::InstructionEnd(Isolate* isolate, Address pc) const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapInstructionEnd(isolate, pc)
             : raw_instruction_end();
}

int Code::GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const {
  Address instruction_start = InstructionStart(isolate, pc);
  Address offset = pc - instruction_start;
  DCHECK_LE(offset, InstructionSize());
  return static_cast<int>(offset);
}

Address Code::MetadataStart() const {
  STATIC_ASSERT(kOnHeapBodyIsContiguous);
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapMetadataStart()
                                               : raw_metadata_start();
}

Address Code::raw_metadata_end() const {
  return raw_metadata_start() + raw_metadata_size();
}

Address Code::MetadataEnd() const {
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapMetadataEnd()
                                               : raw_metadata_end();
}

int Code::MetadataSize() const {
  return V8_UNLIKELY(is_off_heap_trampoline()) ? OffHeapMetadataSize()
                                               : raw_metadata_size();
}

int Code::SizeIncludingMetadata() const {
  int size = CodeSize();
  size += relocation_info().Size();
  if (kind() != CodeKind::BASELINE) {
    size += deoptimization_data().Size();
  }
  return size;
}

ByteArray Code::unchecked_relocation_info() const {
  PtrComprCageBase cage_base = main_cage_base();
  return ByteArray::unchecked_cast(
      TaggedField<HeapObject, kRelocationInfoOffset>::load(cage_base, *this));
}

byte* Code::relocation_start() const {
  return unchecked_relocation_info().GetDataStartAddress();
}

byte* Code::relocation_end() const {
  return unchecked_relocation_info().GetDataEndAddress();
}

int Code::relocation_size() const {
  return unchecked_relocation_info().length();
}

Address Code::entry() const { return raw_instruction_start(); }

bool Code::contains(Isolate* isolate, Address inner_pointer) {
  if (is_off_heap_trampoline()) {
    if (OffHeapInstructionStart(isolate, inner_pointer) <= inner_pointer &&
        inner_pointer < OffHeapInstructionEnd(isolate, inner_pointer)) {
      return true;
    }
  }
  return (address() <= inner_pointer) && (inner_pointer < address() + Size());
}

// static
void Code::CopyRelocInfoToByteArray(ByteArray dest, const CodeDesc& desc) {
  DCHECK_EQ(dest.length(), desc.reloc_size);
  CopyBytes(dest.GetDataStartAddress(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));
}

int Code::CodeSize() const { return SizeFor(raw_body_size()); }

CodeKind Code::kind() const {
  STATIC_ASSERT(FIELD_SIZE(kFlagsOffset) == kInt32Size);
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
    return GetBaselineEndPCForBytecodeOffset(bytecode_offset, bytecodes);
  }
}

void Code::initialize_flags(CodeKind kind, bool is_turbofanned, int stack_slots,
                            bool is_off_heap_trampoline) {
  CHECK(0 <= stack_slots && stack_slots < StackSlotsField::kMax);
  DCHECK(!CodeKindIsInterpretedJSFunction(kind));
  uint32_t flags = KindField::encode(kind) |
                   IsTurbofannedField::encode(is_turbofanned) |
                   StackSlotsField::encode(stack_slots) |
                   IsOffHeapTrampoline::encode(is_off_heap_trampoline);
  STATIC_ASSERT(FIELD_SIZE(kFlagsOffset) == kInt32Size);
  RELAXED_WRITE_UINT32_FIELD(*this, kFlagsOffset, flags);
  DCHECK_IMPLIES(stack_slots != 0, has_safepoint_info());
}

inline bool Code::is_interpreter_trampoline_builtin() const {
  // Check for kNoBuiltinId first to abort early when the current Code object
  // is not a builtin.
  return builtin_id() != Builtin::kNoBuiltinId &&
         (builtin_id() == Builtin::kInterpreterEntryTrampoline ||
          builtin_id() == Builtin::kInterpreterEnterAtBytecode ||
          builtin_id() == Builtin::kInterpreterEnterAtNextBytecode);
}

inline bool Code::is_baseline_trampoline_builtin() const {
  return builtin_id() != Builtin::kNoBuiltinId &&
         (builtin_id() == Builtin::kBaselineOutOfLinePrologue ||
          builtin_id() == Builtin::kBaselineOrInterpreterEnterAtBytecode ||
          builtin_id() == Builtin::kBaselineOrInterpreterEnterAtNextBytecode);
}

inline bool Code::is_baseline_leave_frame_builtin() const {
  return builtin_id() == Builtin::kBaselineLeaveFrame;
}

inline bool Code::checks_optimization_marker() const {
  bool checks_marker = (builtin_id() == Builtin::kCompileLazy ||
                        builtin_id() == Builtin::kInterpreterEntryTrampoline ||
                        CodeKindCanTierUp(kind()));
  return checks_marker ||
         (CodeKindCanDeoptimize(kind()) && marked_for_deoptimization());
}

inline bool Code::has_tagged_outgoing_params() const {
  return kind() != CodeKind::JS_TO_WASM_FUNCTION &&
         kind() != CodeKind::C_WASM_ENTRY && kind() != CodeKind::WASM_FUNCTION;
}

inline bool Code::is_turbofanned() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return IsTurbofannedField::decode(flags);
}

inline bool Code::can_have_weak_objects() const {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return CanHaveWeakObjectsField::decode(flags);
}

inline void Code::set_can_have_weak_objects(bool value) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = CanHaveWeakObjectsField::update(previous, value);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

inline bool Code::is_promise_rejection() const {
  DCHECK(kind() == CodeKind::BUILTIN);
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return IsPromiseRejectionField::decode(flags);
}

inline void Code::set_is_promise_rejection(bool value) {
  DCHECK(kind() == CodeKind::BUILTIN);
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = IsPromiseRejectionField::update(previous, value);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

inline bool Code::is_exception_caught() const {
  DCHECK(kind() == CodeKind::BUILTIN);
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return IsExceptionCaughtField::decode(flags);
}

inline void Code::set_is_exception_caught(bool value) {
  DCHECK(kind() == CodeKind::BUILTIN);
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = IsExceptionCaughtField::update(previous, value);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

inline bool Code::is_off_heap_trampoline() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return IsOffHeapTrampoline::decode(flags);
}

inline HandlerTable::CatchPrediction Code::GetBuiltinCatchPrediction() {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
  if (is_exception_caught()) return HandlerTable::CAUGHT;
  return HandlerTable::UNCAUGHT;
}

Builtin Code::builtin_id() const {
  int index = RELAXED_READ_INT_FIELD(*this, kBuiltinIndexOffset);
  DCHECK(index == static_cast<int>(Builtin::kNoBuiltinId) ||
         Builtins::IsBuiltinId(index));
  return static_cast<Builtin>(index);
}

void Code::set_builtin_id(Builtin builtin) {
  DCHECK(builtin == Builtin::kNoBuiltinId || Builtins::IsBuiltinId(builtin));
  RELAXED_WRITE_INT_FIELD(*this, kBuiltinIndexOffset,
                          static_cast<int>(builtin));
}

bool Code::is_builtin() const { return builtin_id() != Builtin::kNoBuiltinId; }

unsigned Code::inlined_bytecode_size() const {
  unsigned size = RELAXED_READ_UINT_FIELD(*this, kInlinedBytecodeSizeOffset);
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  return size;
}

void Code::set_inlined_bytecode_size(unsigned size) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  RELAXED_WRITE_UINT_FIELD(*this, kInlinedBytecodeSizeOffset, size);
}

bool Code::has_safepoint_info() const {
  return is_turbofanned() || is_wasm_code();
}

int Code::stack_slots() const {
  DCHECK(has_safepoint_info());
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return StackSlotsField::decode(flags);
}

bool Code::marked_for_deoptimization() const {
  DCHECK(CodeKindCanDeoptimize(kind()));
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return MarkedForDeoptimizationField::decode(flags);
}

void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK(CodeKindCanDeoptimize(kind()));
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = MarkedForDeoptimizationField::update(previous, flag);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

int Code::deoptimization_count() const {
  DCHECK(CodeKindCanDeoptimize(kind()));
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  int count = DeoptCountField::decode(flags);
  DCHECK_GE(count, 0);
  return count;
}

void Code::increment_deoptimization_count() {
  DCHECK(CodeKindCanDeoptimize(kind()));
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t flags = container.kind_specific_flags(kRelaxedLoad);
  int32_t count = DeoptCountField::decode(flags);
  DCHECK_GE(count, 0);
  CHECK_LE(count + 1, DeoptCountField::kMax);
  int32_t updated = DeoptCountField::update(flags, count + 1);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

bool Code::embedded_objects_cleared() const {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return EmbeddedObjectsClearedField::decode(flags);
}

void Code::set_embedded_objects_cleared(bool flag) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()));
  DCHECK_IMPLIES(flag, marked_for_deoptimization());
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = EmbeddedObjectsClearedField::update(previous, flag);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

bool Code::deopt_already_counted() const {
  DCHECK(CodeKindCanDeoptimize(kind()));
  int32_t flags =
      code_data_container(kAcquireLoad).kind_specific_flags(kRelaxedLoad);
  return DeoptAlreadyCountedField::decode(flags);
}

void Code::set_deopt_already_counted(bool flag) {
  DCHECK(CodeKindCanDeoptimize(kind()));
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  CodeDataContainer container = code_data_container(kAcquireLoad);
  int32_t previous = container.kind_specific_flags(kRelaxedLoad);
  int32_t updated = DeoptAlreadyCountedField::update(previous, flag);
  container.set_kind_specific_flags(updated, kRelaxedStore);
}

bool Code::is_optimized_code() const {
  return CodeKindIsOptimizedJSFunction(kind());
}
bool Code::is_wasm_code() const { return kind() == CodeKind::WASM_FUNCTION; }

int Code::constant_pool_offset() const {
  if (!FLAG_enable_embedded_constant_pool) {
    // Redirection needed since the field doesn't exist in this case.
    return code_comments_offset();
  }
  return ReadField<int>(kConstantPoolOffsetOffset);
}

void Code::set_constant_pool_offset(int value) {
  if (!FLAG_enable_embedded_constant_pool) {
    // Redirection needed since the field doesn't exist in this case.
    return;
  }
  DCHECK_LE(value, MetadataSize());
  WriteField<int>(kConstantPoolOffsetOffset, value);
}

Address Code::constant_pool() const {
  if (!has_constant_pool()) return kNullAddress;
  return MetadataStart() + constant_pool_offset();
}

Address Code::code_comments() const {
  return MetadataStart() + code_comments_offset();
}

Address Code::unwinding_info_start() const {
  return MetadataStart() + unwinding_info_offset();
}

Address Code::unwinding_info_end() const { return MetadataEnd(); }

int Code::unwinding_info_size() const {
  DCHECK_GE(unwinding_info_end(), unwinding_info_start());
  return static_cast<int>(unwinding_info_end() - unwinding_info_start());
}

bool Code::has_unwinding_info() const { return unwinding_info_size() > 0; }

Code Code::GetCodeFromTargetAddress(Address address) {
  {
    // TODO(jgruber,v8:6666): Support embedded builtins here. We'd need to pass
    // in the current isolate.
    Address start =
        reinterpret_cast<Address>(Isolate::CurrentEmbeddedBlobCode());
    Address end = start + Isolate::CurrentEmbeddedBlobCodeSize();
    CHECK(address < start || address >= end);
  }

  HeapObject code = HeapObject::FromAddress(address - Code::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently
  // not being a forwarding pointer.
  return Code::unchecked_cast(code);
}

Code Code::GetObjectFromEntryAddress(Address location_of_address) {
  Address code_entry = base::Memory<Address>(location_of_address);
  HeapObject code = HeapObject::FromAddress(code_entry - Code::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently
  // not being a forwarding pointer.
  return Code::unchecked_cast(code);
}

bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}

bool Code::IsWeakObject(HeapObject object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}

bool Code::IsWeakObjectInOptimizedCode(HeapObject object) {
  Map map = object.map(kAcquireLoad);
  InstanceType instance_type = map.instance_type();
  if (InstanceTypeChecker::IsMap(instance_type)) {
    return Map::cast(object).CanTransition();
  }
  return InstanceTypeChecker::IsPropertyCell(instance_type) ||
         InstanceTypeChecker::IsJSReceiver(instance_type) ||
         InstanceTypeChecker::IsContext(instance_type);
}

bool Code::IsExecutable() {
  return !Builtins::IsBuiltinId(builtin_id()) || !is_off_heap_trampoline() ||
         Builtins::CodeObjectIsExecutable(builtin_id());
}

// This field has to have relaxed atomic accessors because it is accessed in the
// concurrent marker.
STATIC_ASSERT(FIELD_SIZE(CodeDataContainer::kKindSpecificFlagsOffset) ==
              kInt32Size);
RELAXED_INT32_ACCESSORS(CodeDataContainer, kind_specific_flags,
                        kKindSpecificFlagsOffset)

#if defined(V8_TARGET_LITTLE_ENDIAN)
static_assert(!V8_EXTERNAL_CODE_SPACE_BOOL ||
                  (CodeDataContainer::kCodeCageBaseUpper32BitsOffset ==
                   CodeDataContainer::kCodeOffset + kTaggedSize),
              "CodeDataContainer::code field layout requires updating "
              "for little endian architectures");
#elif defined(V8_TARGET_BIG_ENDIAN)
static_assert(!V8_EXTERNAL_CODE_SPACE_BOOL,
              "CodeDataContainer::code field layout requires updating "
              "for big endian architectures");
#endif

DEF_GETTER(CodeDataContainer, raw_code, Object) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  // Given the fields layout we can write the Code reference as a full word
  // (see the static asserts above).
  Address* p = reinterpret_cast<Address*>(address() + kCodeOffset);
  Object value = Object(*p);
  return value;
}

void CodeDataContainer::set_raw_code(Object value, WriteBarrierMode mode) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  // Given the fields layout we can write the Code reference as a full word
  // (see the static asserts above).
  Address* p = reinterpret_cast<Address*>(address() + kCodeOffset);
  *p = value.ptr();
  CONDITIONAL_WRITE_BARRIER(*this, kCodeOffset, value, mode);
}

Object CodeDataContainer::raw_code(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return CodeDataContainer::raw_code(cage_base, tag);
}

Object CodeDataContainer::raw_code(PtrComprCageBase cage_base,
                                   RelaxedLoadTag) const {
  Object value =
      TaggedField<Object, kCodeOffset>::Relaxed_Load(cage_base, *this);
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  return value;
}

ACCESSORS(CodeDataContainer, next_code_link, Object, kNextCodeLinkOffset)

PtrComprCageBase CodeDataContainer::code_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  CHECK(!V8_HEAP_SANDBOX_BOOL);
  Address code_cage_base_hi =
      ReadField<Tagged_t>(kCodeCageBaseUpper32BitsOffset);
  return PtrComprCageBase(code_cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void CodeDataContainer::set_code_cage_base(Address code_cage_base) {
#ifdef V8_EXTERNAL_CODE_SPACE
  CHECK(!V8_HEAP_SANDBOX_BOOL);
  Tagged_t code_cage_base_hi = static_cast<Tagged_t>(code_cage_base >> 32);
  WriteField<Tagged_t>(kCodeCageBaseUpper32BitsOffset, code_cage_base_hi);
#else
  UNREACHABLE();
#endif
}

void CodeDataContainer::AllocateExternalPointerEntries(Isolate* isolate) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  InitExternalPointerField(kCodeEntryPointOffset, isolate);
}

Code CodeDataContainer::code() const {
  PtrComprCageBase cage_base = code_cage_base();
  return CodeDataContainer::code(cage_base);
}
Code CodeDataContainer::code(PtrComprCageBase cage_base) const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  return Code::cast(raw_code(cage_base));
}

Code CodeDataContainer::code(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return CodeDataContainer::code(cage_base, tag);
}

Code CodeDataContainer::code(PtrComprCageBase cage_base,
                             RelaxedLoadTag tag) const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  return Code::cast(raw_code(cage_base, tag));
}

DEF_GETTER(CodeDataContainer, code_entry_point, Address) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  Isolate* isolate = GetIsolateForHeapSandbox(*this);
  return ReadExternalPointerField(kCodeEntryPointOffset, isolate,
                                  kCodeEntryPointTag);
}

void CodeDataContainer::set_code_entry_point(Isolate* isolate, Address value) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  WriteExternalPointerField(kCodeEntryPointOffset, isolate, value,
                            kCodeEntryPointTag);
}

void CodeDataContainer::SetCodeAndEntryPoint(Isolate* isolate_for_sandbox,
                                             Code code, WriteBarrierMode mode) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  set_raw_code(code, mode);
  set_code_entry_point(isolate_for_sandbox, code.InstructionStart());
}

void CodeDataContainer::UpdateCodeEntryPoint(Isolate* isolate_for_sandbox,
                                             Code code) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  DCHECK_EQ(raw_code(), code);
  set_code_entry_point(isolate_for_sandbox, code.InstructionStart());
}

Address CodeDataContainer::InstructionStart() const {
  return code_entry_point();
}

void CodeDataContainer::clear_padding() {
  memset(reinterpret_cast<void*>(address() + kUnalignedSize), 0,
         kSize - kUnalignedSize);
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

int BytecodeArray::osr_loop_nesting_level() const {
  return ACQUIRE_READ_INT8_FIELD(*this, kOsrLoopNestingLevelOffset);
}

void BytecodeArray::set_osr_loop_nesting_level(int depth) {
  DCHECK(0 <= depth && depth <= AbstractCode::kMaxLoopNestingMarker);
  STATIC_ASSERT(AbstractCode::kMaxLoopNestingMarker < kMaxInt8);
  RELEASE_WRITE_INT8_FIELD(*this, kOsrLoopNestingLevelOffset, depth);
}

BytecodeArray::Age BytecodeArray::bytecode_age() const {
  // Bytecode is aged by the concurrent marker.
  return static_cast<Age>(RELAXED_READ_INT8_FIELD(*this, kBytecodeAgeOffset));
}

void BytecodeArray::set_bytecode_age(BytecodeArray::Age age) {
  DCHECK_GE(age, kFirstBytecodeAge);
  DCHECK_LE(age, kLastBytecodeAge);
  STATIC_ASSERT(kLastBytecodeAge <= kMaxInt8);
  // Bytecode is aged by the concurrent marker.
  RELAXED_WRITE_INT8_FIELD(*this, kBytecodeAgeOffset, static_cast<int8_t>(age));
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

ByteArray BytecodeArray::SourcePositionTable() const {
  // WARNING: This function may be called from a background thread, hence
  // changes to how it accesses the heap can easily lead to bugs.
  Object maybe_table = source_position_table(kAcquireLoad);
  if (maybe_table.IsByteArray()) return ByteArray::cast(maybe_table);
  ReadOnlyRoots roots = GetReadOnlyRoots();
  DCHECK(maybe_table.IsUndefined(roots) || maybe_table.IsException(roots));
  return roots.empty_byte_array();
}

int BytecodeArray::BytecodeArraySize() { return SizeFor(this->length()); }

int BytecodeArray::SizeIncludingMetadata() {
  int size = BytecodeArraySize();
  size += constant_pool().Size();
  size += handler_table().Size();
  ByteArray table = SourcePositionTable();
  if (table.length() != 0) {
    size += table.Size();
  }
  return size;
}

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, TranslationArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
DEFINE_DEOPT_ELEMENT_ACCESSORS(DeoptExitStart, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(EagerSoftAndBailoutDeoptCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LazyDeoptCount, Smi)

DEFINE_DEOPT_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)
#ifdef DEBUG
DEFINE_DEOPT_ENTRY_ACCESSORS(NodeId, Smi)
#endif  // DEBUG

BytecodeOffset DeoptimizationData::GetBytecodeOffset(int i) {
  return BytecodeOffset(BytecodeOffsetRaw(i).value());
}

void DeoptimizationData::SetBytecodeOffset(int i, BytecodeOffset value) {
  SetBytecodeOffsetRaw(i, Smi::FromInt(value.ToInt()));
}

int DeoptimizationData::DeoptCount() {
  return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_INL_H_
