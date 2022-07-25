// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_INL_H_
#define V8_HEAP_MARKING_VISITOR_INL_H_

#include "src/heap/marking-visitor.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/progress-bar.h"
#include "src/heap/spaces.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

// ===========================================================================
// Visiting strong and weak pointers =========================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::MarkObject(
    HeapObject host, HeapObject object) {
  DCHECK(ReadOnlyHeap::Contains(object) || heap_->Contains(object));
  concrete_visitor()->SynchronizePageAccess(object);
  AddStrongReferenceForReferenceSummarizer(host, object);
  if (concrete_visitor()->marking_state()->WhiteToGrey(object)) {
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
  concrete_visitor()->SynchronizePageAccess(heap_object);
  if (!is_shared_heap_ && heap_object.InSharedHeap()) return;
  MarkObject(host, heap_object);
  concrete_visitor()->RecordSlot(host, slot, heap_object);
}

// class template arguments
template <typename ConcreteVisitor, typename MarkingState>
// method template arguments
template <typename THeapObjectSlot>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::ProcessWeakHeapObject(
    HeapObject host, THeapObjectSlot slot, HeapObject heap_object) {
  concrete_visitor()->SynchronizePageAccess(heap_object);
  if (!is_shared_heap_ && heap_object.InSharedHeap()) return;
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
    HeapObject host, CodeObjectSlot slot) {
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
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
    Code host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
  HeapObject object =
      rinfo->target_object(ObjectVisitorWithCageBases::cage_base());
  if (!is_shared_heap_ && object.InSharedHeap()) return;

  if (!concrete_visitor()->marking_state()->IsBlackOrGrey(object)) {
    if (host.IsWeakObject(object)) {
      local_weak_objects_->weak_objects_in_code_local.Push(
          std::make_pair(object, host));
      AddWeakReferenceForReferenceSummarizer(host, object);
    } else {
      MarkObject(host, object);
    }
  }
  concrete_visitor()->RecordRelocSlot(host, rinfo, object);
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitCodeTarget(
    Code host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
  Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());

  if (!is_shared_heap_ && target.InSharedHeap()) return;
  MarkObject(host, target);
  concrete_visitor()->RecordRelocSlot(host, rinfo, target);
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
    CodeT baseline_codet = CodeT::cast(shared_info.function_data(kAcquireLoad));
    // Safe to do a relaxed load here since the CodeT was acquire-loaded.
    Code baseline_code = FromCodeT(baseline_codet, kRelaxedLoad);
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
  STATIC_ASSERT(kMaxRegularHeapObjectSize % kTaggedSize == 0);
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
      local_marking_worklists_->Push(object);
    }
  }
  return end - start;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitFixedArray(
    Map map, FixedArray object) {
  // Arrays with the progress bar are not left-trimmable because they reside
  // in the large object space.
  ProgressBar& progress_bar =
      MemoryChunk::FromHeapObject(object)->ProgressBar();
  return CanUpdateValuesInHeap() && progress_bar.IsEnabled()
             ? VisitFixedArrayWithProgressBar(map, object, progress_bar)
             : concrete_visitor()->VisitLeftTrimmableArray(map, object);
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitFixedDoubleArray(
    Map map, FixedDoubleArray object) {
  return concrete_visitor()->VisitLeftTrimmableArray(map, object);
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
  if (size) {
    if (valid_snapshot) {
      // Success: The object needs to be processed for embedder references.
      local_marking_worklists_->PushExtractedWrapper(wrapper_snapshot);
    } else if (!requires_snapshot) {
      // Snapshot not supported. Just fall back to pushing the wrapper itself
      // instead which will be processed on the main thread.
      local_marking_worklists_->PushWrapper(object);
    }
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
template <typename T>
int MarkingVisitorBase<ConcreteVisitor,
                       MarkingState>::VisitEmbedderTracingSubclass(Map map,
                                                                   T object) {
  DCHECK(object.MayHaveEmbedderFields());
  if (V8_LIKELY(is_embedder_tracing_enabled_)) {
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
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSDataView(
    Map map, JSDataView object) {
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

    concrete_visitor()->SynchronizePageAccess(key);
    concrete_visitor()->RecordSlot(table, key_slot, key);
    AddWeakReferenceForReferenceSummarizer(table, key);

    ObjectSlot value_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

    if ((!is_shared_heap_ && key.InSharedHeap()) ||
        concrete_visitor()->marking_state()->IsBlackOrGrey(key)) {
      VisitPointer(table, value_slot);
    } else {
      Object value_obj = table.ValueAt(i);

      if (value_obj.IsHeapObject()) {
        HeapObject value = HeapObject::cast(value_obj);
        concrete_visitor()->SynchronizePageAccess(value);
        concrete_visitor()->RecordSlot(table, value_slot, value);
        AddWeakReferenceForReferenceSummarizer(table, value);

        if (!is_shared_heap_ && value.InSharedHeap()) continue;

        // Revisit ephemerons with both key and value unreachable at end
        // of concurrent marking cycle.
        if (concrete_visitor()->marking_state()->IsWhite(value)) {
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
    concrete_visitor()->SynchronizePageAccess(target);
    if (concrete_visitor()->marking_state()->IsBlackOrGrey(target)) {
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
  concrete_visitor()->SynchronizePageAccess(target);
  concrete_visitor()->SynchronizePageAccess(unregister_token);
  if (concrete_visitor()->marking_state()->IsBlackOrGrey(target) &&
      concrete_visitor()->marking_state()->IsBlackOrGrey(unregister_token)) {
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
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::MarkDescriptorArrayBlack(
    DescriptorArray descriptors) {
  concrete_visitor()->marking_state()->WhiteToGrey(descriptors);
  if (concrete_visitor()->marking_state()->GreyToBlack(descriptors)) {
    VisitMapPointer(descriptors);
    VisitPointers(descriptors, descriptors.GetFirstPointerSlot(),
                  descriptors.GetDescriptorSlot(0));
    return DescriptorArray::BodyDescriptor::SizeOf(descriptors.map(),
                                                   descriptors);
  }
  return 0;
}

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitDescriptors(
    DescriptorArray descriptor_array, int number_of_own_descriptors) {
  int16_t new_marked = static_cast<int16_t>(number_of_own_descriptors);
  int16_t old_marked = 0;
  if (CanUpdateValuesInHeap()) {
    old_marked = descriptor_array.UpdateNumberOfMarkedDescriptors(
        mark_compact_epoch_, new_marked);
  }
  if (old_marked < new_marked) {
    VisitPointers(
        descriptor_array,
        MaybeObjectSlot(descriptor_array.GetDescriptorSlot(old_marked)),
        MaybeObjectSlot(descriptor_array.GetDescriptorSlot(new_marked)));
  }
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitDescriptorArray(
    Map map, DescriptorArray array) {
  if (!concrete_visitor()->ShouldVisit(array)) return 0;
  this->VisitMapPointer(array);
  int size = DescriptorArray::BodyDescriptor::SizeOf(map, array);
  VisitPointers(array, array.GetFirstPointerSlot(), array.GetDescriptorSlot(0));
  VisitDescriptors(array, array.number_of_descriptors());
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitDescriptorsForMap(
    Map map) {
  if (!map.CanTransition()) return 0;

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
    return 0;
  }

  DescriptorArray descriptors = DescriptorArray::cast(maybe_descriptors);

  // Don't do any special processing of strong descriptor arrays, let them get
  // marked through the normal visitor mechanism.
  if (descriptors.IsStrongDescriptorArray()) {
    return 0;
  }
  concrete_visitor()->SynchronizePageAccess(descriptors);
  int size = MarkDescriptorArrayBlack(descriptors);
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  if (number_of_own_descriptors) {
    // It is possible that the concurrent marker observes the
    // number_of_own_descriptors out of sync with the descriptors. In that
    // case the marking write barrier for the descriptor array will ensure
    // that all required descriptors are marked. The concurrent marker
    // just should avoid crashing in that case. That's why we need the
    // std::min<int>() below.
    VisitDescriptors(descriptors,
                     std::min<int>(number_of_own_descriptors,
                                   descriptors.number_of_descriptors()));
  }

  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitMap(Map meta_map,
                                                                Map map) {
  if (!concrete_visitor()->ShouldVisit(map)) return 0;
  int size = Map::BodyDescriptor::SizeOf(meta_map, map);
  size += VisitDescriptorsForMap(map);

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

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_INL_H_
