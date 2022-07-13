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
OBJECT_CONSTRUCTORS_IMPL(CodeDataContainer, HeapObject)
NEVER_READ_ONLY_SPACE_IMPL(CodeDataContainer)

NEVER_READ_ONLY_SPACE_IMPL(AbstractCode)

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(CodeDataContainer)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(DeoptimizationData)
CAST_ACCESSOR(DeoptimizationLiteralArray)

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

bool AbstractCode::contains(Isolate* isolate, Address inner_pointer) {
  PtrComprCageBase cage_base(isolate);
  if (IsCode(cage_base)) {
    return GetCode().contains(isolate, inner_pointer);
  } else {
    return (address() <= inner_pointer) &&
           (inner_pointer <= address() + Size(cage_base));
  }
}

CodeKind AbstractCode::kind() {
  return IsCode() ? GetCode().kind() : CodeKind::INTERPRETED_FUNCTION;
}

Code AbstractCode::GetCode() { return Code::cast(*this); }

BytecodeArray AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(*this);
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
// using main_cage_base(kRelaxedLoad) for computing the base.
#define RELEASE_ACQUIRE_CODE_ACCESSORS_CHECKED2(name, type, offset,           \
                                                get_condition, set_condition) \
  type Code::name(AcquireLoadTag tag) const {                                 \
    PtrComprCageBase cage_base = main_cage_base(kRelaxedLoad);                \
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

PtrComprCageBase Code::main_cage_base(RelaxedLoadTag) const {
#ifdef V8_EXTERNAL_CODE_SPACE
  Address cage_base_hi =
      Relaxed_ReadField<Tagged_t>(kMainCageBaseUpper32BitsOffset);
  return PtrComprCageBase(cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void Code::set_main_cage_base(Address cage_base, RelaxedStoreTag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Tagged_t cage_base_hi = static_cast<Tagged_t>(cage_base >> 32);
  Relaxed_WriteField<Tagged_t>(kMainCageBaseUpper32BitsOffset, cage_base_hi);
#else
  UNREACHABLE();
#endif
}

CodeDataContainer Code::GCSafeCodeDataContainer(AcquireLoadTag) const {
  PtrComprCageBase cage_base = main_cage_base(kRelaxedLoad);
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
#ifdef V8_EXTERNAL_CODE_SPACE
  return code.code_data_container(kAcquireLoad);
#else
  return code;
#endif
}

inline Handle<CodeT> ToCodeT(Handle<Code> code, Isolate* isolate) {
#ifdef V8_EXTERNAL_CODE_SPACE
  return handle(ToCodeT(*code), isolate);
#else
  return code;
#endif
}

inline MaybeHandle<CodeT> ToCodeT(MaybeHandle<Code> maybe_code,
                                  Isolate* isolate) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Handle<Code> code;
  if (maybe_code.ToHandle(&code)) return ToCodeT(code, isolate);
  return {};
#else
  return maybe_code;
#endif
}

inline Code FromCodeT(CodeT code) {
#ifdef V8_EXTERNAL_CODE_SPACE
  return code.code();
#else
  return code;
#endif
}

inline Code FromCodeT(CodeT code, RelaxedLoadTag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  return code.code(kRelaxedLoad);
#else
  return code;
#endif
}

inline Handle<Code> FromCodeT(Handle<CodeT> code, Isolate* isolate) {
#ifdef V8_EXTERNAL_CODE_SPACE
  return handle(FromCodeT(*code), isolate);
#else
  return code;
#endif
}

inline Handle<AbstractCode> ToAbstractCode(Handle<CodeT> code,
                                           Isolate* isolate) {
  return Handle<AbstractCode>::cast(FromCodeT(code, isolate));
}

inline CodeDataContainer CodeDataContainerFromCodeT(CodeT code) {
#ifdef V8_EXTERNAL_CODE_SPACE
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
    set_main_cage_base(kNullAddress, kRelaxedStore);
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
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapInstructionSize(*this, builtin_id())
             : raw_instruction_size();
}

Address Code::raw_instruction_start() const {
  return field_address(kHeaderSize);
}

Address Code::InstructionStart() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? i::OffHeapInstructionStart(*this, builtin_id())
             : raw_instruction_start();
}

Address Code::raw_instruction_end() const {
  return raw_instruction_start() + raw_instruction_size();
}

Address Code::InstructionEnd() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? i::OffHeapInstructionEnd(*this, builtin_id())
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

Address Code::raw_metadata_end() const {
  return raw_metadata_start() + raw_metadata_size();
}

int Code::MetadataSize() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapMetadataSize(*this, builtin_id())
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

Address Code::SafepointTableAddress() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapSafepointTableAddress(*this, builtin_id())
             : raw_metadata_start() + safepoint_table_offset();
}

int Code::safepoint_table_size() const {
  DCHECK_GE(handler_table_offset() - safepoint_table_offset(), 0);
  return handler_table_offset() - safepoint_table_offset();
}

bool Code::has_safepoint_table() const { return safepoint_table_size() > 0; }

Address Code::HandlerTableAddress() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapHandlerTableAddress(*this, builtin_id())
             : raw_metadata_start() + handler_table_offset();
}

int Code::handler_table_size() const {
  DCHECK_GE(constant_pool_offset() - handler_table_offset(), 0);
  return constant_pool_offset() - handler_table_offset();
}

bool Code::has_handler_table() const { return handler_table_size() > 0; }

int Code::constant_pool_size() const {
  const int size = code_comments_offset() - constant_pool_offset();
  DCHECK_IMPLIES(!FLAG_enable_embedded_constant_pool, size == 0);
  DCHECK_GE(size, 0);
  return size;
}

bool Code::has_constant_pool() const { return constant_pool_size() > 0; }

int Code::code_comments_size() const {
  DCHECK_GE(unwinding_info_offset() - code_comments_offset(), 0);
  return unwinding_info_offset() - code_comments_offset();
}

bool Code::has_code_comments() const { return code_comments_size() > 0; }

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
  return (address() <= inner_pointer) &&
         (inner_pointer < address() + CodeSize());
}

// static
void Code::CopyRelocInfoToByteArray(ByteArray dest, const CodeDesc& desc) {
  DCHECK_EQ(dest.length(), desc.reloc_size);
  CopyBytes(dest.GetDataStartAddress(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));
}

int Code::CodeSize() const { return SizeFor(raw_body_size()); }

DEF_GETTER(Code, Size, int) { return CodeSize(); }

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
  DCHECK_IMPLIES(stack_slots != 0, uses_safepoint_table());
  DCHECK_IMPLIES(!uses_safepoint_table(), stack_slots == 0);
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

#ifdef V8_EXTERNAL_CODE_SPACE
// Note, must be in sync with Code::checks_tiering_state().
inline bool CodeDataContainer::checks_tiering_state() const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  bool checks_state = (builtin_id() == Builtin::kCompileLazy ||
                       builtin_id() == Builtin::kInterpreterEntryTrampoline ||
                       CodeKindCanTierUp(kind()));
  return checks_state ||
         (CodeKindCanDeoptimize(kind()) && marked_for_deoptimization());
}
#endif  // V8_EXTERNAL_CODE_SPACE

// Note, must be in sync with CodeDataContainer::checks_tiering_state().
inline bool Code::checks_tiering_state() const {
  bool checks_state = (builtin_id() == Builtin::kCompileLazy ||
                       builtin_id() == Builtin::kInterpreterEntryTrampoline ||
                       CodeKindCanTierUp(kind()));
  return checks_state ||
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

bool Code::is_maglevved() const { return kind() == CodeKind::MAGLEV; }

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

inline bool Code::is_off_heap_trampoline() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  return IsOffHeapTrampoline::decode(flags);
}

inline HandlerTable::CatchPrediction Code::GetBuiltinCatchPrediction() {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
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

bool Code::uses_safepoint_table() const {
  return is_turbofanned() || is_maglevved() || is_wasm_code();
}

int Code::stack_slots() const {
  const uint32_t flags = RELAXED_READ_UINT32_FIELD(*this, kFlagsOffset);
  const int slots = StackSlotsField::decode(flags);
  DCHECK_IMPLIES(!uses_safepoint_table(), slots == 0);
  return slots;
}

bool CodeDataContainer::marked_for_deoptimization() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  // kind field is not available on CodeDataContainer when external code space
  // is not enabled.
  DCHECK(CodeKindCanDeoptimize(kind()));
#endif  // V8_EXTERNAL_CODE_SPACE
  int32_t flags = kind_specific_flags(kRelaxedLoad);
  return Code::MarkedForDeoptimizationField::decode(flags);
}

bool Code::marked_for_deoptimization() const {
  DCHECK(CodeKindCanDeoptimize(kind()));
  return code_data_container(kAcquireLoad).marked_for_deoptimization();
}

void CodeDataContainer::set_marked_for_deoptimization(bool flag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  // kind field is not available on CodeDataContainer when external code space
  // is not enabled.
  DCHECK(CodeKindCanDeoptimize(kind()));
#endif  // V8_EXTERNAL_CODE_SPACE
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int32_t previous = kind_specific_flags(kRelaxedLoad);
  int32_t updated = Code::MarkedForDeoptimizationField::update(previous, flag);
  set_kind_specific_flags(updated, kRelaxedStore);
}

void Code::set_marked_for_deoptimization(bool flag) {
  code_data_container(kAcquireLoad).set_marked_for_deoptimization(flag);
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
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapConstantPoolAddress(*this, builtin_id())
             : raw_metadata_start() + constant_pool_offset();
}

Address Code::code_comments() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapCodeCommentsAddress(*this, builtin_id())
             : raw_metadata_start() + code_comments_offset();
}

Address Code::unwinding_info_start() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapUnwindingInfoAddress(*this, builtin_id())
             : raw_metadata_start() + unwinding_info_offset();
}

Address Code::unwinding_info_end() const {
  return V8_UNLIKELY(is_off_heap_trampoline())
             ? OffHeapMetadataEnd(*this, builtin_id())
             : raw_metadata_end();
}

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

bool Code::IsWeakObjectInDeoptimizationLiteralArray(Object object) {
  // Maps must be strong because they can be used as part of the description for
  // how to materialize an object upon deoptimization, in which case it is
  // possible to reach the code that requires the Map without anything else
  // holding a strong pointer to that Map.
  return object.IsHeapObject() && !object.IsMap() &&
         Code::IsWeakObjectInOptimizedCode(HeapObject::cast(object));
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

Object CodeDataContainer::raw_code() const {
  PtrComprCageBase cage_base = code_cage_base();
  return CodeDataContainer::raw_code(cage_base);
}

Object CodeDataContainer::raw_code(PtrComprCageBase cage_base) const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  Object value = TaggedField<Object, kCodeOffset>::load(cage_base, *this);
  return value;
}

void CodeDataContainer::set_raw_code(Object value, WriteBarrierMode mode) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  TaggedField<Object, kCodeOffset>::store(*this, value);
  CONDITIONAL_WRITE_BARRIER(*this, kCodeOffset, value, mode);
}

Object CodeDataContainer::raw_code(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base(tag);
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
  // TODO(v8:10391): consider protecting this value with the sandbox.
  Address code_cage_base_hi =
      ReadField<Tagged_t>(kCodeCageBaseUpper32BitsOffset);
  return PtrComprCageBase(code_cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void CodeDataContainer::set_code_cage_base(Address code_cage_base) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Tagged_t code_cage_base_hi = static_cast<Tagged_t>(code_cage_base >> 32);
  WriteField<Tagged_t>(kCodeCageBaseUpper32BitsOffset, code_cage_base_hi);
#else
  UNREACHABLE();
#endif
}

PtrComprCageBase CodeDataContainer::code_cage_base(RelaxedLoadTag) const {
#ifdef V8_EXTERNAL_CODE_SPACE
  // TODO(v8:10391): consider protecting this value with the sandbox.
  Address code_cage_base_hi =
      Relaxed_ReadField<Tagged_t>(kCodeCageBaseUpper32BitsOffset);
  return PtrComprCageBase(code_cage_base_hi << 32);
#else
  return GetPtrComprCageBase(*this);
#endif
}

void CodeDataContainer::set_code_cage_base(Address code_cage_base,
                                           RelaxedStoreTag) {
#ifdef V8_EXTERNAL_CODE_SPACE
  Tagged_t code_cage_base_hi = static_cast<Tagged_t>(code_cage_base >> 32);
  Relaxed_WriteField<Tagged_t>(kCodeCageBaseUpper32BitsOffset,
                               code_cage_base_hi);
#else
  UNREACHABLE();
#endif
}

void CodeDataContainer::AllocateExternalPointerEntries(Isolate* isolate) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  InitExternalPointerField(kCodeEntryPointOffset, isolate, kCodeEntryPointTag);
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
  PtrComprCageBase cage_base = code_cage_base(tag);
  return CodeDataContainer::code(cage_base, tag);
}

Code CodeDataContainer::code(PtrComprCageBase cage_base,
                             RelaxedLoadTag tag) const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  return Code::cast(raw_code(cage_base, tag));
}

DEF_GETTER(CodeDataContainer, code_entry_point, Address) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  Isolate* isolate = GetIsolateForSandbox(*this);
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

Address CodeDataContainer::raw_instruction_start() {
  return code_entry_point();
}

Address CodeDataContainer::entry() const { return code_entry_point(); }

void CodeDataContainer::clear_padding() {
  memset(reinterpret_cast<void*>(address() + kUnalignedSize), 0,
         kSize - kUnalignedSize);
}

RELAXED_UINT16_ACCESSORS(CodeDataContainer, flags, kFlagsOffset)

// Ensure builtin_id field fits into int16_t, so that we can rely on sign
// extension to convert int16_t{-1} to kNoBuiltinId.
// If the asserts fail, update the code that use kBuiltinIdOffset below.
STATIC_ASSERT(static_cast<int>(Builtin::kNoBuiltinId) == -1);
STATIC_ASSERT(Builtins::kBuiltinCount < std::numeric_limits<int16_t>::max());

void CodeDataContainer::initialize_flags(CodeKind kind, Builtin builtin_id) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  uint16_t value = KindField::encode(kind);
  set_flags(value, kRelaxedStore);

  WriteField<int16_t>(kBuiltinIdOffset, static_cast<int16_t>(builtin_id));
}

#ifdef V8_EXTERNAL_CODE_SPACE

CodeKind CodeDataContainer::kind() const {
  return KindField::decode(flags(kRelaxedLoad));
}

Builtin CodeDataContainer::builtin_id() const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  // Rely on sign-extension when converting int16_t to int to preserve
  // kNoBuiltinId value.
  STATIC_ASSERT(static_cast<int>(static_cast<int16_t>(Builtin::kNoBuiltinId)) ==
                static_cast<int>(Builtin::kNoBuiltinId));
  int value = ReadField<int16_t>(kBuiltinIdOffset);
  return static_cast<Builtin>(value);
}

bool CodeDataContainer::is_builtin() const {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  return builtin_id() != Builtin::kNoBuiltinId;
}

bool CodeDataContainer::is_optimized_code() const {
  return CodeKindIsOptimizedJSFunction(kind());
}

inline bool CodeDataContainer::is_interpreter_trampoline_builtin() const {
  return IsInterpreterTrampolineBuiltin(builtin_id());
}

//
// A collection of getters and predicates that forward queries to associated
// Code object.
//

#define DEF_PRIMITIVE_FORWARDING_CDC_GETTER(name, type) \
  type CodeDataContainer::name() const { return FromCodeT(*this).name(); }

#define DEF_FORWARDING_CDC_GETTER(name, type) \
  DEF_GETTER(CodeDataContainer, name, type) { \
    return FromCodeT(*this).name(cage_base);  \
  }

DEF_PRIMITIVE_FORWARDING_CDC_GETTER(is_maglevved, bool)
DEF_PRIMITIVE_FORWARDING_CDC_GETTER(is_turbofanned, bool)
DEF_PRIMITIVE_FORWARDING_CDC_GETTER(is_off_heap_trampoline, bool)

DEF_FORWARDING_CDC_GETTER(deoptimization_data, FixedArray)
DEF_FORWARDING_CDC_GETTER(bytecode_or_interpreter_data, HeapObject)
DEF_FORWARDING_CDC_GETTER(source_position_table, ByteArray)
DEF_FORWARDING_CDC_GETTER(bytecode_offset_table, ByteArray)

#undef DEF_PRIMITIVE_FORWARDING_CDC_GETTER
#undef DEF_FORWARDING_CDC_GETTER

#endif  // V8_EXTERNAL_CODE_SPACE

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

int BytecodeArray::osr_urgency() const {
  return OsrUrgencyBits::decode(osr_urgency_and_install_target());
}

void BytecodeArray::set_osr_urgency(int urgency) {
  DCHECK(0 <= urgency && urgency <= BytecodeArray::kMaxOsrUrgency);
  STATIC_ASSERT(BytecodeArray::kMaxOsrUrgency <= OsrUrgencyBits::kMax);
  uint32_t value = osr_urgency_and_install_target();
  set_osr_urgency_and_install_target(OsrUrgencyBits::update(value, urgency));
}

BytecodeArray::Age BytecodeArray::bytecode_age() const {
  // Bytecode is aged by the concurrent marker.
  static_assert(kBytecodeAgeSize == kUInt16Size);
  return static_cast<Age>(RELAXED_READ_INT16_FIELD(*this, kBytecodeAgeOffset));
}

void BytecodeArray::reset_osr_urgency() { set_osr_urgency(0); }

void BytecodeArray::RequestOsrAtNextOpportunity() {
  set_osr_urgency(kMaxOsrUrgency);
}

int BytecodeArray::osr_install_target() {
  return OsrInstallTargetBits::decode(osr_urgency_and_install_target());
}

void BytecodeArray::set_osr_install_target(BytecodeOffset jump_loop_offset) {
  DCHECK_LE(jump_loop_offset.ToInt(), length());
  set_osr_urgency_and_install_target(OsrInstallTargetBits::update(
      osr_urgency_and_install_target(), OsrInstallTargetFor(jump_loop_offset)));
}

void BytecodeArray::reset_osr_install_target() {
  uint32_t value = osr_urgency_and_install_target();
  set_osr_urgency_and_install_target(
      OsrInstallTargetBits::update(value, kNoOsrInstallTarget));
}

void BytecodeArray::reset_osr_urgency_and_install_target() {
  set_osr_urgency_and_install_target(OsrUrgencyBits::encode(0) |
                                     OsrInstallTargetBits::encode(0));
}

void BytecodeArray::set_bytecode_age(BytecodeArray::Age age) {
  DCHECK_GE(age, kFirstBytecodeAge);
  DCHECK_LE(age, kLastBytecodeAge);
  static_assert(kLastBytecodeAge <= kMaxInt16);
  static_assert(kBytecodeAgeSize == kUInt16Size);
  // Bytecode is aged by the concurrent marker.
  RELAXED_WRITE_INT16_FIELD(*this, kBytecodeAgeOffset,
                            static_cast<int16_t>(age));
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

BytecodeOffset DeoptimizationData::GetBytecodeOffset(int i) {
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
  // exception is currently running Code: in that case, the deoptimization
  // literals array might be the only thing keeping the target object alive.
  // Thus, when a Code is running, we strongly mark all of its deoptimization
  // literals.
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_INL_H_
