// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"
#include "src/heap/remembered-set.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::PushBlack(HeapObject* obj) {
  DCHECK((ObjectMarking::IsBlack<AccessMode::NON_ATOMIC>(
      obj, MarkingState::Internal(obj))));
  if (!marking_worklist()->Push(obj)) {
    ObjectMarking::BlackToGrey<AccessMode::NON_ATOMIC>(
        obj, MarkingState::Internal(obj));
  }
}

void MarkCompactCollector::MarkObject(HeapObject* obj) {
  if (ObjectMarking::WhiteToBlack<AccessMode::NON_ATOMIC>(
          obj, MarkingState::Internal(obj))) {
    PushBlack(obj);
  }
}

void MarkCompactCollector::RecordSlot(HeapObject* object, Object** slot,
                                      Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(object));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    DCHECK(
        ObjectMarking::IsBlackOrGrey(object, MarkingState::Internal(object)));
    RememberedSet<OLD_TO_OLD>::Insert(source_page,
                                      reinterpret_cast<Address>(slot));
  }
}

template <LiveObjectIterationMode mode>
LiveObjectRange<mode>::iterator::iterator(MemoryChunk* chunk,
                                          MarkingState state, Address start)
    : chunk_(chunk),
      one_word_filler_map_(chunk->heap()->one_pointer_filler_map()),
      two_word_filler_map_(chunk->heap()->two_pointer_filler_map()),
      free_space_map_(chunk->heap()->free_space_map()),
      it_(chunk, state) {
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
      uint32_t trailing_zeros = base::bits::CountTrailingZeros32(current_cell_);
      Address addr = cell_base_ + trailing_zeros * kPointerSize;

      // Clear the first bit of the found object..
      current_cell_ &= ~(1u << trailing_zeros);

      uint32_t second_bit_index = 1u << (trailing_zeros + 1);
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
      }

      Map* map = nullptr;
      if (current_cell_ & second_bit_index) {
        // We found a black object. If the black object is within a black area,
        // make sure that we skip all set bits in the black area until the
        // object ends.
        HeapObject* black_object = HeapObject::FromAddress(addr);
        map = base::NoBarrierAtomicValue<Map*>::FromAddress(addr)->Value();
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
        map = base::NoBarrierAtomicValue<Map*>::FromAddress(addr)->Value();
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
  return iterator(chunk_, state_, start_);
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::end() {
  return iterator(chunk_, state_, end_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
