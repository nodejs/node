// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/builtins/constants-table-builder.h"
#include "src/compiler.h"
#include "src/conversions.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/scope-info.h"
#include "src/unicode-cache.h"
#include "src/unicode-decoder.h"

namespace v8 {
namespace internal {

namespace {

int ComputeCodeObjectSize(const CodeDesc& desc) {
  bool has_unwinding_info = desc.unwinding_info != nullptr;
  DCHECK((has_unwinding_info && desc.unwinding_info_size > 0) ||
         (!has_unwinding_info && desc.unwinding_info_size == 0));
  int body_size = desc.instr_size;
  int unwinding_info_size_field_size = kInt64Size;
  if (has_unwinding_info) {
    body_size = RoundUp(body_size, kInt64Size) + desc.unwinding_info_size +
                unwinding_info_size_field_size;
  }
  int object_size = Code::SizeFor(RoundUp(body_size, kObjectAlignment));
  DCHECK(IsAligned(static_cast<intptr_t>(object_size), kCodeAlignment));
  return object_size;
}

void InitializeCode(Heap* heap, Handle<Code> code, int object_size,
                    const CodeDesc& desc, Code::Kind kind,
                    Handle<Object> self_ref, int32_t builtin_index,
                    Handle<ByteArray> source_position_table,
                    Handle<DeoptimizationData> deopt_data,
                    Handle<ByteArray> reloc_info,
                    Handle<CodeDataContainer> data_container, uint32_t stub_key,
                    bool is_turbofanned, int stack_slots,
                    int safepoint_table_offset, int handler_table_offset) {
  DCHECK(IsAligned(code->address(), kCodeAlignment));
  DCHECK(!heap->memory_allocator()->code_range()->valid() ||
         heap->memory_allocator()->code_range()->contains(code->address()) ||
         object_size <= heap->code_space()->AreaSize());

  bool has_unwinding_info = desc.unwinding_info != nullptr;

  code->set_raw_instruction_size(desc.instr_size);
  code->set_relocation_info(*reloc_info);
  const bool is_off_heap_trampoline = false;
  code->initialize_flags(kind, has_unwinding_info, is_turbofanned, stack_slots,
                         is_off_heap_trampoline);
  code->set_safepoint_table_offset(safepoint_table_offset);
  code->set_handler_table_offset(handler_table_offset);
  code->set_code_data_container(*data_container);
  code->set_deoptimization_data(*deopt_data);
  code->set_stub_key(stub_key);
  code->set_source_position_table(*source_position_table);
  code->set_constant_pool_offset(desc.instr_size - desc.constant_pool_size);
  code->set_builtin_index(builtin_index);

  // Allow self references to created code object by patching the handle to
  // point to the newly allocated Code object.
  if (!self_ref.is_null()) {
    DCHECK(self_ref->IsOddball());
    DCHECK(Oddball::cast(*self_ref)->kind() == Oddball::kSelfReferenceMarker);
    if (FLAG_embedded_builtins) {
      auto builder = heap->isolate()->builtins_constants_table_builder();
      if (builder != nullptr) builder->PatchSelfReference(self_ref, code);
    }
    *(self_ref.location()) = *code;
  }

  // Migrate generated code.
  // The generated code can contain Object** values (typically from handles)
  // that are dereferenced during the copy to point directly to the actual heap
  // objects. These pointers can include references to the code object itself,
  // through the self_reference parameter.
  code->CopyFromNoFlush(heap, desc);

  code->clear_padding();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) code->ObjectVerify(heap->isolate());
#endif
}

}  // namespace

HeapObject* Factory::AllocateRawWithImmortalMap(int size,
                                                PretenureFlag pretenure,
                                                Map* map,
                                                AllocationAlignment alignment) {
  HeapObject* result = isolate()->heap()->AllocateRawWithRetryOrFail(
      size, Heap::SelectSpace(pretenure), alignment);
  result->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
}

HeapObject* Factory::AllocateRawWithAllocationSite(
    Handle<Map> map, PretenureFlag pretenure,
    Handle<AllocationSite> allocation_site) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  if (!allocation_site.is_null()) size += AllocationMemento::kSize;
  AllocationSpace space = Heap::SelectSpace(pretenure);
  HeapObject* result =
      isolate()->heap()->AllocateRawWithRetryOrFail(size, space);
  WriteBarrierMode write_barrier_mode =
      space == NEW_SPACE ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  result->set_map_after_allocation(*map, write_barrier_mode);
  if (!allocation_site.is_null()) {
    AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
        reinterpret_cast<Address>(result) + map->instance_size());
    InitializeAllocationMemento(alloc_memento, *allocation_site);
  }
  return result;
}

void Factory::InitializeAllocationMemento(AllocationMemento* memento,
                                          AllocationSite* allocation_site) {
  memento->set_map_after_allocation(*allocation_memento_map(),
                                    SKIP_WRITE_BARRIER);
  memento->set_allocation_site(allocation_site, SKIP_WRITE_BARRIER);
  if (FLAG_allocation_site_pretenuring) {
    allocation_site->IncrementMementoCreateCount();
  }
}

HeapObject* Factory::AllocateRawArray(int size, PretenureFlag pretenure) {
  AllocationSpace space = Heap::SelectSpace(pretenure);
  HeapObject* result =
      isolate()->heap()->AllocateRawWithRetryOrFail(size, space);
  if (size > kMaxRegularHeapObjectSize && FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(result->address());
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  return result;
}

HeapObject* Factory::AllocateRawFixedArray(int length,
                                           PretenureFlag pretenure) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  return AllocateRawArray(FixedArray::SizeFor(length), pretenure);
}

HeapObject* Factory::AllocateRawWeakArrayList(int capacity,
                                              PretenureFlag pretenure) {
  if (capacity < 0 || capacity > WeakArrayList::kMaxCapacity) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  return AllocateRawArray(WeakArrayList::SizeForCapacity(capacity), pretenure);
}

HeapObject* Factory::New(Handle<Map> map, PretenureFlag pretenure) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  AllocationSpace space = Heap::SelectSpace(pretenure);
  HeapObject* result =
      isolate()->heap()->AllocateRawWithRetryOrFail(size, space);
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode =
      space == NEW_SPACE ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  result->set_map_after_allocation(*map, write_barrier_mode);
  return result;
}

Handle<HeapObject> Factory::NewFillerObject(int size, bool double_align,
                                            AllocationSpace space) {
  AllocationAlignment alignment = double_align ? kDoubleAligned : kWordAligned;
  Heap* heap = isolate()->heap();
  HeapObject* result = heap->AllocateRawWithRetryOrFail(size, space, alignment);
#ifdef DEBUG
  MemoryChunk* chunk = MemoryChunk::FromAddress(result->address());
  DCHECK(chunk->owner()->identity() == space);
#endif
  heap->CreateFillerObjectAt(result->address(), size, ClearRecordedSlots::kNo);
  return Handle<HeapObject>(result, isolate());
}

Handle<PrototypeInfo> Factory::NewPrototypeInfo() {
  Handle<PrototypeInfo> result =
      Handle<PrototypeInfo>::cast(NewStruct(PROTOTYPE_INFO_TYPE, TENURED));
  result->set_prototype_users(*empty_weak_array_list());
  result->set_registry_slot(PrototypeInfo::UNREGISTERED);
  result->set_bit_field(0);
  result->set_module_namespace(*undefined_value());
  return result;
}

Handle<EnumCache> Factory::NewEnumCache(Handle<FixedArray> keys,
                                        Handle<FixedArray> indices) {
  return Handle<EnumCache>::cast(NewTuple2(keys, indices, TENURED));
}

Handle<Tuple2> Factory::NewTuple2(Handle<Object> value1, Handle<Object> value2,
                                  PretenureFlag pretenure) {
  Handle<Tuple2> result =
      Handle<Tuple2>::cast(NewStruct(TUPLE2_TYPE, pretenure));
  result->set_value1(*value1);
  result->set_value2(*value2);
  return result;
}

Handle<Tuple3> Factory::NewTuple3(Handle<Object> value1, Handle<Object> value2,
                                  Handle<Object> value3,
                                  PretenureFlag pretenure) {
  Handle<Tuple3> result =
      Handle<Tuple3>::cast(NewStruct(TUPLE3_TYPE, pretenure));
  result->set_value1(*value1);
  result->set_value2(*value2);
  result->set_value3(*value3);
  return result;
}

Handle<ArrayBoilerplateDescription> Factory::NewArrayBoilerplateDescription(
    ElementsKind elements_kind, Handle<FixedArrayBase> constant_values) {
  Handle<ArrayBoilerplateDescription> result =
      Handle<ArrayBoilerplateDescription>::cast(
          NewStruct(ARRAY_BOILERPLATE_DESCRIPTION_TYPE, TENURED));
  result->set_elements_kind(elements_kind);
  result->set_constant_elements(*constant_values);
  return result;
}

Handle<TemplateObjectDescription> Factory::NewTemplateObjectDescription(
    Handle<FixedArray> raw_strings, Handle<FixedArray> cooked_strings) {
  DCHECK_EQ(raw_strings->length(), cooked_strings->length());
  DCHECK_LT(0, raw_strings->length());
  Handle<TemplateObjectDescription> result =
      Handle<TemplateObjectDescription>::cast(NewStruct(TUPLE2_TYPE, TENURED));
  result->set_raw_strings(*raw_strings);
  result->set_cooked_strings(*cooked_strings);
  return result;
}

Handle<Oddball> Factory::NewOddball(Handle<Map> map, const char* to_string,
                                    Handle<Object> to_number,
                                    const char* type_of, byte kind,
                                    PretenureFlag pretenure) {
  Handle<Oddball> oddball(Oddball::cast(New(map, pretenure)), isolate());
  Oddball::Initialize(isolate(), oddball, to_string, to_number, type_of, kind);
  return oddball;
}

Handle<Oddball> Factory::NewSelfReferenceMarker(PretenureFlag pretenure) {
  return NewOddball(self_reference_marker_map(), "self_reference_marker",
                    handle(Smi::FromInt(-1), isolate()), "undefined",
                    Oddball::kSelfReferenceMarker, pretenure);
}

Handle<PropertyArray> Factory::NewPropertyArray(int length,
                                                PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_property_array();
  HeapObject* result = AllocateRawFixedArray(length, pretenure);
  result->set_map_after_allocation(*property_array_map(), SKIP_WRITE_BARRIER);
  Handle<PropertyArray> array(PropertyArray::cast(result), isolate());
  array->initialize_length(length);
  MemsetPointer(array->data_start(), *undefined_value(), length);
  return array;
}

Handle<FixedArray> Factory::NewFixedArrayWithFiller(
    Heap::RootListIndex map_root_index, int length, Object* filler,
    PretenureFlag pretenure) {
  HeapObject* result = AllocateRawFixedArray(length, pretenure);
  DCHECK(Heap::RootIsImmortalImmovable(map_root_index));
  Map* map = Map::cast(isolate()->heap()->root(map_root_index));
  result->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  Handle<FixedArray> array(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetPointer(array->data_start(), filler, length);
  return array;
}

template <typename T>
Handle<T> Factory::NewFixedArrayWithMap(Heap::RootListIndex map_root_index,
                                        int length, PretenureFlag pretenure) {
  static_assert(std::is_base_of<FixedArray, T>::value,
                "T must be a descendant of FixedArray");
  // Zero-length case must be handled outside, where the knowledge about
  // the map is.
  DCHECK_LT(0, length);
  return Handle<T>::cast(NewFixedArrayWithFiller(
      map_root_index, length, *undefined_value(), pretenure));
}

template <typename T>
Handle<T> Factory::NewWeakFixedArrayWithMap(Heap::RootListIndex map_root_index,
                                            int length,
                                            PretenureFlag pretenure) {
  static_assert(std::is_base_of<WeakFixedArray, T>::value,
                "T must be a descendant of WeakFixedArray");

  // Zero-length case must be handled outside.
  DCHECK_LT(0, length);

  HeapObject* result =
      AllocateRawArray(WeakFixedArray::SizeFor(length), pretenure);
  Map* map = Map::cast(isolate()->heap()->root(map_root_index));
  result->set_map_after_allocation(map, SKIP_WRITE_BARRIER);

  Handle<WeakFixedArray> array(WeakFixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetPointer(array->data_start(),
                HeapObjectReference::Strong(*undefined_value()), length);

  return Handle<T>::cast(array);
}

template Handle<FixedArray> Factory::NewFixedArrayWithMap<FixedArray>(
    Heap::RootListIndex, int, PretenureFlag);

template Handle<DescriptorArray>
Factory::NewWeakFixedArrayWithMap<DescriptorArray>(Heap::RootListIndex, int,
                                                   PretenureFlag);

Handle<FixedArray> Factory::NewFixedArray(int length, PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();
  return NewFixedArrayWithFiller(Heap::kFixedArrayMapRootIndex, length,
                                 *undefined_value(), pretenure);
}

Handle<WeakFixedArray> Factory::NewWeakFixedArray(int length,
                                                  PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_weak_fixed_array();
  HeapObject* result =
      AllocateRawArray(WeakFixedArray::SizeFor(length), pretenure);
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kWeakFixedArrayMapRootIndex));
  result->set_map_after_allocation(*weak_fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<WeakFixedArray> array(WeakFixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetPointer(array->data_start(),
                HeapObjectReference::Strong(*undefined_value()), length);
  return array;
}

MaybeHandle<FixedArray> Factory::TryNewFixedArray(int length,
                                                  PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();

  int size = FixedArray::SizeFor(length);
  AllocationSpace space = Heap::SelectSpace(pretenure);
  Heap* heap = isolate()->heap();
  AllocationResult allocation = heap->AllocateRaw(size, space);
  HeapObject* result = nullptr;
  if (!allocation.To(&result)) return MaybeHandle<FixedArray>();
  if (size > kMaxRegularHeapObjectSize && FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(result->address());
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  result->set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<FixedArray> array(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetPointer(array->data_start(), ReadOnlyRoots(heap).undefined_value(),
                length);
  return array;
}

Handle<FixedArray> Factory::NewFixedArrayWithHoles(int length,
                                                   PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();
  return NewFixedArrayWithFiller(Heap::kFixedArrayMapRootIndex, length,
                                 *the_hole_value(), pretenure);
}

Handle<FixedArray> Factory::NewUninitializedFixedArray(
    int length, PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();

  // TODO(ulan): As an experiment this temporarily returns an initialized fixed
  // array. After getting canary/performance coverage, either remove the
  // function or revert to returning uninitilized array.
  return NewFixedArrayWithFiller(Heap::kFixedArrayMapRootIndex, length,
                                 *undefined_value(), pretenure);
}

Handle<FeedbackVector> Factory::NewFeedbackVector(
    Handle<SharedFunctionInfo> shared, PretenureFlag pretenure) {
  int length = shared->feedback_metadata()->slot_count();
  DCHECK_LE(0, length);
  int size = FeedbackVector::SizeFor(length);

  HeapObject* result =
      AllocateRawWithImmortalMap(size, pretenure, *feedback_vector_map());
  Handle<FeedbackVector> vector(FeedbackVector::cast(result), isolate());
  vector->set_shared_function_info(*shared);
  vector->set_optimized_code_weak_or_smi(MaybeObject::FromSmi(Smi::FromEnum(
      FLAG_log_function_events ? OptimizationMarker::kLogFirstExecution
                               : OptimizationMarker::kNone)));
  vector->set_length(length);
  vector->set_invocation_count(0);
  vector->set_profiler_ticks(0);
  vector->set_deopt_count(0);
  // TODO(leszeks): Initialize based on the feedback metadata.
  MemsetPointer(vector->slots_start(),
                MaybeObject::FromObject(*undefined_value()), length);
  return vector;
}

Handle<ObjectBoilerplateDescription> Factory::NewObjectBoilerplateDescription(
    int boilerplate, int all_properties, int index_keys, bool has_seen_proto) {
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
          Heap::kObjectBoilerplateDescriptionMapRootIndex, size, TENURED));

  if (has_different_size_backing_store) {
    DCHECK_IMPLIES((boilerplate == (all_properties - index_keys)),
                   has_seen_proto);
    description->set_backing_store_size(isolate(), backing_store_size);
  }

  description->set_flags(0);

  return description;
}

Handle<FixedArrayBase> Factory::NewFixedDoubleArray(int length,
                                                    PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();
  if (length > FixedDoubleArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  int size = FixedDoubleArray::SizeFor(length);
  Map* map = *fixed_double_array_map();
  HeapObject* result =
      AllocateRawWithImmortalMap(size, pretenure, map, kDoubleAligned);
  Handle<FixedDoubleArray> array(FixedDoubleArray::cast(result), isolate());
  array->set_length(length);
  return array;
}

Handle<FixedArrayBase> Factory::NewFixedDoubleArrayWithHoles(
    int length, PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  Handle<FixedArrayBase> array = NewFixedDoubleArray(length, pretenure);
  if (length > 0) {
    Handle<FixedDoubleArray>::cast(array)->FillWithHoles(0, length);
  }
  return array;
}

Handle<FeedbackMetadata> Factory::NewFeedbackMetadata(int slot_count,
                                                      PretenureFlag tenure) {
  DCHECK_LE(0, slot_count);
  int size = FeedbackMetadata::SizeFor(slot_count);
  HeapObject* result =
      AllocateRawWithImmortalMap(size, tenure, *feedback_metadata_map());
  Handle<FeedbackMetadata> data(FeedbackMetadata::cast(result), isolate());
  data->set_slot_count(slot_count);

  // Initialize the data section to 0.
  int data_size = size - FeedbackMetadata::kHeaderSize;
  Address data_start = data->address() + FeedbackMetadata::kHeaderSize;
  memset(reinterpret_cast<byte*>(data_start), 0, data_size);
  // Fields have been zeroed out but not initialized, so this object will not
  // pass object verification at this point.
  return data;
}

Handle<FrameArray> Factory::NewFrameArray(int number_of_frames,
                                          PretenureFlag pretenure) {
  DCHECK_LE(0, number_of_frames);
  Handle<FixedArray> result = NewFixedArrayWithHoles(
      FrameArray::LengthFor(number_of_frames), pretenure);
  result->set(FrameArray::kFrameCountIndex, Smi::kZero);
  return Handle<FrameArray>::cast(result);
}

Handle<SmallOrderedHashSet> Factory::NewSmallOrderedHashSet(
    int capacity, PretenureFlag pretenure) {
  DCHECK_LE(0, capacity);
  CHECK_LE(capacity, SmallOrderedHashSet::kMaxCapacity);
  DCHECK_EQ(0, capacity % SmallOrderedHashSet::kLoadFactor);

  int size = SmallOrderedHashSet::SizeFor(capacity);
  Map* map = *small_ordered_hash_set_map();
  HeapObject* result = AllocateRawWithImmortalMap(size, pretenure, map);
  Handle<SmallOrderedHashSet> table(SmallOrderedHashSet::cast(result),
                                    isolate());
  table->Initialize(isolate(), capacity);
  return table;
}

Handle<SmallOrderedHashMap> Factory::NewSmallOrderedHashMap(
    int capacity, PretenureFlag pretenure) {
  DCHECK_LE(0, capacity);
  CHECK_LE(capacity, SmallOrderedHashMap::kMaxCapacity);
  DCHECK_EQ(0, capacity % SmallOrderedHashMap::kLoadFactor);

  int size = SmallOrderedHashMap::SizeFor(capacity);
  Map* map = *small_ordered_hash_map_map();
  HeapObject* result = AllocateRawWithImmortalMap(size, pretenure, map);
  Handle<SmallOrderedHashMap> table(SmallOrderedHashMap::cast(result),
                                    isolate());
  table->Initialize(isolate(), capacity);
  return table;
}

Handle<OrderedHashSet> Factory::NewOrderedHashSet() {
  return OrderedHashSet::Allocate(isolate(), OrderedHashSet::kMinCapacity);
}

Handle<OrderedHashMap> Factory::NewOrderedHashMap() {
  return OrderedHashMap::Allocate(isolate(), OrderedHashMap::kMinCapacity);
}

Handle<AccessorPair> Factory::NewAccessorPair() {
  Handle<AccessorPair> accessors =
      Handle<AccessorPair>::cast(NewStruct(ACCESSOR_PAIR_TYPE, TENURED));
  accessors->set_getter(*null_value(), SKIP_WRITE_BARRIER);
  accessors->set_setter(*null_value(), SKIP_WRITE_BARRIER);
  return accessors;
}

// Internalized strings are created in the old generation (data space).
Handle<String> Factory::InternalizeUtf8String(Vector<const char> string) {
  Utf8StringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}

Handle<String> Factory::InternalizeOneByteString(Vector<const uint8_t> string) {
  OneByteStringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}

Handle<String> Factory::InternalizeOneByteString(
    Handle<SeqOneByteString> string, int from, int length) {
  SeqOneByteSubStringKey key(isolate(), string, from, length);
  return InternalizeStringWithKey(&key);
}

Handle<String> Factory::InternalizeTwoByteString(Vector<const uc16> string) {
  TwoByteStringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}

template <class StringTableKey>
Handle<String> Factory::InternalizeStringWithKey(StringTableKey* key) {
  return StringTable::LookupKey(isolate(), key);
}

MaybeHandle<String> Factory::NewStringFromOneByte(Vector<const uint8_t> string,
                                                  PretenureFlag pretenure) {
  int length = string.length();
  if (length == 0) return empty_string();
  if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
  Handle<SeqOneByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                             NewRawOneByteString(string.length(), pretenure),
                             String);

  DisallowHeapAllocation no_gc;
  // Copy the characters into the new object.
  CopyChars(SeqOneByteString::cast(*result)->GetChars(), string.start(),
            length);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8(Vector<const char> string,
                                               PretenureFlag pretenure) {
  // Check for ASCII first since this is the common case.
  const char* ascii_data = string.start();
  int length = string.length();
  int non_ascii_start = String::NonAsciiStart(ascii_data, length);
  if (non_ascii_start >= length) {
    // If the string is ASCII, we do not need to convert the characters
    // since UTF8 is backwards compatible with ASCII.
    return NewStringFromOneByte(Vector<const uint8_t>::cast(string), pretenure);
  }

  // Non-ASCII and we need to decode.
  auto non_ascii = string.SubVector(non_ascii_start, length);
  Access<UnicodeCache::Utf8Decoder> decoder(
      isolate()->unicode_cache()->utf8_decoder());
  decoder->Reset(non_ascii);

  int utf16_length = static_cast<int>(decoder->Utf16Length());
  DCHECK_GT(utf16_length, 0);

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(non_ascii_start + utf16_length, pretenure), String);

  // Copy ASCII portion.
  uint16_t* data = result->GetChars();
  for (int i = 0; i < non_ascii_start; i++) {
    *data++ = *ascii_data++;
  }

  // Now write the remainder.
  decoder->WriteUtf16(data, utf16_length, non_ascii);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8SubString(
    Handle<SeqOneByteString> str, int begin, int length,
    PretenureFlag pretenure) {
  const char* ascii_data =
      reinterpret_cast<const char*>(str->GetChars() + begin);
  int non_ascii_start = String::NonAsciiStart(ascii_data, length);
  if (non_ascii_start >= length) {
    // If the string is ASCII, we can just make a substring.
    // TODO(v8): the pretenure flag is ignored in this case.
    return NewSubString(str, begin, begin + length);
  }

  // Non-ASCII and we need to decode.
  auto non_ascii = Vector<const char>(ascii_data + non_ascii_start,
                                      length - non_ascii_start);
  Access<UnicodeCache::Utf8Decoder> decoder(
      isolate()->unicode_cache()->utf8_decoder());
  decoder->Reset(non_ascii);

  int utf16_length = static_cast<int>(decoder->Utf16Length());
  DCHECK_GT(utf16_length, 0);

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(non_ascii_start + utf16_length, pretenure), String);

  // Update pointer references, since the original string may have moved after
  // allocation.
  ascii_data = reinterpret_cast<const char*>(str->GetChars() + begin);
  non_ascii = Vector<const char>(ascii_data + non_ascii_start,
                                 length - non_ascii_start);

  // Copy ASCII portion.
  uint16_t* data = result->GetChars();
  for (int i = 0; i < non_ascii_start; i++) {
    *data++ = *ascii_data++;
  }

  // Now write the remainder.
  decoder->WriteUtf16(data, utf16_length, non_ascii);
  return result;
}

MaybeHandle<String> Factory::NewStringFromTwoByte(const uc16* string,
                                                  int length,
                                                  PretenureFlag pretenure) {
  if (length == 0) return empty_string();
  if (String::IsOneByte(string, length)) {
    if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               NewRawOneByteString(length, pretenure), String);
    CopyChars(result->GetChars(), string, length);
    return result;
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               NewRawTwoByteString(length, pretenure), String);
    CopyChars(result->GetChars(), string, length);
    return result;
  }
}

MaybeHandle<String> Factory::NewStringFromTwoByte(Vector<const uc16> string,
                                                  PretenureFlag pretenure) {
  return NewStringFromTwoByte(string.start(), string.length(), pretenure);
}

MaybeHandle<String> Factory::NewStringFromTwoByte(
    const ZoneVector<uc16>* string, PretenureFlag pretenure) {
  return NewStringFromTwoByte(string->data(), static_cast<int>(string->size()),
                              pretenure);
}

namespace {

bool inline IsOneByte(Vector<const char> str, int chars) {
  // TODO(dcarney): incorporate Latin-1 check when Latin-1 is supported?
  return chars == str.length();
}

bool inline IsOneByte(Handle<String> str) {
  return str->IsOneByteRepresentation();
}

inline void WriteOneByteData(Vector<const char> vector, uint8_t* chars,
                             int len) {
  // Only works for one byte strings.
  DCHECK(vector.length() == len);
  MemCopy(chars, vector.start(), len);
}

inline void WriteTwoByteData(Vector<const char> vector, uint16_t* chars,
                             int len) {
  unibrow::Utf8Iterator it = unibrow::Utf8Iterator(vector);
  while (!it.Done()) {
    DCHECK_GT(len, 0);
    len -= 1;

    uint16_t c = *it;
    ++it;
    DCHECK_NE(unibrow::Utf8::kBadChar, c);
    *chars++ = c;
  }
  DCHECK_EQ(len, 0);
}

inline void WriteOneByteData(Handle<String> s, uint8_t* chars, int len) {
  DCHECK(s->length() == len);
  String::WriteToFlat(*s, chars, 0, len);
}

inline void WriteTwoByteData(Handle<String> s, uint16_t* chars, int len) {
  DCHECK(s->length() == len);
  String::WriteToFlat(*s, chars, 0, len);
}

}  // namespace

Handle<SeqOneByteString> Factory::AllocateRawOneByteInternalizedString(
    int length, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  // The canonical empty_string is the only zero-length string we allow.
  DCHECK_IMPLIES(
      length == 0,
      isolate()->heap()->roots_[Heap::kempty_stringRootIndex] == nullptr);

  Map* map = *one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(length);
  HeapObject* result = AllocateRawWithImmortalMap(
      size,
      isolate()->heap()->CanAllocateInReadOnlySpace() ? TENURED_READ_ONLY
                                                      : TENURED,
      map);
  Handle<SeqOneByteString> answer(SeqOneByteString::cast(result), isolate());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  return answer;
}

Handle<String> Factory::AllocateTwoByteInternalizedString(
    Vector<const uc16> str, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, str.length());
  DCHECK_NE(0, str.length());  // Use Heap::empty_string() instead.

  Map* map = *internalized_string_map();
  int size = SeqTwoByteString::SizeFor(str.length());
  HeapObject* result = AllocateRawWithImmortalMap(size, TENURED, map);
  Handle<SeqTwoByteString> answer(SeqTwoByteString::cast(result), isolate());
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());

  // Fill in the characters.
  MemCopy(answer->GetChars(), str.start(), str.length() * kUC16Size);

  return answer;
}

template <bool is_one_byte, typename T>
Handle<String> Factory::AllocateInternalizedStringImpl(T t, int chars,
                                                       uint32_t hash_field) {
  DCHECK_LE(0, chars);
  DCHECK_GE(String::kMaxLength, chars);

  // Compute map and object size.
  int size;
  Map* map;
  if (is_one_byte) {
    map = *one_byte_internalized_string_map();
    size = SeqOneByteString::SizeFor(chars);
  } else {
    map = *internalized_string_map();
    size = SeqTwoByteString::SizeFor(chars);
  }

  HeapObject* result = AllocateRawWithImmortalMap(
      size,
      isolate()->heap()->CanAllocateInReadOnlySpace() ? TENURED_READ_ONLY
                                                      : TENURED,
      map);
  Handle<String> answer(String::cast(result), isolate());
  answer->set_length(chars);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());

  if (is_one_byte) {
    WriteOneByteData(t, SeqOneByteString::cast(*answer)->GetChars(), chars);
  } else {
    WriteTwoByteData(t, SeqTwoByteString::cast(*answer)->GetChars(), chars);
  }
  return answer;
}

Handle<String> Factory::NewInternalizedStringFromUtf8(Vector<const char> str,
                                                      int chars,
                                                      uint32_t hash_field) {
  if (IsOneByte(str, chars)) {
    Handle<SeqOneByteString> result =
        AllocateRawOneByteInternalizedString(str.length(), hash_field);
    MemCopy(result->GetChars(), str.start(), str.length());
    return result;
  }
  return AllocateInternalizedStringImpl<false>(str, chars, hash_field);
}

Handle<String> Factory::NewOneByteInternalizedString(Vector<const uint8_t> str,
                                                     uint32_t hash_field) {
  Handle<SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(str.length(), hash_field);
  MemCopy(result->GetChars(), str.start(), str.length());
  return result;
}

Handle<String> Factory::NewOneByteInternalizedSubString(
    Handle<SeqOneByteString> string, int offset, int length,
    uint32_t hash_field) {
  Handle<SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(length, hash_field);
  MemCopy(result->GetChars(), string->GetChars() + offset, length);
  return result;
}

Handle<String> Factory::NewTwoByteInternalizedString(Vector<const uc16> str,
                                                     uint32_t hash_field) {
  return AllocateTwoByteInternalizedString(str, hash_field);
}

Handle<String> Factory::NewInternalizedStringImpl(Handle<String> string,
                                                  int chars,
                                                  uint32_t hash_field) {
  if (IsOneByte(string)) {
    return AllocateInternalizedStringImpl<true>(string, chars, hash_field);
  }
  return AllocateInternalizedStringImpl<false>(string, chars, hash_field);
}

namespace {

MaybeHandle<Map> GetInternalizedStringMap(Factory* f, Handle<String> string) {
  switch (string->map()->instance_type()) {
    case STRING_TYPE:
      return f->internalized_string_map();
    case ONE_BYTE_STRING_TYPE:
      return f->one_byte_internalized_string_map();
    case EXTERNAL_STRING_TYPE:
      return f->external_internalized_string_map();
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
      return f->external_one_byte_internalized_string_map();
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return f->external_internalized_string_with_one_byte_data_map();
    case SHORT_EXTERNAL_STRING_TYPE:
      return f->short_external_internalized_string_map();
    case SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE:
      return f->short_external_one_byte_internalized_string_map();
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return f->short_external_internalized_string_with_one_byte_data_map();
    default:
      return MaybeHandle<Map>();  // No match found.
  }
}

}  // namespace

MaybeHandle<Map> Factory::InternalizedStringMapForString(
    Handle<String> string) {
  // If the string is in new space it cannot be used as internalized.
  if (Heap::InNewSpace(*string)) return MaybeHandle<Map>();

  return GetInternalizedStringMap(this, string);
}

template <class StringClass>
Handle<StringClass> Factory::InternalizeExternalString(Handle<String> string) {
  Handle<StringClass> cast_string = Handle<StringClass>::cast(string);
  Handle<Map> map = GetInternalizedStringMap(this, string).ToHandleChecked();
  Handle<StringClass> external_string(StringClass::cast(New(map, TENURED)),
                                      isolate());
  external_string->set_length(cast_string->length());
  external_string->set_hash_field(cast_string->hash_field());
  external_string->SetResource(isolate(), nullptr);
  isolate()->heap()->RegisterExternalString(*external_string);
  return external_string;
}

template Handle<ExternalOneByteString>
    Factory::InternalizeExternalString<ExternalOneByteString>(Handle<String>);
template Handle<ExternalTwoByteString>
    Factory::InternalizeExternalString<ExternalTwoByteString>(Handle<String>);

MaybeHandle<SeqOneByteString> Factory::NewRawOneByteString(
    int length, PretenureFlag pretenure) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqOneByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqOneByteString::SizeFor(length);
  DCHECK_GE(SeqOneByteString::kMaxSize, size);

  HeapObject* result =
      AllocateRawWithImmortalMap(size, pretenure, *one_byte_string_map());
  Handle<SeqOneByteString> string(SeqOneByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

MaybeHandle<SeqTwoByteString> Factory::NewRawTwoByteString(
    int length, PretenureFlag pretenure) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqTwoByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqTwoByteString::SizeFor(length);
  DCHECK_GE(SeqTwoByteString::kMaxSize, size);

  HeapObject* result =
      AllocateRawWithImmortalMap(size, pretenure, *string_map());
  Handle<SeqTwoByteString> string(SeqTwoByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

Handle<String> Factory::LookupSingleCharacterStringFromCode(uint32_t code) {
  if (code <= String::kMaxOneByteCharCodeU) {
    {
      DisallowHeapAllocation no_allocation;
      Object* value = single_character_string_cache()->get(code);
      if (value != *undefined_value()) {
        return handle(String::cast(value), isolate());
      }
    }
    uint8_t buffer[1];
    buffer[0] = static_cast<uint8_t>(code);
    Handle<String> result =
        InternalizeOneByteString(Vector<const uint8_t>(buffer, 1));
    single_character_string_cache()->set(code, *result);
    return result;
  }
  DCHECK_LE(code, String::kMaxUtf16CodeUnitU);

  Handle<SeqTwoByteString> result = NewRawTwoByteString(1).ToHandleChecked();
  result->SeqTwoByteStringSet(0, static_cast<uint16_t>(code));
  return result;
}

// Returns true for a character in a range.  Both limits are inclusive.
static inline bool Between(uint32_t character, uint32_t from, uint32_t to) {
  // This makes uses of the the unsigned wraparound.
  return character - from <= to - from;
}

static inline Handle<String> MakeOrFindTwoCharacterString(Isolate* isolate,
                                                          uint16_t c1,
                                                          uint16_t c2) {
  // Numeric strings have a different hash algorithm not known by
  // LookupTwoCharsStringIfExists, so we skip this step for such strings.
  if (!Between(c1, '0', '9') || !Between(c2, '0', '9')) {
    Handle<String> result;
    if (StringTable::LookupTwoCharsStringIfExists(isolate, c1, c2)
            .ToHandle(&result)) {
      return result;
    }
  }

  // Now we know the length is 2, we might as well make use of that fact
  // when building the new string.
  if (static_cast<unsigned>(c1 | c2) <= String::kMaxOneByteCharCodeU) {
    // We can do this.
    DCHECK(base::bits::IsPowerOfTwo(String::kMaxOneByteCharCodeU +
                                    1));  // because of this.
    Handle<SeqOneByteString> str =
        isolate->factory()->NewRawOneByteString(2).ToHandleChecked();
    uint8_t* dest = str->GetChars();
    dest[0] = static_cast<uint8_t>(c1);
    dest[1] = static_cast<uint8_t>(c2);
    return str;
  } else {
    Handle<SeqTwoByteString> str =
        isolate->factory()->NewRawTwoByteString(2).ToHandleChecked();
    uc16* dest = str->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return str;
  }
}

template <typename SinkChar, typename StringType>
Handle<String> ConcatStringContent(Handle<StringType> result,
                                   Handle<String> first,
                                   Handle<String> second) {
  DisallowHeapAllocation pointer_stays_valid;
  SinkChar* sink = result->GetChars();
  String::WriteToFlat(*first, sink, 0, first->length());
  String::WriteToFlat(*second, sink + first->length(), 0, second->length());
  return result;
}

MaybeHandle<String> Factory::NewConsString(Handle<String> left,
                                           Handle<String> right) {
  if (left->IsThinString()) {
    left = handle(Handle<ThinString>::cast(left)->actual(), isolate());
  }
  if (right->IsThinString()) {
    right = handle(Handle<ThinString>::cast(right)->actual(), isolate());
  }
  int left_length = left->length();
  if (left_length == 0) return right;
  int right_length = right->length();
  if (right_length == 0) return left;

  int length = left_length + right_length;

  if (length == 2) {
    uint16_t c1 = left->Get(0);
    uint16_t c2 = right->Get(0);
    return MakeOrFindTwoCharacterString(isolate(), c1, c2);
  }

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }

  bool left_is_one_byte = left->IsOneByteRepresentation();
  bool right_is_one_byte = right->IsOneByteRepresentation();
  bool is_one_byte = left_is_one_byte && right_is_one_byte;
  bool is_one_byte_data_in_two_byte_string = false;
  if (!is_one_byte) {
    // At least one of the strings uses two-byte representation so we
    // can't use the fast case code for short one-byte strings below, but
    // we can try to save memory if all chars actually fit in one-byte.
    is_one_byte_data_in_two_byte_string =
        left->HasOnlyOneByteChars() && right->HasOnlyOneByteChars();
    if (is_one_byte_data_in_two_byte_string) {
      isolate()->counters()->string_add_runtime_ext_to_one_byte()->Increment();
    }
  }

  // If the resulting string is small make a flat string.
  if (length < ConsString::kMinLength) {
    // Note that neither of the two inputs can be a slice because:
    STATIC_ASSERT(ConsString::kMinLength <= SlicedString::kMinLength);
    DCHECK(left->IsFlat());
    DCHECK(right->IsFlat());

    STATIC_ASSERT(ConsString::kMinLength <= String::kMaxLength);
    if (is_one_byte) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      DisallowHeapAllocation no_gc;
      uint8_t* dest = result->GetChars();
      // Copy left part.
      const uint8_t* src =
          left->IsExternalString()
              ? Handle<ExternalOneByteString>::cast(left)->GetChars()
              : Handle<SeqOneByteString>::cast(left)->GetChars();
      for (int i = 0; i < left_length; i++) *dest++ = src[i];
      // Copy right part.
      src = right->IsExternalString()
                ? Handle<ExternalOneByteString>::cast(right)->GetChars()
                : Handle<SeqOneByteString>::cast(right)->GetChars();
      for (int i = 0; i < right_length; i++) *dest++ = src[i];
      return result;
    }

    return (is_one_byte_data_in_two_byte_string)
               ? ConcatStringContent<uint8_t>(
                     NewRawOneByteString(length).ToHandleChecked(), left, right)
               : ConcatStringContent<uc16>(
                     NewRawTwoByteString(length).ToHandleChecked(), left,
                     right);
  }

  bool one_byte = (is_one_byte || is_one_byte_data_in_two_byte_string);
  return NewConsString(left, right, length, one_byte);
}

Handle<String> Factory::NewConsString(Handle<String> left, Handle<String> right,
                                      int length, bool one_byte) {
  DCHECK(!left->IsThinString());
  DCHECK(!right->IsThinString());
  DCHECK_GE(length, ConsString::kMinLength);
  DCHECK_LE(length, String::kMaxLength);

  Handle<ConsString> result(
      ConsString::cast(one_byte ? New(cons_one_byte_string_map(), NOT_TENURED)
                                : New(cons_string_map(), NOT_TENURED)),
      isolate());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(isolate(), *left, mode);
  result->set_second(isolate(), *right, mode);
  return result;
}

Handle<String> Factory::NewSurrogatePairString(uint16_t lead, uint16_t trail) {
  DCHECK_GE(lead, 0xD800);
  DCHECK_LE(lead, 0xDBFF);
  DCHECK_GE(trail, 0xDC00);
  DCHECK_LE(trail, 0xDFFF);

  Handle<SeqTwoByteString> str =
      isolate()->factory()->NewRawTwoByteString(2).ToHandleChecked();
  uc16* dest = str->GetChars();
  dest[0] = lead;
  dest[1] = trail;
  return str;
}

Handle<String> Factory::NewProperSubString(Handle<String> str, int begin,
                                           int end) {
#if VERIFY_HEAP
  if (FLAG_verify_heap) str->StringVerify(isolate());
#endif
  DCHECK(begin > 0 || end < str->length());

  str = String::Flatten(isolate(), str);

  int length = end - begin;
  if (length <= 0) return empty_string();
  if (length == 1) {
    return LookupSingleCharacterStringFromCode(str->Get(begin));
  }
  if (length == 2) {
    // Optimization for 2-byte strings often used as keys in a decompression
    // dictionary.  Check whether we already have the string in the string
    // table to prevent creation of many unnecessary strings.
    uint16_t c1 = str->Get(begin);
    uint16_t c2 = str->Get(begin + 1);
    return MakeOrFindTwoCharacterString(isolate(), c1, c2);
  }

  if (!FLAG_string_slices || length < SlicedString::kMinLength) {
    if (str->IsOneByteRepresentation()) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      uint8_t* dest = result->GetChars();
      DisallowHeapAllocation no_gc;
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    } else {
      Handle<SeqTwoByteString> result =
          NewRawTwoByteString(length).ToHandleChecked();
      uc16* dest = result->GetChars();
      DisallowHeapAllocation no_gc;
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    }
  }

  int offset = begin;

  if (str->IsSlicedString()) {
    Handle<SlicedString> slice = Handle<SlicedString>::cast(str);
    str = Handle<String>(slice->parent(), isolate());
    offset += slice->offset();
  }
  if (str->IsThinString()) {
    Handle<ThinString> thin = Handle<ThinString>::cast(str);
    str = handle(thin->actual(), isolate());
  }

  DCHECK(str->IsSeqString() || str->IsExternalString());
  Handle<Map> map = str->IsOneByteRepresentation()
                        ? sliced_one_byte_string_map()
                        : sliced_string_map();
  Handle<SlicedString> slice(SlicedString::cast(New(map, NOT_TENURED)),
                             isolate());

  slice->set_hash_field(String::kEmptyHashField);
  slice->set_length(length);
  slice->set_parent(isolate(), *str);
  slice->set_offset(offset);
  return slice;
}

MaybeHandle<String> Factory::NewExternalStringFromOneByte(
    const ExternalOneByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }
  if (length == 0) return empty_string();

  Handle<Map> map;
  if (resource->IsCompressible()) {
    // TODO(hajimehoshi): Rename this to 'uncached_external_one_byte_string_map'
    map = short_external_one_byte_string_map();
  } else {
    map = external_one_byte_string_map();
  }
  Handle<ExternalOneByteString> external_string(
      ExternalOneByteString::cast(New(map, TENURED)), isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->SetResource(isolate(), resource);
  isolate()->heap()->RegisterExternalString(*external_string);

  return external_string;
}

MaybeHandle<String> Factory::NewExternalStringFromTwoByte(
    const ExternalTwoByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }
  if (length == 0) return empty_string();

  // For small strings we check whether the resource contains only
  // one byte characters.  If yes, we use a different string map.
  static const size_t kOneByteCheckLengthLimit = 32;
  bool is_one_byte =
      length <= kOneByteCheckLengthLimit &&
      String::IsOneByte(resource->data(), static_cast<int>(length));
  Handle<Map> map;
  if (resource->IsCompressible()) {
    // TODO(hajimehoshi): Rename these to 'uncached_external_string_...'.
    map = is_one_byte ? short_external_string_with_one_byte_data_map()
                      : short_external_string_map();
  } else {
    map = is_one_byte ? external_string_with_one_byte_data_map()
                      : external_string_map();
  }
  Handle<ExternalTwoByteString> external_string(
      ExternalTwoByteString::cast(New(map, TENURED)), isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->SetResource(isolate(), resource);
  isolate()->heap()->RegisterExternalString(*external_string);

  return external_string;
}

Handle<ExternalOneByteString> Factory::NewNativeSourceString(
    const ExternalOneByteString::Resource* resource) {
  size_t length = resource->length();
  DCHECK_LE(length, static_cast<size_t>(String::kMaxLength));

  Handle<Map> map = native_source_string_map();
  Handle<ExternalOneByteString> external_string(
      ExternalOneByteString::cast(New(map, TENURED)), isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->SetResource(isolate(), resource);
  isolate()->heap()->RegisterExternalString(*external_string);

  return external_string;
}

Handle<JSStringIterator> Factory::NewJSStringIterator(Handle<String> string) {
  Handle<Map> map(isolate()->native_context()->string_iterator_map(),
                  isolate());
  Handle<String> flat_string = String::Flatten(isolate(), string);
  Handle<JSStringIterator> iterator =
      Handle<JSStringIterator>::cast(NewJSObjectFromMap(map));
  iterator->set_string(*flat_string);
  iterator->set_index(0);

  return iterator;
}

Handle<Symbol> Factory::NewSymbol(PretenureFlag flag) {
  DCHECK(flag != NOT_TENURED);
  // Statically ensure that it is safe to allocate symbols in paged spaces.
  STATIC_ASSERT(Symbol::kSize <= kMaxRegularHeapObjectSize);

  HeapObject* result =
      AllocateRawWithImmortalMap(Symbol::kSize, flag, *symbol_map());

  // Generate a random hash value.
  int hash = isolate()->GenerateIdentityHash(Name::kHashBitMask);

  Handle<Symbol> symbol(Symbol::cast(result), isolate());
  symbol->set_hash_field(Name::kIsNotArrayIndexMask |
                         (hash << Name::kHashShift));
  symbol->set_name(*undefined_value());
  symbol->set_flags(0);
  DCHECK(!symbol->is_private());
  return symbol;
}

Handle<Symbol> Factory::NewPrivateSymbol(PretenureFlag flag) {
  DCHECK(flag != NOT_TENURED);
  Handle<Symbol> symbol = NewSymbol(flag);
  symbol->set_is_private(true);
  return symbol;
}

Handle<Symbol> Factory::NewPrivateFieldSymbol() {
  Handle<Symbol> symbol = NewSymbol();
  symbol->set_is_private_field();
  return symbol;
}

Handle<NativeContext> Factory::NewNativeContext() {
  Handle<NativeContext> context = NewFixedArrayWithMap<NativeContext>(
      Heap::kNativeContextMapRootIndex, Context::NATIVE_CONTEXT_SLOTS, TENURED);
  context->set_native_context(*context);
  context->set_errors_thrown(Smi::kZero);
  context->set_math_random_index(Smi::kZero);
  context->set_serialized_objects(*empty_fixed_array());
  return context;
}

Handle<Context> Factory::NewScriptContext(Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), SCRIPT_SCOPE);
  Handle<Context> context = NewFixedArrayWithMap<Context>(
      Heap::kScriptContextMapRootIndex, scope_info->ContextLength(), TENURED);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*the_hole_value());
  context->set_native_context(*outer);
  DCHECK(context->IsScriptContext());
  return context;
}

Handle<ScriptContextTable> Factory::NewScriptContextTable() {
  Handle<ScriptContextTable> context_table =
      NewFixedArrayWithMap<ScriptContextTable>(
          Heap::kScriptContextTableMapRootIndex,
          ScriptContextTable::kMinLength);
  context_table->set_used(0);
  return context_table;
}

Handle<Context> Factory::NewModuleContext(Handle<Module> module,
                                          Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), MODULE_SCOPE);
  Handle<Context> context = NewFixedArrayWithMap<Context>(
      Heap::kModuleContextMapRootIndex, scope_info->ContextLength(), TENURED);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*module);
  context->set_native_context(*outer);
  DCHECK(context->IsModuleContext());
  return context;
}

Handle<Context> Factory::NewFunctionContext(Handle<Context> outer,
                                            Handle<ScopeInfo> scope_info) {
  int length = scope_info->ContextLength();
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, length);
  Heap::RootListIndex mapRootIndex;
  switch (scope_info->scope_type()) {
    case EVAL_SCOPE:
      mapRootIndex = Heap::kEvalContextMapRootIndex;
      break;
    case FUNCTION_SCOPE:
      mapRootIndex = Heap::kFunctionContextMapRootIndex;
      break;
    default:
      UNREACHABLE();
  }
  Handle<Context> context = NewFixedArrayWithMap<Context>(mapRootIndex, length);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*the_hole_value());
  context->set_native_context(outer->native_context());
  return context;
}

Handle<Context> Factory::NewCatchContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info,
                                         Handle<Object> thrown_object) {
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == Context::THROWN_OBJECT_INDEX);
  Handle<Context> context = NewFixedArrayWithMap<Context>(
      Heap::kCatchContextMapRootIndex, Context::MIN_CONTEXT_SLOTS + 1);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*the_hole_value());
  context->set_native_context(previous->native_context());
  context->set(Context::THROWN_OBJECT_INDEX, *thrown_object);
  return context;
}

Handle<Context> Factory::NewDebugEvaluateContext(Handle<Context> previous,
                                                 Handle<ScopeInfo> scope_info,
                                                 Handle<JSReceiver> extension,
                                                 Handle<Context> wrapped,
                                                 Handle<StringSet> whitelist) {
  STATIC_ASSERT(Context::WHITE_LIST_INDEX == Context::MIN_CONTEXT_SLOTS + 1);
  DCHECK(scope_info->IsDebugEvaluateScope());
  Handle<HeapObject> ext = extension.is_null()
                               ? Handle<HeapObject>::cast(the_hole_value())
                               : Handle<HeapObject>::cast(extension);
  Handle<Context> c = NewFixedArrayWithMap<Context>(
      Heap::kDebugEvaluateContextMapRootIndex, Context::MIN_CONTEXT_SLOTS + 2);
  c->set_scope_info(*scope_info);
  c->set_previous(*previous);
  c->set_native_context(previous->native_context());
  c->set_extension(*ext);
  if (!wrapped.is_null()) c->set(Context::WRAPPED_CONTEXT_INDEX, *wrapped);
  if (!whitelist.is_null()) c->set(Context::WHITE_LIST_INDEX, *whitelist);
  return c;
}

Handle<Context> Factory::NewWithContext(Handle<Context> previous,
                                        Handle<ScopeInfo> scope_info,
                                        Handle<JSReceiver> extension) {
  Handle<Context> context = NewFixedArrayWithMap<Context>(
      Heap::kWithContextMapRootIndex, Context::MIN_CONTEXT_SLOTS);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*extension);
  context->set_native_context(previous->native_context());
  return context;
}

Handle<Context> Factory::NewBlockContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), BLOCK_SCOPE);
  Handle<Context> context = NewFixedArrayWithMap<Context>(
      Heap::kBlockContextMapRootIndex, scope_info->ContextLength());
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*the_hole_value());
  context->set_native_context(previous->native_context());
  return context;
}

Handle<Context> Factory::NewBuiltinContext(Handle<NativeContext> native_context,
                                           int length) {
  DCHECK_GE(length, Context::MIN_CONTEXT_SLOTS);
  Handle<Context> context =
      NewFixedArrayWithMap<Context>(Heap::kFunctionContextMapRootIndex, length);
  context->set_scope_info(ReadOnlyRoots(isolate()).empty_scope_info());
  context->set_extension(*the_hole_value());
  context->set_native_context(*native_context);
  return context;
}

Handle<Struct> Factory::NewStruct(InstanceType type, PretenureFlag pretenure) {
  Map* map;
  switch (type) {
#define MAKE_CASE(NAME, Name, name) \
  case NAME##_TYPE:                 \
    map = *name##_map();            \
    break;
    STRUCT_LIST(MAKE_CASE)
#undef MAKE_CASE
    default:
      UNREACHABLE();
  }
  int size = map->instance_size();
  HeapObject* result = AllocateRawWithImmortalMap(size, pretenure, map);
  Handle<Struct> str(Struct::cast(result), isolate());
  str->InitializeBody(size);
  return str;
}

Handle<AliasedArgumentsEntry> Factory::NewAliasedArgumentsEntry(
    int aliased_context_slot) {
  Handle<AliasedArgumentsEntry> entry = Handle<AliasedArgumentsEntry>::cast(
      NewStruct(ALIASED_ARGUMENTS_ENTRY_TYPE, NOT_TENURED));
  entry->set_aliased_context_slot(aliased_context_slot);
  return entry;
}

Handle<AccessorInfo> Factory::NewAccessorInfo() {
  Handle<AccessorInfo> info =
      Handle<AccessorInfo>::cast(NewStruct(ACCESSOR_INFO_TYPE, TENURED));
  info->set_name(*empty_string());
  info->set_flags(0);  // Must clear the flags, it was initialized as undefined.
  info->set_is_sloppy(true);
  info->set_initial_property_attributes(NONE);
  return info;
}

Handle<Script> Factory::NewScript(Handle<String> source, PretenureFlag tenure) {
  return NewScriptWithId(source, isolate()->heap()->NextScriptId(), tenure);
}

Handle<Script> Factory::NewScriptWithId(Handle<String> source, int script_id,
                                        PretenureFlag tenure) {
  DCHECK(tenure == TENURED || tenure == TENURED_READ_ONLY);
  // Create and initialize script object.
  Heap* heap = isolate()->heap();
  ReadOnlyRoots roots(heap);
  Handle<Script> script = Handle<Script>::cast(NewStruct(SCRIPT_TYPE, tenure));
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
  script->set_shared_function_infos(*empty_weak_fixed_array(),
                                    SKIP_WRITE_BARRIER);
  script->set_flags(0);
  script->set_host_defined_options(*empty_fixed_array());
  Handle<WeakArrayList> scripts = script_list();
  scripts = WeakArrayList::AddToEnd(isolate(), scripts,
                                    MaybeObjectHandle::Weak(script));
  heap->set_script_list(*scripts);
  LOG(isolate(), ScriptEvent(Logger::ScriptEventType::kCreate, script_id));
  return script;
}

Handle<Script> Factory::CloneScript(Handle<Script> script) {
  Heap* heap = isolate()->heap();
  int script_id = isolate()->heap()->NextScriptId();
  Handle<Script> new_script =
      Handle<Script>::cast(NewStruct(SCRIPT_TYPE, TENURED));
  new_script->set_source(script->source());
  new_script->set_name(script->name());
  new_script->set_id(script_id);
  new_script->set_line_offset(script->line_offset());
  new_script->set_column_offset(script->column_offset());
  new_script->set_context_data(script->context_data());
  new_script->set_type(script->type());
  new_script->set_line_ends(ReadOnlyRoots(heap).undefined_value());
  new_script->set_eval_from_shared_or_wrapped_arguments(
      script->eval_from_shared_or_wrapped_arguments());
  new_script->set_shared_function_infos(*empty_weak_fixed_array(),
                                        SKIP_WRITE_BARRIER);
  new_script->set_eval_from_position(script->eval_from_position());
  new_script->set_flags(script->flags());
  new_script->set_host_defined_options(script->host_defined_options());
  Handle<WeakArrayList> scripts = script_list();
  scripts = WeakArrayList::AddToEnd(isolate(), scripts,
                                    MaybeObjectHandle::Weak(new_script));
  heap->set_script_list(*scripts);
  LOG(isolate(), ScriptEvent(Logger::ScriptEventType::kCreate, script_id));
  return new_script;
}

Handle<CallableTask> Factory::NewCallableTask(Handle<JSReceiver> callable,
                                              Handle<Context> context) {
  DCHECK(callable->IsCallable());
  Handle<CallableTask> microtask =
      Handle<CallableTask>::cast(NewStruct(CALLABLE_TASK_TYPE));
  microtask->set_callable(*callable);
  microtask->set_context(*context);
  return microtask;
}

Handle<CallbackTask> Factory::NewCallbackTask(Handle<Foreign> callback,
                                              Handle<Foreign> data) {
  Handle<CallbackTask> microtask =
      Handle<CallbackTask>::cast(NewStruct(CALLBACK_TASK_TYPE));
  microtask->set_callback(*callback);
  microtask->set_data(*data);
  return microtask;
}

Handle<PromiseResolveThenableJobTask> Factory::NewPromiseResolveThenableJobTask(
    Handle<JSPromise> promise_to_resolve, Handle<JSReceiver> then,
    Handle<JSReceiver> thenable, Handle<Context> context) {
  DCHECK(then->IsCallable());
  Handle<PromiseResolveThenableJobTask> microtask =
      Handle<PromiseResolveThenableJobTask>::cast(
          NewStruct(PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE));
  microtask->set_promise_to_resolve(*promise_to_resolve);
  microtask->set_then(*then);
  microtask->set_thenable(*thenable);
  microtask->set_context(*context);
  return microtask;
}

Handle<Foreign> Factory::NewForeign(Address addr, PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  STATIC_ASSERT(Foreign::kSize <= kMaxRegularHeapObjectSize);
  Map* map = *foreign_map();
  HeapObject* result =
      AllocateRawWithImmortalMap(map->instance_size(), pretenure, map);
  Handle<Foreign> foreign(Foreign::cast(result), isolate());
  foreign->set_foreign_address(addr);
  return foreign;
}

Handle<ByteArray> Factory::NewByteArray(int length, PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  if (length > ByteArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  int size = ByteArray::SizeFor(length);
  HeapObject* result =
      AllocateRawWithImmortalMap(size, pretenure, *byte_array_map());
  Handle<ByteArray> array(ByteArray::cast(result), isolate());
  array->set_length(length);
  array->clear_padding();
  return array;
}

Handle<BytecodeArray> Factory::NewBytecodeArray(
    int length, const byte* raw_bytecodes, int frame_size, int parameter_count,
    Handle<FixedArray> constant_pool) {
  DCHECK_LE(0, length);
  if (length > BytecodeArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  // Bytecode array is pretenured, so constant pool array should be too.
  DCHECK(!Heap::InNewSpace(*constant_pool));

  int size = BytecodeArray::SizeFor(length);
  HeapObject* result =
      AllocateRawWithImmortalMap(size, TENURED, *bytecode_array_map());
  Handle<BytecodeArray> instance(BytecodeArray::cast(result), isolate());
  instance->set_length(length);
  instance->set_frame_size(frame_size);
  instance->set_parameter_count(parameter_count);
  instance->set_incoming_new_target_or_generator_register(
      interpreter::Register::invalid_value());
  instance->set_interrupt_budget(interpreter::Interpreter::InterruptBudget());
  instance->set_osr_loop_nesting_level(0);
  instance->set_bytecode_age(BytecodeArray::kNoAgeBytecodeAge);
  instance->set_constant_pool(*constant_pool);
  instance->set_handler_table(*empty_byte_array());
  instance->set_source_position_table(*empty_byte_array());
  CopyBytes(reinterpret_cast<byte*>(instance->GetFirstBytecodeAddress()),
            raw_bytecodes, length);
  instance->clear_padding();

  return instance;
}

Handle<FixedTypedArrayBase> Factory::NewFixedTypedArrayWithExternalPointer(
    int length, ExternalArrayType array_type, void* external_pointer,
    PretenureFlag pretenure) {
  // TODO(7881): Smi length check
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  int size = FixedTypedArrayBase::kHeaderSize;
  HeapObject* result = AllocateRawWithImmortalMap(
      size, pretenure, isolate()->heap()->MapForFixedTypedArray(array_type));
  Handle<FixedTypedArrayBase> elements(FixedTypedArrayBase::cast(result),
                                       isolate());
  elements->set_base_pointer(Smi::kZero, SKIP_WRITE_BARRIER);
  elements->set_external_pointer(external_pointer, SKIP_WRITE_BARRIER);
  elements->set_length(length);
  return elements;
}

Handle<FixedTypedArrayBase> Factory::NewFixedTypedArray(
    size_t length, size_t byte_length, ExternalArrayType array_type,
    bool initialize, PretenureFlag pretenure) {
  // TODO(7881): Smi length check
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  CHECK(byte_length <= kMaxInt - FixedTypedArrayBase::kDataOffset);
  size_t size =
      OBJECT_POINTER_ALIGN(byte_length + FixedTypedArrayBase::kDataOffset);
  Map* map = isolate()->heap()->MapForFixedTypedArray(array_type);
  AllocationAlignment alignment =
      array_type == kExternalFloat64Array ? kDoubleAligned : kWordAligned;
  HeapObject* object = AllocateRawWithImmortalMap(static_cast<int>(size),
                                                  pretenure, map, alignment);

  Handle<FixedTypedArrayBase> elements(FixedTypedArrayBase::cast(object),
                                       isolate());
  elements->set_base_pointer(*elements, SKIP_WRITE_BARRIER);
  elements->set_external_pointer(
      reinterpret_cast<void*>(
          ExternalReference::fixed_typed_array_base_data_offset().address()),
      SKIP_WRITE_BARRIER);
  elements->set_length(static_cast<int>(length));
  if (initialize) memset(elements->DataPtr(), 0, elements->DataSize());
  return elements;
}

Handle<Cell> Factory::NewCell(Handle<Object> value) {
  AllowDeferredHandleDereference convert_to_cell;
  STATIC_ASSERT(Cell::kSize <= kMaxRegularHeapObjectSize);
  HeapObject* result =
      AllocateRawWithImmortalMap(Cell::kSize, TENURED, *cell_map());
  Handle<Cell> cell(Cell::cast(result), isolate());
  cell->set_value(*value);
  return cell;
}

Handle<FeedbackCell> Factory::NewNoClosuresCell(Handle<HeapObject> value) {
  AllowDeferredHandleDereference convert_to_cell;
  HeapObject* result = AllocateRawWithImmortalMap(FeedbackCell::kSize, TENURED,
                                                  *no_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  return cell;
}

Handle<FeedbackCell> Factory::NewOneClosureCell(Handle<HeapObject> value) {
  AllowDeferredHandleDereference convert_to_cell;
  HeapObject* result = AllocateRawWithImmortalMap(FeedbackCell::kSize, TENURED,
                                                  *one_closure_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  return cell;
}

Handle<FeedbackCell> Factory::NewManyClosuresCell(Handle<HeapObject> value) {
  AllowDeferredHandleDereference convert_to_cell;
  HeapObject* result = AllocateRawWithImmortalMap(FeedbackCell::kSize, TENURED,
                                                  *many_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  return cell;
}

Handle<PropertyCell> Factory::NewPropertyCell(Handle<Name> name,
                                              PretenureFlag pretenure) {
  DCHECK(name->IsUniqueName());
  STATIC_ASSERT(PropertyCell::kSize <= kMaxRegularHeapObjectSize);
  HeapObject* result = AllocateRawWithImmortalMap(
      PropertyCell::kSize, pretenure, *global_property_cell_map());
  Handle<PropertyCell> cell(PropertyCell::cast(result), isolate());
  cell->set_dependent_code(DependentCode::cast(*empty_weak_fixed_array()),
                           SKIP_WRITE_BARRIER);
  cell->set_property_details(PropertyDetails(Smi::kZero));
  cell->set_name(*name);
  cell->set_value(*the_hole_value());
  return cell;
}

Handle<TransitionArray> Factory::NewTransitionArray(int number_of_transitions,
                                                    int slack) {
  int capacity = TransitionArray::LengthFor(number_of_transitions + slack);
  Handle<TransitionArray> array = NewWeakFixedArrayWithMap<TransitionArray>(
      Heap::kTransitionArrayMapRootIndex, capacity, TENURED);
  // Transition arrays are tenured. When black allocation is on we have to
  // add the transition array to the list of encountered_transition_arrays.
  Heap* heap = isolate()->heap();
  if (heap->incremental_marking()->black_allocation()) {
    heap->mark_compact_collector()->AddTransitionArray(*array);
  }
  array->WeakFixedArray::Set(TransitionArray::kPrototypeTransitionsIndex,
                             MaybeObject::FromObject(Smi::kZero));
  array->WeakFixedArray::Set(
      TransitionArray::kTransitionLengthIndex,
      MaybeObject::FromObject(Smi::FromInt(number_of_transitions)));
  return array;
}

Handle<AllocationSite> Factory::NewAllocationSite(bool with_weak_next) {
  Handle<Map> map = with_weak_next ? allocation_site_map()
                                   : allocation_site_without_weaknext_map();
  Handle<AllocationSite> site(AllocationSite::cast(New(map, TENURED)),
                              isolate());
  site->Initialize();

  if (with_weak_next) {
    // Link the site
    site->set_weak_next(isolate()->heap()->allocation_sites_list());
    isolate()->heap()->set_allocation_sites_list(*site);
  }
  return site;
}

Handle<Map> Factory::NewMap(InstanceType type, int instance_size,
                            ElementsKind elements_kind,
                            int inobject_properties) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  DCHECK_IMPLIES(Map::IsJSObject(type) &&
                     !Map::CanHaveFastTransitionableElementsKind(type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind));
  HeapObject* result =
      isolate()->heap()->AllocateRawWithRetryOrFail(Map::kSize, MAP_SPACE);
  result->set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
}

Map* Factory::InitializeMap(Map* map, InstanceType type, int instance_size,
                            ElementsKind elements_kind,
                            int inobject_properties) {
  map->set_instance_type(type);
  map->set_prototype(*null_value(), SKIP_WRITE_BARRIER);
  map->set_constructor_or_backpointer(*null_value(), SKIP_WRITE_BARRIER);
  map->set_instance_size(instance_size);
  if (map->IsJSObjectMap()) {
    DCHECK(!isolate()->heap()->InReadOnlySpace(map));
    map->SetInObjectPropertiesStartInWords(instance_size / kPointerSize -
                                           inobject_properties);
    DCHECK_EQ(map->GetInObjectProperties(), inobject_properties);
    map->set_prototype_validity_cell(*invalid_prototype_validity_cell());
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map->set_inobject_properties_start_or_constructor_function_index(0);
    map->set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  }
  map->set_dependent_code(DependentCode::cast(*empty_weak_fixed_array()),
                          SKIP_WRITE_BARRIER);
  map->set_raw_transitions(MaybeObject::FromSmi(Smi::kZero));
  map->SetInObjectUnusedPropertyFields(inobject_properties);
  map->set_instance_descriptors(*empty_descriptor_array());
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  // Must be called only after |instance_type|, |instance_size| and
  // |layout_descriptor| are set.
  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_bit_field(0);
  map->set_bit_field2(Map::IsExtensibleBit::kMask);
  DCHECK(!map->is_in_retained_map_list());
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptorsBit::encode(true) |
                   Map::ConstructionCounterBits::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(elements_kind);
  map->set_new_target_is_base(true);
  isolate()->counters()->maps_created()->Increment();
  if (FLAG_trace_maps) LOG(isolate(), MapCreate(map));
  return map;
}

Handle<JSObject> Factory::CopyJSObject(Handle<JSObject> source) {
  return CopyJSObjectWithAllocationSite(source, Handle<AllocationSite>());
}

Handle<JSObject> Factory::CopyJSObjectWithAllocationSite(
    Handle<JSObject> source, Handle<AllocationSite> site) {
  Handle<Map> map(source->map(), isolate());

  // We can only clone regexps, normal objects, api objects, errors or arrays.
  // Copying anything else will break invariants.
  CHECK(map->instance_type() == JS_REGEXP_TYPE ||
        map->instance_type() == JS_OBJECT_TYPE ||
        map->instance_type() == JS_ERROR_TYPE ||
        map->instance_type() == JS_ARRAY_TYPE ||
        map->instance_type() == JS_API_OBJECT_TYPE ||
        map->instance_type() == WASM_GLOBAL_TYPE ||
        map->instance_type() == WASM_INSTANCE_TYPE ||
        map->instance_type() == WASM_MEMORY_TYPE ||
        map->instance_type() == WASM_MODULE_TYPE ||
        map->instance_type() == WASM_TABLE_TYPE ||
        map->instance_type() == JS_SPECIAL_API_OBJECT_TYPE);
  DCHECK(site.is_null() || AllocationSite::CanTrack(map->instance_type()));

  int object_size = map->instance_size();
  int adjusted_object_size =
      site.is_null() ? object_size : object_size + AllocationMemento::kSize;
  HeapObject* raw_clone = isolate()->heap()->AllocateRawWithRetryOrFail(
      adjusted_object_size, NEW_SPACE);

  SLOW_DCHECK(Heap::InNewSpace(raw_clone));
  // Since we know the clone is allocated in new space, we can copy
  // the contents without worrying about updating the write barrier.
  Heap::CopyBlock(raw_clone->address(), source->address(), object_size);
  Handle<JSObject> clone(JSObject::cast(raw_clone), isolate());

  if (!site.is_null()) {
    AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
        reinterpret_cast<Address>(raw_clone) + object_size);
    InitializeAllocationMemento(alloc_memento, *site);
  }

  SLOW_DCHECK(clone->GetElementsKind() == source->GetElementsKind());
  FixedArrayBase* elements = FixedArrayBase::cast(source->elements());
  // Update elements if necessary.
  if (elements->length() > 0) {
    FixedArrayBase* elem = nullptr;
    if (elements->map() == *fixed_cow_array_map()) {
      elem = elements;
    } else if (source->HasDoubleElements()) {
      elem = *CopyFixedDoubleArray(
          handle(FixedDoubleArray::cast(elements), isolate()));
    } else {
      elem = *CopyFixedArray(handle(FixedArray::cast(elements), isolate()));
    }
    clone->set_elements(elem);
  }

  // Update properties if necessary.
  if (source->HasFastProperties()) {
    PropertyArray* properties = source->property_array();
    if (properties->length() > 0) {
      // TODO(gsathya): Do not copy hash code.
      Handle<PropertyArray> prop = CopyArrayWithMap(
          handle(properties, isolate()), handle(properties->map(), isolate()));
      clone->set_raw_properties_or_hash(*prop);
    }
  } else {
    Handle<FixedArray> properties(
        FixedArray::cast(source->property_dictionary()), isolate());
    Handle<FixedArray> prop = CopyFixedArray(properties);
    clone->set_raw_properties_or_hash(*prop);
  }
  return clone;
}

namespace {
template <typename T>
void initialize_length(T* array, int length) {
  array->set_length(length);
}

template <>
void initialize_length<PropertyArray>(PropertyArray* array, int length) {
  array->initialize_length(length);
}

}  // namespace

template <typename T>
Handle<T> Factory::CopyArrayWithMap(Handle<T> src, Handle<Map> map) {
  int len = src->length();
  HeapObject* obj = AllocateRawFixedArray(len, NOT_TENURED);
  obj->set_map_after_allocation(*map, SKIP_WRITE_BARRIER);

  T* result = T::cast(obj);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  if (mode == SKIP_WRITE_BARRIER) {
    // Eliminate the write barrier if possible.
    Heap::CopyBlock(obj->address() + kPointerSize,
                    src->address() + kPointerSize,
                    T::SizeFor(len) - kPointerSize);
  } else {
    // Slow case: Just copy the content one-by-one.
    initialize_length(result, len);
    for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  }
  return Handle<T>(result, isolate());
}

template <typename T>
Handle<T> Factory::CopyArrayAndGrow(Handle<T> src, int grow_by,
                                    PretenureFlag pretenure) {
  DCHECK_LT(0, grow_by);
  DCHECK_LE(grow_by, kMaxInt - src->length());
  int old_len = src->length();
  int new_len = old_len + grow_by;
  HeapObject* obj = AllocateRawFixedArray(new_len, pretenure);
  obj->set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);

  T* result = T::cast(obj);
  initialize_length(result, new_len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = obj->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < old_len; i++) result->set(i, src->get(i), mode);
  MemsetPointer(result->data_start() + old_len, *undefined_value(), grow_by);
  return Handle<T>(result, isolate());
}

Handle<FixedArray> Factory::CopyFixedArrayWithMap(Handle<FixedArray> array,
                                                  Handle<Map> map) {
  return CopyArrayWithMap(array, map);
}

Handle<FixedArray> Factory::CopyFixedArrayAndGrow(Handle<FixedArray> array,
                                                  int grow_by,
                                                  PretenureFlag pretenure) {
  return CopyArrayAndGrow(array, grow_by, pretenure);
}

Handle<WeakFixedArray> Factory::CopyWeakFixedArrayAndGrow(
    Handle<WeakFixedArray> src, int grow_by, PretenureFlag pretenure) {
  DCHECK(
      !src->IsTransitionArray());  // Compacted by GC, this code doesn't work.
  int old_len = src->length();
  int new_len = old_len + grow_by;
  DCHECK_GE(new_len, old_len);
  HeapObject* obj = AllocateRawFixedArray(new_len, pretenure);
  DCHECK_EQ(old_len, src->length());
  obj->set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);

  WeakFixedArray* result = WeakFixedArray::cast(obj);
  result->set_length(new_len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = obj->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < old_len; i++) result->Set(i, src->Get(i), mode);
  HeapObjectReference* undefined_reference =
      HeapObjectReference::Strong(ReadOnlyRoots(isolate()).undefined_value());
  MemsetPointer(result->data_start() + old_len, undefined_reference, grow_by);
  return Handle<WeakFixedArray>(result, isolate());
}

Handle<WeakArrayList> Factory::CopyWeakArrayListAndGrow(
    Handle<WeakArrayList> src, int grow_by, PretenureFlag pretenure) {
  int old_capacity = src->capacity();
  int new_capacity = old_capacity + grow_by;
  DCHECK_GE(new_capacity, old_capacity);
  HeapObject* obj = AllocateRawWeakArrayList(new_capacity, pretenure);
  obj->set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);

  WeakArrayList* result = WeakArrayList::cast(obj);
  result->set_length(src->length());
  result->set_capacity(new_capacity);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = obj->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < old_capacity; i++) result->Set(i, src->Get(i), mode);
  HeapObjectReference* undefined_reference =
      HeapObjectReference::Strong(ReadOnlyRoots(isolate()).undefined_value());
  MemsetPointer(result->data_start() + old_capacity, undefined_reference,
                grow_by);
  return Handle<WeakArrayList>(result, isolate());
}

Handle<PropertyArray> Factory::CopyPropertyArrayAndGrow(
    Handle<PropertyArray> array, int grow_by, PretenureFlag pretenure) {
  return CopyArrayAndGrow(array, grow_by, pretenure);
}

Handle<FixedArray> Factory::CopyFixedArrayUpTo(Handle<FixedArray> array,
                                               int new_len,
                                               PretenureFlag pretenure) {
  DCHECK_LE(0, new_len);
  DCHECK_LE(new_len, array->length());
  if (new_len == 0) return empty_fixed_array();

  HeapObject* obj = AllocateRawFixedArray(new_len, pretenure);
  obj->set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<FixedArray> result(FixedArray::cast(obj), isolate());
  result->set_length(new_len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < new_len; i++) result->set(i, array->get(i), mode);
  return result;
}

Handle<FixedArray> Factory::CopyFixedArray(Handle<FixedArray> array) {
  if (array->length() == 0) return array;
  return CopyArrayWithMap(array, handle(array->map(), isolate()));
}

Handle<FixedArray> Factory::CopyAndTenureFixedCOWArray(
    Handle<FixedArray> array) {
  DCHECK(Heap::InNewSpace(*array));
  Handle<FixedArray> result =
      CopyFixedArrayUpTo(array, array->length(), TENURED);

  // TODO(mvstanton): The map is set twice because of protection against calling
  // set() on a COW FixedArray. Issue v8:3221 created to track this, and
  // we might then be able to remove this whole method.
  result->set_map_after_allocation(*fixed_cow_array_map(), SKIP_WRITE_BARRIER);
  return result;
}

Handle<FixedDoubleArray> Factory::CopyFixedDoubleArray(
    Handle<FixedDoubleArray> array) {
  int len = array->length();
  if (len == 0) return array;
  Handle<FixedDoubleArray> result =
      Handle<FixedDoubleArray>::cast(NewFixedDoubleArray(len, NOT_TENURED));
  Heap::CopyBlock(
      result->address() + FixedDoubleArray::kLengthOffset,
      array->address() + FixedDoubleArray::kLengthOffset,
      FixedDoubleArray::SizeFor(len) - FixedDoubleArray::kLengthOffset);
  return result;
}

Handle<FeedbackVector> Factory::CopyFeedbackVector(
    Handle<FeedbackVector> array) {
  int len = array->length();
  HeapObject* obj = AllocateRawWithImmortalMap(
      FeedbackVector::SizeFor(len), NOT_TENURED, *feedback_vector_map());
  Handle<FeedbackVector> result(FeedbackVector::cast(obj), isolate());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  // Eliminate the write barrier if possible.
  if (mode == SKIP_WRITE_BARRIER) {
    Heap::CopyBlock(result->address() + kPointerSize,
                    result->address() + kPointerSize,
                    FeedbackVector::SizeFor(len) - kPointerSize);
  } else {
    // Slow case: Just copy the content one-by-one.
    result->set_shared_function_info(array->shared_function_info());
    result->set_optimized_code_weak_or_smi(array->optimized_code_weak_or_smi());
    result->set_invocation_count(array->invocation_count());
    result->set_profiler_ticks(array->profiler_ticks());
    result->set_deopt_count(array->deopt_count());
    for (int i = 0; i < len; i++) result->set(i, array->get(i), mode);
  }
  return result;
}

Handle<Object> Factory::NewNumber(double value, PretenureFlag pretenure) {
  // Materialize as a SMI if possible.
  int32_t int_value;
  if (DoubleToSmiInteger(value, &int_value)) {
    return handle(Smi::FromInt(int_value), isolate());
  }
  return NewHeapNumber(value, pretenure);
}

Handle<Object> Factory::NewNumberFromInt(int32_t value,
                                         PretenureFlag pretenure) {
  if (Smi::IsValid(value)) return handle(Smi::FromInt(value), isolate());
  // Bypass NewNumber to avoid various redundant checks.
  return NewHeapNumber(FastI2D(value), pretenure);
}

Handle<Object> Factory::NewNumberFromUint(uint32_t value,
                                          PretenureFlag pretenure) {
  int32_t int32v = static_cast<int32_t>(value);
  if (int32v >= 0 && Smi::IsValid(int32v)) {
    return handle(Smi::FromInt(int32v), isolate());
  }
  return NewHeapNumber(FastUI2D(value), pretenure);
}

Handle<HeapNumber> Factory::NewHeapNumber(PretenureFlag pretenure) {
  STATIC_ASSERT(HeapNumber::kSize <= kMaxRegularHeapObjectSize);
  Map* map = *heap_number_map();
  HeapObject* result = AllocateRawWithImmortalMap(HeapNumber::kSize, pretenure,
                                                  map, kDoubleUnaligned);
  return handle(HeapNumber::cast(result), isolate());
}

Handle<MutableHeapNumber> Factory::NewMutableHeapNumber(
    PretenureFlag pretenure) {
  STATIC_ASSERT(HeapNumber::kSize <= kMaxRegularHeapObjectSize);
  Map* map = *mutable_heap_number_map();
  HeapObject* result = AllocateRawWithImmortalMap(
      MutableHeapNumber::kSize, pretenure, map, kDoubleUnaligned);
  return handle(MutableHeapNumber::cast(result), isolate());
}

Handle<FreshlyAllocatedBigInt> Factory::NewBigInt(int length,
                                                  PretenureFlag pretenure) {
  if (length < 0 || length > BigInt::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid BigInt length");
  }
  HeapObject* result = AllocateRawWithImmortalMap(BigInt::SizeFor(length),
                                                  pretenure, *bigint_map());
  return handle(FreshlyAllocatedBigInt::cast(result), isolate());
}

Handle<Object> Factory::NewError(Handle<JSFunction> constructor,
                                 MessageTemplate::Template template_index,
                                 Handle<Object> arg0, Handle<Object> arg1,
                                 Handle<Object> arg2) {
  HandleScope scope(isolate());
  if (isolate()->bootstrapper()->IsActive()) {
    // During bootstrapping we cannot construct error objects.
    return scope.CloseAndEscape(NewStringFromAsciiChecked(
        MessageTemplate::TemplateString(template_index)));
  }

  if (arg0.is_null()) arg0 = undefined_value();
  if (arg1.is_null()) arg1 = undefined_value();
  if (arg2.is_null()) arg2 = undefined_value();

  Handle<Object> result;
  if (!ErrorUtils::MakeGenericError(isolate(), constructor, template_index,
                                    arg0, arg1, arg2, SKIP_NONE)
           .ToHandle(&result)) {
    // If an exception is thrown while
    // running the factory method, use the exception as the result.
    DCHECK(isolate()->has_pending_exception());
    result = handle(isolate()->pending_exception(), isolate());
    isolate()->clear_pending_exception();
  }

  return scope.CloseAndEscape(result);
}

Handle<Object> Factory::NewError(Handle<JSFunction> constructor,
                                 Handle<String> message) {
  // Construct a new error object. If an exception is thrown, use the exception
  // as the result.

  Handle<Object> no_caller;
  MaybeHandle<Object> maybe_error =
      ErrorUtils::Construct(isolate(), constructor, constructor, message,
                            SKIP_NONE, no_caller, false);
  if (maybe_error.is_null()) {
    DCHECK(isolate()->has_pending_exception());
    maybe_error = handle(isolate()->pending_exception(), isolate());
    isolate()->clear_pending_exception();
  }

  return maybe_error.ToHandleChecked();
}

Handle<Object> Factory::NewInvalidStringLengthError() {
  if (FLAG_abort_on_stack_or_string_length_overflow) {
    FATAL("Aborting on invalid string length");
  }
  // Invalidate the "string length" protector.
  if (isolate()->IsStringLengthOverflowIntact()) {
    isolate()->InvalidateStringLengthOverflowProtector();
  }
  return NewRangeError(MessageTemplate::kInvalidStringLength);
}

#define DEFINE_ERROR(NAME, name)                                              \
  Handle<Object> Factory::New##NAME(MessageTemplate::Template template_index, \
                                    Handle<Object> arg0, Handle<Object> arg1, \
                                    Handle<Object> arg2) {                    \
    return NewError(isolate()->name##_function(), template_index, arg0, arg1, \
                    arg2);                                                    \
  }
DEFINE_ERROR(Error, error)
DEFINE_ERROR(EvalError, eval_error)
DEFINE_ERROR(RangeError, range_error)
DEFINE_ERROR(ReferenceError, reference_error)
DEFINE_ERROR(SyntaxError, syntax_error)
DEFINE_ERROR(TypeError, type_error)
DEFINE_ERROR(WasmCompileError, wasm_compile_error)
DEFINE_ERROR(WasmLinkError, wasm_link_error)
DEFINE_ERROR(WasmRuntimeError, wasm_runtime_error)
#undef DEFINE_ERROR

Handle<JSFunction> Factory::NewFunction(Handle<Map> map,
                                        Handle<SharedFunctionInfo> info,
                                        Handle<Context> context,
                                        PretenureFlag pretenure) {
  Handle<JSFunction> function(JSFunction::cast(New(map, pretenure)), isolate());

  function->initialize_properties();
  function->initialize_elements();
  function->set_shared(*info);
  function->set_code(info->GetCode());
  function->set_context(*context);
  function->set_feedback_cell(*many_closures_cell());
  int header_size;
  if (map->has_prototype_slot()) {
    header_size = JSFunction::kSizeWithPrototype;
    function->set_prototype_or_initial_map(*the_hole_value());
  } else {
    header_size = JSFunction::kSizeWithoutPrototype;
  }
  InitializeJSObjectBody(function, map, header_size);
  return function;
}

Handle<JSFunction> Factory::NewFunctionForTest(Handle<String> name) {
  NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(
      name, isolate()->sloppy_function_map(), LanguageMode::kSloppy);
  Handle<JSFunction> result = NewFunction(args);
  DCHECK(is_sloppy(result->shared()->language_mode()));
  return result;
}

Handle<JSFunction> Factory::NewFunction(const NewFunctionArgs& args) {
  DCHECK(!args.name_.is_null());

  // Create the SharedFunctionInfo.
  Handle<NativeContext> context(isolate()->native_context());
  Handle<Map> map = args.GetMap(isolate());
  Handle<SharedFunctionInfo> info =
      NewSharedFunctionInfo(args.name_, args.maybe_exported_function_data_,
                            args.maybe_builtin_id_, kNormalFunction);

  // Proper language mode in shared function info will be set later.
  DCHECK(is_sloppy(info->language_mode()));
  DCHECK(!map->IsUndefined(isolate()));

#ifdef DEBUG
  if (isolate()->bootstrapper()->IsActive()) {
    Handle<Code> code;
    DCHECK(
        // During bootstrapping some of these maps could be not created yet.
        (*map == context->get(Context::STRICT_FUNCTION_MAP_INDEX)) ||
        (*map ==
         context->get(Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX)) ||
        (*map ==
         context->get(
             Context::STRICT_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX)) ||
        // Check if it's a creation of an empty or Proxy function during
        // bootstrapping.
        (args.maybe_builtin_id_ == Builtins::kEmptyFunction ||
         args.maybe_builtin_id_ == Builtins::kProxyConstructor));
  } else {
    DCHECK(
        (*map == *isolate()->sloppy_function_map()) ||
        (*map == *isolate()->sloppy_function_without_prototype_map()) ||
        (*map == *isolate()->sloppy_function_with_readonly_prototype_map()) ||
        (*map == *isolate()->strict_function_map()) ||
        (*map == *isolate()->strict_function_without_prototype_map()) ||
        (*map == *isolate()->native_function_map()));
  }
#endif

  Handle<JSFunction> result = NewFunction(map, info, context);

  if (args.should_set_prototype_) {
    result->set_prototype_or_initial_map(
        *args.maybe_prototype_.ToHandleChecked());
  }

  if (args.should_set_language_mode_) {
    result->shared()->set_language_mode(args.language_mode_);
  }

  if (args.should_create_and_set_initial_map_) {
    ElementsKind elements_kind;
    switch (args.type_) {
      case JS_ARRAY_TYPE:
        elements_kind = PACKED_SMI_ELEMENTS;
        break;
      case JS_ARGUMENTS_TYPE:
        elements_kind = PACKED_ELEMENTS;
        break;
      default:
        elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
        break;
    }
    Handle<Map> initial_map = NewMap(args.type_, args.instance_size_,
                                     elements_kind, args.inobject_properties_);
    result->shared()->set_expected_nof_properties(args.inobject_properties_);
    // TODO(littledan): Why do we have this is_generator test when
    // NewFunctionPrototype already handles finding an appropriately
    // shared prototype?
    Handle<Object> prototype = args.maybe_prototype_.ToHandleChecked();
    if (!IsResumableFunction(result->shared()->kind())) {
      if (prototype->IsTheHole(isolate())) {
        prototype = NewFunctionPrototype(result);
      }
    }
    JSFunction::SetInitialMap(result, initial_map, prototype);
  }

  return result;
}

Handle<JSObject> Factory::NewFunctionPrototype(Handle<JSFunction> function) {
  // Make sure to use globals from the function's context, since the function
  // can be from a different context.
  Handle<NativeContext> native_context(function->context()->native_context(),
                                       isolate());
  Handle<Map> new_map;
  if (V8_UNLIKELY(IsAsyncGeneratorFunction(function->shared()->kind()))) {
    new_map = handle(native_context->async_generator_object_prototype_map(),
                     isolate());
  } else if (IsResumableFunction(function->shared()->kind())) {
    // Generator and async function prototypes can share maps since they
    // don't have "constructor" properties.
    new_map =
        handle(native_context->generator_object_prototype_map(), isolate());
  } else {
    // Each function prototype gets a fresh map to avoid unwanted sharing of
    // maps between prototypes of different constructors.
    Handle<JSFunction> object_function(native_context->object_function(),
                                       isolate());
    DCHECK(object_function->has_initial_map());
    new_map = handle(object_function->initial_map(), isolate());
  }

  DCHECK(!new_map->is_prototype_map());
  Handle<JSObject> prototype = NewJSObjectFromMap(new_map);

  if (!IsResumableFunction(function->shared()->kind())) {
    JSObject::AddProperty(isolate(), prototype, constructor_string(), function,
                          DONT_ENUM);
  }

  return prototype;
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info, Handle<Context> context,
    PretenureFlag pretenure) {
  Handle<Map> initial_map(
      Map::cast(context->native_context()->get(info->function_map_index())),
      isolate());
  return NewFunctionFromSharedFunctionInfo(initial_map, info, context,
                                           pretenure);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info, Handle<Context> context,
    Handle<FeedbackCell> feedback_cell, PretenureFlag pretenure) {
  Handle<Map> initial_map(
      Map::cast(context->native_context()->get(info->function_map_index())),
      isolate());
  return NewFunctionFromSharedFunctionInfo(initial_map, info, context,
                                           feedback_cell, pretenure);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Context> context, PretenureFlag pretenure) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context, pretenure);

  // Give compiler a chance to pre-initialize.
  Compiler::PostInstantiation(result, pretenure);

  return result;
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Context> context, Handle<FeedbackCell> feedback_cell,
    PretenureFlag pretenure) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context, pretenure);

  // Bump the closure count that is encoded in the feedback cell's map.
  if (feedback_cell->map() == *no_closures_cell_map()) {
    feedback_cell->set_map(*one_closure_cell_map());
  } else if (feedback_cell->map() == *one_closure_cell_map()) {
    feedback_cell->set_map(*many_closures_cell_map());
  } else {
    DCHECK_EQ(feedback_cell->map(), *many_closures_cell_map());
  }

  // Check that the optimized code in the feedback cell wasn't marked for
  // deoptimization while not pointed to by any live JSFunction.
  if (feedback_cell->value()->IsFeedbackVector()) {
    FeedbackVector::cast(feedback_cell->value())
        ->EvictOptimizedCodeMarkedForDeoptimization(
            *info, "new function from shared function info");
  }
  result->set_feedback_cell(*feedback_cell);

  // Give compiler a chance to pre-initialize.
  Compiler::PostInstantiation(result, pretenure);

  return result;
}

Handle<ScopeInfo> Factory::NewScopeInfo(int length) {
  return NewFixedArrayWithMap<ScopeInfo>(Heap::kScopeInfoMapRootIndex, length,
                                         TENURED);
}

Handle<ModuleInfo> Factory::NewModuleInfo() {
  return NewFixedArrayWithMap<ModuleInfo>(Heap::kModuleInfoMapRootIndex,
                                          ModuleInfo::kLength, TENURED);
}

Handle<PreParsedScopeData> Factory::NewPreParsedScopeData(int length) {
  int size = PreParsedScopeData::SizeFor(length);
  Handle<PreParsedScopeData> result(
      PreParsedScopeData::cast(AllocateRawWithImmortalMap(
          size, TENURED, *pre_parsed_scope_data_map())),
      isolate());
  result->set_scope_data(PodArray<uint8_t>::cast(*empty_byte_array()));
  result->set_length(length);
  MemsetPointer(result->child_data_start(), *null_value(), length);

  result->clear_padding();
  return result;
}

Handle<UncompiledDataWithoutPreParsedScope>
Factory::NewUncompiledDataWithoutPreParsedScope(Handle<String> inferred_name,
                                                int32_t start_position,
                                                int32_t end_position,
                                                int32_t function_literal_id) {
  Handle<UncompiledDataWithoutPreParsedScope> result(
      UncompiledDataWithoutPreParsedScope::cast(
          New(uncompiled_data_without_pre_parsed_scope_map(), TENURED)),
      isolate());
  result->set_inferred_name(*inferred_name);
  result->set_start_position(start_position);
  result->set_end_position(end_position);
  result->set_function_literal_id(function_literal_id);

  result->clear_padding();
  return result;
}

Handle<UncompiledDataWithPreParsedScope>
Factory::NewUncompiledDataWithPreParsedScope(
    Handle<String> inferred_name, int32_t start_position, int32_t end_position,
    int32_t function_literal_id,
    Handle<PreParsedScopeData> pre_parsed_scope_data) {
  Handle<UncompiledDataWithPreParsedScope> result(
      UncompiledDataWithPreParsedScope::cast(
          New(uncompiled_data_with_pre_parsed_scope_map(), TENURED)),
      isolate());
  result->set_inferred_name(*inferred_name);
  result->set_start_position(start_position);
  result->set_end_position(end_position);
  result->set_function_literal_id(function_literal_id);
  result->set_pre_parsed_scope_data(*pre_parsed_scope_data);

  result->clear_padding();
  return result;
}

Handle<JSObject> Factory::NewExternal(void* value) {
  Handle<Foreign> foreign = NewForeign(reinterpret_cast<Address>(value));
  Handle<JSObject> external = NewJSObjectFromMap(external_map());
  external->SetEmbedderField(0, *foreign);
  return external;
}

Handle<CodeDataContainer> Factory::NewCodeDataContainer(int flags) {
  Handle<CodeDataContainer> data_container(
      CodeDataContainer::cast(New(code_data_container_map(), TENURED)),
      isolate());
  data_container->set_next_code_link(*undefined_value(), SKIP_WRITE_BARRIER);
  data_container->set_kind_specific_flags(flags);
  data_container->clear_padding();
  return data_container;
}

MaybeHandle<Code> Factory::TryNewCode(
    const CodeDesc& desc, Code::Kind kind, Handle<Object> self_ref,
    int32_t builtin_index, MaybeHandle<ByteArray> maybe_source_position_table,
    MaybeHandle<DeoptimizationData> maybe_deopt_data, Movability movability,
    uint32_t stub_key, bool is_turbofanned, int stack_slots,
    int safepoint_table_offset, int handler_table_offset) {
  // Allocate objects needed for code initialization.
  Handle<ByteArray> reloc_info = NewByteArray(desc.reloc_size, TENURED);
  Handle<CodeDataContainer> data_container = NewCodeDataContainer(0);
  Handle<ByteArray> source_position_table =
      maybe_source_position_table.is_null()
          ? empty_byte_array()
          : maybe_source_position_table.ToHandleChecked();
  Handle<DeoptimizationData> deopt_data =
      maybe_deopt_data.is_null() ? DeoptimizationData::Empty(isolate())
                                 : maybe_deopt_data.ToHandleChecked();
  Handle<Code> code;
  {
    int object_size = ComputeCodeObjectSize(desc);

    Heap* heap = isolate()->heap();
    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject* result =
        heap->AllocateRawWithLightRetry(object_size, CODE_SPACE);

    // Return an empty handle if we cannot allocate the code object.
    if (!result) return MaybeHandle<Code>();

    if (movability == kImmovable) {
      result = heap->EnsureImmovableCode(result, object_size);
    }

    // The code object has not been fully initialized yet.  We rely on the
    // fact that no allocation will happen from this point on.
    DisallowHeapAllocation no_gc;

    result->set_map_after_allocation(*code_map(), SKIP_WRITE_BARRIER);
    code = handle(Code::cast(result), isolate());

    InitializeCode(heap, code, object_size, desc, kind, self_ref, builtin_index,
                   source_position_table, deopt_data, reloc_info,
                   data_container, stub_key, is_turbofanned, stack_slots,
                   safepoint_table_offset, handler_table_offset);
  }
  // Flush the instruction cache after changing the permissions.
  code->FlushICache();

  return code;
}

Handle<Code> Factory::NewCode(
    const CodeDesc& desc, Code::Kind kind, Handle<Object> self_ref,
    int32_t builtin_index, MaybeHandle<ByteArray> maybe_source_position_table,
    MaybeHandle<DeoptimizationData> maybe_deopt_data, Movability movability,
    uint32_t stub_key, bool is_turbofanned, int stack_slots,
    int safepoint_table_offset, int handler_table_offset) {
  // Allocate objects needed for code initialization.
  Handle<ByteArray> reloc_info = NewByteArray(desc.reloc_size, TENURED);
  Handle<CodeDataContainer> data_container = NewCodeDataContainer(0);
  Handle<ByteArray> source_position_table =
      maybe_source_position_table.is_null()
          ? empty_byte_array()
          : maybe_source_position_table.ToHandleChecked();
  Handle<DeoptimizationData> deopt_data =
      maybe_deopt_data.is_null() ? DeoptimizationData::Empty(isolate())
                                 : maybe_deopt_data.ToHandleChecked();

  Handle<Code> code;
  {
    int object_size = ComputeCodeObjectSize(desc);

    Heap* heap = isolate()->heap();
    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject* result =
        heap->AllocateRawWithRetryOrFail(object_size, CODE_SPACE);

    if (movability == kImmovable) {
      result = heap->EnsureImmovableCode(result, object_size);
    }

    // The code object has not been fully initialized yet.  We rely on the
    // fact that no allocation will happen from this point on.
    DisallowHeapAllocation no_gc;

    result->set_map_after_allocation(*code_map(), SKIP_WRITE_BARRIER);
    code = handle(Code::cast(result), isolate());

    InitializeCode(heap, code, object_size, desc, kind, self_ref, builtin_index,
                   source_position_table, deopt_data, reloc_info,
                   data_container, stub_key, is_turbofanned, stack_slots,
                   safepoint_table_offset, handler_table_offset);
  }
  // Flush the instruction cache after changing the permissions.
  code->FlushICache();

  return code;
}

Handle<Code> Factory::NewCodeForDeserialization(uint32_t size) {
  DCHECK(IsAligned(static_cast<intptr_t>(size), kCodeAlignment));
  Heap* heap = isolate()->heap();
  HeapObject* result = heap->AllocateRawWithRetryOrFail(size, CODE_SPACE);
  // Unprotect the memory chunk of the object if it was not unprotected
  // already.
  heap->UnprotectAndRegisterMemoryChunk(result);
  heap->ZapCodeObject(result->address(), size);
  result->set_map_after_allocation(*code_map(), SKIP_WRITE_BARRIER);
  DCHECK(IsAligned(result->address(), kCodeAlignment));
  DCHECK(!heap->memory_allocator()->code_range()->valid() ||
         heap->memory_allocator()->code_range()->contains(result->address()) ||
         static_cast<int>(size) <= heap->code_space()->AreaSize());
  return handle(Code::cast(result), isolate());
}

Handle<Code> Factory::NewOffHeapTrampolineFor(Handle<Code> code,
                                              Address off_heap_entry) {
  CHECK(isolate()->serializer_enabled());
  CHECK_NOT_NULL(isolate()->embedded_blob());
  CHECK_NE(0, isolate()->embedded_blob_size());
  CHECK(Builtins::IsIsolateIndependentBuiltin(*code));

  Handle<Code> result =
      Builtins::GenerateOffHeapTrampolineFor(isolate(), off_heap_entry);

  // The trampoline code object must inherit specific flags from the original
  // builtin (e.g. the safepoint-table offset). We set them manually here.

  const bool set_is_off_heap_trampoline = true;
  const int stack_slots = code->has_safepoint_info() ? code->stack_slots() : 0;
  result->initialize_flags(code->kind(), code->has_unwinding_info(),
                           code->is_turbofanned(), stack_slots,
                           set_is_off_heap_trampoline);
  result->set_builtin_index(code->builtin_index());
  result->set_handler_table_offset(code->handler_table_offset());
  result->code_data_container()->set_kind_specific_flags(
      code->code_data_container()->kind_specific_flags());
  result->set_constant_pool_offset(code->constant_pool_offset());
  if (code->has_safepoint_info()) {
    result->set_safepoint_table_offset(code->safepoint_table_offset());
  }

  return result;
}

Handle<Code> Factory::CopyCode(Handle<Code> code) {
  Handle<CodeDataContainer> data_container =
      NewCodeDataContainer(code->code_data_container()->kind_specific_flags());

  Heap* heap = isolate()->heap();
  int obj_size = code->Size();
  HeapObject* result = heap->AllocateRawWithRetryOrFail(obj_size, CODE_SPACE);

  // Copy code object.
  Address old_addr = code->address();
  Address new_addr = result->address();
  Heap::CopyBlock(new_addr, old_addr, obj_size);
  Handle<Code> new_code(Code::cast(result), isolate());

  // Set the {CodeDataContainer}, it cannot be shared.
  new_code->set_code_data_container(*data_container);

  new_code->Relocate(new_addr - old_addr);
  // We have to iterate over the object and process its pointers when black
  // allocation is on.
  heap->incremental_marking()->ProcessBlackAllocatedObject(*new_code);
  // Record all references to embedded objects in the new code object.
  WriteBarrierForCode(*new_code);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) new_code->ObjectVerify(isolate());
#endif
  DCHECK(IsAligned(new_code->address(), kCodeAlignment));
  DCHECK(
      !heap->memory_allocator()->code_range()->valid() ||
      heap->memory_allocator()->code_range()->contains(new_code->address()) ||
      obj_size <= heap->code_space()->AreaSize());
  return new_code;
}

Handle<BytecodeArray> Factory::CopyBytecodeArray(
    Handle<BytecodeArray> bytecode_array) {
  int size = BytecodeArray::SizeFor(bytecode_array->length());
  HeapObject* result =
      AllocateRawWithImmortalMap(size, TENURED, *bytecode_array_map());

  Handle<BytecodeArray> copy(BytecodeArray::cast(result), isolate());
  copy->set_length(bytecode_array->length());
  copy->set_frame_size(bytecode_array->frame_size());
  copy->set_parameter_count(bytecode_array->parameter_count());
  copy->set_incoming_new_target_or_generator_register(
      bytecode_array->incoming_new_target_or_generator_register());
  copy->set_constant_pool(bytecode_array->constant_pool());
  copy->set_handler_table(bytecode_array->handler_table());
  copy->set_source_position_table(bytecode_array->source_position_table());
  copy->set_interrupt_budget(bytecode_array->interrupt_budget());
  copy->set_osr_loop_nesting_level(bytecode_array->osr_loop_nesting_level());
  copy->set_bytecode_age(bytecode_array->bytecode_age());
  bytecode_array->CopyBytecodesTo(*copy);
  return copy;
}

Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor,
                                      PretenureFlag pretenure) {
  JSFunction::EnsureHasInitialMap(constructor);
  Handle<Map> map(constructor->initial_map(), isolate());
  return NewJSObjectFromMap(map, pretenure);
}

Handle<JSObject> Factory::NewJSObjectWithNullProto(PretenureFlag pretenure) {
  Handle<JSObject> result =
      NewJSObject(isolate()->object_function(), pretenure);
  Handle<Map> new_map = Map::Copy(
      isolate(), Handle<Map>(result->map(), isolate()), "ObjectWithNullProto");
  Map::SetPrototype(isolate(), new_map, null_value());
  JSObject::MigrateToMap(result, new_map);
  return result;
}

Handle<JSGlobalObject> Factory::NewJSGlobalObject(
    Handle<JSFunction> constructor) {
  DCHECK(constructor->has_initial_map());
  Handle<Map> map(constructor->initial_map(), isolate());
  DCHECK(map->is_dictionary_map());

  // Make sure no field properties are described in the initial map.
  // This guarantees us that normalizing the properties does not
  // require us to change property values to PropertyCells.
  DCHECK_EQ(map->NextFreePropertyIndex(), 0);

  // Make sure we don't have a ton of pre-allocated slots in the
  // global objects. They will be unused once we normalize the object.
  DCHECK_EQ(map->UnusedPropertyFields(), 0);
  DCHECK_EQ(map->GetInObjectProperties(), 0);

  // Initial size of the backing store to avoid resize of the storage during
  // bootstrapping. The size differs between the JS global object ad the
  // builtins object.
  int initial_size = 64;

  // Allocate a dictionary object for backing storage.
  int at_least_space_for = map->NumberOfOwnDescriptors() * 2 + initial_size;
  Handle<GlobalDictionary> dictionary =
      GlobalDictionary::New(isolate(), at_least_space_for);

  // The global object might be created from an object template with accessors.
  // Fill these accessors into the dictionary.
  Handle<DescriptorArray> descs(map->instance_descriptors(), isolate());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    PropertyDetails details = descs->GetDetails(i);
    // Only accessors are expected.
    DCHECK_EQ(kAccessor, details.kind());
    PropertyDetails d(kAccessor, details.attributes(),
                      PropertyCellType::kMutable);
    Handle<Name> name(descs->GetKey(i), isolate());
    Handle<PropertyCell> cell = NewPropertyCell(name);
    cell->set_value(descs->GetStrongValue(i));
    // |dictionary| already contains enough space for all properties.
    USE(GlobalDictionary::Add(isolate(), dictionary, name, cell, d));
  }

  // Allocate the global object and initialize it with the backing store.
  Handle<JSGlobalObject> global(JSGlobalObject::cast(New(map, TENURED)),
                                isolate());
  InitializeJSObjectFromMap(global, dictionary, map);

  // Create a new map for the global object.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate(), map);
  new_map->set_may_have_interesting_symbols(true);
  new_map->set_is_dictionary_map(true);

  // Set up the global object as a normalized object.
  global->set_global_dictionary(*dictionary);
  global->synchronized_set_map(*new_map);

  // Make sure result is a global object with properties in dictionary.
  DCHECK(global->IsJSGlobalObject() && !global->HasFastProperties());
  return global;
}

void Factory::InitializeJSObjectFromMap(Handle<JSObject> obj,
                                        Handle<Object> properties,
                                        Handle<Map> map) {
  obj->set_raw_properties_or_hash(*properties);
  obj->initialize_elements();
  // TODO(1240798): Initialize the object's body using valid initial values
  // according to the object's initial map.  For example, if the map's
  // instance type is JS_ARRAY_TYPE, the length field should be initialized
  // to a number (e.g. Smi::kZero) and the elements initialized to a
  // fixed array (e.g. Heap::empty_fixed_array()).  Currently, the object
  // verification code has to cope with (temporarily) invalid objects.  See
  // for example, JSArray::JSArrayVerify).
  InitializeJSObjectBody(obj, map, JSObject::kHeaderSize);
}

void Factory::InitializeJSObjectBody(Handle<JSObject> obj, Handle<Map> map,
                                     int start_offset) {
  if (start_offset == map->instance_size()) return;
  DCHECK_LT(start_offset, map->instance_size());

  // We cannot always fill with one_pointer_filler_map because objects
  // created from API functions expect their embedder fields to be initialized
  // with undefined_value.
  // Pre-allocated fields need to be initialized with undefined_value as well
  // so that object accesses before the constructor completes (e.g. in the
  // debugger) will not cause a crash.

  // In case of Array subclassing the |map| could already be transitioned
  // to different elements kind from the initial map on which we track slack.
  bool in_progress = map->IsInobjectSlackTrackingInProgress();
  Object* filler;
  if (in_progress) {
    filler = *one_pointer_filler_map();
  } else {
    filler = *undefined_value();
  }
  obj->InitializeBody(*map, start_offset, *undefined_value(), filler);
  if (in_progress) {
    map->FindRootMap(isolate())->InobjectSlackTrackingStep(isolate());
  }
}

Handle<JSObject> Factory::NewJSObjectFromMap(
    Handle<Map> map, PretenureFlag pretenure,
    Handle<AllocationSite> allocation_site) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  DCHECK(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  DCHECK(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);

  HeapObject* obj =
      AllocateRawWithAllocationSite(map, pretenure, allocation_site);
  Handle<JSObject> js_obj(JSObject::cast(obj), isolate());

  InitializeJSObjectFromMap(js_obj, empty_fixed_array(), map);

  DCHECK(js_obj->HasFastElements() || js_obj->HasFixedTypedArrayElements() ||
         js_obj->HasFastStringWrapperElements() ||
         js_obj->HasFastArgumentsElements());
  return js_obj;
}

Handle<JSObject> Factory::NewSlowJSObjectFromMap(Handle<Map> map, int capacity,
                                                 PretenureFlag pretenure) {
  DCHECK(map->is_dictionary_map());
  Handle<NameDictionary> object_properties =
      NameDictionary::New(isolate(), capacity);
  Handle<JSObject> js_object = NewJSObjectFromMap(map, pretenure);
  js_object->set_raw_properties_or_hash(*object_properties);
  return js_object;
}

Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind,
                                    PretenureFlag pretenure) {
  NativeContext* native_context = isolate()->raw_native_context();
  Map* map = native_context->GetInitialJSArrayMap(elements_kind);
  if (map == nullptr) {
    JSFunction* array_function = native_context->array_function();
    map = array_function->initial_map();
  }
  return Handle<JSArray>::cast(
      NewJSObjectFromMap(handle(map, isolate()), pretenure));
}

Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind, int length,
                                    int capacity,
                                    ArrayStorageAllocationMode mode,
                                    PretenureFlag pretenure) {
  Handle<JSArray> array = NewJSArray(elements_kind, pretenure);
  NewJSArrayStorage(array, length, capacity, mode);
  return array;
}

Handle<JSArray> Factory::NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                                ElementsKind elements_kind,
                                                int length,
                                                PretenureFlag pretenure) {
  DCHECK(length <= elements->length());
  Handle<JSArray> array = NewJSArray(elements_kind, pretenure);

  array->set_elements(*elements);
  array->set_length(Smi::FromInt(length));
  JSObject::ValidateElements(*array);
  return array;
}

void Factory::NewJSArrayStorage(Handle<JSArray> array, int length, int capacity,
                                ArrayStorageAllocationMode mode) {
  DCHECK(capacity >= length);

  if (capacity == 0) {
    array->set_length(Smi::kZero);
    array->set_elements(*empty_fixed_array());
    return;
  }

  HandleScope inner_scope(isolate());
  Handle<FixedArrayBase> elms;
  ElementsKind elements_kind = array->GetElementsKind();
  if (IsDoubleElementsKind(elements_kind)) {
    if (mode == DONT_INITIALIZE_ARRAY_ELEMENTS) {
      elms = NewFixedDoubleArray(capacity);
    } else {
      DCHECK(mode == INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
      elms = NewFixedDoubleArrayWithHoles(capacity);
    }
  } else {
    DCHECK(IsSmiOrObjectElementsKind(elements_kind));
    if (mode == DONT_INITIALIZE_ARRAY_ELEMENTS) {
      elms = NewUninitializedFixedArray(capacity);
    } else {
      DCHECK(mode == INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
      elms = NewFixedArrayWithHoles(capacity);
    }
  }

  array->set_elements(*elms);
  array->set_length(Smi::FromInt(length));
}

Handle<JSWeakMap> Factory::NewJSWeakMap() {
  NativeContext* native_context = isolate()->raw_native_context();
  Handle<Map> map(native_context->js_weak_map_fun()->initial_map(), isolate());
  Handle<JSWeakMap> weakmap(JSWeakMap::cast(*NewJSObjectFromMap(map)),
                            isolate());
  {
    // Do not leak handles for the hash table, it would make entries strong.
    HandleScope scope(isolate());
    JSWeakCollection::Initialize(weakmap, isolate());
  }
  return weakmap;
}

Handle<JSModuleNamespace> Factory::NewJSModuleNamespace() {
  Handle<Map> map = isolate()->js_module_namespace_map();
  Handle<JSModuleNamespace> module_namespace(
      Handle<JSModuleNamespace>::cast(NewJSObjectFromMap(map)));
  FieldIndex index = FieldIndex::ForDescriptor(
      *map, JSModuleNamespace::kToStringTagFieldIndex);
  module_namespace->FastPropertyAtPut(index,
                                      ReadOnlyRoots(isolate()).Module_string());
  return module_namespace;
}

Handle<JSGeneratorObject> Factory::NewJSGeneratorObject(
    Handle<JSFunction> function) {
  DCHECK(IsResumableFunction(function->shared()->kind()));
  JSFunction::EnsureHasInitialMap(function);
  Handle<Map> map(function->initial_map(), isolate());

  DCHECK(map->instance_type() == JS_GENERATOR_OBJECT_TYPE ||
         map->instance_type() == JS_ASYNC_GENERATOR_OBJECT_TYPE);

  return Handle<JSGeneratorObject>::cast(NewJSObjectFromMap(map));
}

Handle<Module> Factory::NewModule(Handle<SharedFunctionInfo> code) {
  Handle<ModuleInfo> module_info(code->scope_info()->ModuleDescriptorInfo(),
                                 isolate());
  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), module_info->RegularExportCount());
  Handle<FixedArray> regular_exports =
      NewFixedArray(module_info->RegularExportCount());
  Handle<FixedArray> regular_imports =
      NewFixedArray(module_info->regular_imports()->length());
  int requested_modules_length = module_info->module_requests()->length();
  Handle<FixedArray> requested_modules =
      requested_modules_length > 0 ? NewFixedArray(requested_modules_length)
                                   : empty_fixed_array();

  ReadOnlyRoots roots(isolate());
  Handle<Module> module = Handle<Module>::cast(NewStruct(MODULE_TYPE, TENURED));
  module->set_code(*code);
  module->set_exports(*exports);
  module->set_regular_exports(*regular_exports);
  module->set_regular_imports(*regular_imports);
  module->set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module->set_module_namespace(roots.undefined_value());
  module->set_requested_modules(*requested_modules);
  module->set_script(Script::cast(code->script()));
  module->set_status(Module::kUninstantiated);
  module->set_exception(roots.the_hole_value());
  module->set_import_meta(roots.the_hole_value());
  module->set_dfs_index(-1);
  module->set_dfs_ancestor_index(-1);
  return module;
}

Handle<JSArrayBuffer> Factory::NewJSArrayBuffer(SharedFlag shared,
                                                PretenureFlag pretenure) {
  Handle<JSFunction> array_buffer_fun(
      shared == SharedFlag::kShared
          ? isolate()->native_context()->shared_array_buffer_fun()
          : isolate()->native_context()->array_buffer_fun(),
      isolate());
  Handle<Map> map(array_buffer_fun->initial_map(), isolate());
  return Handle<JSArrayBuffer>::cast(NewJSObjectFromMap(map, pretenure));
}

Handle<JSIteratorResult> Factory::NewJSIteratorResult(Handle<Object> value,
                                                      bool done) {
  Handle<Map> map(isolate()->native_context()->iterator_result_map(),
                  isolate());
  Handle<JSIteratorResult> js_iter_result =
      Handle<JSIteratorResult>::cast(NewJSObjectFromMap(map));
  js_iter_result->set_value(*value);
  js_iter_result->set_done(*ToBoolean(done));
  return js_iter_result;
}

Handle<JSAsyncFromSyncIterator> Factory::NewJSAsyncFromSyncIterator(
    Handle<JSReceiver> sync_iterator, Handle<Object> next) {
  Handle<Map> map(isolate()->native_context()->async_from_sync_iterator_map(),
                  isolate());
  Handle<JSAsyncFromSyncIterator> iterator =
      Handle<JSAsyncFromSyncIterator>::cast(NewJSObjectFromMap(map));

  iterator->set_sync_iterator(*sync_iterator);
  iterator->set_next(*next);
  return iterator;
}

Handle<JSMap> Factory::NewJSMap() {
  Handle<Map> map(isolate()->native_context()->js_map_map(), isolate());
  Handle<JSMap> js_map = Handle<JSMap>::cast(NewJSObjectFromMap(map));
  JSMap::Initialize(js_map, isolate());
  return js_map;
}

Handle<JSSet> Factory::NewJSSet() {
  Handle<Map> map(isolate()->native_context()->js_set_map(), isolate());
  Handle<JSSet> js_set = Handle<JSSet>::cast(NewJSObjectFromMap(map));
  JSSet::Initialize(js_set, isolate());
  return js_set;
}

Handle<JSMapIterator> Factory::NewJSMapIterator(Handle<Map> map,
                                                Handle<OrderedHashMap> table,
                                                int index) {
  Handle<JSMapIterator> result =
      Handle<JSMapIterator>::cast(NewJSObjectFromMap(map));
  result->set_table(*table);
  result->set_index(Smi::FromInt(index));
  return result;
}

Handle<JSSetIterator> Factory::NewJSSetIterator(Handle<Map> map,
                                                Handle<OrderedHashSet> table,
                                                int index) {
  Handle<JSSetIterator> result =
      Handle<JSSetIterator>::cast(NewJSObjectFromMap(map));
  result->set_table(*table);
  result->set_index(Smi::FromInt(index));
  return result;
}

void Factory::TypeAndSizeForElementsKind(ElementsKind kind,
                                         ExternalArrayType* array_type,
                                         size_t* element_size) {
  switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    *array_type = kExternal##Type##Array;         \
    *element_size = sizeof(ctype);                \
    break;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      UNREACHABLE();
  }
}

namespace {

static void ForFixedTypedArray(ExternalArrayType array_type,
                               size_t* element_size,
                               ElementsKind* element_kind) {
  switch (array_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    *element_size = sizeof(ctype);                \
    *element_kind = TYPE##_ELEMENTS;              \
    return;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

JSFunction* GetTypedArrayFun(ExternalArrayType type, Isolate* isolate) {
  NativeContext* native_context = isolate->context()->native_context();
  switch (type) {
#define TYPED_ARRAY_FUN(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                   \
    return native_context->type##_array_fun();

    TYPED_ARRAYS(TYPED_ARRAY_FUN)
#undef TYPED_ARRAY_FUN
  }
  UNREACHABLE();
}

JSFunction* GetTypedArrayFun(ElementsKind elements_kind, Isolate* isolate) {
  NativeContext* native_context = isolate->context()->native_context();
  switch (elements_kind) {
#define TYPED_ARRAY_FUN(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                          \
    return native_context->type##_array_fun();

    TYPED_ARRAYS(TYPED_ARRAY_FUN)
#undef TYPED_ARRAY_FUN

    default:
      UNREACHABLE();
  }
}

void SetupArrayBufferView(i::Isolate* isolate,
                          i::Handle<i::JSArrayBufferView> obj,
                          i::Handle<i::JSArrayBuffer> buffer,
                          size_t byte_offset, size_t byte_length,
                          PretenureFlag pretenure = NOT_TENURED) {
  DCHECK(byte_offset + byte_length <=
         static_cast<size_t>(buffer->byte_length()->Number()));

  DCHECK_EQ(obj->GetEmbedderFieldCount(),
            v8::ArrayBufferView::kEmbedderFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kEmbedderFieldCount; i++) {
    obj->SetEmbedderField(i, Smi::kZero);
  }

  obj->set_buffer(*buffer);

  i::Handle<i::Object> byte_offset_object =
      isolate->factory()->NewNumberFromSize(byte_offset, pretenure);
  obj->set_byte_offset(*byte_offset_object);

  i::Handle<i::Object> byte_length_object =
      isolate->factory()->NewNumberFromSize(byte_length, pretenure);
  obj->set_byte_length(*byte_length_object);
}

}  // namespace

Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              PretenureFlag pretenure) {
  Handle<JSFunction> typed_array_fun(GetTypedArrayFun(type, isolate()),
                                     isolate());
  Handle<Map> map(typed_array_fun->initial_map(), isolate());
  return Handle<JSTypedArray>::cast(NewJSObjectFromMap(map, pretenure));
}

Handle<JSTypedArray> Factory::NewJSTypedArray(ElementsKind elements_kind,
                                              PretenureFlag pretenure) {
  Handle<JSFunction> typed_array_fun(GetTypedArrayFun(elements_kind, isolate()),
                                     isolate());
  Handle<Map> map(typed_array_fun->initial_map(), isolate());
  return Handle<JSTypedArray>::cast(NewJSObjectFromMap(map, pretenure));
}

Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              Handle<JSArrayBuffer> buffer,
                                              size_t byte_offset, size_t length,
                                              PretenureFlag pretenure) {
  Handle<JSTypedArray> obj = NewJSTypedArray(type, pretenure);

  size_t element_size;
  ElementsKind elements_kind;
  ForFixedTypedArray(type, &element_size, &elements_kind);

  CHECK_EQ(byte_offset % element_size, 0);

  CHECK(length <= (std::numeric_limits<size_t>::max() / element_size));
  // TODO(7881): Smi length check
  CHECK(length <= static_cast<size_t>(Smi::kMaxValue));
  size_t byte_length = length * element_size;
  SetupArrayBufferView(isolate(), obj, buffer, byte_offset, byte_length,
                       pretenure);

  Handle<Object> length_object = NewNumberFromSize(length, pretenure);
  obj->set_length(*length_object);

  Handle<FixedTypedArrayBase> elements = NewFixedTypedArrayWithExternalPointer(
      static_cast<int>(length), type,
      static_cast<uint8_t*>(buffer->backing_store()) + byte_offset, pretenure);
  Handle<Map> map = JSObject::GetElementsTransitionMap(obj, elements_kind);
  JSObject::SetMapAndElements(obj, map, elements);
  return obj;
}

Handle<JSTypedArray> Factory::NewJSTypedArray(ElementsKind elements_kind,
                                              size_t number_of_elements,
                                              PretenureFlag pretenure) {
  Handle<JSTypedArray> obj = NewJSTypedArray(elements_kind, pretenure);
  DCHECK_EQ(obj->GetEmbedderFieldCount(),
            v8::ArrayBufferView::kEmbedderFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kEmbedderFieldCount; i++) {
    obj->SetEmbedderField(i, Smi::kZero);
  }

  size_t element_size;
  ExternalArrayType array_type;
  TypeAndSizeForElementsKind(elements_kind, &array_type, &element_size);

  CHECK(number_of_elements <=
        (std::numeric_limits<size_t>::max() / element_size));
  // TODO(7881): Smi length check
  CHECK(number_of_elements <= static_cast<size_t>(Smi::kMaxValue));
  size_t byte_length = number_of_elements * element_size;

  obj->set_byte_offset(Smi::kZero);
  i::Handle<i::Object> byte_length_object =
      NewNumberFromSize(byte_length, pretenure);
  obj->set_byte_length(*byte_length_object);
  Handle<Object> length_object =
      NewNumberFromSize(number_of_elements, pretenure);
  obj->set_length(*length_object);

  Handle<JSArrayBuffer> buffer =
      NewJSArrayBuffer(SharedFlag::kNotShared, pretenure);
  JSArrayBuffer::Setup(buffer, isolate(), true, nullptr, byte_length,
                       SharedFlag::kNotShared);
  obj->set_buffer(*buffer);
  Handle<FixedTypedArrayBase> elements = NewFixedTypedArray(
      number_of_elements, byte_length, array_type, true, pretenure);
  obj->set_elements(*elements);
  return obj;
}

Handle<JSDataView> Factory::NewJSDataView(Handle<JSArrayBuffer> buffer,
                                          size_t byte_offset,
                                          size_t byte_length) {
  Handle<Map> map(isolate()->native_context()->data_view_fun()->initial_map(),
                  isolate());
  Handle<JSDataView> obj = Handle<JSDataView>::cast(NewJSObjectFromMap(map));
  SetupArrayBufferView(isolate(), obj, buffer, byte_offset, byte_length);
  return obj;
}

MaybeHandle<JSBoundFunction> Factory::NewJSBoundFunction(
    Handle<JSReceiver> target_function, Handle<Object> bound_this,
    Vector<Handle<Object>> bound_args) {
  DCHECK(target_function->IsCallable());
  STATIC_ASSERT(Code::kMaxArguments <= FixedArray::kMaxLength);
  if (bound_args.length() >= Code::kMaxArguments) {
    THROW_NEW_ERROR(isolate(),
                    NewRangeError(MessageTemplate::kTooManyArguments),
                    JSBoundFunction);
  }

  // Determine the prototype of the {target_function}.
  Handle<Object> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), prototype,
      JSReceiver::GetPrototype(isolate(), target_function), JSBoundFunction);

  SaveContext save(isolate());
  isolate()->set_context(*target_function->GetCreationContext());

  // Create the [[BoundArguments]] for the result.
  Handle<FixedArray> bound_arguments;
  if (bound_args.length() == 0) {
    bound_arguments = empty_fixed_array();
  } else {
    bound_arguments = NewFixedArray(bound_args.length());
    for (int i = 0; i < bound_args.length(); ++i) {
      bound_arguments->set(i, *bound_args[i]);
    }
  }

  // Setup the map for the JSBoundFunction instance.
  Handle<Map> map = target_function->IsConstructor()
                        ? isolate()->bound_function_with_constructor_map()
                        : isolate()->bound_function_without_constructor_map();
  if (map->prototype() != *prototype) {
    map = Map::TransitionToPrototype(isolate(), map, prototype);
  }
  DCHECK_EQ(target_function->IsConstructor(), map->is_constructor());

  // Setup the JSBoundFunction instance.
  Handle<JSBoundFunction> result =
      Handle<JSBoundFunction>::cast(NewJSObjectFromMap(map));
  result->set_bound_target_function(*target_function);
  result->set_bound_this(*bound_this);
  result->set_bound_arguments(*bound_arguments);
  return result;
}

// ES6 section 9.5.15 ProxyCreate (target, handler)
Handle<JSProxy> Factory::NewJSProxy(Handle<JSReceiver> target,
                                    Handle<JSReceiver> handler) {
  // Allocate the proxy object.
  Handle<Map> map;
  if (target->IsCallable()) {
    if (target->IsConstructor()) {
      map = Handle<Map>(isolate()->proxy_constructor_map());
    } else {
      map = Handle<Map>(isolate()->proxy_callable_map());
    }
  } else {
    map = Handle<Map>(isolate()->proxy_map());
  }
  DCHECK(map->prototype()->IsNull(isolate()));
  Handle<JSProxy> result(JSProxy::cast(New(map, NOT_TENURED)), isolate());
  result->initialize_properties();
  result->set_target(*target);
  result->set_handler(*handler);
  return result;
}

Handle<JSGlobalProxy> Factory::NewUninitializedJSGlobalProxy(int size) {
  // Create an empty shell of a JSGlobalProxy that needs to be reinitialized
  // via ReinitializeJSGlobalProxy later.
  Handle<Map> map = NewMap(JS_GLOBAL_PROXY_TYPE, size);
  // Maintain invariant expected from any JSGlobalProxy.
  map->set_is_access_check_needed(true);
  map->set_may_have_interesting_symbols(true);
  return Handle<JSGlobalProxy>::cast(NewJSObjectFromMap(map, NOT_TENURED));
}

void Factory::ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> object,
                                        Handle<JSFunction> constructor) {
  DCHECK(constructor->has_initial_map());
  Handle<Map> map(constructor->initial_map(), isolate());
  Handle<Map> old_map(object->map(), isolate());

  // The proxy's hash should be retained across reinitialization.
  Handle<Object> raw_properties_or_hash(object->raw_properties_or_hash(),
                                        isolate());

  if (old_map->is_prototype_map()) {
    map = Map::Copy(isolate(), map, "CopyAsPrototypeForJSGlobalProxy");
    map->set_is_prototype_map(true);
  }
  JSObject::NotifyMapChange(old_map, map, isolate());
  old_map->NotifyLeafMapLayoutChange(isolate());

  // Check that the already allocated object has the same size and type as
  // objects allocated using the constructor.
  DCHECK(map->instance_size() == old_map->instance_size());
  DCHECK(map->instance_type() == old_map->instance_type());

  // In order to keep heap in consistent state there must be no allocations
  // before object re-initialization is finished.
  DisallowHeapAllocation no_allocation;

  // Reset the map for the object.
  object->synchronized_set_map(*map);

  // Reinitialize the object from the constructor map.
  InitializeJSObjectFromMap(object, raw_properties_or_hash, map);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForLiteral(
    FunctionLiteral* literal, Handle<Script> script, bool is_toplevel) {
  FunctionKind kind = literal->kind();
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfoForBuiltin(
      literal->name(), Builtins::kCompileLazy, kind);
  SharedFunctionInfo::InitFromFunctionLiteral(shared, literal, is_toplevel);
  SharedFunctionInfo::SetScript(shared, script, literal->function_literal_id(),
                                false);
  return shared;
}

Handle<JSMessageObject> Factory::NewJSMessageObject(
    MessageTemplate::Template message, Handle<Object> argument,
    int start_position, int end_position, Handle<Script> script,
    Handle<Object> stack_frames) {
  Handle<Map> map = message_object_map();
  Handle<JSMessageObject> message_obj(
      JSMessageObject::cast(New(map, NOT_TENURED)), isolate());
  message_obj->set_raw_properties_or_hash(*empty_fixed_array(),
                                          SKIP_WRITE_BARRIER);
  message_obj->initialize_elements();
  message_obj->set_elements(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message_obj->set_type(message);
  message_obj->set_argument(*argument);
  message_obj->set_start_position(start_position);
  message_obj->set_end_position(end_position);
  message_obj->set_script(*script);
  message_obj->set_stack_frames(*stack_frames);
  message_obj->set_error_level(v8::Isolate::kMessageError);
  return message_obj;
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForApiFunction(
    MaybeHandle<String> maybe_name,
    Handle<FunctionTemplateInfo> function_template_info, FunctionKind kind) {
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo(
      maybe_name, function_template_info, Builtins::kNoBuiltinId, kind);
  return shared;
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForBuiltin(
    MaybeHandle<String> maybe_name, int builtin_index, FunctionKind kind) {
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo(
      maybe_name, MaybeHandle<Code>(), builtin_index, kind);
  return shared;
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    MaybeHandle<String> maybe_name, MaybeHandle<HeapObject> maybe_function_data,
    int maybe_builtin_index, FunctionKind kind) {
  // Function names are assumed to be flat elsewhere. Must flatten before
  // allocating SharedFunctionInfo to avoid GC seeing the uninitialized SFI.
  Handle<String> shared_name;
  bool has_shared_name = maybe_name.ToHandle(&shared_name);
  if (has_shared_name) {
    shared_name = String::Flatten(isolate(), shared_name, TENURED);
  }

  Handle<Map> map = shared_function_info_map();
  Handle<SharedFunctionInfo> share(SharedFunctionInfo::cast(New(map, TENURED)),
                                   isolate());
  {
    DisallowHeapAllocation no_allocation;

    // Set pointer fields.
    share->set_name_or_scope_info(
        has_shared_name ? *shared_name
                        : SharedFunctionInfo::kNoSharedNameSentinel);
    Handle<HeapObject> function_data;
    if (maybe_function_data.ToHandle(&function_data)) {
      // If we pass function_data then we shouldn't pass a builtin index, and
      // the function_data should not be code with a builtin.
      DCHECK(!Builtins::IsBuiltinId(maybe_builtin_index));
      DCHECK_IMPLIES(function_data->IsCode(),
                     !Code::cast(*function_data)->is_builtin());
      share->set_function_data(*function_data);
    } else if (Builtins::IsBuiltinId(maybe_builtin_index)) {
      DCHECK_NE(maybe_builtin_index, Builtins::kDeserializeLazy);
      share->set_builtin_id(maybe_builtin_index);
    } else {
      share->set_builtin_id(Builtins::kIllegal);
    }
    // Generally functions won't have feedback, unless they have been created
    // from a FunctionLiteral. Those can just reset this field to keep the
    // SharedFunctionInfo in a consistent state.
    if (maybe_builtin_index == Builtins::kCompileLazy) {
      share->set_raw_outer_scope_info_or_feedback_metadata(*the_hole_value(),
                                                           SKIP_WRITE_BARRIER);
    } else {
      share->set_raw_outer_scope_info_or_feedback_metadata(
          *empty_feedback_metadata(), SKIP_WRITE_BARRIER);
    }
    share->set_script_or_debug_info(*undefined_value(), SKIP_WRITE_BARRIER);
#if V8_SFI_HAS_UNIQUE_ID
    share->set_unique_id(isolate()->GetNextUniqueSharedFunctionInfoId());
#endif

    // Set integer fields (smi or int, depending on the architecture).
    share->set_length(0);
    share->set_internal_formal_parameter_count(0);
    share->set_expected_nof_properties(0);
    share->set_builtin_function_id(
        BuiltinFunctionId::kInvalidBuiltinFunctionId);
    share->set_raw_function_token_offset(0);
    // All flags default to false or 0.
    share->set_flags(0);
    share->CalculateConstructAsBuiltin();
    share->set_kind(kind);

    share->clear_padding();
  }
  // Link into the list.
  Handle<WeakArrayList> noscript_list = noscript_shared_function_infos();
  noscript_list = WeakArrayList::AddToEnd(isolate(), noscript_list,
                                          MaybeObjectHandle::Weak(share));
  isolate()->heap()->set_noscript_shared_function_infos(*noscript_list);

#ifdef VERIFY_HEAP
  share->SharedFunctionInfoVerify(isolate());
#endif
  return share;
}

namespace {
inline int NumberToStringCacheHash(Handle<FixedArray> cache, Smi* number) {
  int mask = (cache->length() >> 1) - 1;
  return number->value() & mask;
}
inline int NumberToStringCacheHash(Handle<FixedArray> cache, double number) {
  int mask = (cache->length() >> 1) - 1;
  int64_t bits = bit_cast<int64_t>(number);
  return (static_cast<int>(bits) ^ static_cast<int>(bits >> 32)) & mask;
}
}  // namespace

Handle<String> Factory::NumberToStringCacheSet(Handle<Object> number, int hash,
                                               const char* string,
                                               bool check_cache) {
  // We tenure the allocated string since it is referenced from the
  // number-string cache which lives in the old space.
  Handle<String> js_string =
      NewStringFromAsciiChecked(string, check_cache ? TENURED : NOT_TENURED);
  if (!check_cache) return js_string;

  if (!number_string_cache()->get(hash * 2)->IsUndefined(isolate())) {
    int full_size = isolate()->heap()->MaxNumberToStringCacheSize();
    if (number_string_cache()->length() != full_size) {
      Handle<FixedArray> new_cache = NewFixedArray(full_size, TENURED);
      isolate()->heap()->set_number_string_cache(*new_cache);
      return js_string;
    }
  }
  number_string_cache()->set(hash * 2, *number);
  number_string_cache()->set(hash * 2 + 1, *js_string);
  return js_string;
}

Handle<Object> Factory::NumberToStringCacheGet(Object* number, int hash) {
  DisallowHeapAllocation no_gc;
  Object* key = number_string_cache()->get(hash * 2);
  if (key == number || (key->IsHeapNumber() && number->IsHeapNumber() &&
                        key->Number() == number->Number())) {
    return Handle<String>(
        String::cast(number_string_cache()->get(hash * 2 + 1)), isolate());
  }
  return undefined_value();
}

Handle<String> Factory::NumberToString(Handle<Object> number,
                                       bool check_cache) {
  if (number->IsSmi()) return NumberToString(Smi::cast(*number), check_cache);

  double double_value = Handle<HeapNumber>::cast(number)->value();
  // Try to canonicalize doubles.
  int smi_value;
  if (DoubleToSmiInteger(double_value, &smi_value)) {
    return NumberToString(Smi::FromInt(smi_value), check_cache);
  }

  int hash = 0;
  if (check_cache) {
    hash = NumberToStringCacheHash(number_string_cache(), double_value);
    Handle<Object> cached = NumberToStringCacheGet(*number, hash);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  const char* string = DoubleToCString(double_value, buffer);

  return NumberToStringCacheSet(number, hash, string, check_cache);
}

Handle<String> Factory::NumberToString(Smi* number, bool check_cache) {
  int hash = 0;
  if (check_cache) {
    hash = NumberToStringCacheHash(number_string_cache(), number);
    Handle<Object> cached = NumberToStringCacheGet(number, hash);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  const char* string = IntToCString(number->value(), buffer);

  return NumberToStringCacheSet(handle(number, isolate()), hash, string,
                                check_cache);
}

Handle<DebugInfo> Factory::NewDebugInfo(Handle<SharedFunctionInfo> shared) {
  DCHECK(!shared->HasDebugInfo());
  Heap* heap = isolate()->heap();

  Handle<DebugInfo> debug_info =
      Handle<DebugInfo>::cast(NewStruct(DEBUG_INFO_TYPE, TENURED));
  debug_info->set_flags(DebugInfo::kNone);
  debug_info->set_shared(*shared);
  debug_info->set_debugger_hints(0);
  DCHECK_EQ(DebugInfo::kNoDebuggingId, debug_info->debugging_id());
  DCHECK(!shared->HasDebugInfo());
  debug_info->set_script(shared->script_or_debug_info());
  debug_info->set_original_bytecode_array(
      ReadOnlyRoots(heap).undefined_value());
  debug_info->set_break_points(ReadOnlyRoots(heap).empty_fixed_array());

  // Link debug info to function.
  shared->SetDebugInfo(*debug_info);

  return debug_info;
}

Handle<CoverageInfo> Factory::NewCoverageInfo(
    const ZoneVector<SourceRange>& slots) {
  const int slot_count = static_cast<int>(slots.size());

  const int length = CoverageInfo::FixedArrayLengthForSlotCount(slot_count);
  Handle<CoverageInfo> info =
      Handle<CoverageInfo>::cast(NewUninitializedFixedArray(length));

  for (int i = 0; i < slot_count; i++) {
    SourceRange range = slots[i];
    info->InitializeSlot(i, range.start, range.end);
  }

  return info;
}

Handle<BreakPointInfo> Factory::NewBreakPointInfo(int source_position) {
  Handle<BreakPointInfo> new_break_point_info =
      Handle<BreakPointInfo>::cast(NewStruct(TUPLE2_TYPE, TENURED));
  new_break_point_info->set_source_position(source_position);
  new_break_point_info->set_break_points(*undefined_value());
  return new_break_point_info;
}

Handle<BreakPoint> Factory::NewBreakPoint(int id, Handle<String> condition) {
  Handle<BreakPoint> new_break_point =
      Handle<BreakPoint>::cast(NewStruct(TUPLE2_TYPE, TENURED));
  new_break_point->set_id(id);
  new_break_point->set_condition(*condition);
  return new_break_point;
}

Handle<StackFrameInfo> Factory::NewStackFrameInfo() {
  Handle<StackFrameInfo> stack_frame_info = Handle<StackFrameInfo>::cast(
      NewStruct(STACK_FRAME_INFO_TYPE, NOT_TENURED));
  stack_frame_info->set_line_number(0);
  stack_frame_info->set_column_number(0);
  stack_frame_info->set_script_id(0);
  stack_frame_info->set_script_name(Smi::kZero);
  stack_frame_info->set_script_name_or_source_url(Smi::kZero);
  stack_frame_info->set_function_name(Smi::kZero);
  stack_frame_info->set_flag(0);
  return stack_frame_info;
}

Handle<SourcePositionTableWithFrameCache>
Factory::NewSourcePositionTableWithFrameCache(
    Handle<ByteArray> source_position_table,
    Handle<SimpleNumberDictionary> stack_frame_cache) {
  Handle<SourcePositionTableWithFrameCache>
      source_position_table_with_frame_cache =
          Handle<SourcePositionTableWithFrameCache>::cast(
              NewStruct(TUPLE2_TYPE, TENURED));
  source_position_table_with_frame_cache->set_source_position_table(
      *source_position_table);
  source_position_table_with_frame_cache->set_stack_frame_cache(
      *stack_frame_cache);
  return source_position_table_with_frame_cache;
}

Handle<JSObject> Factory::NewArgumentsObject(Handle<JSFunction> callee,
                                             int length) {
  bool strict_mode_callee = is_strict(callee->shared()->language_mode()) ||
                            !callee->shared()->has_simple_parameters();
  Handle<Map> map = strict_mode_callee ? isolate()->strict_arguments_map()
                                       : isolate()->sloppy_arguments_map();
  AllocationSiteUsageContext context(isolate(), Handle<AllocationSite>(),
                                     false);
  DCHECK(!isolate()->has_pending_exception());
  Handle<JSObject> result = NewJSObjectFromMap(map);
  Handle<Smi> value(Smi::FromInt(length), isolate());
  Object::SetProperty(isolate(), result, length_string(), value,
                      LanguageMode::kStrict)
      .Assert();
  if (!strict_mode_callee) {
    Object::SetProperty(isolate(), result, callee_string(), callee,
                        LanguageMode::kStrict)
        .Assert();
  }
  return result;
}

Handle<Map> Factory::ObjectLiteralMapFromCache(Handle<NativeContext> context,
                                               int number_of_properties) {
  if (number_of_properties == 0) {
    // Reuse the initial map of the Object function if the literal has no
    // predeclared properties.
    return handle(context->object_function()->initial_map(), isolate());
  }

  // We do not cache maps for too many properties or when running builtin code.
  if (isolate()->bootstrapper()->IsActive()) {
    return Map::Create(isolate(), number_of_properties);
  }

  // Use initial slow object proto map for too many properties.
  const int kMapCacheSize = 128;
  if (number_of_properties > kMapCacheSize) {
    return handle(context->slow_object_with_object_prototype_map(), isolate());
  }

  int cache_index = number_of_properties - 1;
  Handle<Object> maybe_cache(context->map_cache(), isolate());
  if (maybe_cache->IsUndefined(isolate())) {
    // Allocate the new map cache for the native context.
    maybe_cache = NewWeakFixedArray(kMapCacheSize, TENURED);
    context->set_map_cache(*maybe_cache);
  } else {
    // Check to see whether there is a matching element in the cache.
    Handle<WeakFixedArray> cache = Handle<WeakFixedArray>::cast(maybe_cache);
    MaybeObject* result = cache->Get(cache_index);
    HeapObject* heap_object;
    if (result->ToWeakHeapObject(&heap_object)) {
      Map* map = Map::cast(heap_object);
      DCHECK(!map->is_dictionary_map());
      return handle(map, isolate());
    }
  }

  // Create a new map and add it to the cache.
  Handle<WeakFixedArray> cache = Handle<WeakFixedArray>::cast(maybe_cache);
  Handle<Map> map = Map::Create(isolate(), number_of_properties);
  DCHECK(!map->is_dictionary_map());
  cache->Set(cache_index, HeapObjectReference::Weak(*map));
  return map;
}

Handle<LoadHandler> Factory::NewLoadHandler(int data_count) {
  Handle<Map> map;
  switch (data_count) {
    case 1:
      map = load_handler1_map();
      break;
    case 2:
      map = load_handler2_map();
      break;
    case 3:
      map = load_handler3_map();
      break;
    default:
      UNREACHABLE();
      break;
  }
  return handle(LoadHandler::cast(New(map, TENURED)), isolate());
}

Handle<StoreHandler> Factory::NewStoreHandler(int data_count) {
  Handle<Map> map;
  switch (data_count) {
    case 0:
      map = store_handler0_map();
      break;
    case 1:
      map = store_handler1_map();
      break;
    case 2:
      map = store_handler2_map();
      break;
    case 3:
      map = store_handler3_map();
      break;
    default:
      UNREACHABLE();
      break;
  }
  return handle(StoreHandler::cast(New(map, TENURED)), isolate());
}

void Factory::SetRegExpAtomData(Handle<JSRegExp> regexp, JSRegExp::Type type,
                                Handle<String> source, JSRegExp::Flags flags,
                                Handle<Object> data) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kAtomDataSize);

  store->set(JSRegExp::kTagIndex, Smi::FromInt(type));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kAtomPatternIndex, *data);
  regexp->set_data(*store);
}

void Factory::SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                                    JSRegExp::Type type, Handle<String> source,
                                    JSRegExp::Flags flags, int capture_count) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kIrregexpDataSize);
  Smi* uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);
  store->set(JSRegExp::kTagIndex, Smi::FromInt(type));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::kZero);
  store->set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store->set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  regexp->set_data(*store);
}

Handle<RegExpMatchInfo> Factory::NewRegExpMatchInfo() {
  // Initially, the last match info consists of all fixed fields plus space for
  // the match itself (i.e., 2 capture indices).
  static const int kInitialSize = RegExpMatchInfo::kFirstCaptureIndex +
                                  RegExpMatchInfo::kInitialCaptureIndices;

  Handle<FixedArray> elems = NewFixedArray(kInitialSize);
  Handle<RegExpMatchInfo> result = Handle<RegExpMatchInfo>::cast(elems);

  result->SetNumberOfCaptureRegisters(RegExpMatchInfo::kInitialCaptureIndices);
  result->SetLastSubject(*empty_string());
  result->SetLastInput(*undefined_value());
  result->SetCapture(0, 0);
  result->SetCapture(1, 0);

  return result;
}

Handle<Object> Factory::GlobalConstantFor(Handle<Name> name) {
  if (Name::Equals(isolate(), name, undefined_string())) {
    return undefined_value();
  }
  if (Name::Equals(isolate(), name, NaN_string())) return nan_value();
  if (Name::Equals(isolate(), name, Infinity_string())) return infinity_value();
  return Handle<Object>::null();
}

Handle<Object> Factory::ToBoolean(bool value) {
  return value ? true_value() : false_value();
}

Handle<String> Factory::ToPrimitiveHintString(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return default_string();
    case ToPrimitiveHint::kNumber:
      return number_string();
    case ToPrimitiveHint::kString:
      return string_string();
  }
  UNREACHABLE();
}

Handle<Map> Factory::CreateSloppyFunctionMap(
    FunctionMode function_mode, MaybeHandle<JSFunction> maybe_empty_function) {
  bool has_prototype = IsFunctionModeWithPrototype(function_mode);
  int header_size = has_prototype ? JSFunction::kSizeWithPrototype
                                  : JSFunction::kSizeWithoutPrototype;
  int descriptors_count = has_prototype ? 5 : 4;
  int inobject_properties_count = 0;
  if (IsFunctionModeWithName(function_mode)) ++inobject_properties_count;

  Handle<Map> map = NewMap(
      JS_FUNCTION_TYPE, header_size + inobject_properties_count * kPointerSize,
      TERMINAL_FAST_ELEMENTS_KIND, inobject_properties_count);
  map->set_has_prototype_slot(has_prototype);
  map->set_is_constructor(has_prototype);
  map->set_is_callable(true);
  Handle<JSFunction> empty_function;
  if (maybe_empty_function.ToHandle(&empty_function)) {
    Map::SetPrototype(isolate(), map, empty_function);
  }

  //
  // Setup descriptors array.
  //
  Map::EnsureDescriptorSlack(isolate(), map, descriptors_count);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  int field_index = 0;
  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length accessor.
    Descriptor d = Descriptor::AccessorConstant(
        length_string(), function_length_accessor(), roc_attribs);
    map->AppendDescriptor(&d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  if (IsFunctionModeWithName(function_mode)) {
    // Add name field.
    Handle<Name> name = isolate()->factory()->name_string();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         roc_attribs, Representation::Tagged());
    map->AppendDescriptor(&d);

  } else {
    // Add name accessor.
    Descriptor d = Descriptor::AccessorConstant(
        name_string(), function_name_accessor(), roc_attribs);
    map->AppendDescriptor(&d);
  }
  {  // Add arguments accessor.
    Descriptor d = Descriptor::AccessorConstant(
        arguments_string(), function_arguments_accessor(), ro_attribs);
    map->AppendDescriptor(&d);
  }
  {  // Add caller accessor.
    Descriptor d = Descriptor::AccessorConstant(
        caller_string(), function_caller_accessor(), ro_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype accessor.
    PropertyAttributes attribs =
        IsFunctionModeWithWritablePrototype(function_mode) ? rw_attribs
                                                           : ro_attribs;
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), attribs);
    map->AppendDescriptor(&d);
  }
  DCHECK_EQ(inobject_properties_count, field_index);
  return map;
}

Handle<Map> Factory::CreateStrictFunctionMap(
    FunctionMode function_mode, Handle<JSFunction> empty_function) {
  bool has_prototype = IsFunctionModeWithPrototype(function_mode);
  int header_size = has_prototype ? JSFunction::kSizeWithPrototype
                                  : JSFunction::kSizeWithoutPrototype;
  int inobject_properties_count = 0;
  if (IsFunctionModeWithName(function_mode)) ++inobject_properties_count;
  if (IsFunctionModeWithHomeObject(function_mode)) ++inobject_properties_count;
  int descriptors_count = (IsFunctionModeWithPrototype(function_mode) ? 3 : 2) +
                          inobject_properties_count;

  Handle<Map> map = NewMap(
      JS_FUNCTION_TYPE, header_size + inobject_properties_count * kPointerSize,
      TERMINAL_FAST_ELEMENTS_KIND, inobject_properties_count);
  map->set_has_prototype_slot(has_prototype);
  map->set_is_constructor(has_prototype);
  map->set_is_callable(true);
  Map::SetPrototype(isolate(), map, empty_function);

  //
  // Setup descriptors array.
  //
  Map::EnsureDescriptorSlack(isolate(), map, descriptors_count);

  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  int field_index = 0;
  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length accessor.
    Descriptor d = Descriptor::AccessorConstant(
        length_string(), function_length_accessor(), roc_attribs);
    map->AppendDescriptor(&d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  if (IsFunctionModeWithName(function_mode)) {
    // Add name field.
    Handle<Name> name = isolate()->factory()->name_string();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         roc_attribs, Representation::Tagged());
    map->AppendDescriptor(&d);

  } else {
    // Add name accessor.
    Descriptor d = Descriptor::AccessorConstant(
        name_string(), function_name_accessor(), roc_attribs);
    map->AppendDescriptor(&d);
  }

  STATIC_ASSERT(JSFunction::kMaybeHomeObjectDescriptorIndex == 2);
  if (IsFunctionModeWithHomeObject(function_mode)) {
    // Add home object field.
    Handle<Name> name = isolate()->factory()->home_object_symbol();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         DONT_ENUM, Representation::Tagged());
    map->AppendDescriptor(&d);
  }

  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype accessor.
    PropertyAttributes attribs =
        IsFunctionModeWithWritablePrototype(function_mode) ? rw_attribs
                                                           : ro_attribs;
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), attribs);
    map->AppendDescriptor(&d);
  }
  DCHECK_EQ(inobject_properties_count, field_index);
  return map;
}

Handle<Map> Factory::CreateClassFunctionMap(Handle<JSFunction> empty_function) {
  Handle<Map> map = NewMap(JS_FUNCTION_TYPE, JSFunction::kSizeWithPrototype);
  map->set_has_prototype_slot(true);
  map->set_is_constructor(true);
  map->set_is_prototype_map(true);
  map->set_is_callable(true);
  Map::SetPrototype(isolate(), map, empty_function);

  //
  // Setup descriptors array.
  //
  Map::EnsureDescriptorSlack(isolate(), map, 2);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length accessor.
    Descriptor d = Descriptor::AccessorConstant(
        length_string(), function_length_accessor(), roc_attribs);
    map->AppendDescriptor(&d);
  }

  {
    // Add prototype accessor.
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), ro_attribs);
    map->AppendDescriptor(&d);
  }
  return map;
}

Handle<JSPromise> Factory::NewJSPromiseWithoutHook(PretenureFlag pretenure) {
  Handle<JSPromise> promise = Handle<JSPromise>::cast(
      NewJSObject(isolate()->promise_function(), pretenure));
  promise->set_reactions_or_result(Smi::kZero);
  promise->set_flags(0);
  for (int i = 0; i < v8::Promise::kEmbedderFieldCount; i++) {
    promise->SetEmbedderField(i, Smi::kZero);
  }
  return promise;
}

Handle<JSPromise> Factory::NewJSPromise(PretenureFlag pretenure) {
  Handle<JSPromise> promise = NewJSPromiseWithoutHook(pretenure);
  isolate()->RunPromiseHook(PromiseHookType::kInit, promise, undefined_value());
  return promise;
}

Handle<CallHandlerInfo> Factory::NewCallHandlerInfo(bool has_no_side_effect) {
  Handle<Map> map = has_no_side_effect
                        ? side_effect_free_call_handler_info_map()
                        : side_effect_call_handler_info_map();
  Handle<CallHandlerInfo> info(CallHandlerInfo::cast(New(map, TENURED)),
                               isolate());
  Object* undefined_value = ReadOnlyRoots(isolate()).undefined_value();
  info->set_callback(undefined_value);
  info->set_js_callback(undefined_value);
  info->set_data(undefined_value);
  return info;
}

// static
NewFunctionArgs NewFunctionArgs::ForWasm(
    Handle<String> name,
    Handle<WasmExportedFunctionData> exported_function_data, Handle<Map> map) {
  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_map_ = map;
  args.maybe_exported_function_data_ = exported_function_data;
  args.language_mode_ = LanguageMode::kSloppy;
  args.prototype_mutability_ = MUTABLE;

  return args;
}

// static
NewFunctionArgs NewFunctionArgs::ForBuiltin(Handle<String> name,
                                            Handle<Map> map, int builtin_id) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_map_ = map;
  args.maybe_builtin_id_ = builtin_id;
  args.language_mode_ = LanguageMode::kStrict;
  args.prototype_mutability_ = MUTABLE;

  args.SetShouldSetLanguageMode();

  return args;
}

// static
NewFunctionArgs NewFunctionArgs::ForFunctionWithoutCode(
    Handle<String> name, Handle<Map> map, LanguageMode language_mode) {
  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_map_ = map;
  args.maybe_builtin_id_ = Builtins::kIllegal;
  args.language_mode_ = language_mode;
  args.prototype_mutability_ = MUTABLE;

  args.SetShouldSetLanguageMode();

  return args;
}

// static
NewFunctionArgs NewFunctionArgs::ForBuiltinWithPrototype(
    Handle<String> name, Handle<Object> prototype, InstanceType type,
    int instance_size, int inobject_properties, int builtin_id,
    MutableMode prototype_mutability) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  NewFunctionArgs args;
  args.name_ = name;
  args.type_ = type;
  args.instance_size_ = instance_size;
  args.inobject_properties_ = inobject_properties;
  args.maybe_prototype_ = prototype;
  args.maybe_builtin_id_ = builtin_id;
  args.language_mode_ = LanguageMode::kStrict;
  args.prototype_mutability_ = prototype_mutability;

  args.SetShouldCreateAndSetInitialMap();
  args.SetShouldSetPrototype();
  args.SetShouldSetLanguageMode();

  return args;
}

// static
NewFunctionArgs NewFunctionArgs::ForBuiltinWithoutPrototype(
    Handle<String> name, int builtin_id, LanguageMode language_mode) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_builtin_id_ = builtin_id;
  args.language_mode_ = language_mode;
  args.prototype_mutability_ = MUTABLE;

  args.SetShouldSetLanguageMode();

  return args;
}

void NewFunctionArgs::SetShouldCreateAndSetInitialMap() {
  // Needed to create the initial map.
  maybe_prototype_.Assert();
  DCHECK_NE(kUninitialized, instance_size_);
  DCHECK_NE(kUninitialized, inobject_properties_);

  should_create_and_set_initial_map_ = true;
}

void NewFunctionArgs::SetShouldSetPrototype() {
  maybe_prototype_.Assert();
  should_set_prototype_ = true;
}

void NewFunctionArgs::SetShouldSetLanguageMode() {
  DCHECK(language_mode_ == LanguageMode::kStrict ||
         language_mode_ == LanguageMode::kSloppy);
  should_set_language_mode_ = true;
}

Handle<Map> NewFunctionArgs::GetMap(Isolate* isolate) const {
  if (!maybe_map_.is_null()) {
    return maybe_map_.ToHandleChecked();
  } else if (maybe_prototype_.is_null()) {
    return is_strict(language_mode_)
               ? isolate->strict_function_without_prototype_map()
               : isolate->sloppy_function_without_prototype_map();
  } else {
    DCHECK(!maybe_prototype_.is_null());
    switch (prototype_mutability_) {
      case MUTABLE:
        return is_strict(language_mode_) ? isolate->strict_function_map()
                                         : isolate->sloppy_function_map();
      case IMMUTABLE:
        return is_strict(language_mode_)
                   ? isolate->strict_function_with_readonly_prototype_map()
                   : isolate->sloppy_function_with_readonly_prototype_map();
    }
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
