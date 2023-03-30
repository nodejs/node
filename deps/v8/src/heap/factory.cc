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
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/flags/flags.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap-allocator-inl.h"
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
#include "src/objects/call-site-info-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-shared-array-inl.h"
#include "src/objects/js-struct-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/string-set-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/roots/roots.h"
#include "src/strings/unicode-inl.h"
#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-value.h"
#endif

#include "src/heap/local-factory-inl.h"
#include "src/heap/local-heap-inl.h"

namespace v8 {
namespace internal {

Factory::CodeBuilder::CodeBuilder(Isolate* isolate, const CodeDesc& desc,
                                  CodeKind kind)
    : isolate_(isolate),
      local_isolate_(isolate_->main_thread_local_isolate()),
      code_desc_(desc),
      kind_(kind),
      position_table_(isolate_->factory()->empty_byte_array()) {}

Factory::CodeBuilder::CodeBuilder(LocalIsolate* local_isolate,
                                  const CodeDesc& desc, CodeKind kind)
    : isolate_(local_isolate->GetMainThreadIsolateUnsafe()),
      local_isolate_(local_isolate),
      code_desc_(desc),
      kind_(kind),
      position_table_(isolate_->factory()->empty_byte_array()) {}

MaybeHandle<Code> Factory::CodeBuilder::BuildInternal(
    bool retry_allocation_or_fail) {
  const auto factory = isolate_->factory();
  // Allocate objects needed for code initialization.
  Handle<ByteArray> reloc_info =
      CompiledWithConcurrentBaseline()
          ? local_isolate_->factory()->NewByteArray(code_desc_.reloc_size,
                                                    AllocationType::kOld)
          : factory->NewByteArray(code_desc_.reloc_size, AllocationType::kOld);

  Handle<Code> code;

  NewCodeOptions new_code_options = {
      /*kind=*/kind_,
      /*builtin=*/builtin_,
      /*is_turbofanned=*/is_turbofanned_,
      /*stack_slots=*/stack_slots_,
      /*kind_specific_flags=*/kind_specific_flags_,
      /*allocation=*/AllocationType::kOld,
      /*instruction_size=*/code_desc_.instruction_size(),
      /*metadata_size=*/code_desc_.metadata_size(),
      /*inlined_bytecode_size=*/inlined_bytecode_size_,
      /*osr_offset=*/osr_offset_,
      /*handler_table_offset=*/code_desc_.handler_table_offset_relative(),
      /*constant_pool_offset=*/code_desc_.constant_pool_offset_relative(),
      /*code_comments_offset=*/code_desc_.code_comments_offset_relative(),
      /*unwinding_info_offset=*/code_desc_.unwinding_info_offset_relative(),
      /*reloc_info=*/reloc_info,
      /*bytecode_or_deoptimization_data=*/kind_ == CodeKind::BASELINE
          ? interpreter_data_
          : deoptimization_data_,
      /*bytecode_offsets_or_source_position_table=*/position_table_};

  if (CompiledWithConcurrentBaseline()) {
    code = local_isolate_->factory()->NewCode(new_code_options);
  } else {
    code = factory->NewCode(new_code_options);
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
    Handle<ArrayList> new_list = ArrayList::Add(
        isolate_, list, on_heap_profiler_data, AllocationType::kOld);
    isolate_->heap()->SetBasicBlockProfilingData(new_list);
  }

  static_assert(InstructionStream::kOnHeapBodyIsContiguous);
  Heap* heap = isolate_->heap();
  CodePageCollectionMemoryModificationScope code_allocation(heap);

  Handle<InstructionStream> instruction_stream;
  if (CompiledWithConcurrentBaseline()) {
    if (!AllocateConcurrentSparkplugInstructionStream(retry_allocation_or_fail)
             .ToHandle(&instruction_stream)) {
      return {};
    }
  } else if (!AllocateInstructionStream(retry_allocation_or_fail)
                  .ToHandle(&instruction_stream)) {
    return {};
  }

  {
    InstructionStream raw_istream = *instruction_stream;
    DisallowGarbageCollection no_gc;

    // This might impact direct concurrent reads from TF if we are resetting
    // this field. We currently assume it's immutable thus a relaxed read (after
    // passing IsPendingAllocation).
    raw_istream.set_code(*code, kReleaseStore);

    // Allow self references to created code object by patching the handle to
    // point to the newly allocated InstructionStream object.
    Handle<Object> self_reference;
    if (self_reference_.ToHandle(&self_reference)) {
      DCHECK(self_reference->IsOddball());
      DCHECK_EQ(Oddball::cast(*self_reference).kind(),
                Oddball::kSelfReferenceMarker);
      DCHECK_NE(kind_, CodeKind::BASELINE);
      if (isolate_->IsGeneratingEmbeddedBuiltins()) {
        isolate_->builtins_constants_table_builder()->PatchSelfReference(
            self_reference, instruction_stream);
      }
      self_reference.PatchValue(*instruction_stream);
    }

    // Likewise, any references to the basic block counters marker need to be
    // updated to point to the newly-allocated counters array.
    if (!on_heap_profiler_data.is_null()) {
      isolate_->builtins_constants_table_builder()
          ->PatchBasicBlockCountersReference(
              handle(on_heap_profiler_data->counts(), isolate_));
    }

    if (V8_EXTERNAL_CODE_SPACE_BOOL) {
      raw_istream.set_main_cage_base(isolate_->cage_base(), kRelaxedStore);
    }
    code->SetInstructionStreamAndEntryPoint(isolate_, raw_istream);

    // Migrate generated code.
    // The generated code can contain embedded objects (typically from
    // handles) in a pointer-to-tagged-value format (i.e. with indirection
    // like a handle) that are dereferenced during the copy to point directly
    // to the actual heap objects. These pointers can include references to
    // the code object itself, through the self_reference parameter.
    code->CopyFromNoFlush(*reloc_info, heap, code_desc_);

    code->ClearInstructionStreamPadding();

#ifdef VERIFY_HEAP
    if (v8_flags.verify_heap) {
      HeapObject::VerifyCodePointer(isolate_, raw_istream);
    }
#endif

    // Flush the instruction cache before changing the permissions.
    // Note: we do this before setting permissions to ReadExecute because on
    // some older ARM kernels there is a bug which causes an access error on
    // cache flush instructions to trigger access error on non-writable memory.
    // See https://bugs.chromium.org/p/v8/issues/detail?id=8157
    code->FlushICache();
  }

  if (V8_UNLIKELY(profiler_data_ && v8_flags.turbo_profiling_verbose)) {
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

// TODO(victorgomes): Unify the two AllocateCodes
MaybeHandle<InstructionStream> Factory::CodeBuilder::AllocateInstructionStream(
    bool retry_allocation_or_fail) {
  Heap* heap = isolate_->heap();
  HeapAllocator* allocator = heap->allocator();
  HeapObject result;
  const AllocationType allocation_type = AllocationType::kCode;
  const int object_size = InstructionStream::SizeFor(code_desc_.body_size());
  if (retry_allocation_or_fail) {
    result = allocator->AllocateRawWith<HeapAllocator::kRetryOrFail>(
        object_size, allocation_type, AllocationOrigin::kRuntime);
  } else {
    result = allocator->AllocateRawWith<HeapAllocator::kLightRetry>(
        object_size, allocation_type, AllocationOrigin::kRuntime);
    // Return an empty handle if we cannot allocate the code object.
    if (result.is_null()) return MaybeHandle<InstructionStream>();
  }

  // The code object has not been fully initialized yet.  We rely on the
  // fact that no allocation will happen from this point on.
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(
      *isolate_->factory()->instruction_stream_map(), SKIP_WRITE_BARRIER);
  Handle<InstructionStream> istream =
      handle(InstructionStream::cast(result), isolate_);
  DCHECK(IsAligned(istream->instruction_start(), kCodeAlignment));
  DCHECK_IMPLIES(
      !V8_ENABLE_THIRD_PARTY_HEAP_BOOL && !heap->code_region().is_empty(),
      heap->code_region().contains(istream->address()));
  return istream;
}

MaybeHandle<InstructionStream>
Factory::CodeBuilder::AllocateConcurrentSparkplugInstructionStream(
    bool retry_allocation_or_fail) {
  LocalHeap* heap = local_isolate_->heap();
  const int object_size = InstructionStream::SizeFor(code_desc_.body_size());
  HeapObject result;
  if (!heap->AllocateRaw(object_size, AllocationType::kCode).To(&result)) {
    return MaybeHandle<InstructionStream>();
  }
  CHECK(!result.is_null());

  // The code object has not been fully initialized yet.  We rely on the
  // fact that no allocation will happen from this point on.
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(
      *local_isolate_->factory()->instruction_stream_map(), SKIP_WRITE_BARRIER);
  Handle<InstructionStream> istream =
      handle(InstructionStream::cast(result), local_isolate_);
  DCHECK(IsAligned(istream->instruction_start(), kCodeAlignment));
  return istream;
}

MaybeHandle<Code> Factory::CodeBuilder::TryBuild() {
  return BuildInternal(false);
}

Handle<Code> Factory::CodeBuilder::Build() {
  return BuildInternal(true).ToHandleChecked();
}

HeapObject Factory::AllocateRaw(int size, AllocationType allocation,
                                AllocationAlignment alignment) {
  return allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      size, allocation, AllocationOrigin::kRuntime, alignment);
}

HeapObject Factory::AllocateRawWithAllocationSite(
    Handle<Map> map, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  if (!allocation_site.is_null()) {
    DCHECK(V8_ALLOCATION_SITE_TRACKING_BOOL);
    size += ALIGN_TO_ALLOCATION_ALIGNMENT(AllocationMemento::kSize);
  }
  HeapObject result = allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      size, allocation);
  WriteBarrierMode write_barrier_mode = allocation == AllocationType::kYoung
                                            ? SKIP_WRITE_BARRIER
                                            : UPDATE_WRITE_BARRIER;
  result.set_map_after_allocation(*map, write_barrier_mode);
  if (!allocation_site.is_null()) {
    int aligned_size = ALIGN_TO_ALLOCATION_ALIGNMENT(map->instance_size());
    AllocationMemento alloc_memento =
        AllocationMemento::unchecked_cast(Object(result.ptr() + aligned_size));
    InitializeAllocationMemento(alloc_memento, *allocation_site);
  }
  return result;
}

void Factory::InitializeAllocationMemento(AllocationMemento memento,
                                          AllocationSite allocation_site) {
  DCHECK(V8_ALLOCATION_SITE_TRACKING_BOOL);
  memento.set_map_after_allocation(*allocation_memento_map(),
                                   SKIP_WRITE_BARRIER);
  memento.set_allocation_site(allocation_site, SKIP_WRITE_BARRIER);
  if (v8_flags.allocation_site_pretenuring) {
    allocation_site.IncrementMementoCreateCount();
  }
}

HeapObject Factory::New(Handle<Map> map, AllocationType allocation) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  HeapObject result = allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      size, allocation);
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode = allocation == AllocationType::kYoung
                                            ? SKIP_WRITE_BARRIER
                                            : UPDATE_WRITE_BARRIER;
  result.set_map_after_allocation(*map, write_barrier_mode);
  return result;
}

Handle<HeapObject> Factory::NewFillerObject(int size,
                                            AllocationAlignment alignment,
                                            AllocationType allocation,
                                            AllocationOrigin origin) {
  Heap* heap = isolate()->heap();
  HeapObject result = allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      size, allocation, origin, alignment);
  heap->CreateFillerObjectAt(result.address(), size);
  return Handle<HeapObject>(result, isolate());
}

Handle<PrototypeInfo> Factory::NewPrototypeInfo() {
  auto result = NewStructInternal<PrototypeInfo>(PROTOTYPE_INFO_TYPE,
                                                 AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  result.set_prototype_users(Smi::zero());
  result.set_registry_slot(PrototypeInfo::UNREGISTERED);
  result.set_bit_field(0);
  result.set_module_namespace(*undefined_value(), SKIP_WRITE_BARRIER);
  return handle(result, isolate());
}

Handle<EnumCache> Factory::NewEnumCache(Handle<FixedArray> keys,
                                        Handle<FixedArray> indices,
                                        AllocationType allocation) {
  DCHECK(allocation == AllocationType::kOld ||
         allocation == AllocationType::kSharedOld);
  DCHECK_EQ(allocation == AllocationType::kSharedOld,
            keys->InSharedHeap() && indices->InSharedHeap());
  auto result = NewStructInternal<EnumCache>(ENUM_CACHE_TYPE, allocation);
  DisallowGarbageCollection no_gc;
  result.set_keys(*keys);
  result.set_indices(*indices);
  return handle(result, isolate());
}

Handle<Tuple2> Factory::NewTuple2(Handle<Object> value1, Handle<Object> value2,
                                  AllocationType allocation) {
  auto result = NewStructInternal<Tuple2>(TUPLE2_TYPE, allocation);
  DisallowGarbageCollection no_gc;
  result.set_value1(*value1);
  result.set_value2(*value2);
  return handle(result, isolate());
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

Handle<PropertyArray> Factory::NewPropertyArray(int length,
                                                AllocationType allocation) {
  DCHECK_LE(0, length);
  if (length == 0) return empty_property_array();
  HeapObject result = AllocateRawFixedArray(length, allocation);
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(*property_array_map(), SKIP_WRITE_BARRIER);
  PropertyArray array = PropertyArray::cast(result);
  array.initialize_length(length);
  MemsetTagged(array.data_start(), read_only_roots().undefined_value(), length);
  return handle(array, isolate());
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
  if ((size > heap->MaxRegularHeapObjectSize(allocation_type)) &&
      v8_flags.use_marking_progress_bar) {
    LargePage::FromHeapObject(result)->ProgressBar().Enable();
  }
  DisallowGarbageCollection no_gc;
  result.set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  FixedArray array = FixedArray::cast(result);
  array.set_length(length);
  MemsetTagged(array.data_start(), *undefined_value(), length);
  return handle(array, isolate());
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
    Handle<ClosureFeedbackCellArray> closure_feedback_cell_array,
    Handle<FeedbackCell> parent_feedback_cell) {
  int length = shared->feedback_metadata().slot_count();
  DCHECK_LE(0, length);
  int size = FeedbackVector::SizeFor(length);

  FeedbackVector vector = FeedbackVector::cast(AllocateRawWithImmortalMap(
      size, AllocationType::kOld, *feedback_vector_map()));
  DisallowGarbageCollection no_gc;
  vector.set_shared_function_info(*shared);
  vector.set_maybe_optimized_code(HeapObjectReference::ClearedValue(isolate()));
  vector.set_length(length);
  vector.set_invocation_count(0);
  vector.set_profiler_ticks(0);
  vector.set_placeholder0(0);
  vector.reset_osr_state();
  vector.reset_flags();
  vector.set_log_next_execution(v8_flags.log_function_events);
  vector.set_closure_feedback_cell_array(*closure_feedback_cell_array);
  vector.set_parent_feedback_cell(*parent_feedback_cell);

  // TODO(leszeks): Initialize based on the feedback metadata.
  MemsetTagged(ObjectSlot(vector.slots_start()), *undefined_value(), length);
  return handle(vector, isolate());
}

Handle<EmbedderDataArray> Factory::NewEmbedderDataArray(int length) {
  DCHECK_LE(0, length);
  int size = EmbedderDataArray::SizeFor(length);
  EmbedderDataArray array = EmbedderDataArray::cast(AllocateRawWithImmortalMap(
      size, AllocationType::kYoung, *embedder_data_array_map()));
  DisallowGarbageCollection no_gc;
  array.set_length(length);

  if (length > 0) {
    for (int i = 0; i < length; i++) {
      // TODO(v8): consider initializing embedded data array with Smi::zero().
      EmbedderDataSlot(array, i).Initialize(*undefined_value());
    }
  }
  return handle(array, isolate());
}

Handle<FixedArrayBase> Factory::NewFixedDoubleArrayWithHoles(int length) {
  DCHECK_LE(0, length);
  Handle<FixedArrayBase> array = NewFixedDoubleArray(length);
  if (length > 0) {
    Handle<FixedDoubleArray>::cast(array)->FillWithHoles(0, length);
  }
  return array;
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
  auto object = NewStructInternal<PropertyDescriptorObject>(
      PROPERTY_DESCRIPTOR_OBJECT_TYPE, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  object.set_flags(0);
  Oddball the_hole = read_only_roots().the_hole_value();
  object.set_value(the_hole, SKIP_WRITE_BARRIER);
  object.set_get(the_hole, SKIP_WRITE_BARRIER);
  object.set_set(the_hole, SKIP_WRITE_BARRIER);
  return handle(object, isolate());
}

Handle<SwissNameDictionary> Factory::CreateCanonicalEmptySwissNameDictionary() {
  // This function is only supposed to be used to create the canonical empty
  // version and should not be used afterwards.
  DCHECK(!ReadOnlyRoots(isolate()).is_initialized(
      RootIndex::kEmptySwissPropertyDictionary));

  ReadOnlyRoots roots(isolate());

  Handle<ByteArray> empty_meta_table =
      NewByteArray(SwissNameDictionary::kMetaTableEnumerationDataStartIndex,
                   AllocationType::kReadOnly);

  Map map = roots.swiss_name_dictionary_map();
  int size = SwissNameDictionary::SizeFor(0);
  HeapObject obj =
      AllocateRawWithImmortalMap(size, AllocationType::kReadOnly, map);
  SwissNameDictionary result = SwissNameDictionary::cast(obj);
  result.Initialize(isolate(), *empty_meta_table, 0);
  return handle(result, isolate());
}

// Internalized strings are created in the old generation (data space).
Handle<String> Factory::InternalizeUtf8String(
    const base::Vector<const char>& string) {
  base::Vector<const uint8_t> utf8_data =
      base::Vector<const uint8_t>::cast(string);
  Utf8Decoder decoder(utf8_data);
  if (decoder.is_ascii()) return InternalizeString(utf8_data);
  if (decoder.is_one_byte()) {
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[decoder.utf16_length()]);
    decoder.Decode(buffer.get(), utf8_data);
    return InternalizeString(
        base::Vector<const uint8_t>(buffer.get(), decoder.utf16_length()));
  }
  std::unique_ptr<uint16_t[]> buffer(new uint16_t[decoder.utf16_length()]);
  decoder.Decode(buffer.get(), utf8_data);
  return InternalizeString(
      base::Vector<const base::uc16>(buffer.get(), decoder.utf16_length()));
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

namespace {
void ThrowInvalidEncodedStringBytes(Isolate* isolate, MessageTemplate message) {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(message == MessageTemplate::kWasmTrapStringInvalidWtf8 ||
         message == MessageTemplate::kWasmTrapStringInvalidUtf8);
  Handle<JSObject> error_obj = isolate->factory()->NewWasmRuntimeError(message);
  JSObject::AddProperty(isolate, error_obj,
                        isolate->factory()->wasm_uncatchable_symbol(),
                        isolate->factory()->true_value(), NONE);
  isolate->Throw(*error_obj);
#else
  // The default in JS-land is to use Utf8Variant::kLossyUtf8, which never
  // throws an error, so if there is no WebAssembly compiled in we'll never get
  // here.
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

template <typename Decoder, typename PeekBytes>
MaybeHandle<String> NewStringFromBytes(Isolate* isolate, PeekBytes peek_bytes,
                                       AllocationType allocation,
                                       MessageTemplate message) {
  Decoder decoder(peek_bytes());
  if (decoder.is_invalid()) {
    if (message != MessageTemplate::kNone) {
      ThrowInvalidEncodedStringBytes(isolate, message);
    }
    return MaybeHandle<String>();
  }

  if (decoder.utf16_length() == 0) return isolate->factory()->empty_string();

  if (decoder.is_one_byte()) {
    if (decoder.utf16_length() == 1) {
      uint8_t codepoint;
      decoder.Decode(&codepoint, peek_bytes());
      return isolate->factory()->LookupSingleCharacterStringFromCode(codepoint);
    }
    // Allocate string.
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawOneByteString(
                                   decoder.utf16_length(), allocation),
                               String);

    DisallowGarbageCollection no_gc;
    decoder.Decode(result->GetChars(no_gc), peek_bytes());
    return result;
  }

  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             isolate->factory()->NewRawTwoByteString(
                                 decoder.utf16_length(), allocation),
                             String);

  DisallowGarbageCollection no_gc;
  decoder.Decode(result->GetChars(no_gc), peek_bytes());
  return result;
}

template <typename PeekBytes>
MaybeHandle<String> NewStringFromUtf8Variant(Isolate* isolate,
                                             PeekBytes peek_bytes,
                                             unibrow::Utf8Variant utf8_variant,
                                             AllocationType allocation) {
  switch (utf8_variant) {
    case unibrow::Utf8Variant::kLossyUtf8:
      return NewStringFromBytes<Utf8Decoder>(isolate, peek_bytes, allocation,
                                             MessageTemplate::kNone);
#if V8_ENABLE_WEBASSEMBLY
    case unibrow::Utf8Variant::kUtf8:
      return NewStringFromBytes<StrictUtf8Decoder>(
          isolate, peek_bytes, allocation,
          MessageTemplate::kWasmTrapStringInvalidUtf8);
    case unibrow::Utf8Variant::kUtf8NoTrap:
      return NewStringFromBytes<StrictUtf8Decoder>(
          isolate, peek_bytes, allocation, MessageTemplate::kNone);
    case unibrow::Utf8Variant::kWtf8:
      return NewStringFromBytes<Wtf8Decoder>(
          isolate, peek_bytes, allocation,
          MessageTemplate::kWasmTrapStringInvalidWtf8);
#endif
  }
}

}  // namespace

MaybeHandle<String> Factory::NewStringFromUtf8(
    const base::Vector<const uint8_t>& string,
    unibrow::Utf8Variant utf8_variant, AllocationType allocation) {
  if (string.size() > kMaxInt) {
    // The Utf8Decode can't handle longer inputs, and we couldn't create
    // strings from them anyway.
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }
  auto peek_bytes = [&]() -> base::Vector<const uint8_t> { return string; };
  return NewStringFromUtf8Variant(isolate(), peek_bytes, utf8_variant,
                                  allocation);
}

MaybeHandle<String> Factory::NewStringFromUtf8(
    const base::Vector<const char>& string, AllocationType allocation) {
  return NewStringFromUtf8(base::Vector<const uint8_t>::cast(string),
                           unibrow::Utf8Variant::kLossyUtf8, allocation);
}

#if V8_ENABLE_WEBASSEMBLY
MaybeHandle<String> Factory::NewStringFromUtf8(
    Handle<WasmArray> array, uint32_t start, uint32_t end,
    unibrow::Utf8Variant utf8_variant, AllocationType allocation) {
  DCHECK_EQ(sizeof(uint8_t), array->type()->element_type().value_kind_size());
  DCHECK_LE(start, end);
  DCHECK_LE(end, array->length());
  // {end - start} can never be more than what the Utf8Decoder can handle.
  static_assert(WasmArray::MaxLength(sizeof(uint8_t)) <= kMaxInt);
  auto peek_bytes = [&]() -> base::Vector<const uint8_t> {
    const uint8_t* contents =
        reinterpret_cast<const uint8_t*>(array->ElementAddress(0));
    return {contents + start, end - start};
  };
  return NewStringFromUtf8Variant(isolate(), peek_bytes, utf8_variant,
                                  allocation);
}

MaybeHandle<String> Factory::NewStringFromUtf8(
    Handle<ByteArray> array, uint32_t start, uint32_t end,
    unibrow::Utf8Variant utf8_variant, AllocationType allocation) {
  DCHECK_LE(start, end);
  DCHECK_LE(end, array->length());
  // {end - start} can never be more than what the Utf8Decoder can handle.
  static_assert(ByteArray::kMaxLength <= kMaxInt);
  auto peek_bytes = [&]() -> base::Vector<const uint8_t> {
    const uint8_t* contents =
        reinterpret_cast<const uint8_t*>(array->GetDataStartAddress());
    return {contents + start, end - start};
  };
  return NewStringFromUtf8Variant(isolate(), peek_bytes, utf8_variant,
                                  allocation);
}

namespace {
struct Wtf16Decoder {
  int length_;
  bool is_one_byte_;
  explicit Wtf16Decoder(const base::Vector<const uint16_t>& data)
      : length_(data.length()),
        is_one_byte_(String::IsOneByte(data.begin(), length_)) {}
  bool is_invalid() const { return false; }
  bool is_one_byte() const { return is_one_byte_; }
  int utf16_length() const { return length_; }
  template <typename Char>
  void Decode(Char* out, const base::Vector<const uint16_t>& data) {
    CopyChars(out, data.begin(), length_);
  }
};
}  // namespace

MaybeHandle<String> Factory::NewStringFromUtf16(Handle<WasmArray> array,
                                                uint32_t start, uint32_t end,
                                                AllocationType allocation) {
  DCHECK_EQ(sizeof(uint16_t), array->type()->element_type().value_kind_size());
  DCHECK_LE(start, end);
  DCHECK_LE(end, array->length());
  // {end - start} can never be more than what the Utf8Decoder can handle.
  static_assert(WasmArray::MaxLength(sizeof(uint16_t)) <= kMaxInt);
  auto peek_bytes = [&]() -> base::Vector<const uint16_t> {
    const uint16_t* contents =
        reinterpret_cast<const uint16_t*>(array->ElementAddress(0));
    return {contents + start, end - start};
  };
  return NewStringFromBytes<Wtf16Decoder>(isolate(), peek_bytes, allocation,
                                          MessageTemplate::kNone);
}
#endif  // V8_ENABLE_WEBASSEMBLY

MaybeHandle<String> Factory::NewStringFromUtf8SubString(
    Handle<SeqOneByteString> str, int begin, int length,
    AllocationType allocation) {
  base::Vector<const uint8_t> utf8_data;
  {
    DisallowGarbageCollection no_gc;
    utf8_data =
        base::Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
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
    utf8_data =
        base::Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
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
  utf8_data = base::Vector<const uint8_t>(str->GetChars(no_gc) + begin, length);
  decoder.Decode(result->GetChars(no_gc), utf8_data);
  return result;
}

MaybeHandle<String> Factory::NewStringFromTwoByte(const base::uc16* string,
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
    const base::Vector<const base::uc16>& string, AllocationType allocation) {
  return NewStringFromTwoByte(string.begin(), string.length(), allocation);
}

MaybeHandle<String> Factory::NewStringFromTwoByte(
    const ZoneVector<base::uc16>* string, AllocationType allocation) {
  return NewStringFromTwoByte(string->data(), static_cast<int>(string->size()),
                              allocation);
}

#if V8_ENABLE_WEBASSEMBLY
MaybeHandle<String> Factory::NewStringFromTwoByteLittleEndian(
    const base::Vector<const base::uc16>& str, AllocationType allocation) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  return NewStringFromTwoByte(str, allocation);
#elif defined(V8_TARGET_BIG_ENDIAN)
  // TODO(12868): Duplicate the guts of NewStringFromTwoByte, so that
  // copying and transcoding the data can be done in a single pass.
  UNIMPLEMENTED();
#else
#error Unknown endianness
#endif
}
#endif  // V8_ENABLE_WEBASSEMBLY

Handle<String> Factory::NewInternalizedStringImpl(Handle<String> string,
                                                  int len,
                                                  uint32_t hash_field) {
  if (string->IsOneByteRepresentation()) {
    Handle<SeqOneByteString> result =
        AllocateRawOneByteInternalizedString(len, hash_field);
    DisallowGarbageCollection no_gc;
    String::WriteToFlat(*string, result->GetChars(no_gc), 0, len);
    return result;
  }

  Handle<SeqTwoByteString> result =
      AllocateRawTwoByteInternalizedString(len, hash_field);
  DisallowGarbageCollection no_gc;
  String::WriteToFlat(*string, result->GetChars(no_gc), 0, len);
  return result;
}

StringTransitionStrategy Factory::ComputeInternalizationStrategyForString(
    Handle<String> string, MaybeHandle<Map>* internalized_map) {
  // Do not internalize young strings in-place: This allows us to ignore both
  // string table and stub cache on scavenges.
  if (Heap::InYoungGeneration(*string)) {
    return StringTransitionStrategy::kCopy;
  }
  // If the string table is shared, we need to copy if the string is not already
  // in the shared heap.
  if (v8_flags.shared_string_table && !string->InSharedHeap()) {
    return StringTransitionStrategy::kCopy;
  }
  DCHECK_NOT_NULL(internalized_map);
  DisallowGarbageCollection no_gc;
  // This method may be called concurrently, so snapshot the map from the input
  // string instead of the calling IsType methods on HeapObject, which would
  // reload the map each time.
  Map map = string->map();
  *internalized_map = GetInPlaceInternalizedStringMap(map);
  if (!internalized_map->is_null()) {
    return StringTransitionStrategy::kInPlace;
  }
  if (InstanceTypeChecker::IsInternalizedString(map)) {
    return StringTransitionStrategy::kAlreadyTransitioned;
  }
  return StringTransitionStrategy::kCopy;
}

template <class StringClass>
Handle<StringClass> Factory::InternalizeExternalString(Handle<String> string) {
  Handle<Map> map =
      GetInPlaceInternalizedStringMap(string->map()).ToHandleChecked();
  StringClass external_string =
      StringClass::cast(New(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  external_string.InitExternalPointerFields(isolate());
  StringClass cast_string = StringClass::cast(*string);
  external_string.set_length(cast_string.length());
  external_string.set_raw_hash_field(cast_string.raw_hash_field());
  external_string.SetResource(isolate(), nullptr);
  isolate()->heap()->RegisterExternalString(external_string);
  return handle(external_string, isolate());
}

template Handle<ExternalOneByteString>
    Factory::InternalizeExternalString<ExternalOneByteString>(Handle<String>);
template Handle<ExternalTwoByteString>
    Factory::InternalizeExternalString<ExternalTwoByteString>(Handle<String>);

StringTransitionStrategy Factory::ComputeSharingStrategyForString(
    Handle<String> string, MaybeHandle<Map>* shared_map) {
  DCHECK(v8_flags.shared_string_table);
  // TODO(pthier): Avoid copying LO-space strings. Update page flags instead.
  if (!string->InSharedHeap()) {
    return StringTransitionStrategy::kCopy;
  }
  DCHECK_NOT_NULL(shared_map);
  DisallowGarbageCollection no_gc;
  InstanceType instance_type = string->map().instance_type();
  if (StringShape(instance_type).IsShared()) {
    return StringTransitionStrategy::kAlreadyTransitioned;
  }
  switch (instance_type) {
    case STRING_TYPE:
      *shared_map = read_only_roots().shared_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    case ONE_BYTE_STRING_TYPE:
      *shared_map = read_only_roots().shared_one_byte_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    case EXTERNAL_STRING_TYPE:
      *shared_map = read_only_roots().shared_external_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
      *shared_map =
          read_only_roots().shared_external_one_byte_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    case UNCACHED_EXTERNAL_STRING_TYPE:
      *shared_map =
          read_only_roots().shared_uncached_external_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    case UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      *shared_map = read_only_roots()
                        .shared_uncached_external_one_byte_string_map_handle();
      return StringTransitionStrategy::kInPlace;
    default:
      return StringTransitionStrategy::kCopy;
  }
}

Handle<String> Factory::NewSurrogatePairString(uint16_t lead, uint16_t trail) {
  DCHECK_GE(lead, 0xD800);
  DCHECK_LE(lead, 0xDBFF);
  DCHECK_GE(trail, 0xDC00);
  DCHECK_LE(trail, 0xDFFF);

  Handle<SeqTwoByteString> str =
      isolate()->factory()->NewRawTwoByteString(2).ToHandleChecked();
  DisallowGarbageCollection no_gc;
  base::uc16* dest = str->GetChars(no_gc);
  dest[0] = lead;
  dest[1] = trail;
  return str;
}

Handle<String> Factory::NewProperSubString(Handle<String> str, int begin,
                                           int end) {
#if VERIFY_HEAP
  if (v8_flags.verify_heap) str->StringVerify(isolate());
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

  if (!v8_flags.string_slices || length < SlicedString::kMinLength) {
    if (str->IsOneByteRepresentation()) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      DisallowGarbageCollection no_gc;
      uint8_t* dest = result->GetChars(no_gc);
      String::WriteToFlat(*str, dest, begin, length);
      return result;
    } else {
      Handle<SeqTwoByteString> result =
          NewRawTwoByteString(length).ToHandleChecked();
      DisallowGarbageCollection no_gc;
      base::uc16* dest = result->GetChars(no_gc);
      String::WriteToFlat(*str, dest, begin, length);
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
  SlicedString slice = SlicedString::cast(New(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  slice.set_raw_hash_field(String::kEmptyHashField);
  slice.set_length(length);
  slice.set_parent(*str);
  slice.set_offset(offset);
  return handle(slice, isolate());
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
  ExternalOneByteString external_string =
      ExternalOneByteString::cast(New(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  external_string.InitExternalPointerFields(isolate());
  external_string.set_length(static_cast<int>(length));
  external_string.set_raw_hash_field(String::kEmptyHashField);
  external_string.SetResource(isolate(), resource);

  isolate()->heap()->RegisterExternalString(external_string);

  return Handle<String>(external_string, isolate());
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
  ExternalTwoByteString string =
      ExternalTwoByteString::cast(New(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  string.InitExternalPointerFields(isolate());
  string.set_length(static_cast<int>(length));
  string.set_raw_hash_field(String::kEmptyHashField);
  string.SetResource(isolate(), resource);

  isolate()->heap()->RegisterExternalString(string);

  return Handle<ExternalTwoByteString>(string, isolate());
}

Handle<JSStringIterator> Factory::NewJSStringIterator(Handle<String> string) {
  Handle<Map> map(isolate()->native_context()->initial_string_iterator_map(),
                  isolate());
  Handle<String> flat_string = String::Flatten(isolate(), string);
  Handle<JSStringIterator> iterator =
      Handle<JSStringIterator>::cast(NewJSObjectFromMap(map));

  DisallowGarbageCollection no_gc;
  JSStringIterator raw = *iterator;
  raw.set_string(*flat_string);
  raw.set_index(0);
  return iterator;
}

Symbol Factory::NewSymbolInternal(AllocationType allocation) {
  DCHECK(allocation != AllocationType::kYoung);
  // Statically ensure that it is safe to allocate symbols in paged spaces.
  static_assert(Symbol::kSize <= kMaxRegularHeapObjectSize);

  Symbol symbol = Symbol::cast(AllocateRawWithImmortalMap(
      Symbol::kSize, allocation, read_only_roots().symbol_map()));
  DisallowGarbageCollection no_gc;
  // Generate a random hash value.
  int hash = isolate()->GenerateIdentityHash(Name::HashBits::kMax);
  symbol.set_raw_hash_field(
      Name::CreateHashFieldValue(hash, Name::HashFieldType::kHash));
  if (isolate()->read_only_heap()->roots_init_complete()) {
    symbol.set_description(read_only_roots().undefined_value(),
                           SKIP_WRITE_BARRIER);
  } else {
    // Can't use setter during bootstrapping as its typecheck tries to access
    // the roots table before it is initialized.
    TaggedField<Object>::store(symbol, Symbol::kDescriptionOffset,
                               read_only_roots().undefined_value());
  }
  symbol.set_flags(0);
  DCHECK(!symbol.is_private());
  return symbol;
}

Handle<Symbol> Factory::NewSymbol(AllocationType allocation) {
  return handle(NewSymbolInternal(allocation), isolate());
}

Handle<Symbol> Factory::NewPrivateSymbol(AllocationType allocation) {
  DCHECK(allocation != AllocationType::kYoung);
  Symbol symbol = NewSymbolInternal(allocation);
  DisallowGarbageCollection no_gc;
  symbol.set_is_private(true);
  return handle(symbol, isolate());
}

Handle<Symbol> Factory::NewPrivateNameSymbol(Handle<String> name) {
  Symbol symbol = NewSymbolInternal();
  DisallowGarbageCollection no_gc;
  symbol.set_is_private_name();
  symbol.set_description(*name);
  return handle(symbol, isolate());
}

Context Factory::NewContextInternal(Handle<Map> map, int size,
                                    int variadic_part_length,
                                    AllocationType allocation) {
  DCHECK_LE(Context::kTodoHeaderSize, size);
  DCHECK(IsAligned(size, kTaggedSize));
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  DCHECK_LE(Context::SizeFor(variadic_part_length), size);

  HeapObject result = allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      size, allocation);
  result.set_map_after_allocation(*map);
  DisallowGarbageCollection no_gc;
  Context context = Context::cast(result);
  context.set_length(variadic_part_length);
  DCHECK_EQ(context.SizeFromMap(*map), size);
  if (size > Context::kTodoHeaderSize) {
    ObjectSlot start = context.RawField(Context::kTodoHeaderSize);
    ObjectSlot end = context.RawField(size);
    size_t slot_count = end - start;
    MemsetTagged(start, *undefined_value(), slot_count);
  }
  return context;
}

Handle<NativeContext> Factory::NewNativeContext() {
  Handle<Map> map = NewMap(NATIVE_CONTEXT_TYPE, kVariableSizeSentinel);
  NativeContext context = NativeContext::cast(NewContextInternal(
      map, NativeContext::kSize, NativeContext::NATIVE_CONTEXT_SLOTS,
      AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  context.set_native_context_map(*map);
  map->set_native_context(context);
  // The ExternalPointerTable is a C++ object.
  context.set_scope_info(*native_scope_info());
  context.set_previous(Context());
  context.set_extension(*undefined_value());
  context.set_errors_thrown(Smi::zero());
  context.set_math_random_index(Smi::zero());
  context.set_serialized_objects(*empty_fixed_array());
  context.init_microtask_queue(isolate(), nullptr);
  context.set_retained_maps(*empty_weak_array_list());
  return handle(context, isolate());
}

Handle<Context> Factory::NewScriptContext(Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), SCRIPT_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Context context =
      NewContextInternal(handle(outer->script_context_map(), isolate()),
                         Context::SizeFor(variadic_part_length),
                         variadic_part_length, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  context.set_scope_info(*scope_info);
  context.set_previous(*outer);
  DCHECK(context.IsScriptContext());
  return handle(context, isolate());
}

Handle<ScriptContextTable> Factory::NewScriptContextTable() {
  Handle<ScriptContextTable> context_table = Handle<ScriptContextTable>::cast(
      NewFixedArrayWithMap(read_only_roots().script_context_table_map_handle(),
                           ScriptContextTable::kMinLength));
  Handle<NameToIndexHashTable> names = NameToIndexHashTable::New(isolate(), 16);
  context_table->set_used(0, kReleaseStore);
  context_table->set_names_to_context_index(*names);
  return context_table;
}

Handle<Context> Factory::NewModuleContext(Handle<SourceTextModule> module,
                                          Handle<NativeContext> outer,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), MODULE_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Context context = NewContextInternal(
      isolate()->module_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  context.set_scope_info(*scope_info);
  context.set_previous(*outer);
  context.set_extension(*module);
  DCHECK(context.IsModuleContext());
  return handle(context, isolate());
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
  Context context =
      NewContextInternal(map, Context::SizeFor(variadic_part_length),
                         variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  context.set_scope_info(*scope_info);
  context.set_previous(*outer);
  return handle(context, isolate());
}

Handle<Context> Factory::NewCatchContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info,
                                         Handle<Object> thrown_object) {
  DCHECK_EQ(scope_info->scope_type(), CATCH_SCOPE);
  static_assert(Context::MIN_CONTEXT_SLOTS == Context::THROWN_OBJECT_INDEX);
  // TODO(ishell): Take the details from CatchContext class.
  int variadic_part_length = Context::MIN_CONTEXT_SLOTS + 1;
  Context context = NewContextInternal(
      isolate()->catch_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  DCHECK_IMPLIES(!v8_flags.single_generation, Heap::InYoungGeneration(context));
  context.set_scope_info(*scope_info, SKIP_WRITE_BARRIER);
  context.set_previous(*previous, SKIP_WRITE_BARRIER);
  context.set(Context::THROWN_OBJECT_INDEX, *thrown_object, SKIP_WRITE_BARRIER);
  return handle(context, isolate());
}

Handle<Context> Factory::NewDebugEvaluateContext(Handle<Context> previous,
                                                 Handle<ScopeInfo> scope_info,
                                                 Handle<JSReceiver> extension,
                                                 Handle<Context> wrapped) {
  DCHECK(scope_info->IsDebugEvaluateScope());
  Handle<HeapObject> ext = extension.is_null()
                               ? Handle<HeapObject>::cast(undefined_value())
                               : Handle<HeapObject>::cast(extension);
  // TODO(ishell): Take the details from DebugEvaluateContextContext class.
  int variadic_part_length = Context::MIN_CONTEXT_EXTENDED_SLOTS + 1;
  Context context =
      NewContextInternal(isolate()->debug_evaluate_context_map(),
                         Context::SizeFor(variadic_part_length),
                         variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  DCHECK_IMPLIES(!v8_flags.single_generation, Heap::InYoungGeneration(context));
  context.set_scope_info(*scope_info, SKIP_WRITE_BARRIER);
  context.set_previous(*previous, SKIP_WRITE_BARRIER);
  context.set_extension(*ext, SKIP_WRITE_BARRIER);
  if (!wrapped.is_null()) {
    context.set(Context::WRAPPED_CONTEXT_INDEX, *wrapped, SKIP_WRITE_BARRIER);
  }
  return handle(context, isolate());
}

Handle<Context> Factory::NewWithContext(Handle<Context> previous,
                                        Handle<ScopeInfo> scope_info,
                                        Handle<JSReceiver> extension) {
  DCHECK_EQ(scope_info->scope_type(), WITH_SCOPE);
  // TODO(ishell): Take the details from WithContext class.
  int variadic_part_length = Context::MIN_CONTEXT_EXTENDED_SLOTS;
  Context context = NewContextInternal(
      isolate()->with_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  DCHECK_IMPLIES(!v8_flags.single_generation, Heap::InYoungGeneration(context));
  context.set_scope_info(*scope_info, SKIP_WRITE_BARRIER);
  context.set_previous(*previous, SKIP_WRITE_BARRIER);
  context.set_extension(*extension, SKIP_WRITE_BARRIER);
  return handle(context, isolate());
}

Handle<Context> Factory::NewBlockContext(Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  DCHECK_IMPLIES(scope_info->scope_type() != BLOCK_SCOPE,
                 scope_info->scope_type() == CLASS_SCOPE);
  int variadic_part_length = scope_info->ContextLength();
  Context context = NewContextInternal(
      isolate()->block_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  DCHECK_IMPLIES(!v8_flags.single_generation, Heap::InYoungGeneration(context));
  context.set_scope_info(*scope_info, SKIP_WRITE_BARRIER);
  context.set_previous(*previous, SKIP_WRITE_BARRIER);
  return handle(context, isolate());
}

Handle<Context> Factory::NewBuiltinContext(Handle<NativeContext> native_context,
                                           int variadic_part_length) {
  DCHECK_LE(Context::MIN_CONTEXT_SLOTS, variadic_part_length);
  Context context = NewContextInternal(
      isolate()->function_context_map(), Context::SizeFor(variadic_part_length),
      variadic_part_length, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  DCHECK_IMPLIES(!v8_flags.single_generation, Heap::InYoungGeneration(context));
  context.set_scope_info(read_only_roots().empty_scope_info(),
                         SKIP_WRITE_BARRIER);
  context.set_previous(*native_context, SKIP_WRITE_BARRIER);
  return handle(context, isolate());
}

Handle<AliasedArgumentsEntry> Factory::NewAliasedArgumentsEntry(
    int aliased_context_slot) {
  auto entry = NewStructInternal<AliasedArgumentsEntry>(
      ALIASED_ARGUMENTS_ENTRY_TYPE, AllocationType::kYoung);
  entry.set_aliased_context_slot(aliased_context_slot);
  return handle(entry, isolate());
}

Handle<AccessorInfo> Factory::NewAccessorInfo() {
  AccessorInfo info =
      AccessorInfo::cast(New(accessor_info_map(), AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  info.set_name(*empty_string(), SKIP_WRITE_BARRIER);
  info.set_data(*undefined_value(), SKIP_WRITE_BARRIER);
  info.set_flags(0);  // Must clear the flags, it was initialized as undefined.
  info.set_is_sloppy(true);
  info.set_initial_property_attributes(NONE);

  info.init_getter(isolate(), kNullAddress);
  info.init_setter(isolate(), kNullAddress);

  info.clear_padding();

  return handle(info, isolate());
}

Handle<ErrorStackData> Factory::NewErrorStackData(
    Handle<Object> call_site_infos_or_formatted_stack,
    Handle<Object> limit_or_stack_frame_infos) {
  ErrorStackData error_stack_data = NewStructInternal<ErrorStackData>(
      ERROR_STACK_DATA_TYPE, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  error_stack_data.set_call_site_infos_or_formatted_stack(
      *call_site_infos_or_formatted_stack, SKIP_WRITE_BARRIER);
  error_stack_data.set_limit_or_stack_frame_infos(*limit_or_stack_frame_infos,
                                                  SKIP_WRITE_BARRIER);
  return handle(error_stack_data, isolate());
}

void Factory::AddToScriptList(Handle<Script> script) {
  Handle<WeakArrayList> scripts = script_list();
  scripts =
      WeakArrayList::Append(isolate(), scripts, MaybeObjectHandle::Weak(script),
                            AllocationType::kOld);
  isolate()->heap()->set_script_list(*scripts);
}

Handle<Script> Factory::CloneScript(Handle<Script> script) {
  Heap* heap = isolate()->heap();
  int script_id = isolate()->GetNextScriptId();
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
  Handle<ArrayList> list = ArrayList::New(isolate(), 0);
#endif
  Handle<Script> new_script_handle =
      Handle<Script>::cast(NewStruct(SCRIPT_TYPE, AllocationType::kOld));
  {
    DisallowGarbageCollection no_gc;
    Script new_script = *new_script_handle;
    const Script old_script = *script;
    new_script.set_source(old_script.source());
    new_script.set_name(old_script.name());
    new_script.set_id(script_id);
    new_script.set_line_offset(old_script.line_offset());
    new_script.set_column_offset(old_script.column_offset());
    new_script.set_context_data(old_script.context_data());
    new_script.set_type(old_script.type());
    new_script.set_line_ends(*undefined_value(), SKIP_WRITE_BARRIER);
    new_script.set_eval_from_shared_or_wrapped_arguments(
        script->eval_from_shared_or_wrapped_arguments());
    new_script.set_shared_function_infos(*empty_weak_fixed_array(),
                                         SKIP_WRITE_BARRIER);
    new_script.set_eval_from_position(old_script.eval_from_position());
    new_script.set_flags(old_script.flags());
    new_script.set_host_defined_options(old_script.host_defined_options());
    new_script.set_source_hash(*undefined_value(), SKIP_WRITE_BARRIER);
    new_script.set_compiled_lazy_function_positions(*undefined_value(),
                                                    SKIP_WRITE_BARRIER);
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
    new_script.set_script_or_modules(*list);
#endif
  }

  Handle<WeakArrayList> scripts = script_list();
  scripts = WeakArrayList::AddToEnd(isolate(), scripts,
                                    MaybeObjectHandle::Weak(new_script_handle));
  heap->set_script_list(*scripts);
  LOG(isolate(), ScriptEvent(ScriptEventType::kCreate, script_id));
  return new_script_handle;
}

Handle<CallableTask> Factory::NewCallableTask(Handle<JSReceiver> callable,
                                              Handle<Context> context) {
  DCHECK(callable->IsCallable());
  auto microtask = NewStructInternal<CallableTask>(CALLABLE_TASK_TYPE,
                                                   AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  microtask.set_callable(*callable, SKIP_WRITE_BARRIER);
  microtask.set_context(*context, SKIP_WRITE_BARRIER);
  return handle(microtask, isolate());
}

Handle<CallbackTask> Factory::NewCallbackTask(Handle<Foreign> callback,
                                              Handle<Foreign> data) {
  auto microtask = NewStructInternal<CallbackTask>(CALLBACK_TASK_TYPE,
                                                   AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  microtask.set_callback(*callback, SKIP_WRITE_BARRIER);
  microtask.set_data(*data, SKIP_WRITE_BARRIER);
  return handle(microtask, isolate());
}

Handle<PromiseResolveThenableJobTask> Factory::NewPromiseResolveThenableJobTask(
    Handle<JSPromise> promise_to_resolve, Handle<JSReceiver> thenable,
    Handle<JSReceiver> then, Handle<Context> context) {
  DCHECK(then->IsCallable());
  auto microtask = NewStructInternal<PromiseResolveThenableJobTask>(
      PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  microtask.set_promise_to_resolve(*promise_to_resolve, SKIP_WRITE_BARRIER);
  microtask.set_thenable(*thenable, SKIP_WRITE_BARRIER);
  microtask.set_then(*then, SKIP_WRITE_BARRIER);
  microtask.set_context(*context, SKIP_WRITE_BARRIER);
  return handle(microtask, isolate());
}

Handle<Foreign> Factory::NewForeign(Address addr,
                                    AllocationType allocation_type) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  static_assert(Foreign::kSize <= kMaxRegularHeapObjectSize);
  Map map = *foreign_map();
  Foreign foreign = Foreign::cast(
      AllocateRawWithImmortalMap(map.instance_size(), allocation_type, map));
  DisallowGarbageCollection no_gc;
  foreign.init_foreign_address(isolate(), addr);
  return handle(foreign, isolate());
}

#if V8_ENABLE_WEBASSEMBLY
Handle<WasmTypeInfo> Factory::NewWasmTypeInfo(
    Address type_address, Handle<Map> opt_parent, int instance_size_bytes,
    Handle<WasmInstanceObject> instance, uint32_t type_index) {
  // We pretenure WasmTypeInfo objects for two reasons:
  // (1) They are referenced by Maps, which are assumed to be long-lived,
  //     so pretenuring the WTI is a bit more efficient.
  // (2) The object visitors need to read the WasmTypeInfo to find tagged
  //     fields in Wasm structs; in the middle of a GC cycle that's only
  //     safe to do if the WTI is in old space.
  std::vector<Handle<Object>> supertypes;
  if (opt_parent.is_null()) {
    supertypes.resize(wasm::kMinimumSupertypeArraySize, undefined_value());
  } else {
    Handle<WasmTypeInfo> parent_type_info =
        handle(opt_parent->wasm_type_info(), isolate());
    int first_undefined_index = -1;
    for (int i = 0; i < parent_type_info->supertypes_length(); i++) {
      Handle<Object> supertype =
          handle(parent_type_info->supertypes(i), isolate());
      if (supertype->IsUndefined() && first_undefined_index == -1) {
        first_undefined_index = i;
      }
      supertypes.emplace_back(supertype);
    }
    if (first_undefined_index >= 0) {
      supertypes[first_undefined_index] = opt_parent;
    } else {
      supertypes.emplace_back(opt_parent);
    }
  }
  Map map = *wasm_type_info_map();
  WasmTypeInfo result = WasmTypeInfo::cast(AllocateRawWithImmortalMap(
      WasmTypeInfo::SizeFor(static_cast<int>(supertypes.size())),
      AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  result.set_supertypes_length(static_cast<int>(supertypes.size()));
  for (size_t i = 0; i < supertypes.size(); i++) {
    result.set_supertypes(static_cast<int>(i), *supertypes[i]);
  }
  result.init_native_type(isolate(), type_address);
  result.set_instance(*instance);
  result.set_type_index(type_index);
  return handle(result, isolate());
}

Handle<WasmApiFunctionRef> Factory::NewWasmApiFunctionRef(
    Handle<JSReceiver> callable, wasm::Suspend suspend,
    Handle<WasmInstanceObject> instance) {
  Map map = *wasm_api_function_ref_map();
  auto result = WasmApiFunctionRef::cast(AllocateRawWithImmortalMap(
      map.instance_size(), AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  result.set_native_context(*isolate()->native_context());
  if (!callable.is_null()) {
    result.set_callable(*callable);
  } else {
    result.set_callable(*undefined_value());
  }
  result.set_suspend(suspend);
  if (!instance.is_null()) {
    result.set_instance(*instance);
  } else {
    result.set_instance(*undefined_value());
  }
  return handle(result, isolate());
}

Handle<WasmInternalFunction> Factory::NewWasmInternalFunction(
    Address opt_call_target, Handle<HeapObject> ref, Handle<Map> rtt) {
  HeapObject raw = AllocateRaw(rtt->instance_size(), AllocationType::kOld);
  raw.set_map_after_allocation(*rtt);
  WasmInternalFunction result = WasmInternalFunction::cast(raw);
  DisallowGarbageCollection no_gc;
  result.init_call_target(isolate(), opt_call_target);
  result.set_ref(*ref);
  // Default values, will be overwritten by the caller.
  result.set_code(*BUILTIN_CODE(isolate(), Abort));
  result.set_external(*undefined_value());
  return handle(result, isolate());
}

Handle<WasmJSFunctionData> Factory::NewWasmJSFunctionData(
    Address opt_call_target, Handle<JSReceiver> callable, int return_count,
    int parameter_count, Handle<PodArray<wasm::ValueType>> serialized_sig,
    Handle<Code> wrapper_code, Handle<Map> rtt, wasm::Suspend suspend,
    wasm::Promise promise) {
  Handle<WasmApiFunctionRef> ref =
      NewWasmApiFunctionRef(callable, suspend, Handle<WasmInstanceObject>());
  Handle<WasmInternalFunction> internal =
      NewWasmInternalFunction(opt_call_target, ref, rtt);
  Map map = *wasm_js_function_data_map();
  WasmJSFunctionData result =
      WasmJSFunctionData::cast(AllocateRawWithImmortalMap(
          map.instance_size(), AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  result.set_internal(*internal);
  result.set_wrapper_code(*wrapper_code);
  result.set_serialized_return_count(return_count);
  result.set_serialized_parameter_count(parameter_count);
  result.set_serialized_signature(*serialized_sig);
  result.set_js_promise_flags(WasmFunctionData::SuspendField::encode(suspend) |
                              WasmFunctionData::PromiseField::encode(promise));
  return handle(result, isolate());
}

Handle<WasmResumeData> Factory::NewWasmResumeData(
    Handle<WasmSuspenderObject> suspender, wasm::OnResume on_resume) {
  Map map = *wasm_resume_data_map();
  WasmResumeData result = WasmResumeData::cast(AllocateRawWithImmortalMap(
      map.instance_size(), AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  result.set_suspender(*suspender);
  result.set_on_resume(static_cast<int>(on_resume));
  return handle(result, isolate());
}

Handle<WasmExportedFunctionData> Factory::NewWasmExportedFunctionData(
    Handle<Code> export_wrapper, Handle<WasmInstanceObject> instance,
    Address call_target, Handle<Object> ref, int func_index,
    const wasm::FunctionSig* sig, uint32_t canonical_type_index,
    int wrapper_budget, Handle<Map> rtt, wasm::Promise promise) {
  Handle<WasmInternalFunction> internal =
      NewWasmInternalFunction(call_target, Handle<HeapObject>::cast(ref), rtt);
  Map map = *wasm_exported_function_data_map();
  WasmExportedFunctionData result =
      WasmExportedFunctionData::cast(AllocateRawWithImmortalMap(
          map.instance_size(), AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  DCHECK(ref->IsWasmInstanceObject() || ref->IsWasmApiFunctionRef());
  result.set_internal(*internal);
  result.set_wrapper_code(*export_wrapper);
  result.set_instance(*instance);
  result.set_function_index(func_index);
  result.init_sig(isolate(), sig);
  result.set_canonical_type_index(canonical_type_index);
  result.set_wrapper_budget(wrapper_budget);
  // We can't skip the write barrier because Code objects are not immovable.
  result.set_c_wrapper_code(*BUILTIN_CODE(isolate(), Illegal),
                            UPDATE_WRITE_BARRIER);
  result.set_packed_args_size(0);
  result.set_js_promise_flags(
      WasmFunctionData::SuspendField::encode(wasm::kNoSuspend) |
      WasmFunctionData::PromiseField::encode(promise));
  return handle(result, isolate());
}

Handle<WasmCapiFunctionData> Factory::NewWasmCapiFunctionData(
    Address call_target, Handle<Foreign> embedder_data,
    Handle<Code> wrapper_code, Handle<Map> rtt,
    Handle<PodArray<wasm::ValueType>> serialized_sig) {
  Handle<WasmApiFunctionRef> ref = NewWasmApiFunctionRef(
      Handle<JSReceiver>(), wasm::kNoSuspend, Handle<WasmInstanceObject>());
  Handle<WasmInternalFunction> internal =
      NewWasmInternalFunction(call_target, ref, rtt);
  Map map = *wasm_capi_function_data_map();
  WasmCapiFunctionData result =
      WasmCapiFunctionData::cast(AllocateRawWithImmortalMap(
          map.instance_size(), AllocationType::kOld, map));
  DisallowGarbageCollection no_gc;
  result.set_internal(*internal);
  result.set_wrapper_code(*wrapper_code);
  result.set_embedder_data(*embedder_data);
  result.set_serialized_signature(*serialized_sig);
  result.set_js_promise_flags(
      WasmFunctionData::SuspendField::encode(wasm::kNoSuspend) |
      WasmFunctionData::PromiseField::encode(wasm::kNoPromise));
  return handle(result, isolate());
}

Handle<WasmArray> Factory::NewWasmArray(const wasm::ArrayType* type,
                                        uint32_t length,
                                        wasm::WasmValue initial_value,
                                        Handle<Map> map) {
  HeapObject raw =
      AllocateRaw(WasmArray::SizeFor(*map, length), AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  raw.set_map_after_allocation(*map);
  WasmArray result = WasmArray::cast(raw);
  result.set_raw_properties_or_hash(*empty_fixed_array(), kRelaxedStore);
  result.set_length(length);
  if (type->element_type().is_numeric()) {
    if (initial_value.zero_byte_representation()) {
      memset(reinterpret_cast<void*>(result.ElementAddress(0)), 0,
             length * type->element_type().value_kind_size());
    } else {
      wasm::WasmValue packed = initial_value.Packed(type->element_type());
      for (uint32_t i = 0; i < length; i++) {
        Address address = result.ElementAddress(i);
        packed.CopyTo(reinterpret_cast<byte*>(address));
      }
    }
  } else {
    for (uint32_t i = 0; i < length; i++) {
      result.SetTaggedElement(i, initial_value.to_ref());
    }
  }
  return handle(result, isolate());
}

Handle<WasmArray> Factory::NewWasmArrayFromElements(
    const wasm::ArrayType* type, const std::vector<wasm::WasmValue>& elements,
    Handle<Map> map) {
  uint32_t length = static_cast<uint32_t>(elements.size());
  HeapObject raw =
      AllocateRaw(WasmArray::SizeFor(*map, length), AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  raw.set_map_after_allocation(*map);
  WasmArray result = WasmArray::cast(raw);
  result.set_raw_properties_or_hash(*empty_fixed_array(), kRelaxedStore);
  result.set_length(length);
  if (type->element_type().is_numeric()) {
    for (uint32_t i = 0; i < length; i++) {
      Address address = result.ElementAddress(i);
      elements[i]
          .Packed(type->element_type())
          .CopyTo(reinterpret_cast<byte*>(address));
    }
  } else {
    for (uint32_t i = 0; i < length; i++) {
      result.SetTaggedElement(i, elements[i].to_ref());
    }
  }
  return handle(result, isolate());
}

Handle<WasmArray> Factory::NewWasmArrayFromMemory(uint32_t length,
                                                  Handle<Map> map,
                                                  Address source) {
  wasm::ValueType element_type =
      reinterpret_cast<wasm::ArrayType*>(map->wasm_type_info().native_type())
          ->element_type();
  DCHECK(element_type.is_numeric());
  HeapObject raw =
      AllocateRaw(WasmArray::SizeFor(*map, length), AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  raw.set_map_after_allocation(*map);
  WasmArray result = WasmArray::cast(raw);
  result.set_raw_properties_or_hash(*empty_fixed_array(), kRelaxedStore);
  result.set_length(length);
  MemCopy(reinterpret_cast<void*>(result.ElementAddress(0)),
          reinterpret_cast<void*>(source),
          length * element_type.value_kind_size());

  return handle(result, isolate());
}

Handle<Object> Factory::NewWasmArrayFromElementSegment(
    Handle<WasmInstanceObject> instance, uint32_t segment_index,
    uint32_t start_offset, uint32_t length, Handle<Map> map) {
  DCHECK(WasmArray::type(*map)->element_type().is_reference());
  HeapObject raw =
      AllocateRaw(WasmArray::SizeFor(*map, length), AllocationType::kYoung);
  {
    DisallowGarbageCollection no_gc;
    raw.set_map_after_allocation(*map);
    WasmArray result = WasmArray::cast(raw);
    result.set_raw_properties_or_hash(*empty_fixed_array(), kRelaxedStore);
    result.set_length(length);
    // We have to initialize the elements to a default value, because we might
    // allocate new objects while computing the elements below.
    for (uint32_t i = 0; i < length; i++) {
      result.SetTaggedElement(i, undefined_value(), SKIP_WRITE_BARRIER);
    }
  }

  Handle<WasmArray> result = handle(WasmArray::cast(raw), isolate());

  // Lazily initialize the element segment if needed.
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  base::Optional<MessageTemplate> opt_error =
      wasm::InitializeElementSegment(&zone, isolate(), instance, segment_index);
  if (opt_error.has_value()) {
    return handle(Smi::FromEnum(opt_error.value()), isolate());
  }

  Handle<FixedArray> elements =
      handle(FixedArray::cast(instance->element_segments().get(segment_index)),
             isolate());

  for (uint32_t i = 0; i < length; i++) {
    result->SetTaggedElement(
        i, handle(elements->get(start_offset + i), isolate()));
  }

  return result;
}

Handle<WasmStruct> Factory::NewWasmStruct(const wasm::StructType* type,
                                          wasm::WasmValue* args,
                                          Handle<Map> map) {
  HeapObject raw = AllocateRaw(WasmStruct::Size(type), AllocationType::kYoung);
  raw.set_map_after_allocation(*map);
  WasmStruct result = WasmStruct::cast(raw);
  result.set_raw_properties_or_hash(*empty_fixed_array(), kRelaxedStore);
  for (uint32_t i = 0; i < type->field_count(); i++) {
    int offset = type->field_offset(i);
    if (type->field(i).is_numeric()) {
      Address address = result.RawFieldAddress(offset);
      args[i].Packed(type->field(i)).CopyTo(reinterpret_cast<byte*>(address));
    } else {
      offset += WasmStruct::kHeaderSize;
      TaggedField<Object>::store(result, offset, *args[i].to_ref());
    }
  }
  return handle(result, isolate());
}

Handle<WasmContinuationObject> Factory::NewWasmContinuationObject(
    Address jmpbuf, Handle<Foreign> managed_stack, Handle<HeapObject> parent,
    AllocationType allocation) {
  Map map = *wasm_continuation_object_map();
  auto result = WasmContinuationObject::cast(
      AllocateRawWithImmortalMap(map.instance_size(), allocation, map));
  result.init_jmpbuf(isolate(), jmpbuf);
  result.set_stack(*managed_stack);
  result.set_parent(*parent);
  return handle(result, isolate());
}

Handle<SharedFunctionInfo>
Factory::NewSharedFunctionInfoForWasmExportedFunction(
    Handle<String> name, Handle<WasmExportedFunctionData> data) {
  return NewSharedFunctionInfo(name, data, Builtin::kNoBuiltinId);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForWasmJSFunction(
    Handle<String> name, Handle<WasmJSFunctionData> data) {
  return NewSharedFunctionInfo(name, data, Builtin::kNoBuiltinId);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForWasmResume(
    Handle<WasmResumeData> data) {
  return NewSharedFunctionInfo({}, data, Builtin::kNoBuiltinId);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForWasmCapiFunction(
    Handle<WasmCapiFunctionData> data) {
  return NewSharedFunctionInfo(MaybeHandle<String>(), data,
                               Builtin::kNoBuiltinId,
                               FunctionKind::kConciseMethod);
}
#endif  // V8_ENABLE_WEBASSEMBLY

Handle<Cell> Factory::NewCell(Smi value) {
  static_assert(Cell::kSize <= kMaxRegularHeapObjectSize);
  Cell result = Cell::cast(AllocateRawWithImmortalMap(
      Cell::kSize, AllocationType::kOld, *cell_map()));
  DisallowGarbageCollection no_gc;
  result.set_value(value, WriteBarrierMode::SKIP_WRITE_BARRIER);
  return handle(result, isolate());
}

Handle<Cell> Factory::NewCell() {
  static_assert(Cell::kSize <= kMaxRegularHeapObjectSize);
  Cell result = Cell::cast(AllocateRawWithImmortalMap(
      Cell::kSize, AllocationType::kOld, *cell_map()));
  result.set_value(read_only_roots().undefined_value(),
                   WriteBarrierMode::SKIP_WRITE_BARRIER);
  return handle(result, isolate());
}

Handle<FeedbackCell> Factory::NewNoClosuresCell(Handle<HeapObject> value) {
  FeedbackCell result = FeedbackCell::cast(AllocateRawWithImmortalMap(
      FeedbackCell::kAlignedSize, AllocationType::kOld,
      *no_closures_cell_map()));
  DisallowGarbageCollection no_gc;
  result.set_value(*value);
  result.SetInitialInterruptBudget();
  result.clear_padding();
  return handle(result, isolate());
}

Handle<FeedbackCell> Factory::NewOneClosureCell(Handle<HeapObject> value) {
  FeedbackCell result = FeedbackCell::cast(AllocateRawWithImmortalMap(
      FeedbackCell::kAlignedSize, AllocationType::kOld,
      *one_closure_cell_map()));
  DisallowGarbageCollection no_gc;
  result.set_value(*value);
  result.SetInitialInterruptBudget();
  result.clear_padding();
  return handle(result, isolate());
}

Handle<FeedbackCell> Factory::NewManyClosuresCell(Handle<HeapObject> value) {
  FeedbackCell result = FeedbackCell::cast(AllocateRawWithImmortalMap(
      FeedbackCell::kAlignedSize, AllocationType::kOld,
      *many_closures_cell_map()));
  DisallowGarbageCollection no_gc;
  result.set_value(*value);
  result.SetInitialInterruptBudget();
  result.clear_padding();
  return handle(result, isolate());
}

Handle<PropertyCell> Factory::NewPropertyCell(Handle<Name> name,
                                              PropertyDetails details,
                                              Handle<Object> value,
                                              AllocationType allocation) {
  DCHECK(name->IsUniqueName());
  static_assert(PropertyCell::kSize <= kMaxRegularHeapObjectSize);
  PropertyCell cell = PropertyCell::cast(AllocateRawWithImmortalMap(
      PropertyCell::kSize, allocation, *global_property_cell_map()));
  DisallowGarbageCollection no_gc;
  cell.set_dependent_code(
      DependentCode::empty_dependent_code(ReadOnlyRoots(isolate())),
      SKIP_WRITE_BARRIER);
  WriteBarrierMode mode = allocation == AllocationType::kYoung
                              ? SKIP_WRITE_BARRIER
                              : UPDATE_WRITE_BARRIER;
  cell.set_name(*name, mode);
  cell.set_value(*value, mode);
  cell.set_property_details_raw(details.AsSmi(), SKIP_WRITE_BARRIER);
  return handle(cell, isolate());
}

Handle<PropertyCell> Factory::NewProtector() {
  return NewPropertyCell(
      empty_string(), PropertyDetails::Empty(PropertyCellType::kConstantType),
      handle(Smi::FromInt(Protectors::kProtectorValid), isolate()));
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
                            ElementsKind elements_kind, int inobject_properties,
                            AllocationType allocation_type) {
  static_assert(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  DCHECK(!InstanceTypeChecker::MayHaveMapCheckFastCase(type));
  DCHECK_IMPLIES(InstanceTypeChecker::IsJSObject(type) &&
                     !Map::CanHaveFastTransitionableElementsKind(type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind) ||
                     IsSharedArrayElementsKind(elements_kind));
  DCHECK(allocation_type == AllocationType::kMap ||
         allocation_type == AllocationType::kSharedMap);
  HeapObject result = allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
      Map::kSize, allocation_type);
  DisallowGarbageCollection no_gc;
  Heap* roots = allocation_type == AllocationType::kMap
                    ? isolate()->heap()
                    : isolate()->shared_space_isolate()->heap();
  result.set_map_after_allocation(ReadOnlyRoots(roots).meta_map(),
                                  SKIP_WRITE_BARRIER);
#if V8_STATIC_ROOTS_BOOL
  CHECK_IMPLIES(InstanceTypeChecker::IsJSReceiver(type),
                V8HeapCompressionScheme::CompressObject(result.ptr()) >
                    InstanceTypeChecker::kNonJsReceiverMapLimit);
#endif
  return handle(InitializeMap(Map::cast(result), type, instance_size,
                              elements_kind, inobject_properties, roots),
                isolate());
}

Map Factory::InitializeMap(Map map, InstanceType type, int instance_size,
                           ElementsKind elements_kind, int inobject_properties,
                           Heap* roots) {
  DisallowGarbageCollection no_gc;
  map.set_bit_field(0);
  map.set_bit_field2(Map::Bits2::NewTargetIsBaseBit::encode(true));
  int bit_field3 =
      Map::Bits3::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
      Map::Bits3::OwnsDescriptorsBit::encode(true) |
      Map::Bits3::ConstructionCounterBits::encode(Map::kNoSlackTracking) |
      Map::Bits3::IsExtensibleBit::encode(true);
  map.set_bit_field3(bit_field3);
  map.set_instance_type(type);
  ReadOnlyRoots ro_roots(roots);
  map.init_prototype_and_constructor_or_back_pointer(ro_roots);
  map.set_instance_size(instance_size);
  if (map.IsJSObjectMap()) {
    DCHECK(!ReadOnlyHeap::Contains(map));
    map.SetInObjectPropertiesStartInWords(instance_size / kTaggedSize -
                                          inobject_properties);
    DCHECK_EQ(map.GetInObjectProperties(), inobject_properties);
    map.set_prototype_validity_cell(ro_roots.invalid_prototype_validity_cell(),
                                    kRelaxedStore);
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map.set_inobject_properties_start_or_constructor_function_index(0);
    map.set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid),
                                    kRelaxedStore, SKIP_WRITE_BARRIER);
  }
  map.set_dependent_code(DependentCode::empty_dependent_code(ro_roots),
                         SKIP_WRITE_BARRIER);
  map.set_raw_transitions(MaybeObject::FromSmi(Smi::zero()),
                          SKIP_WRITE_BARRIER);
  map.SetInObjectUnusedPropertyFields(inobject_properties);
  map.SetInstanceDescriptors(isolate(), ro_roots.empty_descriptor_array(), 0);
  // Must be called only after |instance_type| and |instance_size| are set.
  map.set_visitor_id(Map::GetVisitorId(map));
  DCHECK(!map.is_in_retained_map_list());
  map.clear_padding();
  map.set_elements_kind(elements_kind);
  if (v8_flags.log_maps) LOG(isolate(), MapCreate(map));
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
  InstanceType instance_type = map->instance_type();
  bool is_clonable_js_type =
      instance_type == JS_REG_EXP_TYPE || instance_type == JS_OBJECT_TYPE ||
      instance_type == JS_ERROR_TYPE || instance_type == JS_ARRAY_TYPE ||
      instance_type == JS_SPECIAL_API_OBJECT_TYPE ||
      InstanceTypeChecker::IsJSApiObject(instance_type);
  bool is_clonable_wasm_type = false;
#if V8_ENABLE_WEBASSEMBLY
  is_clonable_wasm_type = instance_type == WASM_GLOBAL_OBJECT_TYPE ||
                          instance_type == WASM_INSTANCE_OBJECT_TYPE ||
                          instance_type == WASM_MEMORY_OBJECT_TYPE ||
                          instance_type == WASM_MODULE_OBJECT_TYPE ||
                          instance_type == WASM_TABLE_OBJECT_TYPE;
#endif  // V8_ENABLE_WEBASSEMBLY
  CHECK(is_clonable_js_type || is_clonable_wasm_type);

  DCHECK(site.is_null() || AllocationSite::CanTrack(instance_type));

  int object_size = map->instance_size();
  int aligned_object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  int adjusted_object_size = aligned_object_size;
  if (!site.is_null()) {
    DCHECK(V8_ALLOCATION_SITE_TRACKING_BOOL);
    adjusted_object_size +=
        ALIGN_TO_ALLOCATION_ALIGNMENT(AllocationMemento::kSize);
  }
  HeapObject raw_clone =
      allocator()->AllocateRawWith<HeapAllocator::kRetryOrFail>(
          adjusted_object_size, AllocationType::kYoung);

  DCHECK(Heap::InYoungGeneration(raw_clone) || v8_flags.single_generation);

  Heap::CopyBlock(raw_clone.address(), source->address(), object_size);
  Handle<JSObject> clone(JSObject::cast(raw_clone), isolate());

  if (v8_flags.enable_unconditional_write_barriers) {
    // By default, we shouldn't need to update the write barrier here, as the
    // clone will be allocated in new space.
    const ObjectSlot start(raw_clone.address());
    const ObjectSlot end(raw_clone.address() + object_size);
    isolate()->heap()->WriteBarrierForRange(raw_clone, start, end);
  }
  if (!site.is_null()) {
    AllocationMemento alloc_memento = AllocationMemento::unchecked_cast(
        Object(raw_clone.ptr() + aligned_object_size));
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
      clone->set_raw_properties_or_hash(*prop, kRelaxedStore);
    }
  } else {
    Handle<Object> copied_properties;
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      copied_properties = SwissNameDictionary::ShallowCopy(
          isolate(), handle(source->property_dictionary_swiss(), isolate()));
    } else {
      copied_properties =
          CopyFixedArray(handle(source->property_dictionary(), isolate()));
    }
    clone->set_raw_properties_or_hash(*copied_properties, kRelaxedStore);
  }
  return clone;
}

namespace {
template <typename T>
void initialize_length(T array, int length) {
  array.set_length(length);
}

template <>
void initialize_length<PropertyArray>(PropertyArray array, int length) {
  array.initialize_length(length);
}

inline void InitEmbedderFields(i::JSObject obj, i::Object initial_value) {
  for (int i = 0; i < obj.GetEmbedderFieldCount(); i++) {
    EmbedderDataSlot(obj, i).Initialize(initial_value);
  }
}

}  // namespace

template <typename T>
Handle<T> Factory::CopyArrayWithMap(Handle<T> src, Handle<Map> map) {
  int len = src->length();
  HeapObject new_object = AllocateRawFixedArray(len, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  new_object.set_map_after_allocation(*map, SKIP_WRITE_BARRIER);
  T result = T::cast(new_object);
  initialize_length(result, len);
  // Copy the content.
  WriteBarrierMode mode = result.GetWriteBarrierMode(no_gc);
  result.CopyElements(isolate(), 0, *src, 0, len, mode);
  return handle(result, isolate());
}

template <typename T>
Handle<T> Factory::CopyArrayAndGrow(Handle<T> src, int grow_by,
                                    AllocationType allocation) {
  DCHECK_LT(0, grow_by);
  DCHECK_LE(grow_by, kMaxInt - src->length());
  int old_len = src->length();
  int new_len = old_len + grow_by;
  HeapObject new_object = AllocateRawFixedArray(new_len, allocation);
  DisallowGarbageCollection no_gc;
  new_object.set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);
  T result = T::cast(new_object);
  initialize_length(result, new_len);
  // Copy the content.
  WriteBarrierMode mode = result.GetWriteBarrierMode(no_gc);
  result.CopyElements(isolate(), 0, *src, 0, old_len, mode);
  MemsetTagged(ObjectSlot(result.data_start() + old_len),
               read_only_roots().undefined_value(), grow_by);
  return handle(result, isolate());
}

Handle<FixedArray> Factory::CopyFixedArrayWithMap(Handle<FixedArray> array,
                                                  Handle<Map> map) {
  return CopyArrayWithMap(array, map);
}

Handle<WeakArrayList> Factory::NewUninitializedWeakArrayList(
    int capacity, AllocationType allocation) {
  DCHECK_LE(0, capacity);
  if (capacity == 0) return empty_weak_array_list();

  HeapObject heap_object = AllocateRawWeakArrayList(capacity, allocation);
  DisallowGarbageCollection no_gc;
  heap_object.set_map_after_allocation(*weak_array_list_map(),
                                       SKIP_WRITE_BARRIER);
  WeakArrayList result = WeakArrayList::cast(heap_object);
  result.set_length(0);
  result.set_capacity(capacity);
  return handle(result, isolate());
}

Handle<WeakArrayList> Factory::NewWeakArrayList(int capacity,
                                                AllocationType allocation) {
  Handle<WeakArrayList> result =
      NewUninitializedWeakArrayList(capacity, allocation);
  MemsetTagged(ObjectSlot(result->data_start()),
               read_only_roots().undefined_value(), capacity);
  return result;
}

Handle<FixedArray> Factory::CopyFixedArrayAndGrow(Handle<FixedArray> array,
                                                  int grow_by,
                                                  AllocationType allocation) {
  return CopyArrayAndGrow(array, grow_by, allocation);
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
  DisallowGarbageCollection no_gc;
  WeakArrayList raw = *result;
  int old_len = src->length();
  raw.set_length(old_len);
  // Copy the content.
  WriteBarrierMode mode = raw.GetWriteBarrierMode(no_gc);
  raw.CopyElements(isolate(), 0, *src, 0, old_len, mode);
  MemsetTagged(ObjectSlot(raw.data_start() + old_len),
               read_only_roots().undefined_value(), new_capacity - old_len);
  return result;
}

Handle<WeakArrayList> Factory::CompactWeakArrayList(Handle<WeakArrayList> src,
                                                    int new_capacity,
                                                    AllocationType allocation) {
  Handle<WeakArrayList> result =
      NewUninitializedWeakArrayList(new_capacity, allocation);

  // Copy the content.
  DisallowGarbageCollection no_gc;
  WeakArrayList raw_src = *src;
  WeakArrayList raw_result = *result;
  WriteBarrierMode mode = raw_result.GetWriteBarrierMode(no_gc);
  int copy_to = 0, length = raw_src.length();
  for (int i = 0; i < length; i++) {
    MaybeObject element = raw_src.Get(i);
    if (element->IsCleared()) continue;
    raw_result.Set(copy_to++, element, mode);
  }
  raw_result.set_length(copy_to);

  MemsetTagged(ObjectSlot(raw_result.data_start() + copy_to),
               read_only_roots().undefined_value(), new_capacity - copy_to);
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
  HeapObject heap_object = AllocateRawFixedArray(new_len, allocation);
  DisallowGarbageCollection no_gc;
  heap_object.set_map_after_allocation(*fixed_array_map(), SKIP_WRITE_BARRIER);
  FixedArray result = FixedArray::cast(heap_object);
  result.set_length(new_len);
  // Copy the content.
  WriteBarrierMode mode = result.GetWriteBarrierMode(no_gc);
  result.CopyElements(isolate(), 0, *array, 0, new_len, mode);
  return handle(result, isolate());
}

Handle<FixedArray> Factory::CopyFixedArray(Handle<FixedArray> array) {
  if (array->length() == 0) return array;
  return CopyArrayWithMap(array, handle(array->map(), isolate()));
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
  ReadOnlyRoots roots(isolate());
  auto num = roots.FindHeapNumber(value);
  if (!num.is_null()) return num;
  // Add known HeapNumber constants to the read only roots. This ensures
  // r/o snapshots to be deterministic.
  DCHECK(!CanAllocateInReadOnlySpace());
  return NewHeapNumber<AllocationType::kOld>(value);
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
                               undefined_value(), SKIP_NONE, no_caller,
                               ErrorUtils::StackTraceCollection::kEnabled)
      .ToHandleChecked();
}

Handle<Object> Factory::NewInvalidStringLengthError() {
  if (v8_flags.correctness_fuzzer_suppressions) {
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
  Handle<NativeContext> native_context(function->native_context(), isolate());
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
  auto external =
      Handle<JSExternalObject>::cast(NewJSObjectFromMap(external_map()));
  external->init_value(isolate(), value);
  return external;
}

Handle<DeoptimizationLiteralArray> Factory::NewDeoptimizationLiteralArray(
    int length) {
  return Handle<DeoptimizationLiteralArray>::cast(
      NewWeakFixedArray(length, AllocationType::kOld));
}

Handle<Code> Factory::NewOffHeapTrampolineFor(Handle<Code> code,
                                              Address off_heap_entry) {
  CHECK_NOT_NULL(isolate()->embedded_blob_code());
  CHECK_NE(0, isolate()->embedded_blob_code_size());
  CHECK(Builtins::IsIsolateIndependentBuiltin(*code));

#if !defined(V8_SHORT_BUILTIN_CALLS) || \
    defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
  // Builtins have a single unique shared entry point per process. The
  // embedded builtins region may be remapped into the process-wide code
  // range, but that happens before RO space is deserialized. Their Code
  // objects can be shared in RO space.
  const AllocationType allocation_type = AllocationType::kReadOnly;
#else
  // Builtins may be remapped more than once per process and thus their
  // Code objects cannot be shared.
  const AllocationType allocation_type = AllocationType::kOld;
#endif  // !defined(V8_SHORT_BUILTIN_CALLS) ||
        // defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)

  NewCodeOptions new_code_options = {
      /*kind=*/code->kind(),
      /*builtin=*/code->builtin_id(),
      /*is_turbofanned=*/code->is_turbofanned(),
      /*stack_slots=*/code->stack_slots(),
      /*kind_specific_flags=*/code->kind_specific_flags(kRelaxedLoad),
      /*allocation=*/allocation_type,
      /*instruction_size=*/code->instruction_size(),
      /*metadata_size=*/code->metadata_size(),
      /*inlined_bytecode_size=*/code->inlined_bytecode_size(),
      /*osr_offset=*/code->osr_offset(),
      /*handler_table_offset=*/code->handler_table_offset(),
      /*constant_pool_offset=*/code->constant_pool_offset(),
      /*code_comments_offset=*/code->code_comments_offset(),
      /*unwinding_info_offset=*/code->unwinding_info_offset(),
      /*reloc_info=*/
      Handle<ByteArray>(read_only_roots().empty_byte_array(), isolate()),
      /*bytecode_or_deoptimization_data=*/
      Handle<FixedArray>(read_only_roots().empty_fixed_array(), isolate()),
      /*bytecode_offsets_or_source_position_table=*/
      Handle<ByteArray>(read_only_roots().empty_byte_array(), isolate())};

  Handle<Code> off_heap_trampoline = NewCode(new_code_options);
  off_heap_trampoline->set_code_entry_point(isolate(),
                                            code->code_entry_point());

  DCHECK_EQ(code->instruction_size(), code->OffHeapInstructionSize());
  DCHECK_EQ(code->metadata_size(), code->OffHeapMetadataSize());
  DCHECK_EQ(code->inlined_bytecode_size(), 0);
  DCHECK_EQ(code->osr_offset(), BytecodeOffset::None());

  return off_heap_trampoline;
}

Handle<BytecodeArray> Factory::CopyBytecodeArray(Handle<BytecodeArray> source) {
  int size = BytecodeArray::SizeFor(source->length());
  BytecodeArray copy = BytecodeArray::cast(AllocateRawWithImmortalMap(
      size, AllocationType::kOld, *bytecode_array_map()));
  DisallowGarbageCollection no_gc;
  BytecodeArray raw_source = *source;
  copy.set_length(raw_source.length());
  copy.set_frame_size(raw_source.frame_size());
  copy.set_parameter_count(raw_source.parameter_count());
  copy.set_incoming_new_target_or_generator_register(
      raw_source.incoming_new_target_or_generator_register());
  copy.set_constant_pool(raw_source.constant_pool());
  copy.set_handler_table(raw_source.handler_table());
  copy.set_source_position_table(raw_source.source_position_table(kAcquireLoad),
                                 kReleaseStore);
  copy.set_bytecode_age(raw_source.bytecode_age());
  raw_source.CopyBytecodesTo(copy);
  return handle(copy, isolate());
}

Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor,
                                      AllocationType allocation) {
  JSFunction::EnsureHasInitialMap(constructor);
  Handle<Map> map(constructor->initial_map(), isolate());
  return NewJSObjectFromMap(map, allocation);
}

Handle<JSObject> Factory::NewSlowJSObjectWithNullProto() {
  Handle<JSObject> result =
      NewSlowJSObjectFromMap(isolate()->slow_object_with_null_prototype_map());
  return result;
}

Handle<JSObject> Factory::NewJSObjectWithNullProto() {
  Handle<Map> map(isolate()->object_function()->initial_map(), isolate());
  Handle<Map> map_with_null_proto =
      Map::TransitionToPrototype(isolate(), map, null_value());
  return NewJSObjectFromMap(map_with_null_proto);
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
  Handle<DescriptorArray> descs(map->instance_descriptors(isolate()),
                                isolate());
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details = descs->GetDetails(i);
    // Only accessors are expected.
    DCHECK_EQ(PropertyKind::kAccessor, details.kind());
    PropertyDetails d(PropertyKind::kAccessor, details.attributes(),
                      PropertyCellType::kMutable);
    Handle<Name> name(descs->GetKey(i), isolate());
    Handle<Object> value(descs->GetStrongValue(i), isolate());
    Handle<PropertyCell> cell = NewPropertyCell(name, d, value);
    // |dictionary| already contains enough space for all properties.
    USE(GlobalDictionary::Add(isolate(), dictionary, name, cell, d));
  }

  // Allocate the global object and initialize it with the backing store.
  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(New(map, AllocationType::kOld)), isolate());
  InitializeJSObjectFromMap(*global, *dictionary, *map);

  // Create a new map for the global object.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate(), map);
  Map raw_map = *new_map;
  raw_map.set_may_have_interesting_symbols(true);
  raw_map.set_is_dictionary_map(true);
  LOG(isolate(), MapDetails(raw_map));

  // Set up the global object as a normalized object.
  global->set_global_dictionary(*dictionary, kReleaseStore);
  global->set_map(raw_map, kReleaseStore);

  // Make sure result is a global object with properties in dictionary.
  DCHECK(global->IsJSGlobalObject() && !global->HasFastProperties());
  return global;
}

void Factory::InitializeJSObjectFromMap(JSObject obj, Object properties,
                                        Map map) {
  DisallowGarbageCollection no_gc;
  obj.set_raw_properties_or_hash(properties, kRelaxedStore);
  obj.initialize_elements();
  // TODO(1240798): Initialize the object's body using valid initial values
  // according to the object's initial map.  For example, if the map's
  // instance type is JS_ARRAY_TYPE, the length field should be initialized
  // to a number (e.g. Smi::zero()) and the elements initialized to a
  // fixed array (e.g. Heap::empty_fixed_array()).  Currently, the object
  // verification code has to cope with (temporarily) invalid objects.  See
  // for example, JSArray::JSArrayVerify).
  InitializeJSObjectBody(obj, map, JSObject::kHeaderSize);
}

void Factory::InitializeJSObjectBody(JSObject obj, Map map, int start_offset) {
  DisallowGarbageCollection no_gc;
  if (start_offset == map.instance_size()) return;
  DCHECK_LT(start_offset, map.instance_size());

  // We cannot always fill with one_pointer_filler_map because objects
  // created from API functions expect their embedder fields to be initialized
  // with undefined_value.
  // Pre-allocated fields need to be initialized with undefined_value as well
  // so that object accesses before the constructor completes (e.g. in the
  // debugger) will not cause a crash.

  // In case of Array subclassing the |map| could already be transitioned
  // to different elements kind from the initial map on which we track slack.
  bool in_progress = map.IsInobjectSlackTrackingInProgress();
  obj.InitializeBody(map, start_offset, in_progress,
                     ReadOnlyRoots(isolate()).one_pointer_filler_map_word(),
                     *undefined_value());
  if (in_progress) {
    map.FindRootMap(isolate()).InobjectSlackTrackingStep(isolate());
  }
}

Handle<JSObject> Factory::NewJSObjectFromMap(
    Handle<Map> map, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  DCHECK(!InstanceTypeChecker::IsJSFunction(*map));

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  DCHECK(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);

  JSObject js_obj = JSObject::cast(
      AllocateRawWithAllocationSite(map, allocation, allocation_site));

  InitializeJSObjectFromMap(js_obj, *empty_fixed_array(), *map);

  DCHECK(js_obj.HasFastElements() ||
         (isolate()->bootstrapper()->IsActive() ||
          *map == isolate()
                      ->raw_native_context()
                      .js_array_template_literal_object_map()) ||
         js_obj.HasTypedArrayOrRabGsabTypedArrayElements() ||
         js_obj.HasFastStringWrapperElements() ||
         js_obj.HasFastArgumentsElements() || js_obj.HasDictionaryElements() ||
         js_obj.HasSharedArrayElements());
  return handle(js_obj, isolate());
}

Handle<JSObject> Factory::NewSlowJSObjectFromMap(
    Handle<Map> map, int capacity, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  DCHECK(map->is_dictionary_map());
  Handle<HeapObject> object_properties;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    object_properties = NewSwissNameDictionary(capacity, allocation);
  } else {
    object_properties = NameDictionary::New(isolate(), capacity);
  }
  Handle<JSObject> js_object =
      NewJSObjectFromMap(map, allocation, allocation_site);
  js_object->set_raw_properties_or_hash(*object_properties, kRelaxedStore);
  return js_object;
}

Handle<JSObject> Factory::NewSlowJSObjectWithPropertiesAndElements(
    Handle<HeapObject> prototype, Handle<HeapObject> properties,
    Handle<FixedArrayBase> elements) {
  DCHECK_IMPLIES(V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL,
                 properties->IsSwissNameDictionary());
  DCHECK_IMPLIES(!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL,
                 properties->IsNameDictionary());

  Handle<Map> object_map = isolate()->slow_object_with_object_prototype_map();
  if (object_map->prototype() != *prototype) {
    object_map = Map::TransitionToPrototype(isolate(), object_map, prototype);
  }
  DCHECK(object_map->is_dictionary_map());
  Handle<JSObject> object =
      NewJSObjectFromMap(object_map, AllocationType::kYoung);
  object->set_raw_properties_or_hash(*properties);
  if (*elements != read_only_roots().empty_fixed_array()) {
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
#ifdef ENABLE_SLOW_DCHECKS
  JSObject::ValidateElements(*array);
#endif
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
  return NewJSArrayWithUnverifiedElements(handle(map, isolate()), elements,
                                          length, allocation);
}

Handle<JSArray> Factory::NewJSArrayWithUnverifiedElements(
    Handle<Map> map, Handle<FixedArrayBase> elements, int length,
    AllocationType allocation) {
  auto array = Handle<JSArray>::cast(NewJSObjectFromMap(map, allocation));
  DisallowGarbageCollection no_gc;
  JSArray raw = *array;
  raw.set_elements(*elements);
  raw.set_length(Smi::FromInt(length));
  return array;
}

Handle<JSArray> Factory::NewJSArrayForTemplateLiteralArray(
    Handle<FixedArray> cooked_strings, Handle<FixedArray> raw_strings,
    int function_literal_id, int slot_id) {
  Handle<JSArray> raw_object =
      NewJSArrayWithElements(raw_strings, PACKED_ELEMENTS,
                             raw_strings->length(), AllocationType::kOld);
  JSObject::SetIntegrityLevel(isolate(), raw_object, FROZEN, kThrowOnError)
      .ToChecked();

  Handle<NativeContext> native_context = isolate()->native_context();
  Handle<TemplateLiteralObject> template_object =
      Handle<TemplateLiteralObject>::cast(NewJSArrayWithUnverifiedElements(
          handle(native_context->js_array_template_literal_object_map(),
                 isolate()),
          cooked_strings, cooked_strings->length(), AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  TemplateLiteralObject raw_template_object = *template_object;
  DCHECK_EQ(raw_template_object.map(),
            native_context->js_array_template_literal_object_map());
  raw_template_object.set_raw(*raw_object);
  raw_template_object.set_function_literal_id(function_literal_id);
  raw_template_object.set_slot_id(slot_id);
  return template_object;
}

void Factory::NewJSArrayStorage(Handle<JSArray> array, int length, int capacity,
                                ArrayStorageAllocationMode mode) {
  DCHECK(capacity >= length);

  if (capacity == 0) {
    JSArray raw = *array;
    DisallowGarbageCollection no_gc;
    raw.set_length(Smi::zero());
    raw.set_elements(*empty_fixed_array());
    return;
  }

  HandleScope inner_scope(isolate());
  Handle<FixedArrayBase> elms =
      NewJSArrayStorage(array->GetElementsKind(), capacity, mode);
  DisallowGarbageCollection no_gc;
  JSArray raw = *array;
  raw.set_elements(*elms);
  raw.set_length(Smi::FromInt(length));
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
      elms = NewFixedArray(capacity);
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
  module_namespace->FastPropertyAtPut(index, read_only_roots().Module_string(),
                                      SKIP_WRITE_BARRIER);
  return module_namespace;
}

Handle<JSWrappedFunction> Factory::NewJSWrappedFunction(
    Handle<NativeContext> creation_context, Handle<Object> target) {
  DCHECK(target->IsCallable());
  Handle<Map> map(
      Map::cast(creation_context->get(Context::WRAPPED_FUNCTION_MAP_INDEX)),
      isolate());
  // 2. Let wrapped be ! MakeBasicObject(internalSlotsList).
  // 3. Set wrapped.[[Prototype]] to
  // callerRealm.[[Intrinsics]].[[%Function.prototype%]].
  // 4. Set wrapped.[[Call]] as described in 2.1.
  Handle<JSWrappedFunction> wrapped = Handle<JSWrappedFunction>::cast(
      isolate()->factory()->NewJSObjectFromMap(map));
  // 5. Set wrapped.[[WrappedTargetFunction]] to Target.
  wrapped->set_wrapped_target_function(JSReceiver::cast(*target));
  // 6. Set wrapped.[[Realm]] to callerRealm.
  wrapped->set_context(*creation_context);
  // TODO(v8:11989): https://github.com/tc39/proposal-shadowrealm/pull/348

  return wrapped;
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

  ReadOnlyRoots roots(isolate());
  SourceTextModule module = SourceTextModule::cast(
      New(source_text_module_map(), AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  module.set_code(*sfi);
  module.set_exports(*exports);
  module.set_regular_exports(*regular_exports);
  module.set_regular_imports(*regular_imports);
  module.set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module.set_module_namespace(roots.undefined_value(), SKIP_WRITE_BARRIER);
  module.set_requested_modules(*requested_modules);
  module.set_status(Module::kUnlinked);
  module.set_exception(roots.the_hole_value(), SKIP_WRITE_BARRIER);
  module.set_top_level_capability(roots.undefined_value(), SKIP_WRITE_BARRIER);
  module.set_import_meta(roots.the_hole_value(), kReleaseStore,
                         SKIP_WRITE_BARRIER);
  module.set_dfs_index(-1);
  module.set_dfs_ancestor_index(-1);
  module.set_flags(0);
  module.set_async(IsAsyncModule(sfi->kind()));
  module.set_async_evaluating_ordinal(SourceTextModule::kNotAsyncEvaluated);
  module.set_cycle_root(roots.the_hole_value(), SKIP_WRITE_BARRIER);
  module.set_async_parent_modules(roots.empty_array_list());
  module.set_pending_async_dependencies(0);
  return handle(module, isolate());
}

Handle<SyntheticModule> Factory::NewSyntheticModule(
    Handle<String> module_name, Handle<FixedArray> export_names,
    v8::Module::SyntheticModuleEvaluationSteps evaluation_steps) {
  ReadOnlyRoots roots(isolate());

  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), static_cast<int>(export_names->length()));
  Handle<Foreign> evaluation_steps_foreign =
      NewForeign(reinterpret_cast<i::Address>(evaluation_steps));

  SyntheticModule module =
      SyntheticModule::cast(New(synthetic_module_map(), AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  module.set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module.set_module_namespace(roots.undefined_value(), SKIP_WRITE_BARRIER);
  module.set_status(Module::kUnlinked);
  module.set_exception(roots.the_hole_value(), SKIP_WRITE_BARRIER);
  module.set_top_level_capability(roots.undefined_value(), SKIP_WRITE_BARRIER);
  module.set_name(*module_name);
  module.set_export_names(*export_names);
  module.set_exports(*exports);
  module.set_evaluation_steps(*evaluation_steps_foreign);
  return handle(module, isolate());
}

Handle<JSArrayBuffer> Factory::NewJSArrayBuffer(
    std::shared_ptr<BackingStore> backing_store, AllocationType allocation) {
  Handle<Map> map(isolate()->native_context()->array_buffer_fun().initial_map(),
                  isolate());
  ResizableFlag resizable_by_js = ResizableFlag::kNotResizable;
  if (v8_flags.harmony_rab_gsab && backing_store->is_resizable_by_js()) {
    resizable_by_js = ResizableFlag::kResizable;
  }
  auto result =
      Handle<JSArrayBuffer>::cast(NewJSObjectFromMap(map, allocation));
  result->Setup(SharedFlag::kNotShared, resizable_by_js,
                std::move(backing_store), isolate());
  return result;
}

MaybeHandle<JSArrayBuffer> Factory::NewJSArrayBufferAndBackingStore(
    size_t byte_length, InitializedFlag initialized,
    AllocationType allocation) {
  return NewJSArrayBufferAndBackingStore(byte_length, byte_length, initialized,
                                         ResizableFlag::kNotResizable,
                                         allocation);
}

MaybeHandle<JSArrayBuffer> Factory::NewJSArrayBufferAndBackingStore(
    size_t byte_length, size_t max_byte_length, InitializedFlag initialized,
    ResizableFlag resizable, AllocationType allocation) {
  DCHECK_LE(byte_length, max_byte_length);
  std::unique_ptr<BackingStore> backing_store = nullptr;

  if (resizable == ResizableFlag::kResizable) {
    size_t page_size, initial_pages, max_pages;
    if (JSArrayBuffer::GetResizableBackingStorePageConfiguration(
            isolate(), byte_length, max_byte_length, kDontThrow, &page_size,
            &initial_pages, &max_pages)
            .IsNothing()) {
      return MaybeHandle<JSArrayBuffer>();
    }

    backing_store = BackingStore::TryAllocateAndPartiallyCommitMemory(
        isolate(), byte_length, max_byte_length, page_size, initial_pages,
        max_pages, WasmMemoryFlag::kNotWasm, SharedFlag::kNotShared);
    if (!backing_store) return MaybeHandle<JSArrayBuffer>();
  } else {
    if (byte_length > 0) {
      backing_store = BackingStore::Allocate(
          isolate(), byte_length, SharedFlag::kNotShared, initialized);
      if (!backing_store) return MaybeHandle<JSArrayBuffer>();
    }
  }
  Handle<Map> map(isolate()->native_context()->array_buffer_fun().initial_map(),
                  isolate());
  auto array_buffer =
      Handle<JSArrayBuffer>::cast(NewJSObjectFromMap(map, allocation));
  array_buffer->Setup(SharedFlag::kNotShared, resizable,
                      std::move(backing_store), isolate());
  return array_buffer;
}

Handle<JSArrayBuffer> Factory::NewJSSharedArrayBuffer(
    std::shared_ptr<BackingStore> backing_store) {
  DCHECK_IMPLIES(backing_store->is_resizable_by_js(),
                 v8_flags.harmony_rab_gsab);
  Handle<Map> map(
      isolate()->native_context()->shared_array_buffer_fun().initial_map(),
      isolate());
  auto result = Handle<JSArrayBuffer>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  ResizableFlag resizable = backing_store->is_resizable_by_js()
                                ? ResizableFlag::kResizable
                                : ResizableFlag::kNotResizable;
  result->Setup(SharedFlag::kShared, resizable, std::move(backing_store),
                isolate());
  return result;
}

Handle<JSIteratorResult> Factory::NewJSIteratorResult(Handle<Object> value,
                                                      bool done) {
  Handle<Map> map(isolate()->native_context()->iterator_result_map(),
                  isolate());
  Handle<JSIteratorResult> js_iter_result = Handle<JSIteratorResult>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  JSIteratorResult raw = *js_iter_result;
  raw.set_value(*value, SKIP_WRITE_BARRIER);
  raw.set_done(*ToBoolean(done), SKIP_WRITE_BARRIER);
  return js_iter_result;
}

Handle<JSAsyncFromSyncIterator> Factory::NewJSAsyncFromSyncIterator(
    Handle<JSReceiver> sync_iterator, Handle<Object> next) {
  Handle<Map> map(isolate()->native_context()->async_from_sync_iterator_map(),
                  isolate());
  Handle<JSAsyncFromSyncIterator> iterator =
      Handle<JSAsyncFromSyncIterator>::cast(
          NewJSObjectFromMap(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  JSAsyncFromSyncIterator raw = *iterator;
  raw.set_sync_iterator(*sync_iterator, SKIP_WRITE_BARRIER);
  raw.set_next(*next, SKIP_WRITE_BARRIER);
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
    RAB_GSAB_TYPED_ARRAYS_WITH_TYPED_ARRAY_TYPE(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      UNREACHABLE();
  }
}

Handle<JSArrayBufferView> Factory::NewJSArrayBufferView(
    Handle<Map> map, Handle<FixedArrayBase> elements,
    Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length) {
  if (!IsRabGsabTypedArrayElementsKind(map->elements_kind())) {
    CHECK_LE(byte_length, buffer->GetByteLength());
    CHECK_LE(byte_offset, buffer->GetByteLength());
    CHECK_LE(byte_offset + byte_length, buffer->GetByteLength());
  }

  Handle<JSArrayBufferView> array_buffer_view = Handle<JSArrayBufferView>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  JSArrayBufferView raw = *array_buffer_view;
  raw.set_elements(*elements, SKIP_WRITE_BARRIER);
  raw.set_buffer(*buffer, SKIP_WRITE_BARRIER);
  raw.set_byte_offset(byte_offset);
  raw.set_byte_length(byte_length);
  raw.set_bit_field(0);
  // TODO(v8) remove once embedder data slots are always zero-initialized.
  InitEmbedderFields(raw, Smi::zero());
  DCHECK_EQ(raw.GetEmbedderFieldCount(),
            v8::ArrayBufferView::kEmbedderFieldCount);
  return array_buffer_view;
}

Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              Handle<JSArrayBuffer> buffer,
                                              size_t byte_offset, size_t length,
                                              bool is_length_tracking) {
  size_t element_size;
  ElementsKind elements_kind;
  JSTypedArray::ForFixedTypedArray(type, &element_size, &elements_kind);

  CHECK_IMPLIES(is_length_tracking, v8_flags.harmony_rab_gsab);
  const bool is_backed_by_rab =
      buffer->is_resizable_by_js() && !buffer->is_shared();

  Handle<Map> map;
  if (is_backed_by_rab || is_length_tracking) {
    map = handle(
        isolate()->raw_native_context().TypedArrayElementsKindToRabGsabCtorMap(
            elements_kind),
        isolate());
  } else {
    map =
        handle(isolate()->raw_native_context().TypedArrayElementsKindToCtorMap(
                   elements_kind),
               isolate());
  }

  if (is_length_tracking) {
    // Security: enforce the invariant that length-tracking TypedArrays have
    // their length and byte_length set to 0.
    length = 0;
  }

  size_t byte_length = length * element_size;

  CHECK_LE(length, JSTypedArray::kMaxLength);
  CHECK_EQ(length, byte_length / element_size);
  CHECK_EQ(0, byte_offset % ElementsKindToByteSize(elements_kind));

  Handle<JSTypedArray> typed_array =
      Handle<JSTypedArray>::cast(NewJSArrayBufferView(
          map, empty_byte_array(), buffer, byte_offset, byte_length));
  JSTypedArray raw = *typed_array;
  DisallowGarbageCollection no_gc;
  raw.set_length(length);
  raw.SetOffHeapDataPtr(isolate(), buffer->backing_store(), byte_offset);
  raw.set_is_length_tracking(is_length_tracking);
  raw.set_is_backed_by_rab(is_backed_by_rab);
  return typed_array;
}

Handle<JSDataViewOrRabGsabDataView> Factory::NewJSDataViewOrRabGsabDataView(
    Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length,
    bool is_length_tracking) {
  CHECK_IMPLIES(is_length_tracking, v8_flags.harmony_rab_gsab);
  if (is_length_tracking) {
    // Security: enforce the invariant that length-tracking DataViews have their
    // byte_length set to 0.
    byte_length = 0;
  }
  bool is_backed_by_rab = !buffer->is_shared() && buffer->is_resizable_by_js();
  Handle<Map> map;
  if (is_backed_by_rab || is_length_tracking) {
    map = handle(isolate()->native_context()->js_rab_gsab_data_view_map(),
                 isolate());
  } else {
    map = handle(isolate()->native_context()->data_view_fun().initial_map(),
                 isolate());
  }
  Handle<JSDataViewOrRabGsabDataView> obj =
      Handle<JSDataViewOrRabGsabDataView>::cast(NewJSArrayBufferView(
          map, empty_fixed_array(), buffer, byte_offset, byte_length));
  obj->set_data_pointer(
      isolate(), static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
  obj->set_is_length_tracking(is_length_tracking);
  obj->set_is_backed_by_rab(is_backed_by_rab);
  return obj;
}

MaybeHandle<JSBoundFunction> Factory::NewJSBoundFunction(
    Handle<JSReceiver> target_function, Handle<Object> bound_this,
    base::Vector<Handle<Object>> bound_args) {
  DCHECK(target_function->IsCallable());
  static_assert(InstructionStream::kMaxArguments <= FixedArray::kMaxLength);
  if (bound_args.length() >= InstructionStream::kMaxArguments) {
    THROW_NEW_ERROR(isolate(),
                    NewRangeError(MessageTemplate::kTooManyArguments),
                    JSBoundFunction);
  }

  // Determine the prototype of the {target_function}.
  Handle<HeapObject> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), prototype,
      JSReceiver::GetPrototype(isolate(), target_function), JSBoundFunction);

  SaveAndSwitchContext save(
      isolate(), *target_function->GetCreationContext().ToHandleChecked());

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
  Handle<JSBoundFunction> result = Handle<JSBoundFunction>::cast(
      NewJSObjectFromMap(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  JSBoundFunction raw = *result;
  raw.set_bound_target_function(*target_function, SKIP_WRITE_BARRIER);
  raw.set_bound_this(*bound_this, SKIP_WRITE_BARRIER);
  raw.set_bound_arguments(*bound_arguments, SKIP_WRITE_BARRIER);
  return result;
}

// ES6 section 9.5.15 ProxyCreate (target, handler)
Handle<JSProxy> Factory::NewJSProxy(Handle<JSReceiver> target,
                                    Handle<JSReceiver> handler) {
  // Allocate the proxy object.
  Handle<Map> map = target->IsCallable()
                        ? target->IsConstructor()
                              ? isolate()->proxy_constructor_map()
                              : isolate()->proxy_callable_map()
                        : isolate()->proxy_map();
  DCHECK(map->prototype().IsNull(isolate()));
  JSProxy result = JSProxy::cast(New(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  result.initialize_properties(isolate());
  result.set_target(*target, SKIP_WRITE_BARRIER);
  result.set_handler(*handler, SKIP_WRITE_BARRIER);
  return handle(result, isolate());
}

Handle<JSGlobalProxy> Factory::NewUninitializedJSGlobalProxy(int size) {
  // Create an empty shell of a JSGlobalProxy that needs to be reinitialized
  // via ReinitializeJSGlobalProxy later.
  Handle<Map> map = NewMap(JS_GLOBAL_PROXY_TYPE, size);
  // Maintain invariant expected from any JSGlobalProxy.
  {
    DisallowGarbageCollection no_gc;
    Map raw = *map;
    raw.set_is_access_check_needed(true);
    raw.set_may_have_interesting_symbols(true);
    LOG(isolate(), MapDetails(raw));
  }
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
  JSGlobalProxy raw = *object;
  raw.set_map(*map, kReleaseStore);

  // Reinitialize the object from the constructor map.
  InitializeJSObjectFromMap(raw, *raw_properties_or_hash, *map);
}

Handle<JSMessageObject> Factory::NewJSMessageObject(
    MessageTemplate message, Handle<Object> argument, int start_position,
    int end_position, Handle<SharedFunctionInfo> shared_info,
    int bytecode_offset, Handle<Script> script, Handle<Object> stack_frames) {
  Handle<Map> map = message_object_map();
  JSMessageObject message_obj =
      JSMessageObject::cast(New(map, AllocationType::kYoung));
  DisallowGarbageCollection no_gc;
  message_obj.set_raw_properties_or_hash(*empty_fixed_array(),
                                         SKIP_WRITE_BARRIER);
  message_obj.initialize_elements();
  message_obj.set_elements(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message_obj.set_type(message);
  message_obj.set_argument(*argument, SKIP_WRITE_BARRIER);
  message_obj.set_start_position(start_position);
  message_obj.set_end_position(end_position);
  message_obj.set_script(*script, SKIP_WRITE_BARRIER);
  if (start_position >= 0) {
    // If there's a start_position, then there's no need to store the
    // SharedFunctionInfo as it will never be necessary to regenerate the
    // position.
    message_obj.set_shared_info(*undefined_value(), SKIP_WRITE_BARRIER);
    message_obj.set_bytecode_offset(Smi::FromInt(0));
  } else {
    message_obj.set_bytecode_offset(Smi::FromInt(bytecode_offset));
    if (shared_info.is_null()) {
      message_obj.set_shared_info(*undefined_value(), SKIP_WRITE_BARRIER);
      DCHECK_EQ(bytecode_offset, -1);
    } else {
      message_obj.set_shared_info(*shared_info, SKIP_WRITE_BARRIER);
      DCHECK_GE(bytecode_offset, kFunctionEntryBytecodeOffset);
    }
  }

  message_obj.set_stack_frames(*stack_frames, SKIP_WRITE_BARRIER);
  message_obj.set_error_level(v8::Isolate::kMessageError);
  return handle(message_obj, isolate());
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForApiFunction(
    MaybeHandle<String> maybe_name,
    Handle<FunctionTemplateInfo> function_template_info, FunctionKind kind) {
  return NewSharedFunctionInfo(maybe_name, function_template_info,
                               Builtin::kNoBuiltinId, kind);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForBuiltin(
    MaybeHandle<String> maybe_name, Builtin builtin, FunctionKind kind) {
  return NewSharedFunctionInfo(maybe_name, MaybeHandle<HeapObject>(), builtin,
                               kind);
}

int Factory::NumberToStringCacheHash(Smi number) {
  int mask = (number_string_cache()->length() >> 1) - 1;
  return number.value() & mask;
}

int Factory::NumberToStringCacheHash(double number) {
  int mask = (number_string_cache()->length() >> 1) - 1;
  int64_t bits = base::bit_cast<int64_t>(number);
  return (static_cast<int>(bits) ^ static_cast<int>(bits >> 32)) & mask;
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
    base::Vector<char> buffer(arr, arraysize(arr));
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
  {
    DisallowGarbageCollection no_gc;
    String raw = *result;
    if (value <= JSArray::kMaxArrayIndex &&
        raw.raw_hash_field() == String::kEmptyHashField) {
      uint32_t raw_hash_field = StringHasher::MakeArrayIndexHash(
          static_cast<uint32_t>(value), raw.length());
      raw.set_raw_hash_field(raw_hash_field);
    }
  }
  return result;
}

Handle<DebugInfo> Factory::NewDebugInfo(Handle<SharedFunctionInfo> shared) {
  DCHECK(!shared->HasDebugInfo());

  auto debug_info =
      NewStructInternal<DebugInfo>(DEBUG_INFO_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  SharedFunctionInfo raw_shared = *shared;
  debug_info.set_flags(DebugInfo::kNone, kRelaxedStore);
  debug_info.set_shared(raw_shared);
  debug_info.set_debugger_hints(0);
  DCHECK_EQ(DebugInfo::kNoDebuggingId, debug_info.debugging_id());
  debug_info.set_script(raw_shared.script_or_debug_info(kAcquireLoad));
  HeapObject undefined = *undefined_value();
  debug_info.set_original_bytecode_array(undefined, kReleaseStore,
                                         SKIP_WRITE_BARRIER);
  debug_info.set_debug_bytecode_array(undefined, kReleaseStore,
                                      SKIP_WRITE_BARRIER);
  debug_info.set_break_points(*empty_fixed_array(), SKIP_WRITE_BARRIER);

  // Link debug info to function.
  raw_shared.SetDebugInfo(debug_info);

  return handle(debug_info, isolate());
}

Handle<BreakPointInfo> Factory::NewBreakPointInfo(int source_position) {
  auto new_break_point_info = NewStructInternal<BreakPointInfo>(
      BREAK_POINT_INFO_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  new_break_point_info.set_source_position(source_position);
  new_break_point_info.set_break_points(*undefined_value(), SKIP_WRITE_BARRIER);
  return handle(new_break_point_info, isolate());
}

Handle<BreakPoint> Factory::NewBreakPoint(int id, Handle<String> condition) {
  auto new_break_point =
      NewStructInternal<BreakPoint>(BREAK_POINT_TYPE, AllocationType::kOld);
  DisallowGarbageCollection no_gc;
  new_break_point.set_id(id);
  new_break_point.set_condition(*condition);
  return handle(new_break_point, isolate());
}

Handle<CallSiteInfo> Factory::NewCallSiteInfo(
    Handle<Object> receiver_or_instance, Handle<Object> function,
    Handle<HeapObject> code_object, int code_offset_or_source_position,
    int flags, Handle<FixedArray> parameters) {
  auto info = NewStructInternal<CallSiteInfo>(CALL_SITE_INFO_TYPE,
                                              AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  info.set_receiver_or_instance(*receiver_or_instance, SKIP_WRITE_BARRIER);
  info.set_function(*function, SKIP_WRITE_BARRIER);
  info.set_code_object(*code_object, SKIP_WRITE_BARRIER);
  info.set_code_offset_or_source_position(code_offset_or_source_position);
  info.set_flags(flags);
  info.set_parameters(*parameters, SKIP_WRITE_BARRIER);
  return handle(info, isolate());
}

Handle<StackFrameInfo> Factory::NewStackFrameInfo(
    Handle<HeapObject> shared_or_script, int bytecode_offset_or_source_position,
    Handle<String> function_name, bool is_constructor) {
  DCHECK_GE(bytecode_offset_or_source_position, 0);
  StackFrameInfo info = NewStructInternal<StackFrameInfo>(
      STACK_FRAME_INFO_TYPE, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  info.set_flags(0);
  info.set_shared_or_script(*shared_or_script, SKIP_WRITE_BARRIER);
  info.set_bytecode_offset_or_source_position(
      bytecode_offset_or_source_position);
  info.set_function_name(*function_name, SKIP_WRITE_BARRIER);
  info.set_is_constructor(is_constructor);
  return handle(info, isolate());
}

Handle<PromiseOnStack> Factory::NewPromiseOnStack(Handle<Object> prev,
                                                  Handle<JSObject> promise) {
  PromiseOnStack promise_on_stack = NewStructInternal<PromiseOnStack>(
      PROMISE_ON_STACK_TYPE, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  promise_on_stack.set_prev(*prev, SKIP_WRITE_BARRIER);
  promise_on_stack.set_promise(*MaybeObjectHandle::Weak(promise));
  return handle(promise_on_stack, isolate());
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
  // Use initial slow object proto map for too many properties.
  if (number_of_properties >= JSObject::kMapCacheSize) {
    return handle(context->slow_object_with_object_prototype_map(), isolate());
  }

  Handle<WeakFixedArray> cache(WeakFixedArray::cast(context->map_cache()),
                               isolate());

  // Check to see whether there is a matching element in the cache.
  MaybeObject result = cache->Get(number_of_properties);
  HeapObject heap_object;
  if (result->GetHeapObjectIfWeak(&heap_object)) {
    Map map = Map::cast(heap_object);
    DCHECK(!map.is_dictionary_map());
    return handle(map, isolate());
  }

  // Create a new map and add it to the cache.
  Handle<Map> map = Map::Create(isolate(), number_of_properties);
  DCHECK(!map->is_dictionary_map());
  cache->Set(number_of_properties, HeapObjectReference::Weak(*map));
  return map;
}

Handle<MegaDomHandler> Factory::NewMegaDomHandler(MaybeObjectHandle accessor,
                                                  MaybeObjectHandle context) {
  Handle<Map> map = read_only_roots().mega_dom_handler_map_handle();
  MegaDomHandler handler = MegaDomHandler::cast(New(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  handler.set_accessor(*accessor, kReleaseStore);
  handler.set_context(*context);
  return handle(handler, isolate());
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
  FixedArray store =
      *NewFixedArray(JSRegExp::kAtomDataSize, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  store.set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::ATOM));
  store.set(JSRegExp::kSourceIndex, *source, SKIP_WRITE_BARRIER);
  store.set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store.set(JSRegExp::kAtomPatternIndex, *data, SKIP_WRITE_BARRIER);
  regexp->set_data(store);
}

void Factory::SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                                    Handle<String> source,
                                    JSRegExp::Flags flags, int capture_count,
                                    uint32_t backtrack_limit) {
  DCHECK(Smi::IsValid(backtrack_limit));
  FixedArray store =
      *NewFixedArray(JSRegExp::kIrregexpDataSize, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  Smi uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);
  Smi ticks_until_tier_up = v8_flags.regexp_tier_up
                                ? Smi::FromInt(v8_flags.regexp_tier_up_ticks)
                                : uninitialized;
  store.set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::IRREGEXP));
  store.set(JSRegExp::kSourceIndex, *source, SKIP_WRITE_BARRIER);
  store.set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store.set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpLatin1BytecodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpUC16BytecodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::zero());
  store.set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store.set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  store.set(JSRegExp::kIrregexpTicksUntilTierUpIndex, ticks_until_tier_up);
  store.set(JSRegExp::kIrregexpBacktrackLimit, Smi::FromInt(backtrack_limit));
  regexp->set_data(store);
}

void Factory::SetRegExpExperimentalData(Handle<JSRegExp> regexp,
                                        Handle<String> source,
                                        JSRegExp::Flags flags,
                                        int capture_count) {
  FixedArray store =
      *NewFixedArray(JSRegExp::kExperimentalDataSize, AllocationType::kYoung);
  DisallowGarbageCollection no_gc;
  Smi uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);

  store.set(JSRegExp::kTagIndex, Smi::FromInt(JSRegExp::EXPERIMENTAL));
  store.set(JSRegExp::kSourceIndex, *source, SKIP_WRITE_BARRIER);
  store.set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store.set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpLatin1BytecodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpUC16BytecodeIndex, uninitialized);
  store.set(JSRegExp::kIrregexpMaxRegisterCountIndex, uninitialized);
  store.set(JSRegExp::kIrregexpCaptureCountIndex, Smi::FromInt(capture_count));
  store.set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  store.set(JSRegExp::kIrregexpTicksUntilTierUpIndex, uninitialized);
  store.set(JSRegExp::kIrregexpBacktrackLimit, uninitialized);
  regexp->set_data(store);
}

Handle<RegExpMatchInfo> Factory::NewRegExpMatchInfo() {
  // Initially, the last match info consists of all fixed fields plus space for
  // the match itself (i.e., 2 capture indices).
  static const int kInitialSize = RegExpMatchInfo::kFirstCaptureIndex +
                                  RegExpMatchInfo::kInitialCaptureIndices;

  Handle<FixedArray> elems =
      NewFixedArray(kInitialSize, AllocationType::kYoung);
  Handle<RegExpMatchInfo> result = Handle<RegExpMatchInfo>::cast(elems);
  {
    DisallowGarbageCollection no_gc;
    RegExpMatchInfo raw = *result;
    raw.SetNumberOfCaptureRegisters(RegExpMatchInfo::kInitialCaptureIndices);
    raw.SetLastSubject(*empty_string(), SKIP_WRITE_BARRIER);
    raw.SetLastInput(*undefined_value(), SKIP_WRITE_BARRIER);
    raw.SetCapture(0, 0);
    raw.SetCapture(1, 0);
  }
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
  {
    DisallowGarbageCollection no_gc;
    Map raw_map = *map;
    raw_map.set_has_prototype_slot(has_prototype);
    raw_map.set_is_constructor(has_prototype);
    raw_map.set_is_callable(true);
  }
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
  static_assert(
      JSFunctionOrBoundFunctionOrWrappedFunction::kLengthDescriptorIndex == 0);
  {  // Add length accessor.
    Descriptor d = Descriptor::AccessorConstant(
        length_string(), function_length_accessor(), roc_attribs);
    map->AppendDescriptor(isolate(), &d);
  }

  static_assert(
      JSFunctionOrBoundFunctionOrWrappedFunction::kNameDescriptorIndex == 1);
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
  DCHECK_EQ(0,
            map->instance_descriptors(isolate()).number_of_slack_descriptors());
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
  descriptors_count += inobject_properties_count;

  Handle<Map> map = NewMap(
      JS_FUNCTION_TYPE, header_size + inobject_properties_count * kTaggedSize,
      TERMINAL_FAST_ELEMENTS_KIND, inobject_properties_count);
  {
    DisallowGarbageCollection no_gc;
    Map raw_map = *map;
    raw_map.set_has_prototype_slot(has_prototype);
    raw_map.set_is_constructor(has_prototype);
    raw_map.set_is_callable(true);
  }
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
  static_assert(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length accessor.
    Descriptor d = Descriptor::AccessorConstant(
        length_string(), function_length_accessor(), roc_attribs);
    map->AppendDescriptor(isolate(), &d);
  }

  static_assert(JSFunction::kNameDescriptorIndex == 1);
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
  DCHECK_EQ(0,
            map->instance_descriptors(isolate()).number_of_slack_descriptors());
  LOG(isolate(), MapDetails(*map));
  return map;
}

Handle<Map> Factory::CreateClassFunctionMap(Handle<JSFunction> empty_function) {
  Handle<Map> map =
      NewMap(JS_CLASS_CONSTRUCTOR_TYPE, JSFunction::kSizeWithPrototype);
  {
    DisallowGarbageCollection no_gc;
    Map raw_map = *map;
    raw_map.set_has_prototype_slot(true);
    raw_map.set_is_constructor(true);
    raw_map.set_is_prototype_map(true);
    raw_map.set_is_callable(true);
  }
  Map::SetPrototype(isolate(), map, empty_function);

  //
  // Setup descriptors array.
  //
  Map::EnsureDescriptorSlack(isolate(), map, 2);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  static_assert(JSFunction::kLengthDescriptorIndex == 0);
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
  DisallowGarbageCollection no_gc;
  JSPromise raw = *promise;
  raw.set_reactions_or_result(Smi::zero(), SKIP_WRITE_BARRIER);
  raw.set_flags(0);
  // TODO(v8) remove once embedder data slots are always zero-initialized.
  InitEmbedderFields(*promise, Smi::zero());
  DCHECK_EQ(raw.GetEmbedderFieldCount(), v8::Promise::kEmbedderFieldCount);
  return promise;
}

Handle<JSPromise> Factory::NewJSPromise() {
  Handle<JSPromise> promise = NewJSPromiseWithoutHook();
  isolate()->RunAllPromiseHooks(PromiseHookType::kInit, promise,
                                undefined_value());
  return promise;
}

Handle<CallHandlerInfo> Factory::NewCallHandlerInfo(bool has_no_side_effect) {
  Handle<Map> map = has_no_side_effect
                        ? side_effect_free_call_handler_info_map()
                        : side_effect_call_handler_info_map();
  CallHandlerInfo info = CallHandlerInfo::cast(New(map, AllocationType::kOld));
  DisallowGarbageCollection no_gc;
  info.set_data(*undefined_value(), SKIP_WRITE_BARRIER);
  info.init_callback(isolate(), kNullAddress);
  return handle(info, isolate());
}

bool Factory::CanAllocateInReadOnlySpace() {
  return allocator()->CanAllocateInReadOnlySpace();
}

bool Factory::EmptyStringRootIsInitialized() {
  return isolate()->roots_table()[RootIndex::kempty_string] != kNullAddress;
}

AllocationType Factory::AllocationTypeForInPlaceInternalizableString() {
  return isolate()
      ->heap()
      ->allocation_type_for_in_place_internalizable_strings();
}

Handle<JSFunction> Factory::NewFunctionForTesting(Handle<String> name) {
  Handle<SharedFunctionInfo> info =
      NewSharedFunctionInfoForBuiltin(name, Builtin::kIllegal);
  info->set_language_mode(LanguageMode::kSloppy);
  return JSFunctionBuilder{isolate(), info, isolate()->native_context()}
      .Build();
}

Handle<JSSharedStruct> Factory::NewJSSharedStruct(
    Handle<JSFunction> constructor) {
  SharedObjectSafePublishGuard publish_guard;

  Handle<Map> instance_map(constructor->initial_map(), isolate());
  Handle<PropertyArray> property_array;
  const int num_oob_fields =
      instance_map->NumberOfFields(ConcurrencyMode::kSynchronous) -
      instance_map->GetInObjectProperties();
  if (num_oob_fields > 0) {
    property_array =
        NewPropertyArray(num_oob_fields, AllocationType::kSharedOld);
  }

  Handle<JSSharedStruct> instance = Handle<JSSharedStruct>::cast(
      NewJSObject(constructor, AllocationType::kSharedOld));

  // The struct object has not been fully initialized yet. Disallow allocation
  // from this point on.
  DisallowGarbageCollection no_gc;
  if (!property_array.is_null()) instance->SetProperties(*property_array);

  return instance;
}

Handle<JSSharedArray> Factory::NewJSSharedArray(Handle<JSFunction> constructor,
                                                int length) {
  SharedObjectSafePublishGuard publish_guard;
  Handle<FixedArrayBase> storage =
      NewFixedArray(length, AllocationType::kSharedOld);
  Handle<JSSharedArray> instance = Handle<JSSharedArray>::cast(
      NewJSObject(constructor, AllocationType::kSharedOld));
  instance->set_elements(*storage);
  FieldIndex index = FieldIndex::ForDescriptor(
      constructor->initial_map(),
      InternalIndex(JSSharedArray::kLengthFieldIndex));
  instance->FastPropertyAtPut(index, Smi::FromInt(length), SKIP_WRITE_BARRIER);
  return instance;
}

Handle<JSAtomicsMutex> Factory::NewJSAtomicsMutex() {
  SharedObjectSafePublishGuard publish_guard;
  Handle<Map> map = isolate()->js_atomics_mutex_map();
  Handle<JSAtomicsMutex> mutex = Handle<JSAtomicsMutex>::cast(
      NewJSObjectFromMap(map, AllocationType::kSharedOld));
  mutex->set_state(JSAtomicsMutex::kUnlocked);
  mutex->set_owner_thread_id(ThreadId::Invalid().ToInteger());
  return mutex;
}

Handle<JSAtomicsCondition> Factory::NewJSAtomicsCondition() {
  SharedObjectSafePublishGuard publish_guard;
  Handle<Map> map = isolate()->js_atomics_condition_map();
  Handle<JSAtomicsCondition> cond = Handle<JSAtomicsCondition>::cast(
      NewJSObjectFromMap(map, AllocationType::kSharedOld));
  cond->set_state(JSAtomicsCondition::kEmptyState);
  return cond;
}

Factory::JSFunctionBuilder::JSFunctionBuilder(Isolate* isolate,
                                              Handle<SharedFunctionInfo> sfi,
                                              Handle<Context> context)
    : isolate_(isolate), sfi_(sfi), context_(context) {}

Handle<JSFunction> Factory::JSFunctionBuilder::Build() {
  PrepareMap();
  PrepareFeedbackCell();

  Handle<Code> code = handle(sfi_->GetCode(isolate_), isolate_);
  Handle<JSFunction> result = BuildRaw(code);

  if (code->kind() == CodeKind::BASELINE) {
    IsCompiledScope is_compiled_scope(sfi_->is_compiled_scope(isolate_));
    JSFunction::EnsureFeedbackVector(isolate_, result, &is_compiled_scope);
  }

  Compiler::PostInstantiation(result);
  return result;
}

Handle<JSFunction> Factory::JSFunctionBuilder::BuildRaw(Handle<Code> code) {
  Isolate* isolate = isolate_;
  Factory* factory = isolate_->factory();

  Handle<Map> map = maybe_map_.ToHandleChecked();
  Handle<FeedbackCell> feedback_cell = maybe_feedback_cell_.ToHandleChecked();

  DCHECK(InstanceTypeChecker::IsJSFunction(*map));

  // Allocation.
  JSFunction function = JSFunction::cast(factory->New(map, allocation_type_));
  DisallowGarbageCollection no_gc;

  WriteBarrierMode mode = allocation_type_ == AllocationType::kYoung
                              ? SKIP_WRITE_BARRIER
                              : UPDATE_WRITE_BARRIER;
  // Header initialization.
  function.initialize_properties(isolate);
  function.initialize_elements();
  function.set_shared(*sfi_, mode);
  function.set_context(*context_, kReleaseStore, mode);
  function.set_raw_feedback_cell(*feedback_cell, mode);
  function.set_code(*code, kReleaseStore, mode);
  if (function.has_prototype_slot()) {
    function.set_prototype_or_initial_map(
        ReadOnlyRoots(isolate).the_hole_value(), kReleaseStore,
        SKIP_WRITE_BARRIER);
  }

  // Potentially body initialization.
  factory->InitializeJSObjectBody(
      function, *map, JSFunction::GetHeaderSize(map->has_prototype_slot()));

  return handle(function, isolate_);
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
    // Track the newly-created closure.
    feedback_cell->IncrementClosureCount(isolate_);
  } else {
    // Fall back to the many_closures_cell.
    maybe_feedback_cell_ = isolate_->factory()->many_closures_cell();
  }
}

}  // namespace internal
}  // namespace v8
