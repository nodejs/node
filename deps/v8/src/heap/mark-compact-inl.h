// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/index-generator.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::MarkObject(HeapObject host, HeapObject obj) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap()->Contains(obj));
  if (marking_state()->TryMark(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject obj) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap()->Contains(obj));
  if (marking_state()->TryMark(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

void MinorMarkCompactCollector::MarkRootObject(HeapObject obj) {
  if (Heap::InYoungGeneration(obj) &&
      non_atomic_marking_state()->TryMark(obj)) {
    local_marking_worklists_->Push(obj);
  }
}

// static
void MarkCompactCollector::RecordSlot(HeapObject object, ObjectSlot slot,
                                      HeapObject target) {
  RecordSlot(object, HeapObjectSlot(slot), target);
}

// static
void MarkCompactCollector::RecordSlot(HeapObject object, HeapObjectSlot slot,
                                      HeapObject target) {
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  if (!source_page->ShouldSkipEvacuationSlotRecording()) {
    RecordSlot(source_page, slot, target);
  }
}

// static
void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot, HeapObject target) {
  BasicMemoryChunk* target_page = BasicMemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate()) {
    if (target_page->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
      RememberedSet<OLD_TO_CODE>::Insert<AccessMode::ATOMIC>(source_page,
                                                             slot.address());
    } else {
      RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                            slot.address());
    }
  }
}

void MarkCompactCollector::AddTransitionArray(TransitionArray array) {
  local_weak_objects()->transition_arrays_local.Push(array);
}

bool MarkCompactCollector::ShouldMarkObject(HeapObject object) const {
  if (object.InReadOnlySpace()) return false;
  if (V8_LIKELY(!uses_shared_heap_)) return true;
  if (is_shared_space_isolate_) return true;
  return !object.InAnySharedSpace();
}

template <typename MarkingState>
template <typename TSlot>
void MainMarkingVisitor<MarkingState>::RecordSlot(HeapObject object, TSlot slot,
                                                  HeapObject target) {
  MarkCompactCollector::RecordSlot(object, slot, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::RecordRelocSlot(RelocInfo* rinfo,
                                                       HeapObject target) {
  MarkCompactCollector::RecordRelocSlot(rinfo, target);
}

template <LiveObjectIterationMode mode>
LiveObjectRange<mode>::iterator::iterator(const MemoryChunk* chunk,
                                          Bitmap* bitmap, Address start)
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
  PtrComprCageBase cage_base(chunk_->heap()->isolate());
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
        map = black_object.map(cage_base, kAcquireLoad);
        // Map might be forwarded during GC.
        DCHECK(MarkCompactCollector::IsMapOrForwarded(map));
        size = black_object.SizeFromMap(map);
        int aligned_size = ALIGN_TO_ALLOCATION_ALIGNMENT(size);
        CHECK_LE(addr + aligned_size, chunk_->area_end());
        Address end = addr + aligned_size - kTaggedSize;
        // One word filler objects do not borrow the second mark bit. We have
        // to jump over the advancing and clearing part.
        // Note that we know that we are at a one word filler when
        // object_start + object_size - kTaggedSize == object_start.
        if (addr != end) {
          DCHECK_EQ(chunk_, BasicMemoryChunk::FromAddress(end));
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
        object = HeapObject::FromAddress(addr);
        Object map_object = object.map(cage_base, kAcquireLoad);
        CHECK(map_object.IsMap(cage_base));
        map = Map::cast(map_object);
        DCHECK(map.IsMap(cage_base));
        size = object.SizeFromMap(map);
        CHECK_LE(addr + ALIGN_TO_ALLOCATION_ALIGNMENT(size),
                 chunk_->area_end());
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

Isolate* CollectorBase::isolate() { return heap()->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
