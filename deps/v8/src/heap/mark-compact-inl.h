// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions.h"

namespace v8 {
namespace internal {

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::GreyToBlack(HeapObject obj) {
  MemoryChunk* p = MemoryChunk::FromHeapObject(obj);
  MarkBit markbit = MarkBitFrom(p, obj.address());
  if (!Marking::GreyToBlack<access_mode>(markbit)) return false;
  static_cast<ConcreteState*>(this)->IncrementLiveBytes(p, obj.Size());
  return true;
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::WhiteToGrey(HeapObject obj) {
  return Marking::WhiteToGrey<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::WhiteToBlack(
    HeapObject obj) {
  return WhiteToGrey(obj) && GreyToBlack(obj);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
MarkingVisitor<fixed_array_mode, retaining_path_mode,
               MarkingState>::MarkingVisitor(MarkCompactCollector* collector,
                                             MarkingState* marking_state)
    : heap_(collector->heap()),
      collector_(collector),
      marking_state_(marking_state),
      mark_compact_epoch_(collector->epoch()) {}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitBytecodeArray(Map map,
                                                     BytecodeArray array) {
  int size = BytecodeArray::BodyDescriptor::SizeOf(map, array);
  BytecodeArray::BodyDescriptor::IterateBody(map, array, size, this);

  if (!heap_->is_current_gc_forced()) {
    array.MakeOlder();
  }
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitDescriptorArray(Map map,
                                                       DescriptorArray array) {
  int size = DescriptorArray::BodyDescriptor::SizeOf(map, array);
  VisitPointers(array, array.GetFirstPointerSlot(), array.GetDescriptorSlot(0));
  VisitDescriptors(array, array.number_of_descriptors());
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitSharedFunctionInfo(Map map, SharedFunctionInfo shared_info) {
  int size = SharedFunctionInfo::BodyDescriptor::SizeOf(map, shared_info);
  SharedFunctionInfo::BodyDescriptor::IterateBody(map, shared_info, size, this);

  // If the SharedFunctionInfo has old bytecode, mark it as flushable,
  // otherwise visit the function data field strongly.
  if (shared_info.ShouldFlushBytecode(Heap::GetBytecodeFlushMode())) {
    collector_->AddBytecodeFlushingCandidate(shared_info);
  } else {
    VisitPointer(shared_info,
                 shared_info.RawField(SharedFunctionInfo::kFunctionDataOffset));
  }
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSFunction(Map map, JSFunction object) {
  int size = Parent::VisitJSFunction(map, object);

  // Check if the JSFunction needs reset due to bytecode being flushed.
  if (FLAG_flush_bytecode && object.NeedsResetDueToFlushedBytecode()) {
    collector_->AddFlushedJSFunction(object);
  }

  return size;
}
template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitFixedArray(Map map, FixedArray object) {
  return (fixed_array_mode == FixedArrayVisitationMode::kRegular)
             ? Parent::VisitFixedArray(map, object)
             : VisitFixedArrayIncremental(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
template <typename T>
V8_INLINE int
MarkingVisitor<fixed_array_mode, retaining_path_mode,
               MarkingState>::VisitEmbedderTracingSubclass(Map map, T object) {
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    marking_worklist()->embedder()->Push(MarkCompactCollectorBase::kMainThread,
                                         object);
  }
  int size = T::BodyDescriptor::SizeOf(map, object);
  T::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSApiObject(Map map, JSObject object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSArrayBuffer(Map map,
                                                     JSArrayBuffer object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSDataView(Map map, JSDataView object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSTypedArray(Map map,
                                                    JSTypedArray object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitEphemeronHashTable(Map map, EphemeronHashTable table) {
  collector_->AddEphemeronHashTable(table);

  for (int i = 0; i < table.Capacity(); i++) {
    ObjectSlot key_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
    HeapObject key = HeapObject::cast(table.KeyAt(i));
    collector_->RecordSlot(table, key_slot, key);

    ObjectSlot value_slot =
        table.RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

    if (marking_state()->IsBlackOrGrey(key)) {
      VisitPointer(table, value_slot);

    } else {
      Object value_obj = *value_slot;

      if (value_obj.IsHeapObject()) {
        HeapObject value = HeapObject::cast(value_obj);
        collector_->RecordSlot(table, value_slot, value);

        // Revisit ephemerons with both key and value unreachable at end
        // of concurrent marking cycle.
        if (marking_state()->IsWhite(value)) {
          collector_->AddEphemeron(key, value);
        }
      }
    }
  }

  return table.SizeFromMap(map);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitMap(Map meta_map, Map map) {
  int size = Map::BodyDescriptor::SizeOf(meta_map, map);
  if (map.CanTransition()) {
    // Maps that can transition share their descriptor arrays and require
    // special visiting logic to avoid memory leaks.
    // Since descriptor arrays are potentially shared, ensure that only the
    // descriptors that belong to this map are marked. The first time a
    // non-empty descriptor array is marked, its header is also visited. The
    // slot holding the descriptor array will be implicitly recorded when the
    // pointer fields of this map are visited.
    DescriptorArray descriptors = map.instance_descriptors();
    MarkDescriptorArrayBlack(map, descriptors);
    int number_of_own_descriptors = map.NumberOfOwnDescriptors();
    if (number_of_own_descriptors) {
      DCHECK_LE(number_of_own_descriptors, descriptors.number_of_descriptors());
      VisitDescriptors(descriptors, number_of_own_descriptors);
    }
    // Mark the pointer fields of the Map. Since the transitions array has
    // been marked already, it is fine that one of these fields contains a
    // pointer to it.
  }
  Map::BodyDescriptor::IterateBody(meta_map, map, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitTransitionArray(Map map,
                                                       TransitionArray array) {
  int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
  TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
  collector_->AddTransitionArray(array);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSWeakRef(Map map, JSWeakRef weak_ref) {
  if (weak_ref.target().IsHeapObject()) {
    HeapObject target = HeapObject::cast(weak_ref.target());
    if (marking_state()->IsBlackOrGrey(target)) {
      // Record the slot inside the JSWeakRef, since the IterateBody below
      // won't visit it.
      ObjectSlot slot = weak_ref.RawField(JSWeakRef::kTargetOffset);
      collector_->RecordSlot(weak_ref, slot, target);
    } else {
      // JSWeakRef points to a potentially dead object. We have to process
      // them when we know the liveness of the whole transitive closure.
      collector_->AddWeakRef(weak_ref);
    }
  }
  int size = JSWeakRef::BodyDescriptor::SizeOf(map, weak_ref);
  JSWeakRef::BodyDescriptor::IterateBody(map, weak_ref, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitWeakCell(Map map, WeakCell weak_cell) {
  if (weak_cell.target().IsHeapObject()) {
    HeapObject target = HeapObject::cast(weak_cell.target());
    if (marking_state()->IsBlackOrGrey(target)) {
      // Record the slot inside the WeakCell, since the IterateBody below
      // won't visit it.
      ObjectSlot slot = weak_cell.RawField(WeakCell::kTargetOffset);
      collector_->RecordSlot(weak_cell, slot, target);
    } else {
      // WeakCell points to a potentially dead object. We have to process
      // them when we know the liveness of the whole transitive closure.
      collector_->AddWeakCell(weak_cell);
    }
  }
  int size = WeakCell::BodyDescriptor::SizeOf(map, weak_cell);
  WeakCell::BodyDescriptor::IterateBody(map, weak_cell, size, this);
  return size;
}

// class template arguments
template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
// method template arguments
template <typename TSlot>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointerImpl(HeapObject host,
                                                    TSlot slot) {
  static_assert(std::is_same<TSlot, ObjectSlot>::value ||
                    std::is_same<TSlot, MaybeObjectSlot>::value,
                "Only ObjectSlot and MaybeObjectSlot are expected here");
  typename TSlot::TObject object = *slot;
  HeapObject target_object;
  if (object.GetHeapObjectIfStrong(&target_object)) {
    collector_->RecordSlot(host, HeapObjectSlot(slot), target_object);
    MarkObject(host, target_object);
  } else if (TSlot::kCanBeWeak && object.GetHeapObjectIfWeak(&target_object)) {
    if (marking_state()->IsBlackOrGrey(target_object)) {
      // Weak references with live values are directly processed here to reduce
      // the processing time of weak cells during the main GC pause.
      collector_->RecordSlot(host, HeapObjectSlot(slot), target_object);
    } else {
      // If we do not know about liveness of values of weak cells, we have to
      // process them when we know the liveness of the whole transitive
      // closure.
      collector_->AddWeakReference(host, HeapObjectSlot(slot));
    }
  }
}

// class template arguments
template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
// method template arguments
template <typename TSlot>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointersImpl(HeapObject host,
                                                     TSlot start, TSlot end) {
  for (TSlot p = start; p < end; ++p) {
    VisitPointer(host, p);
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitEmbeddedPointer(Code host,
                                                        RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
  HeapObject object = HeapObject::cast(rinfo->target_object());
  collector_->RecordRelocSlot(host, rinfo, object);
  if (!marking_state()->IsBlackOrGrey(object)) {
    if (host.IsWeakObject(object)) {
      collector_->AddWeakObjectInCode(object, host);
    } else {
      MarkObject(host, object);
    }
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitCodeTarget(Code host,
                                                   RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
  Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  collector_->RecordRelocSlot(host, rinfo, target);
  MarkObject(host, target);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    MarkDescriptorArrayBlack(HeapObject host, DescriptorArray descriptors) {
  // Note that WhiteToBlack is not sufficient here because it fails if the
  // descriptor array is grey. So we need to do two steps: WhiteToGrey and
  // GreyToBlack. Alternatively, we could check WhiteToGrey || WhiteToBlack.
  if (marking_state()->WhiteToGrey(descriptors)) {
    if (retaining_path_mode == TraceRetainingPathMode::kEnabled &&
        V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, descriptors);
    }
  }
  if (marking_state()->GreyToBlack(descriptors)) {
    VisitPointers(descriptors, descriptors.GetFirstPointerSlot(),
                  descriptors.GetDescriptorSlot(0));
  }
  DCHECK(marking_state()->IsBlack(descriptors));
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::MarkObject(HeapObject host,
                                              HeapObject object) {
  if (marking_state()->WhiteToGrey(object)) {
    marking_worklist()->Push(object);
    if (retaining_path_mode == TraceRetainingPathMode::kEnabled &&
        V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, object);
    }
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitFixedArrayIncremental(Map map, FixedArray object) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  int size = FixedArray::BodyDescriptor::SizeOf(map, object);
  if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
    DCHECK(FLAG_use_marking_progress_bar);
    DCHECK(heap_->IsLargeObject(object));
    size_t current_progress_bar = chunk->ProgressBar();
    int start = static_cast<int>(current_progress_bar);
    if (start == 0) start = FixedArray::BodyDescriptor::kStartOffset;
    int end = Min(size, start + kProgressBarScanningChunk);
    if (start < end) {
      VisitPointers(object, object.RawField(start), object.RawField(end));
      bool success = chunk->TrySetProgressBar(current_progress_bar, end);
      CHECK(success);
      if (end < size) {
        DCHECK(marking_state()->IsBlack(object));
        // The object can be pushed back onto the marking worklist only after
        // progress bar was updated.
        marking_worklist()->Push(object);
      }
    }
    return end - start;
  }

  // Non-batched processing.
  FixedArray::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitDescriptors(DescriptorArray descriptors,
                     int number_of_own_descriptors) {
  // Updating the number of marked descriptor is supported only for black
  // descriptor arrays.
  DCHECK(marking_state()->IsBlack(descriptors));
  int16_t new_marked = static_cast<int16_t>(number_of_own_descriptors);
  int16_t old_marked = descriptors.UpdateNumberOfMarkedDescriptors(
      mark_compact_epoch_, new_marked);
  if (old_marked < new_marked) {
    VisitPointers(descriptors,
                  MaybeObjectSlot(descriptors.GetDescriptorSlot(old_marked)),
                  MaybeObjectSlot(descriptors.GetDescriptorSlot(new_marked)));
  }
}

void MarkCompactCollector::MarkObject(HeapObject host, HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

#ifdef ENABLE_MINOR_MC

void MinorMarkCompactCollector::MarkRootObject(HeapObject obj) {
  if (Heap::InYoungGeneration(obj) &&
      non_atomic_marking_state_.WhiteToGrey(obj)) {
    worklist_->Push(kMainThread, obj);
  }
}

#endif

void MarkCompactCollector::MarkExternallyReferencedObject(HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(Root::kWrapperTracing, obj);
    }
  }
}

void MarkCompactCollector::RecordSlot(HeapObject object, ObjectSlot slot,
                                      HeapObject target) {
  RecordSlot(object, HeapObjectSlot(slot), target);
}

void MarkCompactCollector::RecordSlot(HeapObject object, HeapObjectSlot slot,
                                      HeapObject target) {
  MemoryChunk* target_page = MemoryChunk::FromHeapObject(target);
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>() &&
      !source_page->ShouldSkipEvacuationSlotRecording<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert(source_page, slot.address());
  }
}

void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot, HeapObject target) {
  MemoryChunk* target_page = MemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert(source_page, slot.address());
  }
}

void MarkCompactCollector::AddTransitionArray(TransitionArray array) {
  weak_objects_.transition_arrays.Push(kMainThread, array);
}

void MarkCompactCollector::AddBytecodeFlushingCandidate(
    SharedFunctionInfo flush_candidate) {
  weak_objects_.bytecode_flushing_candidates.Push(kMainThread, flush_candidate);
}

void MarkCompactCollector::AddFlushedJSFunction(JSFunction flushed_function) {
  weak_objects_.flushed_js_functions.Push(kMainThread, flushed_function);
}

template <LiveObjectIterationMode mode>
LiveObjectRange<mode>::iterator::iterator(MemoryChunk* chunk, Bitmap* bitmap,
                                          Address start)
    : chunk_(chunk),
      one_word_filler_map_(
          ReadOnlyRoots(chunk->heap()).one_pointer_filler_map()),
      two_word_filler_map_(
          ReadOnlyRoots(chunk->heap()).two_pointer_filler_map()),
      free_space_map_(ReadOnlyRoots(chunk->heap()).free_space_map()),
      it_(chunk, bitmap) {
  it_.Advance(Bitmap::IndexToCell(
      Bitmap::CellAlignIndex(chunk_->AddressToMarkbitIndex(start))));
  if (!it_.Done()) {
    cell_base_ = it_.CurrentCellBase();
    current_cell_ = *it_.CurrentCell();
    AdvanceToNextValidObject();
  }
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator& LiveObjectRange<mode>::iterator::
operator++() {
  AdvanceToNextValidObject();
  return *this;
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::iterator::
operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

template <LiveObjectIterationMode mode>
void LiveObjectRange<mode>::iterator::AdvanceToNextValidObject() {
  while (!it_.Done()) {
    HeapObject object;
    int size = 0;
    while (current_cell_ != 0) {
      uint32_t trailing_zeros = base::bits::CountTrailingZeros(current_cell_);
      Address addr = cell_base_ + trailing_zeros * kTaggedSize;

      // Clear the first bit of the found object..
      current_cell_ &= ~(1u << trailing_zeros);

      uint32_t second_bit_index = 0;
      if (trailing_zeros >= Bitmap::kBitIndexMask) {
        second_bit_index = 0x1;
        // The overlapping case; there has to exist a cell after the current
        // cell.
        // However, if there is a black area at the end of the page, and the
        // last word is a one word filler, we are not allowed to advance. In
        // that case we can return immediately.
        if (!it_.Advance()) {
          DCHECK(HeapObject::FromAddress(addr).map() == one_word_filler_map_);
          current_object_ = HeapObject();
          return;
        }
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      } else {
        second_bit_index = 1u << (trailing_zeros + 1);
      }

      Map map;
      if (current_cell_ & second_bit_index) {
        // We found a black object. If the black object is within a black area,
        // make sure that we skip all set bits in the black area until the
        // object ends.
        HeapObject black_object = HeapObject::FromAddress(addr);
        map = Map::cast(ObjectSlot(addr).Acquire_Load());
        size = black_object.SizeFromMap(map);
        Address end = addr + size - kTaggedSize;
        // One word filler objects do not borrow the second mark bit. We have
        // to jump over the advancing and clearing part.
        // Note that we know that we are at a one word filler when
        // object_start + object_size - kTaggedSize == object_start.
        if (addr != end) {
          DCHECK_EQ(chunk_, MemoryChunk::FromAddress(end));
          uint32_t end_mark_bit_index = chunk_->AddressToMarkbitIndex(end);
          unsigned int end_cell_index =
              end_mark_bit_index >> Bitmap::kBitsPerCellLog2;
          MarkBit::CellType end_index_mask =
              1u << Bitmap::IndexInCell(end_mark_bit_index);
          if (it_.Advance(end_cell_index)) {
            cell_base_ = it_.CurrentCellBase();
            current_cell_ = *it_.CurrentCell();
          }

          // Clear all bits in current_cell, including the end index.
          current_cell_ &= ~(end_index_mask + end_index_mask - 1);
        }

        if (mode == kBlackObjects || mode == kAllLiveObjects) {
          object = black_object;
        }
      } else if ((mode == kGreyObjects || mode == kAllLiveObjects)) {
        map = Map::cast(ObjectSlot(addr).Acquire_Load());
        object = HeapObject::FromAddress(addr);
        size = object.SizeFromMap(map);
      }

      // We found a live object.
      if (!object.is_null()) {
        // Do not use IsFiller() here. This may cause a data race for reading
        // out the instance type when a new map concurrently is written into
        // this object while iterating over the object.
        if (map == one_word_filler_map_ || map == two_word_filler_map_ ||
            map == free_space_map_) {
          // There are two reasons why we can get black or grey fillers:
          // 1) Black areas together with slack tracking may result in black one
          // word filler objects.
          // 2) Left trimming may leave black or grey fillers behind because we
          // do not clear the old location of the object start.
          // We filter these objects out in the iterator.
          object = HeapObject();
        } else {
          break;
        }
      }
    }

    if (current_cell_ == 0) {
      if (it_.Advance()) {
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      }
    }
    if (!object.is_null()) {
      current_object_ = object;
      current_size_ = size;
      return;
    }
  }
  current_object_ = HeapObject();
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::begin() {
  return iterator(chunk_, bitmap_, start_);
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::end() {
  return iterator(chunk_, bitmap_, end_);
}

Isolate* MarkCompactCollectorBase::isolate() { return heap()->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
