// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LIVE_OBJECT_RANGE_INL_H_
#define V8_HEAP_LIVE_OBJECT_RANGE_INL_H_

#include "src/heap/live-object-range.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-inl.h"
#include "src/heap/normal-page-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-inl.h"
#include "src/sandbox/check.h"

namespace v8::internal {

template <ConcurrencyMode kConcurrencyMode>
LiveObjectRange<kConcurrencyMode>::iterator::iterator()
    : cage_base_(kNullAddress) {}

template <ConcurrencyMode kConcurrencyMode>
LiveObjectRange<kConcurrencyMode>::iterator::iterator(const NormalPage* page)
    : page_(page),
      cells_(page->marking_bitmap()->cells()),
      cage_base_(page->heap()->isolate()),
      current_cell_index_(MarkingBitmap::IndexToCell(
          MarkingBitmap::AddressToIndex(page->area_start()))),
      current_cell_(cells_[current_cell_index_]) {
  AdvanceToNextValidObject();
}

template <ConcurrencyMode kConcurrencyMode>
typename LiveObjectRange<kConcurrencyMode>::iterator&
LiveObjectRange<kConcurrencyMode>::iterator::operator++() {
  AdvanceToNextValidObject();
  return *this;
}

template <ConcurrencyMode kConcurrencyMode>
typename LiveObjectRange<kConcurrencyMode>::iterator
LiveObjectRange<kConcurrencyMode>::iterator::operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

template <ConcurrencyMode kConcurrencyMode>
void LiveObjectRange<kConcurrencyMode>::iterator::AdvanceToNextValidObject() {
  // If we found a regular object we are done. In case of free space, we
  // need to continue.
  //
  // Reading the instance type of the map is safe here even in the presence
  // of the mutator writing a new Map because Map objects are published with
  // release stores (or are otherwise read-only) and in concurrent mode the map
  // is retrieved in `ReadMapAndSize()` using an acquire load.
  while (AdvanceToNextMarkedObject() &&
         InstanceTypeChecker::IsFreeSpaceOrFiller(current_map_)) {
  }
}

template <ConcurrencyMode kConcurrencyMode>
bool LiveObjectRange<kConcurrencyMode>::iterator::AdvanceToNextMarkedObject() {
  // The following block moves the iterator to the next cell from the current
  // object. This means skipping all possibly set mark bits (in case of black
  // allocation).
  if (!current_object_.is_null()) {
    // Compute an end address that is inclusive. This allows clearing the cell
    // up and including the end address. This works for one word fillers as
    // well as other objects.
    Address next_object = current_object_.address() + current_size_;
    current_object_ = {};
    if (MemoryChunk::IsAligned(next_object)) {
      return false;
    }
    // Area end may not be exactly aligned to kAlignment. We don't need to bail
    // out for area_end() though as we are guaranteed to have a bit for the
    // whole page.
    DCHECK_LE(next_object, page_->area_end());
    // Move to the corresponding cell of the end index.
    const auto next_markbit_index = MarkingBitmap::AddressToIndex(next_object);
    DCHECK_GE(MarkingBitmap::IndexToCell(next_markbit_index),
              current_cell_index_);
    current_cell_index_ = MarkingBitmap::IndexToCell(next_markbit_index);
    DCHECK_LT(current_cell_index_, MarkingBitmap::kCellsCount);
    // Mask out lower addresses in the cell.
    const MarkBit::CellType mask =
        MarkingBitmap::IndexInCellMask(next_markbit_index);
    current_cell_ = cells_[current_cell_index_] & ~(mask - 1);
  }
  // The next block finds any marked object starting from the current cell.
  const MemoryChunk* chunk = page_->Chunk();
  while (true) {
    if (current_cell_) {
      const auto trailing_zeros = base::bits::CountTrailingZeros(current_cell_);
      Address current_cell_base =
          chunk->address() + MarkingBitmap::CellToBase(current_cell_index_);
      Address object_address = current_cell_base + trailing_zeros * kTaggedSize;
      // The object may be a filler which we want to skip.
      current_object_ = HeapObject::FromAddress(object_address);
      const auto [map, size] = ReadMapAndSize(current_object_);
      current_map_ = map;
      current_size_ = ALIGN_TO_ALLOCATION_ALIGNMENT(size.value());
      CHECK(page_->ContainsLimit(object_address + current_size_));
      return true;
    }
    if (++current_cell_index_ >= MarkingBitmap::kCellsCount) break;
    current_cell_ = cells_[current_cell_index_];
  }
  return false;
}

// static
template <ConcurrencyMode kConcurrencyMode>
std::pair<Tagged<Map>, SafeHeapObjectSize>
LiveObjectRange<kConcurrencyMode>::iterator::ReadMapAndSize(
    Tagged<HeapObject> object) {
  if constexpr (kConcurrencyMode == ConcurrencyMode::kSynchronous) {
    Tagged<Map> map = object->map();
    DCHECK(MapWord::IsMapOrForwarded(map));
    return {map, object->SafeSizeFromMap(map)};
  } else {
    Tagged<Map> map = object->map(kAcquireLoad);
    while (true) {
      DCHECK(MapWord::IsMapOrForwarded(map));
      if (!IsJSObjectMap(map)) {
        return {map, object->SafeSizeFromMap(map)};
      }
      const int raw_size = map->instance_size(kAcquireLoad);
      SBXCHECK_NE(raw_size, kVariableSizeSentinel);
      // Between loading `map` above and reading its `instance_size` here,
      // `object` may have migrated to a different map that keeps the full size
      // while in-object slack tracking shrank `map` in-place. Because slack
      // tracking updates `instance_size` with a release store and `raw_size`
      // was read with an acquire load, a relaxed reload of `object->map` here
      // is guaranteed to observe any migration that happened before `map`
      // shrank.
      Tagged<Map> confirmed_map = object->map(kAcquireLoad);
      if (V8_LIKELY(confirmed_map == map)) {
        return {map, SafeHeapObjectSize(static_cast<uint32_t>(raw_size))};
      }
      map = confirmed_map;
    }
  }
}

template <ConcurrencyMode kConcurrencyMode>
typename LiveObjectRange<kConcurrencyMode>::iterator
LiveObjectRange<kConcurrencyMode>::begin() {
  return iterator(page_);
}

template <ConcurrencyMode kConcurrencyMode>
typename LiveObjectRange<kConcurrencyMode>::iterator
LiveObjectRange<kConcurrencyMode>::end() {
  return iterator();
}

}  // namespace v8::internal

#endif  // V8_HEAP_LIVE_OBJECT_RANGE_INL_H_
