// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory-base.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/execution/off-thread-isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/off-thread-factory-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/logging/log.h"
#include "src/logging/off-thread-logger.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/source-text-module.h"
#include "src/objects/template-objects-inl.h"

namespace v8 {
namespace internal {

template <typename Impl>
template <AllocationType allocation>
Handle<HeapNumber> FactoryBase<Impl>::NewHeapNumber() {
  STATIC_ASSERT(HeapNumber::kSize <= kMaxRegularHeapObjectSize);
  Map map = read_only_roots().heap_number_map();
  HeapObject result = AllocateRawWithImmortalMap(HeapNumber::kSize, allocation,
                                                 map, kDoubleUnaligned);
  return handle(HeapNumber::cast(result), isolate());
}

template V8_EXPORT_PRIVATE Handle<HeapNumber>
FactoryBase<Factory>::NewHeapNumber<AllocationType::kYoung>();
template V8_EXPORT_PRIVATE Handle<HeapNumber>
FactoryBase<Factory>::NewHeapNumber<AllocationType::kOld>();
template V8_EXPORT_PRIVATE Handle<HeapNumber>
FactoryBase<Factory>::NewHeapNumber<AllocationType::kReadOnly>();

template V8_EXPORT_PRIVATE Handle<HeapNumber>
FactoryBase<OffThreadFactory>::NewHeapNumber<AllocationType::kOld>();

template <typename Impl>
Handle<Struct> FactoryBase<Impl>::NewStruct(InstanceType type,
                                            AllocationType allocation) {
  Map map = Map::GetInstanceTypeMap(read_only_roots(), type);
  int size = map.instance_size();
  HeapObject result = AllocateRawWithImmortalMap(size, allocation, map);
  Handle<Struct> str = handle(Struct::cast(result), isolate());
  str->InitializeBody(size);
  return str;
}

template <typename Impl>
Handle<AccessorPair> FactoryBase<Impl>::NewAccessorPair() {
  Handle<AccessorPair> accessors = Handle<AccessorPair>::cast(
      NewStruct(ACCESSOR_PAIR_TYPE, AllocationType::kOld));
  accessors->set_getter(read_only_roots().null_value(), SKIP_WRITE_BARRIER);
  accessors->set_setter(read_only_roots().null_value(), SKIP_WRITE_BARRIER);
  return accessors;
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArray(int length,
                                                    AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return impl()->empty_fixed_array();
  return NewFixedArrayWithFiller(
      read_only_roots().fixed_array_map_handle(), length,
      read_only_roots().undefined_value_handle(), allocation);
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArrayWithMap(
    Handle<Map> map, int length, AllocationType allocation) {
  // Zero-length case must be handled outside, where the knowledge about
  // the map is.
  DCHECK_LT(0, length);
  return NewFixedArrayWithFiller(
      map, length, read_only_roots().undefined_value_handle(), allocation);
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArrayWithHoles(
    int length, AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return impl()->empty_fixed_array();
  return NewFixedArrayWithFiller(
      read_only_roots().fixed_array_map_handle(), length,
      read_only_roots().the_hole_value_handle(), allocation);
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArrayWithFiller(
    Handle<Map> map, int length, Handle<Oddball> filler,
    AllocationType allocation) {
  HeapObject result = AllocateRawFixedArray(length, allocation);
  DCHECK(ReadOnlyHeap::Contains(*map));
  DCHECK(ReadOnlyHeap::Contains(*filler));
  result.set_map_after_allocation(*map, SKIP_WRITE_BARRIER);
  Handle<FixedArray> array = handle(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(array->data_start(), *filler, length);
  return array;
}

template <typename Impl>
Handle<FixedArrayBase> FactoryBase<Impl>::NewFixedDoubleArray(
    int length, AllocationType allocation) {
  if (length == 0) return impl()->empty_fixed_array();
  if (length < 0 || length > FixedDoubleArray::kMaxLength) {
    isolate()->FatalProcessOutOfHeapMemory("invalid array length");
  }
  int size = FixedDoubleArray::SizeFor(length);
  Map map = read_only_roots().fixed_double_array_map();
  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, map, kDoubleAligned);
  Handle<FixedDoubleArray> array =
      handle(FixedDoubleArray::cast(result), isolate());
  array->set_length(length);
  return array;
}

template <typename Impl>
Handle<WeakFixedArray> FactoryBase<Impl>::NewWeakFixedArrayWithMap(
    Map map, int length, AllocationType allocation) {
  // Zero-length case must be handled outside.
  DCHECK_LT(0, length);
  DCHECK(ReadOnlyHeap::Contains(map));

  HeapObject result =
      AllocateRawArray(WeakFixedArray::SizeFor(length), allocation);
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);

  Handle<WeakFixedArray> array =
      handle(WeakFixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(ObjectSlot(array->data_start()),
               read_only_roots().undefined_value(), length);

  return array;
}

template <typename Impl>
Handle<WeakFixedArray> FactoryBase<Impl>::NewWeakFixedArray(
    int length, AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return impl()->empty_weak_fixed_array();
  return NewWeakFixedArrayWithMap(read_only_roots().weak_fixed_array_map(),
                                  length, allocation);
}

template <typename Impl>
Handle<ByteArray> FactoryBase<Impl>::NewByteArray(int length,
                                                  AllocationType allocation) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    isolate()->FatalProcessOutOfHeapMemory("invalid array length");
  }
  int size = ByteArray::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().byte_array_map());
  Handle<ByteArray> array(ByteArray::cast(result), isolate());
  array->set_length(length);
  array->clear_padding();
  return array;
}

template <typename Impl>
Handle<BytecodeArray> FactoryBase<Impl>::NewBytecodeArray(
    int length, const byte* raw_bytecodes, int frame_size, int parameter_count,
    Handle<FixedArray> constant_pool) {
  if (length < 0 || length > BytecodeArray::kMaxLength) {
    isolate()->FatalProcessOutOfHeapMemory("invalid array length");
  }
  // Bytecode array is AllocationType::kOld, so constant pool array should be
  // too.
  DCHECK(!Heap::InYoungGeneration(*constant_pool));

  int size = BytecodeArray::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size, AllocationType::kOld, read_only_roots().bytecode_array_map());
  Handle<BytecodeArray> instance(BytecodeArray::cast(result), isolate());
  instance->set_length(length);
  instance->set_frame_size(frame_size);
  instance->set_parameter_count(parameter_count);
  instance->set_incoming_new_target_or_generator_register(
      interpreter::Register::invalid_value());
  instance->set_osr_loop_nesting_level(0);
  instance->set_bytecode_age(BytecodeArray::kNoAgeBytecodeAge);
  instance->set_constant_pool(*constant_pool);
  instance->set_handler_table(read_only_roots().empty_byte_array());
  instance->set_source_position_table(read_only_roots().undefined_value());
  CopyBytes(reinterpret_cast<byte*>(instance->GetFirstBytecodeAddress()),
            raw_bytecodes, length);
  instance->clear_padding();

  return instance;
}

template <typename Impl>
Handle<Script> FactoryBase<Impl>::NewScript(Handle<String> source) {
  return NewScriptWithId(source, isolate()->GetNextScriptId());
}

template <typename Impl>
Handle<Script> FactoryBase<Impl>::NewScriptWithId(Handle<String> source,
                                                  int script_id) {
  // Create and initialize script object.
  ReadOnlyRoots roots = read_only_roots();
  Handle<Script> script =
      Handle<Script>::cast(NewStruct(SCRIPT_TYPE, AllocationType::kOld));
  script->set_source(*source);
  script->set_name(roots.undefined_value());
  script->set_id(script_id);
  script->set_line_offset(0);
  script->set_column_offset(0);
  script->set_context_data(roots.undefined_value());
  script->set_type(Script::TYPE_NORMAL);
  script->set_line_ends(roots.undefined_value());
  script->set_eval_from_shared_or_wrapped_arguments(roots.undefined_value());
  script->set_eval_from_position(0);
  script->set_shared_function_infos(roots.empty_weak_fixed_array(),
                                    SKIP_WRITE_BARRIER);
  script->set_flags(0);
  script->set_host_defined_options(roots.empty_fixed_array());

  impl()->AddToScriptList(script);

  LOG(isolate(), ScriptEvent(Logger::ScriptEventType::kCreate, script_id));
  return script;
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::NewSharedFunctionInfoForLiteral(
    FunctionLiteral* literal, Handle<Script> script, bool is_toplevel) {
  FunctionKind kind = literal->kind();
  Handle<SharedFunctionInfo> shared =
      NewSharedFunctionInfo(literal->GetName(isolate()), MaybeHandle<Code>(),
                            Builtins::kCompileLazy, kind);
  SharedFunctionInfo::InitFromFunctionLiteral(isolate(), shared, literal,
                                              is_toplevel);
  shared->SetScript(read_only_roots(), *script, literal->function_literal_id(),
                    false);
  return shared;
}

template <typename Impl>
Handle<PreparseData> FactoryBase<Impl>::NewPreparseData(int data_length,
                                                        int children_length) {
  int size = PreparseData::SizeFor(data_length, children_length);
  Handle<PreparseData> result = handle(
      PreparseData::cast(AllocateRawWithImmortalMap(
          size, AllocationType::kOld, read_only_roots().preparse_data_map())),
      isolate());
  result->set_data_length(data_length);
  result->set_children_length(children_length);
  MemsetTagged(result->inner_data_start(), read_only_roots().null_value(),
               children_length);
  result->clear_padding();
  return result;
}

template <typename Impl>
Handle<UncompiledDataWithoutPreparseData>
FactoryBase<Impl>::NewUncompiledDataWithoutPreparseData(
    Handle<String> inferred_name, int32_t start_position,
    int32_t end_position) {
  Handle<UncompiledDataWithoutPreparseData> result = handle(
      UncompiledDataWithoutPreparseData::cast(NewWithImmortalMap(
          impl()->read_only_roots().uncompiled_data_without_preparse_data_map(),
          AllocationType::kOld)),
      isolate());

  result->Init(impl(), *inferred_name, start_position, end_position);
  return result;
}

template <typename Impl>
Handle<UncompiledDataWithPreparseData>
FactoryBase<Impl>::NewUncompiledDataWithPreparseData(
    Handle<String> inferred_name, int32_t start_position, int32_t end_position,
    Handle<PreparseData> preparse_data) {
  Handle<UncompiledDataWithPreparseData> result = handle(
      UncompiledDataWithPreparseData::cast(NewWithImmortalMap(
          impl()->read_only_roots().uncompiled_data_with_preparse_data_map(),
          AllocationType::kOld)),
      isolate());

  result->Init(impl(), *inferred_name, start_position, end_position,
               *preparse_data);

  return result;
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::NewSharedFunctionInfo(
    MaybeHandle<String> maybe_name, MaybeHandle<HeapObject> maybe_function_data,
    int maybe_builtin_index, FunctionKind kind) {
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo();

  // Function names are assumed to be flat elsewhere.
  Handle<String> shared_name;
  bool has_shared_name = maybe_name.ToHandle(&shared_name);
  if (has_shared_name) {
    DCHECK(shared_name->IsFlat());
    shared->set_name_or_scope_info(*shared_name);
  } else {
    DCHECK_EQ(shared->name_or_scope_info(),
              SharedFunctionInfo::kNoSharedNameSentinel);
  }

  Handle<HeapObject> function_data;
  if (maybe_function_data.ToHandle(&function_data)) {
    // If we pass function_data then we shouldn't pass a builtin index, and
    // the function_data should not be code with a builtin.
    DCHECK(!Builtins::IsBuiltinId(maybe_builtin_index));
    DCHECK_IMPLIES(function_data->IsCode(),
                   !Code::cast(*function_data).is_builtin());
    shared->set_function_data(*function_data);
  } else if (Builtins::IsBuiltinId(maybe_builtin_index)) {
    shared->set_builtin_id(maybe_builtin_index);
  } else {
    shared->set_builtin_id(Builtins::kIllegal);
  }

  shared->CalculateConstructAsBuiltin();
  shared->set_kind(kind);

#ifdef VERIFY_HEAP
  shared->SharedFunctionInfoVerify(isolate());
#endif  // VERIFY_HEAP
  return shared;
}

template <typename Impl>
Handle<ObjectBoilerplateDescription>
FactoryBase<Impl>::NewObjectBoilerplateDescription(int boilerplate,
                                                   int all_properties,
                                                   int index_keys,
                                                   bool has_seen_proto) {
  DCHECK_GE(boilerplate, 0);
  DCHECK_GE(all_properties, index_keys);
  DCHECK_GE(index_keys, 0);

  int backing_store_size =
      all_properties - index_keys - (has_seen_proto ? 1 : 0);
  DCHECK_GE(backing_store_size, 0);
  bool has_different_size_backing_store = boilerplate != backing_store_size;

  // Space for name and value for every boilerplate property + LiteralType flag.
  int size =
      2 * boilerplate + ObjectBoilerplateDescription::kDescriptionStartIndex;

  if (has_different_size_backing_store) {
    // An extra entry for the backing store size.
    size++;
  }

  Handle<ObjectBoilerplateDescription> description =
      Handle<ObjectBoilerplateDescription>::cast(NewFixedArrayWithMap(
          read_only_roots().object_boilerplate_description_map_handle(), size,
          AllocationType::kOld));

  if (has_different_size_backing_store) {
    DCHECK_IMPLIES((boilerplate == (all_properties - index_keys)),
                   has_seen_proto);
    description->set_backing_store_size(backing_store_size);
  }

  description->set_flags(0);

  return description;
}

template <typename Impl>
Handle<ArrayBoilerplateDescription>
FactoryBase<Impl>::NewArrayBoilerplateDescription(
    ElementsKind elements_kind, Handle<FixedArrayBase> constant_values) {
  Handle<ArrayBoilerplateDescription> result =
      Handle<ArrayBoilerplateDescription>::cast(
          NewStruct(ARRAY_BOILERPLATE_DESCRIPTION_TYPE, AllocationType::kOld));
  result->set_elements_kind(elements_kind);
  result->set_constant_elements(*constant_values);
  return result;
}

template <typename Impl>
Handle<TemplateObjectDescription>
FactoryBase<Impl>::NewTemplateObjectDescription(
    Handle<FixedArray> raw_strings, Handle<FixedArray> cooked_strings) {
  DCHECK_EQ(raw_strings->length(), cooked_strings->length());
  DCHECK_LT(0, raw_strings->length());
  Handle<TemplateObjectDescription> result =
      Handle<TemplateObjectDescription>::cast(
          NewStruct(TEMPLATE_OBJECT_DESCRIPTION_TYPE, AllocationType::kOld));
  result->set_raw_strings(*raw_strings);
  result->set_cooked_strings(*cooked_strings);
  return result;
}

template <typename Impl>
Handle<FeedbackMetadata> FactoryBase<Impl>::NewFeedbackMetadata(
    int slot_count, int feedback_cell_count, AllocationType allocation) {
  DCHECK_LE(0, slot_count);
  int size = FeedbackMetadata::SizeFor(slot_count);
  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().feedback_metadata_map());
  Handle<FeedbackMetadata> data(FeedbackMetadata::cast(result), isolate());
  data->set_slot_count(slot_count);
  data->set_closure_feedback_cell_count(feedback_cell_count);

  // Initialize the data section to 0.
  int data_size = size - FeedbackMetadata::kHeaderSize;
  Address data_start = data->address() + FeedbackMetadata::kHeaderSize;
  memset(reinterpret_cast<byte*>(data_start), 0, data_size);
  // Fields have been zeroed out but not initialized, so this object will not
  // pass object verification at this point.
  return data;
}

template <typename Impl>
Handle<CoverageInfo> FactoryBase<Impl>::NewCoverageInfo(
    const ZoneVector<SourceRange>& slots) {
  const int slot_count = static_cast<int>(slots.size());

  int size = CoverageInfo::SizeFor(slot_count);
  Map map = read_only_roots().coverage_info_map();
  HeapObject result =
      AllocateRawWithImmortalMap(size, AllocationType::kYoung, map);
  Handle<CoverageInfo> info(CoverageInfo::cast(result), isolate());

  info->set_slot_count(slot_count);
  for (int i = 0; i < slot_count; i++) {
    SourceRange range = slots[i];
    info->InitializeSlot(i, range.start, range.end);
  }

  return info;
}

template <typename Impl>
Handle<SeqOneByteString> FactoryBase<Impl>::NewOneByteInternalizedString(
    const Vector<const uint8_t>& str, uint32_t hash_field) {
  Handle<SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_gc;
  MemCopy(result->GetChars(no_gc), str.begin(), str.length());
  return result;
}

template <typename Impl>
Handle<SeqTwoByteString> FactoryBase<Impl>::NewTwoByteInternalizedString(
    const Vector<const uc16>& str, uint32_t hash_field) {
  Handle<SeqTwoByteString> result =
      AllocateRawTwoByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_gc;
  MemCopy(result->GetChars(no_gc), str.begin(), str.length() * kUC16Size);
  return result;
}

template <typename Impl>
MaybeHandle<SeqOneByteString> FactoryBase<Impl>::NewRawOneByteString(
    int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqOneByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqOneByteString::SizeFor(length);
  DCHECK_GE(SeqOneByteString::kMaxSize, size);

  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().one_byte_string_map());
  Handle<SeqOneByteString> string =
      handle(SeqOneByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

template <typename Impl>
MaybeHandle<SeqTwoByteString> FactoryBase<Impl>::NewRawTwoByteString(
    int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqTwoByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqTwoByteString::SizeFor(length);
  DCHECK_GE(SeqTwoByteString::kMaxSize, size);

  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().string_map());
  Handle<SeqTwoByteString> string =
      handle(SeqTwoByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

template <typename Impl>
MaybeHandle<String> FactoryBase<Impl>::NewConsString(
    Handle<String> left, Handle<String> right, AllocationType allocation) {
  if (left->IsThinString()) {
    left = handle(ThinString::cast(*left).actual(), isolate());
  }
  if (right->IsThinString()) {
    right = handle(ThinString::cast(*right).actual(), isolate());
  }
  int left_length = left->length();
  if (left_length == 0) return right;
  int right_length = right->length();
  if (right_length == 0) return left;

  int length = left_length + right_length;

  if (length == 2) {
    uint16_t c1 = left->Get(0);
    uint16_t c2 = right->Get(0);
    return impl()->MakeOrFindTwoCharacterString(c1, c2);
  }

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }

  bool left_is_one_byte = left->IsOneByteRepresentation();
  bool right_is_one_byte = right->IsOneByteRepresentation();
  bool is_one_byte = left_is_one_byte && right_is_one_byte;

  // If the resulting string is small make a flat string.
  if (length < ConsString::kMinLength) {
    // Note that neither of the two inputs can be a slice because:
    STATIC_ASSERT(ConsString::kMinLength <= SlicedString::kMinLength);
    DCHECK(left->IsFlat());
    DCHECK(right->IsFlat());

    STATIC_ASSERT(ConsString::kMinLength <= String::kMaxLength);
    if (is_one_byte) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length, allocation).ToHandleChecked();
      DisallowHeapAllocation no_gc;
      uint8_t* dest = result->GetChars(no_gc);
      // Copy left part.
      const uint8_t* src = left->template GetChars<uint8_t>(no_gc);
      CopyChars(dest, src, left_length);
      // Copy right part.
      src = right->template GetChars<uint8_t>(no_gc);
      CopyChars(dest + left_length, src, right_length);
      return result;
    }

    Handle<SeqTwoByteString> result =
        NewRawTwoByteString(length, allocation).ToHandleChecked();

    DisallowHeapAllocation pointer_stays_valid;
    uc16* sink = result->GetChars(pointer_stays_valid);
    String::WriteToFlat(*left, sink, 0, left->length());
    String::WriteToFlat(*right, sink + left->length(), 0, right->length());
    return result;
  }

  return NewConsString(left, right, length, is_one_byte, allocation);
}

template <typename Impl>
Handle<String> FactoryBase<Impl>::NewConsString(Handle<String> left,
                                                Handle<String> right,
                                                int length, bool one_byte,
                                                AllocationType allocation) {
  DCHECK(!left->IsThinString());
  DCHECK(!right->IsThinString());
  DCHECK_GE(length, ConsString::kMinLength);
  DCHECK_LE(length, String::kMaxLength);

  Handle<ConsString> result = handle(
      ConsString::cast(
          one_byte
              ? NewWithImmortalMap(read_only_roots().cons_one_byte_string_map(),
                                   allocation)
              : NewWithImmortalMap(read_only_roots().cons_string_map(),
                                   allocation)),
      isolate());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(*left, mode);
  result->set_second(*right, mode);
  return result;
}

template <typename Impl>
Handle<FreshlyAllocatedBigInt> FactoryBase<Impl>::NewBigInt(
    int length, AllocationType allocation) {
  if (length < 0 || length > BigInt::kMaxLength) {
    isolate()->FatalProcessOutOfHeapMemory("invalid BigInt length");
  }
  HeapObject result = AllocateRawWithImmortalMap(
      BigInt::SizeFor(length), allocation, read_only_roots().bigint_map());
  FreshlyAllocatedBigInt bigint = FreshlyAllocatedBigInt::cast(result);
  bigint.clear_padding();
  return handle(bigint, isolate());
}

template <typename Impl>
Handle<ScopeInfo> FactoryBase<Impl>::NewScopeInfo(int length,
                                                  AllocationType type) {
  DCHECK(type == AllocationType::kOld || type == AllocationType::kReadOnly);
  return Handle<ScopeInfo>::cast(NewFixedArrayWithMap(
      read_only_roots().scope_info_map_handle(), length, type));
}

template <typename Impl>
Handle<SourceTextModuleInfo> FactoryBase<Impl>::NewSourceTextModuleInfo() {
  return Handle<SourceTextModuleInfo>::cast(NewFixedArrayWithMap(
      read_only_roots().module_info_map_handle(), SourceTextModuleInfo::kLength,
      AllocationType::kOld));
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::NewSharedFunctionInfo() {
  Map map = read_only_roots().shared_function_info_map();

  Handle<SharedFunctionInfo> shared = handle(
      SharedFunctionInfo::cast(NewWithImmortalMap(map, AllocationType::kOld)),
      isolate());
  int unique_id = -1;
#if V8_SFI_HAS_UNIQUE_ID
  unique_id = isolate()->GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  shared->Init(read_only_roots(), unique_id);

#ifdef VERIFY_HEAP
  shared->SharedFunctionInfoVerify(isolate());
#endif  // VERIFY_HEAP
  return shared;
}

template <typename Impl>
Handle<DescriptorArray> FactoryBase<Impl>::NewDescriptorArray(
    int number_of_descriptors, int slack, AllocationType allocation) {
  int number_of_all_descriptors = number_of_descriptors + slack;
  // Zero-length case must be handled outside.
  DCHECK_LT(0, number_of_all_descriptors);
  int size = DescriptorArray::SizeFor(number_of_all_descriptors);
  HeapObject obj = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().descriptor_array_map());
  DescriptorArray array = DescriptorArray::cast(obj);
  array.Initialize(read_only_roots().empty_enum_cache(),
                   read_only_roots().undefined_value(), number_of_descriptors,
                   slack);
  return handle(array, isolate());
}

template <typename Impl>
Handle<ClassPositions> FactoryBase<Impl>::NewClassPositions(int start,
                                                            int end) {
  Handle<ClassPositions> class_positions = Handle<ClassPositions>::cast(
      NewStruct(CLASS_POSITIONS_TYPE, AllocationType::kOld));
  class_positions->set_start(start);
  class_positions->set_end(end);
  return class_positions;
}

template <typename Impl>
Handle<SeqOneByteString>
FactoryBase<Impl>::AllocateRawOneByteInternalizedString(int length,
                                                        uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  // The canonical empty_string is the only zero-length string we allow.
  DCHECK_IMPLIES(length == 0, !impl()->EmptyStringRootIsInitialized());

  Map map = read_only_roots().one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size,
      impl()->CanAllocateInReadOnlySpace() ? AllocationType::kReadOnly
                                           : AllocationType::kOld,
      map);
  Handle<SeqOneByteString> answer =
      handle(SeqOneByteString::cast(result), isolate());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  return answer;
}

template <typename Impl>
Handle<SeqTwoByteString>
FactoryBase<Impl>::AllocateRawTwoByteInternalizedString(int length,
                                                        uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  DCHECK_NE(0, length);  // Use Heap::empty_string() instead.

  Map map = read_only_roots().internalized_string_map();
  int size = SeqTwoByteString::SizeFor(length);
  HeapObject result =
      AllocateRawWithImmortalMap(size, AllocationType::kOld, map);
  Handle<SeqTwoByteString> answer =
      handle(SeqTwoByteString::cast(result), isolate());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, result.Size());
  return answer;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawArray(int size,
                                               AllocationType allocation) {
  HeapObject result = AllocateRaw(size, allocation);
  if (size > kMaxRegularHeapObjectSize && FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(result);
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  return result;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawFixedArray(int length,
                                                    AllocationType allocation) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    isolate()->FatalProcessOutOfHeapMemory("invalid array length");
  }
  return AllocateRawArray(FixedArray::SizeFor(length), allocation);
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawWeakArrayList(
    int capacity, AllocationType allocation) {
  if (capacity < 0 || capacity > WeakArrayList::kMaxCapacity) {
    isolate()->FatalProcessOutOfHeapMemory("invalid array length");
  }
  return AllocateRawArray(WeakArrayList::SizeForCapacity(capacity), allocation);
}

template <typename Impl>
HeapObject FactoryBase<Impl>::NewWithImmortalMap(Map map,
                                                 AllocationType allocation) {
  return AllocateRawWithImmortalMap(map.instance_size(), allocation, map);
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawWithImmortalMap(
    int size, AllocationType allocation, Map map,
    AllocationAlignment alignment) {
  // TODO(delphick): Potentially you could also pass a immortal immovable Map
  // from MAP_SPACE here, like external_map or message_object_map, but currently
  // noone does so this check is sufficient.
  DCHECK(ReadOnlyHeap::Contains(map));
  HeapObject result = AllocateRaw(size, allocation, alignment);
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRaw(int size, AllocationType allocation,
                                          AllocationAlignment alignment) {
  return impl()->AllocateRaw(size, allocation, alignment);
}

// Instantiate FactoryBase for the two variants we want.
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) FactoryBase<Factory>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    FactoryBase<OffThreadFactory>;

}  // namespace internal
}  // namespace v8
