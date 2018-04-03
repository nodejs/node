// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_INL_H_
#define V8_OBJECTS_CODE_INL_H_

#include "src/objects/code.h"

#include "src/objects/dictionary.h"
#include "src/v8memory.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TYPE_CHECKER(BytecodeArray, BYTECODE_ARRAY_TYPE)
TYPE_CHECKER(Code, CODE_TYPE)
TYPE_CHECKER(CodeDataContainer, CODE_DATA_CONTAINER_TYPE)

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(BytecodeArray)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(CodeDataContainer)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(DeoptimizationData)
CAST_ACCESSOR(HandlerTable)

int AbstractCode::instruction_size() {
  if (IsCode()) {
    return GetCode()->instruction_size();
  } else {
    return GetBytecodeArray()->length();
  }
}

ByteArray* AbstractCode::source_position_table() {
  if (IsCode()) {
    return GetCode()->SourcePositionTable();
  } else {
    return GetBytecodeArray()->SourcePositionTable();
  }
}

Object* AbstractCode::stack_frame_cache() {
  Object* maybe_table;
  if (IsCode()) {
    maybe_table = GetCode()->source_position_table();
  } else {
    maybe_table = GetBytecodeArray()->source_position_table();
  }
  if (maybe_table->IsSourcePositionTableWithFrameCache()) {
    return SourcePositionTableWithFrameCache::cast(maybe_table)
        ->stack_frame_cache();
  }
  return Smi::kZero;
}

int AbstractCode::SizeIncludingMetadata() {
  if (IsCode()) {
    return GetCode()->SizeIncludingMetadata();
  } else {
    return GetBytecodeArray()->SizeIncludingMetadata();
  }
}
int AbstractCode::ExecutableSize() {
  if (IsCode()) {
    return GetCode()->ExecutableSize();
  } else {
    return GetBytecodeArray()->BytecodeArraySize();
  }
}

Address AbstractCode::instruction_start() {
  if (IsCode()) {
    return GetCode()->instruction_start();
  } else {
    return GetBytecodeArray()->GetFirstBytecodeAddress();
  }
}

Address AbstractCode::instruction_end() {
  if (IsCode()) {
    return GetCode()->instruction_end();
  } else {
    return GetBytecodeArray()->GetFirstBytecodeAddress() +
           GetBytecodeArray()->length();
  }
}

bool AbstractCode::contains(byte* inner_pointer) {
  return (address() <= inner_pointer) && (inner_pointer <= address() + Size());
}

AbstractCode::Kind AbstractCode::kind() {
  if (IsCode()) {
    return static_cast<AbstractCode::Kind>(GetCode()->kind());
  } else {
    return INTERPRETED_FUNCTION;
  }
}

Code* AbstractCode::GetCode() { return Code::cast(this); }

BytecodeArray* AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(this);
}

DependentCode* DependentCode::next_link() {
  return DependentCode::cast(get(kNextLinkIndex));
}

void DependentCode::set_next_link(DependentCode* next) {
  set(kNextLinkIndex, next);
}

int DependentCode::flags() { return Smi::ToInt(get(kFlagsIndex)); }

void DependentCode::set_flags(int flags) {
  set(kFlagsIndex, Smi::FromInt(flags));
}

int DependentCode::count() { return CountField::decode(flags()); }

void DependentCode::set_count(int value) {
  set_flags(CountField::update(flags(), value));
}

DependentCode::DependencyGroup DependentCode::group() {
  return static_cast<DependencyGroup>(GroupField::decode(flags()));
}

void DependentCode::set_group(DependentCode::DependencyGroup group) {
  set_flags(GroupField::update(flags(), static_cast<int>(group)));
}

void DependentCode::set_object_at(int i, Object* object) {
  set(kCodesStartIndex + i, object);
}

Object* DependentCode::object_at(int i) { return get(kCodesStartIndex + i); }

void DependentCode::clear_at(int i) { set_undefined(kCodesStartIndex + i); }

void DependentCode::copy(int from, int to) {
  set(kCodesStartIndex + to, get(kCodesStartIndex + from));
}

INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, constant_pool_offset, kConstantPoolOffset)
#define CODE_ACCESSORS(name, type, offset)           \
  ACCESSORS_CHECKED2(Code, name, type, offset, true, \
                     !GetHeap()->InNewSpace(value))
CODE_ACCESSORS(relocation_info, ByteArray, kRelocationInfoOffset)
CODE_ACCESSORS(handler_table, FixedArray, kHandlerTableOffset)
CODE_ACCESSORS(deoptimization_data, FixedArray, kDeoptimizationDataOffset)
CODE_ACCESSORS(source_position_table, Object, kSourcePositionTableOffset)
CODE_ACCESSORS(protected_instructions, FixedArray, kProtectedInstructionsOffset)
CODE_ACCESSORS(code_data_container, CodeDataContainer, kCodeDataContainerOffset)
CODE_ACCESSORS(trap_handler_index, Smi, kTrapHandlerIndex)
#undef CODE_ACCESSORS

void Code::WipeOutHeader() {
  WRITE_FIELD(this, kRelocationInfoOffset, nullptr);
  WRITE_FIELD(this, kHandlerTableOffset, nullptr);
  WRITE_FIELD(this, kDeoptimizationDataOffset, nullptr);
  WRITE_FIELD(this, kSourcePositionTableOffset, nullptr);
  WRITE_FIELD(this, kProtectedInstructionsOffset, nullptr);
  WRITE_FIELD(this, kCodeDataContainerOffset, nullptr);
}

void Code::clear_padding() {
  memset(address() + kHeaderPaddingStart, 0, kHeaderSize - kHeaderPaddingStart);
  Address data_end =
      has_unwinding_info() ? unwinding_info_end() : instruction_end();
  memset(data_end, 0, CodeSize() - (data_end - address()));
}

ByteArray* Code::SourcePositionTable() const {
  Object* maybe_table = source_position_table();
  if (maybe_table->IsByteArray()) return ByteArray::cast(maybe_table);
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      ->source_position_table();
}

uint32_t Code::stub_key() const {
  DCHECK(is_stub());
  return READ_UINT32_FIELD(this, kStubKeyOffset);
}

void Code::set_stub_key(uint32_t key) {
  DCHECK(is_stub() || key == 0);  // Allow zero initialization.
  WRITE_UINT32_FIELD(this, kStubKeyOffset, key);
}

Object* Code::next_code_link() const {
  return code_data_container()->next_code_link();
}

void Code::set_next_code_link(Object* value) {
  code_data_container()->set_next_code_link(value);
}

byte* Code::instruction_start() const {
  return const_cast<byte*>(FIELD_ADDR_CONST(this, kHeaderSize));
}

byte* Code::instruction_end() const {
  return instruction_start() + instruction_size();
}

int Code::GetUnwindingInfoSizeOffset() const {
  DCHECK(has_unwinding_info());
  return RoundUp(kHeaderSize + instruction_size(), kInt64Size);
}

int Code::unwinding_info_size() const {
  DCHECK(has_unwinding_info());
  return static_cast<int>(
      READ_UINT64_FIELD(this, GetUnwindingInfoSizeOffset()));
}

void Code::set_unwinding_info_size(int value) {
  DCHECK(has_unwinding_info());
  WRITE_UINT64_FIELD(this, GetUnwindingInfoSizeOffset(), value);
}

byte* Code::unwinding_info_start() const {
  DCHECK(has_unwinding_info());
  return const_cast<byte*>(
             FIELD_ADDR_CONST(this, GetUnwindingInfoSizeOffset())) +
         kInt64Size;
}

byte* Code::unwinding_info_end() const {
  DCHECK(has_unwinding_info());
  return unwinding_info_start() + unwinding_info_size();
}

int Code::body_size() const {
  int unpadded_body_size =
      has_unwinding_info()
          ? static_cast<int>(unwinding_info_end() - instruction_start())
          : instruction_size();
  return RoundUp(unpadded_body_size, kObjectAlignment);
}

int Code::SizeIncludingMetadata() const {
  int size = CodeSize();
  size += relocation_info()->Size();
  size += deoptimization_data()->Size();
  size += handler_table()->Size();
  size += protected_instructions()->Size();
  return size;
}

ByteArray* Code::unchecked_relocation_info() const {
  return reinterpret_cast<ByteArray*>(READ_FIELD(this, kRelocationInfoOffset));
}

byte* Code::relocation_start() const {
  return unchecked_relocation_info()->GetDataStartAddress();
}

int Code::relocation_size() const {
  return unchecked_relocation_info()->length();
}

byte* Code::entry() const { return instruction_start(); }

bool Code::contains(byte* inner_pointer) {
  return (address() <= inner_pointer) && (inner_pointer <= address() + Size());
}

int Code::ExecutableSize() const {
  // Check that the assumptions about the layout of the code object holds.
  DCHECK_EQ(static_cast<int>(instruction_start() - address()),
            Code::kHeaderSize);
  return instruction_size() + Code::kHeaderSize;
}

int Code::CodeSize() const { return SizeFor(body_size()); }

Code::Kind Code::kind() const {
  return KindField::decode(READ_UINT32_FIELD(this, kFlagsOffset));
}

void Code::initialize_flags(Kind kind, bool has_unwinding_info,
                            bool is_turbofanned, int stack_slots) {
  CHECK(0 <= stack_slots && stack_slots < StackSlotsField::kMax);
  static_assert(Code::NUMBER_OF_KINDS <= KindField::kMax + 1, "field overflow");
  uint32_t flags = HasUnwindingInfoField::encode(has_unwinding_info) |
                   KindField::encode(kind) |
                   IsTurbofannedField::encode(is_turbofanned) |
                   StackSlotsField::encode(stack_slots);
  WRITE_UINT32_FIELD(this, kFlagsOffset, flags);
  DCHECK_IMPLIES(stack_slots != 0, has_safepoint_info());
}

inline bool Code::is_interpreter_trampoline_builtin() const {
  Builtins* builtins = GetIsolate()->builtins();
  bool is_interpreter_trampoline =
      (this == builtins->builtin(Builtins::kInterpreterEntryTrampoline) ||
       this == builtins->builtin(Builtins::kInterpreterEnterBytecodeAdvance) ||
       this == builtins->builtin(Builtins::kInterpreterEnterBytecodeDispatch));
  DCHECK_IMPLIES(is_interpreter_trampoline, !Builtins::IsLazy(builtin_index()));
  return is_interpreter_trampoline;
}

inline bool Code::checks_optimization_marker() const {
  Builtins* builtins = GetIsolate()->builtins();
  bool checks_marker =
      (this == builtins->builtin(Builtins::kCompileLazy) ||
       this == builtins->builtin(Builtins::kInterpreterEntryTrampoline) ||
       this == builtins->builtin(Builtins::kCheckOptimizationMarker));
  DCHECK_IMPLIES(checks_marker, !Builtins::IsLazy(builtin_index()));
  return checks_marker ||
         (kind() == OPTIMIZED_FUNCTION && marked_for_deoptimization());
}

inline bool Code::has_unwinding_info() const {
  return HasUnwindingInfoField::decode(READ_UINT32_FIELD(this, kFlagsOffset));
}

inline bool Code::has_tagged_params() const {
  int flags = READ_UINT32_FIELD(this, kFlagsOffset);
  return HasTaggedStackField::decode(flags);
}

inline void Code::set_has_tagged_params(bool value) {
  int previous = READ_UINT32_FIELD(this, kFlagsOffset);
  int updated = HasTaggedStackField::update(previous, value);
  WRITE_UINT32_FIELD(this, kFlagsOffset, updated);
}

inline bool Code::is_turbofanned() const {
  return IsTurbofannedField::decode(READ_UINT32_FIELD(this, kFlagsOffset));
}

inline bool Code::can_have_weak_objects() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int flags = code_data_container()->kind_specific_flags();
  return CanHaveWeakObjectsField::decode(flags);
}

inline void Code::set_can_have_weak_objects(bool value) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int previous = code_data_container()->kind_specific_flags();
  int updated = CanHaveWeakObjectsField::update(previous, value);
  code_data_container()->set_kind_specific_flags(updated);
}

inline bool Code::is_construct_stub() const {
  DCHECK(kind() == BUILTIN);
  int flags = code_data_container()->kind_specific_flags();
  return IsConstructStubField::decode(flags);
}

inline void Code::set_is_construct_stub(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = code_data_container()->kind_specific_flags();
  int updated = IsConstructStubField::update(previous, value);
  code_data_container()->set_kind_specific_flags(updated);
}

inline bool Code::is_promise_rejection() const {
  DCHECK(kind() == BUILTIN);
  int flags = code_data_container()->kind_specific_flags();
  return IsPromiseRejectionField::decode(flags);
}

inline void Code::set_is_promise_rejection(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = code_data_container()->kind_specific_flags();
  int updated = IsPromiseRejectionField::update(previous, value);
  code_data_container()->set_kind_specific_flags(updated);
}

inline bool Code::is_exception_caught() const {
  DCHECK(kind() == BUILTIN);
  int flags = code_data_container()->kind_specific_flags();
  return IsExceptionCaughtField::decode(flags);
}

inline void Code::set_is_exception_caught(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = code_data_container()->kind_specific_flags();
  int updated = IsExceptionCaughtField::update(previous, value);
  code_data_container()->set_kind_specific_flags(updated);
}

inline HandlerTable::CatchPrediction Code::GetBuiltinCatchPrediction() {
  if (is_promise_rejection()) return HandlerTable::PROMISE;
  if (is_exception_caught()) return HandlerTable::CAUGHT;
  return HandlerTable::UNCAUGHT;
}

int Code::builtin_index() const {
  int index = READ_INT_FIELD(this, kBuiltinIndexOffset);
  DCHECK(index == -1 || Builtins::IsBuiltinId(index));
  return index;
}

void Code::set_builtin_index(int index) {
  DCHECK(index == -1 || Builtins::IsBuiltinId(index));
  WRITE_INT_FIELD(this, kBuiltinIndexOffset, index);
}

bool Code::is_builtin() const { return builtin_index() != -1; }

bool Code::has_safepoint_info() const {
  return is_turbofanned() || is_wasm_code();
}

int Code::stack_slots() const {
  DCHECK(has_safepoint_info());
  return StackSlotsField::decode(READ_UINT32_FIELD(this, kFlagsOffset));
}

int Code::safepoint_table_offset() const {
  DCHECK(has_safepoint_info());
  return READ_INT32_FIELD(this, kSafepointTableOffsetOffset);
}

void Code::set_safepoint_table_offset(int offset) {
  CHECK_LE(0, offset);
  DCHECK(has_safepoint_info() || offset == 0);  // Allow zero initialization.
  DCHECK(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  WRITE_INT32_FIELD(this, kSafepointTableOffsetOffset, offset);
}

bool Code::marked_for_deoptimization() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int flags = code_data_container()->kind_specific_flags();
  return MarkedForDeoptimizationField::decode(flags);
}

void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = code_data_container()->kind_specific_flags();
  int updated = MarkedForDeoptimizationField::update(previous, flag);
  code_data_container()->set_kind_specific_flags(updated);
}

bool Code::deopt_already_counted() const {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int flags = code_data_container()->kind_specific_flags();
  return DeoptAlreadyCountedField::decode(flags);
}

void Code::set_deopt_already_counted(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = code_data_container()->kind_specific_flags();
  int updated = DeoptAlreadyCountedField::update(previous, flag);
  code_data_container()->set_kind_specific_flags(updated);
}

bool Code::is_stub() const { return kind() == STUB; }
bool Code::is_optimized_code() const { return kind() == OPTIMIZED_FUNCTION; }
bool Code::is_wasm_code() const { return kind() == WASM_FUNCTION; }

Address Code::constant_pool() {
  Address constant_pool = nullptr;
  if (FLAG_enable_embedded_constant_pool) {
    int offset = constant_pool_offset();
    if (offset < instruction_size()) {
      constant_pool = FIELD_ADDR(this, kHeaderSize + offset);
    }
  }
  return constant_pool;
}

Code* Code::GetCodeFromTargetAddress(Address address) {
  HeapObject* code = HeapObject::FromAddress(address - Code::kHeaderSize);
  // GetCodeFromTargetAddress might be called when marking objects during mark
  // sweep. reinterpret_cast is therefore used instead of the more appropriate
  // Code::cast. Code::cast does not work when the object's map is
  // marked.
  Code* result = reinterpret_cast<Code*>(code);
  return result;
}

Object* Code::GetObjectFromCodeEntry(Address code_entry) {
  return HeapObject::FromAddress(code_entry - Code::kHeaderSize);
}

Object* Code::GetObjectFromEntryAddress(Address location_of_address) {
  return GetObjectFromCodeEntry(Memory::Address_at(location_of_address));
}

bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}

bool Code::IsWeakObject(Object* object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}

bool Code::IsWeakObjectInOptimizedCode(Object* object) {
  if (object->IsMap()) {
    return Map::cast(object)->CanTransition();
  }
  if (object->IsCell()) {
    object = Cell::cast(object)->value();
  } else if (object->IsPropertyCell()) {
    object = PropertyCell::cast(object)->value();
  }
  if (object->IsJSReceiver() || object->IsContext()) {
    return true;
  }
  return false;
}

INT_ACCESSORS(CodeDataContainer, kind_specific_flags, kKindSpecificFlagsOffset)
ACCESSORS(CodeDataContainer, next_code_link, Object, kNextCodeLinkOffset)

void CodeDataContainer::clear_padding() {
  memset(address() + kUnalignedSize, 0, kSize - kUnalignedSize);
}

byte BytecodeArray::get(int index) {
  DCHECK(index >= 0 && index < this->length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}

void BytecodeArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize, value);
}

void BytecodeArray::set_frame_size(int frame_size) {
  DCHECK_GE(frame_size, 0);
  DCHECK(IsAligned(frame_size, static_cast<unsigned>(kPointerSize)));
  WRITE_INT_FIELD(this, kFrameSizeOffset, frame_size);
}

int BytecodeArray::frame_size() const {
  return READ_INT_FIELD(this, kFrameSizeOffset);
}

int BytecodeArray::register_count() const {
  return frame_size() / kPointerSize;
}

void BytecodeArray::set_parameter_count(int number_of_parameters) {
  DCHECK_GE(number_of_parameters, 0);
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  WRITE_INT_FIELD(this, kParameterSizeOffset,
                  (number_of_parameters << kPointerSizeLog2));
}

interpreter::Register BytecodeArray::incoming_new_target_or_generator_register()
    const {
  int register_operand =
      READ_INT_FIELD(this, kIncomingNewTargetOrGeneratorRegisterOffset);
  if (register_operand == 0) {
    return interpreter::Register::invalid_value();
  } else {
    return interpreter::Register::FromOperand(register_operand);
  }
}

void BytecodeArray::set_incoming_new_target_or_generator_register(
    interpreter::Register incoming_new_target_or_generator_register) {
  if (!incoming_new_target_or_generator_register.is_valid()) {
    WRITE_INT_FIELD(this, kIncomingNewTargetOrGeneratorRegisterOffset, 0);
  } else {
    DCHECK(incoming_new_target_or_generator_register.index() <
           register_count());
    DCHECK_NE(0, incoming_new_target_or_generator_register.ToOperand());
    WRITE_INT_FIELD(this, kIncomingNewTargetOrGeneratorRegisterOffset,
                    incoming_new_target_or_generator_register.ToOperand());
  }
}

int BytecodeArray::interrupt_budget() const {
  return READ_INT_FIELD(this, kInterruptBudgetOffset);
}

void BytecodeArray::set_interrupt_budget(int interrupt_budget) {
  DCHECK_GE(interrupt_budget, 0);
  WRITE_INT_FIELD(this, kInterruptBudgetOffset, interrupt_budget);
}

int BytecodeArray::osr_loop_nesting_level() const {
  return READ_INT8_FIELD(this, kOSRNestingLevelOffset);
}

void BytecodeArray::set_osr_loop_nesting_level(int depth) {
  DCHECK(0 <= depth && depth <= AbstractCode::kMaxLoopNestingMarker);
  STATIC_ASSERT(AbstractCode::kMaxLoopNestingMarker < kMaxInt8);
  WRITE_INT8_FIELD(this, kOSRNestingLevelOffset, depth);
}

BytecodeArray::Age BytecodeArray::bytecode_age() const {
  // Bytecode is aged by the concurrent marker.
  return static_cast<Age>(RELAXED_READ_INT8_FIELD(this, kBytecodeAgeOffset));
}

void BytecodeArray::set_bytecode_age(BytecodeArray::Age age) {
  DCHECK_GE(age, kFirstBytecodeAge);
  DCHECK_LE(age, kLastBytecodeAge);
  STATIC_ASSERT(kLastBytecodeAge <= kMaxInt8);
  // Bytecode is aged by the concurrent marker.
  RELAXED_WRITE_INT8_FIELD(this, kBytecodeAgeOffset, static_cast<int8_t>(age));
}

int BytecodeArray::parameter_count() const {
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  return READ_INT_FIELD(this, kParameterSizeOffset) >> kPointerSizeLog2;
}

ACCESSORS(BytecodeArray, constant_pool, FixedArray, kConstantPoolOffset)
ACCESSORS(BytecodeArray, handler_table, FixedArray, kHandlerTableOffset)
ACCESSORS(BytecodeArray, source_position_table, Object,
          kSourcePositionTableOffset)

void BytecodeArray::clear_padding() {
  int data_size = kHeaderSize + length();
  memset(address() + data_size, 0, SizeFor(length()) - data_size);
}

Address BytecodeArray::GetFirstBytecodeAddress() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag + kHeaderSize;
}

ByteArray* BytecodeArray::SourcePositionTable() {
  Object* maybe_table = source_position_table();
  if (maybe_table->IsByteArray()) return ByteArray::cast(maybe_table);
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      ->source_position_table();
}

void BytecodeArray::ClearFrameCacheFromSourcePositionTable() {
  Object* maybe_table = source_position_table();
  if (maybe_table->IsByteArray()) return;
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  set_source_position_table(SourcePositionTableWithFrameCache::cast(maybe_table)
                                ->source_position_table());
}

int BytecodeArray::BytecodeArraySize() { return SizeFor(this->length()); }

int BytecodeArray::SizeIncludingMetadata() {
  int size = BytecodeArraySize();
  size += constant_pool()->Size();
  size += handler_table()->Size();
  size += SourcePositionTable()->Size();
  return size;
}

int HandlerTable::GetRangeStart(int index) const {
  return Smi::ToInt(get(index * kRangeEntrySize + kRangeStartIndex));
}

int HandlerTable::GetRangeEnd(int index) const {
  return Smi::ToInt(get(index * kRangeEntrySize + kRangeEndIndex));
}

int HandlerTable::GetRangeHandler(int index) const {
  return HandlerOffsetField::decode(
      Smi::ToInt(get(index * kRangeEntrySize + kRangeHandlerIndex)));
}

int HandlerTable::GetRangeData(int index) const {
  return Smi::ToInt(get(index * kRangeEntrySize + kRangeDataIndex));
}

void HandlerTable::SetRangeStart(int index, int value) {
  set(index * kRangeEntrySize + kRangeStartIndex, Smi::FromInt(value));
}

void HandlerTable::SetRangeEnd(int index, int value) {
  set(index * kRangeEntrySize + kRangeEndIndex, Smi::FromInt(value));
}

void HandlerTable::SetRangeHandler(int index, int offset,
                                   CatchPrediction prediction) {
  int value = HandlerOffsetField::encode(offset) |
              HandlerPredictionField::encode(prediction);
  set(index * kRangeEntrySize + kRangeHandlerIndex, Smi::FromInt(value));
}

void HandlerTable::SetRangeData(int index, int value) {
  set(index * kRangeEntrySize + kRangeDataIndex, Smi::FromInt(value));
}

void HandlerTable::SetReturnOffset(int index, int value) {
  set(index * kReturnEntrySize + kReturnOffsetIndex, Smi::FromInt(value));
}

void HandlerTable::SetReturnHandler(int index, int offset) {
  int value = HandlerOffsetField::encode(offset);
  set(index * kReturnEntrySize + kReturnHandlerIndex, Smi::FromInt(value));
}

int HandlerTable::NumberOfRangeEntries() const {
  return length() / kRangeEntrySize;
}

BailoutId DeoptimizationData::BytecodeOffset(int i) {
  return BailoutId(BytecodeOffsetRaw(i)->value());
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
