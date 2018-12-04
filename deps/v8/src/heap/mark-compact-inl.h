// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/base/bits.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set.h"
#include "src/objects/js-collection-inl.h"

namespace v8 {
namespace internal {

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::GreyToBlack(
    HeapObject* obj) {
  MemoryChunk* p = MemoryChunk::FromAddress(obj->address());
  MarkBit markbit = MarkBitFrom(p, obj->address());
  if (!Marking::GreyToBlack<access_mode>(markbit)) return false;
  static_cast<ConcreteState*>(this)->IncrementLiveBytes(p, obj->Size());
  return true;
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::WhiteToGrey(
    HeapObject* obj) {
  return Marking::WhiteToGrey<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::WhiteToBlack(
    HeapObject* obj) {
  return WhiteToGrey(obj) && GreyToBlack(obj);
}

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
                   MarkingState>::VisitBytecodeArray(Map* map,
                                                     BytecodeArray* array) {
  int size = BytecodeArray::BodyDescriptor::SizeOf(map, array);
  BytecodeArray::BodyDescriptor::IterateBody(map, array, size, this);
  array->MakeOlder();
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
template <typename T>
V8_INLINE int
MarkingVisitor<fixed_array_mode, retaining_path_mode,
               MarkingState>::VisitEmbedderTracingSubclass(Map* map,
                                                           T* object) {
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    heap_->TracePossibleWrapper(object);
  }
  int size = T::BodyDescriptor::SizeOf(map, object);
  T::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSApiObject(Map* map, JSObject* object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSArrayBuffer(Map* map,
                                                     JSArrayBuffer* object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSDataView(Map* map,
                                                  JSDataView* object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode,
                   MarkingState>::VisitJSTypedArray(Map* map,
                                                    JSTypedArray* object) {
  return VisitEmbedderTracingSubclass(map, object);
}

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
int MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>::
    VisitEphemeronHashTable(Map* map, EphemeronHashTable* table) {
  collector_->AddEphemeronHashTable(table);

  for (int i = 0; i < table->Capacity(); i++) {
    Object** key_slot =
        table->RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
    HeapObject* key = HeapObject::cast(table->KeyAt(i));
    collector_->RecordSlot(table, key_slot, key);

    Object** value_slot =
        table->RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

    if (marking_state()->IsBlackOrGrey(key)) {
      VisitPointer(table, value_slot);

    } else {
      Object* value_obj = *value_slot;

      if (value_obj->IsHeapObject()) {
        HeapObject* value = HeapObject::cast(value_obj);
        collector_->RecordSlot(table, value_slot, value);

        // Revisit ephemerons with both key and value unreachable at end
        // of concurrent marking cycle.
        if (marking_state()->IsWhite(value)) {
          collector_->AddEphemeron(key, value);
        }
      }
    }
  }

  return table->SizeFromMap(map);
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
                   MarkingState>::VisitTransitionArray(Map* map,
                                                       TransitionArray* array) {
  int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
  TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
  collector_->AddTransitionArray(array);
  return size;
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
  if ((*p)->GetHeapObjectIfStrong(&target_object)) {
    collector_->RecordSlot(host, reinterpret_cast<HeapObjectReference**>(p),
                           target_object);
    MarkObject(host, target_object);
  } else if ((*p)->GetHeapObjectIfWeak(&target_object)) {
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
  DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
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

#ifdef ENABLE_MINOR_MC

void MinorMarkCompactCollector::MarkRootObject(HeapObject* obj) {
  if (Heap::InNewSpace(obj) && non_atomic_marking_state_.WhiteToGrey(obj)) {
    worklist_->Push(kMainThread, obj);
  }
}

#endif

void MarkCompactCollector::MarkExternallyReferencedObject(HeapObject* obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(Root::kWrapperTracing, obj);
    }
  }
}

void MarkCompactCollector::RecordSlot(HeapObject* object, Object** slot,
                                      HeapObject* target) {
  RecordSlot(object, reinterpret_cast<HeapObjectReference**>(slot), target);
}

void MarkCompactCollector::RecordSlot(HeapObject* object,
                                      HeapObjectReference** slot,
                                      HeapObject* target) {
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

Isolate* MarkCompactCollectorBase::isolate() { return heap()->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
