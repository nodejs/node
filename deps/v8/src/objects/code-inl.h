// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_INL_H_
#define V8_OBJECTS_CODE_INL_H_

#include "src/objects/code.h"

#include "src/base/memory.h"
#include "src/codegen/code-desc.h"
#include "src/execution/isolate.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/dictionary.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(DeoptimizationData, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(BytecodeArray, FixedArrayBase)
OBJECT_CONSTRUCTORS_IMPL(AbstractCode, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(DependentCode, WeakFixedArray)
OBJECT_CONSTRUCTORS_IMPL(CodeDataContainer, HeapObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(SourcePositionTableWithFrameCache)

NEVER_READ_ONLY_SPACE_IMPL(AbstractCode)

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(BytecodeArray)
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

ByteArray AbstractCode::source_position_table() {
  if (IsCode()) {
    return GetCode().SourcePositionTable();
  } else {
    return GetBytecodeArray().SourcePositionTable();
  }
}

Object AbstractCode::stack_frame_cache() {
  Object maybe_table;
  if (IsCode()) {
    maybe_table = GetCode().source_position_table();
  } else {
    maybe_table = GetBytecodeArray().source_position_table();
  }
  if (maybe_table.IsSourcePositionTableWithFrameCache()) {
    return SourcePositionTableWithFrameCache::cast(maybe_table)
        .stack_frame_cache();
  }
  return Smi::zero();
}

int AbstractCode::SizeIncludingMetadata() {
  if (IsCode()) {
    return GetCode().SizeIncludingMetadata();
  } else {
    return GetBytecodeArray().SizeIncludingMetadata();
  }
}
int AbstractCode::ExecutableSize() {
  if (IsCode()) {
    return GetCode().ExecutableSize();
  } else {
    return GetBytecodeArray().BytecodeArraySize();
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

AbstractCode::Kind AbstractCode::kind() {
  if (IsCode()) {
    return static_cast<AbstractCode::Kind>(GetCode().kind());
  } else {
    return INTERPRETED_FUNCTION;
  }
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
INT_ACCESSORS(Code, safepoint_table_offset, kSafepointTableOffsetOffset)
INT_ACCESSORS(Code, handler_table_offset, kHandlerTableOffsetOffset)
INT_ACCESSORS(Code, code_comments_offset, kCodeCommentsOffsetOffset)
#define CODE_ACCESSORS(name, type, offset)           \
  ACCESSORS_CHECKED2(Code, name, type, offset, true, \
                     !ObjectInYoungGeneration(value))
#define SYNCHRONIZED_CODE_ACCESSORS(name, type, offset)           \
  SYNCHRONIZED_ACCESSORS_CHECKED2(Code, name, type, offset, true, \
                                  !ObjectInYoungGeneration(value))

CODE_ACCESSORS(relocation_info, ByteArray, kRelocationInfoOffset)
CODE_ACCESSORS(deoptimization_data, FixedArray, kDeoptimizationDataOffset)
CODE_ACCESSORS(source_position_table, Object, kSourcePositionTableOffset)
// Concurrent marker needs to access kind specific flags in code data container.
SYNCHRONIZED_CODE_ACCESSORS(code_data_container, CodeDataContainer,
                            kCodeDataContainerOffset)
#undef CODE_ACCESSORS
#undef SYNCHRONIZED_CODE_ACCESSORS

void Code::WipeOutHeader() {
  WRITE_FIELD(*this, kRelocationInfoOffset, Smi::FromInt(0));
  WRITE_FIELD(*this, kDeoptimizationDataOffset, Smi::FromInt(0));
  WRITE_FIELD(*this, kSourcePositionTableOffset, Smi::FromInt(0));
  WRITE_FIELD(*this, kCodeDataContainerOffset, Smi::FromInt(0));
}

void Code::clear_padding() {
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }
  Address data_end =
      has_unwinding_info() ? unwinding_info_end() : raw_instruction_end();
  memset(reinterpret_cast<void*>(data_end), 0,
         CodeSize() - (data_end - address()));
}

ByteArray Code::SourcePositionTableIfCollected() const {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  Object maybe_table = source_position_table();
  if (maybe_table.IsUndefined(roots) || maybe_table.IsException(roots))
    return roots.empty_byte_array();
  return SourcePositionTable();
}

ByteArray Code::SourcePositionTable() const {
  Object maybe_table = source_position_table();
  DCHECK(!maybe_table.IsUndefined() && !maybe_table.IsException());
  if (maybe_table.IsByteArray()) return ByteArray::cast(maybe_table);
  DCHECK(maybe_table.IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      .source_position_table();
}

Object Code::next_code_link() const {
  return code_data_container().next_code_link();
}

void Code::set_next_code_link(Object value) {
  code_data_container().set_next_code_link(value);
}

int Code::InstructionSize() const {
  if (is_off_heap_trampoline()) return OffHeapInstructionSize();
  return raw_instruction_size();
}

Address Code::raw_instruction_start() const {
  return FIELD_ADDR(*this, kHeaderSize);
}

Address Code::InstructionStart() const {
  if (is_off_heap_trampoline()) return OffHeapInstructionStart();
  return raw_instruction_start();
}

Address Code::raw_instruction_end() const {
  return raw_instruction_start() + raw_instruction_size();
}

Address Code::InstructionEnd() const {
  if (is_off_heap_trampoline()) return OffHeapInstructionEnd();
  return raw_instruction_end();
}

int Code::GetUnwindingInfoSizeOffset() const {
  DCHECK(has_unwinding_info());
  return RoundUp(kHeaderSize + raw_instruction_size(), kInt64Size);
}

int Code::unwinding_info_size() const {
  DCHECK(has_unwinding_info());
  return static_cast<int>(ReadField<uint64_t>(GetUnwindingInfoSizeOffset()));
}

void Code::set_unwinding_info_size(int value) {
  DCHECK(has_unwinding_info());
  WriteField<uint64_t>(GetUnwindingInfoSizeOffset(), value);
}

Address Code::unwinding_info_start() const {
  DCHECK(has_unwinding_info());
  return FIELD_ADDR(*this, GetUnwindingInfoSizeOffset()) + kInt64Size;
}

Address Code::unwinding_info_end() const {
  DCHECK(has_unwinding_info());
  return unwinding_info_start() + unwinding_info_size();
}

int Code::body_size() const {
  int unpadded_body_size =
      has_unwinding_info()
          ? static_cast<int>(unwinding_info_end() - raw_instruction_start())
          : raw_instruction_size();
  return RoundUp(unpadded_body_size, kObjectAlignment);
}

int Code::SizeIncludingMetadata() const {
  int size = CodeSize();
  size += relocation_info().Size();
  size += deoptimization_data().Size();
  return size;
}

ByteArray Code::unchecked_relocation_info() const {
  const Isolate* isolate = GetIsolateForPtrCompr(*this);
  return ByteArray::unchecked_cast(
      TaggedField<HeapObject, kRelocationInfoOffset>::load(isolate, *this));
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

bool Code::contains(Address inner_pointer) {
  if (is_off_heap_trampoline()) {
    if (OffHeapInstructionStart() <= inner_pointer &&
        inner_pointer < OffHeapInstructionEnd()) {
      return true;
    }
  }
  return (address() <= inner_pointer) && (inner_pointer < address() + Size());
}

int Code::ExecutableSize() const {
  // Check that the assumptions about the layout of the code object holds.
  DCHECK_EQ(static_cast<int>(raw_instruction_start() - address()),
            Code::kHeaderSize);
  return raw_instruction_size() + Code::kHeaderSize;
}

// static
void Code::CopyRelocInfoToByteArray(ByteArray dest, const CodeDesc& desc) {
  DCHECK_EQ(dest.length(), desc.reloc_size);
  CopyBytes(dest.GetDataStartAddress(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));
}

int Code::CodeSize() const { return SizeFor(body_size()); }

Code::Kind Code::kind() const {
  return KindField::decode(ReadField<uint32_t>(kFlagsOffset));
}

void Code::initialize_flags(Kind kind, bool has_unwinding_info,
                            bool is_turbofanned, int stack_slots,
                            bool is_off_heap_trampoline) {
  CHECK(0 <= stack_slots && stack_slots < StackSlotsField::kMax);
  static_assert(Code::NUMBER_OF_KINDS <= KindField::kMax + 1, "field overflow");
  uint32_t flags = HasUnwindingInfoField::encode(has_unwinding_info) |
                   KindField::encode(kind) |
                   IsTurbofannedField::encode(is_turbofanned) |
                   StackSlotsField::encode(stack_slots) |
                   IsOffHeapTrampoline::encode(is_off_heap_trampoline);
  WriteField<uint32_t>(kFlagsOffset, flags);
  DCHECK_IMPLIES(stack_slots != 0, has_safepoint_info());
}

inline bool Code::is_interpreter_trampoline_builtin() const {
  bool is_interpreter_trampoline =
      (builtin_index() == Builtins::kInterpreterEntryTrampoline ||
       builtin_index() == Builtins::kInterpreterEnterBytecodeAdvance ||
       builtin_index() == Builtins::kInterpreterEnterBytecodeDispatch);
  return is_interpreter_trampoline;
}

inline bool Code::checks_optimization_marker() const {
  bool checks_marker =
      (builtin_index() == Builtins::kCompileLazy ||
       builtin_index() == Builtins::kInterpreterEntryTrampoline);
  return checks_marker ||
         (kind() == OPTIMIZED_FUNCTION && marked_for_deoptimization());
}

inline bool Code::has_tagged_params() const {
  return kind() != JS_TO_WASM_FUNCTION && kind() != C_WASM_ENTRY &&
         kind() != WASM_FUNCTION;
}

inline bool Code::has_unwinding_info() const {
  return HasUnwindingInfoField::decode(ReadField<uint32_t>(kFlagsOffset));
}

inline bool Code::is_turbofanned() const {
  return IsTurbofannedField::decode(ReadField<uint32_t>(kFlagsOffset));
}

inline bool Code::can_have_weak_objects() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int32_t flags = code_data_container().kind_specific_flags();
  return CanHaveWeakObjectsField::decode(flags);
}

inline void Code::set_can_have_weak_objects(bool value) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = CanHaveWeakObjectsField::update(previous, value);
  code_data_container().set_kind_specific_flags(updated);
}

inline bool Code::is_promise_rejection() const {
  DCHECK(kind() == BUILTIN);
  int32_t flags = code_data_container().kind_specific_flags();
  return IsPromiseRejectionField::decode(flags);
}

inline void Code::set_is_promise_rejection(bool value) {
  DCHECK(kind() == BUILTIN);
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = IsPromiseRejectionField::update(previous, value);
  code_data_container().set_kind_specific_flags(updated);
}

inline bool Code::is_exception_caught() const {
  DCHECK(kind() == BUILTIN);
  int32_t flags = code_data_container().kind_specific_flags();
  return IsExceptionCaughtField::decode(flags);
}

inline void Code::set_is_exception_caught(bool value) {
  DCHECK(kind() == BUILTIN);
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = IsExceptionCaughtField::update(previous, value);
  code_data_container().set_kind_specific_flags(updated);
}

inline bool Code::is_off_heap_trampoline() const {
  return IsOffHeapTrampoline::decode(ReadField<uint32_t>(kFlagsOffset));
}

inline HandlerTable::CatchPrediction Code::GetBuiltinCatchPrediction() {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
  if (is_exception_caught()) return HandlerTable::CAUGHT;
  return HandlerTable::UNCAUGHT;
}

int Code::builtin_index() const {
  int index = ReadField<int>(kBuiltinIndexOffset);
  DCHECK(index == -1 || Builtins::IsBuiltinId(index));
  return index;
}

void Code::set_builtin_index(int index) {
  DCHECK(index == -1 || Builtins::IsBuiltinId(index));
  WriteField<int>(kBuiltinIndexOffset, index);
}

bool Code::is_builtin() const { return builtin_index() != -1; }

bool Code::has_safepoint_info() const {
  return is_turbofanned() || is_wasm_code();
}

int Code::stack_slots() const {
  DCHECK(has_safepoint_info());
  return StackSlotsField::decode(ReadField<uint32_t>(kFlagsOffset));
}

bool Code::marked_for_deoptimization() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int32_t flags = code_data_container().kind_specific_flags();
  return MarkedForDeoptimizationField::decode(flags);
}

void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = MarkedForDeoptimizationField::update(previous, flag);
  code_data_container().set_kind_specific_flags(updated);
}

bool Code::embedded_objects_cleared() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int32_t flags = code_data_container().kind_specific_flags();
  return EmbeddedObjectsClearedField::decode(flags);
}

void Code::set_embedded_objects_cleared(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, marked_for_deoptimization());
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = EmbeddedObjectsClearedField::update(previous, flag);
  code_data_container().set_kind_specific_flags(updated);
}

bool Code::deopt_already_counted() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int32_t flags = code_data_container().kind_specific_flags();
  return DeoptAlreadyCountedField::decode(flags);
}

void Code::set_deopt_already_counted(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int32_t previous = code_data_container().kind_specific_flags();
  int32_t updated = DeoptAlreadyCountedField::update(previous, flag);
  code_data_container().set_kind_specific_flags(updated);
}

bool Code::is_optimized_code() const { return kind() == OPTIMIZED_FUNCTION; }
bool Code::is_wasm_code() const { return kind() == WASM_FUNCTION; }

int Code::constant_pool_offset() const {
  if (!FLAG_enable_embedded_constant_pool) return code_comments_offset();
  return ReadField<int>(kConstantPoolOffsetOffset);
}

void Code::set_constant_pool_offset(int value) {
  if (!FLAG_enable_embedded_constant_pool) return;
  DCHECK_LE(value, InstructionSize());
  WriteField<int>(kConstantPoolOffsetOffset, value);
}

Address Code::constant_pool() const {
  if (!has_constant_pool()) return kNullAddress;
  return InstructionStart() + constant_pool_offset();
}

Address Code::code_comments() const {
  return InstructionStart() + code_comments_offset();
}

Code Code::GetCodeFromTargetAddress(Address address) {
  {
    // TODO(jgruber,v8:6666): Support embedded builtins here. We'd need to pass
    // in the current isolate.
    Address start = reinterpret_cast<Address>(Isolate::CurrentEmbeddedBlob());
    Address end = start + Isolate::CurrentEmbeddedBlobSize();
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
  Map map = object.synchronized_map();
  InstanceType instance_type = map.instance_type();
  if (InstanceTypeChecker::IsMap(instance_type)) {
    return Map::cast(object).CanTransition();
  }
  return InstanceTypeChecker::IsPropertyCell(instance_type) ||
         InstanceTypeChecker::IsJSReceiver(instance_type) ||
         InstanceTypeChecker::IsContext(instance_type);
}

bool Code::IsExecutable() {
  return !Builtins::IsBuiltinId(builtin_index()) || !is_off_heap_trampoline() ||
         Builtins::CodeObjectIsExecutable(builtin_index());
}

// This field has to have relaxed atomic accessors because it is accessed in the
// concurrent marker.
RELAXED_INT32_ACCESSORS(CodeDataContainer, kind_specific_flags,
                        kKindSpecificFlagsOffset)
ACCESSORS(CodeDataContainer, next_code_link, Object, kNextCodeLinkOffset)

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
  return ReadField<int8_t>(kOsrNestingLevelOffset);
}

void BytecodeArray::set_osr_loop_nesting_level(int depth) {
  DCHECK(0 <= depth && depth <= AbstractCode::kMaxLoopNestingMarker);
  STATIC_ASSERT(AbstractCode::kMaxLoopNestingMarker < kMaxInt8);
  WriteField<int8_t>(kOsrNestingLevelOffset, depth);
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

ACCESSORS(BytecodeArray, constant_pool, FixedArray, kConstantPoolOffset)
ACCESSORS(BytecodeArray, handler_table, ByteArray, kHandlerTableOffset)
ACCESSORS(BytecodeArray, source_position_table, Object,
          kSourcePositionTableOffset)

void BytecodeArray::clear_padding() {
  int data_size = kHeaderSize + length();
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

Address BytecodeArray::GetFirstBytecodeAddress() {
  return ptr() - kHeapObjectTag + kHeaderSize;
}

bool BytecodeArray::HasSourcePositionTable() const {
  Object maybe_table = source_position_table();
  return !(maybe_table.IsUndefined() || DidSourcePositionGenerationFail());
}

bool BytecodeArray::DidSourcePositionGenerationFail() const {
  return source_position_table().IsException();
}

void BytecodeArray::SetSourcePositionsFailedToCollect() {
  set_source_position_table(GetReadOnlyRoots().exception());
}

ByteArray BytecodeArray::SourcePositionTable() const {
  Object maybe_table = source_position_table();
  if (maybe_table.IsByteArray()) return ByteArray::cast(maybe_table);
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (maybe_table.IsException(roots)) return roots.empty_byte_array();

  DCHECK(!maybe_table.IsUndefined(roots));
  DCHECK(maybe_table.IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      .source_position_table();
}

ByteArray BytecodeArray::SourcePositionTableIfCollected() const {
  if (!HasSourcePositionTable()) return GetReadOnlyRoots().empty_byte_array();

  return SourcePositionTable();
}

void BytecodeArray::ClearFrameCacheFromSourcePositionTable() {
  Object maybe_table = source_position_table();
  if (maybe_table.IsUndefined() || maybe_table.IsByteArray() ||
      maybe_table.IsException())
    return;
  DCHECK(maybe_table.IsSourcePositionTableWithFrameCache());
  set_source_position_table(SourcePositionTableWithFrameCache::cast(maybe_table)
                                .source_position_table());
}

int BytecodeArray::BytecodeArraySize() { return SizeFor(this->length()); }

int BytecodeArray::SizeIncludingMetadata() {
  int size = BytecodeArraySize();
  size += constant_pool().Size();
  size += handler_table().Size();
  if (HasSourcePositionTable()) {
    size += SourcePositionTable().Size();
  }
  return size;
}

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
DEFINE_DEOPT_ELEMENT_ACCESSORS(DeoptExitStart, Smi)

DEFINE_DEOPT_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)

BailoutId DeoptimizationData::BytecodeOffset(int i) {
  return BailoutId(BytecodeOffsetRaw(i).value());
}

void DeoptimizationData::SetBytecodeOffset(int i, BailoutId value) {
  SetBytecodeOffsetRaw(i, Smi::FromInt(value.ToInt()));
}

int DeoptimizationData::DeoptCount() {
  return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_INL_H_
