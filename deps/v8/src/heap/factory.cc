// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/builtins/accessors.h"
#include "src/builtins/constants-table-builder.h"
#include "src/codegen/compiler.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
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
#include "src/objects/scope-info.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/strings/unicode-inl.h"

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

}  // namespace

Factory::CodeBuilder::CodeBuilder(Isolate* isolate, const CodeDesc& desc,
                                  Code::Kind kind)
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

  Handle<Code> code;
  {
    int object_size = ComputeCodeObjectSize(code_desc_);
    Heap* heap = isolate_->heap();

    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject result;
    if (retry_allocation_or_fail) {
      result = heap->AllocateRawWith<Heap::kRetryOrFail>(object_size,
                                                         AllocationType::kCode);
    } else {
      result = heap->AllocateRawWith<Heap::kLightRetry>(object_size,
                                                        AllocationType::kCode);
      // Return an empty handle if we cannot allocate the code object.
      if (result.is_null()) return MaybeHandle<Code>();
    }

    if (!is_movable_) {
      result = heap->EnsureImmovableCode(result, object_size);
    }

    // The code object has not been fully initialized yet.  We rely on the
    // fact that no allocation will happen from this point on.
    DisallowHeapAllocation no_gc;

    result.set_map_after_allocation(*factory->code_map(), SKIP_WRITE_BARRIER);
    code = handle(Code::cast(result), isolate_);
    DCHECK(IsAligned(code->address(), kCodeAlignment));
    DCHECK_IMPLIES(
        !heap->memory_allocator()->code_range().is_empty(),
        heap->memory_allocator()->code_range().contains(code->address()));

    constexpr bool kIsNotOffHeapTrampoline = false;
    const bool has_unwinding_info = code_desc_.unwinding_info != nullptr;

    code->set_raw_instruction_size(code_desc_.instr_size);
    code->set_relocation_info(*reloc_info);
    code->initialize_flags(kind_, has_unwinding_info, is_turbofanned_,
                           stack_slots_, kIsNotOffHeapTrampoline);
    code->set_builtin_index(builtin_index_);
    code->set_code_data_container(*data_container);
    code->set_deoptimization_data(*deoptimization_data_);
    code->set_source_position_table(*source_position_table_);
    code->set_safepoint_table_offset(code_desc_.safepoint_table_offset);
    code->set_handler_table_offset(code_desc_.handler_table_offset);
    code->set_constant_pool_offset(code_desc_.constant_pool_offset);
    code->set_code_comments_offset(code_desc_.code_comments_offset);

    // Allow self references to created code object by patching the handle to
    // point to the newly allocated Code object.
    Handle<Object> self_reference;
    if (self_reference_.ToHandle(&self_reference)) {
      DCHECK(self_reference->IsOddball());
      DCHECK(Oddball::cast(*self_reference).kind() ==
             Oddball::kSelfReferenceMarker);
      if (FLAG_embedded_builtins) {
        auto builder = isolate_->builtins_constants_table_builder();
        if (builder != nullptr)
          builder->PatchSelfReference(self_reference, code);
      }
      *(self_reference.location()) = code->ptr();
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

  return code;
}

MaybeHandle<Code> Factory::CodeBuilder::TryBuild() {
  return BuildInternal(false);
}

Handle<Code> Factory::CodeBuilder::Build() {
  return BuildInternal(true).ToHandleChecked();
}

HeapObject Factory::AllocateRawWithImmortalMap(int size,
                                               AllocationType allocation,
                                               Map map,
                                               AllocationAlignment alignment) {
  HeapObject result = isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, allocation, AllocationOrigin::kRuntime, alignment);
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
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

HeapObject Factory::AllocateRawArray(int size, AllocationType allocation) {
  HeapObject result =
      isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(size, allocation);
  if (size > kMaxRegularHeapObjectSize && FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(result);
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  return result;
}

HeapObject Factory::AllocateRawFixedArray(int length,
                                          AllocationType allocation) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  return AllocateRawArray(FixedArray::SizeFor(length), allocation);
}

HeapObject Factory::AllocateRawWeakArrayList(int capacity,
                                             AllocationType allocation) {
  if (capacity < 0 || capacity > WeakArrayList::kMaxCapacity) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  return AllocateRawArray(WeakArrayList::SizeForCapacity(capacity), allocation);
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
  result->set_prototype_users(Smi::kZero);
  result->set_registry_slot(PrototypeInfo::UNREGISTERED);
  result->set_bit_field(0);
  result->set_module_namespace(*undefined_value());
  return result;
}

Handle<EnumCache> Factory::NewEnumCache(Handle<FixedArray> keys,
                                        Handle<FixedArray> indices) {
  Handle<EnumCache> result = Handle<EnumCache>::cast(
      NewStruct(ENUM_CACHE_TYPE, AllocationType::kOld));
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

Handle<ArrayBoilerplateDescription> Factory::NewArrayBoilerplateDescription(
    ElementsKind elements_kind, Handle<FixedArrayBase> constant_values) {
  Handle<ArrayBoilerplateDescription> result =
      Handle<ArrayBoilerplateDescription>::cast(
          NewStruct(ARRAY_BOILERPLATE_DESCRIPTION_TYPE, AllocationType::kOld));
  result->set_elements_kind(elements_kind);
  result->set_constant_elements(*constant_values);
  return result;
}

Handle<TemplateObjectDescription> Factory::NewTemplateObjectDescription(
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

Handle<FixedArray> Factory::NewFixedArrayWithFiller(RootIndex map_root_index,
                                                    int length, Object filler,
                                                    AllocationType allocation) {
  HeapObject result = AllocateRawFixedArray(length, allocation);
  DCHECK(RootsTable::IsImmortalImmovable(map_root_index));
  Map map = Map::cast(isolate()->root(map_root_index));
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  Handle<FixedArray> array(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(array->data_start(), filler, length);
  return array;
}

template <typename T>
Handle<T> Factory::NewFixedArrayWithMap(RootIndex map_root_index, int length,
                                        AllocationType allocation) {
  static_assert(std::is_base_of<FixedArray, T>::value,
                "T must be a descendant of FixedArray");
  // Zero-length case must be handled outside, where the knowledge about
  // the map is.
  DCHECK_LT(0, length);
  return Handle<T>::cast(NewFixedArrayWithFiller(
      map_root_index, length, *undefined_value(), allocation));
}

template <typename T>
Handle<T> Factory::NewWeakFixedArrayWithMap(RootIndex map_root_index,
                                            int length,
                                            AllocationType allocation) {
  static_assert(std::is_base_of<WeakFixedArray, T>::value,
                "T must be a descendant of WeakFixedArray");

  // Zero-length case must be handled outside.
  DCHECK_LT(0, length);

  HeapObject result =
      AllocateRawArray(WeakFixedArray::SizeFor(length), AllocationType::kOld);
  Map map = Map::cast(isolate()->root(map_root_index));
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);

  Handle<WeakFixedArray> array(WeakFixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(ObjectSlot(array->data_start()), *undefined_value(), length);

  return Handle<T>::cast(array);
}

template Handle<FixedArray> Factory::NewFixedArrayWithMap<FixedArray>(
    RootIndex, int, AllocationType allocation);

Handle<FixedArray> Factory::NewFixedArray(int length,
                                          AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();
  return NewFixedArrayWithFiller(RootIndex::kFixedArrayMap, length,
                                 *undefined_value(), allocation);
}

Handle<WeakFixedArray> Factory::NewWeakFixedArray(int length,
                                                  AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_weak_fixed_array();
  HeapObject result =
      AllocateRawArray(WeakFixedArray::SizeFor(length), allocation);
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kWeakFixedArrayMap));
  result.set_map_after_allocation(*weak_fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<WeakFixedArray> array(WeakFixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(ObjectSlot(array->data_start()), *undefined_value(), length);
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
  if (size > kMaxRegularHeapObjectSize && FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(result);
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  result.set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  Handle<FixedArray> array(FixedArray::cast(result), isolate());
  array->set_length(length);
  MemsetTagged(array->data_start(), ReadOnlyRoots(heap).undefined_value(),
               length);
  return array;
}

Handle<FixedArray> Factory::NewFixedArrayWithHoles(int length,
                                                   AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();
  return NewFixedArrayWithFiller(RootIndex::kFixedArrayMap, length,
                                 *the_hole_value(), allocation);
}

Handle<FixedArray> Factory::NewUninitializedFixedArray(int length) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_fixed_array();

  // TODO(ulan): As an experiment this temporarily returns an initialized fixed
  // array. After getting canary/performance coverage, either remove the
  // function or revert to returning uninitilized array.
  return NewFixedArrayWithFiller(RootIndex::kFixedArrayMap, length,
                                 *undefined_value(), AllocationType::kYoung);
}

Handle<ClosureFeedbackCellArray> Factory::NewClosureFeedbackCellArray(
    int length) {
  if (length == 0) return empty_closure_feedback_cell_array();

  Handle<ClosureFeedbackCellArray> feedback_cell_array =
      NewFixedArrayWithMap<ClosureFeedbackCellArray>(
          RootIndex::kClosureFeedbackCellArrayMap, length,
          AllocationType::kYoung);

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
  vector->set_optimized_code_weak_or_smi(MaybeObject::FromSmi(Smi::FromEnum(
      FLAG_log_function_events ? OptimizationMarker::kLogFirstExecution
                               : OptimizationMarker::kNone)));
  vector->set_length(length);
  vector->set_invocation_count(0);
  vector->set_profiler_ticks(0);
  vector->clear_padding();
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
  }
  return array;
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
      Handle<ObjectBoilerplateDescription>::cast(
          NewFixedArrayWithMap(RootIndex::kObjectBoilerplateDescriptionMap,
                               size, AllocationType::kOld));

  if (has_different_size_backing_store) {
    DCHECK_IMPLIES((boilerplate == (all_properties - index_keys)),
                   has_seen_proto);
    description->set_backing_store_size(backing_store_size);
  }

  description->set_flags(0);

  return description;
}

Handle<FixedArrayBase> Factory::NewFixedDoubleArray(int length) {
  if (length == 0) return empty_fixed_array();
  if (length < 0 || length > FixedDoubleArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  int size = FixedDoubleArray::SizeFor(length);
  Map map = *fixed_double_array_map();
  HeapObject result = AllocateRawWithImmortalMap(size, AllocationType::kYoung,
                                                 map, kDoubleAligned);
  Handle<FixedDoubleArray> array(FixedDoubleArray::cast(result), isolate());
  array->set_length(length);
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

Handle<FeedbackMetadata> Factory::NewFeedbackMetadata(
    int slot_count, int feedback_cell_count, AllocationType allocation) {
  DCHECK_LE(0, slot_count);
  int size = FeedbackMetadata::SizeFor(slot_count);
  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, *feedback_metadata_map());
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

Handle<FrameArray> Factory::NewFrameArray(int number_of_frames) {
  DCHECK_LE(0, number_of_frames);
  Handle<FixedArray> result =
      NewFixedArrayWithHoles(FrameArray::LengthFor(number_of_frames));
  result->set(FrameArray::kFrameCountIndex, Smi::kZero);
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
  capacity = base::bits::RoundUpToPowerOfTwo32(Max(T::kMinCapacity, capacity));
  capacity = Min(capacity, T::kMaxCapacity);

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
  return OrderedHashSet::Allocate(isolate(), OrderedHashSet::kMinCapacity)
      .ToHandleChecked();
}

Handle<OrderedHashMap> Factory::NewOrderedHashMap() {
  return OrderedHashMap::Allocate(isolate(), OrderedHashMap::kMinCapacity)
      .ToHandleChecked();
}

Handle<OrderedNameDictionary> Factory::NewOrderedNameDictionary() {
  return OrderedNameDictionary::Allocate(isolate(),
                                         OrderedNameDictionary::kMinCapacity)
      .ToHandleChecked();
}

Handle<AccessorPair> Factory::NewAccessorPair() {
  Handle<AccessorPair> accessors = Handle<AccessorPair>::cast(
      NewStruct(ACCESSOR_PAIR_TYPE, AllocationType::kOld));
  accessors->set_getter(*null_value(), SKIP_WRITE_BARRIER);
  accessors->set_setter(*null_value(), SKIP_WRITE_BARRIER);
  return accessors;
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

template <typename Char>
Handle<String> Factory::InternalizeString(const Vector<const Char>& string,
                                          bool convert_encoding) {
  SequentialStringKey<Char> key(string, HashSeed(isolate()), convert_encoding);
  return InternalizeStringWithKey(&key);
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> Factory::InternalizeString(
        const Vector<const uint8_t>& string, bool convert_encoding);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<String> Factory::InternalizeString(
        const Vector<const uint16_t>& string, bool convert_encoding);

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

template <class StringTableKey>
Handle<String> Factory::InternalizeStringWithKey(StringTableKey* key) {
  return StringTable::LookupKey(isolate(), key);
}

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

  DisallowHeapAllocation no_gc;
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

    DisallowHeapAllocation no_gc;
    decoder.Decode(result->GetChars(no_gc), utf8_data);
    return result;
  }

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(decoder.utf16_length(), allocation), String);

  DisallowHeapAllocation no_gc;
  decoder.Decode(result->GetChars(no_gc), utf8_data);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8SubString(
    Handle<SeqOneByteString> str, int begin, int length,
    AllocationType allocation) {
  Vector<const uint8_t> utf8_data;
  {
    DisallowHeapAllocation no_gc;
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
    DisallowHeapAllocation no_gc;
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

  DisallowHeapAllocation no_gc;
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
    DisallowHeapAllocation no_gc;
    CopyChars(result->GetChars(no_gc), string, length);
    return result;
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               NewRawTwoByteString(length, allocation), String);
    DisallowHeapAllocation no_gc;
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

Handle<SeqOneByteString> Factory::AllocateRawOneByteInternalizedString(
    int length, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  // The canonical empty_string is the only zero-length string we allow.
  DCHECK_IMPLIES(
      length == 0,
      isolate()->roots_table()[RootIndex::kempty_string] == kNullAddress);

  Map map = *one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(length);
  HeapObject result =
      AllocateRawWithImmortalMap(size,
                                 isolate()->heap()->CanAllocateInReadOnlySpace()
                                     ? AllocationType::kReadOnly
                                     : AllocationType::kOld,
                                 map);
  Handle<SeqOneByteString> answer(SeqOneByteString::cast(result), isolate());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  return answer;
}

Handle<String> Factory::AllocateTwoByteInternalizedString(
    const Vector<const uc16>& str, uint32_t hash_field) {
  Handle<SeqTwoByteString> result =
      AllocateRawTwoByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_gc;

  // Fill in the characters.
  MemCopy(result->GetChars(no_gc), str.begin(), str.length() * kUC16Size);

  return result;
}

Handle<SeqTwoByteString> Factory::AllocateRawTwoByteInternalizedString(
    int length, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  DCHECK_NE(0, length);  // Use Heap::empty_string() instead.

  Map map = *internalized_string_map();
  int size = SeqTwoByteString::SizeFor(length);
  HeapObject result =
      AllocateRawWithImmortalMap(size, AllocationType::kOld, map);
  Handle<SeqTwoByteString> answer(SeqTwoByteString::cast(result), isolate());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, result.Size());
  return answer;
}

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
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  DisallowHeapAllocation no_gc;

  if (is_one_byte) {
    WriteOneByteData(t, SeqOneByteString::cast(*answer).GetChars(no_gc), chars);
  } else {
    WriteTwoByteData(t, SeqTwoByteString::cast(*answer).GetChars(no_gc), chars);
  }
  return answer;
}

Handle<String> Factory::NewOneByteInternalizedString(
    const Vector<const uint8_t>& str, uint32_t hash_field) {
  Handle<SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_allocation;
  MemCopy(result->GetChars(no_allocation), str.begin(), str.length());
  return result;
}

Handle<String> Factory::NewTwoByteInternalizedString(
    const Vector<const uc16>& str, uint32_t hash_field) {
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
    int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqOneByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqOneByteString::SizeFor(length);
  DCHECK_GE(SeqOneByteString::kMaxSize, size);

  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, *one_byte_string_map());
  Handle<SeqOneByteString> string(SeqOneByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

MaybeHandle<SeqTwoByteString> Factory::NewRawTwoByteString(
    int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqTwoByteString);
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqTwoByteString::SizeFor(length);
  DCHECK_GE(SeqTwoByteString::kMaxSize, size);

  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, *string_map());
  Handle<SeqTwoByteString> string(SeqTwoByteString::cast(result), isolate());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

Handle<String> Factory::LookupSingleCharacterStringFromCode(uint16_t code) {
  if (code <= unibrow::Latin1::kMaxChar) {
    {
      DisallowHeapAllocation no_allocation;
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

static inline Handle<String> MakeOrFindTwoCharacterString(Isolate* isolate,
                                                          uint16_t c1,
                                                          uint16_t c2) {
  if ((c1 | c2) <= unibrow::Latin1::kMaxChar) {
    uint8_t buffer[] = {static_cast<uint8_t>(c1), static_cast<uint8_t>(c2)};
    return isolate->factory()->InternalizeString(
        Vector<const uint8_t>(buffer, 2));
  }
  uint16_t buffer[] = {c1, c2};
  return isolate->factory()->InternalizeString(
      Vector<const uint16_t>(buffer, 2));
}

template <typename SinkChar, typename StringType>
Handle<String> ConcatStringContent(Handle<StringType> result,
                                   Handle<String> first,
                                   Handle<String> second) {
  DisallowHeapAllocation pointer_stays_valid;
  SinkChar* sink = result->GetChars(pointer_stays_valid);
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
      uint8_t* dest = result->GetChars(no_gc);
      // Copy left part.
      const uint8_t* src =
          left->IsExternalString()
              ? Handle<ExternalOneByteString>::cast(left)->GetChars()
              : Handle<SeqOneByteString>::cast(left)->GetChars(no_gc);
      for (int i = 0; i < left_length; i++) *dest++ = src[i];
      // Copy right part.
      src = right->IsExternalString()
                ? Handle<ExternalOneByteString>::cast(right)->GetChars()
                : Handle<SeqOneByteString>::cast(right)->GetChars(no_gc);
      for (int i = 0; i < right_length; i++) *dest++ = src[i];
      return result;
    }

    return ConcatStringContent<uc16>(
        NewRawTwoByteString(length).ToHandleChecked(), left, right);
  }

  return NewConsString(left, right, length, is_one_byte);
}

Handle<String> Factory::NewConsString(Handle<String> left, Handle<String> right,
                                      int length, bool one_byte) {
  DCHECK(!left->IsThinString());
  DCHECK(!right->IsThinString());
  DCHECK_GE(length, ConsString::kMinLength);
  DCHECK_LE(length, String::kMaxLength);

  Handle<ConsString> result(
      ConsString::cast(
          one_byte ? New(cons_one_byte_string_map(), AllocationType::kYoung)
                   : New(cons_string_map(), AllocationType::kYoung)),
      isolate());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(*left, mode);
  result->set_second(*right, mode);
  return result;
}

Handle<String> Factory::NewSurrogatePairString(uint16_t lead, uint16_t trail) {
  DCHECK_GE(lead, 0xD800);
  DCHECK_LE(lead, 0xDBFF);
  DCHECK_GE(trail, 0xDC00);
  DCHECK_LE(trail, 0xDFFF);

  Handle<SeqTwoByteString> str =
      isolate()->factory()->NewRawTwoByteString(2).ToHandleChecked();
  DisallowHeapAllocation no_allocation;
  uc16* dest = str->GetChars(no_allocation);
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
      DisallowHeapAllocation no_gc;
      uint8_t* dest = result->GetChars(no_gc);
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    } else {
      Handle<SeqTwoByteString> result =
          NewRawTwoByteString(length).ToHandleChecked();
      DisallowHeapAllocation no_gc;
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

  slice->set_hash_field(String::kEmptyHashField);
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

  Handle<Map> map = resource->IsCacheable() ? external_string_map()
                                            : uncached_external_string_map();
  Handle<ExternalTwoByteString> external_string(
      ExternalTwoByteString::cast(New(map, AllocationType::kOld)), isolate());
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
      ExternalOneByteString::cast(New(map, AllocationType::kOld)), isolate());
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
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
  symbol->set_hash_field(Name::kIsNotArrayIndexMask |
                         (hash << Name::kHashShift));
  symbol->set_name(*undefined_value());
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
  symbol->set_name(*name);
  return symbol;
}

Handle<Context> Factory::NewContext(RootIndex map_root_index, int size,
                                    int variadic_part_length,
                                    AllocationType allocation) {
  DCHECK(RootsTable::IsImmortalImmovable(map_root_index));
  DCHECK_LE(Context::kTodoHeaderSize, size);
  DCHECK(IsAligned(size, kTaggedSize));
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  DCHECK_LE(Context::SizeFor(variadic_part_length), size);

  Map map = Map::cast(isolate()->root(map_root_index));
  HeapObject result = AllocateRawWithImmortalMap(size, allocation, map);
  Handle<Context> context(Context::cast(result), isolate());
  context->initialize_length_and_extension_bit(variadic_part_length);
  DCHECK_EQ(context->SizeFromMap(map), size);
  if (size > Context::kTodoHeaderSize) {
    ObjectSlot start = context->RawField(Context::kTodoHeaderSize);
    ObjectSlot end = context->RawField(size);
    size_t slot_count = end - start;
    MemsetTagged(start, *undefined_value(), slot_count);
  }
  return context;
}

Handle<NativeContext> Factory::NewNativeContext() {
  Handle<NativeContext> context = Handle<NativeContext>::cast(
      NewContext(RootIndex::kNativeContextMap, NativeContext::kSize,
                 NativeContext::NATIVE_CONTEXT_SLOTS, AllocationType::kOld));
  context->set_scope_info(ReadOnlyRoots(isolate()).empty_scope_info());
  context->set_previous(Context::unchecked_cast(Smi::zero()));
  context->set_extension(*the_hole_value());
  context->set_native_context(*context);
  context->set_errors_thrown(Smi::zero());
  context->set_math_random_index(Smi::zero());
  context->set_serialized_objects(*empty_fixed_array());
  context->set_microtask_queue(nullptr);
  context->set_osr_code_cache(*empty_weak_fixed_array());
  return context;
}

Handle<Context> Factory::NewScriptContext(Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), SCRIPT_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context = NewContext(
      RootIndex::kScriptContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kOld);
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
          RootIndex::kScriptContextTableMap, ScriptContextTable::kMinLength);
  context_table->set_used(0);
  return context_table;
}

Handle<Context> Factory::NewModuleContext(Handle<SourceTextModule> module,
                                          Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), MODULE_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context = NewContext(
      RootIndex::kModuleContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kOld);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*module);
  context->set_native_context(*outer);
  DCHECK(context->IsModuleContext());
  return context;
}

Handle<Context> Factory::NewFunctionContext(Handle<Context> outer,
                                            Handle<ScopeInfo> scope_info) {
  RootIndex mapRootIndex;
  switch (scope_info->scope_type()) {
    case EVAL_SCOPE:
      mapRootIndex = RootIndex::kEvalContextMap;
      break;
    case FUNCTION_SCOPE:
      mapRootIndex = RootIndex::kFunctionContextMap;
      break;
    default:
      UNREACHABLE();
  }
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context =
      NewContext(mapRootIndex, Context::SizeFor(variadic_part_length),
                 variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*outer);
  context->set_extension(*the_hole_value());
  context->set_native_context(outer->native_context());
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
      RootIndex::kCatchContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
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
                                                 Handle<StringSet> blacklist) {
  STATIC_ASSERT(Context::BLACK_LIST_INDEX == Context::MIN_CONTEXT_SLOTS + 1);
  DCHECK(scope_info->IsDebugEvaluateScope());
  Handle<HeapObject> ext = extension.is_null()
                               ? Handle<HeapObject>::cast(the_hole_value())
                               : Handle<HeapObject>::cast(extension);
  // TODO(ishell): Take the details from DebugEvaluateContextContext class.
  int variadic_part_length = Context::MIN_CONTEXT_SLOTS + 2;
  Handle<Context> c = NewContext(RootIndex::kDebugEvaluateContextMap,
                                 Context::SizeFor(variadic_part_length),
                                 variadic_part_length, AllocationType::kYoung);
  c->set_scope_info(*scope_info);
  c->set_previous(*previous);
  c->set_native_context(previous->native_context());
  c->set_extension(*ext);
  if (!wrapped.is_null()) c->set(Context::WRAPPED_CONTEXT_INDEX, *wrapped);
  if (!blacklist.is_null()) c->set(Context::BLACK_LIST_INDEX, *blacklist);
  return c;
}

Handle<Context> Factory::NewWithContext(Handle<Context> previous,
                                        Handle<ScopeInfo> scope_info,
                                        Handle<JSReceiver> extension) {
  DCHECK_EQ(scope_info->scope_type(), WITH_SCOPE);
  // TODO(ishell): Take the details from WithContext class.
  int variadic_part_length = Context::MIN_CONTEXT_SLOTS;
  Handle<Context> context = NewContext(
      RootIndex::kWithContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*extension);
  context->set_native_context(previous->native_context());
  return context;
}

Handle<Context> Factory::NewBlockContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  DCHECK_IMPLIES(scope_info->scope_type() != BLOCK_SCOPE,
                 scope_info->scope_type() == CLASS_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Handle<Context> context = NewContext(
      RootIndex::kBlockContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(*scope_info);
  context->set_previous(*previous);
  context->set_extension(*the_hole_value());
  context->set_native_context(previous->native_context());
  return context;
}

Handle<Context> Factory::NewBuiltinContext(Handle<NativeContext> native_context,
                                           int variadic_part_length) {
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  Handle<Context> context = NewContext(
      RootIndex::kFunctionContextMap, Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  context->set_scope_info(ReadOnlyRoots(isolate()).empty_scope_info());
  context->set_previous(*native_context);
  context->set_extension(*the_hole_value());
  context->set_native_context(*native_context);
  return context;
}

Handle<Struct> Factory::NewStruct(InstanceType type,
                                  AllocationType allocation) {
  Map map = Map::GetStructMap(isolate(), type);
  int size = map.instance_size();
  HeapObject result = AllocateRawWithImmortalMap(size, allocation, map);
  Handle<Struct> str(Struct::cast(result), isolate());
  str->InitializeBody(size);
  return str;
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
  DisallowHeapAllocation no_gc;
  info->set_name(*empty_string());
  info->set_flags(0);  // Must clear the flags, it was initialized as undefined.
  info->set_is_sloppy(true);
  info->set_initial_property_attributes(NONE);

  // Clear some other fields that should not be undefined.
  info->set_getter(Smi::kZero);
  info->set_setter(Smi::kZero);
  info->set_js_getter(Smi::kZero);

  return info;
}

Handle<Script> Factory::NewScript(Handle<String> source) {
  return NewScriptWithId(source, isolate()->heap()->NextScriptId());
}

Handle<Script> Factory::NewScriptWithId(Handle<String> source, int script_id) {
  // Create and initialize script object.
  Heap* heap = isolate()->heap();
  ReadOnlyRoots roots(heap);
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
  script->set_shared_function_infos(*empty_weak_fixed_array(),
                                    SKIP_WRITE_BARRIER);
  script->set_flags(0);
  script->set_host_defined_options(*empty_fixed_array());
  Handle<WeakArrayList> scripts = script_list();
  scripts = WeakArrayList::AddToEnd(isolate(), scripts,
                                    MaybeObjectHandle::Weak(script));
  heap->set_script_list(*scripts);
  LOG(isolate(), ScriptEvent(Logger::ScriptEventType::kCreate, script_id));
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "Script",
      TRACE_ID_WITH_SCOPE(Script::kTraceScope, script_id));
  return script;
}

Handle<Script> Factory::CloneScript(Handle<Script> script) {
  Heap* heap = isolate()->heap();
  int script_id = isolate()->heap()->NextScriptId();
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

Handle<Foreign> Factory::NewForeign(Address addr) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  STATIC_ASSERT(Foreign::kSize <= kMaxRegularHeapObjectSize);
  Map map = *foreign_map();
  HeapObject result = AllocateRawWithImmortalMap(map.instance_size(),
                                                 AllocationType::kYoung, map);
  Handle<Foreign> foreign(Foreign::cast(result), isolate());
  foreign->set_foreign_address(addr);
  return foreign;
}

Handle<ByteArray> Factory::NewByteArray(int length, AllocationType allocation) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  int size = ByteArray::SizeFor(length);
  HeapObject result =
      AllocateRawWithImmortalMap(size, allocation, *byte_array_map());
  Handle<ByteArray> array(ByteArray::cast(result), isolate());
  array->set_length(length);
  array->clear_padding();
  return array;
}

Handle<BytecodeArray> Factory::NewBytecodeArray(
    int length, const byte* raw_bytecodes, int frame_size, int parameter_count,
    Handle<FixedArray> constant_pool) {
  if (length < 0 || length > BytecodeArray::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid array length");
  }
  // Bytecode array is AllocationType::kOld, so constant pool array should be
  // too.
  DCHECK(!Heap::InYoungGeneration(*constant_pool));

  int size = BytecodeArray::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(size, AllocationType::kOld,
                                                 *bytecode_array_map());
  Handle<BytecodeArray> instance(BytecodeArray::cast(result), isolate());
  instance->set_length(length);
  instance->set_frame_size(frame_size);
  instance->set_parameter_count(parameter_count);
  instance->set_incoming_new_target_or_generator_register(
      interpreter::Register::invalid_value());
  instance->set_osr_loop_nesting_level(0);
  instance->set_bytecode_age(BytecodeArray::kNoAgeBytecodeAge);
  instance->set_constant_pool(*constant_pool);
  instance->set_handler_table(*empty_byte_array());
  instance->set_source_position_table(*undefined_value());
  CopyBytes(reinterpret_cast<byte*>(instance->GetFirstBytecodeAddress()),
            raw_bytecodes, length);
  instance->clear_padding();

  return instance;
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
  HeapObject result = AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
      AllocationType::kOld, *no_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->set_interrupt_budget(FeedbackCell::GetInitialInterruptBudget());
  cell->clear_padding();
  return cell;
}

Handle<FeedbackCell> Factory::NewOneClosureCell(Handle<HeapObject> value) {
  HeapObject result = AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
      AllocationType::kOld, *one_closure_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->set_interrupt_budget(FeedbackCell::GetInitialInterruptBudget());
  cell->clear_padding();
  return cell;
}

Handle<FeedbackCell> Factory::NewManyClosuresCell(Handle<HeapObject> value) {
  HeapObject result = AllocateRawWithImmortalMap(FeedbackCell::kAlignedSize,
      AllocationType::kOld, *many_closures_cell_map());
  Handle<FeedbackCell> cell(FeedbackCell::cast(result), isolate());
  cell->set_value(*value);
  cell->set_interrupt_budget(FeedbackCell::GetInitialInterruptBudget());
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

Handle<DescriptorArray> Factory::NewDescriptorArray(int number_of_descriptors,
                                                    int slack) {
  int number_of_all_descriptors = number_of_descriptors + slack;
  // Zero-length case must be handled outside.
  DCHECK_LT(0, number_of_all_descriptors);
  int size = DescriptorArray::SizeFor(number_of_all_descriptors);
  HeapObject obj = isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, AllocationType::kYoung);
  obj.set_map_after_allocation(*descriptor_array_map(), SKIP_WRITE_BARRIER);
  DescriptorArray array = DescriptorArray::cast(obj);
  array.Initialize(*empty_enum_cache(), *undefined_value(),
                   number_of_descriptors, slack);
  return Handle<DescriptorArray>(array, isolate());
}

Handle<TransitionArray> Factory::NewTransitionArray(int number_of_transitions,
                                                    int slack) {
  int capacity = TransitionArray::LengthFor(number_of_transitions + slack);
  Handle<TransitionArray> array = NewWeakFixedArrayWithMap<TransitionArray>(
      RootIndex::kTransitionArrayMap, capacity, AllocationType::kOld);
  // Transition arrays are AllocationType::kOld. When black allocation is on we
  // have to add the transition array to the list of
  // encountered_transition_arrays.
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
    map.set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  // Must be called only after |instance_type|, |instance_size| and
  // |layout_descriptor| are set.
  map.set_visitor_id(Map::GetVisitorId(map));
  map.set_bit_field(0);
  map.set_bit_field2(Map::NewTargetIsBaseBit::encode(true));
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptorsBit::encode(true) |
                   Map::ConstructionCounterBits::encode(Map::kNoSlackTracking) |
                   Map::IsExtensibleBit::encode(true);
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

  // Since we know the clone is allocated in new space, we can copy
  // the contents without worrying about updating the write barrier.
  Heap::CopyBlock(raw_clone.address(), source->address(), object_size);
  Handle<JSObject> clone(JSObject::cast(raw_clone), isolate());

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
    Handle<FixedArray> properties(
        FixedArray::cast(source->property_dictionary()), isolate());
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
    obj->SetEmbedderField(i, Smi::kZero);
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

  DisallowHeapAllocation no_gc;
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
  DisallowHeapAllocation no_gc;
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
  HeapObject obj = AllocateRawWeakArrayList(new_capacity, allocation);
  obj.set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);

  WeakArrayList result = WeakArrayList::cast(obj);
  int old_len = src->length();
  result.set_length(old_len);
  result.set_capacity(new_capacity);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = obj.GetWriteBarrierMode(no_gc);
  result.CopyElements(isolate(), 0, *src, 0, old_len, mode);
  MemsetTagged(ObjectSlot(result.data_start() + old_len),
               ReadOnlyRoots(isolate()).undefined_value(),
               new_capacity - old_len);
  return Handle<WeakArrayList>(result, isolate());
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
  DisallowHeapAllocation no_gc;
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

template <AllocationType allocation>
Handle<Object> Factory::NewNumber(double value) {
  // Materialize as a SMI if possible.
  int32_t int_value;
  if (DoubleToSmiInteger(value, &int_value)) {
    return handle(Smi::FromInt(int_value), isolate());
  }
  return NewHeapNumber<allocation>(value);
}

template Handle<Object> V8_EXPORT_PRIVATE
Factory::NewNumber<AllocationType::kYoung>(double);
template Handle<Object> V8_EXPORT_PRIVATE
Factory::NewNumber<AllocationType::kOld>(double);
template Handle<Object> V8_EXPORT_PRIVATE
Factory::NewNumber<AllocationType::kReadOnly>(double);

Handle<Object> Factory::NewNumberFromInt(int32_t value) {
  if (Smi::IsValid(value)) return handle(Smi::FromInt(value), isolate());
  // Bypass NewNumber to avoid various redundant checks.
  return NewHeapNumber(FastI2D(value));
}

Handle<Object> Factory::NewNumberFromUint(uint32_t value) {
  int32_t int32v = static_cast<int32_t>(value);
  if (int32v >= 0 && Smi::IsValid(int32v)) {
    return handle(Smi::FromInt(int32v), isolate());
  }
  return NewHeapNumber(FastUI2D(value));
}

template <AllocationType allocation>
Handle<HeapNumber> Factory::NewHeapNumber() {
  STATIC_ASSERT(HeapNumber::kSize <= kMaxRegularHeapObjectSize);
  Map map = *heap_number_map();
  HeapObject result = AllocateRawWithImmortalMap(HeapNumber::kSize, allocation,
                                                 map, kDoubleUnaligned);
  return handle(HeapNumber::cast(result), isolate());
}

template Handle<HeapNumber> V8_EXPORT_PRIVATE
Factory::NewHeapNumber<AllocationType::kYoung>();
template Handle<HeapNumber> V8_EXPORT_PRIVATE
Factory::NewHeapNumber<AllocationType::kOld>();
template Handle<HeapNumber> V8_EXPORT_PRIVATE
Factory::NewHeapNumber<AllocationType::kReadOnly>();

Handle<HeapNumber> Factory::NewHeapNumberForCodeAssembler(double value) {
  return isolate()->heap()->CanAllocateInReadOnlySpace()
             ? NewHeapNumber<AllocationType::kReadOnly>(value)
             : NewHeapNumber<AllocationType::kOld>(value);
}

Handle<FreshlyAllocatedBigInt> Factory::NewBigInt(int length,
                                                  AllocationType allocation) {
  if (length < 0 || length > BigInt::kMaxLength) {
    isolate()->heap()->FatalProcessOutOfMemory("invalid BigInt length");
  }
  HeapObject result = AllocateRawWithImmortalMap(BigInt::SizeFor(length),
                                                 allocation, *bigint_map());
  FreshlyAllocatedBigInt bigint = FreshlyAllocatedBigInt::cast(result);
  bigint.clear_padding();
  return handle(bigint, isolate());
}

Handle<Object> Factory::NewError(Handle<JSFunction> constructor,
                                 MessageTemplate template_index,
                                 Handle<Object> arg0, Handle<Object> arg1,
                                 Handle<Object> arg2) {
  HandleScope scope(isolate());
  if (isolate()->bootstrapper()->IsActive()) {
    // During bootstrapping we cannot construct error objects.
    return scope.CloseAndEscape(NewStringFromAsciiChecked(
        MessageFormatter::TemplateString(template_index)));
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
  MaybeHandle<Object> maybe_error = ErrorUtils::Construct(
      isolate(), constructor, constructor, message, SKIP_NONE, no_caller,
      ErrorUtils::StackTraceCollection::kDetailed);
  if (maybe_error.is_null()) {
    DCHECK(isolate()->has_pending_exception());
    maybe_error = handle(isolate()->pending_exception(), isolate());
    isolate()->clear_pending_exception();
  }

  return maybe_error.ToHandleChecked();
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
  Handle<Object> Factory::New##NAME(MessageTemplate template_index,           \
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
                                        AllocationType allocation) {
  Handle<JSFunction> function(JSFunction::cast(New(map, allocation)),
                              isolate());

  function->initialize_properties(isolate());
  function->initialize_elements();
  function->set_shared(*info);
  function->set_code(info->GetCode());
  function->set_context(*context);
  function->set_raw_feedback_cell(*many_closures_cell());
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
  DCHECK(is_sloppy(result->shared().language_mode()));
  return result;
}

Handle<JSFunction> Factory::NewFunction(const NewFunctionArgs& args) {
  DCHECK(!args.name_.is_null());

  // Create the SharedFunctionInfo.
  Handle<NativeContext> context(isolate()->native_context());
  Handle<Map> map = args.GetMap(isolate());
  Handle<SharedFunctionInfo> info =
      NewSharedFunctionInfo(args.name_, args.maybe_wasm_function_data_,
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
        (*map == *isolate()->wasm_exported_function_map()));
  }
#endif

  Handle<JSFunction> result = NewFunction(map, info, context);

  if (args.should_set_prototype_) {
    result->set_prototype_or_initial_map(
        *args.maybe_prototype_.ToHandleChecked());
  }

  if (args.should_set_language_mode_) {
    result->shared().set_language_mode(args.language_mode_);
  }

  if (args.should_create_and_set_initial_map_) {
    ElementsKind elements_kind;
    switch (args.type_) {
      case JS_ARRAY_TYPE:
        elements_kind = PACKED_SMI_ELEMENTS;
        break;
      case JS_ARGUMENTS_OBJECT_TYPE:
        elements_kind = PACKED_ELEMENTS;
        break;
      default:
        elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
        break;
    }
    Handle<Map> initial_map = NewMap(args.type_, args.instance_size_,
                                     elements_kind, args.inobject_properties_);
    result->shared().set_expected_nof_properties(args.inobject_properties_);
    // TODO(littledan): Why do we have this is_generator test when
    // NewFunctionPrototype already handles finding an appropriately
    // shared prototype?
    Handle<HeapObject> prototype = args.maybe_prototype_.ToHandleChecked();
    if (!IsResumableFunction(result->shared().kind())) {
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

Handle<WeakCell> Factory::NewWeakCell() {
  // Allocate the WeakCell object in the old space, because 1) WeakCell weakness
  // handling is only implemented in the old space 2) they're supposedly
  // long-living. TODO(marja, gsathya): Support WeakCells in Scavenger.
  Handle<WeakCell> result(
      WeakCell::cast(AllocateRawWithImmortalMap(
          WeakCell::kSize, AllocationType::kOld, *weak_cell_map())),
      isolate());
  return result;
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info, Handle<Context> context,
    AllocationType allocation) {
  Handle<Map> initial_map(
      Map::cast(context->native_context().get(info->function_map_index())),
      isolate());
  return NewFunctionFromSharedFunctionInfo(initial_map, info, context,
                                           allocation);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info, Handle<Context> context,
    Handle<FeedbackCell> feedback_cell, AllocationType allocation) {
  Handle<Map> initial_map(
      Map::cast(context->native_context().get(info->function_map_index())),
      isolate());
  return NewFunctionFromSharedFunctionInfo(initial_map, info, context,
                                           feedback_cell, allocation);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Context> context, AllocationType allocation) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context, allocation);

  // Give compiler a chance to pre-initialize.
  Compiler::PostInstantiation(result);

  return result;
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Context> context, Handle<FeedbackCell> feedback_cell,
    AllocationType allocation) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context, allocation);

  // Bump the closure count that is encoded in the feedback cell's map.
  if (feedback_cell->map() == *no_closures_cell_map()) {
    feedback_cell->set_map(*one_closure_cell_map());
  } else if (feedback_cell->map() == *one_closure_cell_map()) {
    feedback_cell->set_map(*many_closures_cell_map());
  } else {
    DCHECK(feedback_cell->map() == *many_closures_cell_map());
  }

  // Check that the optimized code in the feedback cell wasn't marked for
  // deoptimization while not pointed to by any live JSFunction.
  if (feedback_cell->value().IsFeedbackVector()) {
    FeedbackVector::cast(feedback_cell->value())
        .EvictOptimizedCodeMarkedForDeoptimization(
            *info, "new function from shared function info");
  }
  result->set_raw_feedback_cell(*feedback_cell);

  // Give compiler a chance to pre-initialize.
  Compiler::PostInstantiation(result);

  return result;
}

Handle<ScopeInfo> Factory::NewScopeInfo(int length, AllocationType type) {
  DCHECK(type == AllocationType::kOld || type == AllocationType::kReadOnly);
  return NewFixedArrayWithMap<ScopeInfo>(RootIndex::kScopeInfoMap, length,
                                         type);
}

Handle<SourceTextModuleInfo> Factory::NewSourceTextModuleInfo() {
  return NewFixedArrayWithMap<SourceTextModuleInfo>(
      RootIndex::kModuleInfoMap, SourceTextModuleInfo::kLength,
      AllocationType::kOld);
}

Handle<PreparseData> Factory::NewPreparseData(int data_length,
                                              int children_length) {
  int size = PreparseData::SizeFor(data_length, children_length);
  Handle<PreparseData> result(
      PreparseData::cast(AllocateRawWithImmortalMap(size, AllocationType::kOld,
                                                    *preparse_data_map())),
      isolate());
  result->set_data_length(data_length);
  result->set_children_length(children_length);
  MemsetTagged(result->inner_data_start(), *null_value(), children_length);
  result->clear_padding();
  return result;
}

Handle<UncompiledDataWithoutPreparseData>
Factory::NewUncompiledDataWithoutPreparseData(Handle<String> inferred_name,
                                              int32_t start_position,
                                              int32_t end_position) {
  Handle<UncompiledDataWithoutPreparseData> result(
      UncompiledDataWithoutPreparseData::cast(New(
          uncompiled_data_without_preparse_data_map(), AllocationType::kOld)),
      isolate());

  UncompiledData::Initialize(*result, *inferred_name, start_position,
                             end_position);
  return result;
}

Handle<UncompiledDataWithPreparseData>
Factory::NewUncompiledDataWithPreparseData(Handle<String> inferred_name,
                                           int32_t start_position,
                                           int32_t end_position,
                                           Handle<PreparseData> preparse_data) {
  Handle<UncompiledDataWithPreparseData> result(
      UncompiledDataWithPreparseData::cast(
          New(uncompiled_data_with_preparse_data_map(), AllocationType::kOld)),
      isolate());

  UncompiledDataWithPreparseData::Initialize(
      *result, *inferred_name, start_position, end_position, *preparse_data);

  return result;
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
  CHECK_NOT_NULL(isolate()->embedded_blob());
  CHECK_NE(0, isolate()->embedded_blob_size());
  CHECK(Builtins::IsIsolateIndependentBuiltin(*code));

  Handle<Code> result = Builtins::GenerateOffHeapTrampolineFor(
      isolate(), off_heap_entry,
      code->code_data_container().kind_specific_flags());
  // The CodeDataContainer should not be modified beyond this point since it's
  // now possibly canonicalized.

  // The trampoline code object must inherit specific flags from the original
  // builtin (e.g. the safepoint-table offset). We set them manually here.
  {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(*result);
    CodePageMemoryModificationScope code_allocation(chunk);

    const bool set_is_off_heap_trampoline = true;
    const int stack_slots =
        code->has_safepoint_info() ? code->stack_slots() : 0;
    result->initialize_flags(code->kind(), code->has_unwinding_info(),
                             code->is_turbofanned(), stack_slots,
                             set_is_off_heap_trampoline);
    result->set_builtin_index(code->builtin_index());
    result->set_safepoint_table_offset(code->safepoint_table_offset());
    result->set_handler_table_offset(code->handler_table_offset());
    result->set_constant_pool_offset(code->constant_pool_offset());
    result->set_code_comments_offset(code->code_comments_offset());

    // Replace the newly generated trampoline's RelocInfo ByteArray with the
    // canonical one stored in the roots to avoid duplicating it for every
    // single builtin.
    ByteArray canonical_reloc_info =
        ReadOnlyRoots(isolate()).off_heap_trampoline_relocation_info();
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
      code->code_data_container().kind_specific_flags(), AllocationType::kOld);

  Heap* heap = isolate()->heap();
  Handle<Code> new_code;
  {
    int obj_size = code->Size();
    CodePageCollectionMemoryModificationScope code_allocation(heap);
    HeapObject result = heap->AllocateRawWith<Heap::kRetryOrFail>(
        obj_size, AllocationType::kCode);

    // Copy code object.
    Address old_addr = code->address();
    Address new_addr = result.address();
    Heap::CopyBlock(new_addr, old_addr, obj_size);
    new_code = handle(Code::cast(result), isolate());

    // Set the {CodeDataContainer}, it cannot be shared.
    new_code->set_code_data_container(*data_container);

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
  copy->set_source_position_table(bytecode_array->source_position_table());
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
  Handle<DescriptorArray> descs(map->instance_descriptors(), isolate());
  for (InternalIndex i : InternalIndex::Range(map->NumberOfOwnDescriptors())) {
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
  Handle<NameDictionary> object_properties =
      NameDictionary::New(isolate(), capacity);
  Handle<JSObject> js_object =
      NewJSObjectFromMap(map, allocation, allocation_site);
  js_object->set_raw_properties_or_hash(*object_properties);
  return js_object;
}

Handle<JSObject> Factory::NewSlowJSObjectWithPropertiesAndElements(
    Handle<HeapObject> prototype, Handle<NameDictionary> properties,
    Handle<FixedArrayBase> elements) {
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
  DisallowHeapAllocation no_gc;
  array->set_elements(*elements);
  array->set_length(Smi::FromInt(length));
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
    Handle<SharedFunctionInfo> code) {
  Handle<SourceTextModuleInfo> module_info(
      code->scope_info().ModuleDescriptorInfo(), isolate());
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
  module->set_top_level_capability(roots.undefined_value());
  module->set_flags(0);
  module->set_async(IsAsyncModule(code->kind()));
  module->set_async_evaluating(false);
  module->set_async_parent_modules(*async_parent_modules);
  module->set_pending_async_dependencies(0);
  return module;
}

Handle<SyntheticModule> Factory::NewSyntheticModule(
    Handle<String> module_name, Handle<FixedArray> export_names,
    v8::Module::SyntheticModuleEvaluationSteps evaluation_steps) {
  ReadOnlyRoots roots(isolate());
  Handle<SyntheticModule> module(
      SyntheticModule::cast(New(synthetic_module_map(), AllocationType::kOld)),
      isolate());
  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), static_cast<int>(export_names->length()));
  Handle<Foreign> evaluation_steps_foreign =
      NewForeign(reinterpret_cast<i::Address>(evaluation_steps));
  module->set_exports(*exports);
  module->set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module->set_module_namespace(roots.undefined_value());
  module->set_status(Module::kUninstantiated);
  module->set_exception(roots.the_hole_value());
  module->set_name(*module_name);
  module->set_export_names(*export_names);
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
  typed_array->set_length(length);
  typed_array->SetOffHeapDataPtr(buffer->backing_store(), byte_offset);
  return typed_array;
}

Handle<JSDataView> Factory::NewJSDataView(Handle<JSArrayBuffer> buffer,
                                          size_t byte_offset,
                                          size_t byte_length) {
  Handle<Map> map(isolate()->native_context()->data_view_fun().initial_map(),
                  isolate());
  Handle<JSDataView> obj = Handle<JSDataView>::cast(NewJSArrayBufferView(
      map, empty_fixed_array(), buffer, byte_offset, byte_length));
  obj->set_data_pointer(static_cast<uint8_t*>(buffer->backing_store()) +
                        byte_offset);
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
  return Handle<JSGlobalProxy>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
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
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "SharedFunctionInfo",
      TRACE_ID_WITH_SCOPE(SharedFunctionInfo::kTraceScope,
                          shared->TraceID(literal)));
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "SharedFunctionInfo",
      TRACE_ID_WITH_SCOPE(SharedFunctionInfo::kTraceScope,
                          shared->TraceID(literal)),
      shared->ToTracedValue(literal));
  return shared;
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
      DCHECK_GE(bytecode_offset, 0);
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

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    MaybeHandle<String> maybe_name, MaybeHandle<HeapObject> maybe_function_data,
    int maybe_builtin_index, FunctionKind kind) {
  // Function names are assumed to be flat elsewhere. Must flatten before
  // allocating SharedFunctionInfo to avoid GC seeing the uninitialized SFI.
  Handle<String> shared_name;
  bool has_shared_name = maybe_name.ToHandle(&shared_name);
  if (has_shared_name) {
    shared_name = String::Flatten(isolate(), shared_name, AllocationType::kOld);
  }

  Handle<Map> map = shared_function_info_map();
  Handle<SharedFunctionInfo> share(
      SharedFunctionInfo::cast(New(map, AllocationType::kOld)), isolate());
  {
    DisallowHeapAllocation no_allocation;

    // Set pointer fields.
    share->set_name_or_scope_info(
        has_shared_name ? Object::cast(*shared_name)
                        : SharedFunctionInfo::kNoSharedNameSentinel);
    Handle<HeapObject> function_data;
    if (maybe_function_data.ToHandle(&function_data)) {
      // If we pass function_data then we shouldn't pass a builtin index, and
      // the function_data should not be code with a builtin.
      DCHECK(!Builtins::IsBuiltinId(maybe_builtin_index));
      DCHECK_IMPLIES(function_data->IsCode(),
                     !Code::cast(*function_data).is_builtin());
      share->set_function_data(*function_data);
    } else if (Builtins::IsBuiltinId(maybe_builtin_index)) {
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
    share->set_function_literal_id(kFunctionLiteralIdInvalid);
#if V8_SFI_HAS_UNIQUE_ID
    share->set_unique_id(isolate()->GetNextUniqueSharedFunctionInfoId());
#endif

    // Set integer fields (smi or int, depending on the architecture).
    share->set_length(0);
    share->set_internal_formal_parameter_count(0);
    share->set_expected_nof_properties(0);
    share->set_raw_function_token_offset(0);
    // All flags default to false or 0.
    share->set_flags(0);
    share->CalculateConstructAsBuiltin();
    share->set_kind(kind);

    share->clear_padding();
  }

#ifdef VERIFY_HEAP
  share->SharedFunctionInfoVerify(isolate());
#endif
  return share;
}

namespace {
inline int NumberToStringCacheHash(Handle<FixedArray> cache, Smi number) {
  int mask = (cache->length() >> 1) - 1;
  return number.value() & mask;
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
  Handle<String> js_string = NewStringFromAsciiChecked(
      string, check_cache ? AllocationType::kOld : AllocationType::kYoung);
  if (!check_cache) return js_string;

  if (!number_string_cache()->get(hash * 2).IsUndefined(isolate())) {
    int full_size = isolate()->heap()->MaxNumberToStringCacheSize();
    if (number_string_cache()->length() != full_size) {
      Handle<FixedArray> new_cache =
          NewFixedArray(full_size, AllocationType::kOld);
      isolate()->heap()->set_number_string_cache(*new_cache);
      return js_string;
    }
  }
  number_string_cache()->set(hash * 2, *number);
  number_string_cache()->set(hash * 2 + 1, *js_string);
  return js_string;
}

Handle<Object> Factory::NumberToStringCacheGet(Object number, int hash) {
  DisallowHeapAllocation no_gc;
  Object key = number_string_cache()->get(hash * 2);
  if (key == number || (key.IsHeapNumber() && number.IsHeapNumber() &&
                        key.Number() == number.Number())) {
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

Handle<String> Factory::NumberToString(Smi number, bool check_cache) {
  int hash = 0;
  if (check_cache) {
    hash = NumberToStringCacheHash(number_string_cache(), number);
    Handle<Object> cached = NumberToStringCacheGet(number, hash);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  const char* string = IntToCString(number.value(), buffer);

  return NumberToStringCacheSet(handle(number, isolate()), hash, string,
                                check_cache);
}

Handle<ClassPositions> Factory::NewClassPositions(int start, int end) {
  Handle<ClassPositions> class_positions = Handle<ClassPositions>::cast(
      NewStruct(CLASS_POSITIONS_TYPE, AllocationType::kOld));
  class_positions->set_start(start);
  class_positions->set_end(end);
  return class_positions;
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
  DCHECK(!shared->HasDebugInfo());
  debug_info->set_script(shared->script_or_debug_info());
  debug_info->set_original_bytecode_array(
      ReadOnlyRoots(heap).undefined_value());
  debug_info->set_debug_bytecode_array(ReadOnlyRoots(heap).undefined_value());
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
  Handle<BreakPointInfo> new_break_point_info = Handle<BreakPointInfo>::cast(
      NewStruct(TUPLE2_TYPE, AllocationType::kOld));
  new_break_point_info->set_source_position(source_position);
  new_break_point_info->set_break_points(*undefined_value());
  return new_break_point_info;
}

Handle<BreakPoint> Factory::NewBreakPoint(int id, Handle<String> condition) {
  Handle<BreakPoint> new_break_point =
      Handle<BreakPoint>::cast(NewStruct(TUPLE2_TYPE, AllocationType::kOld));
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

  int id = isolate()->last_stack_frame_info_id() + 1;
  isolate()->set_last_stack_frame_info_id(id);
  frame->set_id(id);
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

  const int script_id = frame->GetScriptId();

  Handle<Object> script_name = frame->GetFileName();
  Handle<Object> script_or_url = frame->GetScriptNameOrSourceUrl();

  // TODO(szuend): Adjust this, once it is decided what name to use in both
  //               "simple" and "detailed" stack traces. This code is for
  //               backwards compatibility to fullfill test expectations.
  auto function_name = frame->GetFunctionName();
  bool is_user_java_script = false;
  if (!is_wasm) {
    Handle<Object> function = frame->GetFunction();
    if (function->IsJSFunction()) {
      Handle<JSFunction> fun = Handle<JSFunction>::cast(function);

      is_user_java_script = fun->shared().IsUserJavaScript();
    }
  }

  Handle<Object> method_name = undefined_value();
  Handle<Object> type_name = undefined_value();
  Handle<Object> eval_origin = frame->GetEvalOrigin();
  Handle<Object> wasm_module_name = frame->GetWasmModuleName();
  Handle<Object> wasm_instance = frame->GetWasmInstance();

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

  DisallowHeapAllocation no_gc;

  info->set_flag(0);
  info->set_is_wasm(is_wasm);
  info->set_is_asmjs_wasm(frame_array->IsAsmJsWasmFrame(index));
  info->set_is_user_java_script(is_user_java_script);
  info->set_line_number(line);
  info->set_column_number(column);
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
  info->set_promise_all_index(frame->GetPromiseIndex());

  return info;
}

Handle<SourcePositionTableWithFrameCache>
Factory::NewSourcePositionTableWithFrameCache(
    Handle<ByteArray> source_position_table,
    Handle<SimpleNumberDictionary> stack_frame_cache) {
  Handle<SourcePositionTableWithFrameCache>
      source_position_table_with_frame_cache =
          Handle<SourcePositionTableWithFrameCache>::cast(
              NewStruct(SOURCE_POSITION_TABLE_WITH_FRAME_CACHE_TYPE,
                        AllocationType::kOld));
  source_position_table_with_frame_cache->set_source_position_table(
      *source_position_table);
  source_position_table_with_frame_cache->set_stack_frame_cache(
      *stack_frame_cache);
  return source_position_table_with_frame_cache;
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
  Smi uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);
  Smi ticks_until_tier_up = FLAG_regexp_tier_up
                                ? Smi::FromInt(FLAG_regexp_tier_up_ticks)
                                : uninitialized;
  store->set(JSRegExp::kTagIndex, Smi::FromInt(type));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpLatin1BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16BytecodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::kZero);
  store->set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store->set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  store->set(JSRegExp::kIrregexpTicksUntilTierUpIndex, ticks_until_tier_up);
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
  LOG(isolate(), MapDetails(*map));
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
  promise->set_reactions_or_result(Smi::kZero);
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

// static
NewFunctionArgs NewFunctionArgs::ForWasm(
    Handle<String> name,
    Handle<WasmExportedFunctionData> exported_function_data, Handle<Map> map) {
  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_map_ = map;
  args.maybe_wasm_function_data_ = exported_function_data;
  args.language_mode_ = LanguageMode::kSloppy;
  args.prototype_mutability_ = MUTABLE;

  return args;
}

// static
NewFunctionArgs NewFunctionArgs::ForWasm(
    Handle<String> name, Handle<WasmJSFunctionData> js_function_data,
    Handle<Map> map) {
  NewFunctionArgs args;
  args.name_ = name;
  args.maybe_map_ = map;
  args.maybe_wasm_function_data_ = js_function_data;
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
    Handle<String> name, Handle<HeapObject> prototype, InstanceType type,
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
