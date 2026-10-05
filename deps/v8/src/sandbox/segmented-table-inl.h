// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SEGMENTED_TABLE_INL_H_
#define V8_SANDBOX_SEGMENTED_TABLE_INL_H_

#include "src/sandbox/segmented-table.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/emulated-virtual-address-subspace.h"
#include "src/common/assert-scope.h"
#include "src/sandbox/testing.h"
#include "src/utils/allocation.h"

namespace v8::internal {

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::Segment
SegmentedTable<Entry, size>::Segment::At(uint32_t offset) {
  DCHECK(IsAligned(offset, kSegmentSize));
  return Segment(SegmentBase::At(offset));
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::Segment
SegmentedTable<Entry, size>::Segment::Containing(uint32_t entry_index) {
  uint32_t number = entry_index / kEntriesPerSegment;
  return Segment(number);
}

template <typename Entry, size_t size>
Entry& SegmentedTable<Entry, size>::at(uint32_t index) {
  return reinterpret_cast<Entry*>(this->base())[index];
}

template <typename Entry, size_t size>
const Entry& SegmentedTable<Entry, size>::at(uint32_t index) const {
  return reinterpret_cast<Entry*>(this->base())[index];
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::WritableRange
SegmentedTable<Entry, size>::GetWritableRange(Segment segment) {
  return WritableRange(reinterpret_cast<Entry*>(this->base()), segment);
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::FreelistHead
SegmentedTable<Entry, size>::InitializeFreeList(Segment segment) {
  uint32_t num_entries = kEntriesPerSegment;
  uint32_t first = segment.first_entry();
  uint32_t last = segment.last_entry();
  {
    auto write_range = GetWritableRange(segment);
    for (auto it = write_range.begin(); it != write_range.end(); ++it) {
      it->MakeFreelistEntry(it.index() == last ? 0 : it.index() + 1);
    }
  }

  return FreelistHead(first, num_entries);
}

template <typename Entry, size_t size>
std::pair<typename SegmentedTable<Entry, size>::Segment,
          typename SegmentedTable<Entry, size>::FreelistHead>
SegmentedTable<Entry, size>::AllocateAndInitializeSegment() {
  if (auto res = TryAllocateAndInitializeSegment()) {
    return *res;
  }
  V8::FatalProcessOutOfMemory(nullptr,
                              "SegmentedTable::AllocateAndInitializeSegment");
}

template <typename Entry, size_t size>
std::optional<std::pair<typename SegmentedTable<Entry, size>::Segment,
                        typename SegmentedTable<Entry, size>::FreelistHead>>
SegmentedTable<Entry, size>::TryAllocateAndInitializeSegment() {
  auto segment_base = TryAllocateSegment();
  if (!segment_base) return {};
  auto segment = Segment::From(*segment_base);
  FreelistHead freelist = InitializeFreeList(segment);
  return {{segment, freelist}};
}

}  // namespace v8::internal

#endif  // V8_SANDBOX_SEGMENTED_TABLE_INL_H_
