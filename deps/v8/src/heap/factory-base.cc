// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory-base.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/common/assert-scope.h"
#include "src/execution/local-isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/logging/local-logger.h"
#include "src/logging/log.h"
#include "src/objects/instance-type.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/source-text-module.h"
#include "src/objects/string-inl.h"
#include "src/objects/string.h"
#include "src/objects/swiss-name-dictionary-inl.h"
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
FactoryBase<Factory>::NewHeapNumber<AllocationType::kSharedOld>();

template V8_EXPORT_PRIVATE Handle<HeapNumber>
FactoryBase<LocalFactory>::NewHeapNumber<AllocationType::kOld>();

template <typename Impl>
Handle<Struct> FactoryBase<Impl>::NewStruct(InstanceType type,
                                            AllocationType allocation) {
  ReadOnlyRoots roots = read_only_roots();
  Map map = Map::GetInstanceTypeMap(roots, type);
  int size = map.instance_size();
  return handle(NewStructInternal(roots, map, size, allocation), isolate());
}

template <typename Impl>
Handle<AccessorPair> FactoryBase<Impl>::NewAccessorPair() {
  auto accessors =
      NewStructInternal<AccessorPair>(ACCESSOR_PAIR_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  accessors.set_getter(read_only_roots().null_value(), SKIP_WRITE_BARRIER);
  accessors.set_setter(read_only_roots().null_value(), SKIP_WRITE_BARRIER);
  return handle(accessors, isolate());
}

template <typename Impl>
Handle<CodeDataContainer> FactoryBase<Impl>::NewCodeDataContainer(
    int flags, AllocationType allocation) {
  Map map = read_only_roots().code_data_container_map();
  int size = map.instance_size();
  CodeDataContainer data_container = CodeDataContainer::cast(
      AllocateRawWithImmortalMap(size, allocation, map));
  DisallowGarbageCollection no_gc;
  data_container.set_next_code_link(read_only_roots().undefined_value(),
                                    SKIP_WRITE_BARRIER);
  data_container.set_kind_specific_flags(flags, kRelaxedStore);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    data_container.set_code_cage_base(impl()->isolate()->code_cage_base(),
                                      kRelaxedStore);
    Isolate* isolate_for_sandbox = impl()->isolate_for_sandbox();
    data_container.AllocateExternalPointerEntries(isolate_for_sandbox);
    data_container.set_raw_code(Smi::zero(), SKIP_WRITE_BARRIER);
    data_container.set_code_entry_point(isolate_for_sandbox, kNullAddress);
  }
  data_container.clear_padding();
  return handle(data_container, isolate());
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArray(int length,
                                                    AllocationType allocation) {
  if (length == 0) return impl()->empty_fixed_array();
  if (length < 0 || length > FixedArray::kMaxLength) {
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
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
  DisallowGarbageCollection no_gc;
  DCHECK(ReadOnlyHeap::Contains(*map));
  DCHECK(ReadOnlyHeap::Contains(*filler));
  result.set_map_after_allocation(*map, SKIP_WRITE_BARRIER);
  FixedArray array = FixedArray::cast(result);
  array.set_length(length);
  MemsetTagged(array.data_start(), *filler, length);
  return handle(array, isolate());
}

template <typename Impl>
Handle<FixedArray> FactoryBase<Impl>::NewFixedArrayWithZeroes(
    int length, AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return impl()->empty_fixed_array();
  if (length > FixedArray::kMaxLength) {
    FATAL("Invalid FixedArray size %d", length);
  }
  HeapObject result = AllocateRawFixedArray(length, allocation);
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(read_only_roots().fixed_array_map(),
                                  SKIP_WRITE_BARRIER);
  FixedArray array = FixedArray::cast(result);
  array.set_length(length);
  MemsetTagged(array.data_start(), Smi::zero(), length);
  return handle(array, isolate());
}

template <typename Impl>
Handle<FixedArrayBase> FactoryBase<Impl>::NewFixedDoubleArray(
    int length, AllocationType allocation) {
  if (length == 0) return impl()->empty_fixed_array();
  if (length < 0 || length > FixedDoubleArray::kMaxLength) {
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
  int size = FixedDoubleArray::SizeFor(length);
  Map map = read_only_roots().fixed_double_array_map();
  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, map, kDoubleAligned);
  DisallowGarbageCollection no_gc;
  FixedDoubleArray array = FixedDoubleArray::cast(result);
  array.set_length(length);
  return handle(array, isolate());
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
  DisallowGarbageCollection no_gc;
  WeakFixedArray array = WeakFixedArray::cast(result);
  array.set_length(length);
  MemsetTagged(ObjectSlot(array.data_start()),
               read_only_roots().undefined_value(), length);

  return handle(array, isolate());
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
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
  if (length == 0) return impl()->empty_byte_array();
  int size = ByteArray::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().byte_array_map());
  DisallowGarbageCollection no_gc;
  ByteArray array = ByteArray::cast(result);
  array.set_length(length);
  array.clear_padding();
  return handle(array, isolate());
}

template <typename Impl>
Handle<BytecodeArray> FactoryBase<Impl>::NewBytecodeArray(
    int length, const byte* raw_bytecodes, int frame_size, int parameter_count,
    Handle<FixedArray> constant_pool) {
  if (length < 0 || length > BytecodeArray::kMaxLength) {
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
  // Bytecode array is AllocationType::kOld, so constant pool array should be
  // too.
  DCHECK(!Heap::InYoungGeneration(*constant_pool));

  int size = BytecodeArray::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size, AllocationType::kOld, read_only_roots().bytecode_array_map());
  DisallowGarbageCollection no_gc;
  BytecodeArray instance = BytecodeArray::cast(result);
  instance.set_length(length);
  instance.set_frame_size(frame_size);
  instance.set_parameter_count(parameter_count);
  instance.set_incoming_new_target_or_generator_register(
      interpreter::Register::invalid_value());
  instance.reset_osr_urgency_and_install_target();
  instance.set_bytecode_age(BytecodeArray::kNoAgeBytecodeAge);
  instance.set_constant_pool(*constant_pool);
  instance.set_handler_table(read_only_roots().empty_byte_array(),
                             SKIP_WRITE_BARRIER);
  instance.set_source_position_table(read_only_roots().undefined_value(),
                                     kReleaseStore, SKIP_WRITE_BARRIER);
  CopyBytes(reinterpret_cast<byte*>(instance.GetFirstBytecodeAddress()),
            raw_bytecodes, length);
  instance.clear_padding();
  return handle(instance, isolate());
}

template <typename Impl>
Handle<Script> FactoryBase<Impl>::NewScript(
    Handle<PrimitiveHeapObject> source) {
  return NewScriptWithId(source, isolate()->GetNextScriptId());
}

template <typename Impl>
Handle<Script> FactoryBase<Impl>::NewScriptWithId(
    Handle<PrimitiveHeapObject> source, int script_id) {
  DCHECK(source->IsString() || source->IsUndefined());
  // Create and initialize script object.
  ReadOnlyRoots roots = read_only_roots();
  Handle<Script> script = handle(
      NewStructInternal<Script>(SCRIPT_TYPE, AllocationType::kOld), isolate());
  {
    DisallowGarbageCollection no_gc;
    Script raw = *script;
    raw.set_source(*source);
    raw.set_name(roots.undefined_value(), SKIP_WRITE_BARRIER);
    raw.set_id(script_id);
    raw.set_line_offset(0);
    raw.set_column_offset(0);
    raw.set_context_data(roots.undefined_value(), SKIP_WRITE_BARRIER);
    raw.set_type(Script::TYPE_NORMAL);
    raw.set_line_ends(roots.undefined_value(), SKIP_WRITE_BARRIER);
    raw.set_eval_from_shared_or_wrapped_arguments_or_sfi_table(
        roots.undefined_value(), SKIP_WRITE_BARRIER);
    raw.set_eval_from_position(0);
    raw.set_shared_function_infos(roots.empty_weak_fixed_array(),
                                  SKIP_WRITE_BARRIER);
    raw.set_flags(0);
    raw.set_host_defined_options(roots.empty_fixed_array(), SKIP_WRITE_BARRIER);
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
    raw.set_script_or_modules(roots.empty_array_list());
#endif
  }

  if (script_id != Script::kTemporaryScriptId) {
    impl()->AddToScriptList(script);
  }

  LOG(isolate(), ScriptEvent(Logger::ScriptEventType::kCreate, script_id));
  return script;
}

template <typename Impl>
Handle<ArrayList> FactoryBase<Impl>::NewArrayList(int size,
                                                  AllocationType allocation) {
  if (size == 0) return impl()->empty_array_list();
  Handle<FixedArray> fixed_array =
      NewFixedArray(size + ArrayList::kFirstIndex, allocation);
  {
    DisallowGarbageCollection no_gc;
    FixedArray raw = *fixed_array;
    raw.set_map_no_write_barrier(read_only_roots().array_list_map());
    ArrayList::cast(raw).SetLength(0);
  }
  return Handle<ArrayList>::cast(fixed_array);
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::NewSharedFunctionInfoForLiteral(
    FunctionLiteral* literal, Handle<Script> script, bool is_toplevel) {
  FunctionKind kind = literal->kind();
  Handle<SharedFunctionInfo> shared =
      NewSharedFunctionInfo(literal->GetName(isolate()), MaybeHandle<Code>(),
                            Builtin::kCompileLazy, kind);
  SharedFunctionInfo::InitFromFunctionLiteral(isolate(), shared, literal,
                                              is_toplevel);
  shared->SetScript(read_only_roots(), *script, literal->function_literal_id(),
                    false);
  return shared;
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::CloneSharedFunctionInfo(
    Handle<SharedFunctionInfo> other) {
  Map map = read_only_roots().shared_function_info_map();

  SharedFunctionInfo shared =
      SharedFunctionInfo::cast(NewWithImmortalMap(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;

  shared.CopyFrom(*other);
  shared.clear_padding();

  return handle(shared, isolate());
}

template <typename Impl>
Handle<PreparseData> FactoryBase<Impl>::NewPreparseData(int data_length,
                                                        int children_length) {
  int size = PreparseData::SizeFor(data_length, children_length);
  PreparseData result = PreparseData::cast(AllocateRawWithImmortalMap(
      size, AllocationType::kOld, read_only_roots().preparse_data_map()));
  DisallowGarbageCollection no_gc;
  result.set_data_length(data_length);
  result.set_children_length(children_length);
  MemsetTagged(result.inner_data_start(), read_only_roots().null_value(),
               children_length);
  result.clear_padding();
  return handle(result, isolate());
}

template <typename Impl>
Handle<UncompiledDataWithoutPreparseData>
FactoryBase<Impl>::NewUncompiledDataWithoutPreparseData(
    Handle<String> inferred_name, int32_t start_position,
    int32_t end_position) {
  return TorqueGeneratedFactory<Impl>::NewUncompiledDataWithoutPreparseData(
      inferred_name, start_position, end_position, AllocationType::kOld);
}

template <typename Impl>
Handle<UncompiledDataWithPreparseData>
FactoryBase<Impl>::NewUncompiledDataWithPreparseData(
    Handle<String> inferred_name, int32_t start_position, int32_t end_position,
    Handle<PreparseData> preparse_data) {
  return TorqueGeneratedFactory<Impl>::NewUncompiledDataWithPreparseData(
      inferred_name, start_position, end_position, preparse_data,
      AllocationType::kOld);
}

template <typename Impl>
Handle<UncompiledDataWithoutPreparseDataWithJob>
FactoryBase<Impl>::NewUncompiledDataWithoutPreparseDataWithJob(
    Handle<String> inferred_name, int32_t start_position,
    int32_t end_position) {
  return TorqueGeneratedFactory<
      Impl>::NewUncompiledDataWithoutPreparseDataWithJob(inferred_name,
                                                         start_position,
                                                         end_position,
                                                         kNullAddress,
                                                         AllocationType::kOld);
}

template <typename Impl>
Handle<UncompiledDataWithPreparseDataAndJob>
FactoryBase<Impl>::NewUncompiledDataWithPreparseDataAndJob(
    Handle<String> inferred_name, int32_t start_position, int32_t end_position,
    Handle<PreparseData> preparse_data) {
  return TorqueGeneratedFactory<Impl>::NewUncompiledDataWithPreparseDataAndJob(
      inferred_name, start_position, end_position, preparse_data, kNullAddress,
      AllocationType::kOld);
}

template <typename Impl>
Handle<SharedFunctionInfo> FactoryBase<Impl>::NewSharedFunctionInfo(
    MaybeHandle<String> maybe_name, MaybeHandle<HeapObject> maybe_function_data,
    Builtin builtin, FunctionKind kind) {
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo();
  DisallowGarbageCollection no_gc;
  SharedFunctionInfo raw = *shared;
  // Function names are assumed to be flat elsewhere.
  Handle<String> shared_name;
  bool has_shared_name = maybe_name.ToHandle(&shared_name);
  if (has_shared_name) {
    DCHECK(shared_name->IsFlat());
    raw.set_name_or_scope_info(*shared_name, kReleaseStore);
  } else {
    DCHECK_EQ(raw.name_or_scope_info(kAcquireLoad),
              SharedFunctionInfo::kNoSharedNameSentinel);
  }

  Handle<HeapObject> function_data;
  if (maybe_function_data.ToHandle(&function_data)) {
    // If we pass function_data then we shouldn't pass a builtin index, and
    // the function_data should not be code with a builtin.
    DCHECK(!Builtins::IsBuiltinId(builtin));
    DCHECK_IMPLIES(function_data->IsCode(),
                   !Code::cast(*function_data).is_builtin());
    raw.set_function_data(*function_data, kReleaseStore);
  } else if (Builtins::IsBuiltinId(builtin)) {
    raw.set_builtin_id(builtin);
  } else {
    DCHECK(raw.HasBuiltinId());
    DCHECK_EQ(Builtin::kIllegal, raw.builtin_id());
  }

  raw.CalculateConstructAsBuiltin();
  raw.set_kind(kind);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) raw.SharedFunctionInfoVerify(isolate());
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
  auto result = NewStructInternal<ArrayBoilerplateDescription>(
      ARRAY_BOILERPLATE_DESCRIPTION_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  result.set_elements_kind(elements_kind);
  result.set_constant_elements(*constant_values);
  return handle(result, isolate());
}

template <typename Impl>
Handle<RegExpBoilerplateDescription>
FactoryBase<Impl>::NewRegExpBoilerplateDescription(Handle<FixedArray> data,
                                                   Handle<String> source,
                                                   Smi flags) {
  auto result = NewStructInternal<RegExpBoilerplateDescription>(
      REG_EXP_BOILERPLATE_DESCRIPTION_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  result.set_data(*data);
  result.set_source(*source);
  result.set_flags(flags.value());
  return handle(result, isolate());
}

template <typename Impl>
Handle<TemplateObjectDescription>
FactoryBase<Impl>::NewTemplateObjectDescription(
    Handle<FixedArray> raw_strings, Handle<FixedArray> cooked_strings) {
  DCHECK_EQ(raw_strings->length(), cooked_strings->length());
  DCHECK_LT(0, raw_strings->length());
  auto result = NewStructInternal<TemplateObjectDescription>(
      TEMPLATE_OBJECT_DESCRIPTION_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  result.set_raw_strings(*raw_strings);
  result.set_cooked_strings(*cooked_strings);
  return handle(result, isolate());
}

template <typename Impl>
Handle<FeedbackMetadata> FactoryBase<Impl>::NewFeedbackMetadata(
    int slot_count, int create_closure_slot_count, AllocationType allocation) {
  DCHECK_LE(0, slot_count);
  int size = FeedbackMetadata::SizeFor(slot_count);
  FeedbackMetadata result = FeedbackMetadata::cast(AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().feedback_metadata_map()));
  result.set_slot_count(slot_count);
  result.set_create_closure_slot_count(create_closure_slot_count);

  // Initialize the data section to 0.
  int data_size = size - FeedbackMetadata::kHeaderSize;
  Address data_start = result.address() + FeedbackMetadata::kHeaderSize;
  memset(reinterpret_cast<byte*>(data_start), 0, data_size);
  // Fields have been zeroed out but not initialized, so this object will not
  // pass object verification at this point.
  return handle(result, isolate());
}

template <typename Impl>
Handle<CoverageInfo> FactoryBase<Impl>::NewCoverageInfo(
    const ZoneVector<SourceRange>& slots) {
  const int slot_count = static_cast<int>(slots.size());

  int size = CoverageInfo::SizeFor(slot_count);
  Map map = read_only_roots().coverage_info_map();
  CoverageInfo info = CoverageInfo::cast(
      AllocateRawWithImmortalMap(size, AllocationType::kOld, map));
  info.set_slot_count(slot_count);
  for (int i = 0; i < slot_count; i++) {
    SourceRange range = slots[i];
    info.InitializeSlot(i, range.start, range.end);
  }
  return handle(info, isolate());
}

template <typename Impl>
Handle<String> FactoryBase<Impl>::MakeOrFindTwoCharacterString(uint16_t c1,
                                                               uint16_t c2) {
  if ((c1 | c2) <= unibrow::Latin1::kMaxChar) {
    uint8_t buffer[] = {static_cast<uint8_t>(c1), static_cast<uint8_t>(c2)};
    return InternalizeString(base::Vector<const uint8_t>(buffer, 2));
  }
  uint16_t buffer[] = {c1, c2};
  return InternalizeString(base::Vector<const uint16_t>(buffer, 2));
}

template <typename Impl>
template <class StringTableKey>
Handle<String> FactoryBase<Impl>::InternalizeStringWithKey(
    StringTableKey* key) {
  return isolate()->string_table()->LookupKey(isolate(), key);
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<Factory>::InternalizeStringWithKey(
        OneByteStringKey* key);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<Factory>::InternalizeStringWithKey(
        TwoByteStringKey* key);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<Factory>::InternalizeStringWithKey(
        SeqOneByteSubStringKey* key);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<Factory>::InternalizeStringWithKey(
        SeqTwoByteSubStringKey* key);

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<LocalFactory>::InternalizeStringWithKey(
        OneByteStringKey* key);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> FactoryBase<LocalFactory>::InternalizeStringWithKey(
        TwoByteStringKey* key);

template <typename Impl>
Handle<String> FactoryBase<Impl>::InternalizeString(
    const base::Vector<const uint8_t>& string, bool convert_encoding) {
  SequentialStringKey<uint8_t> key(string, HashSeed(read_only_roots()),
                                   convert_encoding);
  return InternalizeStringWithKey(&key);
}

template <typename Impl>
Handle<String> FactoryBase<Impl>::InternalizeString(
    const base::Vector<const uint16_t>& string, bool convert_encoding) {
  SequentialStringKey<uint16_t> key(string, HashSeed(read_only_roots()),
                                    convert_encoding);
  return InternalizeStringWithKey(&key);
}

template <typename Impl>
Handle<SeqOneByteString> FactoryBase<Impl>::NewOneByteInternalizedString(
    const base::Vector<const uint8_t>& str, uint32_t raw_hash_field) {
  Handle<SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(str.length(), raw_hash_field);
  // No synchronization is needed since the shared string hasn't yet escaped to
  // script.
  DisallowGarbageCollection no_gc;
  MemCopy(result->GetChars(no_gc, SharedStringAccessGuardIfNeeded::NotNeeded()),
          str.begin(), str.length());
  return result;
}

template <typename Impl>
Handle<SeqTwoByteString> FactoryBase<Impl>::NewTwoByteInternalizedString(
    const base::Vector<const base::uc16>& str, uint32_t raw_hash_field) {
  Handle<SeqTwoByteString> result =
      AllocateRawTwoByteInternalizedString(str.length(), raw_hash_field);
  // No synchronization is needed since the shared string hasn't yet escaped to
  // script.
  DisallowGarbageCollection no_gc;
  MemCopy(result->GetChars(no_gc, SharedStringAccessGuardIfNeeded::NotNeeded()),
          str.begin(), str.length() * base::kUC16Size);
  return result;
}

template <typename Impl>
template <typename SeqStringT>
MaybeHandle<SeqStringT> FactoryBase<Impl>::NewRawStringWithMap(
    int length, Map map, AllocationType allocation) {
  DCHECK(SeqStringT::IsCompatibleMap(map, read_only_roots()));
  DCHECK_IMPLIES(!StringShape(map).IsShared(),
                 RefineAllocationTypeForInPlaceInternalizableString(
                     allocation, map) == allocation);
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqStringT);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqStringT::SizeFor(length);
  DCHECK_GE(SeqStringT::kMaxSize, size);

  SeqStringT string =
      SeqStringT::cast(AllocateRawWithImmortalMap(size, allocation, map));
  DisallowGarbageCollection no_gc;
  string.set_length(length);
  string.set_raw_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string.Size());
  return handle(string, isolate());
}

template <typename Impl>
MaybeHandle<SeqOneByteString> FactoryBase<Impl>::NewRawOneByteString(
    int length, AllocationType allocation) {
  Map map = read_only_roots().one_byte_string_map();
  return NewRawStringWithMap<SeqOneByteString>(
      length, map,
      RefineAllocationTypeForInPlaceInternalizableString(allocation, map));
}

template <typename Impl>
MaybeHandle<SeqTwoByteString> FactoryBase<Impl>::NewRawTwoByteString(
    int length, AllocationType allocation) {
  Map map = read_only_roots().string_map();
  return NewRawStringWithMap<SeqTwoByteString>(
      length, map,
      RefineAllocationTypeForInPlaceInternalizableString(allocation, map));
}

template <typename Impl>
MaybeHandle<SeqOneByteString> FactoryBase<Impl>::NewRawSharedOneByteString(
    int length) {
  return NewRawStringWithMap<SeqOneByteString>(
      length, read_only_roots().shared_one_byte_string_map(),
      AllocationType::kSharedOld);
}

template <typename Impl>
MaybeHandle<SeqTwoByteString> FactoryBase<Impl>::NewRawSharedTwoByteString(
    int length) {
  return NewRawStringWithMap<SeqTwoByteString>(
      length, read_only_roots().shared_string_map(),
      AllocationType::kSharedOld);
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
    uint16_t c1 = left->Get(0, isolate());
    uint16_t c2 = right->Get(0, isolate());
    return MakeOrFindTwoCharacterString(c1, c2);
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
      DisallowGarbageCollection no_gc;
      SharedStringAccessGuardIfNeeded access_guard(isolate());
      uint8_t* dest = result->GetChars(no_gc, access_guard);
      // Copy left part.
      {
        const uint8_t* src =
            left->template GetChars<uint8_t>(isolate(), no_gc, access_guard);
        CopyChars(dest, src, left_length);
      }
      // Copy right part.
      {
        const uint8_t* src =
            right->template GetChars<uint8_t>(isolate(), no_gc, access_guard);
        CopyChars(dest + left_length, src, right_length);
      }
      return result;
    }

    Handle<SeqTwoByteString> result =
        NewRawTwoByteString(length, allocation).ToHandleChecked();

    DisallowGarbageCollection no_gc;
    SharedStringAccessGuardIfNeeded access_guard(isolate());
    base::uc16* sink = result->GetChars(no_gc, access_guard);
    String::WriteToFlat(*left, sink, 0, left->length(), isolate(),
                        access_guard);
    String::WriteToFlat(*right, sink + left->length(), 0, right->length(),
                        isolate(), access_guard);
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

  ConsString result = ConsString::cast(
      one_byte ? NewWithImmortalMap(
                     read_only_roots().cons_one_byte_string_map(), allocation)
               : NewWithImmortalMap(read_only_roots().cons_string_map(),
                                    allocation));

  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = result.GetWriteBarrierMode(no_gc);
  result.set_raw_hash_field(String::kEmptyHashField);
  result.set_length(length);
  result.set_first(*left, mode);
  result.set_second(*right, mode);
  return handle(result, isolate());
}

template <typename Impl>
Handle<FreshlyAllocatedBigInt> FactoryBase<Impl>::NewBigInt(
    int length, AllocationType allocation) {
  if (length < 0 || length > BigInt::kMaxLength) {
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
  HeapObject result = AllocateRawWithImmortalMap(
      BigInt::SizeFor(length), allocation, read_only_roots().bigint_map());
  DisallowGarbageCollection no_gc;
  FreshlyAllocatedBigInt bigint = FreshlyAllocatedBigInt::cast(result);
  bigint.clear_padding();
  return handle(bigint, isolate());
}

template <typename Impl>
Handle<ScopeInfo> FactoryBase<Impl>::NewScopeInfo(int length,
                                                  AllocationType type) {
  DCHECK(type == AllocationType::kOld || type == AllocationType::kReadOnly);
  int size = ScopeInfo::SizeFor(length);
  HeapObject obj = AllocateRawWithImmortalMap(
      size, type, read_only_roots().scope_info_map());
  ScopeInfo scope_info = ScopeInfo::cast(obj);
  MemsetTagged(scope_info.data_start(), read_only_roots().undefined_value(),
               length);
  return handle(scope_info, isolate());
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

  SharedFunctionInfo shared =
      SharedFunctionInfo::cast(NewWithImmortalMap(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  int unique_id = -1;
#if V8_SFI_HAS_UNIQUE_ID
  unique_id = isolate()->GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  shared.Init(read_only_roots(), unique_id);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) shared.SharedFunctionInfoVerify(isolate());
#endif  // VERIFY_HEAP
  return handle(shared, isolate());
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
  auto result = NewStructInternal<ClassPositions>(CLASS_POSITIONS_TYPE,
                                                  AllocationType::kOld);
  result.set_start(start);
  result.set_end(end);
  return handle(result, isolate());
}

template <typename Impl>
Handle<SeqOneByteString>
FactoryBase<Impl>::AllocateRawOneByteInternalizedString(
    int length, uint32_t raw_hash_field) {
  CHECK_GE(String::kMaxLength, length);
  // The canonical empty_string is the only zero-length string we allow.
  DCHECK_IMPLIES(length == 0, !impl()->EmptyStringRootIsInitialized());

  Map map = read_only_roots().one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size,
      RefineAllocationTypeForInPlaceInternalizableString(
          impl()->CanAllocateInReadOnlySpace() ? AllocationType::kReadOnly
                                               : AllocationType::kOld,
          map),
      map);
  SeqOneByteString answer = SeqOneByteString::cast(result);
  DisallowGarbageCollection no_gc;
  answer.set_length(length);
  answer.set_raw_hash_field(raw_hash_field);
  DCHECK_EQ(size, answer.Size());
  return handle(answer, isolate());
}

template <typename Impl>
Handle<SeqTwoByteString>
FactoryBase<Impl>::AllocateRawTwoByteInternalizedString(
    int length, uint32_t raw_hash_field) {
  CHECK_GE(String::kMaxLength, length);
  DCHECK_NE(0, length);  // Use Heap::empty_string() instead.

  Map map = read_only_roots().internalized_string_map();
  int size = SeqTwoByteString::SizeFor(length);
  SeqTwoByteString answer = SeqTwoByteString::cast(AllocateRawWithImmortalMap(
      size,
      RefineAllocationTypeForInPlaceInternalizableString(AllocationType::kOld,
                                                         map),
      map));
  DisallowGarbageCollection no_gc;
  answer.set_length(length);
  answer.set_raw_hash_field(raw_hash_field);
  DCHECK_EQ(size, answer.Size());
  return handle(answer, isolate());
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawArray(int size,
                                               AllocationType allocation) {
  HeapObject result = AllocateRaw(size, allocation);
  if (!V8_ENABLE_THIRD_PARTY_HEAP_BOOL &&
      (size >
       isolate()->heap()->AsHeap()->MaxRegularHeapObjectSize(allocation)) &&
      FLAG_use_marking_progress_bar) {
    LargePage::FromHeapObject(result)->ProgressBar().Enable();
  }
  return result;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawFixedArray(int length,
                                                    AllocationType allocation) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    FATAL("Fatal JavaScript invalid size error %d", length);
    UNREACHABLE();
  }
  return AllocateRawArray(FixedArray::SizeFor(length), allocation);
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawWeakArrayList(
    int capacity, AllocationType allocation) {
  if (capacity < 0 || capacity > WeakArrayList::kMaxCapacity) {
    FATAL("Fatal JavaScript invalid size error %d", capacity);
    UNREACHABLE();
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
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRaw(int size, AllocationType allocation,
                                          AllocationAlignment alignment) {
  return impl()->AllocateRaw(size, allocation, alignment);
}

template <typename Impl>
Handle<SwissNameDictionary>
FactoryBase<Impl>::NewSwissNameDictionaryWithCapacity(
    int capacity, AllocationType allocation) {
  DCHECK(SwissNameDictionary::IsValidCapacity(capacity));

  if (capacity == 0) {
    DCHECK_NE(read_only_roots().at(RootIndex::kEmptySwissPropertyDictionary),
              kNullAddress);

    return read_only_roots().empty_swiss_property_dictionary_handle();
  }

  if (capacity < 0 || capacity > SwissNameDictionary::MaxCapacity()) {
    FATAL("Fatal JavaScript invalid size error %d", capacity);
    UNREACHABLE();
  }

  int meta_table_length = SwissNameDictionary::MetaTableSizeFor(capacity);
  Handle<ByteArray> meta_table =
      impl()->NewByteArray(meta_table_length, allocation);

  Map map = read_only_roots().swiss_name_dictionary_map();
  int size = SwissNameDictionary::SizeFor(capacity);
  SwissNameDictionary table = SwissNameDictionary::cast(
      AllocateRawWithImmortalMap(size, allocation, map));
  DisallowGarbageCollection no_gc;
  table.Initialize(isolate(), *meta_table, capacity);
  return handle(table, isolate());
}

template <typename Impl>
Handle<SwissNameDictionary> FactoryBase<Impl>::NewSwissNameDictionary(
    int at_least_space_for, AllocationType allocation) {
  return NewSwissNameDictionaryWithCapacity(
      SwissNameDictionary::CapacityFor(at_least_space_for), allocation);
}

template <typename Impl>
Handle<FunctionTemplateRareData>
FactoryBase<Impl>::NewFunctionTemplateRareData() {
  auto function_template_rare_data =
      NewStructInternal<FunctionTemplateRareData>(
          FUNCTION_TEMPLATE_RARE_DATA_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  function_template_rare_data.set_c_function_overloads(
      *impl()->empty_fixed_array(), SKIP_WRITE_BARRIER);
  return handle(function_template_rare_data, isolate());
}

template <typename Impl>
MaybeHandle<Map> FactoryBase<Impl>::GetInPlaceInternalizedStringMap(
    Map from_string_map) {
  InstanceType instance_type = from_string_map.instance_type();
  MaybeHandle<Map> map;
  switch (instance_type) {
    case STRING_TYPE:
    case SHARED_STRING_TYPE:
      map = read_only_roots().internalized_string_map_handle();
      break;
    case ONE_BYTE_STRING_TYPE:
    case SHARED_ONE_BYTE_STRING_TYPE:
      map = read_only_roots().one_byte_internalized_string_map_handle();
      break;
    case EXTERNAL_STRING_TYPE:
      map = read_only_roots().external_internalized_string_map_handle();
      break;
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
      map =
          read_only_roots().external_one_byte_internalized_string_map_handle();
      break;
    default:
      break;
  }
  DCHECK_EQ(!map.is_null(), String::IsInPlaceInternalizable(instance_type));
  return map;
}

template <typename Impl>
Handle<Map> FactoryBase<Impl>::GetStringMigrationSentinelMap(
    InstanceType from_string_type) {
  Handle<Map> map;
  switch (from_string_type) {
    case SHARED_STRING_TYPE:
      map = read_only_roots().seq_string_migration_sentinel_map_handle();
      break;
    case SHARED_ONE_BYTE_STRING_TYPE:
      map =
          read_only_roots().one_byte_seq_string_migration_sentinel_map_handle();
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_EQ(map->instance_type(), from_string_type);
  return map;
}

template <typename Impl>
AllocationType
FactoryBase<Impl>::RefineAllocationTypeForInPlaceInternalizableString(
    AllocationType allocation, Map string_map) {
#ifdef DEBUG
  InstanceType instance_type = string_map.instance_type();
  DCHECK(InstanceTypeChecker::IsInternalizedString(instance_type) ||
         String::IsInPlaceInternalizable(instance_type));
#endif
  if (FLAG_single_generation && allocation == AllocationType::kYoung) {
    allocation = AllocationType::kOld;
  }
  if (allocation != AllocationType::kOld) return allocation;
  return impl()->AllocationTypeForInPlaceInternalizableString();
}

// Instantiate FactoryBase for the two variants we want.
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) FactoryBase<Factory>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    FactoryBase<LocalFactory>;

}  // namespace internal
}  // namespace v8
