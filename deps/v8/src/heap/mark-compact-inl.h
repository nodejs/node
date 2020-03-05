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

void MarkCompactCollector::MarkObject(HeapObject host, HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklists()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklists()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

#ifdef ENABLE_MINOR_MC

void MinorMarkCompactCollector::MarkRootObject(HeapObject obj) {
  if (Heap::InYoungGeneration(obj) &&
      non_atomic_marking_state_.WhiteToGrey(obj)) {
    worklist_->Push(kMainThreadTask, obj);
  }
}

#endif

void MarkCompactCollector::MarkExternallyReferencedObject(HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklists()->Push(obj);
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
    RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                          slot.address());
  }
}

void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot, HeapObject target) {
  MemoryChunk* target_page = MemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                          slot.address());
  }
}

void MarkCompactCollector::AddTransitionArray(TransitionArray array) {
  weak_objects_.transition_arrays.Push(kMainThreadTask, array);
}

template <typename MarkingState>
template <typename T, typename TBodyDescriptor>
int MainMarkingVisitor<MarkingState>::VisitJSObjectSubclass(Map map, T object) {
  if (!this->ShouldVisit(object)) return 0;
  this->VisitMapPointer(object);
  int size = TBodyDescriptor::SizeOf(map, object);
  TBodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <typename MarkingState>
template <typename T>
int MainMarkingVisitor<MarkingState>::VisitLeftTrimmableArray(Map map,
                                                              T object) {
  if (!this->ShouldVisit(object)) return 0;
  int size = T::SizeFor(object.length());
  this->VisitMapPointer(object);
  T::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <typename MarkingState>
template <typename TSlot>
void MainMarkingVisitor<MarkingState>::RecordSlot(HeapObject object, TSlot slot,
                                                  HeapObject target) {
  MarkCompactCollector::RecordSlot(object, slot, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::RecordRelocSlot(Code host,
                                                       RelocInfo* rinfo,
                                                       HeapObject target) {
  MarkCompactCollector::RecordRelocSlot(host, rinfo, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::MarkDescriptorArrayFromWriteBarrier(
    HeapObject host, DescriptorArray descriptors,
    int number_of_own_descriptors) {
  // This is necessary because the Scavenger records slots only for the
  // promoted black objects and the marking visitor of DescriptorArray skips
  // the descriptors marked by the visitor.VisitDescriptors() below.
  this->MarkDescriptorArrayBlack(host, descriptors);
  this->VisitDescriptors(descriptors, number_of_own_descriptors);
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
        // Do not use IsFreeSpaceOrFiller() here. This may cause a data race for
        // reading out the instance type when a new map concurrently is written
        // into this object while iterating over the object.
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
