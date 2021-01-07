// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory.h"

#include <algorithm>  // For copy
#include <memory>     // For shared_ptr<>
#include <string>
#include <utility>  // For move

#include "src/ast/ast-source-ranges.h"
#include "src/base/bits.h"
#include "src/builtins/accessors.h"
#include "src/builtins/constants-table-builder.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/common/globals.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/numbers/conversions.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/allocation-site-scopes.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/cell-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/string-set-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/roots/roots.h"
#include "src/strings/unicode-inl.h"

namespace v8 {
namespace internal {

Factory::CodeBuilder::CodeBuilder(Isolate* isolate, const CodeDesc& desc,
                                  CodeKind kind)
    : isolate_(isolate),
      code_desc_(desc),
      kind_(kind),
      source_position_table_(isolate_->factory()->empty_byte_array()) {}

MaybeHandle<Code> Factory::CodeBuilder::BuildInternal(
    bool retry_allocation_or_fail) {
  const auto factory = isolate_->factory();
  // Allocate objects needed for code initialization.
  Handle<ByteArray> reloc_info =
      factory->NewByteArray(code_desc_.reloc_size, AllocationType::kOld);
  Handle<CodeDataContainer> data_container;

  // Use a canonical off-heap trampoline CodeDataContainer if possible.
  const int32_t promise_rejection_flag =
      Code::IsPromiseRejectionField::encode(true);
  if (read_only_data_container_ &&
      (kind_specific_flags_ == 0 ||
       kind_specific_flags_ == promise_rejection_flag)) {
    const ReadOnlyRoots roots(isolate_);
    const auto canonical_code_data_container =
        kind_specific_flags_ == 0
            ? roots.trampoline_trivial_code_data_container_handle()
            : roots.trampoline_promise_rejection_code_data_container_handle();
    DCHECK_EQ(canonical_code_data_container->kind_specific_flags(),
              kind_specific_flags_);
    data_container = canonical_code_data_container;
  } else {
    data_container = factory->NewCodeDataContainer(
        0, read_only_data_container_ ? AllocationType::kReadOnly
                                     : AllocationType::kOld);
    data_container->set_kind_specific_flags(kind_specific_flags_);
  }

  // Basic block profiling data for builtins is stored in the JS heap rather
  // than in separately-allocated C++ objects. Allocate that data now if
  // appropriate.
  Handle<OnHeapBasicBlockProfilerData> on_heap_profiler_data;
  if (profiler_data_ && isolate_->IsGeneratingEmbeddedBuiltins()) {
    on_heap_profiler_data = profiler_data_->CopyToJSHeap(isolate_);

    // Add the on-heap data to a global list, which keeps it alive and allows
    // iteration.
    Handle<ArrayList> list(isolate_->heap()->basic_block_profiling_data(),
                           isolate_);
    Handle<ArrayList> new_list =
        ArrayList::Add(isolate_, list, on_heap_profiler_data);
    isolate_->heap()->SetBasicBlockProfilingData(new_list);
  }

  STATIC_ASSERT(Code::kOnHeapBodyIsContiguous);
  const int object_size = Code::SizeFor(code_desc_.body_size());

  Handle<Code> code;
  {
    Heap* heap = isolate_->heap();

    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject result;
    AllocationType allocation_type =
        is_executable_ ? AllocationType::kCode : AllocationType::kReadOnly;
    if (retry_allocation_or_fail) {
      result = heap->AllocateRawWith<Heap::kRetryOrFail>(
          object_size, allocation_type, AllocationOrigin::kRuntime);
    } else {
      result = heap->AllocateRawWith<Heap::kLightRetry>(
          object_size, allocation_type, AllocationOrigin::kRuntime);
      // Return an empty handle if we cannot allocate the code object.
      if (result.is_null()) return MaybeHandle<Code>();
    }

    // The code object has not been fully initialized yet.  We rely on the
    // fact that no allocation will happen from this point on.
    DisallowGarbageCollection no_gc;

    result.set_map_after_allocation(*factory->code_map(), SKIP_WRITE_BARRIER);
    code = handle(Code::cast(result), isolate_);
    if (is_executable_) {
      DCHECK(IsAligned(code->address(), kCodeAlignment));
      DCHECK_IMPLIES(
          !V8_ENABLE_THIRD_PARTY_HEAP_BOOL &&
              !heap->memory_allocator()->code_range().is_empty(),
          heap->memory_allocator()->code_range().contains(code->address()));
    }

    constexpr bool kIsNotOffHeapTrampoline = false;

    code->set_raw_instruction_size(code_desc_.instruction_size());
    code->set_raw_metadata_size(code_desc_.metadata_size());
    code->set_relocation_info(*reloc_info);
    code->initialize_flags(kind_, is_turbofanned_, stack_slots_,
                           kIsNotOffHeapTrampoline);
    code->set_builtin_index(builtin_index_);
    code->set_inlined_bytecode_size(inlined_bytecode_size_);
    code->set_code_data_container(*data_container, kReleaseStore);
    code->set_deoptimization_data(*deoptimization_data_);
    code->set_source_position_table(*source_position_table_);
    code->set_handler_table_offset(code_desc_.handler_table_offset_relative());
    code->set_constant_pool_offset(code_desc_.constant_pool_offset_relative());
    code->set_code_comments_offset(code_desc_.code_comments_offset_relative());
    code->set_unwinding_info_offset(
        code_desc_.unwinding_info_offset_relative());

    // Allow self references to created code object by patching the handle to
    // point to the newly allocated Code object.
    Handle<Object> self_reference;
    if (self_reference_.ToHandle(&self_reference)) {
      DCHECK(self_reference->IsOddball());
      DCHECK(Oddball::cast(*self_reference).kind() ==
             Oddball::kSelfReferenceMarker);
      if (isolate_->IsGeneratingEmbeddedBuiltins()) {
        isolate_->builtins_constants_table_builder()->PatchSelfReference(
            self_reference, code);
      }
      self_reference.PatchValue(*code);
    }

    // Likewise, any references to the basic block counters marker need to be
    // updated to point to the newly-allocated counters array.
    if (!on_heap_profiler_data.is_null()) {
      isolate_->builtins_constants_table_builder()
          ->PatchBasicBlockCountersReference(
              handle(on_heap_profiler_data->counts(), isolate_));
    }

    // Migrate generated code.
    // The generated code can contain embedded objects (typically from handles)
    // in a pointer-to-tagged-value format (i.e. with indirection like a handle)
    // that are dereferenced during the copy to point directly to the actual
    // heap objects. These pointers can include references to the code object
    // itself, through the self_reference parameter.
    code->CopyFromNoFlush(heap, code_desc_);

    code->clear_padding();

#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) code->ObjectVerify(isolate_);
#endif

    // Flush the instruction cache before changing the permissions.
    // Note: we do this before setting permissions to ReadExecute because on
    // some older ARM kernels there is a bug which causes an access error on
    // cache flush instructions to trigger access error on non-writable memory.
    // See https://bugs.chromium.org/p/v8/issues/detail?id=8157
    code->FlushICache();
  }

  if (profiler_data_ && FLAG_turbo_profiling_verbose) {
#ifdef ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(nullptr, os, isolate_);
    if (!on_heap_profiler_data.is_null()) {
      Handle<String> disassembly =
          isolate_->factory()->NewStringFromAsciiChecked(os.str().c_str(),
                                                         AllocationType::kOld);
      on_heap_profiler_data->set_code(*disassembly);
    } else {
      profiler_data_->SetCode(os);
    }
#endif  // ENABLE_DISASSEMBLER
  }

  return code;
}

MaybeHandle<Code> Factory::CodeBuilder::TryBuild() {
  return BuildInternal(false);
}

Handle<Code> Factory::CodeBuilder::Build() {
  return BuildInternal(true).ToHandleChecked();
}

HeapObject Factory::AllocateRaw(int size, AllocationType allocation,
                                AllocationAlignment alignment) {
  return isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, allocation, AllocationOrigin::kRuntime, alignment);
}

HeapObject Factory::AllocateRawWithAllocationSite(
    Handle<Map> map, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  if (!allocation_site.is_null()) size += AllocationMemento::kSize;
  HeapObject result =
      isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(size, allocation);
  WriteBarrierMode write_barrier_mode = allocation == AllocationType::kYoung
                                            ? SKIP_WRITE_BARRIER
                                            : UPDATE_WRITE_BARRIER;
  result.set_map_after_allocation(*map, write_barrier_mode);
  if (!allocation_site.is_null()) {
    AllocationMemento alloc_memento = AllocationMemento::unchecked_cast(
        Object(result.ptr() + map->instance_size()));
    InitializeAllocationMemento(alloc_memento, *allocation_site);
  }
  return result;
}

void Factory::InitializeAllocationMemento(AllocationMemento memento,
                                          AllocationSite allocation_site) {
  memento.set_map_after_allocation(*allocation_memento_map(),
                                   SKIP_WRITE_BARRIER);
  memento.set_allocation_site(allocation_site, SKIP_WRITE_BARRIER);
  if (FLAG_allocation_site_pretenuring) {
    allocation_site.IncrementMementoCreateCount();
  }
}

HeapObject Factory::New(Handle<Map> map, AllocationType allocation) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  HeapObject result =
      isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(size, allocation);
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode = allocation == AllocationType::kYoung
                                            ? SKIP_WRITE_BARRIER
                                            : UPDATE_WRITE_BARRIER;
  result.set_map_after_allocation(*map, write_barrier_mode);
  return result;
}

Handle<HeapObject> Factory::NewFillerObject(int size, bool double_align,
                                            AllocationType allocation,
                                            AllocationOrigin origin) {
  AllocationAlignment alignment = double_align ? kDoubleAligned : kWordAligned;
  Heap* heap = isolate()->heap();
  HeapObject result = heap->AllocateRawWith<Heap::kRetryOrFail>(
      size, allocation, origin, alignment);
  heap->CreateFillerObjectAt(result.address(), size, ClearRecordedSlots::kNo);
  return Handle<HeapObject>(result, isolate());
}

Handle<PrototypeInfo> Factory::NewPrototypeInfo() {
  Handle<PrototypeInfo> result = Handle<PrototypeInfo>::cast(
      NewStruct(PROTOTYPE_INFO_TYPE, AllocationType::kOld));
  result->set_prototype_users(Smi::zero());
  result->set_registry_slot(PrototypeInfo::UNREGISTERED);
  result->set_bit_field(0);
  result->set_module_namespace(*undefined_value());
  return result;
}

Handle<EnumCache> Factory::NewEnumCache(Handle<FixedArray> keys,
                                        Handle<FixedArray> indices) {
  Handle<EnumCache> result =
      Handle<EnumCache>::cast(NewStruct(ENUM_CACHE_TYPE, AllocationType::kOld));
  result->set_keys(*keys);
  result->set_indices(*indices);
  return result;
}

Handle<Tuple2> Factory::NewTuple2(Handle<Object> value1, Handle<Object> value2,
                                  AllocationType allocation) {
  Handle<Tuple2> result =
      Handle<Tuple2>::cast(NewStruct(TUPLE2_TYPE, allocation));
  result->set_value1(*value1);
  result->set_value2(*value2);
  return result;
}

Handle<Oddball> Factory::NewOddball(Handle<Map> map, const char* to_string,
                                    Handle<Object> to_number,
                                    const char* type_of, byte kind) {
  Handle<Oddball> oddball(Oddball::cast(New(map, AllocationType::kReadOnly)),
                          isolate());
  Oddball::Initialize(isolate(), oddball, to_string, to_number, type_of, kind);
  return oddball;
}

Handle<Oddball> Factory::NewSelfReferenceMarker() {
  return NewOddball(self_reference_marker_map(), "self_reference_marker",
                    handle(Smi::FromInt(-1), isolate()), "undefined",
                    Oddball::kSelfReferenceMarker);
}

Handle<Oddball> Factory::NewBasicBlockCountersMarker() {
  return NewOddball(basic_block_counters_marker_map(),
                    "basic_block_counters_marker",
                    handle(Smi::FromInt(-1), isolate()), "undefined",
                    Oddball::kBasicBlockCountersMarker);
}

Handle<PropertyArray> Factory::NewPropertyArray(int length) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_property_array();
  HeapObject result = AllocateRawFixedArray(length, AllocationType::kYoung);
  result.set_map_after_allocation(*property_array_map(), SKIP_WRITE_BARRIER);
  Handle<PropertyArray> array(PropertyArray::cast(result), isolate());
  array->initialize_length(length);
  MemsetTagged(array->data_start(), *undefined_value(), length);
  return array;
}

MaybeHandle<FixedArray> Factory::TryNewFixedArray(
    int length, AllocationType allocation_type) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();

  int size = FixedArray::SizeFor(length);
  Heap* heap = isolate()->heap();
  AllocationResult allocation = heap->AllocateRaw(size, allocation_type);
  HeapObject result;
  if (!allocation.To(&result)) return MaybeHandle<FixedArray>();
  if ((size > Heap::MaxRegularHeapObjectSize(allocation_type)) &&
      FLAG_use_marking_progress_bar) {
    BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(result);
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  result.set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<FixedArray> array(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(array->data_start(), ReadOnlyRoots(heap).undefined_value(),
               length);
  return array;
}

Handle<FixedArray> Factory::NewUninitializedFixedArray(int length) {
  if (length == 0) return empty_fixed_array();
  if (length < 0 || length > FixedArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }

  // TODO(ulan): As an experiment this temporarily returns an initialized fixed
  // array. After getting canary/performance coverage, either remove the
  // function or revert to returning uninitilized array.
  return NewFixedArrayWithFiller(read_only_roots().fixed_array_map_handle(),
                                 length, undefined_value(),
                                 AllocationType::kYoung);
}

Handle<ClosureFeedbackCellArray> Factory::NewClosureFeedbackCellArray(
    int length) {
  if (length == 0) return empty_closure_feedback_cell_array();

  Handle<ClosureFeedbackCellArray> feedback_cell_array =
      Handle<ClosureFeedbackCellArray>::cast(NewFixedArrayWithMap(
          read_only_roots().closure_feedback_cell_array_map_handle(), length,
          AllocationType::kOld));

  return feedback_cell_array;
}

Handle<FeedbackVector> Factory::NewFeedbackVector(
    Handle<SharedFunctionInfo> shared,
    Handle<ClosureFeedbackCellArray> closure_feedback_cell_array) {
  int length = shared->feedback_metadata().slot_count();
  DCHECK_LE(0, length);
  int size = FeedbackVector::SizeFor(length);

  HeapObject result = AllocateRawWithImmortalMap(size, AllocationType::kOld,
                                                 *feedback_vector_map());
  Handle<FeedbackVector> vector(FeedbackVector::cast(result), isolate());
  vector->set_shared_function_info(*shared);
  vector->set_maybe_optimized_code(
      HeapObjectReference::ClearedValue(isolate()));
  vector->set_length(length);
  vector->set_invocation_count(0);
  vector->set_profiler_ticks(0);
  vector->InitializeOptimizationState();
  vector->set_closure_feedback_cell_array(*closure_feedback_cell_array);

  // TODO(leszeks): Initialize based on the feedback metadata.
  MemsetTagged(ObjectSlot(vector->slots_start()), *undefined_value(), length);
  return vector;
}

Handle<EmbedderDataArray> Factory::NewEmbedderDataArray(int length) {
  DCHECK_LE(0, length);
  int size = EmbedderDataArray::SizeFor(length);

  HeapObject result = AllocateRawWithImmortalMap(size, AllocationType::kYoung,
                                                 *embedder_data_array_map());
  Handle<EmbedderDataArray> array(EmbedderDataArray::cast(result), isolate());
  array->set_length(length);

  if (length > 0) {
    ObjectSlot start(array->slots_start());
    ObjectSlot end(array->slots_end());
    size_t slot_count = end - start;
    MemsetTagged(start, *undefined_value(), slot_count);
    for (int i = 0; i < length; i++) {
      // TODO(v8:10391, saelo): Handle external pointers in EmbedderDataSlot
      EmbedderDataSlot(*array, i).AllocateExternalPointerEntry(isolate());
    }
  }
  return array;
}

Handle<FixedArrayBase> Factory::NewFixedDoubleArrayWithHoles(int length) {
  DCHECK_LE(0, length);
  Handle<FixedArrayBase> array = NewFixedDoubleArray(length);
  if (length > 0) {
    Handle<FixedDoubleArray>::cast(array)->FillWithHoles(0, length);
  }
  return array;
}

Handle<FrameArray> Factory::NewFrameArray(int number_of_frames) {
  DCHECK_LE(0, number_of_frames);
  Handle<FixedArray> result =
      NewFixedArrayWithHoles(FrameArray::LengthFor(number_of_frames));
  result->set(FrameArray::kFrameCountIndex, Smi::zero());
  return Handle<FrameArray>::cast(result);
}

template <typename T>
Handle<T> Factory::AllocateSmallOrderedHashTable(Handle<Map> map, int capacity,
                                                 AllocationType allocation) {
  // Capacity must be a power of two, since we depend on being able
  // to divide and multiple by 2 (kLoadFactor) to derive capacity
  // from number of buckets. If we decide to change kLoadFactor
  // to something other than 2, capacity should be stored as another
  // field of this object.
  DCHECK_EQ(T::kLoadFactor, 2);
  capacity =
      base::bits::RoundUpToPowerOfTwo32(std::max({T::kMinCapacity, capacity}));
  capacity = std::min({capacity, T::kMaxCapacity});

  DCHECK_LT(0, capacity);
  DCHECK_EQ(0, capacity % T::kLoadFactor);

  int size = T::SizeFor(capacity);
  HeapObject result = AllocateRawWithImmortalMap(size, allocation, *map);
  Handle<T> table(T::cast(result), isolate());
  table->Initialize(isolate(), capacity);
  return table;
}

Handle<SmallOrderedHashSet> Factory::NewSmallOrderedHashSet(
    int capacity, AllocationType allocation) {
  return AllocateSmallOrderedHashTable<SmallOrderedHashSet>(
      small_ordered_hash_set_map(), capacity, allocation);
}

Handle<SmallOrderedHashMap> Factory::NewSmallOrderedHashMap(
    int capacity, AllocationType allocation) {
  return AllocateSmallOrderedHashTable<SmallOrderedHashMap>(
      small_ordered_hash_map_map(), capacity, allocation);
}

Handle<SmallOrderedNameDictionary> Factory::NewSmallOrderedNameDictionary(
    int capacity, AllocationType allocation) {
  Handle<SmallOrderedNameDictionary> dict =
      AllocateSmallOrderedHashTable<SmallOrderedNameDictionary>(
          small_ordered_name_dictionary_map(), capacity, allocation);
  dict->SetHash(PropertyArray::kNoHashSentinel);
  return dict;
}

Handle<OrderedHashSet> Factory::NewOrderedHashSet() {
  return OrderedHashSet::Allocate(isolate(), OrderedHashSet::kInitialCapacity,
                                  AllocationType::kYoung)
      .ToHandleChecked();
}

Handle<OrderedHashMap> Factory::NewOrderedHashMap() {
  return OrderedHashMap::Allocate(isolate(), OrderedHashMap::kInitialCapacity,
                                  AllocationType::kYoung)
      .ToHandleChecked();
}

Handle<OrderedNameDictionary> Factory::NewOrderedNameDictionary(int capacity) {
  return OrderedNameDictionary::Allocate(isolate(), capacity,
                                         AllocationType::kYoung)
      .ToHandleChecked();
}

Handle<NameDictionary> Factory::NewNameDictionary(int at_least_space_for) {
  return NameDictionary::New(isolate(), at_least_space_for);
}

Handle<PropertyDescriptorObject> Factory::NewPropertyDescriptorObject() {
  Handle<PropertyDescriptorObject> object =
      Handle<PropertyDescriptorObject>::cast(
          NewStruct(PROPERTY_DESCRIPTOR_OBJECT_TYPE, AllocationType::kYoung));
  object->set_flags(0);
  object->set_value(*the_hole_value(), SKIP_WRITE_BARRIER);
  object->set_get(*the_hole_value(), SKIP_WRITE_BARRIER);
  object->set_set(*the_hole_value(), SKIP_WRITE_BARRIER);
  return object;
}

// Internalized strings are created in the old generation (data space).
Handle<String> Factory::InternalizeUtf8String(
    const Vector<const char>& string) {
  Vector<const uint8_t> utf8_data = Vector<const uint8_t>::cast(string);
  Utf8Decoder decoder(utf8_data);
  if (decoder.is_ascii()) return InternalizeString(utf8_data);
  if (decoder.is_one_byte()) {
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[decoder.utf16_length()]);
    decoder.Decode(buffer.get(), utf8_data);
    return InternalizeString(
        Vector<const uint8_t>(buffer.get(), decoder.utf16_length()));
  }
  std::unique_ptr<uint16_t[]> buffer(new uint16_t[decoder.utf16_length()]);
  decoder.Decode(buffer.get(), utf8_data);
  return InternalizeString(
      Vector<const uc16>(buffer.get(), decoder.utf16_length()));
}

template <typename SeqString>
Handle<String> Factory::InternalizeString(Handle<SeqString> string, int from,
                                          int length, bool convert_encoding) {
  SeqSubStringKey<SeqString> key(isolate(), string, from, length,
                                 convert_encoding);
  return InternalizeStringWithKey(&key);
}

template Handle<String> Factory::InternalizeString(
    Handle<SeqOneByteString> string, int from, int length,
    bool convert_encoding);
template Handle<String> Factory::InternalizeString(
    Handle<SeqTwoByteString> string, int from, int length,
    bool convert_encoding);

MaybeHandle<String> Factory::NewStringFromOneByte(
    const Vector<const uint8_t>& string, AllocationType allocation) {
  DCHECK_NE(allocation, AllocationType::kReadOnly);
  int length = string.length();
  if (length == 0) return empty_string();
  if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
  Handle<SeqOneByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                             NewRawOneByteString(string.length(), allocation),
                             String);

  DisallowGarbageCollection no_gc;
  // Copy the characters into the new object.
  CopyChars(SeqOneByteString::cast(*result).GetChars(no_gc), string.begin(),
            length);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8(const Vector<const char>& string,
                                               AllocationType allocation) {
  Vector<const uint8_t> utf8_data = Vector<const uint8_t>::cast(string);
  Utf8Decoder decoder(utf8_data);

  if (decoder.utf16_length() == 0) return empty_string();

  if (decoder.is_one_byte()) {
    // Allocate string.
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        NewRawOneByteString(decoder.utf16_length(), allocation), String);

    DisallowGarbageCollection no_gc;
    decoder.Decode(result->GetChars(no_gc), utf8_data);
    return result;
  }

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(decoder.utf16_length(), allocation), String);

  DisallowGarbageCollection no_gc;
  decoder.Decode(result->GetChars(no_gc), utf8_data);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8SubString(
    Handle<SeqOneByteString> str, int begin, int length,
    AllocationType allocation) {
  Vector<const uint8_t> utf8_data;
  {
    DisallowGarbageCollection no_gc;
    utf8_data = Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
  }
  Utf8Decoder decoder(utf8_data);

  if (length == 1) {
    uint16_t t;
    // Decode even in the case of length 1 since it can be a bad character.
    decoder.Decode(&t, utf8_data);
    return LookupSingleCharacterStringFromCode(t);
  }

  if (decoder.is_ascii()) {
    // If the string is ASCII, we can just make a substring.
    // TODO(v8): the allocation flag is ignored in this case.
    return NewSubString(str, begin, begin + length);
  }

  DCHECK_GT(decoder.utf16_length(), 0);

  if (decoder.is_one_byte()) {
    // Allocate string.
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        NewRawOneByteString(decoder.utf16_length(), allocation), String);
    DisallowGarbageCollection no_gc;
    // Update pointer references, since the original string may have moved after
    // allocation.
    utf8_data = Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
    decoder.Decode(result->GetChars(no_gc), utf8_data);
    return result;
  }

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(decoder.utf16_length(), allocation), String);

  DisallowGarbageCollection no_gc;
  // Update pointer references, since the original string may have moved after
  // allocation.
  utf8_data = Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
  decoder.Decode(result->GetChars(no_gc), utf8_data);
  return result;
}

MaybeHandle<String> Factory::NewStringFromTwoByte(const uc16* string,
                                                  int length,
                                                  AllocationType allocation) {
  DCHECK_NE(allocation, AllocationType::kReadOnly);
  if (length == 0) return empty_string();
  if (String::IsOneByte(string, length)) {
    if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               NewRawOneByteString(length, allocation), String);
    DisallowGarbageCollection no_gc;
    CopyChars(result->GetChars(no_gc), string, length);
    return result;
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               NewRawTwoByteString(length, allocation), String);
    DisallowGarbageCollection no_gc;
    CopyChars(result->GetChars(no_gc), string, length);
    return result;
  }
}

MaybeHandle<String> Factory::NewStringFromTwoByte(
    const Vector<const uc16>& string, AllocationType allocation) {
  return NewStringFromTwoByte(string.begin(), string.length(), allocation);
}

MaybeHandle<String> Factory::NewStringFromTwoByte(
    const ZoneVector<uc16>* string, AllocationType allocation) {
  return NewStringFromTwoByte(string->data(), static_cast<int>(string->size()),
                              allocation);
}

namespace {

bool inline IsOneByte(Handle<String> str) {
  return str->IsOneByteRepresentation();
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

template <bool is_one_byte, typename T>
Handle<String> Factory::AllocateInternalizedStringImpl(T t, int chars,
                                                       uint32_t hash_field) {
  DCHECK_LE(0, chars);
  DCHECK_GE(String::kMaxLength, chars);

  // Compute map and object size.
  int size;
  Map map;
  if (is_one_byte) {
    map = *one_byte_internalized_string_map();
    size = SeqOneByteString::SizeFor(chars);
  } else {
    map = *internalized_string_map();
    size = SeqTwoByteString::SizeFor(chars);
  }

  HeapObject result =
      AllocateRawWithImmortalMap(size,
                                 isolate()->heap()->CanAllocateInReadOnlySpace()
                                     ? AllocationType::kReadOnly
                                     : AllocationType::kOld,
                                 map);
  Handle<String> answer(String::cast(result), isolate());
  answer->set_length(chars);
  answer->set_raw_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  DisallowGarbageCollection no_gc;

  if (is_one_byte) {
    WriteOneByteData(t, SeqOneByteString::cast(*answer).GetChars(no_gc), chars);
  } else {
    WriteTwoByteData(t, SeqTwoByteString::cast(*answer).GetChars(no_gc), chars);
  }
  return answer;
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
  switch (string->map().instance_type()) {
    case STRING_TYPE:
      return f->internalized_string_map();
    case ONE_BYTE_STRING_TYPE:
      return f->one_byte_internalized_string_map();
    case EXTERNAL_STRING_TYPE:
      return f->external_internalized_string_map();
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
      return f->external_one_byte_internalized_string_map();
    case UNCACHED_EXTERNAL_STRING_TYPE:
      return f->uncached_external_internalized_string_map();
    case UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      return f->uncached_external_one_byte_internalized_string_map();
    default:
      return MaybeHandle<Map>();  // No match found.
  }
}

}  // namespace

MaybeHandle<Map> Factory::InternalizedStringMapForString(
    Handle<String> string) {
  // If the string is in the young generation, it cannot be used as
  // internalized.
  if (Heap::InYoungGeneration(*string)) return MaybeHandle<Map>();

  return GetInternalizedStringMap(this, string);
}

template <class StringClass>
Handle<StringClass> Factory::InternalizeExternalString(Handle<String> string) {
  Handle<StringClass> cast_string = Handle<StringClass>::cast(string);
  Handle<Map> map = GetInternalizedStringMap(this, string).ToHandleChecked();
  Handle<StringClass> external_string(
      StringClass::cast(New(map, AllocationType::kOld)), isolate());
  external_string->AllocateExternalPointerEntries(isolate());
  external_string->set_length(cast_string->length());
  external_string->set_raw_hash_field(cast_string->raw_hash_field());
  external_string->SetResource(isolate(), nullptr);
  isolate()->heap()->RegisterExternalString(*external_string);
  return external_string;
}

template Handle<ExternalOneByteString>
    Factory::InternalizeExternalString<ExternalOneByteString>(Handle<String>);
template Handle<ExternalTwoByteString>
    Factory::InternalizeExternalString<ExternalTwoByteString>(Handle<String>);

Handle<String> Factory::LookupSingleCharacterStringFromCode(uint16_t code) {
  if (code <= unibrow::Latin1::kMaxChar) {
    {
      DisallowGarbageCollection no_gc;
      Object value = single_character_string_cache()->get(code);
      if (value != *undefined_value()) {
        return handle(String::cast(value), isolate());
      }
    }
    uint8_t buffer[] = {static_cast<uint8_t>(code)};
    Handle<String> result = InternalizeString(Vector<const uint8_t>(buffer, 1));
    single_character_string_cache()->set(code, *result);
    return result;
  }
  uint16_t buffer[] = {code};
  return InternalizeString(Vector<const uint16_t>(buffer, 1));
}

Handle<String> Factory::NewSurrogatePairString(uint16_t lead, uint16_t trail) {
  DCHECK_GE(lead, 0xD800);
  DCHECK_LE(lead, 0xDBFF);
  DCHECK_GE(trail, 0xDC00);
  DCHECK_LE(trail, 0xDFFF);

  Handle<SeqTwoByteString> str =
      isolate()->factory()->NewRawTwoByteString(2).ToHandleChecked();
  DisallowGarbageCollection no_gc;
  uc16* dest = str->GetChars(no_gc);
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
    return MakeOrFindTwoCharacterString(c1, c2);
  }

  if (!FLAG_string_slices || length < SlicedString::kMinLength) {
    if (str->IsOneByteRepresentation()) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      DisallowGarbageCollection no_gc;
      uint8_t* dest = result->GetChars(no_gc);
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    } else {
      Handle<SeqTwoByteString> result =
          NewRawTwoByteString(length).ToHandleChecked();
      DisallowGarbageCollection no_gc;
      uc16* dest = result->GetChars(no_gc);
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
  Handle<SlicedString> slice(
      SlicedString::cast(New(map, AllocationType::kYoung)), isolate());

  slice->set_raw_hash_field(String::kEmptyHashField);
  slice->set_length(length);
  slice->set_parent(*str);
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

  Handle<Map> map = resource->IsCacheable()
                        ? external_one_byte_string_map()
                        : uncached_external_one_byte_string_map();
  Handle<ExternalOneByteString> external_string(
      ExternalOneByteString::cast(New(map, AllocationType::kOld)), isolate());
  external_string->AllocateExternalPointerEntries(isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_raw_hash_field(String::kEmptyHashField);
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

  Handle<Map> map = resource->IsCacheable() ? external_string_map()
                                            : uncached_external_string_map();
  Handle<ExternalTwoByteString> external_string(
      ExternalTwoByteString::cast(New(map, AllocationType::kOld)), isolate());
  external_string->AllocateExternalPointerEntries(isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_raw_hash_field(String::kEmptyHashField);
  external_string->SetResource(isolate(), resource);
  isolate()->heap()->RegisterExternalString(*external_string);

  return external_string;
}

Handle<JSStringIterator> Factory::NewJSStringIterator(Handle<String> string) {
  Handle<Map> map(isolate()->native_context()->initial_string_iterator_map(),
                  isolate());
  Handle<String> flat_string = String::Flatten(isolate(), string);
  Handle<JSStringIterator> iterator =
      Handle<JSStringIterator>::cast(NewJSObjectFromMap(map));
  iterator->set_string(*flat_string);
  iterator->set_index(0);

  return iterator;
}

Handle<Symbol> Factory::NewSymbol(AllocationType allocation) {
  DCHECK(allocation != AllocationType::kYoung);
  // Statically ensure that it is safe to allocate symbols in paged spaces.
  STATIC_ASSERT(Symbol::kSize <= kMaxRegularHeapObjectSize);

  HeapObject result =
      AllocateRawWithImmortalMap(Symbol::kSize, allocation, *symbol_map());

  // Generate a random hash value.
  int hash = isolate()->GenerateIdentityHash(Name::kHashBitMask);

  Handle<Symbol> symbol(Symbol::cast(result), isolate());
  symbol->set_raw_hash_field(Name::kIsNotIntegerIndexMask |
                             (hash << Name::kHashShift));
  symbol->set_description(*undefined_value());
  symbol->set_flags(0);
  DCHECK(!symbol->is_private());
  return symbol;
}

Handle<Symbol> Factory::NewPrivateSymbol(AllocationType allocation) {
  DCHECK(allocation != AllocationType::kYoung);
  Handle<Symbol> symbol = NewSymbol(allocation);
  symbol->set_is_private(true);
  return symbol;
}

Handle<Symbol> Factory::NewPrivateNameSymbol(Handle<String> name) {
  Handle<Symbol> symbol = NewSymbol();
  symbol->set_is_private_name();
  symbol->set_description(*name);
  return symbol;
}

Handle<Context> Factory::NewContext(Handle<Map> map, int size,
                                    int variadic_part_length,
                                    AllocationType allocation) {
  DCHECK_LE(Context::kTodoHeaderSize, size);
  DCHECK(IsAligned(size, kTaggedSize));
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  DCHECK_LE(Context::SizeFor(variadic_part_length), size);

  HeapObject result =
      isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(size, allocation);
  result.set_map_after_allocation(*map);
  Handle<Context> context(Context::cast(result), isolate());
  context->set_length(variadic_part_length);
  DCHECK_EQ(context->SizeFromMap(*map), size);
  if (size > Context::kTodoHeaderSize) {
    ObjectSlot start = context->RawField(Context::kTodoHeaderSize);
    ObjectSlot end = context->RawField(size);
    size_t slot_count = end - start;
    MemsetTagged(start, *undefined_value(), slot_count);
  }
  return context;
}

Handle<NativeContext> Factory::NewNativeContext() {
  Handle<Map> map = NewMap(NATIVE_CONTEXT_TYPE, kVariableSizeSentinel);
  Handle<NativeContext> context = Handle<NativeContext>::cast(
      NewContext(map, NativeContext::kSize, NativeContext::NATIVE_CONTEXT_SLOTS,
                 AllocationType::kOld));
  context->set_native_context_map(*map);
  map->set_native_context(*context);
  context->AllocateExternalPointerEntries(isolate());
  context->set_scope_info(ReadOnlyRoots(isolate()).native_scope_info());
  context->set_previous(Context::unchecked_cast(Smi::zero()));
  context->set_extension(*undefined_value());
  context->set_errors_thrown(Smi::zero());
  context->set_math_random_index(Smi::zero());
  context->set_serialized_objects(*empty_fixed_array());
  context->set_microtask_queue(isolate(), nullptr);
  context->set_osr_code_cache(*empty_weak_fixed_array());
  context->set_retained_maps(*empty_weak_array_list());
  return context;
}

Handle<Context> Factory::NewScriptContext(Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), SCRIPT_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context =
      NewContext(handle(outer->script_context_map(), isolate()),
                 Context::SizeFor(variadic_part_length), variadic_part_length,
                 AllocationType::kOld);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  DCHECK(context->IsScriptContext());
  return context;
}

Handle<ScriptContextTable> Factory::NewScriptContextTable() {
  Handle<ScriptContextTable> context_table = Handle<ScriptContextTable>::cast(
      NewFixedArrayWithMap(read_only_roots().script_context_table_map_handle(),
                           ScriptContextTable::kMinLength));
  context_table->synchronized_set_used(0);
  return context_table;
}

Handle<Context> Factory::NewModuleContext(Handle<SourceTextModule> module,
                                          Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), MODULE_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context = NewContext(
      isolate()->module_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kOld);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*module);
  DCHECK(context->IsModuleContext());
  return context;
}

Handle<Context> Factory::NewFunctionContext(Handle<Context> outer,
                                            Handle<ScopeInfo> scope_info) {
  Handle<Map> map;
  switch (scope_info->scope_type()) {
    case EVAL_SCOPE:
      map = isolate()->eval_context_map();
      break;
    case FUNCTION_SCOPE:
      map = isolate()->function_context_map();
      break;
    default:
      UNREACHABLE();
  }
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context =
      NewContext(map, Context::SizeFor(variadic_part_length),
                 variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  return context;
}

Handle<Context> Factory::NewCatchContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info,
                                         Handle<Object> thrown_object) {
  DCHECK_EQ(scope_info->scope_type(), CATCH_SCOPE);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == Context::THROWN_OBJECT_INDEX);
  // TODO(ishell): Take the details from CatchContext class.
  int variadic_part_length = Context::MIN_CONTEXT_SLOTS + 1;
  Handle<Context> context = NewContext(
      isolate()->catch_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set(Context::THROWN_OBJECT_INDEX, *thrown_object);
  return context;
}

Handle<Context> Factory::NewDebugEvaluateContext(Handle<Context> previous,
                                                 Handle<ScopeInfo> scope_info,
                                                 Handle<JSReceiver> extension,
                                                 Handle<Context> wrapped,
                                                 Handle<StringSet> blocklist) {
  STATIC_ASSERT(Context::BLOCK_LIST_INDEX ==
                Context::MIN_CONTEXT_EXTENDED_SLOTS + 1);
  DCHECK(scope_info->IsDebugEvaluateScope());
  Handle<HeapObject> ext = extension.is_null()
                               ? Handle<HeapObject>::cast(undefined_value())
                               : Handle<HeapObject>::cast(extension);
  // TODO(ishell): Take the details from DebugEvaluateContextContext class.
  int variadic_part_length = Context::MIN_CONTEXT_EXTENDED_SLOTS + 2;
  Handle<Context> c = NewContext(isolate()->debug_evaluate_context_map(),
                                 Context::SizeFor(variadic_part_length),
                                 variadic_part_length, AllocationType::kYoung);
  c->set_scope_info(*scope_info);
  c->set_previous(*previous);
  c->set_extension(*ext);
  if (!wrapped.is_null()) c->set(Context::WRAPPED_CONTEXT_INDEX, *wrapped);
  if (!blocklist.is_null()) c->set(Context::BLOCK_LIST_INDEX, *blocklist);
  return c;
}

Handle<Context> Factory::NewWithContext(Handle<Context> previous,
                                        Handle<ScopeInfo> scope_info,
                                        Handle<JSReceiver> extension) {
  DCHECK_EQ(scope_info->scope_type(), WITH_SCOPE);
  // TODO(ishell): Take the details from WithContext class.
  int variadic_part_length = Context::MIN_CONTEXT_EXTENDED_SLOTS;
  Handle<Context> context = NewContext(
      isolate()->with_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*extension);
  return context;
}

Handle<Context> Factory::NewBlockContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  DCHECK_IMPLIES(scope_info->scope_type() != BLOCK_SCOPE,
                 scope_info->scope_type() == CLASS_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context = NewContext(
      isolate()->block_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  return context;
}

Handle<Context> Factory::NewBuiltinContext(Handle<NativeContext> native_context,
                                           int variadic_part_length) {
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  Handle<Context> context = NewContext(
      isolate()->function_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(ReadOnlyRoots(isolate()).empty_scope_info());
  context->set_previous(*native_context);
  return context;
}

Handle<AliasedArgumentsEntry> Factory::NewAliasedArgumentsEntry(
    int aliased_context_slot) {
  Handle<AliasedArgumentsEntry> entry = Handle<AliasedArgumentsEntry>::cast(
      NewStruct(ALIASED_ARGUMENTS_ENTRY_TYPE, AllocationType::kYoung));
  entry->set_aliased_context_slot(aliased_context_slot);
  return entry;
}

Handle<AccessorInfo> Factory::NewAccessorInfo() {
  Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(
      NewStruct(ACCESSOR_INFO_TYPE, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  info->set_name(*empty_string());
  info->set_flags(0);  // Must clear the flags, it was initialized as undefined.
  info->set_is_sloppy(true);
  info->set_initial_property_attributes(NONE);

  // Clear some other fields that should not be undefined.
  info->set_getter(Smi::zero());
  info->set_setter(Smi::zero());
  info->set_js_getter(Smi::zero());

  return info;
}

void Factory::AddToScriptList(Handle<Script> script) {
  Handle<WeakArrayList> scripts = script_list();
  scripts = WeakArrayList::Append(isolate(), scripts,
                                  MaybeObjectHandle::Weak(script));
  isolate()->heap()->set_script_list(*scripts);
}

Handle<Script> Factory::CloneScript(Handle<Script> script) {
  Heap* heap = isolate()->heap();
  int script_id = isolate()->GetNextScriptId();
  Handle<Script> new_script =
      Handle<Script>::cast(NewStruct(SCRIPT_TYPE, AllocationType::kOld));
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
    Handle<JSPromise> promise_to_resolve, Handle<JSReceiver> thenable,
    Handle<JSReceiver> then, Handle<Context> context) {
  DCHECK(then->IsCallable());
  Handle<PromiseResolveThenableJobTask> microtask =
      Handle<PromiseResolveThenableJobTask>::cast(
          NewStruct(PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE));
  microtask->set_promise_to_resolve(*promise_to_resolve);
  microtask->set_thenable(*thenable);
  microtask->set_then(*then);
  microtask->set_context(*context);
  return microtask;
}

Handle<Foreign> Factory::NewForeign(Address addr) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  STATIC_ASSERT(Foreign::kSize <= kMaxRegularHeapObjectSize);
  Map map = *foreign_map();
  HeapObject result = AllocateRawWithImmortalMap(map.instance_size(),
                                                 AllocationType::kYoung, map);
  Handle<Foreign> foreign(Foreign::cast(result), isolate());
  foreign->AllocateExternalPointerEntries(isolate());
  foreign->set_foreign_address(isolate(), addr);
  return foreign;
}

Handle<WasmTypeInfo> Factory::NewWasmTypeInfo(Address type_address,
                                              Handle<Map> parent) {
  Handle<ArrayList> subtypes = ArrayList::New(isolate(), 0);
  Handle<FixedArray> supertypes;
  if (parent->IsWasmStructMap() || parent->IsWasmArrayMap()) {
    supertypes = CopyFixedArrayAndGrow(
        handle(parent->wasm_type_info().supertypes(), isolate()), 1);
    supertypes->set(supertypes->length() - 1, *parent);
  } else {
    supertypes = NewUninitializedFixedArray(1);
    supertypes->set(0, *parent);
  }
  Map map = *wasm_type_info_map();
  HeapObject result = AllocateRawWithImmortalMap(map.instance_size(),
                                                 AllocationType::kYoung, map);
  Handle<WasmTypeInfo> info(WasmTypeInfo::cast(result), isolate());
  info->AllocateExternalPointerEntries(isolate());
  info->set_foreign_address(isolate(), type_address);
  info->set_parent(*parent);
  info->set_supertypes(*supertypes);
  info->set_subtypes(*subtypes);
  return info;
}

Handle<Cell> Factory::NewCell(Handle<Object> value) {
  STATIC_ASSERT(Cell::kSize <= kMaxRegularHeapObjectSize);
  HeapObject result = AllocateRawWithImmortalMap(
      Cell::kSize, AllocationType::kOld, *cell_map());
  Handle<Cell> cell(Cell::cast(result), isolate());
  cell->set_value(*value);
  return cell;
}

Handle<FeedbackCell> Factory::NewNoClosuresCell(Handle<HeapObject> value) {
  HeapObject result =
      AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
                                 AllocationType::kOld, *no_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->SetInitialInterruptBudget();
  cell->clear_padding();
  return cell;
}

Handle<FeedbackCell> Factory::NewOneClosureCell(Handle<HeapObject> value) {
  HeapObject result =
      AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
                                 AllocationType::kOld, *one_closure_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->SetInitialInterruptBudget();
  cell->clear_padding();
  return cell;
}

Handle<FeedbackCell> Factory::NewManyClosuresCell(Handle<HeapObject> value) {
  HeapObject result = AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
                                                 AllocationType::kOld,
                                                 *many_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->SetInitialInterruptBudget();
  cell->clear_padding();
  return cell;
}

Handle<PropertyCell> Factory::NewPropertyCell(Handle<Name> name,
                                              AllocationType allocation) {
  DCHECK(name->IsUniqueName());
  STATIC_ASSERT(PropertyCell::kSize <= kMaxRegularHeapObjectSize);
  HeapObject result = AllocateRawWithImmortalMap(
      PropertyCell::kSize, allocation, *global_property_cell_map());
  Handle<PropertyCell> cell(PropertyCell::cast(result), isolate());
  cell->set_dependent_code(DependentCode::cast(*empty_weak_fixed_array()),
                           SKIP_WRITE_BARRIER);
  cell->set_property_details(PropertyDetails(Smi::zero()));
  cell->set_name(*name);
  cell->set_value(*the_hole_value());
  return cell;
}

Handle<TransitionArray> Factory::NewTransitionArray(int number_of_transitions,
                                                    int slack) {
  int capacity = TransitionArray::LengthFor(number_of_transitions + slack);
  Handle<TransitionArray> array = Handle<TransitionArray>::cast(
      NewWeakFixedArrayWithMap(read_only_roots().transition_array_map(),
                               capacity, AllocationType::kOld));
  // Transition arrays are AllocationType::kOld. When black allocation is on we
  // have to add the transition array to the list of
  // encountered_transition_arrays.
  Heap* heap = isolate()->heap();
  if (heap->incremental_marking()->black_allocation()) {
    heap->mark_compact_collector()->AddTransitionArray(*array);
  }
  array->WeakFixedArray::Set(TransitionArray::kPrototypeTransitionsIndex,
                             MaybeObject::FromObject(Smi::zero()));
  array->WeakFixedArray::Set(
      TransitionArray::kTransitionLengthIndex,
      MaybeObject::FromObject(Smi::FromInt(number_of_transitions)));
  return array;
}

Handle<AllocationSite> Factory::NewAllocationSite(bool with_weak_next) {
  Handle<Map> map = with_weak_next ? allocation_site_map()
                                   : allocation_site_without_weaknext_map();
  Handle<AllocationSite> site(
      AllocationSite::cast(New(map, AllocationType::kOld)), isolate());
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
  DCHECK_IMPLIES(InstanceTypeChecker::IsJSObject(type) &&
                     !Map::CanHaveFastTransitionableElementsKind(type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind));
  HeapObject result = isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      Map::kSize, AllocationType::kMap);
  result.set_map_after_allocation(*meta_map(), SKIP_WRITE_BARRIER);
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties),
                isolate());
}

Map Factory::InitializeMap(Map map, InstanceType type, int instance_size,
                           ElementsKind elements_kind,
                           int inobject_properties) {
  map.set_instance_type(type);
  map.set_prototype(*null_value(), SKIP_WRITE_BARRIER);
  map.set_constructor_or_backpointer(*null_value(), SKIP_WRITE_BARRIER);
  map.set_instance_size(instance_size);
  if (map.IsJSObjectMap()) {
    DCHECK(!ReadOnlyHeap::Contains(map));
    map.SetInObjectPropertiesStartInWords(instance_size / kTaggedSize -
                                          inobject_properties);
    DCHECK_EQ(map.GetInObjectProperties(), inobject_properties);
    map.set_prototype_validity_cell(*invalid_prototype_validity_cell());
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map.set_inobject_properties_start_or_constructor_function_index(0);
    map.set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  }
  map.set_dependent_code(DependentCode::cast(*empty_weak_fixed_array()),
                         SKIP_WRITE_BARRIER);
  map.set_raw_transitions(MaybeObject::FromSmi(Smi::zero()));
  map.SetInObjectUnusedPropertyFields(inobject_properties);
  map.SetInstanceDescriptors(isolate(), *empty_descriptor_array(), 0);
  if (FLAG_unbox_double_fields) {
    map.set_layout_descriptor(LayoutDescriptor::FastPointerLayout(),
                              kReleaseStore);
  }
  // Must be called only after |instance_type|, |instance_size| and
  // |layout_descriptor| are set.
  map.set_visitor_id(Map::GetVisitorId(map));
  map.set_bit_field(0);
  map.set_bit_field2(Map::Bits2::NewTargetIsBaseBit::encode(true));
  int bit_field3 =
      Map::Bits3::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
      Map::Bits3::OwnsDescriptorsBit::encode(true) |
      Map::Bits3::ConstructionCounterBits::encode(Map::kNoSlackTracking) |
      Map::Bits3::IsExtensibleBit::encode(true);
  map.set_bit_field3(bit_field3);
  DCHECK(!map.is_in_retained_map_list());
  map.clear_padding();
  map.set_elements_kind(elements_kind);
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
  CHECK(map->instance_type() == JS_REG_EXP_TYPE ||
        map->instance_type() == JS_OBJECT_TYPE ||
        map->instance_type() == JS_ERROR_TYPE ||
        map->instance_type() == JS_ARRAY_TYPE ||
        map->instance_type() == JS_API_OBJECT_TYPE ||
        map->instance_type() == WASM_GLOBAL_OBJECT_TYPE ||
        map->instance_type() == WASM_INSTANCE_OBJECT_TYPE ||
        map->instance_type() == WASM_MEMORY_OBJECT_TYPE ||
        map->instance_type() == WASM_MODULE_OBJECT_TYPE ||
        map->instance_type() == WASM_TABLE_OBJECT_TYPE ||
        map->instance_type() == JS_SPECIAL_API_OBJECT_TYPE);
  DCHECK(site.is_null() || AllocationSite::CanTrack(map->instance_type()));

  int object_size = map->instance_size();
  int adjusted_object_size =
      site.is_null() ? object_size : object_size + AllocationMemento::kSize;
  HeapObject raw_clone = isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      adjusted_object_size, AllocationType::kYoung);

  DCHECK(Heap::InYoungGeneration(raw_clone) || FLAG_single_generation);

  Heap::CopyBlock(raw_clone.address(), source->address(), object_size);
  Handle<JSObject> clone(JSObject::cast(raw_clone), isolate());

  if (FLAG_enable_unconditional_write_barriers) {
    // By default, we shouldn't need to update the write barrier here, as the
    // clone will be allocated in new space.
    const ObjectSlot start(raw_clone.address());
    const ObjectSlot end(raw_clone.address() + object_size);
    isolate()->heap()->WriteBarrierForRange(raw_clone, start, end);
  }
  if (!site.is_null()) {
    AllocationMemento alloc_memento = AllocationMemento::unchecked_cast(
        Object(raw_clone.ptr() + object_size));
    InitializeAllocationMemento(alloc_memento, *site);
  }

  SLOW_DCHECK(clone->GetElementsKind() == source->GetElementsKind());
  FixedArrayBase elements = source->elements();
  // Update elements if necessary.
  if (elements.length() > 0) {
    FixedArrayBase elem;
    if (elements.map() == *fixed_cow_array_map()) {
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
    PropertyArray properties = source->property_array();
    if (properties.length() > 0) {
      // TODO(gsathya): Do not copy hash code.
      Handle<PropertyArray> prop = CopyArrayWithMap(
          handle(properties, isolate()), handle(properties.map(), isolate()));
      clone->set_raw_properties_or_hash(*prop);
    }
  } else {
    Handle<FixedArray> properties =
        handle(V8_DICT_MODE_PROTOTYPES_BOOL
                   ? FixedArray::cast(source->property_dictionary_ordered())
                   : FixedArray::cast(source->property_dictionary()),
               isolate());
    Handle<FixedArray> prop = CopyFixedArray(properties);
    clone->set_raw_properties_or_hash(*prop);
  }
  return clone;
}

namespace {
template <typename T>
void initialize_length(Handle<T> array, int length) {
  array->set_length(length);
}

template <>
void initialize_length<PropertyArray>(Handle<PropertyArray> array, int length) {
  array->initialize_length(length);
}

inline void ZeroEmbedderFields(i::Handle<i::JSObject> obj) {
  auto count = obj->GetEmbedderFieldCount();
  for (int i = 0; i < count; i++) {
    obj->SetEmbedderField(i, Smi::zero());
  }
}

}  // namespace

template <typename T>
Handle<T> Factory::CopyArrayWithMap(Handle<T> src, Handle<Map> map) {
  int len = src->length();
  HeapObject obj = AllocateRawFixedArray(len, AllocationType::kYoung);
  obj.set_map_after_allocation(*map, SKIP_WRITE_BARRIER);

  Handle<T> result(T::cast(obj), isolate());
  initialize_length(result, len);

  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  result->CopyElements(isolate(), 0, *src, 0, len, mode);
  return result;
}

template <typename T>
Handle<T> Factory::CopyArrayAndGrow(Handle<T> src, int grow_by,
                                    AllocationType allocation) {
  DCHECK_LT(0, grow_by);
  DCHECK_LE(grow_by, kMaxInt - src->length());
  int old_len = src->length();
  int new_len = old_len + grow_by;
  HeapObject obj = AllocateRawFixedArray(new_len, allocation);
  obj.set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);

  Handle<T> result(T::cast(obj), isolate());
  initialize_length(result, new_len);

  // Copy the content.
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = obj.GetWriteBarrierMode(no_gc);
  result->CopyElements(isolate(), 0, *src, 0, old_len, mode);
  MemsetTagged(ObjectSlot(result->data_start() + old_len),
               ReadOnlyRoots(isolate()).undefined_value(), grow_by);
  return result;
}

Handle<FixedArray> Factory::CopyFixedArrayWithMap(Handle<FixedArray> array,
                                                  Handle<Map> map) {
  return CopyArrayWithMap(array, map);
}

Handle<FixedArray> Factory::CopyFixedArrayAndGrow(Handle<FixedArray> array,
                                                  int grow_by) {
  return CopyArrayAndGrow(array, grow_by, AllocationType::kYoung);
}

Handle<WeakArrayList> Factory::NewUninitializedWeakArrayList(
    int capacity, AllocationType allocation) {
  DCHECK_LE(0, capacity);
  if (capacity == 0) return empty_weak_array_list();

  HeapObject obj = AllocateRawWeakArrayList(capacity, allocation);
  obj.set_map_after_allocation(*weak_array_list_map(), SKIP_WRITE_BARRIER);

  Handle<WeakArrayList> result(WeakArrayList::cast(obj), isolate());
  result->set_length(0);
  result->set_capacity(capacity);
  return result;
}

Handle<WeakArrayList> Factory::NewWeakArrayList(int capacity,
                                                AllocationType allocation) {
  Handle<WeakArrayList> result =
      NewUninitializedWeakArrayList(capacity, allocation);
  MemsetTagged(ObjectSlot(result->data_start()),
               ReadOnlyRoots(isolate()).undefined_value(), capacity);
  return result;
}

Handle<WeakFixedArray> Factory::CopyWeakFixedArrayAndGrow(
    Handle<WeakFixedArray> src, int grow_by) {
  DCHECK(!src->IsTransitionArray());  // Compacted by GC, this code doesn't work
  return CopyArrayAndGrow(src, grow_by, AllocationType::kOld);
}

Handle<WeakArrayList> Factory::CopyWeakArrayListAndGrow(
    Handle<WeakArrayList> src, int grow_by, AllocationType allocation) {
  int old_capacity = src->capacity();
  int new_capacity = old_capacity + grow_by;
  DCHECK_GE(new_capacity, old_capacity);
  Handle<WeakArrayList> result =
      NewUninitializedWeakArrayList(new_capacity, allocation);
  int old_len = src->length();
  result->set_length(old_len);

  // Copy the content.
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  result->CopyElements(isolate(), 0, *src, 0, old_len, mode);
  MemsetTagged(ObjectSlot(result->data_start() + old_len),
               ReadOnlyRoots(isolate()).undefined_value(),
               new_capacity - old_len);
  return result;
}

Handle<WeakArrayList> Factory::CompactWeakArrayList(Handle<WeakArrayList> src,
                                                    int new_capacity,
                                                    AllocationType allocation) {
  Handle<WeakArrayList> result =
      NewUninitializedWeakArrayList(new_capacity, allocation);

  // Copy the content.
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  int copy_to = 0, length = src->length();
  for (int i = 0; i < length; i++) {
    MaybeObject element = src->Get(i);
    if (element->IsCleared()) continue;
    result->Set(copy_to++, element, mode);
  }
  result->set_length(copy_to);

  MemsetTagged(ObjectSlot(result->data_start() + copy_to),
               ReadOnlyRoots(isolate()).undefined_value(),
               new_capacity - copy_to);
  return result;
}

Handle<PropertyArray> Factory::CopyPropertyArrayAndGrow(
    Handle<PropertyArray> array, int grow_by) {
  return CopyArrayAndGrow(array, grow_by, AllocationType::kYoung);
}

Handle<FixedArray> Factory::CopyFixedArrayUpTo(Handle<FixedArray> array,
                                               int new_len,
                                               AllocationType allocation) {
  DCHECK_LE(0, new_len);
  DCHECK_LE(new_len, array->length());
  if (new_len == 0) return empty_fixed_array();

  HeapObject obj = AllocateRawFixedArray(new_len, allocation);
  obj.set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<FixedArray> result(FixedArray::cast(obj), isolate());
  result->set_length(new_len);

  // Copy the content.
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  result->CopyElements(isolate(), 0, *array, 0, new_len, mode);
  return result;
}

Handle<FixedArray> Factory::CopyFixedArray(Handle<FixedArray> array) {
  if (array->length() == 0) return array;
  return CopyArrayWithMap(array, handle(array->map(), isolate()));
}

Handle<FixedArray> Factory::CopyAndTenureFixedCOWArray(
    Handle<FixedArray> array) {
  DCHECK(Heap::InYoungGeneration(*array));
  Handle<FixedArray> result =
      CopyFixedArrayUpTo(array, array->length(), AllocationType::kOld);

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
      Handle<FixedDoubleArray>::cast(NewFixedDoubleArray(len));
  Heap::CopyBlock(
      result->address() + FixedDoubleArray::kLengthOffset,
      array->address() + FixedDoubleArray::kLengthOffset,
      FixedDoubleArray::SizeFor(len) - FixedDoubleArray::kLengthOffset);
  return result;
}

Handle<HeapNumber> Factory::NewHeapNumberForCodeAssembler(double value) {
  return isolate()->heap()->CanAllocateInReadOnlySpace()
             ? NewHeapNumber<AllocationType::kReadOnly>(value)
             : NewHeapNumber<AllocationType::kOld>(value);
}

Handle<JSObject> Factory::NewError(Handle<JSFunction> constructor,
                                   MessageTemplate template_index,
                                   Handle<Object> arg0, Handle<Object> arg1,
                                   Handle<Object> arg2) {
  HandleScope scope(isolate());

  if (arg0.is_null()) arg0 = undefined_value();
  if (arg1.is_null()) arg1 = undefined_value();
  if (arg2.is_null()) arg2 = undefined_value();

  return scope.CloseAndEscape(ErrorUtils::MakeGenericError(
      isolate(), constructor, template_index, arg0, arg1, arg2, SKIP_NONE));
}

Handle<JSObject> Factory::NewError(Handle<JSFunction> constructor,
                                   Handle<String> message) {
  // Construct a new error object. If an exception is thrown, use the exception
  // as the result.

  Handle<Object> no_caller;
  return ErrorUtils::Construct(isolate(), constructor, constructor, message,
                               SKIP_NONE, no_caller,
                               ErrorUtils::StackTraceCollection::kDetailed)
      .ToHandleChecked();
}

Handle<Object> Factory::NewInvalidStringLengthError() {
  if (FLAG_correctness_fuzzer_suppressions) {
    FATAL("Aborting on invalid string length");
  }
  // Invalidate the "string length" protector.
  if (Protectors::IsStringLengthOverflowLookupChainIntact(isolate())) {
    Protectors::InvalidateStringLengthOverflowLookupChain(isolate());
  }
  return NewRangeError(MessageTemplate::kInvalidStringLength);
}

#define DEFINE_ERROR(NAME, name)                                              \
  Handle<JSObject> Factory::New##NAME(                                        \
      MessageTemplate template_index, Handle<Object> arg0,                    \
      Handle<Object> arg1, Handle<Object> arg2) {                             \
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

Handle<JSObject> Factory::NewFunctionPrototype(Handle<JSFunction> function) {
  // Make sure to use globals from the function's context, since the function
  // can be from a different context.
  Handle<NativeContext> native_context(function->context().native_context(),
                                       isolate());
  Handle<Map> new_map;
  if (V8_UNLIKELY(IsAsyncGeneratorFunction(function->shared().kind()))) {
    new_map = handle(native_context->async_generator_object_prototype_map(),
                     isolate());
  } else if (IsResumableFunction(function->shared().kind())) {
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

  if (!IsResumableFunction(function->shared().kind())) {
    JSObject::AddProperty(isolate(), prototype, constructor_string(), function,
                          DONT_ENUM);
  }

  return prototype;
}

Handle<JSObject> Factory::NewExternal(void* value) {
  Handle<Foreign> foreign = NewForeign(reinterpret_cast<Address>(value));
  Handle<JSObject> external = NewJSObjectFromMap(external_map());
  external->SetEmbedderField(0, *foreign);
  return external;
}

Handle<CodeDataContainer> Factory::NewCodeDataContainer(
    int flags, AllocationType allocation) {
  Handle<CodeDataContainer> data_container(
      CodeDataContainer::cast(New(code_data_container_map(), allocation)),
      isolate());
  data_container->set_next_code_link(*undefined_value(), SKIP_WRITE_BARRIER);
  data_container->set_kind_specific_flags(flags);
  data_container->clear_padding();
  return data_container;
}

Handle<Code> Factory::NewOffHeapTrampolineFor(Handle<Code> code,
                                              Address off_heap_entry) {
  CHECK_NOT_NULL(isolate()->embedded_blob_code());
  CHECK_NE(0, isolate()->embedded_blob_code_size());
  CHECK(Builtins::IsIsolateIndependentBuiltin(*code));

  bool generate_jump_to_instruction_stream =
      Builtins::CodeObjectIsExecutable(code->builtin_index());
  Handle<Code> result = Builtins::GenerateOffHeapTrampolineFor(
      isolate(), off_heap_entry,
      code->code_data_container(kAcquireLoad).kind_specific_flags(),
      generate_jump_to_instruction_stream);

  // Trampolines may not contain any metadata since all metadata offsets,
  // stored on the Code object, refer to the off-heap metadata area.
  CHECK_EQ(result->raw_metadata_size(), 0);

  // The CodeDataContainer should not be modified beyond this point since it's
  // now possibly canonicalized.

  // The trampoline code object must inherit specific flags from the original
  // builtin (e.g. the safepoint-table offset). We set them manually here.
  {
    CodePageMemoryModificationScope code_allocation(*result);

    const bool set_is_off_heap_trampoline = true;
    const int stack_slots =
        code->has_safepoint_info() ? code->stack_slots() : 0;
    result->initialize_flags(code->kind(), code->is_turbofanned(), stack_slots,
                             set_is_off_heap_trampoline);
    result->set_builtin_index(code->builtin_index());
    result->set_handler_table_offset(code->handler_table_offset());
    result->set_constant_pool_offset(code->constant_pool_offset());
    result->set_code_comments_offset(code->code_comments_offset());
    result->set_unwinding_info_offset(code->unwinding_info_offset());

    // Replace the newly generated trampoline's RelocInfo ByteArray with the
    // canonical one stored in the roots to avoid duplicating it for every
    // single builtin.
    ByteArray canonical_reloc_info =
        generate_jump_to_instruction_stream
            ? ReadOnlyRoots(isolate()).off_heap_trampoline_relocation_info()
            : ReadOnlyRoots(isolate()).empty_byte_array();
#ifdef DEBUG
    // Verify that the contents are the same.
    ByteArray reloc_info = result->relocation_info();
    DCHECK_EQ(reloc_info.length(), canonical_reloc_info.length());
    for (int i = 0; i < reloc_info.length(); ++i) {
      DCHECK_EQ(reloc_info.get(i), canonical_reloc_info.get(i));
    }
#endif
    result->set_relocation_info(canonical_reloc_info);
  }

  return result;
}

Handle<Code> Factory::CopyCode(Handle<Code> code) {
  Handle<CodeDataContainer> data_container = NewCodeDataContainer(
      code->code_data_container(kAcquireLoad).kind_specific_flags(),
      AllocationType::kOld);

  Heap* heap = isolate()->heap();
  Handle<Code> new_code;
  {
    int obj_size = code->Size();
    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject result = heap->AllocateRawWith<Heap::kRetryOrFail>(
        obj_size, AllocationType::kCode, AllocationOrigin::kRuntime);

    // Copy code object.
    Address old_addr = code->address();
    Address new_addr = result.address();
    Heap::CopyBlock(new_addr, old_addr, obj_size);
    new_code = handle(Code::cast(result), isolate());

    // Set the {CodeDataContainer}, it cannot be shared.
    new_code->set_code_data_container(*data_container, kReleaseStore);

    new_code->Relocate(new_addr - old_addr);
    // We have to iterate over the object and process its pointers when black
    // allocation is on.
    heap->incremental_marking()->ProcessBlackAllocatedObject(*new_code);
    // Record all references to embedded objects in the new code object.
#ifndef V8_DISABLE_WRITE_BARRIERS
    WriteBarrierForCode(*new_code);
#endif
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) new_code->ObjectVerify(isolate());
#endif
  DCHECK(IsAligned(new_code->address(), kCodeAlignment));
  DCHECK_IMPLIES(
      !V8_ENABLE_THIRD_PARTY_HEAP_BOOL &&
      !heap->memory_allocator()->code_range().is_empty(),
      heap->memory_allocator()->code_range().contains(new_code->address()));
  return new_code;
}

Handle<BytecodeArray> Factory::CopyBytecodeArray(
    Handle<BytecodeArray> bytecode_array) {
  int size = BytecodeArray::SizeFor(bytecode_array->length());
  HeapObject result = AllocateRawWithImmortalMap(size, AllocationType::kOld,
                                                 *bytecode_array_map());

  Handle<BytecodeArray> copy(BytecodeArray::cast(result), isolate());
  copy->set_length(bytecode_array->length());
  copy->set_frame_size(bytecode_array->frame_size());
  copy->set_parameter_count(bytecode_array->parameter_count());
  copy->set_incoming_new_target_or_generator_register(
      bytecode_array->incoming_new_target_or_generator_register());
  copy->set_constant_pool(bytecode_array->constant_pool());
  copy->set_handler_table(bytecode_array->handler_table());
  copy->set_source_position_table(
      bytecode_array->source_position_table(kAcquireLoad), kReleaseStore);
  copy->set_osr_loop_nesting_level(bytecode_array->osr_loop_nesting_level());
  copy->set_bytecode_age(bytecode_array->bytecode_age());
  bytecode_array->CopyBytecodesTo(*copy);
  return copy;
}

Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor,
                                      AllocationType allocation) {
  JSFunction::EnsureHasInitialMap(constructor);
  Handle<Map> map(constructor->initial_map(), isolate());
  return NewJSObjectFromMap(map, allocation);
}

Handle<JSObject> Factory::NewJSObjectWithNullProto() {
  Handle<JSObject> result = NewJSObject(isolate()->object_function());
  Handle<Map> new_map = Map::Copy(
      isolate(), Handle<Map>(result->map(), isolate()), "ObjectWithNullProto");
  Map::SetPrototype(isolate(), new_map, null_value());
  JSObject::MigrateToMap(isolate(), result, new_map);
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
  Handle<DescriptorArray> descs(map->instance_descriptors(kRelaxedLoad),
                                isolate());
  for (InternalIndex i : map->IterateOwnDescriptors()) {
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
  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(New(map, AllocationType::kOld)), isolate());
  InitializeJSObjectFromMap(global, dictionary, map);

  // Create a new map for the global object.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate(), map);
  new_map->set_may_have_interesting_symbols(true);
  new_map->set_is_dictionary_map(true);
  LOG(isolate(), MapDetails(*new_map));

  // Set up the global object as a normalized object.
  global->set_global_dictionary(*dictionary, kReleaseStore);
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
  // to a number (e.g. Smi::zero()) and the elements initialized to a
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
  Object filler;
  if (in_progress) {
    filler = *one_pointer_filler_map();
  } else {
    filler = *undefined_value();
  }
  obj->InitializeBody(*map, start_offset, *undefined_value(), filler);
  if (in_progress) {
    map->FindRootMap(isolate()).InobjectSlackTrackingStep(isolate());
  }
}

Handle<JSObject> Factory::NewJSObjectFromMap(
    Handle<Map> map, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  DCHECK(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  DCHECK(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);

  HeapObject obj =
      AllocateRawWithAllocationSite(map, allocation, allocation_site);
  Handle<JSObject> js_obj(JSObject::cast(obj), isolate());

  InitializeJSObjectFromMap(js_obj, empty_fixed_array(), map);

  DCHECK(js_obj->HasFastElements() || js_obj->HasTypedArrayElements() ||
         js_obj->HasFastStringWrapperElements() ||
         js_obj->HasFastArgumentsElements() || js_obj->HasDictionaryElements());
  return js_obj;
}

Handle<JSObject> Factory::NewSlowJSObjectFromMap(
    Handle<Map> map, int capacity, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  DCHECK(map->is_dictionary_map());
  Handle<HeapObject> object_properties;
  if (V8_DICT_MODE_PROTOTYPES_BOOL) {
    object_properties =
        OrderedNameDictionary::Allocate(isolate(), capacity).ToHandleChecked();
  } else {
    object_properties = NameDictionary::New(isolate(), capacity);
  }
  Handle<JSObject> js_object =
      NewJSObjectFromMap(map, allocation, allocation_site);
  js_object->set_raw_properties_or_hash(*object_properties);
  return js_object;
}

Handle<JSObject> Factory::NewSlowJSObjectWithPropertiesAndElements(
    Handle<HeapObject> prototype, Handle<HeapObject> properties,
    Handle<FixedArrayBase> elements) {
  DCHECK_IMPLIES(V8_DICT_MODE_PROTOTYPES_BOOL,
                 properties->IsOrderedNameDictionary());
  DCHECK_IMPLIES(!V8_DICT_MODE_PROTOTYPES_BOOL, properties->IsNameDictionary());

  Handle<Map> object_map = isolate()->slow_object_with_object_prototype_map();
  if (object_map->prototype() != *prototype) {
    object_map = Map::TransitionToPrototype(isolate(), object_map, prototype);
  }
  DCHECK(object_map->is_dictionary_map());
  Handle<JSObject> object =
      NewJSObjectFromMap(object_map, AllocationType::kYoung);
  object->set_raw_properties_or_hash(*properties);
  if (*elements != ReadOnlyRoots(isolate()).empty_fixed_array()) {
    DCHECK(elements->IsNumberDictionary());
    object_map =
        JSObject::GetElementsTransitionMap(object, DICTIONARY_ELEMENTS);
    JSObject::MigrateToMap(isolate(), object, object_map);
    object->set_elements(*elements);
  }
  return object;
}

Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind, int length,
                                    int capacity,
                                    ArrayStorageAllocationMode mode,
                                    AllocationType allocation) {
  DCHECK(capacity >= length);
  if (capacity == 0) {
    return NewJSArrayWithElements(empty_fixed_array(), elements_kind, length,
                                  allocation);
  }

  HandleScope inner_scope(isolate());
  Handle<FixedArrayBase> elms =
      NewJSArrayStorage(elements_kind, capacity, mode);
  return inner_scope.CloseAndEscape(NewJSArrayWithUnverifiedElements(
      elms, elements_kind, length, allocation));
}

Handle<JSArray> Factory::NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                                ElementsKind elements_kind,
                                                int length,
                                                AllocationType allocation) {
  Handle<JSArray> array = NewJSArrayWithUnverifiedElements(
      elements, elements_kind, length, allocation);
  JSObject::ValidateElements(*array);
  return array;
}

Handle<JSArray> Factory::NewJSArrayWithUnverifiedElements(
    Handle<FixedArrayBase> elements, ElementsKind elements_kind, int length,
    AllocationType allocation) {
  DCHECK(length <= elements->length());
  NativeContext native_context = isolate()->raw_native_context();
  Map map = native_context.GetInitialJSArrayMap(elements_kind);
  if (map.is_null()) {
    JSFunction array_function = native_context.array_function();
    map = array_function.initial_map();
  }
  Handle<JSArray> array = Handle<JSArray>::cast(
      NewJSObjectFromMap(handle(map, isolate()), allocation));
  DisallowGarbageCollection no_gc;
  array->set_elements(*elements);
  array->set_length(Smi::FromInt(length));
  return array;
}

void Factory::NewJSArrayStorage(Handle<JSArray> array, int length, int capacity,
                                ArrayStorageAllocationMode mode) {
  DCHECK(capacity >= length);

  if (capacity == 0) {
    array->set_length(Smi::zero());
    array->set_elements(*empty_fixed_array());
    return;
  }

  HandleScope inner_scope(isolate());
  Handle<FixedArrayBase> elms =
      NewJSArrayStorage(array->GetElementsKind(), capacity, mode);

  array->set_elements(*elms);
  array->set_length(Smi::FromInt(length));
}

Handle<FixedArrayBase> Factory::NewJSArrayStorage(
    ElementsKind elements_kind, int capacity, ArrayStorageAllocationMode mode) {
  DCHECK_GT(capacity, 0);
  Handle<FixedArrayBase> elms;
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
  return elms;
}

Handle<JSWeakMap> Factory::NewJSWeakMap() {
  NativeContext native_context = isolate()->raw_native_context();
  Handle<Map> map(native_context.js_weak_map_fun().initial_map(), isolate());
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
      *map, InternalIndex(JSModuleNamespace::kToStringTagFieldIndex));
  module_namespace->FastPropertyAtPut(index,
                                      ReadOnlyRoots(isolate()).Module_string());
  return module_namespace;
}

Handle<JSGeneratorObject> Factory::NewJSGeneratorObject(
    Handle<JSFunction> function) {
  DCHECK(IsResumableFunction(function->shared().kind()));
  JSFunction::EnsureHasInitialMap(function);
  Handle<Map> map(function->initial_map(), isolate());

  DCHECK(map->instance_type() == JS_GENERATOR_OBJECT_TYPE ||
         map->instance_type() == JS_ASYNC_GENERATOR_OBJECT_TYPE);

  return Handle<JSGeneratorObject>::cast(NewJSObjectFromMap(map));
}

Handle<SourceTextModule> Factory::NewSourceTextModule(
    Handle<SharedFunctionInfo> sfi) {
  Handle<SourceTextModuleInfo> module_info(
      sfi->scope_info().ModuleDescriptorInfo(), isolate());
  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), module_info->RegularExportCount());
  Handle<FixedArray> regular_exports =
      NewFixedArray(module_info->RegularExportCount());
  Handle<FixedArray> regular_imports =
      NewFixedArray(module_info->regular_imports().length());
  int requested_modules_length = module_info->module_requests().length();
  Handle<FixedArray> requested_modules =
      requested_modules_length > 0 ? NewFixedArray(requested_modules_length)
                                   : empty_fixed_array();
  Handle<ArrayList> async_parent_modules = ArrayList::New(isolate(), 0);

  ReadOnlyRoots roots(isolate());
  Handle<SourceTextModule> module(
      SourceTextModule::cast(
          New(source_text_module_map(), AllocationType::kOld)),
      isolate());
  module->set_code(*sfi);
  module->set_exports(*exports);
  module->set_regular_exports(*regular_exports);
  module->set_regular_imports(*regular_imports);
  module->set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module->set_module_namespace(roots.undefined_value());
  module->set_requested_modules(*requested_modules);
  module->set_status(Module::kUninstantiated);
  module->set_exception(roots.the_hole_value());
  module->set_import_meta(roots.the_hole_value());
  module->set_dfs_index(-1);
  module->set_dfs_ancestor_index(-1);
  module->set_top_level_capability(roots.undefined_value());
  module->set_flags(0);
  module->set_async(IsAsyncModule(sfi->kind()));
  module->set_async_evaluating(false);
  module->set_async_parent_modules(*async_parent_modules);
  module->set_pending_async_dependencies(0);
  return module;
}

Handle<SyntheticModule> Factory::NewSyntheticModule(
    Handle<String> module_name, Handle<FixedArray> export_names,
    v8::Module::SyntheticModuleEvaluationSteps evaluation_steps) {
  ReadOnlyRoots roots(isolate());

  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), static_cast<int>(export_names->length()));
  Handle<Foreign> evaluation_steps_foreign =
      NewForeign(reinterpret_cast<i::Address>(evaluation_steps));

  Handle<SyntheticModule> module(
      SyntheticModule::cast(New(synthetic_module_map(), AllocationType::kOld)),
      isolate());
  module->set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module->set_module_namespace(roots.undefined_value());
  module->set_status(Module::kUninstantiated);
  module->set_exception(roots.the_hole_value());
  module->set_name(*module_name);
  module->set_export_names(*export_names);
  module->set_exports(*exports);
  module->set_evaluation_steps(*evaluation_steps_foreign);
  return module;
}

Handle<JSArrayBuffer> Factory::NewJSArrayBuffer(
    std::shared_ptr<BackingStore> backing_store, AllocationType allocation) {
  Handle<Map> map(isolate()->native_context()->array_buffer_fun().initial_map(),
                  isolate());
  auto result =
      Handle<JSArrayBuffer>::cast(NewJSObjectFromMap(map, allocation));
  result->Setup(SharedFlag::kNotShared, std::move(backing_store));
  return result;
}

MaybeHandle<JSArrayBuffer> Factory::NewJSArrayBufferAndBackingStore(
    size_t byte_length, InitializedFlag initialized,
    AllocationType allocation) {
  std::unique_ptr<BackingStore> backing_store = nullptr;

  if (byte_length > 0) {
    backing_store = BackingStore::Allocate(isolate(), byte_length,
                                           SharedFlag::kNotShared, initialized);
    if (!backing_store) return MaybeHandle<JSArrayBuffer>();
  }
  Handle<Map> map(isolate()->native_context()->array_buffer_fun().initial_map(),
                  isolate());
  auto array_buffer =
      Handle<JSArrayBuffer>::cast(NewJSObjectFromMap(map, allocation));
  array_buffer->Setup(SharedFlag::kNotShared, std::move(backing_store));
  return array_buffer;
}

Handle<JSArrayBuffer> Factory::NewJSSharedArrayBuffer(
    std::shared_ptr<BackingStore> backing_store) {
  Handle<Map> map(
      isolate()->native_context()->shared_array_buffer_fun().initial_map(),
      isolate());
  auto result = Handle<JSArrayBuffer>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  result->Setup(SharedFlag::kShared, std::move(backing_store));
  return result;
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

void ForFixedTypedArray(ExternalArrayType array_type, size_t* element_size,
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

}  // namespace

Handle<JSArrayBufferView> Factory::NewJSArrayBufferView(
    Handle<Map> map, Handle<FixedArrayBase> elements,
    Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length) {
  CHECK_LE(byte_length, buffer->byte_length());
  CHECK_LE(byte_offset, buffer->byte_length());
  CHECK_LE(byte_offset + byte_length, buffer->byte_length());
  Handle<JSArrayBufferView> array_buffer_view = Handle<JSArrayBufferView>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  array_buffer_view->set_elements(*elements);
  array_buffer_view->set_buffer(*buffer);
  array_buffer_view->set_byte_offset(byte_offset);
  array_buffer_view->set_byte_length(byte_length);
  ZeroEmbedderFields(array_buffer_view);
  DCHECK_EQ(array_buffer_view->GetEmbedderFieldCount(),
            v8::ArrayBufferView::kEmbedderFieldCount);
  return array_buffer_view;
}

Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              Handle<JSArrayBuffer> buffer,
                                              size_t byte_offset,
                                              size_t length) {
  size_t element_size;
  ElementsKind elements_kind;
  ForFixedTypedArray(type, &element_size, &elements_kind);
  size_t byte_length = length * element_size;

  CHECK_LE(length, JSTypedArray::kMaxLength);
  CHECK_EQ(length, byte_length / element_size);
  CHECK_EQ(0, byte_offset % ElementsKindToByteSize(elements_kind));

  Handle<Map> map;
  switch (elements_kind) {
#define TYPED_ARRAY_FUN(Type, type, TYPE, ctype)                              \
  case TYPE##_ELEMENTS:                                                       \
    map =                                                                     \
        handle(isolate()->native_context()->type##_array_fun().initial_map(), \
               isolate());                                                    \
    break;

    TYPED_ARRAYS(TYPED_ARRAY_FUN)
#undef TYPED_ARRAY_FUN

    default:
      UNREACHABLE();
  }
  Handle<JSTypedArray> typed_array =
      Handle<JSTypedArray>::cast(NewJSArrayBufferView(
          map, empty_byte_array(), buffer, byte_offset, byte_length));
  typed_array->AllocateExternalPointerEntries(isolate());
  typed_array->set_length(length);
  typed_array->SetOffHeapDataPtr(isolate(), buffer->backing_store(),
                                 byte_offset);
  return typed_array;
}

Handle<JSDataView> Factory::NewJSDataView(Handle<JSArrayBuffer> buffer,
                                          size_t byte_offset,
                                          size_t byte_length) {
  Handle<Map> map(isolate()->native_context()->data_view_fun().initial_map(),
                  isolate());
  Handle<JSDataView> obj = Handle<JSDataView>::cast(NewJSArrayBufferView(
      map, empty_fixed_array(), buffer, byte_offset, byte_length));
  obj->AllocateExternalPointerEntries(isolate());
  obj->set_data_pointer(
      isolate(), static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
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
  Handle<HeapObject> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), prototype,
      JSReceiver::GetPrototype(isolate(), target_function), JSBoundFunction);

  SaveAndSwitchContext save(isolate(), *target_function->GetCreationContext());

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
  DCHECK(map->prototype().IsNull(isolate()));
  Handle<JSProxy> result(JSProxy::cast(New(map, AllocationType::kYoung)),
                         isolate());
  result->initialize_properties(isolate());
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
  LOG(isolate(), MapDetails(*map));
  Handle<JSGlobalProxy> proxy = Handle<JSGlobalProxy>::cast(
      NewJSObjectFromMap(map, AllocationType::kOld));
  // Create identity hash early in case there is any JS collection containing
  // a global proxy key and needs to be rehashed after deserialization.
  proxy->GetOrCreateIdentityHash(isolate());
  return proxy;
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
  DisallowGarbageCollection no_gc;

  // Reset the map for the object.
  object->synchronized_set_map(*map);

  // Reinitialize the object from the constructor map.
  InitializeJSObjectFromMap(object, raw_properties_or_hash, map);
}

Handle<JSMessageObject> Factory::NewJSMessageObject(
    MessageTemplate message, Handle<Object> argument, int start_position,
    int end_position, Handle<SharedFunctionInfo> shared_info,
    int bytecode_offset, Handle<Script> script, Handle<Object> stack_frames) {
  Handle<Map> map = message_object_map();
  Handle<JSMessageObject> message_obj(
      JSMessageObject::cast(New(map, AllocationType::kYoung)), isolate());
  message_obj->set_raw_properties_or_hash(*empty_fixed_array(),
                                          SKIP_WRITE_BARRIER);
  message_obj->initialize_elements();
  message_obj->set_elements(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message_obj->set_type(message);
  message_obj->set_argument(*argument);
  message_obj->set_start_position(start_position);
  message_obj->set_end_position(end_position);
  message_obj->set_script(*script);
  if (start_position >= 0) {
    // If there's a start_position, then there's no need to store the
    // SharedFunctionInfo as it will never be necessary to regenerate the
    // position.
    message_obj->set_shared_info(*undefined_value());
    message_obj->set_bytecode_offset(Smi::FromInt(0));
  } else {
    message_obj->set_bytecode_offset(Smi::FromInt(bytecode_offset));
    if (shared_info.is_null()) {
      message_obj->set_shared_info(*undefined_value());
      DCHECK_EQ(bytecode_offset, -1);
    } else {
      message_obj->set_shared_info(*shared_info);
      DCHECK_GE(bytecode_offset, kFunctionEntryBytecodeOffset);
    }
  }

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

Handle<SharedFunctionInfo>
Factory::NewSharedFunctionInfoForWasmExportedFunction(
    Handle<String> name, Handle<WasmExportedFunctionData> data) {
  return NewSharedFunctionInfo(name, data, Builtins::kNoBuiltinId);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForWasmJSFunction(
    Handle<String> name, Handle<WasmJSFunctionData> data) {
  return NewSharedFunctionInfo(name, data, Builtins::kNoBuiltinId);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForWasmCapiFunction(
    Handle<WasmCapiFunctionData> data) {
  return NewSharedFunctionInfo(MaybeHandle<String>(), data,
                               Builtins::kNoBuiltinId, kConciseMethod);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForBuiltin(
    MaybeHandle<String> maybe_name, int builtin_index, FunctionKind kind) {
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo(
      maybe_name, MaybeHandle<Code>(), builtin_index, kind);
  return shared;
}

namespace {
V8_INLINE int NumberToStringCacheHash(Handle<FixedArray> cache, Smi number) {
  int mask = (cache->length() >> 1) - 1;
  return number.value() & mask;
}

V8_INLINE int NumberToStringCacheHash(Handle<FixedArray> cache, double number) {
  int mask = (cache->length() >> 1) - 1;
  int64_t bits = bit_cast<int64_t>(number);
  return (static_cast<int>(bits) ^ static_cast<int>(bits >> 32)) & mask;
}

V8_INLINE Handle<String> CharToString(Factory* factory, const char* string,
                                      NumberCacheMode mode) {
  // We tenure the allocated string since it is referenced from the
  // number-string cache which lives in the old space.
  AllocationType type = mode == NumberCacheMode::kIgnore
                            ? AllocationType::kYoung
                            : AllocationType::kOld;
  return factory->NewStringFromAsciiChecked(string, type);
}

}  // namespace

void Factory::NumberToStringCacheSet(Handle<Object> number, int hash,
                                     Handle<String> js_string) {
  if (!number_string_cache()->get(hash * 2).IsUndefined(isolate()) &&
      !FLAG_optimize_for_size) {
    int full_size = isolate()->heap()->MaxNumberToStringCacheSize();
    if (number_string_cache()->length() != full_size) {
      Handle<FixedArray> new_cache =
          NewFixedArray(full_size, AllocationType::kOld);
      isolate()->heap()->set_number_string_cache(*new_cache);
      return;
    }
  }
  number_string_cache()->set(hash * 2, *number);
  number_string_cache()->set(hash * 2 + 1, *js_string);
}

Handle<Object> Factory::NumberToStringCacheGet(Object number, int hash) {
  DisallowGarbageCollection no_gc;
  Object key = number_string_cache()->get(hash * 2);
  if (key == number || (key.IsHeapNumber() && number.IsHeapNumber() &&
                        key.Number() == number.Number())) {
    return Handle<String>(
        String::cast(number_string_cache()->get(hash * 2 + 1)), isolate());
  }
  return undefined_value();
}

Handle<String> Factory::NumberToString(Handle<Object> number,
                                       NumberCacheMode mode) {
  if (number->IsSmi()) return SmiToString(Smi::cast(*number), mode);

  double double_value = Handle<HeapNumber>::cast(number)->value();
  // Try to canonicalize doubles.
  int smi_value;
  if (DoubleToSmiInteger(double_value, &smi_value)) {
    return SmiToString(Smi::FromInt(smi_value), mode);
  }
  return HeapNumberToString(Handle<HeapNumber>::cast(number), double_value,
                            mode);
}

// Must be large enough to fit any double, int, or size_t.
static const int kNumberToStringBufferSize = 32;

Handle<String> Factory::HeapNumberToString(Handle<HeapNumber> number,
                                           double value, NumberCacheMode mode) {
  int hash = 0;
  if (mode != NumberCacheMode::kIgnore) {
    hash = NumberToStringCacheHash(number_string_cache(), value);
  }
  if (mode == NumberCacheMode::kBoth) {
    Handle<Object> cached = NumberToStringCacheGet(*number, hash);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[kNumberToStringBufferSize];
  Vector<char> buffer(arr, arraysize(arr));
  const char* string = DoubleToCString(value, buffer);
  Handle<String> result = CharToString(this, string, mode);
  if (mode != NumberCacheMode::kIgnore) {
    NumberToStringCacheSet(number, hash, result);
  }
  return result;
}

inline Handle<String> Factory::SmiToString(Smi number, NumberCacheMode mode) {
  int hash = NumberToStringCacheHash(number_string_cache(), number);
  if (mode == NumberCacheMode::kBoth) {
    Handle<Object> cached = NumberToStringCacheGet(number, hash);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[kNumberToStringBufferSize];
  Vector<char> buffer(arr, arraysize(arr));
  const char* string = IntToCString(number.value(), buffer);
  Handle<String> result = CharToString(this, string, mode);
  if (mode != NumberCacheMode::kIgnore) {
    NumberToStringCacheSet(handle(number, isolate()), hash, result);
  }

  // Compute the hash here (rather than letting the caller take care of it) so
  // that the "cache hit" case above doesn't have to bother with it.
  STATIC_ASSERT(Smi::kMaxValue <= std::numeric_limits<uint32_t>::max());
  if (result->raw_hash_field() == String::kEmptyHashField &&
      number.value() >= 0) {
    uint32_t raw_hash_field = StringHasher::MakeArrayIndexHash(
        static_cast<uint32_t>(number.value()), result->length());
    result->set_raw_hash_field(raw_hash_field);
  }
  return result;
}

Handle<String> Factory::SizeToString(size_t value, bool check_cache) {
  Handle<String> result;
  NumberCacheMode cache_mode =
      check_cache ? NumberCacheMode::kBoth : NumberCacheMode::kIgnore;
  if (value <= Smi::kMaxValue) {
    int32_t int32v = static_cast<int32_t>(static_cast<uint32_t>(value));
    // SmiToString sets the hash when needed, we can return immediately.
    return SmiToString(Smi::FromInt(int32v), cache_mode);
  } else if (value <= kMaxSafeInteger) {
    // TODO(jkummerow): Refactor the cache to not require Objects as keys.
    double double_value = static_cast<double>(value);
    result = HeapNumberToString(NewHeapNumber(double_value), value, cache_mode);
  } else {
    char arr[kNumberToStringBufferSize];
    Vector<char> buffer(arr, arraysize(arr));
    // Build the string backwards from the least significant digit.
    int i = buffer.length();
    size_t value_copy = value;
    buffer[--i] = '\0';
    do {
      buffer[--i] = '0' + (value_copy % 10);
      value_copy /= 10;
    } while (value_copy > 0);
    char* string = buffer.begin() + i;
    // No way to cache this; we'd need an {Object} to use as key.
    result = NewStringFromAsciiChecked(string);
  }
  if (value <= JSArray::kMaxArrayIndex &&
      result->raw_hash_field() == String::kEmptyHashField) {
    uint32_t raw_hash_field = StringHasher::MakeArrayIndexHash(
        static_cast<uint32_t>(value), result->length());
    result->set_raw_hash_field(raw_hash_field);
  }
  return result;
}

Handle<DebugInfo> Factory::NewDebugInfo(Handle<SharedFunctionInfo> shared) {
  DCHECK(!shared->HasDebugInfo());
  Heap* heap = isolate()->heap();

  Handle<DebugInfo> debug_info =
      Handle<DebugInfo>::cast(NewStruct(DEBUG_INFO_TYPE, AllocationType::kOld));
  debug_info->set_flags(DebugInfo::kNone);
  debug_info->set_shared(*shared);
  debug_info->set_debugger_hints(0);
  DCHECK_EQ(DebugInfo::kNoDebuggingId, debug_info->debugging_id());
  debug_info->set_script(shared->script_or_debug_info(kAcquireLoad));
  debug_info->set_original_bytecode_array(ReadOnlyRoots(heap).undefined_value(),
                                          kReleaseStore);
  debug_info->set_debug_bytecode_array(ReadOnlyRoots(heap).undefined_value(),
                                       kReleaseStore);
  debug_info->set_break_points(ReadOnlyRoots(heap).empty_fixed_array());

  // Link debug info to function.
  shared->SetDebugInfo(*debug_info);

  return debug_info;
}

Handle<WasmValue> Factory::NewWasmValue(int value_type, Handle<Object> ref) {
  DCHECK(value_type == 6 || ref->IsByteArray());
  Handle<WasmValue> wasm_value =
      Handle<WasmValue>::cast(NewStruct(WASM_VALUE_TYPE, AllocationType::kOld));
  wasm_value->set_value_type(value_type);
  wasm_value->set_bytes_or_ref(*ref);
  return wasm_value;
}

Handle<BreakPointInfo> Factory::NewBreakPointInfo(int source_position) {
  Handle<BreakPointInfo> new_break_point_info = Handle<BreakPointInfo>::cast(
      NewStruct(BREAK_POINT_INFO_TYPE, AllocationType::kOld));
  new_break_point_info->set_source_position(source_position);
  new_break_point_info->set_break_points(*undefined_value());
  return new_break_point_info;
}

Handle<BreakPoint> Factory::NewBreakPoint(int id, Handle<String> condition) {
  Handle<BreakPoint> new_break_point = Handle<BreakPoint>::cast(
      NewStruct(BREAK_POINT_TYPE, AllocationType::kOld));
  new_break_point->set_id(id);
  new_break_point->set_condition(*condition);
  return new_break_point;
}

Handle<StackTraceFrame> Factory::NewStackTraceFrame(
    Handle<FrameArray> frame_array, int index) {
  Handle<StackTraceFrame> frame = Handle<StackTraceFrame>::cast(
      NewStruct(STACK_TRACE_FRAME_TYPE, AllocationType::kYoung));
  frame->set_frame_array(*frame_array);
  frame->set_frame_index(index);
  frame->set_frame_info(*undefined_value());

  return frame;
}

Handle<StackFrameInfo> Factory::NewStackFrameInfo(
    Handle<FrameArray> frame_array, int index) {
  FrameArrayIterator it(isolate(), frame_array, index);
  DCHECK(it.HasFrame());

  const bool is_wasm = frame_array->IsAnyWasmFrame(index);
  StackFrameBase* frame = it.Frame();

  int line = frame->GetLineNumber();
  int column = frame->GetColumnNumber();
  int wasm_function_index = frame->GetWasmFunctionIndex();

  const int script_id = frame->GetScriptId();

  Handle<Object> script_name = frame->GetFileName();
  Handle<Object> script_or_url = frame->GetScriptNameOrSourceUrl();

  // TODO(szuend): Adjust this, once it is decided what name to use in both
  //               "simple" and "detailed" stack traces. This code is for
  //               backwards compatibility to fullfill test expectations.
  Handle<PrimitiveHeapObject> function_name = frame->GetFunctionName();
  bool is_user_java_script = false;
  if (!is_wasm) {
    Handle<Object> function = frame->GetFunction();
    if (function->IsJSFunction()) {
      Handle<JSFunction> fun = Handle<JSFunction>::cast(function);

      is_user_java_script = fun->shared().IsUserJavaScript();
    }
  }

  Handle<PrimitiveHeapObject> method_name = undefined_value();
  Handle<PrimitiveHeapObject> type_name = undefined_value();
  Handle<PrimitiveHeapObject> eval_origin = frame->GetEvalOrigin();
  Handle<PrimitiveHeapObject> wasm_module_name = frame->GetWasmModuleName();
  Handle<HeapObject> wasm_instance = frame->GetWasmInstance();

  // MethodName and TypeName are expensive to look up, so they are only
  // included when they are strictly needed by the stack trace
  // serialization code.
  // Note: The {is_method_call} predicate needs to be kept in sync with
  //       the corresponding predicate in the stack trace serialization code
  //       in stack-frame-info.cc.
  const bool is_toplevel = frame->IsToplevel();
  const bool is_constructor = frame->IsConstructor();
  const bool is_method_call = !(is_toplevel || is_constructor);
  if (is_method_call) {
    method_name = frame->GetMethodName();
    type_name = frame->GetTypeName();
  }

  Handle<StackFrameInfo> info = Handle<StackFrameInfo>::cast(
      NewStruct(STACK_FRAME_INFO_TYPE, AllocationType::kYoung));

  DisallowGarbageCollection no_gc;

  info->set_flag(0);
  info->set_is_wasm(is_wasm);
  info->set_is_asmjs_wasm(frame_array->IsAsmJsWasmFrame(index));
  info->set_is_user_java_script(is_user_java_script);
  info->set_line_number(line);
  info->set_column_number(column);
  info->set_wasm_function_index(wasm_function_index);
  info->set_script_id(script_id);

  info->set_script_name(*script_name);
  info->set_script_name_or_source_url(*script_or_url);
  info->set_function_name(*function_name);
  info->set_method_name(*method_name);
  info->set_type_name(*type_name);
  info->set_eval_origin(*eval_origin);
  info->set_wasm_module_name(*wasm_module_name);
  info->set_wasm_instance(*wasm_instance);

  info->set_is_eval(frame->IsEval());
  info->set_is_constructor(is_constructor);
  info->set_is_toplevel(is_toplevel);
  info->set_is_async(frame->IsAsync());
  info->set_is_promise_all(frame->IsPromiseAll());
  info->set_is_promise_any(frame->IsPromiseAny());
  info->set_promise_combinator_index(frame->GetPromiseIndex());

  return info;
}

Handle<JSObject> Factory::NewArgumentsObject(Handle<JSFunction> callee,
                                             int length) {
  bool strict_mode_callee = is_strict(callee->shared().language_mode()) ||
                            !callee->shared().has_simple_parameters();
  Handle<Map> map = strict_mode_callee ? isolate()->strict_arguments_map()
                                       : isolate()->sloppy_arguments_map();
  AllocationSiteUsageContext context(isolate(), Handle<AllocationSite>(),
                                     false);
  DCHECK(!isolate()->has_pending_exception());
  Handle<JSObject> result = NewJSObjectFromMap(map);
  Handle<Smi> value(Smi::FromInt(length), isolate());
  Object::SetProperty(isolate(), result, length_string(), value,
                      StoreOrigin::kMaybeKeyed,
                      Just(ShouldThrow::kThrowOnError))
      .Assert();
  if (!strict_mode_callee) {
    Object::SetProperty(isolate(), result, callee_string(), callee,
                        StoreOrigin::kMaybeKeyed,
                        Just(ShouldThrow::kThrowOnError))
        .Assert();
  }
  return result;
}

Handle<Map> Factory::ObjectLiteralMapFromCache(Handle<NativeContext> context,
                                               int number_of_properties) {
  if (number_of_properties == 0) {
    // Reuse the initial map of the Object function if the literal has no
    // predeclared properties.
    return handle(context->object_function().initial_map(), isolate());
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
    maybe_cache = NewWeakFixedArray(kMapCacheSize, AllocationType::kOld);
    context->set_map_cache(*maybe_cache);
  } else {
    // Check to see whether there is a matching element in the cache.
    Handle<WeakFixedArray> cache = Handle<WeakFixedArray>::cast(maybe_cache);
    MaybeObject result = cache->Get(cache_index);
    HeapObject heap_object;
    if (result->GetHeapObjectIfWeak(&heap_object)) {
      Map map = Map::cast(heap_object);
      DCHECK(!map.is_dictionary_map());
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

Handle<LoadHandler> Factory::NewLoadHandler(int data_count,
                                            AllocationType allocation) {
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
  }
  return handle(LoadHandler::cast(New(map, allocation)), isolate());
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
  }
  return handle(StoreHandler::cast(New(map, AllocationType::kOld)), isolate());
}

void Factory::SetRegExpAtomData(Handle<JSRegExp> regexp, Handle<String> source,
                                JSRegExp::Flags flags, Handle<Object> data) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kAtomDataSize);

  store->set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::ATOM));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kAtomPatternIndex, *data);
  regexp->set_data(*store);
}

void Factory::SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                                    Handle<String> source,
                                    JSRegExp::Flags flags, int capture_count,
                                    uint32_t backtrack_limit) {
  DCHECK(Smi::IsValid(backtrack_limit));
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kIrregexpDataSize);
  Smi uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);
  Smi ticks_until_tier_up = FLAG_regexp_tier_up
                                ? Smi::FromInt(FLAG_regexp_tier_up_ticks)
                                : uninitialized;
  store->set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::IRREGEXP));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpLatin1BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::zero());
  store->set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store->set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  store->set(JSRegExp::kIrregexpTicksUntilTierUpIndex, ticks_until_tier_up);
  store->set(JSRegExp::kIrregexpBacktrackLimit, Smi::FromInt(backtrack_limit));
  regexp->set_data(*store);
}

void Factory::SetRegExpExperimentalData(Handle<JSRegExp> regexp,
                                        Handle<String> source,
                                        JSRegExp::Flags flags,
                                        int capture_count) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kExperimentalDataSize);
  Smi uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);

  store->set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::EXPERIMENTAL));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpLatin1BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, uninitialized);
  store->set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store->set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  store->set(JSRegExp::kIrregexpTicksUntilTierUpIndex, uninitialized);
  store->set(JSRegExp::kIrregexpBacktrackLimit, uninitialized);
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
      JS_FUNCTION_TYPE, header_size + inobject_properties_count * kTaggedSize,
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
    map->AppendDescriptor(isolate(), &d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  if (IsFunctionModeWithName(function_mode)) {
    // Add name field.
    Handle<Name> name = isolate()->factory()->name_string();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         roc_attribs, Representation::Tagged());
    map->AppendDescriptor(isolate(), &d);

  } else {
    // Add name accessor.
    Descriptor d = Descriptor::AccessorConstant(
        name_string(), function_name_accessor(), roc_attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  {  // Add arguments accessor.
    Descriptor d = Descriptor::AccessorConstant(
        arguments_string(), function_arguments_accessor(), ro_attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  {  // Add caller accessor.
    Descriptor d = Descriptor::AccessorConstant(
        caller_string(), function_caller_accessor(), ro_attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype accessor.
    PropertyAttributes attribs =
        IsFunctionModeWithWritablePrototype(function_mode) ? rw_attribs
                                                           : ro_attribs;
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  DCHECK_EQ(inobject_properties_count, field_index);
  DCHECK_EQ(
      0, map->instance_descriptors(kRelaxedLoad).number_of_slack_descriptors());
  LOG(isolate(), MapDetails(*map));
  return map;
}

Handle<Map> Factory::CreateStrictFunctionMap(
    FunctionMode function_mode, Handle<JSFunction> empty_function) {
  bool has_prototype = IsFunctionModeWithPrototype(function_mode);
  int header_size = has_prototype ? JSFunction::kSizeWithPrototype
                                  : JSFunction::kSizeWithoutPrototype;
  int inobject_properties_count = 0;
  // length and prototype accessors or just length accessor.
  int descriptors_count = IsFunctionModeWithPrototype(function_mode) ? 2 : 1;
  if (IsFunctionModeWithName(function_mode)) {
    ++inobject_properties_count;  // name property.
  } else {
    ++descriptors_count;  // name accessor.
  }
  if (IsFunctionModeWithHomeObject(function_mode)) ++inobject_properties_count;
  descriptors_count += inobject_properties_count;

  Handle<Map> map = NewMap(
      JS_FUNCTION_TYPE, header_size + inobject_properties_count * kTaggedSize,
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
    map->AppendDescriptor(isolate(), &d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  if (IsFunctionModeWithName(function_mode)) {
    // Add name field.
    Handle<Name> name = isolate()->factory()->name_string();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         roc_attribs, Representation::Tagged());
    map->AppendDescriptor(isolate(), &d);

  } else {
    // Add name accessor.
    Descriptor d = Descriptor::AccessorConstant(
        name_string(), function_name_accessor(), roc_attribs);
    map->AppendDescriptor(isolate(), &d);
  }

  STATIC_ASSERT(JSFunction::kMaybeHomeObjectDescriptorIndex == 2);
  if (IsFunctionModeWithHomeObject(function_mode)) {
    // Add home object field.
    Handle<Name> name = isolate()->factory()->home_object_symbol();
    Descriptor d = Descriptor::DataField(isolate(), name, field_index++,
                                         DONT_ENUM, Representation::Tagged());
    map->AppendDescriptor(isolate(), &d);
  }

  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype accessor.
    PropertyAttributes attribs =
        IsFunctionModeWithWritablePrototype(function_mode) ? rw_attribs
                                                           : ro_attribs;
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  DCHECK_EQ(inobject_properties_count, field_index);
  DCHECK_EQ(
      0, map->instance_descriptors(kRelaxedLoad).number_of_slack_descriptors());
  LOG(isolate(), MapDetails(*map));
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
    map->AppendDescriptor(isolate(), &d);
  }

  {
    // Add prototype accessor.
    Descriptor d = Descriptor::AccessorConstant(
        prototype_string(), function_prototype_accessor(), ro_attribs);
    map->AppendDescriptor(isolate(), &d);
  }
  LOG(isolate(), MapDetails(*map));
  return map;
}

Handle<JSPromise> Factory::NewJSPromiseWithoutHook() {
  Handle<JSPromise> promise =
      Handle<JSPromise>::cast(NewJSObject(isolate()->promise_function()));
  promise->set_reactions_or_result(Smi::zero());
  promise->set_flags(0);
  ZeroEmbedderFields(promise);
  DCHECK_EQ(promise->GetEmbedderFieldCount(), v8::Promise::kEmbedderFieldCount);
  return promise;
}

Handle<JSPromise> Factory::NewJSPromise() {
  Handle<JSPromise> promise = NewJSPromiseWithoutHook();
  isolate()->RunPromiseHook(PromiseHookType::kInit, promise, undefined_value());
  return promise;
}

Handle<CallHandlerInfo> Factory::NewCallHandlerInfo(bool has_no_side_effect) {
  Handle<Map> map = has_no_side_effect
                        ? side_effect_free_call_handler_info_map()
                        : side_effect_call_handler_info_map();
  Handle<CallHandlerInfo> info(
      CallHandlerInfo::cast(New(map, AllocationType::kOld)), isolate());
  Object undefined_value = ReadOnlyRoots(isolate()).undefined_value();
  info->set_callback(undefined_value);
  info->set_js_callback(undefined_value);
  info->set_data(undefined_value);
  return info;
}

bool Factory::CanAllocateInReadOnlySpace() {
  return isolate()->heap()->CanAllocateInReadOnlySpace();
}

bool Factory::EmptyStringRootIsInitialized() {
  return isolate()->roots_table()[RootIndex::kempty_string] != kNullAddress;
}

Handle<JSFunction> Factory::NewFunctionForTesting(Handle<String> name) {
  Handle<SharedFunctionInfo> info =
      NewSharedFunctionInfoForBuiltin(name, Builtins::kIllegal);
  info->set_language_mode(LanguageMode::kSloppy);
  return JSFunctionBuilder{isolate(), info, isolate()->native_context()}
      .Build();
}

Factory::JSFunctionBuilder::JSFunctionBuilder(Isolate* isolate,
                                              Handle<SharedFunctionInfo> sfi,
                                              Handle<Context> context)
    : isolate_(isolate), sfi_(sfi), context_(context) {}

Handle<JSFunction> Factory::JSFunctionBuilder::Build() {
  PrepareMap();
  PrepareFeedbackCell();

  // Determine the associated Code object.
  Handle<Code> code;
  const bool have_cached_code =
      sfi_->TryGetCachedCode(isolate_).ToHandle(&code);
  if (!have_cached_code) code = handle(sfi_->GetCode(), isolate_);

  Handle<JSFunction> result = BuildRaw(code);

  if (have_cached_code) {
    IsCompiledScope is_compiled_scope(sfi_->is_compiled_scope(isolate_));
    JSFunction::EnsureFeedbackVector(result, &is_compiled_scope);
    if (FLAG_trace_turbo_nci) CompilationCacheCode::TraceHit(sfi_, code);
  }

  Compiler::PostInstantiation(result);
  return result;
}

Handle<JSFunction> Factory::JSFunctionBuilder::BuildRaw(Handle<Code> code) {
  Isolate* isolate = isolate_;
  Factory* factory = isolate_->factory();

  Handle<Map> map = maybe_map_.ToHandleChecked();
  Handle<FeedbackCell> feedback_cell = maybe_feedback_cell_.ToHandleChecked();

  DCHECK_EQ(JS_FUNCTION_TYPE, map->instance_type());

  // Allocation.
  Handle<JSFunction> function(
      JSFunction::cast(factory->New(map, allocation_type_)), isolate);

  // Header initialization.
  function->initialize_properties(isolate);
  function->initialize_elements();
  function->set_shared(*sfi_);
  function->set_context(*context_);
  function->set_raw_feedback_cell(*feedback_cell);
  function->set_code(*code);
  if (map->has_prototype_slot()) {
    function->set_prototype_or_initial_map(
        ReadOnlyRoots(isolate).the_hole_value());
  }

  // Potentially body initialization.
  factory->InitializeJSObjectBody(
      function, map, JSFunction::GetHeaderSize(map->has_prototype_slot()));

  return function;
}

void Factory::JSFunctionBuilder::PrepareMap() {
  if (maybe_map_.is_null()) {
    // No specific map requested, use the default.
    maybe_map_ = handle(
        Map::cast(context_->native_context().get(sfi_->function_map_index())),
        isolate_);
  }
}

void Factory::JSFunctionBuilder::PrepareFeedbackCell() {
  Handle<FeedbackCell> feedback_cell;
  if (maybe_feedback_cell_.ToHandle(&feedback_cell)) {
    // Track the newly-created closure, and check that the optimized code in
    // the feedback cell wasn't marked for deoptimization while not pointed to
    // by any live JSFunction.
    feedback_cell->IncrementClosureCount(isolate_);
    if (feedback_cell->value().IsFeedbackVector()) {
      FeedbackVector::cast(feedback_cell->value())
          .EvictOptimizedCodeMarkedForDeoptimization(
              *sfi_, "new function from shared function info");
    }
  } else {
    // Fall back to the many_closures_cell.
    maybe_feedback_cell_ = isolate_->factory()->many_closures_cell();
  }
}

}  // namespace internal
}  // namespace v8
