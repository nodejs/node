// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_INL_H_
#define V8_HEAP_MARKING_VISITOR_INL_H_

#include "src/heap/marking-visitor.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

// ===========================================================================
// Visiting strong and weak pointers =========================================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::MarkObject(
    HeapObject host, HeapObject object) {
  concrete_visitor()->SynchronizePageAccess(object);
  if (concrete_visitor()->marking_state()->WhiteToGrey(object)) {
    marking_worklists_->Push(object);
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
  if (concrete_visitor()->marking_state()->IsBlackOrGrey(heap_object)) {
    // Weak references with live values are directly processed here to
    // reduce the processing time of weak cells during the main GC
    // pause.
    concrete_visitor()->RecordSlot(host, slot, heap_object);
  } else {
    // If we do not know about liveness of the value, we have to process
    // the reference when we know the liveness of the whole transitive
    // closure.
    weak_objects_->weak_references.Push(task_id_, std::make_pair(host, slot));
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
    typename TSlot::TObject object = slot.Relaxed_Load();
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
void MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitEmbeddedPointer(
    Code host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
  HeapObject object = rinfo->target_object();
  if (!concrete_visitor()->marking_state()->IsBlackOrGrey(object)) {
    if (host.IsWeakObject(object)) {
      weak_objects_->weak_objects_in_code.Push(task_id_,
                                               std::make_pair(object, host));
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
  if (!is_forced_gc_) {
    object.MakeOlder();
  }
  return size;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitJSFunction(
    Map map, JSFunction object) {
  int size = concrete_visitor()->VisitJSObjectSubclass(map, object);
  // Check if the JSFunction needs reset due to bytecode being flushed.
  if (bytecode_flush_mode_ != BytecodeFlushMode::kDoNotFlushBytecode &&
      object.NeedsResetDueToFlushedBytecode()) {
    weak_objects_->flushed_js_functions.Push(task_id_, object);
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

  // If the SharedFunctionInfo has old bytecode, mark it as flushable,
  // otherwise visit the function data field strongly.
  if (shared_info.ShouldFlushBytecode(bytecode_flush_mode_)) {
    weak_objects_->bytecode_flushing_candidates.Push(task_id_, shared_info);
  } else {
    VisitPointer(shared_info,
                 shared_info.RawField(SharedFunctionInfo::kFunctionDataOffset));
  }
  return size;
}

// ===========================================================================
// Fixed arrays that need incremental processing and can be left-trimmed =====
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::
    VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                   MemoryChunk* chunk) {
  const int kProgressBarScanningChunk = kMaxRegularHeapObjectSize;
  STATIC_ASSERT(kMaxRegularHeapObjectSize % kTaggedSize == 0);
  DCHECK(concrete_visitor()->marking_state()->IsBlackOrGrey(object));
  concrete_visitor()->marking_state()->GreyToBlack(object);
  int size = FixedArray::BodyDescriptor::SizeOf(map, object);
  size_t current_progress_bar = chunk->ProgressBar();
  int start = static_cast<int>(current_progress_bar);
  if (start == 0) {
    this->VisitMapPointer(object);
    start = FixedArray::BodyDescriptor::kStartOffset;
  }
  int end = Min(size, start + kProgressBarScanningChunk);
  if (start < end) {
    VisitPointers(object, object.RawField(start), object.RawField(end));
    bool success = chunk->TrySetProgressBar(current_progress_bar, end);
    CHECK(success);
    if (end < size) {
      // The object can be pushed back onto the marking worklist only after
      // progress bar was updated.
      marking_worklists_->Push(object);
    }
  }
  return end - start;
}

template <typename ConcreteVisitor, typename MarkingState>
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitFixedArray(
    Map map, FixedArray object) {
  // Arrays with the progress bar are not left-trimmable because they reside
  // in the large object space.
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  return chunk->IsFlagSet<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR)
             ? VisitFixedArrayWithProgressBar(map, object, chunk)
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
int MarkingVisitorBase<ConcreteVisitor,
                       MarkingState>::VisitEmbedderTracingSubclass(Map map,
                                                                   T object) {
  DCHECK(object.IsApiWrapper());
  int size = concrete_visitor()->VisitJSObjectSubclass(map, object);
  if (size && is_embedder_tracing_enabled_) {
    // Success: The object needs to be processed for embedder references on
    // the main thread.
    marking_worklists_->PushEmbedder(object);
  }
  return size;
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
  weak_objects_->ephemeron_hash_tables.Push(task_id_, table);

  for (InternalIndex i : table.IterateEntries()) {
    ObjectSlot key_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
    HeapObject key = HeapObject::cast(table.KeyAt(i));

    concrete_visitor()->SynchronizePageAccess(key);
    concrete_visitor()->RecordSlot(table, key_slot, key);

    ObjectSlot value_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

    if (concrete_visitor()->marking_state()->IsBlackOrGrey(key)) {
      VisitPointer(table, value_slot);
    } else {
      Object value_obj = table.ValueAt(i);

      if (value_obj.IsHeapObject()) {
        HeapObject value = HeapObject::cast(value_obj);
        concrete_visitor()->SynchronizePageAccess(value);
        concrete_visitor()->RecordSlot(table, value_slot, value);

        // Revisit ephemerons with both key and value unreachable at end
        // of concurrent marking cycle.
        if (concrete_visitor()->marking_state()->IsWhite(value)) {
          weak_objects_->discovered_ephemerons.Push(task_id_,
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
      weak_objects_->js_weak_refs.Push(task_id_, weak_ref);
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
  HeapObject unregister_token = HeapObject::cast(weak_cell.unregister_token());
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
    weak_objects_->weak_cells.Push(task_id_, weak_cell);
  }
  return size;
}

// ===========================================================================
// Custom weakness in descriptor arrays and transition arrays ================
// ===========================================================================

template <typename ConcreteVisitor, typename MarkingState>
size_t
MarkingVisitorBase<ConcreteVisitor, MarkingState>::MarkDescriptorArrayBlack(
    HeapObject host, DescriptorArray descriptors) {
  concrete_visitor()->marking_state()->WhiteToGrey(descriptors);
  if (concrete_visitor()->marking_state()->GreyToBlack(descriptors)) {
    VisitPointer(descriptors, descriptors.map_slot());
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
  int16_t old_marked = descriptor_array.UpdateNumberOfMarkedDescriptors(
      mark_compact_epoch_, new_marked);
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
int MarkingVisitorBase<ConcreteVisitor, MarkingState>::VisitMap(Map meta_map,
                                                                Map map) {
  if (!concrete_visitor()->ShouldVisit(map)) return 0;
  int size = Map::BodyDescriptor::SizeOf(meta_map, map);
  if (map.CanTransition()) {
    // Maps that can transition share their descriptor arrays and require
    // special visiting logic to avoid memory leaks.
    // Since descriptor arrays are potentially shared, ensure that only the
    // descriptors that belong to this map are marked. The first time a
    // non-empty descriptor array is marked, its header is also visited. The
    // slot holding the descriptor array will be implicitly recorded when the
    // pointer fields of this map are visited.
    DescriptorArray descriptors = map.synchronized_instance_descriptors();
    size += MarkDescriptorArrayBlack(map, descriptors);
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
    // Mark the pointer fields of the Map. Since the transitions array has
    // been marked already, it is fine that one of these fields contains a
    // pointer to it.
  }
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
  weak_objects_->transition_arrays.Push(task_id_, array);
  return size;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_INL_H_
