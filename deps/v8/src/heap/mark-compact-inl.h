// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/base/bits.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set.h"

namespace v8 {
namespace internal {

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
MarkingVisitor<fixed_array_mode, retaining_path_mode,
               MarkingState>::MarkingVisitor(MarkCompactCollector* collector,
                                             MarkingState* marking_state)
    : heap_(collector->heap()),
      collector_(collector),
      marking_state_(marking_state) {}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitAllocationSite(Map* map,
                                                      AllocationSite* object) {
  int size = AllocationSite::BodyDescriptorWeak::SizeOf(map, object);
  AllocationSite::BodyDescriptorWeak::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitBytecodeArray(Map* map,
                                                     BytecodeArray* array) {
  int size = BytecodeArray::BodyDescriptor::SizeOf(map, array);
  BytecodeArray::BodyDescriptor::IterateBody(map, array, size, this);
  array->MakeOlder();
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitCodeDataContainer(Map* map, CodeDataContainer* object) {
  int size = CodeDataContainer::BodyDescriptorWeak::SizeOf(map, object);
  CodeDataContainer::BodyDescriptorWeak::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitFixedArray(Map* map,
                                                  FixedArray* object) {
  return (fixed_array_mode == FixedArrayVisitationMode::kRegular)
             ? Parent::VisitFixedArray(map, object)
             : VisitFixedArrayIncremental(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSApiObject(Map* map, JSObject* object) {
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    DCHECK(object->IsJSObject());
    heap_->TracePossibleWrapper(object);
  }
  int size = JSObject::BodyDescriptor::SizeOf(map, object);
  JSObject::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSFunction(Map* map,
                                                  JSFunction* object) {
  int size = JSFunction::BodyDescriptorWeak::SizeOf(map, object);
  JSFunction::BodyDescriptorWeak::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitJSWeakCollection(Map* map, JSWeakCollection* weak_collection) {
  // Enqueue weak collection in linked list of encountered weak collections.
  if (weak_collection->next() == heap_->undefined_value()) {
    weak_collection->set_next(heap_->encountered_weak_collections());
    heap_->set_encountered_weak_collections(weak_collection);
  }

  // Skip visiting the backing hash table containing the mappings and the
  // pointer to the other enqueued weak collections, both are post-processed.
  int size = JSWeakCollection::BodyDescriptorWeak::SizeOf(map, weak_collection);
  JSWeakCollection::BodyDescriptorWeak::IterateBody(map, weak_collection, size,
                                                    this);

  // Partially initialized weak collection is enqueued, but table is ignored.
  if (!weak_collection->table()->IsHashTable()) return size;

  // Mark the backing hash table without pushing it on the marking stack.
  Object** slot =
      HeapObject::RawField(weak_collection, JSWeakCollection::kTableOffset);
  HeapObject* obj = HeapObject::cast(*slot);
  collector_->RecordSlot(weak_collection, slot, obj);
  MarkObjectWithoutPush(weak_collection, obj);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitMap(Map* map, Map* object) {
  // When map collection is enabled we have to mark through map's transitions
  // and back pointers in a special way to make these links weak.
  int size = Map::BodyDescriptor::SizeOf(map, object);
  if (object->CanTransition()) {
    MarkMapContents(object);
  } else {
    Map::BodyDescriptor::IterateBody(map, object, size, this);
  }
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitNativeContext(Map* map,
                                                     Context* context) {
  int size = Context::BodyDescriptorWeak::SizeOf(map, context);
  Context::BodyDescriptorWeak::IterateBody(map, context, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitTransitionArray(Map* map,
                                                       TransitionArray* array) {
  int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
  TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
  collector_->AddTransitionArray(array);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitWeakCell(Map* map, WeakCell* weak_cell) {
  // Enqueue weak cell in linked list of encountered weak collections.
  // We can ignore weak cells with cleared values because they will always
  // contain smi zero.
  if (!weak_cell->cleared()) {
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (marking_state()->IsBlackOrGrey(value)) {
      // Weak cells with live values are directly processed here to reduce
      // the processing time of weak cells during the main GC pause.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      collector_->RecordSlot(weak_cell, slot, *slot);
    } else {
      // If we do not know about liveness of values of weak cells, we have to
      // process them when we know the liveness of the whole transitive
      // closure.
      collector_->AddWeakCell(weak_cell);
    }
  }
  return WeakCell::BodyDescriptor::SizeOf(map, weak_cell);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointer(HeapObject* host, Object** p) {
  if (!(*p)->IsHeapObject()) return;
  HeapObject* target_object = HeapObject::cast(*p);
  collector_->RecordSlot(host, p, target_object);
  MarkObject(host, target_object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointer(HeapObject* host,
                                                MaybeObject** p) {
  HeapObject* target_object;
  if ((*p)->ToStrongHeapObject(&target_object)) {
    collector_->RecordSlot(host, reinterpret_cast<HeapObjectReference**>(p),
                           target_object);
    MarkObject(host, target_object);
  } else if ((*p)->ToWeakHeapObject(&target_object)) {
    if (marking_state()->IsBlackOrGrey(target_object)) {
      // Weak references with live values are directly processed here to reduce
      // the processing time of weak cells during the main GC pause.
      collector_->RecordSlot(host, reinterpret_cast<HeapObjectReference**>(p),
                             target_object);
    } else {
      // If we do not know about liveness of values of weak cells, we have to
      // process them when we know the liveness of the whole transitive
      // closure.
      collector_->AddWeakReference(host,
                                   reinterpret_cast<HeapObjectReference**>(p));
    }
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointers(HeapObject* host,
                                                 Object** start, Object** end) {
  for (Object** p = start; p < end; p++) {
    VisitPointer(host, p);
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitPointers(HeapObject* host,
                                                 MaybeObject** start,
                                                 MaybeObject** end) {
  for (MaybeObject** p = start; p < end; p++) {
    VisitPointer(host, p);
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitEmbeddedPointer(Code* host,
                                                        RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  HeapObject* object = HeapObject::cast(rinfo->target_object());
  collector_->RecordRelocSlot(host, rinfo, object);
  if (!host->IsWeakObject(object)) {
    MarkObject(host, object);
  } else if (!marking_state()->IsBlackOrGrey(object)) {
    collector_->AddWeakObjectInCode(object, host);
  }
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::VisitCodeTarget(Code* host,
                                                   RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  collector_->RecordRelocSlot(host, rinfo, target);
  MarkObject(host, target);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
bool MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::MarkObjectWithoutPush(HeapObject* host,
                                                         HeapObject* object) {
  if (marking_state()->WhiteToBlack(object)) {
    if (retaining_path_mode == TraceRetainingPathMode::kEnabled &&
        V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, object);
    }
    return true;
  }
  return false;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::MarkObject(HeapObject* host,
                                              HeapObject* object) {
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
    VisitFixedArrayIncremental(Map* map, FixedArray* object) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
  int object_size = FixedArray::BodyDescriptor::SizeOf(map, object);
  if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
    DCHECK(!FLAG_use_marking_progress_bar ||
           chunk->owner()->identity() == LO_SPACE);
    // When using a progress bar for large fixed arrays, scan only a chunk of
    // the array and try to push it onto the marking deque again until it is
    // fully scanned. Fall back to scanning it through to the end in case this
    // fails because of a full deque.
    int start_offset =
        Max(FixedArray::BodyDescriptor::kStartOffset, chunk->progress_bar());
    if (start_offset < object_size) {
      // Ensure that the object is either grey or black before pushing it
      // into marking worklist.
      marking_state()->WhiteToGrey(object);
      if (FLAG_concurrent_marking) {
        marking_worklist()->PushBailout(object);
      } else {
        marking_worklist()->Push(object);
      }
      DCHECK(marking_state()->IsGrey(object) ||
             marking_state()->IsBlack(object));

      int end_offset =
          Min(object_size, start_offset + kProgressBarScanningChunk);
      int already_scanned_offset = start_offset;
      VisitPointers(object, HeapObject::RawField(object, start_offset),
                    HeapObject::RawField(object, end_offset));
      start_offset = end_offset;
      end_offset = Min(object_size, end_offset + kProgressBarScanningChunk);
      chunk->set_progress_bar(start_offset);
      if (start_offset < object_size) {
        heap_->incremental_marking()->NotifyIncompleteScanOfObject(
            object_size - (start_offset - already_scanned_offset));
      }
    }
  } else {
    FixedArray::BodyDescriptor::IterateBody(map, object, object_size, this);
  }
  return object_size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
void MarkingVisitor<fixed_array_mode, retaining_path_mode,
                    MarkingState>::MarkMapContents(Map* map) {
  // Since descriptor arrays are potentially shared, ensure that only the
  // descriptors that belong to this map are marked. The first time a non-empty
  // descriptor array is marked, its header is also visited. The slot holding
  // the descriptor array will be implicitly recorded when the pointer fields of
  // this map are visited.  Prototype maps don't keep track of transitions, so
  // just mark the entire descriptor array.
  if (!map->is_prototype_map()) {
    DescriptorArray* descriptors = map->instance_descriptors();
    if (MarkObjectWithoutPush(map, descriptors) && descriptors->length() > 0) {
      VisitPointers(descriptors, descriptors->GetFirstElementAddress(),
                    descriptors->GetDescriptorEndSlot(0));
    }
    int start = 0;
    int end = map->NumberOfOwnDescriptors();
    if (start < end) {
      VisitPointers(descriptors, descriptors->GetDescriptorStartSlot(start),
                    descriptors->GetDescriptorEndSlot(end));
    }
  }

  // Mark the pointer fields of the Map. Since the transitions array has
  // been marked already, it is fine that one of these fields contains a
  // pointer to it.
  Map::BodyDescriptor::IterateBody(
      map->map(), map, Map::BodyDescriptor::SizeOf(map->map(), map), this);
}

void MarkCompactCollector::MarkObject(HeapObject* host, HeapObject* obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject* obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

void MarkCompactCollector::MarkExternallyReferencedObject(HeapObject* obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(Root::kWrapperTracing, obj);
    }
  }
}

void MarkCompactCollector::RecordSlot(HeapObject* object, Object** slot,
                                      Object* target) {
  RecordSlot(object, reinterpret_cast<HeapObjectReference**>(slot), target);
}

void MarkCompactCollector::RecordSlot(HeapObject* object,
                                      HeapObjectReference** slot,
                                      Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(object));
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>() &&
      !source_page->ShouldSkipEvacuationSlotRecording<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert(source_page,
                                      reinterpret_cast<Address>(slot));
  }
}

template <LiveObjectIterationMode mode>
LiveObjectRange<mode>::iterator::iterator(MemoryChunk* chunk, Bitmap* bitmap,
                                          Address start)
    : chunk_(chunk),
      one_word_filler_map_(chunk->heap()->one_pointer_filler_map()),
      two_word_filler_map_(chunk->heap()->two_pointer_filler_map()),
      free_space_map_(chunk->heap()->free_space_map()),
      it_(chunk, bitmap) {
  it_.Advance(Bitmap::IndexToCell(
      Bitmap::CellAlignIndex(chunk_->AddressToMarkbitIndex(start))));
  if (!it_.Done()) {
    cell_base_ = it_.CurrentCellBase();
    current_cell_ = *it_.CurrentCell();
    AdvanceToNextValidObject();
  } else {
    current_object_ = nullptr;
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
    HeapObject* object = nullptr;
    int size = 0;
    while (current_cell_ != 0) {
      uint32_t trailing_zeros = base::bits::CountTrailingZeros(current_cell_);
      Address addr = cell_base_ + trailing_zeros * kPointerSize;

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
          DCHECK(HeapObject::FromAddress(addr)->map() == one_word_filler_map_);
          current_object_ = nullptr;
          return;
        }
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      } else {
        second_bit_index = 1u << (trailing_zeros + 1);
      }

      Map* map = nullptr;
      if (current_cell_ & second_bit_index) {
        // We found a black object. If the black object is within a black area,
        // make sure that we skip all set bits in the black area until the
        // object ends.
        HeapObject* black_object = HeapObject::FromAddress(addr);
        map =
            base::AsAtomicPointer::Relaxed_Load(reinterpret_cast<Map**>(addr));
        size = black_object->SizeFromMap(map);
        Address end = addr + size - kPointerSize;
        // One word filler objects do not borrow the second mark bit. We have
        // to jump over the advancing and clearing part.
        // Note that we know that we are at a one word filler when
        // object_start + object_size - kPointerSize == object_start.
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
        map =
            base::AsAtomicPointer::Relaxed_Load(reinterpret_cast<Map**>(addr));
        object = HeapObject::FromAddress(addr);
        size = object->SizeFromMap(map);
      }

      // We found a live object.
      if (object != nullptr) {
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
          object = nullptr;
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
    if (object != nullptr) {
      current_object_ = object;
      current_size_ = size;
      return;
    }
  }
  current_object_ = nullptr;
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::begin() {
  return iterator(chunk_, bitmap_, start_);
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::end() {
  return iterator(chunk_, bitmap_, end_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
