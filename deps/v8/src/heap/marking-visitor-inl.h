// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_INL_H_
#define V8_HEAP_MARKING_VISITOR_INL_H_

#include "src/common/globals.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/pretenuring-handler-inl.h"
#include "src/heap/progress-bar.h"
#include "src/heap/spaces.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/objects.h"
#include "src/objects/property-details.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"
#include "src/sandbox/external-pointer-inl.h"

namespace v8 {
namespace internal {

// ===========================================================================
// Visiting strong and weak pointers =========================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::MarkObject(
    HeapObject host, HeapObject object) {
  DCHECK(ReadOnlyHeap::Contains(object) || heap_->Contains(object));
  SynchronizePageAccess(object);
  AddStrongReferenceForReferenceSummarizer(host, object);
  if (concrete_visitor()->marking_state()->TryMark(object)) {
    local_marking_worklists_->Push(object);
    if (V8_UNLIKELY(concrete_visitor()->retaining_path_mode() ==
                    TraceRetainingPathMode::kEnabled)) {
      heap_->AddRetainer(host, object);
    }
  }
}

// class template arguments
template <typename ConcreteVisitor, typename MarkingState>
// method template arguments
template <typename THeapObjectSlot>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::ProcessStrongHeapObject(
    HeapObject host, THeapObjectSlot slot, HeapObject heap_object) {
  SynchronizePageAccess(heap_object);
  if (!ShouldMarkObject(heap_object)) return;
  MarkObject(host, heap_object);
  concrete_visitor()->RecordSlot(host, slot, heap_object);
}

// class template arguments
template <typename ConcreteVisitor, typename MarkingState>
// method template arguments
template <typename THeapObjectSlot>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::ProcessWeakHeapObject(
    HeapObject host, THeapObjectSlot slot, HeapObject heap_object) {
  SynchronizePageAccess(heap_object);
  if (!ShouldMarkObject(heap_object)) return;
  if (concrete_visitor()->marking_state()->IsBlackOrGrey(heap_object)) {
    // Weak references with live values are directly processed here to
    // reduce the processing time of weak cells during the main GC
    // pause.
    concrete_visitor()->RecordSlot(host, slot, heap_object);
  } else {
    // If we do not know about liveness of the value, we have to process
    // the reference when we know the liveness of the whole transitive
    // closure.
    local_weak_objects_->weak_references_local.Push(std::make_pair(host, slot));
    AddWeakReferenceForReferenceSummarizer(host, heap_object);
  }
}

// class template arguments
template <typename ConcreteVisitor, typename MarkingState>
// method template arguments
template <typename TSlot>
V8_INLINE void
MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitPointersImpl(
    HeapObject host, TSlot start, TSlot end) {
  using THeapObjectSlot = typename TSlot::THeapObjectSlot;
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object =
        slot.Relaxed_Load(ObjectVisitorWithCageBases::cage_base());
    HeapObject heap_object;
    if (object.GetHeapObjectIfStrong(&heap_object)) {
      // If the reference changes concurrently from strong to weak, the write
      // barrier will treat the weak reference as strong, so we won't miss the
      // weak reference.
      ProcessStrongHeapObject(host, THeapObjectSlot(slot), heap_object);
    } else if (TSlot::kCanBeWeak && object.GetHeapObjectIfWeak(&heap_object)) {
      ProcessWeakHeapObject(host, THeapObjectSlot(slot), heap_object);
    }
  }
}

template <typename ConcreteVisitor, typename MarkingState>
V8_INLINE void
MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitCodePointerImpl(
    Code host, CodeObjectSlot slot) {
  Object object =
      slot.Relaxed_Load(ObjectVisitorWithCageBases::code_cage_base());
  HeapObject heap_object;
  if (object.GetHeapObjectIfStrong(&heap_object)) {
    // If the reference changes concurrently from strong to weak, the write
    // barrier will treat the weak reference as strong, so we won't miss the
    // weak reference.
    ProcessStrongHeapObject(host, HeapObjectSlot(slot), heap_object);
  }
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitEmbeddedPointer(
    RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
  HeapObject object =
      rinfo->target_object(ObjectVisitorWithCageBases::cage_base());
  if (!ShouldMarkObject(object)) return;

  if (!concrete_visitor()->marking_state()->IsBlackOrGrey(object)) {
    if (rinfo->code().IsWeakObject(object)) {
      local_weak_objects_->weak_objects_in_code_local.Push(
          std::make_pair(object, rinfo->code()));
      AddWeakReferenceForReferenceSummarizer(rinfo->instruction_stream(),
                                             object);
    } else {
      MarkObject(rinfo->instruction_stream(), object);
    }
  }
  concrete_visitor()->RecordRelocSlot(rinfo, object);
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitCodeTarget(
    RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
  InstructionStream target =
      InstructionStream::FromTargetAddress(rinfo->target_address());

  if (!ShouldMarkObject(target)) return;
  MarkObject(rinfo->instruction_stream(), target);
  concrete_visitor()->RecordRelocSlot(rinfo, target);
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitExternalPointer(
    HeapObject host, ExternalPointerSlot slot, ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  ExternalPointerHandle handle = slot.Relaxed_LoadHandle();
  ExternalPointerTable* table = IsSharedExternalPointerType(tag)
                                    ? shared_external_pointer_table_
                                    : external_pointer_table_;
  table->Mark(handle, slot.address());
#endif  // V8_ENABLE_SANDBOX
}

// ===========================================================================
// Object participating in bytecode flushing =================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitBytecodeArray(
    Map map, BytecodeArray object) {
  if (!concrete_visitor()->ShouldVisit(object)) return 0;
  int size = BytecodeArray::BodyDescriptor::SizeOf(map, object);
  this->VisitMapPointer(object);
  BytecodeArray::BodyDescriptor::IterateBody(map, object, size, this);
  if (!should_keep_ages_unchanged_) {
    object.MakeOlder();
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSFunction(
    Map map, JSFunction js_function) {
  int size = concrete_visitor()->VisitJSObjectSubclass(map, js_function);
  if (js_function.ShouldFlushBaselineCode(code_flush_mode_)) {
    DCHECK(IsBaselineCodeFlushingEnabled(code_flush_mode_));
    local_weak_objects_->baseline_flushing_candidates_local.Push(js_function);
  } else {
    VisitPointer(js_function, js_function.RawField(JSFunction::kCodeOffset));
    // TODO(mythria): Consider updating the check for ShouldFlushBaselineCode to
    // also include cases where there is old bytecode even when there is no
    // baseline code and remove this check here.
    if (IsByteCodeFlushingEnabled(code_flush_mode_) &&
        js_function.NeedsResetDueToFlushedBytecode()) {
      local_weak_objects_->flushed_js_functions_local.Push(js_function);
    }
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitSharedFunctionInfo(
    Map map, SharedFunctionInfo shared_info) {
  if (!concrete_visitor()->ShouldVisit(shared_info)) return 0;

  int size = SharedFunctionInfo::BodyDescriptor::SizeOf(map, shared_info);
  this->VisitMapPointer(shared_info);
  SharedFunctionInfo::BodyDescriptor::IterateBody(map, shared_info, size, this);

  if (!shared_info.ShouldFlushCode(code_flush_mode_)) {
    // If the SharedFunctionInfo doesn't have old bytecode visit the function
    // data strongly.
    VisitPointer(shared_info,
                 shared_info.RawField(SharedFunctionInfo::kFunctionDataOffset));
  } else if (!IsByteCodeFlushingEnabled(code_flush_mode_)) {
    // If bytecode flushing is disabled but baseline code flushing is enabled
    // then we have to visit the bytecode but not the baseline code.
    DCHECK(IsBaselineCodeFlushingEnabled(code_flush_mode_));
    Code baseline_code = Code::cast(shared_info.function_data(kAcquireLoad));
    // Visit the bytecode hanging off baseline code.
    VisitPointer(baseline_code,
                 baseline_code.RawField(
                     Code::kDeoptimizationDataOrInterpreterDataOffset));
    local_weak_objects_->code_flushing_candidates_local.Push(shared_info);
  } else {
    // In other cases, record as a flushing candidate since we have old
    // bytecode.
    local_weak_objects_->code_flushing_candidates_local.Push(shared_info);
  }
  return size;
}

// ===========================================================================
// Fixed arrays that need incremental processing and can be left-trimmed =====
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                   ProgressBar& progress_bar) {
  const int kProgressBarScanningChunk = kMaxRegularHeapObjectSize;
  static_assert(kMaxRegularHeapObjectSize % kTaggedSize == 0);
  DCHECK(concrete_visitor()->marking_state()->IsBlackOrGrey(object));
  concrete_visitor()->marking_state()->GreyToBlack(object);
  int size = FixedArray::BodyDescriptor::SizeOf(map, object);
  size_t current_progress_bar = progress_bar.Value();
  int start = static_cast<int>(current_progress_bar);
  if (start == 0) {
    this->VisitMapPointer(object);
    start = FixedArray::BodyDescriptor::kStartOffset;
  }
  int end = std::min(size, start + kProgressBarScanningChunk);
  if (start < end) {
    VisitPointers(object, object.RawField(start), object.RawField(end));
    bool success = progress_bar.TrySetNewValue(current_progress_bar, end);
    CHECK(success);
    if (end < size) {
      // The object can be pushed back onto the marking worklist only after
      // progress bar was updated.
      DCHECK(ShouldMarkObject(object));
      local_marking_worklists_->Push(object);
    }
  }
  return end - start;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitFixedArrayRegularly(
    Map map, FixedArray object) {
  if (!concrete_visitor()->ShouldVisit(object)) return 0;
  int size = FixedArray::BodyDescriptor::SizeOf(map, object);
  concrete_visitor()->VisitMapPointerIfNeeded(object);
  FixedArray::BodyDescriptor::IterateBody(map, object, size,
                                          concrete_visitor());
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitFixedArray(
    Map map, FixedArray object) {
  ProgressBar& progress_bar =
      MemoryChunk::FromHeapObject(object)->ProgressBar();
  return CanUpdateValuesInHeap() && progress_bar.IsEnabled()
             ? VisitFixedArrayWithProgressBar(map, object, progress_bar)
             : VisitFixedArrayRegularly(map, object);
}

// ===========================================================================
// Objects participating in embedder tracing =================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
template <typename T>
inline int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitEmbedderTracingSubClassNoEmbedderTracing(Map map, T object) {
  return concrete_visitor()->VisitJSObjectSubclass(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
template <typename T>
inline int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object) {
  const bool requires_snapshot =
      local_marking_worklists_->SupportsExtractWrapper();
  MarkingWorklists::Local::WrapperSnapshot wrapper_snapshot;
  const bool valid_snapshot =
      requires_snapshot &&
      local_marking_worklists_->ExtractWrapper(map, object, wrapper_snapshot);
  const int size = concrete_visitor()->VisitJSObjectSubclass(map, object);
  if (size && valid_snapshot) {
    local_marking_worklists_->PushExtractedWrapper(wrapper_snapshot);
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
template <typename T>
int MarkingVisitorBase<ConcreteVisitor,
                       MarkingState>::VisitEmbedderTracingSubclass(Map map,
                                                                   T object) {
  DCHECK(object.MayHaveEmbedderFields());
  if (V8_LIKELY(trace_embedder_fields_)) {
    return VisitEmbedderTracingSubClassWithEmbedderTracing(map, object);
  }
  return VisitEmbedderTracingSubClassNoEmbedderTracing(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSApiObject(
    Map map, JSObject object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSArrayBuffer(
    Map map, JSArrayBuffer object) {
  object.MarkExtension();
  return VisitEmbedderTracingSubclass(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitJSDataViewOrRabGsabDataView(Map map,
                                     JSDataViewOrRabGsabDataView object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSTypedArray(
    Map map, JSTypedArray object) {
  return VisitEmbedderTracingSubclass(map, object);
}

// ===========================================================================
// Weak JavaScript objects ===================================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitEphemeronHashTable(
    Map map, EphemeronHashTable table) {
  if (!concrete_visitor()->ShouldVisit(table)) return 0;
  local_weak_objects_->ephemeron_hash_tables_local.Push(table);

  for (InternalIndex i : table.IterateEntries()) {
    ObjectSlot key_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
    HeapObject key = HeapObject::cast(table.KeyAt(i));

    SynchronizePageAccess(key);
    concrete_visitor()->RecordSlot(table, key_slot, key);
    AddWeakReferenceForReferenceSummarizer(table, key);

    ObjectSlot value_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

    // Objects in the shared heap are prohibited from being used as keys in
    // WeakMaps and WeakSets and therefore cannot be ephemeron keys. See also
    // MarkCompactCollector::ProcessEphemeron.
    DCHECK(!key.InWritableSharedSpace());
    if (key.InReadOnlySpace() ||
        concrete_visitor()->marking_state()->IsBlackOrGrey(key)) {
      VisitPointer(table, value_slot);
    } else {
      Object value_obj = table.ValueAt(i);

      if (value_obj.IsHeapObject()) {
        HeapObject value = HeapObject::cast(value_obj);
        SynchronizePageAccess(value);
        concrete_visitor()->RecordSlot(table, value_slot, value);
        AddWeakReferenceForReferenceSummarizer(table, value);

        if (!ShouldMarkObject(value)) continue;

        // Revisit ephemerons with both key and value unreachable at end
        // of concurrent marking cycle.
        if (concrete_visitor()->marking_state()->IsUnmarked(value)) {
          local_weak_objects_->discovered_ephemerons_local.Push(
              Ephemeron{key, value});
        }
      }
    }
  }
  return table.SizeFromMap(map);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSWeakRef(
    Map map, JSWeakRef weak_ref) {
  int size = concrete_visitor()->VisitJSObjectSubclass(map, weak_ref);
  if (size == 0) return 0;
  if (weak_ref.target().IsHeapObject()) {
    HeapObject target = HeapObject::cast(weak_ref.target());
    SynchronizePageAccess(target);
    if (target.InReadOnlySpace() ||
        concrete_visitor()->marking_state()->IsBlackOrGrey(target)) {
      // Record the slot inside the JSWeakRef, since the
      // VisitJSObjectSubclass above didn't visit it.
      ObjectSlot slot = weak_ref.RawField(JSWeakRef::kTargetOffset);
      concrete_visitor()->RecordSlot(weak_ref, slot, target);
    } else {
      // JSWeakRef points to a potentially dead object. We have to process
      // them when we know the liveness of the whole transitive closure.
      local_weak_objects_->js_weak_refs_local.Push(weak_ref);
      AddWeakReferenceForReferenceSummarizer(weak_ref, target);
    }
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitWeakCell(
    Map map, WeakCell weak_cell) {
  if (!concrete_visitor()->ShouldVisit(weak_cell)) return 0;

  int size = WeakCell::BodyDescriptor::SizeOf(map, weak_cell);
  this->VisitMapPointer(weak_cell);
  WeakCell::BodyDescriptor::IterateBody(map, weak_cell, size, this);
  HeapObject target = weak_cell.relaxed_target();
  HeapObject unregister_token = weak_cell.relaxed_unregister_token();
  SynchronizePageAccess(target);
  SynchronizePageAccess(unregister_token);
  if ((target.InReadOnlySpace() ||
       concrete_visitor()->marking_state()->IsBlackOrGrey(target)) &&
      (unregister_token.InReadOnlySpace() ||
       concrete_visitor()->marking_state()->IsBlackOrGrey(unregister_token))) {
    // Record the slots inside the WeakCell, since the IterateBody above
    // didn't visit it.
    ObjectSlot slot = weak_cell.RawField(WeakCell::kTargetOffset);
    concrete_visitor()->RecordSlot(weak_cell, slot, target);
    slot = weak_cell.RawField(WeakCell::kUnregisterTokenOffset);
    concrete_visitor()->RecordSlot(weak_cell, slot, unregister_token);
  } else {
    // WeakCell points to a potentially dead object or a dead unregister
    // token. We have to process them when we know the liveness of the whole
    // transitive closure.
    local_weak_objects_->weak_cells_local.Push(weak_cell);
    AddWeakReferenceForReferenceSummarizer(weak_cell, target);
    AddWeakReferenceForReferenceSummarizer(weak_cell, unregister_token);
  }
  return size;
}

// ===========================================================================
// Custom weakness in descriptor arrays and transition arrays ================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitDescriptorArrayStrongly(Map map, DescriptorArray array) {
  concrete_visitor()->ShouldVisit(array);
  this->VisitMapPointer(array);
  int size = DescriptorArray::BodyDescriptor::SizeOf(map, array);
  VisitPointers(array, array.GetFirstPointerSlot(), array.GetDescriptorSlot(0));
  VisitPointers(
      array, MaybeObjectSlot(array.GetDescriptorSlot(0)),
      MaybeObjectSlot(array.GetDescriptorSlot(array.number_of_descriptors())));
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitDescriptorArray(
    Map map, DescriptorArray array) {
  if (!CanUpdateValuesInHeap()) {
    // If we cannot update the values in the heap, we just treat the array
    // strongly.
    return VisitDescriptorArrayStrongly(map, array);
  }

  // The markbit is not used anymore. This is different from a checked
  // transition in that the array is re-added to the worklist and thus there's
  // many invocations of this transition. All cases (roots, marking via map,
  // write barrier) are handled here as they all update the state accordingly.
  concrete_visitor()->marking_state()->GreyToBlack(array);
  const auto [start, end] =
      DescriptorArrayMarkingState::AcquireDescriptorRangeToMark(
          mark_compact_epoch_, array);
  if (start != end) {
    DCHECK_LT(start, end);
    VisitPointers(array, MaybeObjectSlot(array.GetDescriptorSlot(start)),
                  MaybeObjectSlot(array.GetDescriptorSlot(end)));
    if (start == 0) {
      // We are processing the object the first time. Visit the header and
      // return a size for accounting.
      int size = DescriptorArray::BodyDescriptor::SizeOf(map, array);
      VisitPointers(array, array.GetFirstPointerSlot(),
                    array.GetDescriptorSlot(0));
      concrete_visitor()->VisitMapPointerIfNeeded(array);
      return size;
    }
  }
  return 0;
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitDescriptorsForMap(
    Map map) {
  if (!CanUpdateValuesInHeap() || !map.CanTransition()) return;

  // Maps that can transition share their descriptor arrays and require
  // special visiting logic to avoid memory leaks.
  // Since descriptor arrays are potentially shared, ensure that only the
  // descriptors that belong to this map are marked. The first time a
  // non-empty descriptor array is marked, its header is also visited. The
  // slot holding the descriptor array will be implicitly recorded when the
  // pointer fields of this map are visited.
  Object maybe_descriptors =
      TaggedField<Object, Map::kInstanceDescriptorsOffset>::Acquire_Load(
          heap_->isolate(), map);

  // If the descriptors are a Smi, then this Map is in the process of being
  // deserialized, and doesn't yet have an initialized descriptor field.
  if (maybe_descriptors.IsSmi()) {
    DCHECK_EQ(maybe_descriptors, Smi::uninitialized_deserialization_value());
    return;
  }

  DescriptorArray descriptors = DescriptorArray::cast(maybe_descriptors);
  // Normal processing of descriptor arrays through the pointers iteration that
  // follows this call:
  // - Array in read only space;
  // - StrongDescriptor array;
  if (descriptors.InReadOnlySpace() || descriptors.IsStrongDescriptorArray()) {
    return;
  }

  const int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  if (number_of_own_descriptors) {
    // It is possible that the concurrent marker observes the
    // number_of_own_descriptors out of sync with the descriptors. In that
    // case the marking write barrier for the descriptor array will ensure
    // that all required descriptors are marked. The concurrent marker
    // just should avoid crashing in that case. That's why we need the
    // std::min<int>() below.
    const auto descriptors_to_mark = std::min<int>(
        number_of_own_descriptors, descriptors.number_of_descriptors());
    concrete_visitor()->marking_state()->TryMark(descriptors);
    if (DescriptorArrayMarkingState::TryUpdateIndicesToMark(
            mark_compact_epoch_, descriptors, descriptors_to_mark)) {
      local_marking_worklists_->Push(descriptors);
    }
  }
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitMap(Map meta_map,
                                                                Map map) {
  if (!concrete_visitor()->ShouldVisit(map)) return 0;
  int size = Map::BodyDescriptor::SizeOf(meta_map, map);
  VisitDescriptorsForMap(map);

  // Mark the pointer fields of the Map. If there is a transitions array, it has
  // been marked already, so it is fine that one of these fields contains a
  // pointer to it.
  Map::BodyDescriptor::IterateBody(meta_map, map, size, this);
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitTransitionArray(
    Map map, TransitionArray array) {
  if (!concrete_visitor()->ShouldVisit(array)) return 0;
  this->VisitMapPointer(array);
  int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
  TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
  local_weak_objects_->transition_arrays_local.Push(array);
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
YoungGenerationMarkingVisitorBase<ConcreteVisitor, MarkingState>::
    YoungGenerationMarkingVisitorBase(Isolate* isolate,
                                      MarkingWorklists::Local* worklists_local)
    : NewSpaceVisitor<ConcreteVisitor>(isolate),
      worklists_local_(worklists_local),
      pretenuring_handler_(isolate->heap()->pretenuring_handler()),
      local_pretenuring_feedback_(
          PretenuringHandler::kInitialFeedbackCapacity) {}

template <typename ConcreteVisitor, typename MarkingState>
template <typename T>
int YoungGenerationMarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object) {
  const int size = concrete_visitor()->VisitJSObjectSubclass(map, object);
  if (!worklists_local_->SupportsExtractWrapper()) return size;
  MarkingWorklists::Local::WrapperSnapshot wrapper_snapshot;
  const bool valid_snapshot =
      worklists_local_->ExtractWrapper(map, object, wrapper_snapshot);
  if (size && valid_snapshot) {
    // Success: The object needs to be processed for embedder references.
    worklists_local_->PushExtractedWrapper(wrapper_snapshot);
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSArrayBuffer(Map map,
                                                       JSArrayBuffer object) {
  object.YoungMarkExtension();
  return VisitEmbedderTracingSubClassWithEmbedderTracing(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSApiObject(Map map, JSObject object) {
  return VisitEmbedderTracingSubClassWithEmbedderTracing(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitJSDataViewOrRabGsabDataView(Map map,
                                     JSDataViewOrRabGsabDataView object) {
  return VisitEmbedderTracingSubClassWithEmbedderTracing(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSTypedArray(Map map,
                                                      JSTypedArray object) {
  return VisitEmbedderTracingSubClassWithEmbedderTracing(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSObject(Map map, JSObject object) {
  int result = NewSpaceVisitor<ConcreteVisitor>::VisitJSObject(map, object);
  DCHECK_LT(0, result);
  pretenuring_handler_->UpdateAllocationSite(map, object,
                                             &local_pretenuring_feedback_);
  return result;
}

template <typename ConcreteVisitor, typename MarkingState>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSObjectFast(Map map,
                                                      JSObject object) {
  int result = NewSpaceVisitor<ConcreteVisitor>::VisitJSObjectFast(map, object);
  DCHECK_LT(0, result);
  pretenuring_handler_->UpdateAllocationSite(map, object,
                                             &local_pretenuring_feedback_);
  return result;
}

template <typename ConcreteVisitor, typename MarkingState>
template <typename T, typename TBodyDescriptor>
int YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitJSObjectSubclass(Map map, T object) {
  int result = NewSpaceVisitor<ConcreteVisitor>::template VisitJSObjectSubclass<
      T, TBodyDescriptor>(map, object);
  DCHECK_LT(0, result);
  pretenuring_handler_->UpdateAllocationSite(map, object,
                                             &local_pretenuring_feedback_);
  return result;
}

template <typename ConcreteVisitor, typename MarkingState>
void YoungGenerationMarkingVisitorBase<ConcreteVisitor,
                                       MarkingState>::Finalize() {
  pretenuring_handler_->MergeAllocationSitePretenuringFeedback(
      local_pretenuring_feedback_);
  local_pretenuring_feedback_.clear();
}

template <typename ConcreteVisitor, typename MarkingState>
void YoungGenerationMarkingVisitorBase<ConcreteVisitor, MarkingState>::
    MarkObjectViaMarkingWorklist(HeapObject object) {
  if (concrete_visitor()->marking_state()->TryMark(object)) {
    worklists_local_->Push(object);
  }
}

template <typename ConcreteVisitor, typename MarkingState>
template <typename TSlot>
void YoungGenerationMarkingVisitorBase<
    ConcreteVisitor, MarkingState>::VisitPointerImpl(HeapObject host,
                                                     TSlot slot) {
  typename TSlot::TObject target =
      slot.Relaxed_Load(ObjectVisitorWithCageBases::cage_base());
  if (Heap::InYoungGeneration(target)) {
    // Treat weak references as strong.
    HeapObject target_object = target.GetHeapObject();
    MarkObjectViaMarkingWorklist(target_object);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_INL_H_
